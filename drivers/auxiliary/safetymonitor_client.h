/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Safety Monitor Client

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

#include "baseclient.h"
#include "basedevice.h"
#include "indipropertylight.h"

#include <string>
#include <functional>

/**
 * @brief SafetyMonitorClient connects to a remote INDI device and monitors its SAFETY_STATUS property
 */
class SafetyMonitorClient : public INDI::BaseClient
{
    public:
        SafetyMonitorClient(const std::string &deviceName, std::function<void()> statusCallback);
        ~SafetyMonitorClient();

        /**
         * @brief Connect to an INDI server
         * @param host Server hostname
         * @param port Server port
         * @return True if connection initiated successfully
         */
        bool connectToServer(const std::string &host, int port);

        /**
         * @brief Get the current safety status
         * @return Current IPState of the monitored device's SAFETY_STATUS property
         */
        IPState getSafetyStatus() const;

        /**
         * @brief Get the monitored device name
         * @return Device name
         */
        const std::string& getMonitoredDeviceName() const { return m_DeviceName; }

        /**
         * @brief Check if the monitored device is online
         * @return True if device is online
         */
        bool isDeviceOnline() const { return m_DeviceOnline; }

        /**
         * @brief Check if the SAFETY_STATUS property has been received
         * @return True if property is available
         */
        bool hasSafetyStatus() const { return m_HasSafetyStatus; }
        
        /**
         * @brief Get the device name for logging purposes (INDI logging requirement)
         * @return Device name string for logging
         */
        const char* getDeviceName() const { return "Safety Monitor"; }

    protected:
        void newDevice(INDI::BaseDevice dp) override;
        void removeDevice(INDI::BaseDevice dp) override;
        void newProperty(INDI::Property property) override;
        void updateProperty(INDI::Property property) override;
        void serverConnected() override;
        void serverDisconnected(int exitCode) override;

    private:
        std::string m_DeviceName;
        bool m_DeviceOnline;
        bool m_HasSafetyStatus;
        INDI::PropertyLight m_SafetyStatusLP;
        std::function<void()> m_StatusCallback;
};
