/*
    INDI alpaca FilterWheel Driver
    
    Copyright (C) 2024 Gord Tulloch

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "indi_alpaca_filterwheel.h"
#include <cstring>
#include <memory>

static std::unique_ptr<AlpacaFilterWheel> alpacaFilterWheel(new AlpacaFilterWheel());

AlpacaFilterWheel::AlpacaFilterWheel()
{
    setVersion(1, 0);
}

const char *AlpacaFilterWheel::getDefaultName()
{
    return "Alpaca Filter Wheel";
}

bool AlpacaFilterWheel::initProperties()
{
    INDI::FilterWheel::initProperties();

    // Set connection mode to TCP only
    setFilterConnection(CONNECTION_TCP);

    // Server address
    IUFillText(&ServerAddressT[0], "HOST", "Host", m_Host.c_str());
    IUFillText(&ServerAddressT[1], "PORT", "Port", std::to_string(m_Port).c_str());
    IUFillTextVector(&ServerAddressTP, ServerAddressT, 2, getDeviceName(), "SERVER_ADDRESS",
                     "Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // Load saved configuration
    defineProperty(&ServerAddressTP);
    loadConfig(true, "SERVER_ADDRESS");
    
    // Update m_Host and m_Port from loaded config
    // Check if config was actually loaded (text will have non-default values)
    if (ServerAddressT[0].text && strlen(ServerAddressT[0].text) > 0)
    {
        m_Host = ServerAddressT[0].text;
    }
    if (ServerAddressT[1].text && strlen(ServerAddressT[1].text) > 0)
    {
        m_Port = std::stoi(ServerAddressT[1].text);
    }

    // Device information
    IUFillText(&DeviceInfoT[0], "DESCRIPTION", "Description", "");
    IUFillText(&DeviceInfoT[1], "DRIVERINFO", "Driver Info", "");
    IUFillText(&DeviceInfoT[2], "DRIVERVERSION", "Driver Version", "");
    IUFillText(&DeviceInfoT[3], "INTERFACEVERSION", "Interface Version", "");
    IUFillTextVector(&DeviceInfoTP, DeviceInfoT, 4, getDeviceName(), "DEVICE_INFO",
                     "Device Info", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // Focus offsets for each filter
    IUFillNumber(&FocusOffsetsN[0], "OFFSET_0", "Dark Offset", "%.0f", -1000, 1000, 1, 0);
    IUFillNumber(&FocusOffsetsN[1], "OFFSET_1", "IR Offset", "%.0f", -1000, 1000, 1, 0);
    IUFillNumber(&FocusOffsetsN[2], "OFFSET_2", "LP Offset", "%.0f", -1000, 1000, 1, 0);
    IUFillNumberVector(&FocusOffsetsNP, FocusOffsetsN, 3, getDeviceName(), "FOCUS_OFFSETS",
                       "Focus Offsets", FILTER_TAB, IP_RO, 60, IPS_IDLE);

    addDebugControl();

    return true;
}

bool AlpacaFilterWheel::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        defineProperty(&DeviceInfoTP);
        defineProperty(&FocusOffsetsNP);
    }
    else
    {
        deleteProperty(DeviceInfoTP.name);
        deleteProperty(FocusOffsetsNP.name);
    }

    return true;
}

bool AlpacaFilterWheel::Connect()
{
    LOG_INFO("Connecting to alpaca FilterWheel...");

    // Create HTTP client
    m_AlpacaClient.reset(new httplib::Client(m_Host, m_Port));
    if (!m_AlpacaClient)
    {
        LOG_ERROR("Failed to create HTTP client");
        return false;
    }

    m_AlpacaClient->set_connection_timeout(5, 0);
    m_AlpacaClient->set_read_timeout(10, 0);

    // Test connection with /connected endpoint
    nlohmann::json response;
    if (!sendAlpacaGET("/connected", response))
    {
        LOG_ERROR("Failed to connect to Alpaca device");
        return false;
    }

    LOG_INFO("Connected to Alpaca device");

    // Connect the device
    if (!sendAlpacaPUT("/connected", "Connected=true", response))
    {
        LOG_ERROR("Failed to set device connected state");
        return false;
    }

    // Query device information
    if (sendAlpacaGET("/description", response) && response.contains("Value"))
    {
        std::string desc = response["Value"].get<std::string>();
        IUSaveText(&DeviceInfoT[0], desc.c_str());
    }

    if (sendAlpacaGET("/driverinfo", response) && response.contains("Value"))
    {
        std::string info = response["Value"].get<std::string>();
        IUSaveText(&DeviceInfoT[1], info.c_str());
    }

    if (sendAlpacaGET("/driverversion", response) && response.contains("Value"))
    {
        std::string ver = response["Value"].get<std::string>();
        IUSaveText(&DeviceInfoT[2], ver.c_str());
    }

    if (sendAlpacaGET("/interfaceversion", response) && response.contains("Value"))
    {
        int ifver = response["Value"].get<int>();
        IUSaveText(&DeviceInfoT[3], std::to_string(ifver).c_str());
    }

    IDSetText(&DeviceInfoTP, nullptr);

    // Setup filter wheel
    if (!setupFilterWheel())
    {
        LOG_ERROR("Failed to setup filter wheel");
        return false;
    }

    LOG_INFO("alpaca FilterWheel connected successfully");
    return true;
}

bool AlpacaFilterWheel::Disconnect()
{
    LOG_INFO("Disconnecting alpaca FilterWheel...");

    // Disconnect the device
    nlohmann::json response;
    sendAlpacaPUT("/connected", "Connected=false", response);

    m_AlpacaClient.reset();

    LOG_INFO("alpaca FilterWheel disconnected");
    return true;
}

bool AlpacaFilterWheel::setupFilterWheel()
{
    nlohmann::json response;

    // Query filter names
    if (sendAlpacaGET("/names", response) && response.contains("Value"))
    {
        auto names = response["Value"];
        if (names.is_array())
        {
            // Resize filter name property
            FilterNameTP.resize(names.size());
            
            // Set filter names
            for (size_t i = 0; i < names.size(); i++)
            {
                std::string name = names[i].get<std::string>();
                FilterNameTP[i].setText(name.c_str());
                LOGF_INFO("Filter %zu: %s", i, name.c_str());
            }

            FilterNameTP.apply();
            LOGF_INFO("Found %zu filters", names.size());
        }
    }

    // Query focus offsets
    if (sendAlpacaGET("/focusoffsets", response) && response.contains("Value"))
    {
        auto offsets = response["Value"];
        if (offsets.is_array())
        {
            for (size_t i = 0; i < offsets.size() && i < 3; i++)
            {
                int offset = offsets[i].get<int>();
                FocusOffsetsN[i].value = offset;
                LOGF_INFO("Filter %zu focus offset: %d steps", i, offset);
            }
            FocusOffsetsNP.s = IPS_OK;
            IDSetNumber(&FocusOffsetsNP, nullptr);
        }
    }

    // Query current position
    int currentPos = QueryFilter();
    if (currentPos >= 0)
    {
        LOGF_INFO("Current filter position: %d", currentPos);
        FilterSlotNP[0].setValue(currentPos + 1); // INDI uses 1-based indexing
        FilterSlotNP.apply();
    }

    return true;
}

bool AlpacaFilterWheel::SelectFilter(int position)
{
    // Convert from INDI 1-based to ASCOM 0-based
    int targetPos = position - 1;

    const char *filterName = (targetPos < FilterNameTP.size()) ? FilterNameTP[targetPos].getText() : "Unknown";
    LOGF_INFO("Selecting filter position %d (%s)", targetPos, filterName);

    // Check if already at position to avoid error 1279
    int currentPos = QueryFilter();
    if (currentPos == targetPos)
    {
        LOGF_INFO("Already at position %d, no movement needed", targetPos);
        return true;
    }

    nlohmann::json response;
    std::string data = "Position=" + std::to_string(targetPos);
    
    if (!sendAlpacaPUT("/position", data, response))
    {
        LOGF_ERROR("Failed to set filter position to %d", targetPos);
        return false;
    }

    // Check for errors
    if (response.contains("ErrorNumber"))
    {
        int errorNum = response["ErrorNumber"].get<int>();
        if (errorNum != 0)
        {
            std::string errorMsg = response.contains("ErrorMessage") ? 
                                 response["ErrorMessage"].get<std::string>() : "Unknown error";
            LOGF_ERROR("Error setting filter position: %d - %s", errorNum, errorMsg.c_str());
            return false;
        }
    }

    LOGF_INFO("Filter position set to %d", targetPos);
    return true;
}

int AlpacaFilterWheel::QueryFilter()
{
    nlohmann::json response;
    
    if (!sendAlpacaGET("/position", response))
    {
        LOG_ERROR("Failed to query filter position");
        return -1;
    }

    if (response.contains("Value"))
    {
        int pos = response["Value"].get<int>();
        return pos; // Return 0-based position
    }

    return -1;
}

bool AlpacaFilterWheel::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Server address
        if (strcmp(name, ServerAddressTP.name) == 0)
        {
            IUUpdateText(&ServerAddressTP, texts, names, n);
            
            m_Host = ServerAddressT[0].text;
            m_Port = std::stoi(ServerAddressT[1].text);
            
            ServerAddressTP.s = IPS_OK;
            IDSetText(&ServerAddressTP, nullptr);
            saveConfig(true, "SERVER_ADDRESS");
            
            LOGF_INFO("Server address updated: %s:%d", m_Host.c_str(), m_Port);
            return true;
        }
    }

    return INDI::FilterWheel::ISNewText(dev, name, texts, names, n);
}

bool AlpacaFilterWheel::sendAlpacaGET(const std::string &endpoint, nlohmann::json &response)
{
    if (!m_AlpacaClient)
        return false;

    std::string url = "/api/v1/filterwheel/" + std::to_string(m_DeviceNumber) + endpoint;
    
    LOGF_DEBUG("GET %s", url.c_str());
    
    auto res = m_AlpacaClient->Get(url.c_str());
    
    if (!res)
    {
        LOGF_ERROR("HTTP GET failed for %s", url.c_str());
        return false;
    }

    if (res->status != 200)
    {
        LOGF_ERROR("HTTP GET returned status %d for %s", res->status, url.c_str());
        return false;
    }

    try
    {
        response = nlohmann::json::parse(res->body);
        
        // Check for Alpaca errors
        if (response.contains("ErrorNumber"))
        {
            int errorNum = response["ErrorNumber"].get<int>();
            if (errorNum != 0)
            {
                std::string errorMsg = response.contains("ErrorMessage") ? 
                                     response["ErrorMessage"].get<std::string>() : "Unknown error";
                LOGF_WARN("Alpaca error %d: %s", errorNum, errorMsg.c_str());
                return false;
            }
        }
        
        return true;
    }
    catch (const nlohmann::json::exception &e)
    {
        LOGF_ERROR("JSON parse error: %s", e.what());
        return false;
    }
}

bool AlpacaFilterWheel::sendAlpacaPUT(const std::string &endpoint, const std::string &data, nlohmann::json &response)
{
    if (!m_AlpacaClient)
        return false;

    std::string url = "/api/v1/filterwheel/" + std::to_string(m_DeviceNumber) + endpoint;
    
    // Add client info to data
    std::string fullData = data;
    if (!fullData.empty())
        fullData += "&";
    fullData += "ClientID=" + std::to_string(m_ClientID);
    fullData += "&ClientTransactionID=" + std::to_string(++m_TransactionID);
    
    LOGF_DEBUG("PUT %s: %s", url.c_str(), fullData.c_str());
    
    auto res = m_AlpacaClient->Put(url.c_str(), fullData, "application/x-www-form-urlencoded");
    
    if (!res)
    {
        LOGF_ERROR("HTTP PUT failed for %s", url.c_str());
        return false;
    }

    if (res->status != 200)
    {
        LOGF_ERROR("HTTP PUT returned status %d for %s", res->status, url.c_str());
        return false;
    }

    try
    {
        response = nlohmann::json::parse(res->body);
        
        // Check for Alpaca errors (but ignore error 1279 - already at position)
        if (response.contains("ErrorNumber"))
        {
            int errorNum = response["ErrorNumber"].get<int>();
            if (errorNum != 0 && errorNum != 1279)
            {
                std::string errorMsg = response.contains("ErrorMessage") ? 
                                     response["ErrorMessage"].get<std::string>() : "Unknown error";
                LOGF_WARN("Alpaca error %d: %s", errorNum, errorMsg.c_str());
                return false;
            }
        }
        
        return true;
    }
    catch (const nlohmann::json::exception &e)
    {
        LOGF_ERROR("JSON parse error: %s", e.what());
        return false;
    }
}
