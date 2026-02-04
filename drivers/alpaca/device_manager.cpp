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

#include "device_manager.h"
#include "bridges/device_bridge.h"
#include "bridges/telescope_bridge.h"
#include "bridges/camera_bridge.h"
#include "alpaca_client.h"
#include "indilogger.h"

#include <cstring>
#include <sstream>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

// Static instance for singleton
DeviceManager* DeviceManager::getInstance()
{
    static DeviceManager instance;
    return &instance;
}

DeviceManager::DeviceManager()
{
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Device manager initialized");
}

DeviceManager::~DeviceManager()
{
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Device manager destroyed");
}

void DeviceManager::setAlpacaClient(const std::shared_ptr<AlpacaClient> &client)
{
    m_Client = client;
    DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "AlpacaClient set");
}

void DeviceManager::sendNewNumber(const INDI::PropertyNumber &numberProperty)
{
    if (m_Client)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Sending new number property: %s", numberProperty.getName());
        m_Client->sendNewNumber(numberProperty);
    }
    else
    {
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Cannot send new number property: AlpacaClient not set");
    }
}

void DeviceManager::sendNewSwitch(const INDI::PropertySwitch &switchProperty)
{
    if (m_Client)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Sending new switch property: %s", switchProperty.getName());
        m_Client->sendNewSwitch(switchProperty);
    }
    else
    {
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Cannot send new switch property: AlpacaClient not set");
    }
}

void DeviceManager::addDevice(INDI::BaseDevice device)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const char *deviceName = device.getDeviceName();
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Adding device: %s", deviceName);

    // Check if device already exists
    if (m_Devices.find(deviceName) != m_Devices.end())
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Device %s already exists, updating", deviceName);
        m_Devices[deviceName] = device;

        // Check if we need to create a bridge for this device
        if (m_DeviceNumberMap.find(deviceName) == m_DeviceNumberMap.end())
        {
            // Check if device is ready (has non-zero interface)
            uint32_t interface = device.getDriverInterface();
            if (interface > 0)
            {
                // Create bridge for device
                int deviceNumber = m_NextDeviceNumber++;
                auto bridge = createBridge(device, deviceNumber);
                if (bridge)
                {
                    m_Bridges[deviceNumber] = std::move(bridge);
                    m_DeviceNumberMap[deviceName] = deviceNumber;
                    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Created bridge for device %s with number %d", deviceName,
                                 deviceNumber);
                }
                else
                {
                    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to create bridge for device %s", deviceName);
                }
            }
            else
            {
                DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG,
                             "Device %s not ready yet (interface = 0), waiting for driver info", deviceName);
            }
        }
        return;
    }

    // Add device to map
    m_Devices[deviceName] = device;

    // Check if device is ready (has non-zero interface)
    uint32_t interface = device.getDriverInterface();
    if (interface > 0)
    {
        // Create bridge for device
        int deviceNumber = m_NextDeviceNumber++;
        auto bridge = createBridge(device, deviceNumber);
        if (bridge)
        {
            m_Bridges[deviceNumber] = std::move(bridge);
            m_DeviceNumberMap[deviceName] = deviceNumber;
            DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Created bridge for device %s with number %d", deviceName,
                         deviceNumber);
        }
        else
        {
            DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR, "Failed to create bridge for device %s", deviceName);
        }
    }
    else
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG,
                     "Device %s not ready yet (interface = 0), waiting for driver info", deviceName);
    }
}

void DeviceManager::removeDevice(INDI::BaseDevice device)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const char *deviceName = device.getDeviceName();
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Removing device: %s", deviceName);

    // Check if device exists
    auto it = m_DeviceNumberMap.find(deviceName);
    if (it == m_DeviceNumberMap.end())
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Device %s not found", deviceName);
        return;
    }

    // Remove bridge
    int deviceNumber = it->second;
    m_Bridges.erase(deviceNumber);
    m_DeviceNumberMap.erase(it);
    m_Devices.erase(deviceName);

    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Removed device %s with number %d", deviceName, deviceNumber);
}

