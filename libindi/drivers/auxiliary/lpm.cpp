/*******************************************************************************
  Copyright(c) 2019 Christian Liska. All rights reserved.

  INDI Astromechanic Light Pollution Meter Driver


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

#include "lpm.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <memory>
#include <cstring>
#include <unistd.h>

// We declare an auto pointer to LPM.
std::unique_ptr<LPM> lpm(new LPM());

#define UNIT_TAB    "Unit"

void ISGetProperties(const char *dev)
{
    lpm->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    lpm->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    lpm->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    lpm->ISNewNumber(dev, name, values, names, n);
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
    lpm->ISSnoopDevice(root);
}

LPM::LPM()
{
    setVersion(0, 1);
}

bool LPM::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Average Readings
    IUFillNumber(&AverageReadingN[0], "SKY_BRIGHTNESS", "Quality (mag/arcsec^2)", "%6.2f", -20, 30, 0, 0);
    IUFillNumber(&AverageReadingN[1], "AVG_SKY_BRIGHTNESS", "Avg. Quality (mag/argsec^2)", "%6.2f", -20, 30, 0, 0);
    IUFillNumber(&AverageReadingN[2], "MIN_SKY_BRIGHTNESS", "Min. Quality (mag/argsec^2)", "%6.2f", -20, 30, 0, 0);
    IUFillNumber(&AverageReadingN[3], "MAX_SKY_BRIGHTNESS", "Max. Quality (mag/argsec^2)", "%6.2f", -20, 30, 0, 0);

    IUFillNumberVector(&AverageReadingNP, AverageReadingN, 1, getDeviceName(), "SKY_QUALITY", "Readings",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Unit Info
    IUFillNumber(&UnitInfoN[0], "Calibdata", "", "%6.2f", -20, 30, 0, 0);
    IUFillNumberVector(&UnitInfoNP, UnitInfoN, 4, getDeviceName(), "Unit Info", "", UNIT_TAB, IP_RO, 0, IPS_IDLE);

    // TODO: add reset button for SQ-Measurements

    if (lpmConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]() { return getDeviceInfo(); });
        serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
        registerConnection(serialConnection);
    }

    addDebugControl();
    addPollPeriodControl();

    return true;
}

bool LPM::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Already called by handshake
        //getDeviceInfo();

        defineNumber(&AverageReadingNP);
        defineNumber(&UnitInfoNP);
    }
    else
    {
        deleteProperty(AverageReadingNP.name);
        deleteProperty(UnitInfoNP.name);
    }

    return true;
}

bool LPM::getReadings()
{
    const char *cmd = "V#";
    char res[32] = {0};
    int nbytes_written = 0;
    int nbytes_read = 0;

    tty_write_string(PortFD, cmd, &nbytes_written);
    if (tty_read_section(PortFD, res, '#', 60000, &nbytes_read) == TTY_OK)
    {
        LOGF_INFO("RES (%s)", res);
        float mpsas;
        int rc =
            sscanf(res, "%f#", &mpsas);
        if (rc < 1)
        {
            LOGF_ERROR("Failed to parse input %s", res);
            return false;
        } else 
        {
            count++;
        }

        AverageReadingN[0].value = mpsas;
        sumSQ += mpsas;
        
        if (count > 0)
        {
            AverageReadingN[1].value = sumSQ / count;
            if (mpsas < AverageReadingN[2].value) {
                AverageReadingN[2].value = mpsas;
            }
            if (mpsas > AverageReadingN[3].value) {
                AverageReadingN[3].value = mpsas;
            }
        } else {
            AverageReadingN[2].value = mpsas;
            AverageReadingN[3].value = mpsas;
        }
    }
    return true;
}

const char *LPM::getDefaultName()
{
    return (const char *)"LPM";
}

bool LPM::getDeviceInfo()
{
    const char *cmd = "C#";
    char buffer[5]={0};

    if (getActiveConnection() == serialConnection) {
        PortFD = serialConnection->getPortFD();
    }

    LOGF_DEBUG("CMD: %s", cmd);

    ssize_t written = write(PortFD, cmd, 2);

    if (written < 2)
    {
        LOGF_ERROR("Error getting device info while writing to device: %s", strerror(errno));
        return false;
    }

    ssize_t received = 0;

    while (received < 5)
    {
        ssize_t response = read(PortFD, buffer + received, 5 - received);
        if (response < 0)
        {
            LOGF_ERROR("Error getting device info while reading response: %s", strerror(errno));
            return false;
        }

        received += response;
    }

    if (received < 5)
    {
        LOG_ERROR("Error getting device info");
        return false;
    }

    LOGF_DEBUG("RES: %s", buffer);

    float calib;
    int rc = sscanf(buffer, "%f#", &calib);

    if (rc < 1)
    {
        LOGF_ERROR("Failed to parse input %s", buffer);
        return false;
    }

    UnitInfoN[0].value = calib;

    return true;
}

void LPM::TimerHit()
{
    LOG_INFO("TimerHit");
    if (!isConnected())
        return;

    bool rc = getReadings();

    AverageReadingNP.s = rc ? IPS_OK : IPS_ALERT;
    IDSetNumber(&AverageReadingNP, nullptr);

    SetTimer(POLLMS);
}
