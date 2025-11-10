/*******************************************************************************
  Copyright(c) 2025 Jérémie Klein. All rights reserved.

  INDI UPS Driver using NUT (Network UPS Tools)
  This driver monitors a UPS through NUT and exposes its status through INDI's weather interface.
  Battery level and power status are mapped to weather parameters for compatibility.

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
#include "connectionplugins/connectiontcp.h"
#include <string>
#include <functional>
#include <map>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

class UPS : public INDI::DefaultDevice
{
    public:
        UPS();
        virtual ~UPS() = default;

        virtual bool initProperties() override;
        const char *getDefaultName() override;
        void ISGetProperties(const char *dev) override;
        virtual bool updateProperties() override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual bool loadConfig(bool silent = false, const char *property = nullptr) override;

    protected:
        bool Connect() override;
        bool Disconnect() override;
        
        // Timer callback for periodic updates
        void TimerHit() override;

    private:
        // Handshake will be called by the Connection::TCP class
        bool Handshake();
        
        // Update UPS status
        void updateUPSStatus();
        
        // NUT helper methods
        bool queryUPSStatus();
        bool parseUPSResponse(const std::string& response);
        std::string makeNUTRequest(const std::string& command);

        // UPS name property
        INDI::PropertyText UPSNameTP {1};  // UPS name in NUT
        
        // Connection settings property
        INDI::PropertyNumber ConnectionSettingsNP {2};  // Retries, retry delay
        
        // Update period
        INDI::PropertyNumber UpdatePeriodNP {1};  // Update period in seconds
        
        // UPS parameters (battery charge, voltage, etc.)
        INDI::PropertyNumber UPSParametersNP {3};
        enum {
            UPS_BATTERY_CHARGE,
            UPS_BATTERY_VOLTAGE,
            UPS_INPUT_VOLTAGE
        };
        
        // Safety status property
        INDI::PropertyLight SafetyStatusLP {1};
        
        // Battery charge thresholds
        INDI::PropertyNumber BatteryThresholdsNP {2};
        enum {
            BATTERY_WARNING_THRESHOLD,
            BATTERY_CRITICAL_THRESHOLD
        };

        // UPS status parameters mapping
        std::map<std::string, std::string> upsParameters;

        // TCP Connection
        Connection::TCP *tcpConnection {nullptr};

        // State variables
        bool LastParseSuccess {false};
        int PortFD {-1};
        int m_TimerID {-1};
};
