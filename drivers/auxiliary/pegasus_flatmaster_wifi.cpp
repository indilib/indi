/*******************************************************************************
  Copyright(c) 2026 Dominik Merkel. All rights reserved.

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
#include <regex>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>


static std::unique_ptr<PegasusFlatMasterWiFi> flatmasterwifi(new PegasusFlatMasterWiFi());


PegasusFlatMasterWiFi::PegasusFlatMasterWiFi() : LightBoxInterface(this)
{
    setVersion(1, 0);
}

bool PegasusFlatMasterWiFi::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Firmware version (FV command → "FV:MAJOR.MINOR")
    IUFillText(&FirmwareT[0], "Version", "Version", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    LI::initProperties(LIGHTBOX_TAB, LI::CAN_DIM);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE);

    LightIntensityNP[0].setMin(0);
    LightIntensityNP[0].setMax(100);
    LightIntensityNP[0].setStep(1);

    // Uptime (FU command → "FU:seconds")
    UptimeNP[0].fill("UPTIME_SECONDS", "Uptime (s)", "%.0f", 0, 1e9, 1, 0);
    UptimeNP.fill(getDeviceName(), "DEVICE_UPTIME", "Uptime", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Reboot button (FQ command)
    RebootSP[0].fill("REBOOT", "Reboot", ISS_OFF);
    RebootSP.fill(getDeviceName(), "DEVICE_REBOOT", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    serialConnection->registerHandshake([&]()
    {
        return Ack();
    });

    registerConnection(serialConnection);
    return true;
}

void PegasusFlatMasterWiFi::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);
    LI::ISGetProperties(dev);
}

bool PegasusFlatMasterWiFi::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&FirmwareTP);
        defineProperty(UptimeNP);
        defineProperty(RebootSP);
    }
    else
    {
        deleteProperty(FirmwareTP.name);
        deleteProperty(UptimeNP);
        deleteProperty(RebootSP);
    }

    LI::updateProperties();

    return true;
}

const char *PegasusFlatMasterWiFi::getDefaultName()
{
    return "Pegasus FlatMaster WiFi";
}

void PegasusFlatMasterWiFi::updateFirmwareVersion()
{
    char response[16] = {0};

    // Response format: "FV:MAJOR.MINOR"
    if (sendCommand("FV", response))
    {
        IUSaveText(&FirmwareT[0], response + 3);  // skip "FV:"
        FirmwareTP.s = IPS_OK;
        IDSetText(&FirmwareTP, nullptr);
    }
    else
    {
        FirmwareTP.s = IPS_ALERT;
        LOG_ERROR("Error on updateFirmwareVersion.");
    }
}

bool PegasusFlatMasterWiFi::Ack()
{
    PortFD = serialConnection->getPortFD();

    char response[16] = {0};

    // Device responds to F# with "FMW<8-char-id>" — check for "FMW" prefix
    if (sendCommand("F#", response))
    {
        if (strncmp(response, "FMW", 3) == 0)
        {
            updateFirmwareVersion();
            return true;
        }
    }
    LOGF_ERROR("Ack failed: unexpected response '%s'", response);
    return false;
}

bool PegasusFlatMasterWiFi::EnableLightBox(bool enable)
{
    char cmd[16] = {0};
    char response[16] = {0};

    snprintf(cmd, 16, "FE:%d", enable ? 1 : 0);

    if (sendCommand(cmd, response) && strstr(response, "FE:") != nullptr)
        return true;

    LOGF_ERROR("Error on EnableLightBox. %s", response);
    return false;
}

bool PegasusFlatMasterWiFi::SetLightBoxBrightness(uint16_t value)
{
    char cmd[16] = {0};
    char response[16] = {0};

    snprintf(cmd, 16, "FL:%d", value);

    if (sendCommand(cmd, response) && strstr(response, "FL:") != nullptr)
        return true;

    LOGF_ERROR("Error on SetLightBoxBrightness. %s", response);
    return false;
}

bool PegasusFlatMasterWiFi::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (LI::processNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PegasusFlatMasterWiFi::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool PegasusFlatMasterWiFi::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processSwitch(dev, name, states, names, n))
            return true;

        if (RebootSP.isNameMatch(name))
        {
            sendCommand("FQ", nullptr);
            RebootSP.setState(IPS_OK);
            RebootSP[0].setState(ISS_OFF);
            RebootSP.apply();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PegasusFlatMasterWiFi::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool PegasusFlatMasterWiFi::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    LI::saveConfigItems(fp);
    return true;
}

bool PegasusFlatMasterWiFi::sendCommand(const char *command, char *res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    char cmd[16] = {0};
    snprintf(cmd, 16, "%s\n", command);

    LOGF_DEBUG("CMD <%s>", command);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return false;
    }

    // FQ (reboot) produces no response
    if (res == nullptr)
        return true;

    if ((rc = tty_read_section(PortFD, res, 0xA, 3, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("command: %s error: %s.", cmd, errstr);
        return false;
    }

    res[nbytes_read - 1] = 0;

    if (nbytes_read >= 2 && res[nbytes_read - 2] == '\r')
        res[nbytes_read - 2] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

void PegasusFlatMasterWiFi::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    getStatusData();

    SetTimer(getCurrentPollingPeriod());
}

bool PegasusFlatMasterWiFi::getStatusData()
{
    char response[WIFI_LEN] = {0};

    // FA response: "FA:brightness:powerStatus"
    if (!sendCommand("FA", response))
    {
        LOG_ERROR("Error querying device status.");
        return false;
    }

    std::regex re(":");
    std::vector<std::string> result(std::sregex_token_iterator(std::string(response).begin(),
                                                                std::string(response).end(), re, -1),
                                    std::sregex_token_iterator());

    if (result.size() < FA_N)
    {
        LOGF_WARN("Unexpected FA response format: %s", response);
        return false;
    }

    try
    {
        const double lightIntensity = std::stod(result[FA_LIGHT_INTENSITY]);
        const double lightActive    = std::stod(result[FA_LIGHT_ACTIVE]);

        const bool lightOn = (lightActive != 0);
        if (LightSP[FLAT_LIGHT_ON].getState() != (lightOn ? ISS_ON : ISS_OFF))
        {
            LightSP.reset();
            LightSP[lightOn ? FLAT_LIGHT_ON : FLAT_LIGHT_OFF].setState(ISS_ON);
            LightSP.setState(IPS_OK);
            LightSP.apply();
        }
        if (LightIntensityNP[0].getValue() != lightIntensity)
        {
            LightIntensityNP[0].setValue(lightIntensity);
            LightIntensityNP.setState(IPS_OK);
            LightIntensityNP.apply();
        }
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Error parsing FA data: %s", e.what());
        return false;
    }

    // FU response: "FU:seconds"
    char uptimeResponse[WIFI_LEN] = {0};
    if (sendCommand("FU", uptimeResponse) && strncmp(uptimeResponse, "FU:", 3) == 0)
    {
        UptimeNP[0].setValue(atof(uptimeResponse + 3));
        UptimeNP.setState(IPS_OK);
        UptimeNP.apply();
    }

    return true;
}
