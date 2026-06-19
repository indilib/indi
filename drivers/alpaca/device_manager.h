/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Device Manager

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

#include "basedevice.h"
#include "indiproperty.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <httplib.h>

class IDeviceBridge;

// Alpaca device information structure
struct AlpacaDeviceInfo
{
    int deviceNumber;
    std::string deviceName;
    std::string deviceType;
    std::string uniqueID;
};

class AlpacaClient;

class DeviceManager
{
    public:
        // Singleton instance
        static DeviceManager* getInstance();

        ~DeviceManager();

    private:
        // Private constructor for singleton
        DeviceManager();

    public:

        // Set the AlpacaClient
        void setAlpacaClient(const std::shared_ptr<AlpacaClient> &client);

        // Device management
        void addDevice(INDI::BaseDevice device);
        void removeDevice(INDI::BaseDevice device);
        void updateDeviceProperty(INDI::Property property);

        // Alpaca API request handling
        void handleAlpacaRequest(const httplib::Request &req, httplib::Response &res);

        // Alpaca Setup API request handling
        void handleSetupRequest(const httplib::Request &req, httplib::Response &res);

        // Get device information
        std::vector<AlpacaDeviceInfo> getDeviceList();

        // Send commands to INDI server
        void sendNewNumber(const INDI::PropertyNumber &numberProperty);
        void sendNewSwitch(const INDI::PropertySwitch &switchProperty);

    public:
        // Helper method to extract transaction IDs from request
        bool extractTransactionIDs(const httplib::Request &req, httplib::Response &res, int &clientTransactionID,
                                   int &serverTransactionID);

        // Helper method to parse form-urlencoded data from request body
        static void parseFormUrlEncodedBody(const std::string &body, std::map<std::string, std::string> &params);

    private:
        // Create appropriate bridge for device type
        std::unique_ptr<IDeviceBridge> createBridge(INDI::BaseDevice device, int deviceNumber);

        // Route request to appropriate device
        void routeRequest(int deviceNumber, const std::string &deviceType,
                          const std::string &method,
                          const httplib::Request &req, httplib::Response &res,
                          int clientTransactionID, int serverTransactionID);

        // Handle management API requests
        void handleManagementRequest(const std::string &endpoint,
                                     const httplib::Request &req,
                                     httplib::Response &res,
                                     int clientTransactionID, int serverTransactionID);

        // Maps to track devices and bridges
        std::map<std::string, INDI::BaseDevice> m_Devices;  // INDI device name -> device
        std::map<int, std::unique_ptr<IDeviceBridge>> m_Bridges;  // Alpaca device number -> bridge
        std::map<std::string, int> m_DeviceNumberMap;  // INDI device name -> Alpaca device number

        // Next available device number
        int m_NextDeviceNumber {0};

        // AlpacaClient reference
        std::shared_ptr<AlpacaClient> m_Client;

        // Thread safety
        std::mutex m_Mutex;
};
