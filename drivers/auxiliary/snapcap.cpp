/*******************************************************************************
  Copyright(c) 2017 Jarno Paananen. All right reserved.

  Based on Flip Flat driver by:

  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

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

#include "snapcap.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <termios.h>
#include <sys/ioctl.h>

// We declare an auto pointer to SnapCap.
std::unique_ptr<SnapCap> snapcap(new SnapCap());

#define SNAP_CMD 7
#define SNAP_RES 8 // Includes terminating null
#define SNAP_TIMEOUT 3

void ISGetProperties(const char *dev)
{
    snapcap->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    snapcap->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    snapcap->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    snapcap->ISNewNumber(dev, name, values, names, n);
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
    snapcap->ISSnoopDevice(root);
}

SnapCap::SnapCap() : LightBoxInterface(this, true)
{
    setVersion(1, 1);
}

bool SnapCap::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Status
    StatusTP[0].fill("Cover", "", nullptr);
    StatusTP[1].fill("Light", "", nullptr);
    StatusTP[2].fill("Motor", "", nullptr);
    StatusTP.fill(getDeviceName(), "Status", "", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Firmware version
    FirmwareTP[0].fill("Version", "", nullptr);
    FirmwareTP.fill(getDeviceName(), "Firmware", "", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Abort and force open/close buttons
    AbortSP[0].fill("Abort", "", ISS_OFF);
    AbortSP.fill(getDeviceName(), "Abort", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    ForceSP[0].fill("Off", "", ISS_ON);
    ForceSP[1].fill("On", "", ISS_OFF);
    ForceSP.fill(getDeviceName(), "Force movement", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    initDustCapProperties(getDeviceName(), MAIN_CONTROL_TAB);
    initLightBoxProperties(getDeviceName(), MAIN_CONTROL_TAB);

    LightIntensityNP[0].setMin(0);
    LightIntensityNP[0].setMax(255);
    LightIntensityNP[0].setStep(10);

    hasLight = true;

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]() { return Handshake(); });
    registerConnection(serialConnection);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_38400);
    return true;
}

void SnapCap::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Get Light box properties
    isGetLightBoxProperties(dev);
}

bool SnapCap::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&ParkCapSP);
        if (hasLight)
        {
            defineProperty(LightSP);
            defineProperty(LightIntensityNP);
            updateLightBoxProperties();
        }
        defineProperty(StatusTP);
        defineProperty(FirmwareTP);
        defineProperty(AbortSP);
        defineProperty(ForceSP);
        getStartupData();
    }
    else
    {
        deleteProperty(ParkCapSP.name);
        if (hasLight)
        {
            deleteProperty(LightSP.getName());
            deleteProperty(LightIntensityNP.getName());
            updateLightBoxProperties();
        }
        deleteProperty(StatusTP.getName());
        deleteProperty(FirmwareTP.getName());
        deleteProperty(AbortSP.getName());
        deleteProperty(ForceSP.getName());
    }

    return true;
}

const char *SnapCap::getDefaultName()
{
    return (const char *)"SnapCap";
}

bool SnapCap::Handshake()
{
    if (isSimulation())
    {
        LOGF_INFO("Connected successfully to simulated %s. Retrieving startup data...", getDeviceName());

        SetTimer(getCurrentPollingPeriod());
        return true;
    }

    PortFD = serialConnection->getPortFD();

    if (!ping())
    {
        LOG_ERROR("Device ping failed.");
        return false;
    }

    return true;
}

bool SnapCap::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (processLightBoxNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool SnapCap::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processLightBoxText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool SnapCap::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (AbortSP.isNameMatch(name))
        {
            AbortSP.reset();
            AbortSP.setState(Abort());
            AbortSP.apply();
            return true;
        }
        if (ForceSP.isNameMatch(name))
        {
            ForceSP.update(states, names, n);
            AbortSP.apply();
            return true;
        }

        if (processDustCapSwitch(dev, name, states, names, n))
            return true;

        if (processLightBoxSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool SnapCap::ISSnoopDevice(XMLEle *root)
{
    snoopLightBox(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool SnapCap::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    return saveLightBoxConfigItems(fp);
}

bool SnapCap::ping()
{
    bool found = getFirmwareVersion();
    // Sometimes the controller does a corrupt reply at first connect
    // so retry once just in case
    if (!found)
        found = getFirmwareVersion();
    return found;
}

bool SnapCap::sendCommand(const char *command, char *response)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", command);

    char buffer[SNAP_CMD + 1]; // space for terminating null
    snprintf(buffer, SNAP_CMD + 1, "%s\r\n", command);

    if ((rc = tty_write(PortFD, buffer, SNAP_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", command, errstr);
        return false;
    }

    if ((rc = tty_nread_section(PortFD, response, SNAP_RES, '\n', SNAP_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s.", command, errstr);
        return false;
    }

    response[nbytes_read - 2] = 0; // strip \r\n

    LOGF_DEBUG("RES (%s)", response);
    return true;
}

bool SnapCap::getStartupData()
{
    bool rc1 = getFirmwareVersion();
    bool rc2 = getStatus();
    bool rc3 = getBrightness();

    return (rc1 && rc2 && rc3);
}

IPState SnapCap::ParkCap()
{
    if (isSimulation())
    {
        simulationWorkCounter = 3;
        return IPS_BUSY;
    }

    char command[SNAP_CMD];
    char response[SNAP_RES];

    if (ForceSP[1].getState() == ISS_ON)
        strncpy(command, ">c000", SNAP_CMD); // Force close command
    else
        strncpy(command, ">C000", SNAP_CMD);

    if (!sendCommand(command, response))
        return IPS_ALERT;

    if (strcmp(response, "*C000") == 0 || strcmp(response, "*c000") == 0)
    {
        // Set cover status to random value outside of range to force it to refresh
        prevCoverStatus   = 10;
        targetCoverStatus = 2;
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

IPState SnapCap::UnParkCap()
{
    if (isSimulation())
    {
        simulationWorkCounter = 3;
        return IPS_BUSY;
    }

    char command[SNAP_CMD];
    char response[SNAP_RES];

    if (ForceSP[1].getState() == ISS_ON)
        strncpy(command, ">o000", SNAP_CMD); // Force open command
    else
        strncpy(command, ">O000", SNAP_CMD);

    if (!sendCommand(command, response))
        return IPS_ALERT;

    if (strcmp(response, "*O000") == 0 || strcmp(response, "*o000") == 0)
    {
        // Set cover status to random value outside of range to force it to refresh
        prevCoverStatus   = 10;
        targetCoverStatus = 1;
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

IPState SnapCap::Abort()
{
    if (isSimulation())
    {
        simulationWorkCounter = 0;
        return IPS_OK;
    }

    char response[SNAP_RES];

    if (!sendCommand(">A000", response))
        return IPS_ALERT;

    if (strcmp(response, "*A000") == 0)
    {
        // Set cover status to random value outside of range to force it to refresh
        prevCoverStatus = 10;
        return IPS_OK;
    }
    else
        return IPS_ALERT;
}

bool SnapCap::EnableLightBox(bool enable)
{
    char command[SNAP_CMD];
    char response[SNAP_RES];

    if (isSimulation())
        return true;

    if (enable)
        strncpy(command, ">L000", SNAP_CMD);
    else
        strncpy(command, ">D000", SNAP_CMD);

    if (!sendCommand(command, response))
        return false;

    char expectedResponse[SNAP_RES];
    if (enable)
        snprintf(expectedResponse, SNAP_RES, "*L000");
    else
        snprintf(expectedResponse, SNAP_RES, "*D000");

    if (strcmp(response, expectedResponse) == 0)
        return true;

    return false;
}

bool SnapCap::getStatus()
{
    char response[SNAP_RES];

    if (isSimulation())
    {
        if (ParkCapSP.s == IPS_BUSY && --simulationWorkCounter <= 0)
        {
            ParkCapSP.s = IPS_IDLE;
            IDSetSwitch(&ParkCapSP, nullptr);
            simulationWorkCounter = 0;
        }

        if (ParkCapSP.s == IPS_BUSY)
        {
            response[2] = '1';
            response[4] = '0';
        }
        else
        {
            response[2] = '0';
            // Parked/Closed
            if (ParkCapS[CAP_PARK].s == ISS_ON)
                response[4] = '2';
            else
                response[4] = '1';
        }

        response[3] = (LightSP[FLAT_LIGHT_ON].getState() == ISS_ON) ? '1' : '0';
    }
    else
    {
        if (!sendCommand(">S000", response))
            return false;
    }

    char motorStatus = response[2] - '0';
    char lightStatus = response[3] - '0';
    char coverStatus = response[4] - '0';

    // Force cover status as it doesn't reflect moving state otherwise...
    if (motorStatus)
    {
        coverStatus = 0;
    }
    bool statusUpdated = false;

    if (coverStatus != prevCoverStatus)
    {
        prevCoverStatus = coverStatus;

        statusUpdated = true;

        switch (coverStatus)
        {
            case 0:
                StatusTP[0].setText("Opening/closing");
                break;

            case 1:
                if ((targetCoverStatus == 1 && ParkCapSP.s == IPS_BUSY) || ParkCapSP.s == IPS_IDLE)
                {
                    StatusTP[0].setText("Open");
                    IUResetSwitch(&ParkCapSP);
                    ParkCapS[CAP_UNPARK].s = ISS_ON;
                    ParkCapSP.s            = IPS_OK;
                    LOG_INFO("Cover open.");
                    IDSetSwitch(&ParkCapSP, nullptr);
                }
                break;

            case 2:
                if ((targetCoverStatus == 2 && ParkCapSP.s == IPS_BUSY) || ParkCapSP.s == IPS_IDLE)
                {
                    StatusTP[0].setText("Closed");
                    IUResetSwitch(&ParkCapSP);
                    ParkCapS[CAP_PARK].s = ISS_ON;
                    ParkCapSP.s          = IPS_OK;
                    LOG_INFO("Cover closed.");
                    IDSetSwitch(&ParkCapSP, nullptr);
                }
                break;

            case 3:
                StatusTP[0].setText("Timed out");
                break;

            case 4:
                StatusTP[0].setText("Open circuit");
                break;

            case 5:
                StatusTP[0].setText("Overcurrent");
                break;

            case 6:
                StatusTP[0].setText("User abort");
                break;
        }
    }

    if (lightStatus != prevLightStatus)
    {
        prevLightStatus = lightStatus;

        statusUpdated = true;

        switch (lightStatus)
        {
            case 0:
                StatusTP[1].setText("Off");
                if (LightSP[0].getState() == ISS_ON)
                {
                    LightSP[0].setState(ISS_OFF);
                    LightSP[1].setState(ISS_ON);
                    LightSP.apply();
                }
                break;

            case 1:
                StatusTP[1].setText("On");
                if (LightSP[1].getState() == ISS_ON)
                {
                    LightSP[0].setState(ISS_ON);
                    LightSP[1].setState(ISS_OFF);
                    LightSP.apply();
                }
                break;
        }
    }

    if (motorStatus != prevMotorStatus)
    {
        prevMotorStatus = motorStatus;

        statusUpdated = true;

        switch (motorStatus)
        {
            case 0:
                StatusTP[2].setText("Stopped");
                break;

            case 1:
                StatusTP[2].setText("Running");
                break;
        }
    }

    if (statusUpdated)
        StatusTP.apply();

    return true;
}

bool SnapCap::getFirmwareVersion()
{
    if (isSimulation())
    {
        FirmwareTP[0].setText("Simulation");
        FirmwareTP.apply();
        return true;
    }

    char response[SNAP_RES];

    if (!sendCommand(">V000", response))
        return false;

    char versionString[4] = { 0 };
    snprintf(versionString, 4, "%s", response + 2);
    FirmwareTP[0].setText(versionString);
    FirmwareTP.apply();

    return true;
}

void SnapCap::TimerHit()
{
    if (!isConnected())
        return;

    getStatus();

    SetTimer(getCurrentPollingPeriod());
}

bool SnapCap::getBrightness()
{
    if (isSimulation())
    {
        return true;
    }

    char response[SNAP_RES];

    if (!sendCommand(">J000", response))
        return false;

    int brightnessValue = 0;
    int rc              = sscanf(response, "*J%d", &brightnessValue);

    if (rc <= 0)
    {
        LOGF_ERROR("Unable to parse brightness value (%s)", response);
        return false;
    }

    if (brightnessValue != prevBrightness)
    {
        prevBrightness           = brightnessValue;
        LightIntensityNP[0].setValue(brightnessValue);
        LightIntensityNP.apply();
    }

    return true;
}

bool SnapCap::SetLightBoxBrightness(uint16_t value)
{
    if (isSimulation())
    {
        LightIntensityNP[0].setValue(value);
        LightIntensityNP.apply();
        return true;
    }

    char command[SNAP_CMD];
    char response[SNAP_RES];

    snprintf(command, SNAP_CMD, ">B%03d", value);

    if (!sendCommand(command, response))
        return false;

    int brightnessValue = 0;
    int rc              = sscanf(response, "*B%d", &brightnessValue);

    if (rc <= 0)
    {
        LOGF_ERROR("Unable to parse brightness value (%s)", response);
        return false;
    }

    if (brightnessValue != prevBrightness)
    {
        prevBrightness           = brightnessValue;
        LightIntensityNP[0].setValue(brightnessValue);
        LightIntensityNP.apply();
    }

    return true;
}
