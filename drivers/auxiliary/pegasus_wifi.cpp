/*******************************************************************************
  Pegasus Astro WiFi settings

  Copyright (C) 2026 Dominik Merkel

  Shared by the drivers of Pegasus Astro devices with the common WiFi command set.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*******************************************************************************/

#include "pegasus_wifi.h"

#include "defaultdevice.h"
#include "eventloop.h"
#include "indicom.h"
#include "indilogger.h"

#include <algorithm>
#include <cstring>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace PegasusAstro
{

#define WIFI_LOG(level, ...) DEBUGFDEVICE(m_Device->getDeviceName(), level, __VA_ARGS__)

WiFi::WiFi(INDI::DefaultDevice *device) : m_Device(device)
{
}

void WiFi::initProperties(const char *tab, LoadConfig loadConfig, SaveConfig saveConfig)
{
    m_LoadConfig = std::move(loadConfig);
    m_SaveConfig = std::move(saveConfig);
    const char *dev = m_Device->getDeviceName();

    // Hotspot on/off (A? / AE:x)
    HotspotSP[0].fill("HOTSPOT_ENABLE", "Enable", ISS_OFF);
    HotspotSP[1].fill("HOTSPOT_DISABLE", "Disable", ISS_ON);
    HotspotSP.fill(dev, "HOTSPOT_STATUS", "Hotspot", tab, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Hotspot name and password (AL / AN: / AP:). The password is also the TCP and web login.
    HotspotTP[0].fill("HOTSPOT_SSID", "SSID", "");
    HotspotTP[1].fill("HOTSPOT_PASSWORD", "Password", "");
    HotspotTP.fill(dev, "HOTSPOT_CREDENTIALS", "Hotspot Credentials", tab, IP_RW, 60, IPS_IDLE);

    // Hotspot channel (AC / AC:n)
    ChannelNP[0].fill("WIFI_CHANNEL_VALUE", "Channel (1-11)", "%.0f", 1, 11, 1, 11);
    ChannelNP.fill(dev, "WIFI_CHANNEL", "WiFi Channel", tab, IP_RW, 60, IPS_IDLE);

    // Network the device is connected to as a client (WA)
    ConnectionTP[0].fill("CURRENT_WIFI", "Connection", "");
    ConnectionTP.fill(dev, "CURRENT_WIFI", "Current WiFi", tab, IP_RO, 60, IPS_IDLE);

    // Network scan (WS)
    ScanSP[0].fill("WIFI_SCAN", "Scan", ISS_OFF);
    ScanSP.fill(dev, "WIFI_SCAN", "Scan Networks", tab, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    ScanTP[0].fill("WIFI_SCAN_RESULTS", "Networks", "");
    ScanTP.fill(dev, "WIFI_SCAN_RESULTS", "Scan Results", tab, IP_RO, 60, IPS_IDLE);

    // WiFi client (WN: / WP:)
    NetworkTP[0].fill("WIFI_SSID", "SSID", "");
    NetworkTP[1].fill("WIFI_PASSWORD", "Password", "");
    NetworkTP.fill(dev, "WIFI_CONNECT_CREDS", "WiFi Credentials", tab, IP_RW, 60, IPS_IDLE);
    ConnectSP[0].fill("WIFI_CONNECT", "Connect", ISS_OFF);
    ConnectSP.fill(dev, "WIFI_CONNECT", "Connect to WiFi", tab, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Login on the TCP command socket: the hotspot password
    PasswordTP[0].fill("TCP_PASSWORD_VALUE", "Password", DEFAULT_PASSWORD);
    PasswordTP.fill(dev, "TCP_PASSWORD", "TCP Password", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);
}

void WiFi::ISGetProperties()
{
    m_Device->defineProperty(PasswordTP);
    if (!m_PasswordLoaded)
    {
        m_PasswordLoaded = true;
        m_LoadConfig(PasswordTP.getName());
    }
}

void WiFi::updateProperties(bool connected, int portFD)
{
    if (connected)
    {
        m_PortFD = portFD;
        m_Device->defineProperty(HotspotSP);
        m_Device->defineProperty(HotspotTP);
        m_Device->defineProperty(ChannelNP);
        m_Device->defineProperty(ConnectionTP);
        m_Device->defineProperty(ScanSP);
        m_Device->defineProperty(ScanTP);
        m_Device->defineProperty(NetworkTP);
        m_Device->defineProperty(ConnectSP);

        readHotspotStatus();
        readHotspotCredentials();
        readChannel();
        readConnection();
    }
    else
    {
        m_Device->deleteProperty(HotspotSP);
        m_Device->deleteProperty(HotspotTP);
        m_Device->deleteProperty(ChannelNP);
        m_Device->deleteProperty(ConnectionTP);
        m_Device->deleteProperty(ScanSP);
        m_Device->deleteProperty(ScanTP);
        m_Device->deleteProperty(NetworkTP);
        m_Device->deleteProperty(ConnectSP);
        m_PortFD = -1;
    }
}

bool WiFi::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev == nullptr || strcmp(dev, m_Device->getDeviceName()) != 0)
        return false;

    if (HotspotSP.isNameMatch(name))
    {
        const char *onSwitch = IUFindOnSwitchName(states, names, n);
        const bool enable = onSwitch != nullptr && HotspotSP[0].isNameMatch(onSwitch);
        char cmd[LEN] = {0}, res[LEN] = {0};
        snprintf(cmd, LEN, "AE:%d", enable ? 1 : 0);
        // The FocusCube 3 and Falcon V2 answer 1; the FlatMaster Neo answer is not documented. Only 0 is
        // a failure, and the state is read back below either way.
        if (sendCommand(cmd, res, LEN) && strcmp(res, "0") != 0)
        {
            readHotspotStatus();
            if (enable && strcmp(HotspotTP[1].getText(), DEFAULT_PASSWORD) == 0)
                WIFI_LOG(INDI::Logger::DBG_WARNING, "%s",
                         "The hotspot uses the factory password. Change it, since anyone in range can control the device.");
        }
        else
        {
            WIFI_LOG(INDI::Logger::DBG_ERROR, "Switching the hotspot failed: %s", res);
            HotspotSP.setState(IPS_ALERT);
            HotspotSP.apply();
        }
        return true;
    }

    if (ScanSP.isNameMatch(name))
    {
        ScanSP.reset();
        ScanSP.setState(scan() ? IPS_OK : IPS_ALERT);
        ScanSP.apply();
        return true;
    }

    if (ConnectSP.isNameMatch(name))
    {
        ConnectSP.reset();
        ConnectSP.setState(connectToNetwork(NetworkTP[0].getText(), NetworkTP[1].getText()) ? IPS_OK : IPS_ALERT);
        ConnectSP.apply();
        return true;
    }

    return false;
}

bool WiFi::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev == nullptr || strcmp(dev, m_Device->getDeviceName()) != 0)
        return false;

    if (PasswordTP.isNameMatch(name))
    {
        PasswordTP.update(texts, names, n);
        PasswordTP.setState(IPS_OK);
        PasswordTP.apply();
        m_SaveConfig(PasswordTP);
        return true;
    }

    if (HotspotTP.isNameMatch(name))
    {
        HotspotTP.update(texts, names, n);
        if (setHotspotCredentials(HotspotTP[0].getText(), HotspotTP[1].getText()))
        {
            // The hotspot password is also the login of the TCP socket and the web dashboard.
            PasswordTP[0].setText(HotspotTP[1].getText());
            PasswordTP.apply();
            m_SaveConfig(PasswordTP);
            WIFI_LOG(INDI::Logger::DBG_SESSION, "%s",
                     "Hotspot credentials set. The password is also the login for TCP and the web dashboard.");
        }
        readHotspotCredentials();
        return true;
    }

    if (NetworkTP.isNameMatch(name))
    {
        // The credentials are sent with "Connect to WiFi".
        NetworkTP.update(texts, names, n);
        NetworkTP.setState(IPS_OK);
        NetworkTP.apply();
        return true;
    }

    return false;
}

