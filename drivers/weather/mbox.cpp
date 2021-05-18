/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

  INDI MBox Driver

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "mbox.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <memory>
#include <regex>
#include <cstring>
#include <termios.h>
#include <unistd.h>

#define MBOX_TIMEOUT 6
#define MBOX_BUF     64

// We declare an auto pointer to MBox.
static std::unique_ptr<MBox> mbox(new MBox());

MBox::MBox()
{
    setVersion(1, 1);
}

const char *MBox::getDefaultName()
{
    return "MBox";
}

bool MBox::initProperties()
{
    INDI::Weather::initProperties();

    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, 15);
    addParameter("WEATHER_BAROMETER", "Barometer (mbar)", 20, 32.5, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 15);

    setCriticalParameter("WEATHER_TEMPERATURE");

    // Reset Calibration
    IUFillSwitch(&ResetS[0], "RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "CALIBRATION_RESET", "Reset", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Calibration Properties
    IUFillNumber(&CalibrationN[CAL_TEMPERATURE], "CAL_TEMPERATURE", "Temperature", "%.f", -50, 50, 1, 0);
    IUFillNumber(&CalibrationN[CAL_PRESSURE], "CAL_PRESSURE", "Pressure", "%.f", -100, 100, 10, 0);
    IUFillNumber(&CalibrationN[CAL_HUMIDITY], "CAL_HUMIDITY", "Humidity", "%.f", -50, 50, 1, 0);
    IUFillNumberVector(&CalibrationNP, CalibrationN, 3, getDeviceName(), "CALIBRATION", "Calibration", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Firmware Information
    IUFillText(&FirmwareT[0], "VERSION", "Version", "--");
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "DEVICE_FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_38400);

    addAuxControls();

    return true;
}

bool MBox::updateProperties()
{
    INDI::Weather::updateProperties();

    if (isConnected())
    {
        defineProperty(&CalibrationNP);
        defineProperty(&ResetSP);
        defineProperty(&FirmwareTP);
    }
    else
    {
        deleteProperty(CalibrationNP.name);
        deleteProperty(ResetSP.name);
        deleteProperty(FirmwareTP.name);
    }

    return true;
}

bool MBox::Handshake()
{
    //tty_set_debug(1);

    AckResponse rc = ACK_ERROR;

    for (int i = 0; i < 3; i++)
    {
        rc = ack();
        if (rc != ACK_ERROR)
            break;
    }

    if (rc == ACK_OK_STARTUP)
    {
        getCalibration(false);
        return true;
    }
    else if (rc == ACK_OK_INIT)
    {
        //getCalibration(true);
        CalibrationNP.s = IPS_BUSY;
        return true;
    }

    return false;
}

IPState MBox::updateWeather()
{
    char response[MBOX_BUF];

    if (CalibrationNP.s == IPS_BUSY)
    {
        if (getCalibration(true))
        {
            CalibrationNP.s = IPS_OK;
            IDSetNumber(&CalibrationNP, nullptr);
        }
    }

    int nbytes_read = 0;

    if (isSimulation())
    {
        strncpy(response, "$PXDR,P,96276.0,P,0,C,31.8,C,1,H,40.8,P,2,C,16.8,C,3,1.1*31\r\n", MBOX_BUF);
        nbytes_read = strlen(response);
    }
    else
    {
        int rc = -1;
        if ((rc = tty_read_section(PortFD, response, 0xA, MBOX_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            char errstr[MAXRBUF];
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
            return IPS_ALERT;
        }

        tcflush(PortFD, TCIOFLUSH);
    }

    // Remove \r\n
    response[nbytes_read - 2] = '\0';

    LOGF_DEBUG("RES <%s>", response);

    if (verifyCRC(response) == false)
    {
        LOG_ERROR("CRC check failed!");
        return IPS_ALERT;
    }

    // Remove * and checksum
    char *end = strstr(response, "*");
    *end = '\0';

    // PXDR
    std::vector<std::string> result = split(response, ",");
    // Convert Pascal to mbar
    setParameterValue("WEATHER_BAROMETER", std::stod(result[SENSOR_PRESSURE]) / 100.0);
    setParameterValue("WEATHER_TEMPERATURE", std::stod(result[SENSOR_TEMPERATURE]));
    setParameterValue("WEATHER_HUMIDITY", std::stod(result[SENSOR_HUMIDITY]));
    setParameterValue("WEATHER_DEWPOINT", std::stod(result[SENSOR_DEW]));
    if (strcmp(result[FIRMWARE].c_str(), FirmwareT[0].text))
    {
        IUSaveText(&FirmwareT[0], result[FIRMWARE].c_str());
        FirmwareTP.s = IPS_OK;
        IDSetText(&FirmwareTP, nullptr);
    }

    return IPS_OK;
}

MBox::AckResponse MBox::ack()
{
    char response[MBOX_BUF] = {0};
    int nbytes_read = 0;

    if (isSimulation())
    {
        strncpy(response, "MBox by Astromi.ch\r\n", 64);
        nbytes_read = strlen(response);
    }
    else
    {
        char errstr[MAXRBUF] = {0};
        int rc = -1;

        if ((rc = tty_read_section(PortFD, response, 0xA, MBOX_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
            return ACK_ERROR;
        }

        // Read again if we only recieved a newline character
        if (response[0] == '\n')
        {
            if ((rc = tty_read_section(PortFD, response, 0xA, MBOX_TIMEOUT, &nbytes_read)) != TTY_OK)
            {
                tty_error_msg(rc, errstr, MAXRBUF);
                LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
                return ACK_ERROR;
            }
        }
    }

    // Remove \r\n
    response[nbytes_read - 2] = '\0';

    LOGF_DEBUG("RES <%s>", response);

    if (strstr(response, "MBox"))
        return ACK_OK_STARTUP;
    // Check if already initialized
    else if (strstr(response, "PXDR"))
        return ACK_OK_INIT;

    return ACK_ERROR;
}

bool MBox::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, CalibrationNP.name))
        {
            double prevPressure = CalibrationN[CAL_PRESSURE].value;
            double prevTemperature = CalibrationN[CAL_TEMPERATURE].value;
            double prevHumidaty = CalibrationN[CAL_HUMIDITY].value;
            IUUpdateNumber(&CalibrationNP, values, names, n);
            double targetPressure = CalibrationN[CAL_PRESSURE].value;
            double targetTemperature = CalibrationN[CAL_TEMPERATURE].value;
            double targetHumidity = CalibrationN[CAL_HUMIDITY].value;

            bool rc = true;
            if (targetPressure != prevPressure)
            {
                rc = setCalibration(CAL_PRESSURE);
                usleep(200000);
            }
            if (targetTemperature != prevTemperature)
            {
                rc = setCalibration(CAL_TEMPERATURE);
                usleep(200000);
            }
            if (targetHumidity != prevHumidaty)
            {
                rc = setCalibration(CAL_HUMIDITY);
            }

            CalibrationNP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetNumber(&CalibrationNP, nullptr);
            return true;
        }
    }

    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

bool MBox::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, ResetSP.name))
        {
            if (resetCalibration())
            {
                ResetSP.s = IPS_OK;
                IDSetSwitch(&ResetSP, nullptr);
                LOG_INFO("Calibration values are reset.");

                CalibrationN[CAL_PRESSURE].value = 0;
                CalibrationN[CAL_TEMPERATURE].value = 0;
                CalibrationN[CAL_HUMIDITY].value = 0;
                CalibrationNP.s = IPS_IDLE;
                IDSetNumber(&CalibrationNP, nullptr);
            }
            else
            {
                ResetSP.s = IPS_ALERT;
                IDSetSwitch(&ResetSP, nullptr);
            }


            return true;
        }
    }

    return INDI::Weather::ISNewSwitch(dev, name, states, names, n);
}

