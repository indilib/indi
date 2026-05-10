/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Connection Plugin Interface

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

#pragma once

#include "connectioninterface.h"

#include <stdint.h>
#include <cstdlib>
#include <string>

namespace Connection
{
/**
 * @brief The TCP class manages connection with devices over the network via TCP/IP.
 * Upon successful connection, reads & writes from and to the device are performed via the returned file descriptor
 * using standard UNIX read/write functions.
 */

class TCP : public Interface
{
    public:
        // Max base delay for the exponential backoff retry
        static const long long MAX_BACKOFF_BASE_DELAY = 10000LL;
        // Max delay to wait for the next backoff retry
        static const long long MAX_BACKOFF_DELAY = 30000LL;
        // Max retry time
        static const long long MAX_TOTAL_RETRY_TIME_MS = 300000LL; // 5 minutes

        static const int MAX_CONNECT_RETRIES = 10;
        enum ConnectionType
        {
            TYPE_TCP = 0,
            TYPE_UDP
        };

        TCP(INDI::DefaultDevice *dev, IPerm permission = IP_RW);
        virtual ~TCP() = default;

        virtual bool Connect() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

        virtual bool Disconnect() override;

        virtual void Activated() override;

        virtual void Deactivated() override;

        virtual std::string name() override
        {
            return "CONNECTION_TCP";
        }

        virtual std::string label() override
        {
            return "Network";
        }

        virtual const char *host() const
        {
            return AddressT[0].text;
        }
        virtual uint32_t port() const
        {
            return atoi(AddressT[1].text);
        }
        ConnectionType connectionType() const
        {
            return static_cast<ConnectionType>(IUFindOnSwitchIndex(&TcpUdpSP));
        }

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;

        int getPortFD() const
        {
            return PortFD;
        }
        void setDefaultHost(const char *addressHost);
        void setDefaultPort(uint32_t addressPort);
        void setConnectionType(int type);
        void setLANSearchEnabled(bool enabled);

    protected:
        /**
         * @brief establishConnection Create a socket connection to the host and port. If successful, set the socket variable.
         * @param hostname fully qualified hostname or IP address to host
         * @param port Port
         * @param timeout timeout in seconds. If not sent, use default 5 seconds timeout.
         * @return Success if connection established, false otherwise.
         * @note Connection type (TCP vs UDP) is fetched from the TcpUdpSP property.
         */
        bool establishConnection(const std::string &hostname, const std::string &port, int timeout = -1);

        //////////////////////////////////////////////////////////////////////////////////////////////////
        /// Properties
        //////////////////////////////////////////////////////////////////////////////////////////////////
        // IP Address/Port
        ITextVectorProperty AddressTP;
        IText AddressT[2] {};
        // Connection Type
        ISwitch TcpUdpS[2];
        ISwitchVectorProperty TcpUdpSP;
        // Auto search
        ISwitch LANSearchS[2];
        ISwitchVectorProperty LANSearchSP;
        // Retry/backoff configuration
        enum RetryIndex
        {
            RETRY_RETRIES = 0,
            RETRY_BACKOFF_MS = 1,
        };
        INumber RetryN[2];
        INumberVectorProperty RetryNP;

        // Variables
        IPerm m_Permission = IP_RW;
        std::string m_ConfigHost;
        std::string m_ConfigPort;
        int m_ConfigConnectionType {-1};
        int m_SockFD {-1};
        int PortFD = -1;
        static constexpr uint8_t SOCKET_TIMEOUT {5};
        // Runtime-configurable connection retry parameters
        int m_ConnectRetries {3};
        int m_BackoffBaseMs {500}; // milliseconds
        // Flag to defer config persistence (avoid frequent saveConfig calls)
        bool m_RetryConfigDirty {false};
};
}