bool WiFi::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev == nullptr || strcmp(dev, m_Device->getDeviceName()) != 0)
        return false;

    if (ChannelNP.isNameMatch(name))
    {
        if (!ChannelNP.update(values, names, n))
        {
            ChannelNP.setState(IPS_ALERT);
            ChannelNP.apply();
            return true;
        }

        const int channel = static_cast<int>(ChannelNP[0].getValue());
        char cmd[LEN] = {0}, res[LEN] = {0};
        snprintf(cmd, LEN, "AC:%d", channel);
        // The FocusCube 3 and Falcon V2 answer with the channel, the FlatMaster Neo with 1; 0 is a failure.
        if (!sendCommand(cmd, res, LEN) || strcmp(res, "0") == 0)
            WIFI_LOG(INDI::Logger::DBG_ERROR, "Setting the WiFi channel failed: %s", res);
        readChannel();
        return true;
    }

    return false;
}

void WiFi::saveConfigItems(FILE *fp)
{
    PasswordTP.save(fp);
}

bool WiFi::login(int portFD)
{
    // Send the hotspot password first. The Falcon V2 replies AUTHOK, or closes the connection on a
    // wrong password; a closed connection is the expected answer then, so no read error is logged.
    // The FocusCube 3 (firmware 2.0) asks for no password and answers the first line with ">>".
    m_PortFD = portFD;
    WIFI_LOG(INDI::Logger::DBG_DEBUG, "%s", "CMD <password>");
    char cmd[LEN] = {0}, res[LEN] = {0};
    snprintf(cmd, LEN, "%s\r\n", PasswordTP[0].getText());
    int nbytes = 0;
    if (tty_write_string(portFD, cmd, &nbytes) == TTY_OK &&
            tty_nread_section(portFD, res, LEN - 1, '\n', 3, &nbytes) == TTY_OK)
    {
        if (strncmp(res, "AUTHOK", 6) == 0)
        {
            WIFI_LOG(INDI::Logger::DBG_DEBUG, "%s", "Logged in on the TCP command socket.");
            return true;
        }
        if (strncmp(res, ">>", 2) == 0)
        {
            WIFI_LOG(INDI::Logger::DBG_WARNING, "%s",
                     "The device accepts TCP commands without a password, so anyone on the network can control it.");
            return true;
        }
    }

    WIFI_LOG(INDI::Logger::DBG_ERROR, "%s",
             "TCP login failed. Check the TCP Password on the Connection tab; it is the hotspot password.");
    return false;
}

