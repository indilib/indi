/*******************************************************************************
  Copyright(c) 2024 WeatherFlow Tempest Weather Driver

  INDI WeatherFlow Tempest Weather Driver

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

#include "weatherflow.h"

#include "locale_compat.h"

#include <sstream>
#include <iomanip>
#include <httplib.h>
#include <limits>
#include <thread>

// We declare an auto pointer to WeatherFlow.
std::unique_ptr<WeatherFlow> weatherFlow(new WeatherFlow());

WeatherFlow::WeatherFlow()
{
    setVersion(1, 0);
    setWeatherConnection(CONNECTION_NONE);

    // Initialize rate limiting
    m_lastRequestTime = std::chrono::system_clock::now();
    m_requestCount = 0;
}

WeatherFlow::~WeatherFlow()
{
}

const char *WeatherFlow::getDefaultName()
{
    return "WeatherFlow";
}

void WeatherFlow::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);
    defineProperty(wfAPIKeyTP);
    defineProperty(wfStationIDTP);
    defineProperty(wfSettingsNP);
}

bool WeatherFlow::Connect()
{
    if (wfAPIKeyTP[0].isEmpty())
    {
        LOG_ERROR("WeatherFlow API Key is not available. Please register your API key at "
                  "https://tempestwx.com/settings/tokens and save it under Options.");
        return false;
    }

    // Check rate limiting
    auto now = std::chrono::system_clock::now();
    auto timeSinceLastRequest = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastRequestTime).count();

    if (timeSinceLastRequest >= RATE_LIMIT_PERIOD)
    {
        m_requestCount = 0;
        m_lastRequestTime = now;
    }

    if (m_requestCount >= RATE_LIMIT_REQUESTS)
    {
        LOG_ERROR("API rate limit exceeded. Please wait before making more requests.");
        return false;
    }

    // Fetch station information if station ID is not provided
    if (wfStationIDTP[0].isEmpty())
    {
        if (!fetchStationInfo())
        {
            LOG_ERROR("Failed to fetch station information. Please provide a station ID manually.");
            return false;
        }
    }
    else
    {
        m_stationID = wfStationIDTP[0].getText();
    }

    m_isConnected = true;
    LOG_INFO("WeatherFlow connection established successfully.");
    return true;
}

bool WeatherFlow::Disconnect()
{
    m_isConnected = false;
    LOG_INFO("WeatherFlow disconnected.");
    return true;
}

bool WeatherFlow::initProperties()
{
    INDI::Weather::initProperties();

    // API Key
    wfAPIKeyTP[0].fill("API_KEY", "API Key", "");
    wfAPIKeyTP.fill(getDeviceName(), "WF_API_KEY", "WeatherFlow", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    wfAPIKeyTP.load();

    // Station ID (optional)
    wfStationIDTP[0].fill("STATION_ID", "Station ID", "");
    wfStationIDTP.fill(getDeviceName(), "WF_STATION_ID", "WeatherFlow", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    wfStationIDTP.load();

    // Settings
    wfSettingsNP[WF_UPDATE_INTERVAL].fill("UPDATE_INTERVAL", "Update Interval (s)", "%.0f", 30, 3600, 30, 60);
    wfSettingsNP[WF_CONNECTION_TIMEOUT].fill("CONNECTION_TIMEOUT", "Connection Timeout (s)", "%.0f", 5, 120, 5, 30);
    wfSettingsNP[WF_RETRY_ATTEMPTS].fill("RETRY_ATTEMPTS", "Retry Attempts", "%.0f", 1, 10, 1, 3);
    wfSettingsNP.fill(getDeviceName(), "WF_SETTINGS", "Settings", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Weather parameters with appropriate ranges for observatory safety
    addParameter("WEATHER_TEMPERATURE", "Temperature (°C)", -40, 50, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity (%)", 0, 100, 15);
    addParameter("WEATHER_PRESSURE", "Pressure (hPa)", 800, 1200, 15);
    addParameter("WEATHER_WIND_SPEED", "Wind Speed (m/s)", 0, 30, 15);
    addParameter("WEATHER_WIND_GUST", "Wind Gust (m/s)", 0, 50, 15);
    addParameter("WEATHER_WIND_DIRECTION", "Wind Direction (degrees)", 0, 360, 15);
    addParameter("WEATHER_RAIN_HOUR", "Rain Accumulation (mm)", 0, 200, 15);
    addParameter("WEATHER_RAIN_RATE", "Rain Rate (mm/hr)", 0, 100, 15);
    addParameter("WEATHER_SOLAR_RADIATION", "Solar Radiation (W/m²)", 0, 1200, 15);
    addParameter("WEATHER_UV", "UV Index", 0, 15, 15);

    // Set critical parameters for observatory safety
    setCriticalParameter("WEATHER_TEMPERATURE");
    setCriticalParameter("WEATHER_HUMIDITY");
    setCriticalParameter("WEATHER_PRESSURE");
    setCriticalParameter("WEATHER_WIND_SPEED");
    setCriticalParameter("WEATHER_WIND_GUST");
    setCriticalParameter("WEATHER_RAIN_HOUR");
    setCriticalParameter("WEATHER_RAIN_RATE");

    addDebugControl();

    return true;
}

bool WeatherFlow::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (wfAPIKeyTP.isNameMatch(name))
        {
            wfAPIKeyTP.update(texts, names, n);
            wfAPIKeyTP.setState(IPS_OK);
            wfAPIKeyTP.apply();
            saveConfig(wfAPIKeyTP);
            return true;
        }

        if (wfStationIDTP.isNameMatch(name))
        {
            wfStationIDTP.update(texts, names, n);
            wfStationIDTP.setState(IPS_OK);
            wfStationIDTP.apply();
            saveConfig(wfStationIDTP);
            return true;
        }
    }

    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

bool WeatherFlow::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (wfSettingsNP.isNameMatch(name))
        {
            wfSettingsNP.update(values, names, n);
            wfSettingsNP.setState(IPS_OK);
            wfSettingsNP.apply();
            saveConfig(wfSettingsNP);
            return true;
        }
    }

    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

bool WeatherFlow::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    // WeatherFlow uses its own station location, so we don't need to update location
    // But we can log it for debugging purposes
    LOGF_DEBUG("Location updated: Lat %.6f, Lon %.6f", latitude, longitude);

    return true;
}

IPState WeatherFlow::updateWeather()
{
    if (!m_isConnected)
    {
        LOG_ERROR("WeatherFlow is not connected.");
        return IPS_ALERT;
    }

    // Check rate limiting
    auto now = std::chrono::system_clock::now();
    auto timeSinceLastRequest = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastRequestTime).count();

    if (timeSinceLastRequest >= RATE_LIMIT_PERIOD)
    {
        m_requestCount = 0;
        m_lastRequestTime = now;
    }

    if (m_requestCount >= RATE_LIMIT_REQUESTS)
    {
        LOG_ERROR("API rate limit exceeded. Please wait before making more requests.");
        return IPS_ALERT;
    }

    // Fetch current weather observations
    if (!fetchCurrentObservations())
    {
        LOG_ERROR("Failed to fetch weather observations.");
        return IPS_ALERT;
    }

    // Update weather parameters with the latest data
    setParameterValue("WEATHER_TEMPERATURE", m_lastData.air_temperature);
    setParameterValue("WEATHER_HUMIDITY", m_lastData.relative_humidity);
    setParameterValue("WEATHER_PRESSURE", m_lastData.barometric_pressure);
    setParameterValue("WEATHER_WIND_SPEED", m_lastData.wind_avg);
    setParameterValue("WEATHER_WIND_GUST", m_lastData.wind_gust);
    setParameterValue("WEATHER_WIND_DIRECTION", m_lastData.wind_direction);
    setParameterValue("WEATHER_RAIN_HOUR", m_lastData.precip_accum_local_day);
    setParameterValue("WEATHER_RAIN_RATE", m_lastData.precip_rate);
    setParameterValue("WEATHER_SOLAR_RADIATION", m_lastData.solar_radiation);
    setParameterValue("WEATHER_UV", m_lastData.uv);

    m_lastUpdate = now;
    LOGF_DEBUG("Weather data updated: Temp=%.1f°C, Humidity=%.1f%%, Wind=%.1fm/s",
               m_lastData.air_temperature, m_lastData.relative_humidity, m_lastData.wind_avg);

    return IPS_OK;
}

bool WeatherFlow::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

    wfAPIKeyTP.save(fp);
    wfStationIDTP.save(fp);
    wfSettingsNP.save(fp);

    return true;
}

bool WeatherFlow::fetchStationInfo()
{
    std::string response;
    std::string endpoint = STATIONS_ENDPOINT + "?token=" + wfAPIKeyTP[0].getText();

    if (!makeAPIRequest(endpoint, response))
    {
        LOG_ERROR("Failed to fetch station information from WeatherFlow API.");
        return false;
    }

    return parseStationResponse(response);
}

bool WeatherFlow::fetchCurrentObservations()
{
    std::string response;
    std::string endpoint;

    if (!m_deviceID.empty())
    {
        // Use device-specific observations if device ID is available
        endpoint = DEVICE_OBSERVATIONS_ENDPOINT + "?device_id=" + m_deviceID + "&token=" + wfAPIKeyTP[0].getText();
    }
    else
    {
        // Use station observations
        endpoint = OBSERVATIONS_ENDPOINT + m_stationID + "?token=" + wfAPIKeyTP[0].getText();
    }

    if (!makeAPIRequest(endpoint, response))
    {
        LOG_ERROR("Failed to fetch current observations from WeatherFlow API.");
        return false;
    }

    return parseObservationsResponse(response);
}

bool WeatherFlow::parseStationResponse(const std::string &response)
{
    try
    {
        json stationData = json::parse(response);

        if (stationData.contains("status") && stationData["status"]["status_code"] == 0)
        {
            if (stationData.contains("stations") && !stationData["stations"].empty())
            {
                auto station = stationData["stations"][0];
                m_stationID = std::to_string(station["station_id"].get<int>());

                if (station.contains("devices") && !station["devices"].empty())
                {
                    auto device = station["devices"][0];
                    m_deviceID = std::to_string(device["device_id"].get<int>());
                }

                LOGF_INFO("Auto-detected station ID: %s, device ID: %s", m_stationID.c_str(), m_deviceID.c_str());
                return true;
            }
        }
        else
        {
            std::string errorMsg = "Unknown error";
            if (stationData.contains("status") && stationData["status"].contains("status_message"))
            {
                errorMsg = stationData["status"]["status_message"].get<std::string>();
            }
            LOGF_ERROR("WeatherFlow API error: %s", errorMsg.c_str());
        }
    }
    catch (json::exception &e)
    {
        LOGF_ERROR("Error parsing station response: %s", e.what());
    }

    return false;
}

bool WeatherFlow::parseObservationsResponse(const std::string &response)
{
    try
    {
        json obsData = json::parse(response);

        if (obsData.contains("status") && obsData["status"]["status_code"] == 0)
        {
            if (obsData.contains("obs") && !obsData["obs"].empty())
            {
                auto observation = obsData["obs"][0];

                // Extract weather data with proper error handling
                auto extractDouble = [&observation](const std::string & key, double defaultValue = 0.0) -> double
                {
                    if (observation.contains(key) && !observation[key].is_null())
                    {
                        return observation[key].get<double>();
                    }
                    return defaultValue;
                };

                m_lastData.air_temperature = extractDouble("air_temperature");
                m_lastData.relative_humidity = extractDouble("relative_humidity");
                m_lastData.barometric_pressure = extractDouble("barometric_pressure");
                m_lastData.wind_avg = extractDouble("wind_avg");
                m_lastData.wind_gust = extractDouble("wind_gust");
                m_lastData.wind_direction = extractDouble("wind_direction");
                m_lastData.precip_accum_local_day = extractDouble("precip_accum_local_day");
                m_lastData.precip_rate = extractDouble("precip_rate");
                m_lastData.solar_radiation = extractDouble("solar_radiation");
                m_lastData.uv = extractDouble("uv");
                m_lastData.timestamp = std::chrono::system_clock::now();

                return true;
            }
        }
        else
        {
            std::string errorMsg = "Unknown error";
            if (obsData.contains("status") && obsData["status"].contains("status_message"))
            {
                errorMsg = obsData["status"]["status_message"].get<std::string>();
            }
            LOGF_ERROR("WeatherFlow API error: %s", errorMsg.c_str());
        }
    }
    catch (json::exception &e)
    {
        LOGF_ERROR("Error parsing observations response: %s", e.what());
    }

    return false;
}

bool WeatherFlow::makeAPIRequest(const std::string &endpoint, std::string &response)
{
    return retryRequest([this, &endpoint, &response]() -> bool
    {
        try
        {
            httplib::Client cli("swd.weatherflow.com", 443);
            cli.set_connection_timeout(static_cast<int>(wfSettingsNP[WF_CONNECTION_TIMEOUT].getValue()));
            cli.set_read_timeout(static_cast<int>(wfSettingsNP[WF_CONNECTION_TIMEOUT].getValue()));

            // Set headers
            httplib::Headers headers;
            headers.emplace("Authorization", "Bearer " + std::string(wfAPIKeyTP[0].getText()));
            headers.emplace("Content-Type", "application/json");
            headers.emplace("User-Agent", "INDI-WeatherFlow/1.0");

            auto result = cli.Get(endpoint, headers);
            
            if (!result)
            {
                LOGF_ERROR("HTTP request failed: %s", 
                          result.error() != httplib::Error::Success ? httplib::to_string(result.error()).c_str() : "Unknown error");
                return false;
            }

            if (result->status != 200)
            {
                LOGF_ERROR("HTTP error: %d", result->status);
                return false;
            }

            response = result->body;

            // Update rate limiting
            m_requestCount++;
            m_lastRequestTime = std::chrono::system_clock::now();

            return true;
        }
        catch (const std::exception &e)
        {
            LOGF_ERROR("Request error: %s", e.what());
            return false;
        }
    });
}

bool WeatherFlow::retryRequest(const std::function < bool() > &request)
{
    int maxRetries = static_cast < int > (wfSettingsNP[WF_RETRY_ATTEMPTS].getValue());

    for (int attempt = 0; attempt <= maxRetries; ++attempt)
    {
        if (request())
        {
            return true;
        }

        if (attempt < maxRetries)
        {
            int delaySeconds = (1 << attempt); // Exponential backoff: 1, 2, 4, 8, 16...
            LOGF_DEBUG("Request failed, retrying in %d seconds (attempt %d/%d)...",
                       delaySeconds, attempt + 1, maxRetries + 1);
            std::this_thread::sleep_for(std::chrono::seconds(delaySeconds));
        }
    }

    LOG_ERROR("All retry attempts failed.");
    return false;
}
