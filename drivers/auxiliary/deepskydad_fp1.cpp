/*******************************************************************************
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

#include "deepskydad_fp1.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>

// We declare an auto pointer to DeepSkyDadFP1.
static std::unique_ptr<DeepSkyDadFP1> dsdFp1(new DeepSkyDadFP1());

#define FLAT_CMD 40
#define FLAT_RES 40
#define FLAT_TIMEOUT 3

DeepSkyDadFP1::DeepSkyDadFP1() : LightBoxInterface(this, true)
{
    setVersion(1, 1);
}

bool DeepSkyDadFP1::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Heater mode
    IUFillSwitch(&HeaterModeS[Off], "OFF", "Off", ISS_OFF);
    IUFillSwitch(&HeaterModeS[On], "ON", "On", ISS_OFF);
    IUFillSwitch(&HeaterModeS[OnIfFlapOpenOrLedActive], "ON2", "On if flap open/LED active", ISS_OFF);
    IUFillSwitchVector(&HeaterModeSP, HeaterModeS, 3, getDeviceName(), "Heater mode", "Heater mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Status
    IUFillText(&StatusT[0], "Cover", "Cover", nullptr);
    IUFillText(&StatusT[1], "Light", "Light", nullptr);
    IUFillText(&StatusT[2], "Motor", "Motor", nullptr);
    IUFillText(&StatusT[3], "Heater", "Heater", nullptr);
    IUFillTextVector(&StatusTP, StatusT, 4, getDeviceName(), "Status", "Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Firmware version
    IUFillText(&FirmwareT[0], "Version", "Version", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    initDustCapProperties(getDeviceName(), MAIN_CONTROL_TAB);
    initLightBoxProperties(getDeviceName(), MAIN_CONTROL_TAB);

    LightIntensityN[0].min  = 0;
    LightIntensityN[0].max  = 4096;
    LightIntensityN[0].step = 1;

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]() { return Handshake(); });
    registerConnection(serialConnection);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    return true;
}

void DeepSkyDadFP1::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Get Light box properties
    isGetLightBoxProperties(dev);
}

bool DeepSkyDadFP1::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
		defineProperty(&ParkCapSP);
        defineProperty(&LightSP);
        defineProperty(&LightIntensityNP);
        defineProperty(&HeaterModeSP);
        defineProperty(&StatusTP);
        defineProperty(&FirmwareTP);

        updateLightBoxProperties();

        getStartupData();
    }
    else
    {
		deleteProperty(ParkCapSP.name);
        deleteProperty(LightSP.name);
        deleteProperty(LightIntensityNP.name);
        deleteProperty(HeaterModeSP.name);
        deleteProperty(StatusTP.name);
        deleteProperty(FirmwareTP.name);

        updateLightBoxProperties();
    }

    return true;
}

const char *DeepSkyDadFP1::getDefaultName()
{
    return "Deep Sky Dad FP1";
}

bool DeepSkyDadFP1::Handshake()
{
    PortFD = serialConnection->getPortFD();
    if (!ping())
    {
        LOG_ERROR("Device ping failed.");
        return false;
    }
	
	setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);
	syncDriverInfo();

    return true;
}

bool DeepSkyDadFP1::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (processLightBoxNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool DeepSkyDadFP1::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processLightBoxText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool DeepSkyDadFP1::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processDustCapSwitch(dev, name, states, names, n))
            return true;

        if (processLightBoxSwitch(dev, name, states, names, n))
            return true;

        if (strcmp(HeaterModeSP.name, name) == 0)
        {
            int current_mode = IUFindOnSwitchIndex(&HeaterModeSP);

            IUUpdateSwitch(&HeaterModeSP, states, names, n);

            int target_mode = IUFindOnSwitchIndex(&HeaterModeSP);

            if (current_mode == target_mode)
            {
                HeaterModeSP.s = IPS_OK;
                IDSetSwitch(&HeaterModeSP, nullptr);
                return true;
            }

            char cmd[FLAT_RES] = {0};
            char response[FLAT_RES] = {0};

            snprintf(cmd, FLAT_RES, "[SHTM%d]", target_mode);
            bool rc = sendCommand(cmd, response);
            if (!rc)
            {
                IUResetSwitch(&HeaterModeSP);
                HeaterModeS[current_mode].s = ISS_ON;
                HeaterModeSP.s              = IPS_ALERT;
                IDSetSwitch(&HeaterModeSP, nullptr);
                return false;
            }

            HeaterModeSP.s = IPS_OK;
            IDSetSwitch(&HeaterModeSP, nullptr);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool DeepSkyDadFP1::ISSnoopDevice(XMLEle *root)
{
    snoopLightBox(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool DeepSkyDadFP1::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &HeaterModeSP);

    return saveLightBoxConfigItems(fp);
}

bool DeepSkyDadFP1::ping()
{
    char response[FLAT_RES] = {0};

    if (!sendCommand("[GPOS]", response))
        return false;

    return true;
}

void DeepSkyDadFP1::TimerHit()
{
    if (!isConnected())
        return;

    getStatus();

    SetTimer(getCurrentPollingPeriod());
}

bool DeepSkyDadFP1::getStartupData()
{
    bool rc1 = getFirmwareVersion();
    bool rc2 = getStatus();
    bool rc3 = getBrightness();

    return (rc1 && rc2 && rc3);
}

IPState DeepSkyDadFP1::ParkCap()
{
    char response[FLAT_RES];
    if (!sendCommand("[STRG270]", response) || !sendCommand("[SMOV]", response))
        return IPS_ALERT;

    if (strcmp(response, "(OK)") == 0)
    {
        // Set cover status to random value outside of range to force it to refresh
        prevCoverStatus = 10;
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

IPState DeepSkyDadFP1::UnParkCap()
{
	char response[FLAT_RES];
    if (!sendCommand("[STRG0]", response) || !sendCommand("[SMOV]", response))
        return IPS_ALERT;

    if (strcmp(response, "(OK)") == 0)
    {
        // Set cover status to random value outside of range to force it to refresh
        prevCoverStatus = 10;
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

bool DeepSkyDadFP1::EnableLightBox(bool enable)
{
    char command[FLAT_CMD];
    char response[FLAT_RES];

    if (enable)
        strncpy(command, "[SLON1]", FLAT_CMD);
    else
        strncpy(command, "[SLON0]", FLAT_CMD);

    if (!sendCommand(command, response))
        return false;

    if (strcmp(response, "(OK)") == 0)
        return true;

    return false;
}

bool DeepSkyDadFP1::getStatus()
{
    char response[FLAT_RES];

	int motorStatus;
    int lightStatus;
    int coverStatus;
    int heaterTemperature;
    int heaterMode;
	
	if (!sendCommand("[GMOV]", response))
		return false;
	else
		sscanf(response, "(%d)", &motorStatus);
	
	if (!sendCommand("[GLON]", response))
		return false;
	else
		sscanf(response, "(%d)", &lightStatus);
	
	if (!sendCommand("[GPOS]", response))
		return false;
	else
		sscanf(response, "(%d)", &coverStatus);

    if (!sendCommand("[GHTT]", response))
        return false;
    else
        sscanf(response, "(%d)", &heaterTemperature);

    if (!sendCommand("[GHTM]", response))
        return false;
    else
        sscanf(response, "(%d)", &heaterMode);


    bool statusUpdated = false;

    if (coverStatus != prevCoverStatus)
    {
        if(motorStatus == 1) {
            if(coverStatus == 0) {
                IUSaveText(&StatusT[0], "Open");
            } else if(coverStatus == 270) {
                IUSaveText(&StatusT[0], "Closed");
            } else {
                IUSaveText(&StatusT[0], "Not open/closed");
            }
        } else {
            prevCoverStatus = coverStatus;

            statusUpdated = true;

            if(coverStatus == 0) {
                IUSaveText(&StatusT[0], "Open");
                if (ParkCapSP.s == IPS_BUSY || ParkCapSP.s == IPS_IDLE)
                {
                    IUResetSwitch(&ParkCapSP);
                    ParkCapS[0].s = ISS_OFF;
                    ParkCapS[1].s = ISS_ON;
                    ParkCapSP.s   = IPS_OK;
                    LOG_INFO("Cover open.");
                    IDSetSwitch(&ParkCapSP, nullptr);
                }
            } else if(coverStatus == 270) {
                IUSaveText(&StatusT[0], "Closed");
                if (ParkCapSP.s == IPS_BUSY || ParkCapSP.s == IPS_IDLE)
                {
                    IUResetSwitch(&ParkCapSP);
                    ParkCapS[0].s = ISS_ON;
                    ParkCapS[1].s = ISS_OFF;
                    ParkCapSP.s   = IPS_OK;
                    LOG_INFO("Cover closed.");
                    IDSetSwitch(&ParkCapSP, nullptr);
                }
            } else {
                IUSaveText(&StatusT[0], "Not open/closed");
            }
        }
    }

    if(motorStatus == 1) {
        IUSaveText(&StatusT[0], "Moving");
    }

    if (lightStatus != prevLightStatus)
    {
        prevLightStatus = lightStatus;

        statusUpdated = true;

        switch (lightStatus)
        {
            case 0:
                IUSaveText(&StatusT[1], "Off");
                LightS[0].s = ISS_OFF;
                LightS[1].s = ISS_ON;
                IDSetSwitch(&LightSP, nullptr);
                break;

            case 1:
            IUSaveText(&StatusT[1], "On");
                LightS[0].s = ISS_ON;
                LightS[1].s = ISS_OFF;
                IDSetSwitch(&LightSP, nullptr);
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
                IUSaveText(&StatusT[2], "Stopped");
                break;

            case 1:
                IUSaveText(&StatusT[2], "Running");
                break;
        }
    }

    if (statusUpdated)
        IDSetText(&StatusTP, nullptr);

    int heaterConnected = heaterTemperature > -40;
    if(heaterConnected != prevHeaterConnected) {
        prevHeaterConnected = heaterConnected;
        if(heaterConnected == 1) {
            IUSaveText(&StatusT[3], "Connected");
            HeaterModeSP.s   = IPS_OK;
        } else {
            HeaterModeSP.s   = IPS_IDLE;
            IUSaveText(&StatusT[3], "Disconnected");
        }
    }

    if(heaterMode != prevHeaterMode) {
        prevHeaterMode = heaterMode;
        IUResetSwitch(&HeaterModeSP);
        if(heaterMode == 0) {
           HeaterModeS[0].s = ISS_ON;
        } else if(heaterMode == 1) {
            HeaterModeS[1].s = ISS_ON;
        } else if(heaterMode == 2) {
            HeaterModeS[2].s = ISS_ON;
        }
        IDSetSwitch(&HeaterModeSP, nullptr);
    }

    return true;
}

bool DeepSkyDadFP1::getFirmwareVersion()
{
    char response[FLAT_RES] = {0};
    if (!sendCommand("[GFRM]", response))
        return false;

    char versionString[6] = { 0 };
    snprintf(versionString, 6, "%s", response + 31);
    IUSaveText(&FirmwareT[0], response);
    IDSetText(&FirmwareTP, nullptr);

    return true;
}

bool DeepSkyDadFP1::getBrightness()
{
    char response[FLAT_RES] = {0};
    if (!sendCommand("[GLBR]", response))
        return false;

    int32_t brightnessValue;
    int rc = sscanf(response, "(%d)", &brightnessValue);

    if (rc <= 0)
    {
        LOGF_ERROR("Unable to parse brightness value (%s)", response);
        return false;
    }

    if (brightnessValue != prevBrightness)
    {
        prevBrightness           = brightnessValue;
        LightIntensityN[0].value = brightnessValue;
        IDSetNumber(&LightIntensityNP, nullptr);
    }

    return true;
}

bool DeepSkyDadFP1::SetLightBoxBrightness(uint16_t value)
{
    char command[FLAT_CMD] = {0};
    char response[FLAT_RES] = {0};
	snprintf(command, FLAT_CMD, "[SLBR%04d]", value);

    if (!sendCommand(command, response))
        return false;

    if (strcmp(response, "(OK)") != 0)
    {
        LOGF_ERROR("Unable to set brightness value", response);
        return false;
    }

    return true;
}

bool DeepSkyDadFP1::sendCommand(const char *cmd, char *res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if ((rc = tty_nread_section(PortFD, res, FLAT_RES, ')', FLAT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}