bool WiFi::sendCommand(const char *cmd, char *res, int len, int timeout, bool secret)
{
    // Commands end with CR LF (the FocusCube 3 needs the CR over TCP); replies are one line.
    WIFI_LOG(INDI::Logger::DBG_DEBUG, "CMD <%s>", secret ? "<password>" : cmd);

    char command[LEN] = {0};
    snprintf(command, LEN, "%s\r\n", cmd);
    int nbytes = 0;
    drain(m_PortFD);
    int rc = tty_write_string(m_PortFD, command, &nbytes);
    if (rc == TTY_OK)
        rc = tty_nread_section(m_PortFD, res, len - 1, '\n', timeout, &nbytes);
    if (rc != TTY_OK)
    {
        char error[MAXRBUF] = {0};
        tty_error_msg(rc, error, MAXRBUF);
        WIFI_LOG(INDI::Logger::DBG_ERROR, "WiFi command error: %s", error);
        return false;
    }

    res[nbytes] = '\0';
    while (nbytes > 0 && (res[nbytes - 1] == '\n' || res[nbytes - 1] == '\r'))
        res[--nbytes] = '\0';

    // AL carries the hotspot password, and the FlatMaster Neo echoes WP:<password> (maybe also AP:).
    WIFI_LOG(INDI::Logger::DBG_DEBUG, "RES <%s>", strcmp(cmd, "AL") == 0 ? "<ssid>:<password>" : secret ? "<hidden>" : res);
    return true;
}

void WiFi::drain(int portFD)
{
    tcflush(portFD, TCIOFLUSH);

    char buffer[LEN];
    while (true)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(portFD, &set);
        struct timeval timeout = {0, 0};
        if (select(portFD + 1, &set, nullptr, nullptr, &timeout) <= 0 || read(portFD, buffer, sizeof(buffer)) <= 0)
            break;
    }
}

void WiFi::updateConnection()
{
    if (m_PortFD >= 0)
        readConnection();
}

bool WiFi::readHotspotStatus()
{
    char res[LEN] = {0};
    if (!sendCommand("A?", res, LEN))
        return false;

    const bool on = strcmp(res, "1") == 0;
    HotspotSP[0].setState(on ? ISS_ON : ISS_OFF);
    HotspotSP[1].setState(on ? ISS_OFF : ISS_ON);
    HotspotSP.setState(IPS_OK);
    HotspotSP.apply();
    return true;
}

