/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  Simple GPS Simulator

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

#include "flip_flat.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>

// We declare an auto pointer to FlipFlat.
static std::unique_ptr<FlipFlat> flipflat(new FlipFlat());

#define FLAT_CMD 6
#define FLAT_RES 8
#define FLAT_TIMEOUT 3

FlipFlat::FlipFlat() : LightBoxInterface(this, true)
{
    setVersion(1, 1);
}

bool FlipFlat::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Status
    IUFillText(&StatusT[0], "Cover", "Cover", nullptr);
    IUFillText(&StatusT[1], "Light", "Light", nullptr);
    IUFillText(&StatusT[2], "Motor", "Motor", nullptr);
    IUFillTextVector(&StatusTP, StatusT, 3, getDeviceName(), "Status", "Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Firmware version
    IUFillText(&FirmwareT[0], "Version", "Version", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    initDustCapProperties(getDeviceName(), MAIN_CONTROL_TAB);
    initLightBoxProperties(getDeviceName(), MAIN_CONTROL_TAB);

    LightIntensityN[0].min  = 0;
    LightIntensityN[0].max  = 255;
    LightIntensityN[0].step = 10;

    // Set DUSTCAP_INTEFACE later on connect after we verify whether it's flip-flat (dust cover + light) or just flip-man (light only)
    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

void FlipFlat::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Get Light box properties
    isGetLightBoxProperties(dev);
}

bool FlipFlat::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        if (m_Type == FLIP_FLAT || m_Type == ALNITAK_DUST_COVER)
            defineProperty(&ParkCapSP);
        defineProperty(&LightSP);
        defineProperty(&LightIntensityNP);
        defineProperty(&StatusTP);
        defineProperty(&FirmwareTP);

        updateLightBoxProperties();

        getStartupData();
    }
    else
    {
        if (m_Type == FLIP_FLAT || m_Type == ALNITAK_DUST_COVER)
            deleteProperty(ParkCapSP.name);
        deleteProperty(LightSP.name);
        deleteProperty(LightIntensityNP.name);
        deleteProperty(StatusTP.name);
        deleteProperty(FirmwareTP.name);

        updateLightBoxProperties();
    }

    return true;
}

const char *FlipFlat::getDefaultName()
{
    return "Flip Flat";
}

bool FlipFlat::Handshake()
{
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s. Retrieving startup data...", getDeviceName());

        SetTimer(getCurrentPollingPeriod());

        setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);
        syncDriverInfo();
        m_Type = FLIP_FLAT;

        return true;
    }

    PortFD = serialConnection->getPortFD();

    /* Drop RTS */
    int i = 0;
    i |= TIOCM_RTS;
    if (ioctl(PortFD, TIOCMBIC, &i) != 0)
    {
        LOGF_ERROR("IOCTL error %s.", strerror(errno));
        return false;
    }

    i |= TIOCM_RTS;
    if (ioctl(PortFD, TIOCMGET, &i) != 0)
    {
        LOGF_ERROR("IOCTL error %s.", strerror(errno));
        return false;
    }

    if (!ping())
    {
        LOG_ERROR("Device ping failed.");
        return false;
    }

    return true;
}

