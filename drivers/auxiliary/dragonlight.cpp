/*******************************************************************************
  Copyright(c) 2024 Rick Bassham. All rights reserved.

  Dark Dragons Astronomy DragonLIGHT

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

#include "dragonlight.h"

#include <httplib.h>
#include <string.h>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

static std::unique_ptr<DragonLIGHT> dragonlight(new DragonLIGHT());

DragonLIGHT::DragonLIGHT() : LightBoxInterface(this)
{
    setVersion(1, 0);
}

bool DragonLIGHT::initProperties()
{
    INDI::DefaultDevice::initProperties();

    FirmwareTP[0].fill("Version", "Version", nullptr);
    FirmwareTP[1].fill("Serial", "Serial", nullptr);
    FirmwareTP.fill(getDeviceName(), "FIRMWARE", "Firmware", INFO_TAB, IP_RO, 60, IPS_IDLE);

    IPAddressTP[0].fill("IP Address", "IP Address", nullptr);
    IPAddressTP.fill(getDeviceName(), "IP_ADDRESS", "IP Address", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    DiscoverSwitchSP[0].fill("DISCOVER", "Discover", ISS_OFF);
    DiscoverSwitchSP.fill(getDeviceName(), "DISCOVER", "Discover", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    LI::initProperties(MAIN_CONTROL_TAB, CAN_DIM);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE);

    LightIntensityNP[0].setMin(0);
    LightIntensityNP[0].setMax(255);
    LightIntensityNP[0].setMax(1);

    addAuxControls();

    return true;
}

void DragonLIGHT::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(IPAddressTP);
    defineProperty(DiscoverSwitchSP);

    // Get Light box properties
    LI::ISGetProperties(dev);

    loadConfig(IPAddressTP);
}

bool DragonLIGHT::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    LI::updateProperties();

    if (isConnected())
    {
        defineProperty(FirmwareTP);
    }
    else
    {
        deleteProperty(FirmwareTP);
    }

    return true;
}

const char *DragonLIGHT::getDefaultName()
{
    return "DragonLIGHT";
}

bool DragonLIGHT::EnableLightBox(bool enable)
{
    LOGF_DEBUG("EnableLightBox: %d", enable);

    httplib::Client cli(IPAddressTP[0].getText(), 80);

    std::string endpoint = enable ? "/indi/turnon" : "/indi/turnoff";

    auto result = cli.Post(endpoint);
    if (!result)
    {
        LOG_ERROR("Unable to connect.");
        return false;
    }

    return (result.value().status == 200);
}

bool DragonLIGHT::SetLightBoxBrightness(uint16_t value)
{
    if (LightSP[FLAT_LIGHT_ON].getState() != ISS_ON)
    {
        LOG_ERROR("You must set On the Flat Light first.");
        return false;
    }

    LightIntensityNP[0].setValue(value);
    LightIntensityNP.apply();

    httplib::Client cli(IPAddressTP[0].getText(), 80);

    std::string endpoint = "/indi/brightness";

    nlohmann::json j;
    j["brightness"] = value;

    auto result = cli.Post(endpoint, j.dump(), "application/json");
    if (!result)
    {
        LOG_ERROR("Unable to connect.");
        return false;
    }

    if (result.value().status == 200)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool DragonLIGHT::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (LI::processNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool DragonLIGHT::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (isDeviceNameMatch(dev) && IPAddressTP.isNameMatch(name))
    {
        IPAddressTP.update(texts, names, n);
        IPAddressTP.setState(IPS_OK);
        IPAddressTP.apply();

        return true;
    }

    if (LI::processText(dev, name, texts, names, n))
        return true;

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool DragonLIGHT::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (isDeviceNameMatch(dev) && DiscoverSwitchSP.isNameMatch(name))
    {
        DiscoverSwitchSP.update(states, names, n);
        auto isToggled = DiscoverSwitchSP[0].getState() == ISS_ON;
        DiscoverSwitchSP.setState(isToggled ? IPS_BUSY : IPS_IDLE);

        if (isToggled)
        {
            discoverDevices();
        }

        DiscoverSwitchSP.apply();
        return true;
    }

    if (LI::processSwitch(dev, name, states, names, n))
        return true;

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool DragonLIGHT::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool DragonLIGHT::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    IPAddressTP.save(fp);

    return LI::saveConfigItems(fp);
}

bool DragonLIGHT::Connect()
{
    IPAddressTP.getText();

    if (strlen(IPAddressTP[0].getText()) == 0)
    {
        LOG_ERROR("IP Address is not set.");
        return false;
    }

    updateStatus();

    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool DragonLIGHT::Disconnect()
{
    return true;
}

void DragonLIGHT::TimerHit()
{
    if (!isConnected())
        return;

    INDI::DefaultDevice::TimerHit();

    updateStatus();

    SetTimer(getCurrentPollingPeriod());
}

void DragonLIGHT::updateStatus()
{
    httplib::Client cli(IPAddressTP[0].getText(), 80);

    auto result = cli.Get("/indi/status");
    if (!result)
    {
        LOG_ERROR("Unable to connect.");
        return;
    }

    if (result.value().status == 200)
    {
        nlohmann::json j = nlohmann::json::parse(result.value().body);

        std::string version = j["version"];
        std::string serial = j["serialNumber"];

        FirmwareTP[0].setText(version.c_str());
        FirmwareTP[1].setText(serial.c_str());
        FirmwareTP.setState(IPS_OK);
        FirmwareTP.apply();

        uint32_t brightness = j["brightness"];

        LightIntensityNP[0].setValue(brightness);
        LightIntensityNP.setState(IPS_OK);
        LightIntensityNP.apply();

        bool isOn = j["isOn"];

        LightSP[FLAT_LIGHT_ON].setState(isOn ?  ISS_ON : ISS_OFF);
        LightSP[FLAT_LIGHT_OFF].setState(isOn ?  ISS_OFF : ISS_ON);
        LightSP.setState(IPS_OK);
        LightSP.apply();
    }
    else
    {
        LOG_ERROR("Error on updateStatus.");
    }
}

#define DDA_DISCOVERY_PORT 0xdda
#define DDA_DISCOVERY_TIMEOUT 2 // Seconds
#define DDA_DISCOVERY_RECEIVE_BUFFER_SIZE 256

void DragonLIGHT::discoverDevices()
{
    int s;

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == -1)
    {
        LOG_ERROR("Error creating socket for discovery.");
        return;
    }

    int broadcast = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct timeval tv;
    tv.tv_sec = DDA_DISCOVERY_TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in Recv_addr;

    socklen_t len = sizeof(struct sockaddr_in);

    Recv_addr.sin_family       = AF_INET;
    Recv_addr.sin_port         = htons(DDA_DISCOVERY_PORT);
    Recv_addr.sin_addr.s_addr  = INADDR_BROADCAST;

    const char* sendMSG = "darkdragons";

    sendto(s, sendMSG, strlen(sendMSG), 0, (sockaddr *)&Recv_addr, sizeof(Recv_addr));

    char recvbuff[DDA_DISCOVERY_RECEIVE_BUFFER_SIZE];
    while (true)
    {
        memset(recvbuff, 0, DDA_DISCOVERY_RECEIVE_BUFFER_SIZE);

        int n = recvfrom(s, recvbuff, DDA_DISCOVERY_RECEIVE_BUFFER_SIZE, 0, (sockaddr *)&Recv_addr, &len);
        if (n < 0)
            break;

        nlohmann::json doc = nlohmann::json::parse(recvbuff, nullptr, false, true);

        if (!doc.is_discarded() && doc.contains("deviceType"))
        {
            char deviceIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(Recv_addr.sin_addr), deviceIP, INET_ADDRSTRLEN);

            std::string deviceType = doc["deviceType"];
            std::string serialNumber = doc["SerialNumber"].is_string() ? doc["SerialNumber"].get<std::string>() : "";

            LOGF_INFO("Found %s %s at %s", deviceType.c_str(), serialNumber.c_str(), deviceIP);
        }
    }

    close(s);

    DiscoverSwitchSP.reset();
    DiscoverSwitchSP[0].setState(ISS_OFF);
    DiscoverSwitchSP.setState(IPS_OK);
    DiscoverSwitchSP.apply();
}