bool WiFi::readHotspotCredentials()
{
    // Reply: <ssid>:<password>
    char res[LEN] = {0};
    const char *separator = nullptr;
    if (!sendCommand("AL", res, LEN) || (separator = strchr(res, ':')) == nullptr)
    {
        HotspotTP.setState(IPS_ALERT);
        HotspotTP.apply();
        return false;
    }

    HotspotTP[0].setText(std::string(res, separator - res).c_str());
    HotspotTP[1].setText(separator + 1);
    HotspotTP.setState(IPS_OK);
    HotspotTP.apply();
    return true;
}

bool WiFi::readChannel()
{
    char res[LEN] = {0};
    const int channel = sendCommand("AC", res, LEN) ? atoi(res) : 0;
    if (channel < 1 || channel > 11)
    {
        ChannelNP.setState(IPS_ALERT);
        ChannelNP.apply();
        return false;
    }

    ChannelNP[0].setValue(channel);
    ChannelNP.setState(IPS_OK);
    ChannelNP.apply();
    return true;
}

bool WiFi::readConnection()
{
    // Reply: 0 while not connected, otherwise <ssid>,<ip address>,<signal %>,<security>.
    char res[LEN] = {0};
    if (!sendCommand("WA", res, LEN))
        return false;

    m_Connected = strcmp(res, "0") != 0;
    std::string text = m_Connected ? res : "Not connected";
    if (m_Connected)
    {
        // The SSID may contain commas, so take the other fields from the end.
        const std::string reply(res);
        const size_t security = reply.rfind(',');
        const size_t signal = security == std::string::npos || security == 0 ? std::string::npos : reply.rfind(',', security - 1);
        const size_t address = signal == std::string::npos || signal == 0 ? std::string::npos : reply.rfind(',', signal - 1);
        if (address != std::string::npos && address > 0)
            text = reply.substr(0, address) + " (" + reply.substr(address + 1, signal - address - 1) + "), signal " +
                   reply.substr(signal + 1, security - signal - 1) + "%";
    }

    // This runs on every poll, so tell the clients only about a change.
    if (text != ConnectionTP[0].getText() || ConnectionTP.getState() != IPS_OK)
    {
        ConnectionTP[0].setText(text.c_str());
        ConnectionTP.setState(IPS_OK);
        ConnectionTP.apply();
    }
    return true;
}

bool WiFi::scan()
{
    // The radio is only on with the hotspot enabled or a client connection.
    if (HotspotSP[1].getState() == ISS_ON && !m_Connected)
    {
        WIFI_LOG(INDI::Logger::DBG_WARNING, "%s",
                 "The scan needs the WiFi radio. Enable the hotspot or connect to a WiFi network first.");
        return false;
    }

    WIFI_LOG(INDI::Logger::DBG_SESSION, "%s", "Scanning for WiFi networks...");
    char res[SCAN_LEN] = {0};
    // Right after the radio is switched on, the first scans can come back empty.
    for (int i = 0; i < SCAN_RETRIES; i++)
    {
        if (!sendCommand("WS", res, SCAN_LEN, SCAN_TIMEOUT))
            return false;
        if (strcmp(res, "0") != 0)
            break;
        if (i + 1 < SCAN_RETRIES)
            usleep(SCAN_RETRY_DELAY_US);
    }

    ScanTP[0].setText(strcmp(res, "0") == 0 ? "No networks found" : formatScan(res).c_str());
    ScanTP.setState(IPS_OK);
    ScanTP.apply();
    return true;
}

std::string WiFi::formatScan(const char *reply)
{
    // Reply: <ssid>,<signal %>,<security>: per access point. List each SSID once with its best signal.
    std::vector<std::pair<std::string, int>> networks;
    std::string entries(reply);
    size_t start = 0;
    while (start < entries.size())
    {
        size_t end = entries.find(':', start);
        if (end == std::string::npos)
            end = entries.size();
        const std::string entry = entries.substr(start, end - start);
        start = end + 1;

        // The SSID may contain commas, so take signal and security from the end.
        const size_t security = entry.rfind(',');
        const size_t signal = security == std::string::npos || security == 0 ? std::string::npos : entry.rfind(',', security - 1);
        if (signal == std::string::npos || signal == 0)
            continue;

        const std::string ssid = entry.substr(0, signal);
        const int quality = atoi(entry.substr(signal + 1, security - signal - 1).c_str());
        auto known = std::find_if(networks.begin(), networks.end(), [&](const auto & network)
        {
            return network.first == ssid;
        });
        if (known == networks.end())
            networks.emplace_back(ssid, quality);
        else
            known->second = std::max(known->second, quality);
    }

    std::stable_sort(networks.begin(), networks.end(), [](const auto & a, const auto & b)
    {
        return a.second > b.second;
    });

    std::string text;
    for (const auto &network : networks)
        text += (text.empty() ? "" : ", ") + network.first + " (" + std::to_string(network.second) + "%)";
    return text.empty() ? std::string(reply) : text;
}

