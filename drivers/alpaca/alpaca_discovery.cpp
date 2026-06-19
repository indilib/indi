/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Discovery Protocol

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

#include "alpaca_discovery.h"
#include "indilogger.h"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <poll.h>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

// Discovery message
// According to the ASCOM Alpaca API Reference, the discovery message should be "alpacadiscovery1"
// where "1" is the version number. However, we check only for the prefix "alpacadiscovery"
// to be more flexible and handle future versions.
static const char* DISCOVERY_MESSAGE = "alpacadiscovery";

AlpacaDiscovery::AlpacaDiscovery(int discoveryPort, int alpacaPort)
    : m_DiscoveryPort(discoveryPort)
    , m_AlpacaPort(alpacaPort)
    , m_Running(false)
    , m_StopRequested(false)
{
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Alpaca discovery initialized");
}

AlpacaDiscovery::~AlpacaDiscovery()
{
    // Stop the discovery server if it's running
    if (m_Running)
        stop();

    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Alpaca discovery destroyed");
}

bool AlpacaDiscovery::start()
{
    if (m_Running)
    {
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_WARNING, "Discovery server already running");
        return true;
    }

    m_StopRequested = false;
    m_DiscoveryThread = std::thread(&AlpacaDiscovery::discoveryThreadFunc, this);

    // Wait a bit to make sure server starts
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!m_Running)
    {
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to start discovery server");
        return false;
    }

    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Discovery server started on port %d", m_DiscoveryPort);
    return true;
}

bool AlpacaDiscovery::stop()
{
    if (!m_Running)
    {
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_WARNING, "Discovery server not running");
        return true;
    }

    // Signal thread to stop
    m_StopRequested = true;

    // Wait for thread to finish
    if (m_DiscoveryThread.joinable())
        m_DiscoveryThread.join();

    // Close all sockets
    for (int socket : m_Sockets)
    {
        if (socket >= 0)
            close(socket);
    }
    m_Sockets.clear();

    m_Running = false;
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Discovery server stopped");
    return true;
}

bool AlpacaDiscovery::isRunning() const
{
    return m_Running;
}

void AlpacaDiscovery::setDiscoveryPort(int port)
{
    if (port > 0 && port < 65536)
    {
        m_DiscoveryPort = port;
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Discovery port set to %d", port);
    }
    else
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Invalid discovery port: %d", port);
    }
}

int AlpacaDiscovery::getDiscoveryPort() const
{
    return m_DiscoveryPort;
}

void AlpacaDiscovery::setAlpacaPort(int port)
{
    if (port > 0 && port < 65536)
    {
        m_AlpacaPort = port;
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Alpaca port set to %d", port);
    }
    else
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Invalid Alpaca port: %d", port);
    }
}

int AlpacaDiscovery::getAlpacaPort() const
{
    return m_AlpacaPort;
}

void AlpacaDiscovery::discoveryThreadFunc()
{
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Starting discovery thread");

    // Create IPv4 socket
    int ipv4Socket = createIPv4Socket();
    if (ipv4Socket >= 0)
    {
        m_Sockets.push_back(ipv4Socket);
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "IPv4 socket created successfully");
    }
    else
    {
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to create IPv4 socket");
    }

    // Create IPv6 socket
    int ipv6Socket = createIPv6Socket();
    if (ipv6Socket >= 0)
    {
        m_Sockets.push_back(ipv6Socket);
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "IPv6 socket created successfully");
    }
    else
    {
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_WARNING, "Failed to create IPv6 socket");
    }

    // Check if we have at least one socket
    if (m_Sockets.empty())
    {
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "No sockets created, discovery server cannot start");
        return;
    }

    // Set up poll structures
    std::vector<struct pollfd> fds;
    for (int socket : m_Sockets)
    {
        struct pollfd pfd;
        pfd.fd = socket;
        pfd.events = POLLIN;
        fds.push_back(pfd);
    }

    // Buffer for incoming messages
    char buffer[1024];

    // Mark as running
    m_Running = true;

    // Main loop
    while (!m_StopRequested)
    {
        // Wait for data on any socket
        int result = poll(fds.data(), fds.size(), 1000); // 1 second timeout

        if (result < 0)
        {
            // Error
            DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Poll error: %s", strerror(errno));
            break;
        }
        else if (result == 0)
        {
            // Timeout, continue
            continue;
        }

        // Check each socket
        for (size_t i = 0; i < fds.size(); i++)
        {
            if (fds[i].revents & POLLIN)
            {
                // Data available on this socket
                struct sockaddr_storage senderAddr;
                socklen_t senderAddrLen = sizeof(senderAddr);

                // Receive data
                ssize_t bytesRead = recvfrom(fds[i].fd, buffer, sizeof(buffer) - 1, 0,
                                             (struct sockaddr*)&senderAddr, &senderAddrLen);

                if (bytesRead > 0)
                {
                    // Null-terminate the buffer
                    buffer[bytesRead] = '\0';

                    // Process the request
                    processDiscoveryRequest(fds[i].fd, buffer, bytesRead,
                                            (struct sockaddr*)&senderAddr, senderAddrLen);
                }
            }
        }
    }

    // Close all sockets
    for (int socket : m_Sockets)
    {
        if (socket >= 0)
            close(socket);
    }
    m_Sockets.clear();

    m_Running = false;
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Discovery thread stopped");
}