void DeviceManager::updateDeviceProperty(INDI::Property property)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const char *deviceName = property.getDeviceName();
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Updating property for device %s: %s", deviceName,
                 property.getName());

    // Check if this is a DRIVER_INFO property update
    if (strcmp(property.getName(), "DRIVER_INFO") == 0)
    {
        // Check if we already have a bridge for this device
        if (m_DeviceNumberMap.find(deviceName) == m_DeviceNumberMap.end())
        {
            // Get the device
            auto deviceIt = m_Devices.find(deviceName);
            if (deviceIt != m_Devices.end())
            {
                // Check if device is ready (has non-zero interface)
                uint32_t interface = deviceIt->second.getDriverInterface();
                if (interface > 0)
                {
                    // Create bridge for device
                    int deviceNumber = m_NextDeviceNumber++;
                    auto bridge = createBridge(deviceIt->second, deviceNumber);
                    if (bridge)
                    {
                        m_Bridges[deviceNumber] = std::move(bridge);
                        m_DeviceNumberMap[deviceName] = deviceNumber;
                        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION,
                                     "Created bridge for device %s with number %d after DRIVER_INFO update", deviceName,
                                     deviceNumber);
                    }
                    else
                    {
                        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR,
                                     "Failed to create bridge for device %s after DRIVER_INFO update", deviceName);
                    }
                }
                else
                {
                    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG,
                                 "Device %s still not ready after DRIVER_INFO update (interface = 0)", deviceName);
                }
            }
        }
    }

    // Find device number
    auto it = m_DeviceNumberMap.find(deviceName);
    if (it == m_DeviceNumberMap.end())
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Device %s not found for property update", deviceName);
        return;
    }

    // Update bridge
    int deviceNumber = it->second;
    auto bridgeIt = m_Bridges.find(deviceNumber);
    if (bridgeIt != m_Bridges.end())
    {
        bridgeIt->second->updateProperty(property);
    }
}

std::unique_ptr<IDeviceBridge> DeviceManager::createBridge(INDI::BaseDevice device, int deviceNumber)
{
    // Check device interface to determine type
    uint32_t interface = device.getDriverInterface();

    // Create appropriate bridge based on interface
    if (interface & INDI::BaseDevice::TELESCOPE_INTERFACE)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Creating telescope bridge for device %s",
                     device.getDeviceName());
        return std::make_unique<TelescopeBridge>(device, deviceNumber);
    }
    else if (interface & INDI::BaseDevice::CCD_INTERFACE)
    {
        DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Creating camera bridge for device %s",
                     device.getDeviceName());
        return std::make_unique<CameraBridge>(device, deviceNumber);
    }
    // Add more device types here as they are implemented
    // else if (interface & INDI::BaseDevice::DOME_INTERFACE)
    // {
    //     DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_SESSION, "Creating dome bridge for device %s", device.getDeviceName());
    //     return std::make_unique<DomeBridge>(device, deviceNumber);
    // }

    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_WARNING, "Unsupported device interface: %u for device %s", interface,
                 device.getDeviceName());
    return nullptr;
}

