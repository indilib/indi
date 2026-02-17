/*
    INDI alpaca Focuser Driver
    
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

#include "indi_alpaca_focuser.h"
#include <cstring>
#include <memory>
#include <cmath>

static std::unique_ptr<AlpacaFocuser> alpacaFocuser(new AlpacaFocuser());

AlpacaFocuser::AlpacaFocuser()
{
    setVersion(1, 0);
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_ABORT);
    
    tcpConnection = new Connection::TCP(this);
}

AlpacaFocuser::~AlpacaFocuser()
{
    delete tcpConnection;
}

const char *AlpacaFocuser::getDefaultName()
{
    return "Alpaca Focuser";
}

bool AlpacaFocuser::initProperties()
{
    INDI::Focuser::initProperties();

    // Use built-in TCP connection with default alpaca.local:32323
    setActiveConnection(tcpConnection);
    tcpConnection->setDefaultHost("alpaca.local");
    tcpConnection->setDefaultPort(32323);
    tcpConnection->registerHandshake([&]() { return Handshake(); });
    registerConnection(tcpConnection);

    // Device information
    IUFillText(&DeviceInfoT[0], "DESCRIPTION", "Description", "");
    IUFillText(&DeviceInfoT[1], "DRIVERINFO", "Driver Info", "");
    IUFillText(&DeviceInfoT[2], "DRIVERVERSION", "Driver Version", "");
    IUFillText(&DeviceInfoT[3], "INTERFACEVERSION", "Interface Version", "");
    IUFillTextVector(&DeviceInfoTP, DeviceInfoT, 4, getDeviceName(), "DEVICE_INFO",
                     "Device Info", OPTIONS_TAB, IP_RO, 60, IPS_IDLE);

    // Temperature monitoring (read-only)
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Temperature (°C)", "%.2f", -50, 100, 0, 0);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE",
                       "Temperature", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    addDebugControl();
    setDefaultPollingPeriod(500); // Poll every 500ms

    return true;
}

bool AlpacaFocuser::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&DeviceInfoTP);
        defineProperty(&TemperatureNP);
    }
    else
    {
        deleteProperty(DeviceInfoTP.name);
        deleteProperty(TemperatureNP.name);
    }

    return true;
}

bool AlpacaFocuser::Connect()
{
    return INDI::Focuser::Connect();
}

bool AlpacaFocuser::Handshake()
{
    LOG_INFO("Connecting to alpaca Focuser...");

    // Get host and port from TCP connection
    if (tcpConnection)
    {
        m_Host = tcpConnection->host();
        m_Port = tcpConnection->port();
    }

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

    // Setup focuser
    if (!setupFocuser())
    {
        LOG_ERROR("Failed to setup focuser");
        return false;
    }

    LOG_INFO("alpaca Focuser connected successfully");
    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool AlpacaFocuser::Disconnect()
{
    LOG_INFO("Disconnecting alpaca Focuser...");

    // Disconnect the device
    nlohmann::json response;
    sendAlpacaPUT("/connected", "Connected=false", response);

    m_AlpacaClient.reset();

    LOG_INFO("alpaca Focuser disconnected");
    return true;
}

bool AlpacaFocuser::setupFocuser()
{
    nlohmann::json response;

    // Query maximum position
    if (sendAlpacaGET("/maxstep", response) && response.contains("Value"))
    {
        int maxStep = response["Value"].get<int>();
        FocusMaxPosNP[0].setValue(maxStep);
        FocusMaxPosNP.setState(IPS_OK);
        FocusMaxPosNP.apply();
        LOGF_INFO("Focuser max position: %d steps", maxStep);
    }

    // Query current position
    int currentPos = getPosition();
    if (currentPos >= 0)
    {
        FocusAbsPosNP[0].setValue(currentPos);
        FocusAbsPosNP.setState(IPS_OK);
        FocusAbsPosNP.apply();
        LOGF_INFO("Current focuser position: %d steps", currentPos);
    }

    // Query temperature
    if (sendAlpacaGET("/temperature", response) && response.contains("Value"))
    {
        double temp = response["Value"].get<double>();
        TemperatureN[0].value = temp;
        TemperatureNP.s = IPS_OK;
        IDSetNumber(&TemperatureNP, nullptr);
        LOGF_INFO("Focuser temperature: %.2f°C", temp);
    }

    // Check if absolute positioning is supported (should be true)
    if (sendAlpacaGET("/absolute", response) && response.contains("Value"))
    {
        bool absolute = response["Value"].get<bool>();
        if (!absolute)
        {
            LOG_WARN("Focuser does not support absolute positioning!");
        }
        else
        {
            LOG_INFO("Absolute positioning confirmed");
        }
    }

    return true;
}

IPState AlpacaFocuser::MoveAbsFocuser(uint32_t targetTicks)
{
    // Validate range
    if (targetTicks > FocusMaxPosNP[0].getValue())
    {
        LOGF_ERROR("Target position %u exceeds maximum %u", targetTicks, 
                   static_cast<uint32_t>(FocusMaxPosNP[0].getValue()));
        return IPS_ALERT;
    }

    LOGF_INFO("Moving to absolute position: %u", targetTicks);

    nlohmann::json response;
    std::string data = "Position=" + std::to_string(targetTicks);
    
    if (!sendAlpacaPUT("/move", data, response))
    {
        LOGF_ERROR("Failed to move to position %u", targetTicks);
        return IPS_ALERT;
    }

    // Check for errors
    if (response.contains("ErrorNumber"))
    {
        int errorNum = response["ErrorNumber"].get<int>();
        if (errorNum != 0)
        {
            std::string errorMsg = response.contains("ErrorMessage") ? 
                                 response["ErrorMessage"].get<std::string>() : "Unknown error";
            LOGF_ERROR("Error moving focuser: %d - %s", errorNum, errorMsg.c_str());
            return IPS_ALERT;
        }
    }

    m_TargetPosition = targetTicks;
    m_Moving = true;
    
    return IPS_BUSY;
}

bool AlpacaFocuser::AbortFocuser()
{
    LOG_INFO("Aborting focuser movement");

    nlohmann::json response;
    
    if (!sendAlpacaPUT("/halt", "", response))
    {
        LOG_ERROR("Failed to halt focuser");
        return false;
    }

    m_Moving = false;
    FocusAbsPosNP.setState(IPS_IDLE);
    FocusAbsPosNP.apply();
    
    LOG_INFO("Focuser movement halted");
    return true;
}

void AlpacaFocuser::TimerHit()
{
    if (!isConnected())
        return;

    // Update temperature
    nlohmann::json response;
    if (sendAlpacaGET("/temperature", response) && response.contains("Value"))
    {
        double temp = response["Value"].get<double>();
        if (std::abs(temp - TemperatureN[0].value) > 0.1)
        {
            TemperatureN[0].value = temp;
            TemperatureNP.s = IPS_OK;
            IDSetNumber(&TemperatureNP, nullptr);
        }
    }

    // Check if moving
    if (m_Moving)
    {
        bool moving = isMoving();
        
        if (!moving)
        {
            // Movement completed
            m_Moving = false;
            
            // Update current position
            int currentPos = getPosition();
            if (currentPos >= 0)
            {
                FocusAbsPosNP[0].setValue(currentPos);
                FocusAbsPosNP.setState(IPS_OK);
                FocusAbsPosNP.apply();
                
                LOGF_INFO("Focuser reached position: %d", currentPos);
            }
        }
        else
        {
            // Still moving, update current position
            int currentPos = getPosition();
            if (currentPos >= 0)
            {
                FocusAbsPosNP[0].setValue(currentPos);
                FocusAbsPosNP.apply();
            }
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool AlpacaFocuser::isMoving()
{
    nlohmann::json response;
    
    if (!sendAlpacaGET("/ismoving", response))
    {
        return false;
    }

    if (response.contains("Value"))
    {
        return response["Value"].get<bool>();
    }

    return false;
}

int AlpacaFocuser::getPosition()
{
    nlohmann::json response;
    
    if (!sendAlpacaGET("/position", response))
    {
        return -1;
    }

    if (response.contains("Value"))
    {
        return response["Value"].get<int>();
    }

    return -1;
}

bool AlpacaFocuser::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Process parent first
    }

    return INDI::Focuser::ISNewText(dev, name, texts, names, n);
}

bool AlpacaFocuser::sendAlpacaGET(const std::string &endpoint, nlohmann::json &response)
{
    if (!m_AlpacaClient)
        return false;

    std::string url = "/api/v1/focuser/" + std::to_string(m_DeviceNumber) + endpoint;
    
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

bool AlpacaFocuser::sendAlpacaPUT(const std::string &endpoint, const std::string &data, nlohmann::json &response)
{
    if (!m_AlpacaClient)
        return false;

    std::string url = "/api/v1/focuser/" + std::to_string(m_DeviceNumber) + endpoint;
    
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
