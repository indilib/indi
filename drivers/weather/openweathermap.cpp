/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI Weather Underground (TM) Weather Driver

  Modified for OpenWeatherMap API by Jarno Paananen

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

#include "openweathermap.h"

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

#include "locale_compat.h"

#include <curl/curl.h>

#include <memory>
#include <cstring>
#include <limits> // Needed for std::numeric_limits

using json = nlohmann::json;

// We declare an auto pointer to OpenWeatherMap.
std::unique_ptr<OpenWeatherMap> openWeatherMap(new OpenWeatherMap());

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

OpenWeatherMap::OpenWeatherMap()
{
    setVersion(1, 2);

    owmLat  = std::numeric_limits<double>::quiet_NaN();
    owmLong = std::numeric_limits<double>::quiet_NaN();
    previousForecast = std::numeric_limits<double>::quiet_NaN(); // Initialize previous forecast

    setWeatherConnection(CONNECTION_NONE);
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

OpenWeatherMap::~OpenWeatherMap()
{
    curl_global_cleanup();
}

const char *OpenWeatherMap::getDefaultName()
{
    return "OpenWeatherMap";
}

void OpenWeatherMap::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);
    defineProperty(owmAPIKeyTP);
}

bool OpenWeatherMap::Connect()
{
    if (owmAPIKeyTP[0].isEmpty())
    {
        LOG_ERROR("OpenWeatherMap API Key is not available. Please register your API key at "
                  "www.openweathermap.org and save it under Options.");
        return false;
    }

    return true;
}

bool OpenWeatherMap::Disconnect()
{
    return true;
}

bool OpenWeatherMap::initProperties()
{
    INDI::Weather::initProperties();

    char api_key[256] = {0};
    IUGetConfigText(getDeviceName(), "OWM_API_KEY", "API_KEY", api_key, 256);
    owmAPIKeyTP[0].fill("API_KEY", "API Key", api_key);
    owmAPIKeyTP.fill(getDeviceName(), "OWM_API_KEY", "OpenWeatherMap", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    addParameter("WEATHER_FORECAST", "Weather", 0, 0, 15);
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, 15);
    addParameter("WEATHER_PRESSURE", "Pressure (hPa)", 900, 1100, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity (%)", 0, 100, 15);
    addParameter("WEATHER_WIND_SPEED", "Wind (m/s)", 0, 20, 15);
    addParameter("WEATHER_RAIN_HOUR", "Rain precip (mm)", 0, 0, 15);
    addParameter("WEATHER_SNOW_HOUR", "Snow precip (mm)", 0, 0, 15);
    addParameter("WEATHER_CLOUD_COVER", "Clouds (%)", 0, 100, 15);
    addParameter("WEATHER_CODE", "Status code", 200, 810, 15);

    setCriticalParameter("WEATHER_FORECAST");
    setCriticalParameter("WEATHER_TEMPERATURE");
    setCriticalParameter("WEATHER_WIND_SPEED");
    setCriticalParameter("WEATHER_RAIN_HOUR");
    setCriticalParameter("WEATHER_SNOW_HOUR");

    updateLocation(LocationNP[LOCATION_LATITUDE].getValue(), LocationNP[LOCATION_LONGITUDE].getValue(), LocationNP[LOCATION_ELEVATION].getValue());
    addDebugControl();
    return true;
}

bool OpenWeatherMap::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (owmAPIKeyTP.isNameMatch(name))
        {
            owmAPIKeyTP.update(texts, names, n);
            owmAPIKeyTP.setState(IPS_OK);
            owmAPIKeyTP.apply();
            saveConfig(true, owmAPIKeyTP.getName());
            return true;
        }
    }

    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

bool OpenWeatherMap::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    owmLat  = latitude;
    owmLong = (longitude > 180) ? (longitude - 360) : longitude;

    return true;
}

