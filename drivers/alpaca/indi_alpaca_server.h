/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Server

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

#include "defaultdevice.h"
#include "indiproperty.h"
#include <memory>
#include <thread>
#include <atomic>
#include <httplib.h>

class AlpacaClient;
class DeviceManager;
class AlpacaDiscovery;

class INDIAlpacaServer : public INDI::DefaultDevice
{
    public:
        INDIAlpacaServer();
        virtual ~INDIAlpacaServer();

        virtual bool initProperties() override;
        const char *getDefaultName() override;
        void ISGetProperties(const char *dev) override;
        bool updateProperties() override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;

    protected:
        bool Connect() override;
        bool Disconnect() override;

        void TimerHit() override;

    private:
        // Start/stop the Alpaca server
        bool startAlpacaServer();
        bool stopAlpacaServer();

        // HTTP server thread
        void serverThreadFunc();

        // Properties
        INDI::PropertyText ServerSettingsTP {2};    // Host and port for Alpaca server
        INDI::PropertyText INDIServerSettingsTP {2}; // Host and port for INDI server
        INDI::PropertySwitch ServerControlSP {2};    // Start/stop server
        INDI::PropertyNumber ConnectionSettingsNP {3}; // Timeout, retries, retry delay
        INDI::PropertyNumber DiscoverySettingsNP {1}; // Discovery port
        INDI::PropertyNumber StartupDelayNP {1}; // Delay before connecting to INDI server

        // Components
        std::shared_ptr<AlpacaClient> m_Client;
        std::unique_ptr<httplib::Server> m_Server;
        std::unique_ptr<AlpacaDiscovery> m_Discovery;
        DeviceManager* m_DeviceManager;
        std::thread m_ServerThread;
        std::atomic<bool> m_ServerRunning {false};
};
