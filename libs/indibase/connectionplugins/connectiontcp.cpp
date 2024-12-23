/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "connectiontcp.h"

#include "NetIF.hpp"
#include "indilogger.h"
#include "indistandardproperty.h"

#include <cerrno>
#include <netdb.h>
#include <cstring>
#include <unistd.h>
#include <regex>

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace Connection
{
extern const char *CONNECTION_TAB;

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
TCP::TCP(INDI::DefaultDevice *dev, IPerm permission) : Interface(dev, CONNECTION_TCP), m_Permission(permission)
{
    char defaultHostname[MAXINDINAME] = {0};
    char defaultPort[MAXINDINAME] = {0};

    // Try to load the port from the config file. If that fails, use default port.
    if (IUGetConfigText(dev->getDeviceName(), INDI::SP::DEVICE_ADDRESS, "ADDRESS", defaultHostname, MAXINDINAME) == 0)
        m_ConfigHost = defaultHostname;
    if (IUGetConfigText(dev->getDeviceName(), INDI::SP::DEVICE_ADDRESS, "PORT", defaultPort, MAXINDINAME) == 0)
        m_ConfigPort = defaultPort;

    // Address/Port
    IUFillText(&AddressT[0], "ADDRESS", "Address", defaultHostname);
    IUFillText(&AddressT[1], "PORT", "Port", defaultPort);
    IUFillTextVector(&AddressTP, AddressT, 2, getDeviceName(), "DEVICE_ADDRESS", "Server", CONNECTION_TAB,
                     m_Permission, 60, IPS_IDLE);

    int connectionTypeIndex = 0;
    if (IUGetConfigOnSwitchIndex(dev->getDeviceName(), "CONNECTION_TYPE", &connectionTypeIndex) == 0)
        m_ConfigConnectionType = connectionTypeIndex;
    IUFillSwitch(&TcpUdpS[TYPE_TCP], "TCP", "TCP", connectionTypeIndex == TYPE_TCP ? ISS_ON : ISS_OFF);
    IUFillSwitch(&TcpUdpS[TYPE_UDP], "UDP", "UDP", connectionTypeIndex == TYPE_UDP ? ISS_ON : ISS_OFF);
    IUFillSwitchVector(&TcpUdpSP, TcpUdpS, 2, getDeviceName(), "CONNECTION_TYPE", "Connection Type",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    int autoSearchIndex = 1;
    // Try to load the port from the config file. If that fails, use default port.
    IUGetConfigOnSwitchIndex(dev->getDeviceName(), INDI::SP::DEVICE_AUTO_SEARCH, &autoSearchIndex);
    IUFillSwitch(&LANSearchS[INDI::DefaultDevice::INDI_ENABLED], "INDI_ENABLED", "Enabled",
                 autoSearchIndex == 0 ? ISS_ON : ISS_OFF);
    IUFillSwitch(&LANSearchS[INDI::DefaultDevice::INDI_DISABLED], "INDI_DISABLED", "Disabled",
                 autoSearchIndex == 0 ? ISS_OFF : ISS_ON);
    IUFillSwitchVector(&LANSearchSP, LANSearchS, 2, dev->getDeviceName(), INDI::SP::DEVICE_LAN_SEARCH, "LAN Search",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool TCP::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        // TCP Server settings
        if (!strcmp(name, AddressTP.name))
        {
            IUUpdateText(&AddressTP, texts, names, n);
            AddressTP.s = IPS_OK;
            IDSetText(&AddressTP, nullptr);
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool TCP::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        if (!strcmp(name, TcpUdpSP.name))
        {
            IUUpdateSwitch(&TcpUdpSP, states, names, n);
            TcpUdpSP.s = IPS_OK;

            IDSetSwitch(&TcpUdpSP, nullptr);

            return true;
        }

        // Auto Search Devices on connection failure
        if (!strcmp(name, LANSearchSP.name))
        {
            bool wasEnabled = (LANSearchS[0].s == ISS_ON);

            IUUpdateSwitch(&LANSearchSP, states, names, n);
            LANSearchSP.s = IPS_OK;

            // Only display message if there is an actual change
            if (wasEnabled == false && LANSearchS[0].s == ISS_ON)
                LOG_INFO("LAN search is enabled. When connecting, the driver shall attempt to "
                         "communicate with all devices on the local network until a connection is "
                         "established.");
            else if (wasEnabled && LANSearchS[1].s == ISS_ON)
                LOG_INFO("Auto search is disabled.");
            IDSetSwitch(&LANSearchSP, nullptr);

            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool TCP::establishConnection(const std::string &hostname, const std::string &port, int timeout)
{
    struct sockaddr_in serv_addr;
    struct hostent *hp = nullptr;

    struct timeval ts;
    ts.tv_sec  = timeout <= 0 ? SOCKET_TIMEOUT : timeout;
    ts.tv_usec = 0;

    if (m_SockFD != -1)
        close(m_SockFD);

    if (LANSearchS[INDI::DefaultDevice::INDI_ENABLED].s == ISS_OFF)
        LOGF_INFO("Connecting to %s@%s ...", hostname.c_str(), port.c_str());
    else
        LOGF_DEBUG("Connecting to %s@%s ...", hostname.c_str(), port.c_str());


    // Lookup host name or IPv4 address
    hp = gethostbyname(hostname.c_str());
    if (!hp)
    {
        if (LANSearchS[INDI::DefaultDevice::INDI_ENABLED].s == ISS_OFF)
            LOG_ERROR("Failed to lookup IP Address or hostname.");
        return false;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    serv_addr.sin_port        = htons(atoi(port.c_str()));

    int socketType = 0;
    if (TcpUdpS[0].s == ISS_ON)
    {
        socketType = SOCK_STREAM;
    }
    else
    {
        socketType = SOCK_DGRAM;
    }

    if ((m_SockFD = socket(AF_INET, socketType, 0)) < 0)
    {
        LOG_ERROR("Failed to create socket.");
        return false;
    }

    // Set the socket receiving and sending timeouts
    setsockopt(m_SockFD, SOL_SOCKET, SO_RCVTIMEO, &ts, sizeof(struct timeval));
    setsockopt(m_SockFD, SOL_SOCKET, SO_SNDTIMEO, &ts, sizeof(struct timeval));

    // Connect to the device
    if (::connect(m_SockFD, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        if (LANSearchS[INDI::DefaultDevice::INDI_ENABLED].s == ISS_OFF)
            LOGF_ERROR("Failed to connect to %s@%s: %s.", hostname.c_str(), port.c_str(), strerror(errno));
        close(m_SockFD);
        m_SockFD = -1;
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool TCP::Connect()
{
    if (AddressT[0].text == nullptr || AddressT[0].text[0] == '\0' || AddressT[1].text == nullptr ||
            AddressT[1].text[0] == '\0')
    {
        LOG_ERROR("Error! Server address is missing or invalid.");
        return false;
    }

    bool handshakeResult = true;
    std::string hostname = AddressT[0].text;
    std::string port = AddressT[1].text;

    // Should just call handshake on simulation
    if (m_Device->isSimulation())
        handshakeResult = Handshake();

    else
    {
        handshakeResult = false;
        std::regex ipv4("^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
        const auto isIPv4 = regex_match(hostname, ipv4);

        // Establish connection to host:port
        if (establishConnection(hostname, port))
        {
            PortFD = m_SockFD;
            LOGF_DEBUG("Connection to %s@%s is successful, attempting handshake...", hostname.c_str(), port.c_str());
            handshakeResult = Handshake();

            // Auto search is disabled.
            if (handshakeResult == false && LANSearchS[INDI::DefaultDevice::INDI_ENABLED].s == ISS_OFF)
            {
                LOG_DEBUG("Handshake failed.");
                return false;
            }
        }

        // If connection failed; or
        // handshake failed; then we can search LAN if the IP address is v4
        // LAN search is enabled.
        if (handshakeResult == false &&
                LANSearchS[INDI::DefaultDevice::INDI_ENABLED].s == ISS_ON &&
                isIPv4)
        {
            size_t found = hostname.find_last_of(".");

            if (found != std::string::npos)
            {
                // Get the source subnet
                const auto sourceSubnet = hostname.substr(0, found);
                std::deque<std::string> subnets;

                // Get all interface IPv4 addresses. From there we extract subnets
                auto addrs_ipv4 = gmlc::netif::getInterfaceAddressesV4();
                for (auto &oneInterfaceAddress : addrs_ipv4)
                {
                    // Skip local IPs
                    if (oneInterfaceAddress.rfind("127", 0) == 0)
                        continue;

                    size_t found = oneInterfaceAddress.find_last_of(".");
                    if (found != std::string::npos)
                    {
                        // Extract target subnect
                        const auto targetSubnet = oneInterfaceAddress.substr(0, found);
                        // Prefer subnets matching source subnet
                        if (targetSubnet == sourceSubnet)
                            subnets.push_front(targetSubnet);
                        else
                            subnets.push_back(targetSubnet);

                    }
                }

                for (auto &oneSubnet : subnets)
                {
                    LOGF_INFO("Searching %s subnet, this operation will take a few minutes to complete. Stand by...", oneSubnet.c_str());
                    // Brute force search through all subnet
                    // N.B. This operation cannot be interrupted.
                    // TODO Must add a method to abort the search.
                    for (int i = 1; i < 255; i++)
                    {
                        const auto newAddress = oneSubnet + "." + std::to_string(i);
                        if (newAddress == hostname)
                            continue;

                        if (establishConnection(newAddress, port, 1))
                        {
                            PortFD = m_SockFD;
                            LOGF_DEBUG("Connection to %s@%s is successful, attempting handshake...", hostname.c_str(), port.c_str());
                            handshakeResult = Handshake();
                            if (handshakeResult)
                            {
                                hostname = newAddress;
                                break;
                            }
                        }
                    }

                    if (handshakeResult)
                        break;
                }
            }
        }
    }

    if (handshakeResult)
    {
        LOGF_INFO("%s is online.", getDeviceName());
        IUSaveText(&AddressT[0], hostname.c_str());

        if (m_ConfigHost != std::string(AddressT[0].text) || m_ConfigPort != std::string(AddressT[1].text))
            m_Device->saveConfig(true, "DEVICE_ADDRESS");
        if (m_ConfigConnectionType != IUFindOnSwitchIndex(&TcpUdpSP))
            m_Device->saveConfig(true, "CONNECTION_TYPE");
        if (LANSearchS[INDI::DefaultDevice::INDI_ENABLED].s == ISS_ON)
        {
            LANSearchS[INDI::DefaultDevice::INDI_ENABLED].s = ISS_OFF;
            LANSearchS[INDI::DefaultDevice::INDI_DISABLED].s = ISS_ON;
            m_Device->saveConfig(true, LANSearchSP.name);
        }
    }
    else
        LOG_DEBUG("Handshake failed.");

    return handshakeResult;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool TCP::Disconnect()
{
    if (m_SockFD > 0)
    {
        close(m_SockFD);
        m_SockFD = PortFD = -1;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void TCP::Activated()
{
    m_Device->defineProperty(&AddressTP);
    if (m_Permission != IP_RO)
    {
        m_Device->defineProperty(&TcpUdpSP);
        m_Device->defineProperty(&LANSearchSP);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void TCP::Deactivated()
{
    m_Device->deleteProperty(AddressTP.name);
    if (m_Permission != IP_RO)
    {
        m_Device->deleteProperty(TcpUdpSP.name);
        m_Device->deleteProperty(LANSearchSP.name);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool TCP::saveConfigItems(FILE * fp)
{
    if (m_Permission != IP_RO)
    {
        IUSaveConfigText(fp, &AddressTP);
        IUSaveConfigSwitch(fp, &TcpUdpSP);
        IUSaveConfigSwitch(fp, &LANSearchSP);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void TCP::setDefaultHost(const char *addressHost)
{
    if (m_ConfigHost.empty())
        IUSaveText(&AddressT[0], addressHost);
    if (m_Device->isInitializationComplete())
        IDSetText(&AddressTP, nullptr);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void TCP::setDefaultPort(uint32_t addressPort)
{
    if (m_ConfigPort.empty())
    {
        char portStr[8];
        snprintf(portStr, 8, "%d", addressPort);
        IUSaveText(&AddressT[1], portStr);
    }
    if (m_Device->isInitializationComplete())
        IDSetText(&AddressTP, nullptr);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/// TODO should be renamed to setDefaultConnectionType
//////////////////////////////////////////////////////////////////////////////////////////////////
void TCP::setConnectionType(int type)
{
    if (m_ConfigConnectionType < 0)
    {
        IUResetSwitch(&TcpUdpSP);
        TcpUdpS[type].s = ISS_ON;
    }
    if (m_Device->isInitializationComplete())
        IDSetSwitch(&TcpUdpSP, nullptr);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void TCP::setLANSearchEnabled(bool enabled)
{
    LANSearchS[INDI::DefaultDevice::INDI_ENABLED].s = enabled ? ISS_ON : ISS_OFF;
    LANSearchS[INDI::DefaultDevice::INDI_DISABLED].s = enabled ? ISS_OFF : ISS_ON;
    if (m_Device->isInitializationComplete())
        IDSetSwitch(&LANSearchSP, nullptr);
}
}