void DeviceManager::handleAlpacaRequest(const httplib::Request &req, httplib::Response &res)
{
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Handling Alpaca request: %s", req.path.c_str());

    // Extract transaction IDs from request - do this only once per request
    int clientTransactionID, serverTransactionID;
    if (!extractTransactionIDs(req, res, clientTransactionID, serverTransactionID))
    {
        // If extraction failed, response has already been set
        return;
    }

    // Parse path to determine if it's a management or device API request
    std::string path = req.path;

    // Management API
    if (path.find("/management/") == 0)
    {
        handleManagementRequest(path.substr(12), req, res, clientTransactionID, serverTransactionID);
        return;
    }

    // Device API
    if (path.find("/api/v1/") == 0)
    {
        std::string apiPath = path.substr(8); // Remove "/api/v1/"

        // Parse device type, number, and method
        std::istringstream ss(apiPath);
        std::string deviceType, deviceNumberStr, method;

        std::getline(ss, deviceType, '/');
        std::getline(ss, deviceNumberStr, '/');
        std::getline(ss, method, '/');

        // Validate
        if (deviceType.empty() || deviceNumberStr.empty() || method.empty())
        {
            json response =
            {
                {"ClientTransactionID", clientTransactionID},
                {"ServerTransactionID", serverTransactionID},
                {"ErrorNumber", 1001},
                {"ErrorMessage", "Invalid API request format"}
            };
            res.set_content(response.dump(), "application/json");
            res.status = 400; // Bad Request
            return;
        }

        // Check if device type is lowercase
        std::string deviceTypeLower = deviceType;
        std::transform(deviceTypeLower.begin(), deviceTypeLower.end(), deviceTypeLower.begin(), ::tolower);
        if (deviceType != deviceTypeLower)
        {
            json response =
            {
                {"ClientTransactionID", clientTransactionID},
                {"ServerTransactionID", serverTransactionID},
                {"ErrorNumber", 1007},
                {"ErrorMessage", "Device type must be lowercase"}
            };
            res.set_content(response.dump(), "application/json");
            res.status = 400; // Bad Request
            return;
        }

        // Convert device number
        int deviceNumber;
        try
        {
            deviceNumber = std::stoi(deviceNumberStr);
        }
        catch (const std::exception &e)
        {
            json response =
            {
                {"ClientTransactionID", clientTransactionID},
                {"ServerTransactionID", serverTransactionID},
                {"ErrorNumber", 1002},
                {"ErrorMessage", "Invalid device number"}
            };
            res.set_content(response.dump(), "application/json");
            res.status = 400; // Bad Request
            return;
        }

        // Route to appropriate device - pass the transaction IDs
        routeRequest(deviceNumber, deviceType, method, req, res, clientTransactionID, serverTransactionID);
        return;
    }

    // Unknown API
    json response =
    {
        {"ClientTransactionID", clientTransactionID},
        {"ServerTransactionID", serverTransactionID},
        {"ErrorNumber", 1000},
        {"ErrorMessage", "Unknown API endpoint"}
    };
    res.set_content(response.dump(), "application/json");
    res.status = 404; // Not Found
}

void DeviceManager::routeRequest(int deviceNumber, const std::string &deviceType,
                                 const std::string &method,
                                 const httplib::Request &req, httplib::Response &res,
                                 int clientTransactionID, int serverTransactionID)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    // Find bridge for device number
    auto it = m_Bridges.find(deviceNumber);
    if (it == m_Bridges.end())
    {
        json response =
        {
            {"ClientTransactionID", clientTransactionID},
            {"ServerTransactionID", serverTransactionID},
            {"ErrorNumber", 1003},
            {"ErrorMessage", "Device not found"}
        };
        res.set_content(response.dump(), "application/json");
        res.status = 404; // Not Found
        return;
    }

    // Check if device type matches
    if (it->second->getDeviceType() != deviceType)
    {
        json response =
        {
            {"ClientTransactionID", clientTransactionID},
            {"ServerTransactionID", serverTransactionID},
            {"ErrorNumber", 1004},
            {"ErrorMessage", "Device type mismatch"}
        };
        res.set_content(response.dump(), "application/json");
        res.status = 400; // Bad Request
        return;
    }

    // Forward request to bridge
    it->second->handleRequest(method, req, res);

    // Add transaction IDs to the response
    try
    {
        auto responseJson = json::parse(res.body);
        responseJson["ClientTransactionID"] = clientTransactionID;
        responseJson["ServerTransactionID"] = serverTransactionID;
        res.set_content(responseJson.dump(), "application/json");
    }
    catch (const std::exception &e)
    {
        // If parsing fails, create a new response with transaction IDs
        DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_ERROR,
                    "Failed to parse bridge response JSON, creating new response with transaction IDs");

        json response =
        {
            {"ClientTransactionID", clientTransactionID},
            {"ServerTransactionID", serverTransactionID},
            {"ErrorNumber", 1006},
            {"ErrorMessage", "Internal server error: Invalid response format"}
        };
        res.set_content(response.dump(), "application/json");
    }
}

