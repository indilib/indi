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
#include <cstring>
#include <termios.h>

#define MBOX_TIMEOUT 5
#define MBOX_BUF     64

// We declare an auto pointer to MBox.
std::unique_ptr<MBox> mbox(new MBox());

void ISGetProperties(const char *dev)
{
    mbox->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    mbox->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    mbox->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    mbox->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}
void ISSnoopDevice(XMLEle *root)
{
    mbox->ISSnoopDevice(root);
}

MBox::MBox()
{
    setVersion(1, 0);
}

const char *MBox::getDefaultName()
{
    return (const char *)"MBox";
}

bool MBox::initProperties()
{
    INDI::Weather::initProperties();

    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, -20, 40);
    addParameter("WEATHER_BAROMETER", "Barometer (mbar)", 20, 32.5, 20, 32.5);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 0, 100);
    addParameter("WEATHER_DEWPOINT", "Dew Point (C)", 0, 100, 0, 100);

    setCriticalParameter("WEATHER_TEMPERATURE");

    // Reset Calibration
    IUFillSwitch(&ResetS[0], "RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "CALIBRATION_RESET", "Reset", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Calibration Properties
    IUFillNumber(&CalibrationN[CAL_TEMPERATURE], "CAL_TEMPERATURE", "Temperature", "%.f", -50, 50, 1, 0);
    IUFillNumber(&CalibrationN[CAL_PRESSURE], "CAL_PRESSURE", "Pressure", "%.f", -100, 100, 10, 0);
    IUFillNumber(&CalibrationN[CAL_HUMIDITY], "CAL_HUMIDITY", "Humidity", "%.f", -50, 50, 1, 0);
    IUFillNumberVector(&CalibrationNP, CalibrationN, 3, getDeviceName(), "CALIBRATION", "Cabliration", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

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
        defineNumber(&CalibrationNP);
        defineSwitch(&ResetSP);
        defineText(&FirmwareTP);
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
    return ack();
}

IPState MBox::updateWeather()
{
    int nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];    
    char response[MBOX_BUF];

    if (isSimulation())
    {
        strncpy(response, "$PXDR,P,96276.0,P,0,C,31.8,C,1,H,40.8,P,2,C,16.8,C,3,1.1*31\r\n", MBOX_BUF);
        nbytes_read = strlen(response);
    }
    else
    {
        if ((rc = tty_read_section(PortFD, response, 0xA, MBOX_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "% error: %s.", __FUNCTION__, errstr);
            return IPS_ALERT;
        }

        tcflush(PortFD, TCIOFLUSH);
    }

    // Remove \r\n
    response[nbytes_read - 2] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", response);

    if (verifyCRC(response) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "CRC check failed!");
        return IPS_ALERT;
    }

    // Remove * and checksum
    char *end = strstr(response, "*");
    *end = '\0';

    // PXDR
    char *token = std::strtok(response, ",");

    // Sensor Type: Pressure
    token = std::strtok(NULL, ",");

    // P Sensor Value
    token = std::strtok(NULL, ",");
    if (token == nullptr)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Invalid response.");
        return IPS_ALERT;
    }
    // Convert Pascal to mbar
    setParameterValue("WEATHER_BAROMETER", atof(token)/100.0);

    // Sensor Units (Pascal)
    token = std::strtok(NULL, ",");
    // Sensor ID
    token = std::strtok(NULL, ",");

    // Sensor Type: Temperature
    token = std::strtok(NULL, ",");
    // T Sensor value Temperature
    token = std::strtok(NULL, ",");
    if (token == nullptr)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Invalid response.");
        return IPS_ALERT;
    }
    setParameterValue("WEATHER_TEMPERATURE", atof(token));

    // Sensor Units (C)
    token = std::strtok(NULL, ",");
    // Sensor ID
    token = std::strtok(NULL, ",");

    // Sensor Type: Humidity
    token = std::strtok(NULL, ",");
    // Humidity
    token = std::strtok(NULL, ",");
    if (token == nullptr)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Invalid response.");
        return IPS_ALERT;
    }
    setParameterValue("WEATHER_HUMIDITY", atof(token));

    // Sensor Units (Percentage)
    token = std::strtok(NULL, ",");
    // Sensor ID
    token = std::strtok(NULL, ",");

    // Sensor Type: Dew Point
    token = std::strtok(NULL, ",");
    // Dew Point
    token = std::strtok(NULL, ",");
    if (token == nullptr)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Invalid response.");
        return IPS_ALERT;
    }
    setParameterValue("WEATHER_DEWPOINT", atof(token));

    // Sensor Units (C)
    token = std::strtok(NULL, ",");
    // Sensor ID
    token = std::strtok(NULL, ",");

    // Firmware
    token = std::strtok(NULL, ",");
    if (strcmp(token, FirmwareT[0].text))
    {
        IUSaveText(&FirmwareT[0], token);
        FirmwareTP.s = IPS_OK;
        IDSetText(&FirmwareTP, nullptr);
    }

    return IPS_OK;
}