bool MBox::getCalibration(bool sendCommand)
{
    int nbytes_written = 0, nbytes_read = 0;

    const char *command = ":calget*";
    char response[MBOX_BUF];

    if (sendCommand)
        LOGF_DEBUG("CMD <%s>", command);

    if (isSimulation())
    {
        strncpy(response, "$PCAL,P,20,T,50,H,-10*79\r\n", 64);
        nbytes_read = strlen(response);
    }
    else
    {
        int rc = -1;
        char errstr[MAXRBUF] = {0};

        if (sendCommand)
        {
            tcflush(PortFD, TCIOFLUSH);

            if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
            {
                tty_error_msg(rc, errstr, MAXRBUF);
                LOGF_ERROR("%s write error: %s.", __FUNCTION__, errstr);
                return false;
            }
        }

        if ((rc = tty_read_section(PortFD, response, 0xA, MBOX_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("%s read error: %s.", __FUNCTION__, errstr);
            return false;
        }

        // If token is invalid, read again
        if (strstr(response, "$PCAL") == nullptr)
        {
            if ((rc = tty_read_section(PortFD, response, 0xA, MBOX_TIMEOUT, &nbytes_read)) != TTY_OK)
            {
                tty_error_msg(rc, errstr, MAXRBUF);
                LOGF_ERROR("%s read error: %s.", __FUNCTION__, errstr);
                return false;
            }
        }
    }

    // Remove \r\n
    response[nbytes_read - 2] = '\0';

    LOGF_DEBUG("RES <%s>", response);

    if (verifyCRC(response) == false)
    {
        LOG_ERROR("CRC check failed!");
        return false;
    }

    // Remove * and checksum
    char *end = strstr(response, "*");
    *end = '\0';

    // PCAL
    std::vector<std::string> result = split(response, ",");
    CalibrationN[CAL_PRESSURE].value = std::stod(result[SENSOR_PRESSURE]) / 10.0;
    CalibrationN[CAL_TEMPERATURE].value = std::stod(result[SENSOR_PRESSURE + 2]) / 10.0;
    CalibrationN[CAL_HUMIDITY].value = std::stod(result[SENSOR_PRESSURE + 4]) / 10.0;
    return true;
}

bool MBox::setCalibration(CalibrationType type)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char command[16] = {0};

    if (type == CAL_PRESSURE)
    {
        // Pressure.
        snprintf(command, 16, ":calp,%d*", static_cast<int32_t>(CalibrationN[CAL_PRESSURE].value * 10.0));

        LOGF_DEBUG("CMD <%s>", command);

        if (isSimulation() == false)
        {
            tcflush(PortFD, TCIOFLUSH);

            if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
            {
                tty_error_msg(rc, errstr, MAXRBUF);
                LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
                return false;
            }

        }
    }
    else if (type == CAL_TEMPERATURE)
    {
        // Temperature
        snprintf(command, 16, ":calt,%d*", static_cast<int32_t>(CalibrationN[CAL_TEMPERATURE].value * 10.0));

        LOGF_DEBUG("CMD <%s>", command);

        if (isSimulation() == false)
        {
            tcflush(PortFD, TCIOFLUSH);

            if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
            {
                tty_error_msg(rc, errstr, MAXRBUF);
                LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
                return false;
            }
        }
    }
    else
    {
        // Humidity
        snprintf(command, 16, ":calh,%d*", static_cast<int32_t>(CalibrationN[CAL_HUMIDITY].value * 10.0));

        LOGF_DEBUG("CMD <%s>", command);

        if (isSimulation() == false)
        {
            tcflush(PortFD, TCIOFLUSH);

            if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
            {
                tty_error_msg(rc, errstr, MAXRBUF);
                LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
                return false;
            }

        }
    }

    return getCalibration(false);
}

bool MBox::resetCalibration()
{
    const char *command = ":calreset*";
    LOGF_DEBUG("CMD <%s>", command);

    if (isSimulation() == false)
    {
        int nbytes_written = 0, rc = -1;
        tcflush(PortFD, TCIOFLUSH);

        if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
        {
            char errstr[MAXRBUF];
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("%s error: %s.", __FUNCTION__, errstr);
            return false;
        }
    }

    return true;
}

bool MBox::verifyCRC(const char *response)
{
    // Start with $ and ends with * followed by checksum value of the response
    uint8_t calculated_checksum = 0, response_checksum = 0;
    // Skip starting $. Copy string
    char checksum_string[MBOX_BUF] = {0};
    strncpy(checksum_string, response + 1, MBOX_BUF);

    std::vector<std::string> result = split(checksum_string, R"(\*)");

    // Hex value
    try
    {
        response_checksum = std::stoi(result[1], nullptr, 16);
    }
    catch (...)
    {
        return false;
    }


    // Calculate checksum of message XOR
    for (auto oneByte : result[0])
        calculated_checksum ^= oneByte;

    return (calculated_checksum == response_checksum);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::vector<std::string> MBox::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
