/*******************************************************************************
  Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

  Driver for Wake-on-LAN power control

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

#include "wake_on_lan.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// We declare an auto pointer to WakeOnLAN.
std::unique_ptr<WakeOnLAN> wakeonlan(new WakeOnLAN());

WakeOnLAN::WakeOnLAN()
{
    setVersion(1, 0);
    // WoL has no physical connection — always treat as connected
    setConnected(true, IPS_OK);
}

const char *WakeOnLAN::getDefaultName()
{
    return "Wake On Lan";
}

bool WakeOnLAN::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Device 1
    Device1TP[0].fill("MAC", "MAC Address", "");
    Device1TP[1].fill("ALIAS", "Alias", "");
    Device1TP[2].fill("IP", "IP Address", "");
    Device1TP.fill(getDeviceName(), "DEVICE_1", "Device 1", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    Device1TP.load();

    // Device 2
    Device2TP[0].fill("MAC", "MAC Address", "");
    Device2TP[1].fill("ALIAS", "Alias", "");
    Device2TP[2].fill("IP", "IP Address", "");
    Device2TP.fill(getDeviceName(), "DEVICE_2", "Device 2", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    Device2TP.load();

    // Device 3
    Device3TP[0].fill("MAC", "MAC Address", "");
    Device3TP[1].fill("ALIAS", "Alias", "");
    Device3TP[2].fill("IP", "IP Address", "");
    Device3TP.fill(getDeviceName(), "DEVICE_3", "Device 3", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    Device3TP.load();

    // Device 4
    Device4TP[0].fill("MAC", "MAC Address", "");
    Device4TP[1].fill("ALIAS", "Alias", "");
    Device4TP[2].fill("IP", "IP Address", "");
    Device4TP.fill(getDeviceName(), "DEVICE_4", "Device 4", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    Device4TP.load();

    // Send buttons - labels will be updated from aliases
    Send1SP[0].fill("SEND", "Send", ISS_OFF);
    Send1SP.fill(getDeviceName(), "Device 1", "Device 1", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    Send2SP[0].fill("SEND", "Send", ISS_OFF);
    Send2SP.fill(getDeviceName(), "Device 2", "Device 2", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    Send3SP[0].fill("SEND", "Send", ISS_OFF);
    Send3SP.fill(getDeviceName(), "Device 3", "Device 3", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    Send4SP[0].fill("SEND", "Send", ISS_OFF);
    Send4SP.fill(getDeviceName(), "Device 4", "Device 4", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    addAuxControls();

    return true;
}

void WakeOnLAN::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Define and load device properties first
    defineProperty(Device1TP);
    defineProperty(Device2TP);
    defineProperty(Device3TP);
    defineProperty(Device4TP);

    // Update Send button labels from saved aliases before defining them
    if (Device1TP[1].getText() && strlen(Device1TP[1].getText()) > 0)
        Send1SP.setLabel(Device1TP[1].getText());

    if (Device2TP[1].getText() && strlen(Device2TP[1].getText()) > 0)
        Send2SP.setLabel(Device2TP[1].getText());

    if (Device3TP[1].getText() && strlen(Device3TP[1].getText()) > 0)
        Send3SP.setLabel(Device3TP[1].getText());

    if (Device4TP[1].getText() && strlen(Device4TP[1].getText()) > 0)
        Send4SP.setLabel(Device4TP[1].getText());

    // Now define Send buttons with the correct labels already set
    defineProperty(Send1SP);
    defineProperty(Send2SP);
    defineProperty(Send3SP);
    defineProperty(Send4SP);

    // Set initial button state: BUSY if an IP is configured (TimerHit will resolve it)
    if (Device1TP[2].getText() && strlen(Device1TP[2].getText()) > 0) Send1SP.setState(IPS_BUSY);
    if (Device2TP[2].getText() && strlen(Device2TP[2].getText()) > 0) Send2SP.setState(IPS_BUSY);
    if (Device3TP[2].getText() && strlen(Device3TP[2].getText()) > 0) Send3SP.setState(IPS_BUSY);
    if (Device4TP[2].getText() && strlen(Device4TP[2].getText()) > 0) Send4SP.setState(IPS_BUSY);
}

bool WakeOnLAN::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(Device1TP);
        defineProperty(Device2TP);
        defineProperty(Device3TP);
        defineProperty(Device4TP);

        defineProperty(Send1SP);
        defineProperty(Send2SP);
        defineProperty(Send3SP);
        defineProperty(Send4SP);
    }
    else
    {
        deleteProperty(Device1TP);
        deleteProperty(Device2TP);
        deleteProperty(Device3TP);
        deleteProperty(Device4TP);

        deleteProperty(Send1SP);
        deleteProperty(Send2SP);
        deleteProperty(Send3SP);
        deleteProperty(Send4SP);
    }

    return true;
}

bool WakeOnLAN::Connect()
{
    LOG_INFO("Wake On Lan driver connected.");
    m_stopPing = false;
    SetTimer(100);  // fire first ping immediately
    return true;
}

bool WakeOnLAN::Disconnect()
{
    m_stopPing = true;
    // Reset all button states to idle so the UI is clean
    Send1SP.setState(IPS_IDLE); Send1SP.apply();
    Send2SP.setState(IPS_IDLE); Send2SP.apply();
    Send3SP.setState(IPS_IDLE); Send3SP.apply();
    Send4SP.setState(IPS_IDLE); Send4SP.apply();
    LOG_INFO("Wake On Lan driver disconnected. Ping monitoring stopped.");
    return true;
}


void WakeOnLAN::TimerHit()
{
    if (m_stopPing)
        return;

    checkPingStatus(0, Device1TP, Send1SP);
    checkPingStatus(1, Device2TP, Send2SP);
    checkPingStatus(2, Device3TP, Send3SP);
    checkPingStatus(3, Device4TP, Send4SP);

    SetTimer(5000);  
}

bool WakeOnLAN::pingDevice(const char *ip)
{
    std::string cmd = "ping -c1 -W1 " + std::string(ip) + " > /dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

void WakeOnLAN::checkPingStatus(int idx, INDI::PropertyText &devTP, INDI::PropertySwitch &sendSP)
{
    INDI_UNUSED(idx);

    const char *ip = devTP[2].getText();
    if (!ip || strlen(ip) == 0)
        return;

    const char *alias = devTP[1].getText();
    const char *label = (alias && strlen(alias) > 0) ? alias : devTP.getLabel();

    if (pingDevice(ip))
    {
        LOGF_INFO("%s (%s) is online.", label, ip);
        sendSP.setState(IPS_OK);
    }
    else
    {
        LOGF_INFO("%s (%s) is not reachable.", label, ip);
        sendSP.setState(IPS_ALERT);
    }
    sendSP.apply();
}

bool WakeOnLAN::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (Device1TP.isNameMatch(name))
        {
            Device1TP.update(texts, names, n);
            Device1TP.setState(IPS_OK);
            Device1TP.apply();
            saveConfig(true, "DEVICE_1");
            if (Device1TP[1].getText())
                Send1SP.setLabel(Device1TP[1].getText());
            const char *ip1 = Device1TP[2].getText();
            if (ip1 && strlen(ip1) > 0)
            {
                Send1SP.setState(IPS_BUSY);
                Send1SP.apply();
            }
            return true;
        }

        if (Device2TP.isNameMatch(name))
        {
            Device2TP.update(texts, names, n);
            Device2TP.setState(IPS_OK);
            Device2TP.apply();
            saveConfig(true, "DEVICE_2");
            if (Device2TP[1].getText())
                Send2SP.setLabel(Device2TP[1].getText());
            const char *ip2 = Device2TP[2].getText();
            if (ip2 && strlen(ip2) > 0)
            {
                Send2SP.setState(IPS_BUSY);
                Send2SP.apply();
            }
            return true;
        }

        if (Device3TP.isNameMatch(name))
        {
            Device3TP.update(texts, names, n);
            Device3TP.setState(IPS_OK);
            Device3TP.apply();
            saveConfig(true, "DEVICE_3");
            if (Device3TP[1].getText())
                Send3SP.setLabel(Device3TP[1].getText());
            const char *ip3 = Device3TP[2].getText();
            if (ip3 && strlen(ip3) > 0)
            {
                Send3SP.setState(IPS_BUSY);
                Send3SP.apply();
            }
            return true;
        }

        if (Device4TP.isNameMatch(name))
        {
            Device4TP.update(texts, names, n);
            Device4TP.setState(IPS_OK);
            Device4TP.apply();
            saveConfig(true, "DEVICE_4");
            if (Device4TP[1].getText())
                Send4SP.setLabel(Device4TP[1].getText());
            const char *ip4 = Device4TP[2].getText();
            if (ip4 && strlen(ip4) > 0)
            {
                Send4SP.setState(IPS_BUSY);
                Send4SP.apply();
            }
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool WakeOnLAN::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (Send1SP.isNameMatch(name))
        {
            if (Device1TP[0].getText() == nullptr || strlen(Device1TP[0].getText()) == 0)
            {
                LOG_ERROR("MAC address 1 not configured.");
                Send1SP.setState(IPS_ALERT);
                Send1SP.apply();
                return true;
            }

            if (sendWakeOnLan(Device1TP[0].getText()))
            {
                LOGF_INFO("Wake On Lan packet sent to %s (%s)", Device1TP[1].getText(), Device1TP[0].getText());
                // Start pinging if an IP is configured; button turns green when device responds
                const char *ip1 = Device1TP[2].getText();
                if (ip1 && strlen(ip1) > 0)
                {
                    Send1SP.setState(IPS_BUSY);
                    Send1SP.apply();
                }
                else
                {
                    Send1SP.setState(IPS_OK);
                    Send1SP.apply();
                }
            }
            else
            {
                LOGF_ERROR("Failed to send Wake On Lan packet for %s (%s)", Device1TP[1].getText(), Device1TP[0].getText());
                Send1SP.setState(IPS_ALERT);
                Send1SP.apply();
            }
            Send1SP.reset();
            return true;
        }

        if (Send2SP.isNameMatch(name))
        {
            if (Device2TP[0].getText() == nullptr || strlen(Device2TP[0].getText()) == 0)
            {
                LOG_ERROR("MAC address 2 not configured.");
                Send2SP.setState(IPS_ALERT);
                Send2SP.apply();
                return true;
            }

            if (sendWakeOnLan(Device2TP[0].getText()))
            {
                LOGF_INFO("Wake On Lan packet sent to %s (%s)", Device2TP[1].getText(), Device2TP[0].getText());
                const char *ip2 = Device2TP[2].getText();
                if (ip2 && strlen(ip2) > 0)
                {
                    Send2SP.setState(IPS_BUSY);
                    Send2SP.apply();
                }
                else
                {
                    Send2SP.setState(IPS_OK);
                    Send2SP.apply();
                }
            }
            else
            {
                LOGF_ERROR("Failed to send Wake On Lan packet for %s (%s)", Device2TP[1].getText(), Device2TP[0].getText());
                Send2SP.setState(IPS_ALERT);
                Send2SP.apply();
            }
            Send2SP.reset();
            return true;
        }

        if (Send3SP.isNameMatch(name))
        {
            if (Device3TP[0].getText() == nullptr || strlen(Device3TP[0].getText()) == 0)
            {
                LOG_ERROR("MAC address 3 not configured.");
                Send3SP.setState(IPS_ALERT);
                Send3SP.apply();
                return true;
            }

            if (sendWakeOnLan(Device3TP[0].getText()))
            {
                LOGF_INFO("Wake On Lan packet sent to %s (%s)", Device3TP[1].getText(), Device3TP[0].getText());
                const char *ip3 = Device3TP[2].getText();
                if (ip3 && strlen(ip3) > 0)
                {
                    Send3SP.setState(IPS_BUSY);
                    Send3SP.apply();
                }
                else
                {
                    Send3SP.setState(IPS_OK);
                    Send3SP.apply();
                }
            }
            else
            {
                LOGF_ERROR("Failed to send Wake On Lan packet for %s (%s)", Device3TP[1].getText(), Device3TP[0].getText());
                Send3SP.setState(IPS_ALERT);
                Send3SP.apply();
            }
            Send3SP.reset();
            return true;
        }

        if (Send4SP.isNameMatch(name))
        {
            if (Device4TP[0].getText() == nullptr || strlen(Device4TP[0].getText()) == 0)
            {
                LOG_ERROR("MAC address 4 not configured.");
                Send4SP.setState(IPS_ALERT);
                Send4SP.apply();
                return true;
            }

            if (sendWakeOnLan(Device4TP[0].getText()))
            {
                LOGF_INFO("Wake On Lan packet sent to %s (%s)", Device4TP[1].getText(), Device4TP[0].getText());
                const char *ip4 = Device4TP[2].getText();
                if (ip4 && strlen(ip4) > 0)
                {
                    Send4SP.setState(IPS_BUSY);
                    Send4SP.apply();
                }
                else
                {
                    Send4SP.setState(IPS_OK);
                    Send4SP.apply();
                }
            }
            else
            {
                LOGF_ERROR("Failed to send Wake On Lan packet for %s (%s)", Device4TP[1].getText(), Device4TP[0].getText());
                Send4SP.setState(IPS_ALERT);
                Send4SP.apply();
            }
            Send4SP.reset();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}


bool WakeOnLAN::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    Device1TP.save(fp);
    Device2TP.save(fp);
    Device3TP.save(fp);
    Device4TP.save(fp);

    return true;
}

bool WakeOnLAN::sendWakeOnLan(const char *macAddress)
{
    if (!macAddress || strlen(macAddress) == 0)
    {
        LOG_ERROR("MAC address not configured");
        return false;
    }

    // Parse MAC address: "AA:BB:CC:DD:EE:FF" or "AABBCCDDEEFF"
    unsigned char mac[6];
    int macElements = sscanf(macAddress, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

    // Try without colons if first attempt failed
    if (macElements != 6)
    {
        macElements = sscanf(macAddress, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
                             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    }

    if (macElements != 6)
    {
        LOGF_ERROR("Invalid MAC address format: %s (use AA:BB:CC:DD:EE:FF or AABBCCDDEEFF)", macAddress);
        return false;
    }

    // Create magic packet: 6 bytes of 0xFF + 16 repetitions of the MAC address
    unsigned char packet[102];

    // Fill with 0xFF
    for (int i = 0; i < 6; i++)
        packet[i] = 0xFF;

    // Fill with MAC address repeated 16 times
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 6; j++)
            packet[6 + i * 6 + j] = mac[j];

    // Create UDP socket for broadcast
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        LOGF_ERROR("Failed to create socket: %s", strerror(errno));
        return false;
    }

    // Enable broadcast option
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
    {
        LOGF_ERROR("Failed to set broadcast option: %s", strerror(errno));
        close(sock);
        return false;
    }

    // Set destination address to broadcast
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    addr.sin_port = htons(9);  // WoL standard port

    // Send magic packet
    ssize_t sent = sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&addr, sizeof(addr));
    close(sock);

    if (sent != sizeof(packet))
    {
        LOGF_ERROR("Failed to send WoL packet: %s", strerror(errno));
        return false;
    }

    LOGF_INFO("Wake on LAN packet sent to %s", macAddress);
    return true;
}
