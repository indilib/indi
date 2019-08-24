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

// We declare an auto pointer to SQM.
static std::unique_ptr<SQM> sqm(new SQM());

#define UNIT_TAB    "Unit"

void ISGetProperties(const char *dev)
{
    sqm->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    sqm->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    sqm->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    sqm->ISNewNumber(dev, name, values, names, n);
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
    sqm->ISSnoopDevice(root);
}

SQM::SQM()
{
    setVersion(1, 2);
}

bool SQM::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Average Readings
    IUFillNumber(&AverageReadingN[0], "SKY_BRIGHTNESS", "Quality (mag/arcsec^2)", "%6.2f", -20, 30, 0, 0);
    IUFillNumber(&AverageReadingN[1], "SENSOR_FREQUENCY", "Freq (Hz)", "%6.2f", 0, 1000000, 0, 0);
    IUFillNumber(&AverageReadingN[2], "SENSOR_COUNTS", "Period (counts)", "%6.2f", 0, 1000000, 0, 0);
    IUFillNumber(&AverageReadingN[3], "SENSOR_PERIOD", "Period (s)", "%6.2f", 0, 1000000, 0, 0);
    IUFillNumber(&AverageReadingN[4], "SKY_TEMPERATURE", "Temperature (C)", "%6.2f", -50, 80, 0, 0);
    IUFillNumberVector(&AverageReadingNP, AverageReadingN, 5, getDeviceName(), "SKY_QUALITY", "Readings",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Unit Info
    IUFillNumber(&UnitInfoN[0], "Protocol", "", "%.f", 0, 1000000, 0, 0);
    IUFillNumber(&UnitInfoN[1], "Model", "", "%.f", 0, 1000000, 0, 0);
    IUFillNumber(&UnitInfoN[2], "Feature", "", "%.f", 0, 1000000, 0, 0);
    IUFillNumber(&UnitInfoN[3], "Serial", "", "%.f", 0, 1000000, 0, 0);
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
        defineNumber(&AverageReadingNP);
        defineNumber(&UnitInfoNP);

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
                LOGF_WARN("Make sure SQM web timeout is configured for %ld seconds or more. Otherwise SQM will disconnect prematurely.", seconds);
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}


#if 0
bool SQM::getReadings()
{
    const char *cmd = "rx";
    char buffer[57] = {0};

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    ssize_t written = write(PortFD, cmd, 2);

    if (written < 2)
    {
        LOGF_ERROR("Error getting device readings: %s", strerror(errno));
        return false;
    }

    ssize_t received = 0;

    while (received < 57)
    {
        ssize_t response = read(PortFD, buffer + received, 57 - received);
        if (response < 0)
        {
            LOGF_ERROR("Error getting device readings: %s", strerror(errno));
            return false;
        }

        received += response;
    }

    if (received < 57)
    {
        LOG_ERROR("Error getting device readings");
        return false;
    }

    LOGF_DEBUG("RES <%s>", buffer);

    tcflush(PortFD, TCIOFLUSH);

    float mpsas, period_seconds, temperature;
    int frequency, period_counts;
    int rc =
        sscanf(buffer, "r,%fm,%dHz,%dc,%fs,%fC", &mpsas, &frequency, &period_counts, &period_seconds, &temperature);

    if (rc < 5)
    {
        LOGF_ERROR("Failed to parse input %s", buffer);
        return false;
    }

    AverageReadingN[0].value = mpsas;
    AverageReadingN[1].value = frequency;
    AverageReadingN[2].value = period_counts;
    AverageReadingN[3].value = period_seconds;
    AverageReadingN[4].value = temperature;

    return true;
}
#endif

bool SQM::getReadings()
{
    const char *cmd = "rx";
    char res[128] = {0};
    int nbytes_read = 0, nbytes_written = 0, rc = 0;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ( (rc = tty_write(PortFD, cmd, 2, &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF] = {0};
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Error getting device readings: %s", errorMessage);
        return false;
    }

    if ( (rc = tty_nread_section(PortFD, res, 128, 0xA, 3, &nbytes_read)) != TTY_OK)
    {
        if (rc == TTY_OVERFLOW)
        {
            return true;
        }

        char errorMessage[MAXRBUF] = {0};
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Error getting device readings: %s", errorMessage);
        return false;
    }

    res[nbytes_read - 2] = 0;
    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    float mpsas, period_seconds, temperature;
    int frequency, period_counts;
    rc = sscanf(res, "r,%fm,%dHz,%dc,%fs,%fC", &mpsas, &frequency, &period_counts, &period_seconds, &temperature);

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
    const char *cmd = "ix";
    char buffer[39] = {0};

    if (getActiveConnection() == serialConnection)
    {
        PortFD = serialConnection->getPortFD();
    }
    else if (getActiveConnection() == tcpConnection)
    {
        PortFD = tcpConnection->getPortFD();
    }

    LOGF_DEBUG("CMD <%s>", cmd);

    ssize_t written = write(PortFD, cmd, 2);

    if (written < 2)
    {
        LOGF_ERROR("Error getting device info while writing to device: %s", strerror(errno));
        return false;
    }

    ssize_t received = 0;

    while (received < 39)
    {
        ssize_t response = read(PortFD, buffer + received, 39 - received);
        if (response < 0)
        {
            LOGF_ERROR("Error getting device info while reading response: %s", strerror(errno));
            return false;
        }

        received += response;
    }

    if (received < 39)
    {
        LOG_ERROR("Error getting device info");
        return false;
    }

    LOGF_DEBUG("RES <%s>", buffer);

    int protocol, model, feature, serial;
    int rc = sscanf(buffer, "i,%d,%d,%d,%d", &protocol, &model, &feature, &serial);

    if (rc < 4)
    {
        LOGF_ERROR("Failed to parse input %s", buffer);
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

    SetTimer(POLLMS);
}