int AlpacaDiscovery::createIPv4Socket()
{
    // Create socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to create IPv4 socket: %s", strerror(errno));
        return -1;
    }

    // Set socket options
    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(sock);
        return -1;
    }

#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_WARNING, "Failed to set SO_REUSEPORT: %s", strerror(errno));
    }
#endif

    // Bind to the discovery port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_DiscoveryPort);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to bind IPv4 socket: %s", strerror(errno));
        close(sock);
        return -1;
    }

    return sock;
}

int AlpacaDiscovery::createIPv6Socket()
{
    // Create socket
    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to create IPv6 socket: %s", strerror(errno));
        return -1;
    }

    // Set socket options
    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(sock);
        return -1;
    }

#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_WARNING, "Failed to set SO_REUSEPORT: %s", strerror(errno));
    }
#endif

    // Allow IPv4 connections on IPv6 socket
    optval = 0;
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval)) < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_WARNING, "Failed to clear IPV6_V6ONLY: %s", strerror(errno));
    }

    // Bind to the discovery port
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(m_DiscoveryPort);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to bind IPv6 socket: %s", strerror(errno));
        close(sock);
        return -1;
    }

    // Join IPv6 multicast group
    struct ipv6_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));

    // Set the multicast address (ff12::a1:9aca - Alpaca IPv6 multicast address)
    if (inet_pton(AF_INET6, "ff12::a1:9aca", &mreq.ipv6mr_multiaddr) <= 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to set multicast address: %s", strerror(errno));
        close(sock);
        return -1;
    }

    // Set the interface index (0 = any interface)
    mreq.ipv6mr_interface = 0;

    if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to join multicast group: %s", strerror(errno));
        close(sock);
        return -1;
    }

    return sock;
}

void AlpacaDiscovery::processDiscoveryRequest(int socket, const char* buffer, size_t length,
        const struct sockaddr* senderAddr, socklen_t senderAddrLen)
{
    INDI_UNUSED(length);

    // Check if the message is a discovery request
    if (strncmp(buffer, DISCOVERY_MESSAGE, strlen(DISCOVERY_MESSAGE)) == 0)
    {
        // Log the request
        char addrStr[INET6_ADDRSTRLEN];
        uint16_t port = 0;

        if (senderAddr->sa_family == AF_INET)
        {
            struct sockaddr_in* addr = (struct sockaddr_in*)senderAddr;
            inet_ntop(AF_INET, &addr->sin_addr, addrStr, sizeof(addrStr));
            port = ntohs(addr->sin_port);
        }
        else if (senderAddr->sa_family == AF_INET6)
        {
            struct sockaddr_in6* addr = (struct sockaddr_in6*)senderAddr;
            inet_ntop(AF_INET6, &addr->sin6_addr, addrStr, sizeof(addrStr));
            port = ntohs(addr->sin6_port);
        }

        // Extract version number if present
        char version = '?';
        if (length > strlen(DISCOVERY_MESSAGE))
            version = buffer[strlen(DISCOVERY_MESSAGE)];

        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG,
                     "Received discovery request from %s:%d (message: %s, version: %c)",
                     addrStr, port, buffer, version);

        // Send response
        sendDiscoveryResponse(socket, senderAddr, senderAddrLen);
    }
}

void AlpacaDiscovery::sendDiscoveryResponse(int socket, const struct sockaddr* senderAddr, socklen_t senderAddrLen)
{
    // Generate response message
    std::string response = generateResponseMessage();

    // Send response
    ssize_t bytesSent = sendto(socket, response.c_str(), response.length(), 0, senderAddr, senderAddrLen);

    if (bytesSent < 0)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR,
                     "Failed to send discovery response: %s", strerror(errno));
    }
    else
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG,
                     "Sent discovery response: %s", response.c_str());
    }
}

std::string AlpacaDiscovery::generateResponseMessage() const
{
    // Create JSON response
    json response =
    {
        {"AlpacaPort", m_AlpacaPort}
    };

    return response.dump();
}
