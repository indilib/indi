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

#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * @brief Implementation of the ASCOM Alpaca Discovery Protocol
 *
 * This class implements the UDP-based discovery protocol for ASCOM Alpaca.
 * It listens for UDP broadcasts on the discovery port (default 32227) and
 * responds with the Alpaca API port.
 *
 * The protocol is defined at: https://github.com/DanielVanNoord/AlpacaDiscoveryTests
 */
class AlpacaDiscovery
{
    public:
        /**
         * @brief Constructor
         *
         * @param discoveryPort The port to listen for discovery requests (default: 32227)
         * @param alpacaPort The port of the Alpaca API server to report in responses
         */
        AlpacaDiscovery(int discoveryPort = 32227, int alpacaPort = 11111);

        /**
         * @brief Destructor
         *
         * Stops the discovery server if it's running
         */
        ~AlpacaDiscovery();

        /**
         * @brief Start the discovery server
         *
         * @return true if started successfully, false otherwise
         */
        bool start();

        /**
         * @brief Stop the discovery server
         *
         * @return true if stopped successfully, false otherwise
         */
        bool stop();

        /**
         * @brief Check if the discovery server is running
         *
         * @return true if running, false otherwise
         */
        bool isRunning() const;

        /**
         * @brief Set the discovery port
         *
         * @param port The port to listen for discovery requests
         * @note This will only take effect after restarting the server
         */
        void setDiscoveryPort(int port);

        /**
         * @brief Get the discovery port
         *
         * @return The current discovery port
         */
        int getDiscoveryPort() const;

        /**
         * @brief Set the Alpaca API port
         *
         * @param port The port of the Alpaca API server
         */
        void setAlpacaPort(int port);

        /**
         * @brief Get the Alpaca API port
         *
         * @return The current Alpaca API port
         */
        int getAlpacaPort() const;

    private:
        /**
         * @brief Thread function for the discovery server
         */
        void discoveryThreadFunc();

        /**
         * @brief Create a socket for IPv4 discovery
         *
         * @return Socket file descriptor, or -1 on error
         */
        int createIPv4Socket();

        /**
         * @brief Create a socket for IPv6 discovery
         *
         * @return Socket file descriptor, or -1 on error
         */
        int createIPv6Socket();

        /**
         * @brief Process a discovery request
         *
         * @param socket Socket file descriptor
         * @param buffer Buffer containing the request
         * @param length Length of the request
         * @param senderAddr Sender address
         * @param senderAddrLen Length of sender address
         */
        void processDiscoveryRequest(int socket, const char* buffer, size_t length,
                                     const struct sockaddr* senderAddr, socklen_t senderAddrLen);

        /**
         * @brief Send a discovery response
         *
         * @param socket Socket file descriptor
         * @param senderAddr Sender address
         * @param senderAddrLen Length of sender address
         */
        void sendDiscoveryResponse(int socket, const struct sockaddr* senderAddr, socklen_t senderAddrLen);

        /**
         * @brief Generate the JSON response message
         *
         * @return JSON response string
         */
        std::string generateResponseMessage() const;

        // Configuration
        int m_DiscoveryPort;
        int m_AlpacaPort;

        // Thread management
        std::thread m_DiscoveryThread;
        std::atomic<bool> m_Running;
        std::atomic<bool> m_StopRequested;

        // Socket file descriptors
        std::vector<int> m_Sockets;
};
