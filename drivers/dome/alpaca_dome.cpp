/*******************************************************************************
  Copyright(c) 2025 Jérémie Klein. All rights reserved.

  ASCOM Alpaca Dome INDI Driver

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

#include "alpaca_dome.h"

#include <httplib.h>
#include <string.h>
#include <chrono>
#include <thread>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

// We declare an auto pointer to AlpacaDome
std::unique_ptr<AlpacaDome> alpaca_dome(new AlpacaDome());

AlpacaDome::AlpacaDome()
{
    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
}

bool AlpacaDome::initProperties()
{
    setDomeConnection(INDI::Dome::CONNECTION_NONE);

    INDI::Dome::initProperties();

    // Setup server address properties
    ServerAddressTP[0].fill("HOST", "Host", "");  // Empty default to force configuration
    ServerAddressTP[1].fill("PORT", "Port", "");
    ServerAddressTP.fill(getDeviceName(), "SERVER_ADDRESS", "Server", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Setup device number property
    DeviceNumberNP[0].fill("DEVICE_NUMBER", "Device Number", "%.0f", 0, 10, 1, 0);
    DeviceNumberNP.fill(getDeviceName(), "DEVICE_NUMBER", "Alpaca Device", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Setup connection settings properties
    ConnectionSettingsNP[0].fill("TIMEOUT", "Timeout (sec)", "%.0f", 1, 30, 1, 5);
    ConnectionSettingsNP[1].fill("RETRIES", "Max Retries", "%.0f", 1, 10, 1, 3);
    ConnectionSettingsNP[2].fill("RETRY_DELAY", "Retry Delay (ms)", "%.0f", 100, 5000, 100, 1000);
    ConnectionSettingsNP.fill(getDeviceName(), "CONNECTION_SETTINGS", "Connection", SITE_TAB, IP_RW, 60, IPS_IDLE);

    // Load config before setting any defaults
    loadConfig(true);

    SetParkDataType(PARK_NONE);
    addAuxControls();

    return true;
}

void AlpacaDome::ISGetProperties(const char *dev)
{
    INDI::Dome::ISGetProperties(dev);

    // Always define these properties
    defineProperty(ServerAddressTP);
    defineProperty(DeviceNumberNP);
    defineProperty(ConnectionSettingsNP);
}

bool AlpacaDome::Connect()
{
    if (ServerAddressTP[0].getText() == nullptr || ServerAddressTP[1].getText() == nullptr)
    {
        LOG_ERROR("Server address or port is not set.");
        return false;
    }

    // Test connection by getting dome status
    nlohmann::json response;
    bool success = retryRequest([this, &response]() {
        std::string path = "/api/v1/dome/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/connected";
        return makeAlpacaRequest(path, response);
    });

    if (!success)
    {
        LOG_ERROR("Failed to connect to Alpaca dome. Please check server address and port.");
        return false;
    }

    LOG_INFO("Successfully connected to Alpaca dome.");
    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool AlpacaDome::Disconnect()
{
    LOG_INFO("Disconnected from Alpaca dome.");
    return true;
}

const char *AlpacaDome::getDefaultName()
{
    return "Alpaca Dome";
}

bool AlpacaDome::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (isDeviceNameMatch(dev) && ServerAddressTP.isNameMatch(name))
    {
        ServerAddressTP.update(texts, names, n);
        ServerAddressTP.setState(IPS_OK);
        ServerAddressTP.apply();
        // Save configuration after update
        saveConfig();
        return true;
    }

    return INDI::Dome::ISNewText(dev, name, texts, names, n);
}

bool AlpacaDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        LOG_INFO("Alpaca dome is ready for operation.");
    }
    else
    {
        LOG_INFO("Alpaca dome is disconnected.");
    }

    return true;
}

void AlpacaDome::TimerHit()
{
    if (!isConnected())
        return;

    updateStatus();
    SetTimer(getCurrentPollingPeriod());
}

bool AlpacaDome::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);
    
    ServerAddressTP.save(fp);
    DeviceNumberNP.save(fp);
    ConnectionSettingsNP.save(fp);
    
    return true;
}

bool AlpacaDome::loadConfig(bool silent, const char *property)
{
    // Charger d'abord la configuration parent
    bool result = INDI::Dome::loadConfig(silent, property);

    // Si aucune propriété spécifique n'est demandée, charger toutes nos propriétés
    if (property == nullptr)
    {
        result &= ServerAddressTP.load();
        result &= DeviceNumberNP.load();
        result &= ConnectionSettingsNP.load();
    }
    
    return result;
}

IPState AlpacaDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        if (dir == DOME_CW)  // Open
        {
            LOG_INFO("Opening dome...");
            openRoof();
            return IPS_BUSY;
        }
        else  // Close
        {
            if (INDI::Dome::isLocked())
            {
                LOG_WARN("Cannot close dome when mount is locking. See: Telescope parking policy, in options tab");
                return IPS_ALERT;
            }

            LOG_INFO("Closing dome...");
            closeRoof();
            return IPS_BUSY;
        }
    }

    return (Abort() ? IPS_OK : IPS_ALERT);
}

bool AlpacaDome::makeAlpacaRequest(const std::string& path, nlohmann::json& response, bool isPut)
{
    if (ServerAddressTP[0].getText() == nullptr || ServerAddressTP[1].getText() == nullptr)
    {
        LOG_ERROR("Server address or port is not set.");
        return false;
    }

    try
    {
        httplib::Client cli(ServerAddressTP[0].getText(), std::stoi(ServerAddressTP[1].getText()));
        cli.set_connection_timeout(static_cast<int>(ConnectionSettingsNP[0].getValue()));
        cli.set_read_timeout(static_cast<int>(ConnectionSettingsNP[0].getValue()));

        auto result = isPut ? cli.Put(path.c_str()) : cli.Get(path.c_str());
        
        if (!result)
        {
            LOG_ERROR("Failed to connect to Alpaca server.");
            return false;
        }

        if (result->status != 200)
        {
            LOGF_ERROR("HTTP error: %d", result->status);
            return false;
        }

        response = nlohmann::json::parse(result->body);
        
        if (response["ErrorNumber"].get<int>() != 0)
        {
            LOGF_ERROR("Alpaca error: %s", response["ErrorMessage"].get<std::string>().c_str());
            return false;
        }
        
        return true;
    }
    catch(const std::exception &e)
    {
        LOGF_ERROR("Request error: %s", e.what());
        return false;
    }
}

bool AlpacaDome::retryRequest(const std::function<bool()>& request)
{
    int maxRetries = static_cast<int>(ConnectionSettingsNP[1].getValue());
    int retryDelay = static_cast<int>(ConnectionSettingsNP[2].getValue());

    for (int attempt = 1; attempt <= maxRetries; attempt++)
    {
        try
        {
            if (request())
                return true;
        }
        catch(const std::exception &e)
        {
            LOGF_ERROR("Request attempt %d failed: %s", attempt, e.what());
        }

        if (attempt < maxRetries)
        {
            LOGF_DEBUG("Retrying request in %d ms (attempt %d/%d)", 
                      retryDelay, attempt, maxRetries);
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
        }
    }
    return false;
}

void AlpacaDome::updateStatus()
{
    nlohmann::json response;
    
    // Get shutter status with retry
    bool success = retryRequest([this, &response]() {
        std::string path = "/api/v1/dome/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/shutterstatus";
        return makeAlpacaRequest(path, response);
    });

    if (success)
    {
        int shutterStatus = response["Value"].get<int>();
        
        // Alpaca shutter states:
        // 0 = Open
        // 1 = Closed
        // 2 = Opening
        // 3 = Closing
        // 4 = Error
        switch(shutterStatus)
        {
            case 0: // Open
                if (getDomeState() != DOME_UNPARKED)
                {
                    LOG_INFO("Dome is fully open.");
                    setDomeState(DOME_UNPARKED);
                }
                break;
                
            case 1: // Closed
                if (getDomeState() != DOME_PARKED)
                {
                    LOG_INFO("Dome is fully closed.");
                    setDomeState(DOME_PARKED);
                }
                break;
                
            case 2: // Opening
                if (getDomeState() != DOME_UNPARKING)
                {
                    LOG_INFO("Dome is opening...");
                    setDomeState(DOME_UNPARKING);
                }
                break;
                
            case 3: // Closing
                if (getDomeState() != DOME_PARKING)
                {
                    LOG_INFO("Dome is closing...");
                    setDomeState(DOME_PARKING);
                }
                break;
                
            case 4: // Error
                if (getDomeState() != DOME_ERROR)
                {
                    LOG_ERROR("Dome is in error state.");
                    setDomeState(DOME_ERROR);
                }
                break;
        }
    }

    // Get at home status (optional)
    success = retryRequest([this, &response]() {
        std::string path = "/api/v1/dome/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/athome";
        return makeAlpacaRequest(path, response);
    });

    if (success)
    {
        bool atHome = response["Value"].get<bool>();
        LOGF_DEBUG("Dome at home: %s", atHome ? "Yes" : "No");
    }
}

void AlpacaDome::openRoof()
{
    nlohmann::json response;
    bool success = retryRequest([this, &response]() {
        std::string path = "/api/v1/dome/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/openshutter";
        return makeAlpacaRequest(path, response, true);  // true for PUT request
    });

    if (success)
    {
        LOG_INFO("Dome is opening...");
        setDomeState(DOME_UNPARKING);
    }
}

void AlpacaDome::closeRoof()
{
    nlohmann::json response;
    bool success = retryRequest([this, &response]() {
        std::string path = "/api/v1/dome/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/closeshutter";
        return makeAlpacaRequest(path, response, true);  // true for PUT request
    });

    if (success)
    {
        LOG_INFO("Dome is closing...");
        setDomeState(DOME_PARKING);
    }
}

void AlpacaDome::stopRoof()
{
    nlohmann::json response;
    bool success = retryRequest([this, &response]() {
        std::string path = "/api/v1/dome/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/abortslew";
        return makeAlpacaRequest(path, response, true);  // true for PUT request
    });

    if (success)
    {
        LOG_INFO("Dome movement aborted.");
        setDomeState(DOME_IDLE);
    }
}

IPState AlpacaDome::Park()
{
    nlohmann::json response;
    bool success = retryRequest([this, &response]() {
        std::string path = "/api/v1/dome/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/closeshutter";
        return makeAlpacaRequest(path, response, true);
    });

    if (success)
    {
        LOG_INFO("Parking dome (closing shutter)...");
        setDomeState(DOME_PARKING);
        return IPS_BUSY;
    }
    
    LOG_ERROR("Failed to park dome");
    return IPS_ALERT;
}

IPState AlpacaDome::UnPark()
{
    nlohmann::json response;
    bool success = retryRequest([this, &response]() {
        std::string path = "/api/v1/dome/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/openshutter";
        return makeAlpacaRequest(path, response, true);
    });

    if (success)
    {
        LOG_INFO("Unparking dome (opening shutter)...");
        setDomeState(DOME_UNPARKING);
        return IPS_BUSY;
    }

    LOG_ERROR("Failed to unpark dome");
    return IPS_ALERT;
}

bool AlpacaDome::Abort()
{
    nlohmann::json response;
    bool success = retryRequest([this, &response]() {
        std::string path = "/api/v1/dome/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/abortslew";
        return makeAlpacaRequest(path, response, true);
    });

    if (success)
    {
        LOG_INFO("Dome movement aborted.");
        setDomeState(DOME_IDLE);
        return true;
    }

    LOG_ERROR("Failed to abort dome movement");
    return false;
}

bool AlpacaDome::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (isDeviceNameMatch(dev))
    {
        if (ConnectionSettingsNP.isNameMatch(name))
        {
            ConnectionSettingsNP.update(values, names, n);
            ConnectionSettingsNP.setState(IPS_OK);
            ConnectionSettingsNP.apply();
            // Save configuration after update
            saveConfig();
            LOG_INFO("Connection settings updated.");
            return true;
        }
        else if (DeviceNumberNP.isNameMatch(name))
        {
            if (isConnected())
            {
                LOG_WARN("Cannot change device number while connected.");
                return false;
            }
            
            DeviceNumberNP.update(values, names, n);
            DeviceNumberNP.setState(IPS_OK);
            DeviceNumberNP.apply();
            // Save configuration after update
            saveConfig();
            LOG_INFO("Alpaca device number updated.");
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
} 