// Helper method to extract transaction IDs from request
bool DeviceManager::extractTransactionIDs(const httplib::Request &req, httplib::Response &res, int &clientTransactionID,
        int &serverTransactionID)
{
    // Default values
    clientTransactionID = 0;
    serverTransactionID = 0;

    // Extract client ID and client transaction ID from request parameters
    int clientID = 0;

    // Case-insensitive parameter lookup helper
    auto getParamValueCaseInsensitive = [&req](const std::string & paramName) -> std::string
    {
        // Try exact match first for efficiency
        if (req.has_param(paramName))
            return req.get_param_value(paramName);

        // Try case-insensitive match
        std::string lowerParamName = paramName;
        std::transform(lowerParamName.begin(), lowerParamName.end(), lowerParamName.begin(), ::tolower);

        for (const auto &param : req.params)
        {
            std::string lowerKey = param.first;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
            if (lowerKey == lowerParamName)
                return param.second;
        }

        return "";
    };

    // Extract ClientID - identifies the client application
    std::string clientIdStr = getParamValueCaseInsensitive("clientid");
    if (!clientIdStr.empty())
    {
        try
        {
            clientID = std::stoi(clientIdStr);
        }
        catch (const std::exception &e)
        {
            DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Invalid clientid format");
        }
    }

    // Extract ClientTransactionID - identifies a specific transaction
    std::string clientTransactionIdStr = getParamValueCaseInsensitive("clienttransactionid");
    if (!clientTransactionIdStr.empty())
    {
        try
        {
            // Check if the string is a valid uint32
            unsigned long value = std::stoul(clientTransactionIdStr);
            if (value > UINT32_MAX)
            {
                // Invalid range for uint32
                DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "clienttransactionid out of range for uint32");

                // Set response to 400 Bad Request
                res.status = 400;
                json response =
                {
                    {"ClientTransactionID", 0},
                    {"ServerTransactionID", 0},
                    {"ErrorNumber", 1008},
                    {"ErrorMessage", "Invalid clienttransactionid: must be a uint32 number"}
                };
                res.set_content(response.dump(), "application/json");
                return false;
            }
            clientTransactionID = static_cast<int>(value);
        }
        catch (const std::exception &e)
        {
            // Invalid format or out of range
            DEBUGDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Invalid clienttransactionid format");

            // Set response to 400 Bad Request
            res.status = 400;
            json response =
            {
                {"ClientTransactionID", 0},
                {"ServerTransactionID", 0},
                {"ErrorNumber", 1008},
                {"ErrorMessage", "Invalid clienttransactionid: must be a uint32 number"}
            };
            res.set_content(response.dump(), "application/json");
            return false;
        }
    }

    // For server transaction ID, we'll use a static counter
    static int transactionCounter = 1;
    serverTransactionID = transactionCounter++;

    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG,
                 "Client ID: %d, Transaction IDs - Client: %d, Server: %d",
                 clientID, clientTransactionID, serverTransactionID);

    return true;
}

