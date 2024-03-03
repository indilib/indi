/*******************************************************************************
  Copyright(c) 2024 Rick Bassham. All rights reserved.

  Dark Dragons Astronomy DragonLAIR

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

#include "dragonlair.h"

#include <httplib.h>
#include <string.h>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

// We declare an auto pointer to DragonLAIR.
std::unique_ptr<DragonLAIR> dragonlair(new DragonLAIR());

DragonLAIR::DragonLAIR()
{
    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DragonLAIR::initProperties()
{
    setDomeConnection(INDI::Dome::CONNECTION_NONE);

    INDI::Dome::initProperties();

    FirmwareTP[0].fill("Version", "Version", nullptr);
    FirmwareTP[1].fill("Serial", "Serial", nullptr);
    FirmwareTP.fill(getDeviceName(), "FIRMWARE", "Firmware", INFO_TAB, IP_RO, 60, IPS_IDLE);

    IPAddressTP[0].fill("IP Address", "IP Address", nullptr);
    IPAddressTP.fill(getDeviceName(), "IP_ADDRESS", "IP Address", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    DiscoverSwitchSP[0].fill("DISCOVER", "Discover", ISS_OFF);
    DiscoverSwitchSP.fill(getDeviceName(), "DISCOVER", "Discover", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    SafetySensorLP[0].fill("SAFETY_SENSOR_1", "Safety Sensor 1", IPS_IDLE);
    SafetySensorLP[1].fill("SAFETY_SENSOR_2", "Safety Sensor 2", IPS_IDLE);
    SafetySensorLP[2].fill("SAFETY_SENSOR_3", "Safety Sensor 3", IPS_IDLE);
    SafetySensorLP[3].fill("SAFETY_SENSOR_4", "Safety Sensor 4", IPS_IDLE);
    SafetySensorLP.fill(getDeviceName(), "SAFETY_SENSOR", "Safety Sensor", MAIN_CONTROL_TAB, IPS_IDLE);

    LimitSwitchLP[0].fill("LIMIT_SWITCH_1", "Fully Open Switch", IPS_IDLE);
    LimitSwitchLP[1].fill("LIMIT_SWITCH_2", "Fully Closed Switch", IPS_IDLE);
    LimitSwitchLP.fill(getDeviceName(), "LIMIT_SWITCH", "Limit Switch", MAIN_CONTROL_TAB, IPS_IDLE);

    SetParkDataType(PARK_NONE);

    addAuxControls();

    return true;
}

void DragonLAIR::ISGetProperties(const char *dev)
{
    INDI::Dome::ISGetProperties(dev);

    defineProperty(IPAddressTP);
    defineProperty(DiscoverSwitchSP);

    loadConfig(IPAddressTP);
}

bool DragonLAIR::ISSnoopDevice(XMLEle *root)
{
    return INDI::Dome::ISSnoopDevice(root);
}

bool DragonLAIR::Connect()
{
    if (strlen(IPAddressTP[0].getText()) == 0)
    {
        LOG_ERROR("IP Address is not set.");
        return false;
    }

    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool DragonLAIR::Disconnect()
{
    return true;
}

const char *DragonLAIR::getDefaultName()
{
    return (const char *)"DragonLAIR Roll Off Roof";
}

bool DragonLAIR::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
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

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

bool DragonLAIR::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (isDeviceNameMatch(dev) && IPAddressTP.isNameMatch(name))
    {
        IPAddressTP.update(texts, names, n);
        IPAddressTP.setState(IPS_OK);
        IPAddressTP.apply();

        return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool DragonLAIR::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        defineProperty(FirmwareTP);
        defineProperty(SafetySensorLP);
        defineProperty(LimitSwitchLP);
    }
    else
    {
        deleteProperty(FirmwareTP);
        deleteProperty(SafetySensorLP);
        deleteProperty(LimitSwitchLP);
    }

    return true;
}

void DragonLAIR::TimerHit()
{
    if (!isConnected())
        return;

    updateStatus();

    SetTimer(getCurrentPollingPeriod());
}

bool DragonLAIR::saveConfigItems(FILE *fp)
{
    IPAddressTP.save(fp);

    return INDI::Dome::saveConfigItems(fp);
}

IPState DragonLAIR::Move(DomeDirection dir, DomeMotionCommand operation)
{
    updateStatus();

    if (operation == MOTION_START)
    {
        auto state = Dome::getDomeState();

        // DOME_CW --> OPEN. If can we are ask to "open" while we are fully opened as the limit switch indicates,
        // then we simply return false.
        if (dir == DOME_CW && state == DOME_UNPARKED)
        {
            LOG_WARN("Roof is already fully opened.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && state == DOME_PARKED)
        {
            LOG_WARN("Roof is already fully closed.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && INDI::Dome::isLocked())
        {
            LOG_INFO("Cannot close dome when mount is locking. See: Telescope parking policy, in options tab");
            return IPS_ALERT;
        }

        if (dir == DOME_CW)
        {
            LOG_INFO("Roll off is opening...");
            openRoof();
        }
        else
        {
            LOG_INFO("Roll off is closing...");
            closeRoof();
        }

        return IPS_BUSY;
    }

    return (Dome::Abort() ? IPS_OK : IPS_ALERT);
}

IPState DragonLAIR::Park()
{
    IPState rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        LOG_INFO("Roll off is parking...");
        return IPS_BUSY;
    }
    else
    {
        return IPS_ALERT;
    }
}

IPState DragonLAIR::UnPark()
{
    IPState rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        LOG_INFO("Roll off is unparking...");
        return IPS_BUSY;
    }
    else
    {
        return IPS_ALERT;
    }
}

bool DragonLAIR::Abort()
{
    stopRoof();
    updateStatus();

    return true;
}

void DragonLAIR::updateStatus()
{
    if (strlen(IPAddressTP[0].getText()) == 0)
    {
        LOG_ERROR("IP Address is not set.");
        return;
    }

    httplib::Client cli(IPAddressTP[0].getText(), 80);

    auto result = cli.Get("/indi/status");
    if (result.value().status == 200)
    {
        nlohmann::json j = nlohmann::json::parse(result.value().body, nullptr, false, true);

        if (j.is_discarded())
        {
            LOG_ERROR("Error parsing JSON.");
            return;
        }

        std::string version = j["version"];
        std::string serial = j["serialNumber"];

        FirmwareTP[0].setText(version.c_str());
        FirmwareTP[1].setText(serial.c_str());
        FirmwareTP.setState(IPS_OK);
        FirmwareTP.apply();

        std::string safetySensor1 = j["roof"]["safetySensor1"];
        std::string safetySensor2 = j["roof"]["safetySensor2"];
        std::string safetySensor3 = j["roof"]["safetySensor3"];
        std::string safetySensor4 = j["roof"]["safetySensor4"];

        SafetySensorLP[0].setState(safetySensor1 == "disabled" ? IPS_IDLE : safetySensor1 == "unsafe" ? IPS_ALERT : IPS_OK);
        SafetySensorLP[1].setState(safetySensor2 == "disabled" ? IPS_IDLE : safetySensor2 == "unsafe" ? IPS_ALERT : IPS_OK);
        SafetySensorLP[2].setState(safetySensor3 == "disabled" ? IPS_IDLE : safetySensor3 == "unsafe" ? IPS_ALERT : IPS_OK);
        SafetySensorLP[3].setState(safetySensor4 == "disabled" ? IPS_IDLE : safetySensor4 == "unsafe" ? IPS_ALERT : IPS_OK);
        SafetySensorLP.apply();

        bool isRoofFullyClosed = j["roof"]["isRoofFullyClosed"];
        bool isRoofFullyOpen = j["roof"]["isRoofFullyOpen"];

        LimitSwitchLP[0].setState(isRoofFullyOpen ? IPS_OK : IPS_BUSY);
        LimitSwitchLP[1].setState(isRoofFullyClosed ? IPS_OK : IPS_BUSY);
        LimitSwitchLP.apply();

        bool isRoofClosing = j["roof"]["isRoofClosing"];
        bool isRoofOpening = j["roof"]["isRoofOpening"];

        IDLog("Dome state: %d\n", Dome::getDomeState());

        if (isRoofFullyClosed)
        {
            if (Dome::getDomeState() != DOME_PARKED)
            {
                SetParked(true);
            }
        }
        else if (isRoofFullyOpen)
        {
            if (Dome::getDomeState() != DOME_UNPARKED)
            {
                SetParked(false);
            }
        }
        else if (isRoofClosing)
        {
            if (Dome::getDomeState() != DOME_PARKING)
            {
                setDomeState(DOME_PARKING);
            }
        }
        else if (isRoofOpening)
        {
            if (Dome::getDomeState() != DOME_UNPARKING)
            {
                setDomeState(DOME_UNPARKING);
            }
        }
        else if (Dome::getDomeState() != DOME_IDLE)
        {
            setDomeState(DOME_IDLE);
        }
    }
    else
    {
        LOG_ERROR("Error on updateStatus.");
    }
}

void DragonLAIR::openRoof()
{
    if (strlen(IPAddressTP[0].getText()) == 0)
    {
        LOG_ERROR("IP Address is not set.");
        return;
    }

    httplib::Client cli(IPAddressTP[0].getText(), 80);

    auto result = cli.Post("/indi/roof/open");
    if (result.value().status == 200)
    {
        LOG_INFO("Roof is opening...");
    }
    else
    {
        LOG_ERROR("Error on openRoof.");
    }
}

void DragonLAIR::closeRoof()
{
    if (strlen(IPAddressTP[0].getText()) == 0)
    {
        LOG_ERROR("IP Address is not set.");
        return;
    }

    httplib::Client cli(IPAddressTP[0].getText(), 80);

    auto result = cli.Post("/indi/roof/close");
    if (result.value().status == 200)
    {
        LOG_INFO("Roof is closing...");
    }
    else
    {
        LOG_ERROR("Error on closeRoof.");
    }
}

void DragonLAIR::stopRoof()
{
    if (strlen(IPAddressTP[0].getText()) == 0)
    {
        LOG_ERROR("IP Address is not set.");
        return;
    }

    httplib::Client cli(IPAddressTP[0].getText(), 80);

    auto result = cli.Post("/indi/roof/abort");
    if (result.value().status == 200)
    {
        LOG_INFO("Roof is stopping...");
    }
    else
    {
        LOG_ERROR("Error on stopRoof.");
    }
}

#define DDA_DISCOVERY_PORT 0xdda
#define DDA_DISCOVERY_TIMEOUT 2 // Seconds
#define DDA_DISCOVERY_RECEIVE_BUFFER_SIZE 256

void DragonLAIR::discoverDevices()
{
    IDLog("Sending discovery packet\n");

    int s;

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == -1)
    {
        IDLog("Error creating socket\n");
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

        IDLog("Received: %s\n", recvbuff);

        nlohmann::json doc = nlohmann::json::parse(recvbuff, nullptr, false, true);

        if (!doc.is_discarded() && doc.contains("deviceType"))
        {
            char deviceIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(Recv_addr.sin_addr), deviceIP, INET_ADDRSTRLEN);

            std::string deviceType = doc["deviceType"];
            std::string serialNumber = doc["serialNumber"];

            LOGF_INFO("Found %s %s at %s\n", deviceType.c_str(), serialNumber.c_str(), deviceIP);
        }
    }

    IDLog("discovery complete\n");

    close(s);

    DiscoverSwitchSP.reset();
    DiscoverSwitchSP[0].setState(ISS_OFF);
    DiscoverSwitchSP.setState(IPS_OK);
    DiscoverSwitchSP.apply();
}