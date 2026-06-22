/*******************************************************************************
  Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

  Pegasus FlatMaster WiFi

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

#include "pegasus_flatmaster_wifi.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>


static std::unique_ptr<PegasusFlatMasterWifi> flatmasterwifi(new PegasusFlatMasterWifi());


PegasusFlatMasterWifi::PegasusFlatMasterWifi() : LightBoxInterface(this)
{
    setVersion(1, 0);
}

bool PegasusFlatMasterWifi::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Firmware version
    FirmwareTP[0].fill("Version", "Version", nullptr);
    FirmwareTP.fill(getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    LI::initProperties(MAIN_CONTROL_TAB, LI::CAN_DIM);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE);

    LightIntensityNP[0].setMin(0);
    LightIntensityNP[0].setMax(100);
    LightIntensityNP[0].setStep(1);

    // Uptime (seconds, read-only)
    UptimeNP[0].fill("UPTIME_SECONDS", "Uptime (s)", "%.0f", 0, 1e9, 1, 0);
    UptimeNP.fill(getDeviceName(), "UPTIME", "Uptime", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Reboot button
    RebootSP[0].fill("REBOOT", "Reboot", ISS_OFF);
    RebootSP.fill(getDeviceName(), "DEVICE_REBOOT", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    serialConnection->setPortMatchPattern("FlatMaster_WiFi");
    serialConnection->registerHandshake([&]()
    {
        return Ack();
    });

    registerConnection(serialConnection);
    return true;
}

void PegasusFlatMasterWifi::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);
    LI::ISGetProperties(dev);
}

bool PegasusFlatMasterWifi::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(FirmwareTP);
        defineProperty(UptimeNP);
        defineProperty(RebootSP);
        getStatusData();
    }
    else
    {
        deleteProperty(FirmwareTP);
        deleteProperty(UptimeNP);
        deleteProperty(RebootSP);
    }

    LI::updateProperties();
    return true;
}

const char *PegasusFlatMasterWifi::getDefaultName()
{
    return "Pegasus FlatMaster WiFi";
}

void PegasusFlatMasterWifi::updateFirmwareVersion()
{
    char response[32] = {0};

    if (sendCommand("FV", response))
    {
        // Response: FV:[MAJOR].[MINOR]  — strip the "FV:" prefix
        const char *ver = strstr(response, "FV:");
        FirmwareTP[0].setText(ver ? ver + 3 : response);
        FirmwareTP.setState(IPS_OK);
        FirmwareTP.apply();
    }
    else
    {
        FirmwareTP.setState(IPS_ALERT);
        FirmwareTP.apply();
        LOG_ERROR("Error reading firmware version.");
    }
}

bool PegasusFlatMasterWifi::Ack()
{
    PortFD = serialConnection->getPortFD();

    char response[32] = {0};
    if (sendCommand("F#", response))
    {
        if (strstr(response, "FMW") != nullptr)
        {
            updateFirmwareVersion();
            return true;
        }
    }
    LOGF_ERROR("Ack failed: unexpected response '%s'", response);
    return false;
}

bool PegasusFlatMasterWifi::EnableLightBox(bool enable)
{
    char cmd[16]      = {0};
    char response[32] = {0};

    snprintf(cmd, sizeof(cmd), "FE:%d", enable ? 1 : 0);

    if (!sendCommand(cmd, response))
    {
        LOGF_ERROR("EnableLightBox: send failed. %s", response);
        return false;
    }

    if (strstr(response, "FE:") == nullptr)
    {
        LOGF_ERROR("EnableLightBox: unexpected response '%s'", response);
        return false;
    }

    return true;
}

bool PegasusFlatMasterWifi::SetLightBoxBrightness(uint16_t value)
{
    char cmd[16]      = {0};
    char response[32] = {0};

    // Device accepts 0–100 directly
    snprintf(cmd, sizeof(cmd), "FL:%d", value);

    if (!sendCommand(cmd, response))
    {
        LOGF_ERROR("SetLightBoxBrightness: send failed. %s", response);
        return false;
    }

    if (strstr(response, "FL:") == nullptr)
    {
        LOGF_ERROR("SetLightBoxBrightness: unexpected response '%s'", response);
        return false;
    }

    return true;
}

bool PegasusFlatMasterWifi::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (LI::processNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PegasusFlatMasterWifi::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool PegasusFlatMasterWifi::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processSwitch(dev, name, states, names, n))
            return true;

        if (RebootSP.isNameMatch(name))
        {
            char response[32] = {0};
            // FQ reboots the device — no response expected
            sendCommand("FQ", response);
            RebootSP.setState(IPS_OK);
            RebootSP.apply();
            LOG_INFO("Reboot command sent to device.");
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusFlatMasterWifi::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool PegasusFlatMasterWifi::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    return LI::saveConfigItems(fp);
}

bool PegasusFlatMasterWifi::sendCommand(const char *command, char *res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    char cmd[32] = {0};
    snprintf(cmd, sizeof(cmd), "%s\n", command);

    LOGF_DEBUG("CMD <%s>", command);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, res, 0xA, 3, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return false;
    }

    // Strip trailing newline
    res[nbytes_read - 1] = 0;
    if (nbytes_read > 1 && res[nbytes_read - 2] == '\r')
        res[nbytes_read - 2] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

bool PegasusFlatMasterWifi::getStatusData()
{
    char response[32] = {0};

    if (!sendCommand("FA", response))
    {
        LOG_ERROR("Error querying device status.");
        return false;
    }

    // FA response: "FA:<brightness>:<power>"
    // brightness: 0-100, power: 0 or 1
    char *tok = strtok(response, ":");  // "FA"
    if (tok == nullptr) return false;

    char *brStr = strtok(nullptr, ":");
    char *pwStr = strtok(nullptr, ":");

    if (brStr == nullptr || pwStr == nullptr)
    {
        LOG_WARN("Unexpected FA response format.");
        return false;
    }

    const double brightness = atof(brStr);
    const bool   lightOn    = (atof(pwStr) != 0);

    // Update LightBox brightness
    if (LightIntensityNP[0].getValue() != brightness)
    {
        LightIntensityNP[0].setValue(brightness);
        LightIntensityNP.setState(IPS_OK);
        LightIntensityNP.apply();
    }

    // Update LightBox on/off switch
    if (LightSP[FLAT_LIGHT_ON].getState() != (lightOn ? ISS_ON : ISS_OFF))
    {
        LightSP.reset();
        LightSP[lightOn ? FLAT_LIGHT_ON : FLAT_LIGHT_OFF].setState(ISS_ON);
        LightSP.setState(IPS_OK);
        LightSP.apply();
    }

    return true;
}

void PegasusFlatMasterWifi::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    getStatusData();

    // Update uptime
    char response[32] = {0};
    if (sendCommand("FU", response))
    {
        // FU:<seconds>
        const char *val = strstr(response, "FU:");
        if (val)
        {
            UptimeNP[0].setValue(atof(val + 3));
            UptimeNP.setState(IPS_OK);
            UptimeNP.apply();
        }
    }

    SetTimer(getCurrentPollingPeriod());
}