bool FlipFlat::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (processLightBoxNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool FlipFlat::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processLightBoxText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool FlipFlat::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processDustCapSwitch(dev, name, states, names, n))
            return true;

        if (processLightBoxSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool FlipFlat::ISSnoopDevice(XMLEle *root)
{
    snoopLightBox(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool FlipFlat::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    return saveLightBoxConfigItems(fp);
}

bool FlipFlat::ping()
{
    char response[FLAT_RES] = {0};

    if (!sendCommand(">P000", response))
        return false;

    char productString[3] = { 0 };
    snprintf(productString, 3, "%s", response + 2);

    int rc = sscanf(productString, "%" SCNu16, &productID);
    if (rc <= 0)
    {
        LOGF_ERROR("Unable to parse input (%s)", response);
        return false;
    }

    // Flip Flat
    if (productID == 99)
    {
        setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);
        syncDriverInfo();
        m_Type = FLIP_FLAT;
    }
    // AlNitak Dust Cover
    else if (productID == 98)
    {
        setDriverInterface(AUX_INTERFACE | DUSTCAP_INTERFACE);
        syncDriverInfo();
    }
    // Flip Man
    else
        m_Type = FLIP_MAN;

    return true;
}

bool FlipFlat::getStartupData()
{
    bool rc1 = getFirmwareVersion();
    bool rc2 = getStatus();
    bool rc3 = getBrightness();

    return (rc1 && rc2 && rc3);
}

IPState FlipFlat::ParkCap()
{
    if (isSimulation())
    {
        simulationWorkCounter = 3;
        return IPS_BUSY;
    }

    char response[FLAT_RES];
    if (!sendCommand(">C000", response))
        return IPS_ALERT;

    char expectedResponse[FLAT_RES];
    snprintf(expectedResponse, FLAT_RES, "*C%02d000", productID);

    if (strcmp(response, expectedResponse) == 0)
    {
        // Set cover status to random value outside of range to force it to refresh
        prevCoverStatus = 10;

        IERmTimer(parkTimeoutID);
        parkTimeoutID = IEAddTimer(30000, parkTimeoutHelper, this);
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

IPState FlipFlat::UnParkCap()
{
    if (isSimulation())
    {
        simulationWorkCounter = 3;
        return IPS_BUSY;
    }

    char response[FLAT_RES];
    if (!sendCommand(">O000", response))
        return IPS_ALERT;

    char expectedResponse[FLAT_RES];
    snprintf(expectedResponse, FLAT_RES, "*O%02d000", productID);

    if (strcmp(response, expectedResponse) == 0)
    {
        // Set cover status to random value outside of range to force it to refresh
        prevCoverStatus = 10;

        IERmTimer(unparkTimeoutID);
        unparkTimeoutID = IEAddTimer(30000, unparkTimeoutHelper, this);

        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

bool FlipFlat::EnableLightBox(bool enable)
{
    char command[FLAT_CMD];
    char response[FLAT_RES];

    if (m_Type == FLIP_FLAT && ParkCapS[1].s == ISS_ON)
    {
        LOG_ERROR("Cannot control light while cap is unparked.");
        return false;
    }

    if (isSimulation())
        return true;

    if (enable)
        strncpy(command, ">L000", FLAT_CMD);
    else
        strncpy(command, ">D000", FLAT_CMD);

    if (!sendCommand(command, response))
        return false;

    char expectedResponse[FLAT_RES];
    if (enable)
        snprintf(expectedResponse, FLAT_RES, "*L%02d000", productID);
    else
        snprintf(expectedResponse, FLAT_RES, "*D%02d000", productID);

    if (strcmp(response, expectedResponse) == 0)
        return true;

    return false;
}

bool FlipFlat::getStatus()
{
    char response[FLAT_RES];

    if (isSimulation())
    {
        if (ParkCapSP.s == IPS_BUSY && --simulationWorkCounter <= 0)
        {
            ParkCapSP.s = IPS_OK;
            IDSetSwitch(&ParkCapSP, nullptr);
            simulationWorkCounter = 0;
        }

        if (ParkCapSP.s == IPS_BUSY)
        {
            response[4] = '1';
            response[6] = '0';
        }
        else
        {
            response[4] = '0';
            // Parked/Closed
            if (ParkCapS[CAP_PARK].s == ISS_ON)
                response[6] = '1';
            else
                response[6] = '2';
        }

        response[5] = (LightS[FLAT_LIGHT_ON].s == ISS_ON) ? '1' : '0';
    }
    else
    {
        if (!sendCommand(">S000", response))
            return false;
    }

    char motorStatus = *(response + 4) - '0';
    char lightStatus = *(response + 5) - '0';
    char coverStatus = *(response + 6) - '0';

    bool statusUpdated = false;

    if (coverStatus != prevCoverStatus)
    {
        prevCoverStatus = coverStatus;

        statusUpdated = true;

        switch (coverStatus)
        {
            case 0:
                IUSaveText(&StatusT[0], "Not Open/Closed");
                break;

            case 1:
                IUSaveText(&StatusT[0], "Closed");
                if (ParkCapSP.s == IPS_BUSY || ParkCapSP.s == IPS_IDLE)
                {
                    IUResetSwitch(&ParkCapSP);
                    ParkCapS[0].s = ISS_ON;
                    ParkCapSP.s   = IPS_OK;
                    LOG_INFO("Cover closed.");
                    IDSetSwitch(&ParkCapSP, nullptr);
                }
                break;

            case 2:
                IUSaveText(&StatusT[0], "Open");
                if (ParkCapSP.s == IPS_BUSY || ParkCapSP.s == IPS_IDLE)
                {
                    IUResetSwitch(&ParkCapSP);
                    ParkCapS[1].s = ISS_ON;
                    ParkCapSP.s   = IPS_OK;
                    LOG_INFO("Cover open.");
                    IDSetSwitch(&ParkCapSP, nullptr);
                }
                break;

            case 3:
                IUSaveText(&StatusT[0], "Timed out");
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
                IUSaveText(&StatusT[1], "Off");
                if (LightS[0].s == ISS_ON)
                {
                    LightS[0].s = ISS_OFF;
                    LightS[1].s = ISS_ON;
                    IDSetSwitch(&LightSP, nullptr);
                }
                break;

            case 1:
                IUSaveText(&StatusT[1], "On");
                if (LightS[1].s == ISS_ON)
                {
                    LightS[0].s = ISS_ON;
                    LightS[1].s = ISS_OFF;
                    IDSetSwitch(&LightSP, nullptr);
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
                IUSaveText(&StatusT[2], "Stopped");
                break;

            case 1:
                IUSaveText(&StatusT[2], "Running");
                break;
        }
    }

    if (statusUpdated)
        IDSetText(&StatusTP, nullptr);

    return true;
}

bool FlipFlat::getFirmwareVersion()
{
    if (isSimulation())
    {
        IUSaveText(&FirmwareT[0], "Simulation");
        IDSetText(&FirmwareTP, nullptr);
        return true;
    }

    char response[FLAT_RES] = {0};
    if (!sendCommand(">V000", response))
        return false;

    char versionString[4] = { 0 };
    snprintf(versionString, 4, "%s", response + 4);
    IUSaveText(&FirmwareT[0], versionString);
    IDSetText(&FirmwareTP, nullptr);

    return true;
}

void FlipFlat::TimerHit()
{
    if (!isConnected())
        return;

    getStatus();

    // parking or unparking timed out, try again
    if (ParkCapSP.s == IPS_BUSY && !strcmp(StatusT[0].text, "Timed out"))
    {
        if (ParkCapS[0].s == ISS_ON)
            ParkCap();
        else
            UnParkCap();
    }

    SetTimer(getCurrentPollingPeriod());
}

bool FlipFlat::getBrightness()
{
    if (isSimulation())
    {
        return true;
    }

    char response[FLAT_RES] = {0};
    if (!sendCommand(">J000", response))
        return false;

    char brightnessString[4] = { 0 };
    snprintf(brightnessString, 4, "%s", response + 4);

    int brightnessValue = 0;
    int rc              = sscanf(brightnessString, "%d", &brightnessValue);

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

bool FlipFlat::SetLightBoxBrightness(uint16_t value)
{
    if (isSimulation())
    {
        LightIntensityN[0].value = value;
        IDSetNumber(&LightIntensityNP, nullptr);
        return true;
    }

    char command[FLAT_CMD] = {0};
    char response[FLAT_RES] = {0};

    snprintf(command, FLAT_CMD, ">B%03d", value);

    if (!sendCommand(command, response))
        return false;

    char brightnessString[4] = { 0 };
    snprintf(brightnessString, 4, "%s", response + 4);

    int brightnessValue = 0;
    int rc              = sscanf(brightnessString, "%d", &brightnessValue);

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

bool FlipFlat::sendCommand(const char *command, char *response)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    int i = 0;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", command);

    char buffer[FLAT_CMD + 1] = {0}; // space for terminating null
    snprintf(buffer, FLAT_CMD + 1, "%s\n", command);

    for (i = 0; i < 3; i++)
    {
        if ((rc = tty_write(PortFD, buffer, FLAT_CMD, &nbytes_written)) != TTY_OK)
        {
            usleep(50000);
            continue;
        }

        if ((rc = tty_nread_section(PortFD, response, FLAT_RES, 0xA, FLAT_TIMEOUT, &nbytes_read)) != TTY_OK)
            usleep(50000);
        else
            break;
    }

    if (i == 3)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s error: %s.", command, errstr);
        return false;
    }

    response[nbytes_read - 1] = 0; // strip \n

    LOGF_DEBUG("RES <%s>", response);
    return true;
}

void FlipFlat::parkTimeoutHelper(void *context)
{
    static_cast<FlipFlat*>(context)->parkTimeout();
}

void FlipFlat::unparkTimeoutHelper(void *context)
{
    static_cast<FlipFlat*>(context)->unparkTimeout();
}

void FlipFlat::parkTimeout()
{
    if (ParkCapSP.s == IPS_BUSY)
    {
        LOG_WARN("Parking cap timed out. Retrying...");
        ParkCap();
    }
}

void FlipFlat::unparkTimeout()
{
    if (ParkCapSP.s == IPS_BUSY)
    {
        LOG_WARN("UnParking cap timed out. Retrying...");
        UnParkCap();
    }
}