bool WiFi::validText(const char *text, size_t maxLength)
{
    // A newline would end the command early.
    return text != nullptr && text[0] != '\0' && strlen(text) <= maxLength && strpbrk(text, "\r\n") == nullptr;
}

bool WiFi::setHotspotCredentials(const char *ssid, const char *password)
{
    if (!validText(ssid, 32) || !validText(password, 63) || strlen(password) < 8)
    {
        WIFI_LOG(INDI::Logger::DBG_ERROR, "%s",
                 "Hotspot SSID must have 1 to 32 characters and the password 8 to 63 characters.");
        return false;
    }

    // The FocusCube 3 and Falcon V2 answer 1, the FlatMaster Neo echoes the command; only 0 is a failure.
    char cmd[LEN] = {0}, res[LEN] = {0};
    snprintf(cmd, LEN, "AN:%s", ssid);
    if (!sendCommand(cmd, res, LEN) || strcmp(res, "0") == 0)
    {
        WIFI_LOG(INDI::Logger::DBG_ERROR, "Setting the hotspot SSID failed: %s", res);
        return false;
    }

    // The reply may echo the password, so it is not logged.
    snprintf(cmd, LEN, "AP:%s", password);
    if (!sendCommand(cmd, res, LEN, 3, true) || strcmp(res, "0") == 0)
    {
        WIFI_LOG(INDI::Logger::DBG_ERROR, "%s", "Setting the hotspot password failed.");
        return false;
    }

    return true;
}

bool WiFi::connectToNetwork(const char *ssid, const char *password)
{
    // An open network has no password; where allowed, the device gets a placeholder instead.
    const bool open = m_AllowOpenNetworks && password != nullptr && password[0] == '\0';
    if (!validText(ssid, 32) || (!open && (!validText(password, 63) || strlen(password) < 8)))
    {
        WIFI_LOG(INDI::Logger::DBG_ERROR, "%s", m_AllowOpenNetworks ?
                 "Enter the WiFi SSID (1 to 32 characters) and password (8 to 63 characters, empty for an open network) first." :
                 "Enter the WiFi SSID (1 to 32 characters) and password (8 to 63 characters) first.");
        return false;
    }

    char cmd[LEN] = {0}, res[LEN] = {0};
    snprintf(cmd, LEN, "WN:%s", ssid);
    if (!sendCommand(cmd, res, LEN) || strcmp(res, "0") == 0)
    {
        WIFI_LOG(INDI::Logger::DBG_ERROR, "Setting the WiFi SSID failed: %s", res);
        return false;
    }

    // The FlatMaster Neo echoes WP:<password>, so the reply is not logged.
    snprintf(cmd, LEN, "WP:%s", open ? OPEN_NETWORK_PASSWORD : password);
    if (!sendCommand(cmd, res, LEN, 3, true) || strcmp(res, "0") == 0)
    {
        WIFI_LOG(INDI::Logger::DBG_ERROR, "%s", "Setting the WiFi password failed.");
        return false;
    }

    // The device connects in the background (about 5 seconds) and retries every 10 seconds.
    WIFI_LOG(INDI::Logger::DBG_SESSION, "Connecting to %s...", ssid);
    IEAddTimer(CONNECT_CHECK_MS, &WiFi::readConnectionLater, this);
    return true;
}

void WiFi::readConnectionLater(void *context)
{
    auto wifi = static_cast<WiFi *>(context);
    if (wifi->m_PortFD < 0 || !wifi->readConnection())
        return;

    if (wifi->m_Connected)
        DEBUGFDEVICE(wifi->m_Device->getDeviceName(), INDI::Logger::DBG_SESSION, "WiFi connected: %s.",
                     wifi->ConnectionTP[0].getText());
    else
        DEBUGFDEVICE(wifi->m_Device->getDeviceName(), INDI::Logger::DBG_WARNING,
                     "Not connected to %s yet. Check SSID and password; the device keeps trying every 10 seconds.",
                     wifi->NetworkTP[0].getText());
}

}