// Helper method to parse form-urlencoded data from request body
void DeviceManager::parseFormUrlEncodedBody(const std::string &body, std::map<std::string, std::string> &params)
{
    std::istringstream stream(body);
    std::string pair;

    while (std::getline(stream, pair, '&'))
    {
        size_t pos = pair.find('=');
        if (pos != std::string::npos)
        {
            std::string key = pair.substr(0, pos);
            std::string value = pair.substr(pos + 1);

            // URL decode the key and value
            auto decode = [](const std::string & s) -> std::string
            {
                std::string result;
                for (size_t i = 0; i < s.length(); ++i)
                {
                    if (s[i] == '+')
                    {
                        result += ' ';
                    }
                    else if (s[i] == '%' && i + 2 < s.length())
                    {
                        int value;
                        std::istringstream hex_stream(s.substr(i + 1, 2));
                        if (hex_stream >> std::hex >> value)
                        {
                            result += static_cast<char>(value);
                            i += 2;
                        }
                        else
                        {
                            result += s[i];
                        }
                    }
                    else
                    {
                        result += s[i];
                    }
                }
                return result;
            };

            params[decode(key)] = decode(value);
        }
    }
}

void DeviceManager::handleSetupRequest(const httplib::Request &req, httplib::Response &res)
{
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Handling setup request: %s", req.path.c_str());

    // Parse path to determine device type and number
    std::string path = req.path;

    // Setup API format: /setup/v1/{deviceType}/{deviceNumber}/setup
    if (path.find("/setup/v1/") == 0)
    {
        std::string setupPath = path.substr(10); // Remove "/setup/v1/"

        // Parse device type, number, and method
        std::istringstream ss(setupPath);
        std::string deviceType, deviceNumberStr, method;

        std::getline(ss, deviceType, '/');
        std::getline(ss, deviceNumberStr, '/');
        std::getline(ss, method, '/');

        // Validate
        if (deviceType.empty() || deviceNumberStr.empty() || method != "setup")
        {
            res.set_content("<html><body><h1>Invalid Setup Request</h1><p>Invalid URL format. Expected: /setup/v1/{deviceType}/{deviceNumber}/setup</p></body></html>",
                            "text/html");
            res.status = 400; // Bad Request
            return;
        }

        // Check if device type is lowercase
        std::string deviceTypeLower = deviceType;
        std::transform(deviceTypeLower.begin(), deviceTypeLower.end(), deviceTypeLower.begin(), ::tolower);
        if (deviceType != deviceTypeLower)
        {
            res.set_content("<html><body><h1>Invalid Setup Request</h1><p>Device type must be lowercase</p></body></html>",
                            "text/html");
            res.status = 400; // Bad Request
            return;
        }

        // Convert device number
        int deviceNumber;
        try
        {
            deviceNumber = std::stoi(deviceNumberStr);
        }
        catch (const std::exception &e)
        {
            res.set_content("<html><body><h1>Invalid Setup Request</h1><p>Invalid device number</p></body></html>", "text/html");
            res.status = 400; // Bad Request
            return;
        }

        // Find bridge for device number
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Bridges.find(deviceNumber);
        if (it == m_Bridges.end())
        {
            res.set_content("<html><body><h1>Device Not Found</h1><p>The requested device was not found</p></body></html>",
                            "text/html");
            res.status = 404; // Not Found
            return;
        }

        // Check if device type matches
        if (it->second->getDeviceType() != deviceType)
        {
            res.set_content("<html><body><h1>Device Type Mismatch</h1><p>The requested device type does not match the device</p></body></html>",
                            "text/html");
            res.status = 400; // Bad Request
            return;
        }

        // Generate setup page
        std::string deviceName = it->second->getDeviceName();
        std::string uniqueID = it->second->getUniqueID();

        // Create a simple HTML setup page
        std::stringstream html;
        html << "<!DOCTYPE html>\n";
        html << "<html>\n";
        html << "<head>\n";
        html << "    <title>Alpaca Setup - " << deviceName << "</title>\n";
        html << "    <style>\n";
        html << "        body { font-family: Arial, sans-serif; margin: 20px; }\n";
        html << "        h1 { color: #333; }\n";
        html << "        .info { margin-bottom: 20px; }\n";
        html << "        .info div { margin-bottom: 5px; }\n";
        html << "        label { display: inline-block; width: 150px; font-weight: bold; }\n";
        html << "    </style>\n";
        html << "</head>\n";
        html << "<body>\n";
        html << "    <h1>Alpaca Device Setup</h1>\n";
        html << "    <div class=\"info\">\n";
        html << "        <div><label>Device Name:</label> " << deviceName << "</div>\n";
        html << "        <div><label>Device Type:</label> " << deviceType << "</div>\n";
        html << "        <div><label>Device Number:</label> " << deviceNumber << "</div>\n";
        html << "        <div><label>Unique ID:</label> " << uniqueID << "</div>\n";
        html << "    </div>\n";
        html << "    <p>This is a minimal setup page for the device. Additional device-specific setup options can be added here.</p>\n";
        html << "</body>\n";
        html << "</html>";

        res.set_content(html.str(), "text/html");
    }
    else
    {
        // Unknown setup endpoint
        res.set_content("<html><body><h1>Unknown Setup Endpoint</h1><p>The requested setup endpoint is not valid</p></body></html>",
                        "text/html");
        res.status = 404; // Not Found
    }
}