bool MBox::ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    const char *command = ":calget*";
    char response[MBOX_BUF];

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", command);

    if (isSimulation())
    {
        strncpy(response, "$PCAL,P,20,T,50,H,-10*79\r\n", 64);
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(PortFD, TCIOFLUSH);

        if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
            return false;
        }

        if ((rc = tty_read_section(PortFD, response, 0xA, MBOX_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "% error: %s.", __FUNCTION__, errstr);
            return false;
        }
    }

    // Remove \r\n
    response[nbytes_read - 2] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", response);

    if (verifyCRC(response) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "CRC check failed!");
        return false;
    }

    return true;
}

bool MBox::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, CalibrationNP.name))
        {
            IUUpdateNumber(&CalibrationNP, values, names, n);
            if (setCalibration() && getCalibration())
                CalibrationNP.s = IPS_OK;
            else
                CalibrationNP.s = IPS_ALERT;
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
                DEBUG(INDI::Logger::DBG_SESSION, "Calibration values are reset.");

                CalibrationNP.s = getCalibration() ? IPS_OK : IPS_ALERT;
                IDSetNumber(&CalibrationNP, nullptr);
            }
            else
                ResetSP.s = IPS_ALERT;

            IDSetSwitch(&ResetSP, nullptr);
            return true;
        }
    }

    return INDI::Weather::ISNewSwitch(dev, name, states, names, n);
}

bool MBox::getCalibration()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    const char *command = ":calget*";
    char response[MBOX_BUF];

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", command);

    if (isSimulation())
    {
        strncpy(response, "$PCAL,P,20,T,50,H,-10*79\r\n", 64);
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(PortFD, TCIOFLUSH);

        if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
            return false;
        }

        if ((rc = tty_read_section(PortFD, response, 0xA, MBOX_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "% error: %s.", __FUNCTION__, errstr);
            return false;
        }
    }

    // Remove \r\n
    response[nbytes_read - 2] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", response);

    if (verifyCRC(response) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "CRC check failed!");
        return false;
    }

    // Remove * and checksum
    char *end = strstr(response, "*");
    *end = '\0';

    // PCAL
    char *token = std::strtok(response, ",");

    // P
    token = std::strtok(NULL, ",");

    // Pressure
    token = std::strtok(NULL, ",");
    if (token == nullptr)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Invalid response.");
        return false;
    }
    CalibrationN[CAL_PRESSURE].value = atof(token)/10.0;

    // T
    token = std::strtok(NULL, ",");
    // Temperature
    token = std::strtok(NULL, ",");
    if (token == nullptr)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Invalid response.");
        return false;
    }
    CalibrationN[CAL_TEMPERATURE].value = atof(token)/10.0;

    // H
    token = std::strtok(NULL, ",");
    // Humidity
    token = std::strtok(NULL, ",");
    if (token == nullptr)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Invalid response.");
        return false;
    }
    CalibrationN[CAL_TEMPERATURE].value = atof(token)/10.0;

    return true;
}

bool MBox::setCalibration()
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char command[16] = {0};

    // Pressure. Times 100 since mbar -> Pascal conversion.
    snprintf(command, 16, ":calp,%d*", static_cast<int32_t>(CalibrationN[CAL_PRESSURE].value*100.0));

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", command);

    if (isSimulation() == false)
    {
        tcflush(PortFD, TCIOFLUSH);

        if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
            return false;
        }
    }

    // Temperature
    snprintf(command, 16, ":calt,%d*", static_cast<int32_t>(CalibrationN[CAL_TEMPERATURE].value*10.0));

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", command);

    if (isSimulation() == false)
    {
        tcflush(PortFD, TCIOFLUSH);

        if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
            return false;
        }
    }

    // Humidity
    snprintf(command, 16, ":calh,%d*", static_cast<int32_t>(CalibrationN[CAL_HUMIDITY].value*10.0));

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", command);

    if (isSimulation() == false)
    {
        tcflush(PortFD, TCIOFLUSH);

        if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
            return false;
        }
    }

    return true;
}

bool MBox::resetCalibration()
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    const char *command = ":calreset*";

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", command);

    if (isSimulation() == false)
    {
        tcflush(PortFD, TCIOFLUSH);

        if ((rc = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
            return false;
        }
    }

    return true;
}

bool MBox::verifyCRC(const char *response)
{
    // Start with $ and ends with * followed by checksum value of the response
    uint8_t calculated_checksum = 0;
    // Skip starting $. Copy string
    char checksum_string[MBOX_BUF];
    strncpy(checksum_string, response, MBOX_BUF);

    char *token = std::strtok(checksum_string, "*");

    // Checksum string
    token = std::strtok(NULL, "*");
    // Hex value
    uint8_t response_checksum_val = std::stoi(token, 0, 16);
    // Terminate it
    *token = '\0';

    char *str = checksum_string+1;

    // Calculate checksum of message XOR
    while (*str)
        calculated_checksum ^= *str++;

    return (calculated_checksum == response_checksum_val);
}
