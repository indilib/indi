/*******************************************************************************
  Copyright(c) 2025 Jérémie Klein. All rights reserved.

  ASCOM Alpaca Weather Safety INDI Driver

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

#include "weather_safety_alpaca.h"

#include <httplib.h>
#include <memory>
#include <chrono>
#include <thread>

// We declare an auto pointer to WeatherSafetyAlpaca
std::unique_ptr<WeatherSafetyAlpaca> weather_safety_alpaca(new WeatherSafetyAlpaca());

WeatherSafetyAlpaca::WeatherSafetyAlpaca()
{
    setVersion(1, 0);
    setWeatherConnection(CONNECTION_NONE);
}

const char *WeatherSafetyAlpaca::getDefaultName()
{
    return "Weather Safety Alpaca";
}

bool WeatherSafetyAlpaca::initProperties()
{
    INDI::Weather::initProperties();

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

    addParameter("WEATHER_SAFETY", "Weather Safety", 0, 1, 0);  // min=0 (warning), max=1 (safe)
    setCriticalParameter("WEATHER_SAFETY");

    // Load config before setting any defaults
    loadConfig(true);

    addDebugControl();

    return true;
}

void WeatherSafetyAlpaca::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    // Always define these properties
    defineProperty(ServerAddressTP);
    defineProperty(DeviceNumberNP);
    defineProperty(ConnectionSettingsNP);
}

bool WeatherSafetyAlpaca::Connect()
{
    if (ServerAddressTP[0].getText() == nullptr || ServerAddressTP[1].getText() == nullptr)
    {
        LOG_ERROR("Server address or port is not set.");
        return false;
    }

    // Test connection by getting safety monitor status
    nlohmann::json response;
    bool success = retryRequest([this, &response]() {
        std::string path = "/api/v1/safetymonitor/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/issafe";
        return makeAlpacaRequest(path, response);
    });

    if (!success)
    {
        LOG_ERROR("Failed to connect to Alpaca safety monitor. Please check server address and port.");
        return false;
    }

    LOG_INFO("Successfully connected to Alpaca safety monitor.");
    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool WeatherSafetyAlpaca::Disconnect()
{
    LOG_INFO("Disconnected from Alpaca safety monitor.");
    return true;
}

bool WeatherSafetyAlpaca::updateProperties()
{
    INDI::Weather::updateProperties();

    return true;
}

IPState WeatherSafetyAlpaca::updateWeather()
{
    LOGF_DEBUG("Updating weather status for device %d", 
              static_cast<int>(DeviceNumberNP[0].getValue()));

    nlohmann::json response;
    bool success = retryRequest([this, &response]() {
        std::string path = "/api/v1/safetymonitor/" + std::to_string(static_cast<int>(DeviceNumberNP[0].getValue())) + "/issafe";
        LOGF_DEBUG("Requesting safety status with path: %s", path.c_str());
        return makeAlpacaRequest(path, response);
    });

    if (!success)
    {
        LOG_DEBUG("Safety status request failed");
        LastParseSuccess = false;
        return IPS_ALERT;
    }

    try
    {
        LOGF_DEBUG("Full Alpaca response: %s", response.dump(2).c_str());

        bool isSafe = response["Value"].get<bool>();
        LOGF_DEBUG("Parsed safety status: %s", isSafe ? "SAFE" : "UNSAFE");
        
        setParameterValue("WEATHER_SAFETY", isSafe ? 0 : 2);  // 0 for safe, 1 for warning, 2 for danger
        
        LastParseSuccess = true;
        return IPS_OK;
    }
    catch (nlohmann::json::exception &e)
    {
        LOGF_ERROR("JSON parsing error: %s (id: %d)", e.what(), e.id);
        LastParseSuccess = false;
        return IPS_ALERT;
    }
}

bool WeatherSafetyAlpaca::makeAlpacaRequest(const std::string& path, nlohmann::json& response, bool isPut)
{
    if (ServerAddressTP[0].getText() == nullptr || ServerAddressTP[1].getText() == nullptr)
    {
        LOG_ERROR("Server address or port is not set.");
        return false;
    }

    try
    {
        // Log connection details
        LOGF_DEBUG("Creating HTTP client for host: %s, port: %s", 
                  ServerAddressTP[0].getText(), 
                  ServerAddressTP[1].getText());

        httplib::Client cli(ServerAddressTP[0].getText(), std::stoi(ServerAddressTP[1].getText()));
        
        // Log timeout settings
        LOGF_DEBUG("Setting timeouts - Connection: %d sec, Read: %d sec", 
                  static_cast<int>(ConnectionSettingsNP[0].getValue()),
                  static_cast<int>(ConnectionSettingsNP[0].getValue()));

        cli.set_connection_timeout(static_cast<int>(ConnectionSettingsNP[0].getValue()));
        cli.set_read_timeout(static_cast<int>(ConnectionSettingsNP[0].getValue()));

        // Log request details
        LOGF_DEBUG("Making %s request to path: %s", 
                  isPut ? "PUT" : "GET", 
                  path.c_str());

        auto result = isPut ? cli.Put(path.c_str()) : cli.Get(path.c_str());
        
        if (!result)
        {
            LOGF_ERROR("HTTP request failed: %s", 
                      result.error() != httplib::Error::Success ? httplib::to_string(result.error()).c_str() : "Unknown error");
            return false;
        }

        // Log response status and headers
        LOGF_DEBUG("HTTP Status: %d", result->status);
        for (const auto& header : result->headers)
        {
            LOGF_DEBUG("Response Header - %s: %s", 
                      header.first.c_str(), 
                      header.second.c_str());
        }

        // Log response body
        LOGF_DEBUG("Response Body: %s", result->body.c_str());

        if (result->status != 200)
        {
            LOGF_ERROR("HTTP error: %d", result->status);
            return false;
        }

        response = nlohmann::json::parse(result->body);
        
        // Log parsed JSON response
        LOGF_DEBUG("Parsed JSON response: %s", response.dump(2).c_str());

        if (response["ErrorNumber"].get<int>() != 0)
        {
            LOGF_ERROR("Alpaca error: %s (ErrorNumber: %d)", 
                      response["ErrorMessage"].get<std::string>().c_str(),
                      response["ErrorNumber"].get<int>());
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

bool WeatherSafetyAlpaca::retryRequest(const std::function<bool()>& request)
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
            LOGF_INFO("Retrying request in %d ms (attempt %d/%d)", 
                      retryDelay, attempt, maxRetries);
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
        }
    }
    return false;
}

bool WeatherSafetyAlpaca::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (isDeviceNameMatch(dev) && ServerAddressTP.isNameMatch(name))
    {
        ServerAddressTP.update(texts, names, n);
        ServerAddressTP.setState(IPS_OK);
        ServerAddressTP.apply();
        saveConfig();
        return true;
    }

    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

bool WeatherSafetyAlpaca::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (isDeviceNameMatch(dev))
    {
        if (ConnectionSettingsNP.isNameMatch(name))
        {
            ConnectionSettingsNP.update(values, names, n);
            ConnectionSettingsNP.setState(IPS_OK);
            ConnectionSettingsNP.apply();
            saveConfig();
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
            saveConfig();
            return true;
        }
    }

    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

bool WeatherSafetyAlpaca::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);
    
    ServerAddressTP.save(fp);
    DeviceNumberNP.save(fp);
    ConnectionSettingsNP.save(fp);
    
    return true;
}

bool WeatherSafetyAlpaca::loadConfig(bool silent, const char *property)
{
    bool result = INDI::Weather::loadConfig(silent, property);

    if (property == nullptr)
    {
        result &= ServerAddressTP.load();
        result &= DeviceNumberNP.load();
        result &= ConnectionSettingsNP.load();
    }
    
    return result;
} 