IPState OpenWeatherMap::updateWeather()
{
    CURL *curl {nullptr};
    CURLcode res;
    std::string readBuffer;
    char errorBuffer[CURL_ERROR_SIZE] = {0};
    char requestURL[MAXRBUF] = {0};

    // If location is not updated yet, return busy
    if (std::isnan(owmLat) || std::isnan(owmLong))
        return IPS_BUSY;

    AutoCNumeric locale;

    snprintf(requestURL, MAXRBUF, "http://api.openweathermap.org/data/2.5/weather?lat=%g&lon=%g&appid=%s&units=metric",
             owmLat, owmLong, owmAPIKeyTP[0].getText());

    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, requestURL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        errorBuffer[0] = 0;
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if(res != CURLE_OK)
        {
            if(strlen(errorBuffer))
            {
                LOGF_ERROR("Error %d reading data: %s", res, errorBuffer);
            }
            else
            {
                LOGF_ERROR("Error %d reading data: %s", res, curl_easy_strerror(res));
            }
            return IPS_ALERT;
        }
    }

    double forecast = 0;
    double temperature = 0;
    double pressure = 0;
    double humidity = 0;
    double wind = 0;
    double rain = 0;
    double snow = 0;
    double clouds = 0;
    int code = 0;
    std::string description; // To store weather description

    try
    {
        json weatherReport = json::parse(readBuffer);

        weatherReport["weather"][0]["id"].get_to(code);
        weatherReport["weather"][0]["description"].get_to(description); // Get description

        if (code >= 200 && code < 300)
        {
            // Thunderstorm
            forecast = 2;
        }
        else if (code >= 300 && code < 400)
        {
            // Drizzle
            forecast = 2;
        }
        else if (code >= 500 && code < 700)
        {
            // Rain and snow
            forecast = 2;
        }
        else if (code >= 700 && code < 800)
        {
            // Mist and so on
            forecast = 1;
        }
        else if (code == 800)
        {
            // Clear!
            forecast = 0;
        }
        else if (code >= 801 && code <= 803)
        {
            // Some clouds
            forecast = 1;
        }
        else if (code >= 804 && code < 900)
        {
            // Overcast
            forecast = 2;
        }

        // Temperature
        weatherReport["main"]["temp"].get_to(temperature);
        // Pressure
        weatherReport["main"]["pressure"].get_to(pressure);
        // Humidity
        weatherReport["main"]["humidity"].get_to(humidity);
        // Wind
        weatherReport["wind"]["speed"].get_to(wind);
        // Cloud
        weatherReport["clouds"]["all"].get_to(clouds);
        // Rain (does not exist in all reports)
        if (weatherReport.contains("rain"))
        {
            if (weatherReport["rain"].contains("h"))
            {
                weatherReport["rain"]["h"].get_to(rain);
            }
            else if (weatherReport["rain"].contains("1h"))
            {
                weatherReport["rain"]["1h"].get_to(rain);
            }
        }
        // Snow (does not exist in all reports)
        if (weatherReport.contains("snow"))
        {
            if (weatherReport["snow"].contains("h"))
            {
                weatherReport["snow"]["h"].get_to(snow);
            }
            else if (weatherReport["snow"].contains("1h"))
            {
                weatherReport["snow"]["1h"].get_to(snow);
            }
        }

    }
    catch (json::exception &e)
    {
        // output exception information
        LOGF_ERROR("Error parsing weather report %s id: %d", e.what(), e.id);
        return IPS_ALERT;
    }

    // Log forecast change if it differs from the previous value
    if (std::isnan(previousForecast) || forecast != previousForecast)
    {
        LOGF_INFO("Forecast changed: %s (Code: %d)", description.c_str(), code);
        previousForecast = forecast; // Update previous forecast
    }

    setParameterValue("WEATHER_FORECAST", forecast);
    setParameterValue("WEATHER_TEMPERATURE", temperature);
    setParameterValue("WEATHER_PRESSURE", pressure);
    setParameterValue("WEATHER_HUMIDITY", humidity);
    setParameterValue("WEATHER_WIND_SPEED", wind);
    setParameterValue("WEATHER_RAIN_HOUR", rain);
    setParameterValue("WEATHER_SNOW_HOUR", snow);
    setParameterValue("WEATHER_CLOUD_COVER", clouds);
    setParameterValue("WEATHER_CODE", code);
    return IPS_OK;
}

bool OpenWeatherMap::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

    owmAPIKeyTP.save(fp);

    return true;
}
