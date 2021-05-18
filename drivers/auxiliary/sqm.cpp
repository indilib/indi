/*******************************************************************************
  Copyright(c) 2016 Jasem Mutlaq. All rights reserved.

  INDI Sky Quality Meter Driver

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

#include "sqm.h"

#include "connectionplugins/connectiontcp.h"
#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <cerrno>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <regex>

// We declare an auto pointer to SQM.
static std::unique_ptr<SQM> sqm(new SQM());

#define UNIT_TAB    "Unit"

SQM::SQM()
{
    setVersion(1, 3);
}

bool SQM::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Average Readings
    IUFillNumber(&AverageReadingN[SKY_BRIGHTNESS], "SKY_BRIGHTNESS", "Quality (mag/arcsec^2)", "%6.2f", -20, 30, 0, 0);
    IUFillNumber(&AverageReadingN[SENSOR_FREQUENCY], "SENSOR_FREQUENCY", "Freq (Hz)", "%6.2f", 0, 1000000, 0, 0);
    IUFillNumber(&AverageReadingN[SENSOR_COUNTS], "SENSOR_COUNTS", "Period (counts)", "%6.2f", 0, 1000000, 0, 0);
    IUFillNumber(&AverageReadingN[SENSOR_PERIOD], "SENSOR_PERIOD", "Period (s)", "%6.2f", 0, 1000000, 0, 0);
    IUFillNumber(&AverageReadingN[SKY_TEMPERATURE], "SKY_TEMPERATURE", "Temperature (C)", "%6.2f", -50, 80, 0, 0);
    IUFillNumberVector(&AverageReadingNP, AverageReadingN, 5, getDeviceName(), "SKY_QUALITY", "Readings",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Unit Info
    IUFillNumber(&UnitInfoN[UNIT_PROTOCOL], "UNIT_PROTOCOL", "Protocol", "%.f", 0, 1000000, 0, 0);
    IUFillNumber(&UnitInfoN[UNIT_MODEL], "UNIT_MODEL", "Model", "%.f", 0, 1000000, 0, 0);
    IUFillNumber(&UnitInfoN[UNIT_FEATURE], "UNIT_FEATURE", "Feature", "%.f", 0, 1000000, 0, 0);
    IUFillNumber(&UnitInfoN[UNIT_SERIAL], "UNIT_SERIAL", "Serial", "%.f", 0, 1000000, 0, 0);
    IUFillNumberVector(&UnitInfoNP, UnitInfoN, 4, getDeviceName(), "Unit Info", "", UNIT_TAB, IP_RW, 0, IPS_IDLE);

    if (sqmConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]()
        {
            return getDeviceInfo();
        });
        serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
        registerConnection(serialConnection);
    }

    if (sqmConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->setDefaultHost("192.168.1.1");
        tcpConnection->setDefaultPort(10001);
        tcpConnection->registerHandshake([&]()
        {
            return getDeviceInfo();
        });

        registerConnection(tcpConnection);
    }

    addDebugControl();
    addPollPeriodControl();

    return true;
}

bool SQM::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&AverageReadingNP);
        defineProperty(&UnitInfoNP);

        getReadings();

    }
    else
    {
        deleteProperty(AverageReadingNP.name);
        deleteProperty(UnitInfoNP.name);
    }

    return true;
}

bool SQM::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // For polling periods > 2 seconds, the user must configure SQM via the web tool
        if (!strcmp(name, "POLLING_PERIOD"))
        {
            uint32_t seconds = values[0] / 1000;
            if (seconds > 2)
                LOGF_WARN("Make sure SQM web timeout is configured for %ld seconds or more. Otherwise SQM will disconnect prematurely.",
                          seconds);
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool SQM::getReadings()
{
    char res[DRIVER_LEN] = {0};

    if (!sendCommand("rx", res))
        return false;

    float mpsas, period_seconds, temperature;
    int frequency, period_counts;
    int rc = sscanf(res, "r,%fm,%dHz,%dc,%fs,%fC", &mpsas, &frequency, &period_counts, &period_seconds, &temperature);

    if (rc < 5)
    {
        rc = sscanf(res, "r,%fm,%dHz,%dc,%fs,%fC,%*d", &mpsas, &frequency, &period_counts, &period_seconds, &temperature);
        if (rc < 5)
        {
            LOGF_ERROR("Failed to parse input %s", res);
            return false;
        }
    }

    AverageReadingN[0].value = mpsas;
    AverageReadingN[1].value = frequency;
    AverageReadingN[2].value = period_counts;
    AverageReadingN[3].value = period_seconds;
    AverageReadingN[4].value = temperature;

    return true;
}

const char *SQM::getDefaultName()
{
    return "SQM";
}

bool SQM::getDeviceInfo()
{
    if (getActiveConnection() == serialConnection)
    {
        PortFD = serialConnection->getPortFD();
    }
    else if (getActiveConnection() == tcpConnection)
    {
        PortFD = tcpConnection->getPortFD();
    }

    char res[DRIVER_LEN] = {0};
    for (int i = 0; i < 3; i++)
    {
        if (!sendCommand("ix", res))
        {
            usleep(500000);
        }
        else
            break;
    }

    if (res[0] == 0)
    {
        LOGF_ERROR("Error getting device info while reading response: %s", strerror(errno));
        return false;
    }

    int protocol, model, feature, serial;
    int rc = sscanf(res, "i,%d,%d,%d,%d", &protocol, &model, &feature, &serial);

    if (rc < 4)
    {
        LOGF_ERROR("Failed to parse input %s", res);
        return false;
    }

    UnitInfoN[0].value = protocol;
    UnitInfoN[1].value = model;
    UnitInfoN[2].value = feature;
    UnitInfoN[3].value = serial;

    return true;
}

void SQM::TimerHit()
{
    if (!isConnected())
        return;

    bool rc = getReadings();

    AverageReadingNP.s = rc ? IPS_OK : IPS_ALERT;
    IDSetNumber(&AverageReadingNP, nullptr);

    SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool SQM::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        // Remove extra \r\n
        res[nbytes_read - 2] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void SQM::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> SQM::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
