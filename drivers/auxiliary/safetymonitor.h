/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Safety Monitor Driver
  Monitors multiple INDI devices' SAFETY_STATUS properties and reports the worst case

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
#include "safetymonitor_client.h"

#include <memory>
#include <vector>

class SafetyMonitor : public INDI::DefaultDevice
{
    public:
        SafetyMonitor();
        virtual ~SafetyMonitor() = default;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        const char *getDefaultName() override;

    protected:
        virtual bool Connect() override;
        virtual bool Disconnect() override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        /**
         * @brief Parse connection strings and create client instances
         */
        void parseConnectionStrings();
        
        /**
         * @brief Rebuild the SAFETY_STATUS property with dynamic elements
         */
        void rebuildSafetyStatusProperty();
        
        /**
         * @brief Update overall status based on all monitored devices
         * Called by client callbacks when status changes
         */
        void updateOverallStatus();

        // Connection strings property (comma-separated)
        INDI::PropertyText ConnectionStringsTP {1};

        // Dynamic safety status property with one element per monitored device
        INDI::PropertyLight SafetyStatusLP {0};

        // Override switch
        INDI::PropertySwitch OverrideSP {1};

        // Vector of client instances
        std::vector<std::unique_ptr<SafetyMonitorClient>> m_Clients;
};