void DeviceManager::handleManagementRequest(const std::string &endpoint,
        const httplib::Request &req,
        httplib::Response &res,
        int clientTransactionID, int serverTransactionID)
{
    INDI_UNUSED(req);
    DEBUGFDEVICE("INDI Alpaca Server", INDI::Logger::DBG_DEBUG, "Handling management request: %s", endpoint.c_str());

    if (endpoint == "apiversions")
    {
        // Return supported API versions
        json response =
        {
            {"Value", json::array({1})},
            {"ClientTransactionID", clientTransactionID},
            {"ServerTransactionID", serverTransactionID},
            {"ErrorNumber", 0},
            {"ErrorMessage", ""}
        };
        res.set_content(response.dump(), "application/json");
    }
    else if (endpoint == "v1/description")
    {
        // Return server description
        json response =
        {
            {"Value", "INDI Alpaca Server"},
            {"ClientTransactionID", clientTransactionID},
            {"ServerTransactionID", serverTransactionID},
            {"ErrorNumber", 0},
            {"ErrorMessage", ""}
        };
        res.set_content(response.dump(), "application/json");
    }
    else if (endpoint == "v1/configureddevices")
    {
        // Return list of configured devices
        json devices = json::array();

        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const auto &pair : m_Bridges)
        {
            devices.push_back(
            {
                {"DeviceName", pair.second->getDeviceName()},
                {"DeviceType", pair.second->getDeviceType()},
                {"DeviceNumber", pair.second->getDeviceNumber()},
                {"UniqueID", pair.second->getUniqueID()}
            });
        }

        json response =
        {
            {"Value", devices},
            {"ClientTransactionID", clientTransactionID},
            {"ServerTransactionID", serverTransactionID},
            {"ErrorNumber", 0},
            {"ErrorMessage", ""}
        };
        res.set_content(response.dump(), "application/json");
    }
    else
    {
        // Unknown management endpoint
        json response =
        {
            {"ClientTransactionID", clientTransactionID},
            {"ServerTransactionID", serverTransactionID},
            {"ErrorNumber", 1005},
            {"ErrorMessage", "Unknown management endpoint"}
        };
        res.set_content(response.dump(), "application/json");
        res.status = 404; // Not Found
    }
}

std::vector<AlpacaDeviceInfo> DeviceManager::getDeviceList()
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    std::vector<AlpacaDeviceInfo> devices;
    for (const auto &pair : m_Bridges)
    {
        AlpacaDeviceInfo info;
        info.deviceNumber = pair.second->getDeviceNumber();
        info.deviceName = pair.second->getDeviceName();
        info.deviceType = pair.second->getDeviceType();
        info.uniqueID = pair.second->getUniqueID();
        devices.push_back(info);
    }

    return devices;
}
