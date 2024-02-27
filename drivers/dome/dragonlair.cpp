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

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <ctime>
#include <memory>

#if defined(_WIN32) || defined(__USE_W32_SOCKETS)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif
#else
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

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

    IPAddressTP[0].fill("IP Address", "IP Address", nullptr);
    IPAddressTP.fill(getDeviceName(), "IP_ADDRESS", "IP Address", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    DiscoverSwitchSP[0].fill("DISCOVER", "Discover", ISS_OFF);
    DiscoverSwitchSP.fill(getDeviceName(), "DISCOVER", "Discover", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

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
        // TODO: Add any custom properties that need to be updated here.
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
    // if we are not moving, and neither limit switch is tripped, we are
    //      setDomeState(DOME_IDLE);
    //
    // if we are fully opened we are
    //      SetParked(false);
    //
    // if we are fully closed we are
    //      SetParked(true);
}

void DragonLAIR::openRoof()
{

}

void DragonLAIR::closeRoof()
{

}

void DragonLAIR::stopRoof()
{

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

        nlohmann::json doc = nlohmann::json::parse(recvbuff);

        if (doc.contains("deviceType"))
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