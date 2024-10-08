/*******************************************************************************
  Copyright(c) 2022 Rick Bassham. All rights reserved.

  INDI WeeWx JSON Weather Driver

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

#include "indi_weewx_json.h"
#include "config.h"

#include <curl/curl.h>

#include <memory>
#include <cstring>
#include <string>

struct response_t
{
    char *response;
    size_t size;
};

static size_t cb(void *data, size_t size, size_t nmemb, void *userp)
{
    size_t realsize        = size * nmemb;
    struct response_t *mem = (struct response_t *)userp;

    char *ptr = (char *)realloc(mem->response, mem->size + realsize + 1);
    if (ptr == NULL)
        return 0; /* out of memory! */

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

// We declare an auto pointer to WeewxJSON.
std::unique_ptr<WeewxJSON> weewx_json(new WeewxJSON());

WeewxJSON::WeewxJSON()
{
    setVersion(1, 0);

    setWeatherConnection(CONNECTION_NONE);
}

WeewxJSON::~WeewxJSON() {}

const char *WeewxJSON::getDefaultName()
{
    return (const char *)"WeewxJSON";
}

bool WeewxJSON::Connect()
{
    return true;
}

bool WeewxJSON::Disconnect()
{
    return true;
}

bool WeewxJSON::initProperties()
{
    INDI::Weather::initProperties();

    weewxJsonUrl[WEEWX_URL].fill("WEEWX_URL", "Weewx JSON URL", nullptr);

    weewxJsonUrl.fill(getDeviceName(), "WEEWX_URL", "Weewx", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, 15);
    addParameter("WEATHER_DEW_POINT", "Dew Point (C)", -20, 35, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity %", 0, 100, 15);
    addParameter("WEATHER_HEAT_INDEX", "Heat Index (C)", -20, 35, 15);
    addParameter("WEATHER_BAROMETER", "Barometer (mbar)", 20, 32.5, 15);
    addParameter("WEATHER_WIND_SPEED", "Wind (kph)", 0, 20, 15);
    addParameter("WEATHER_WIND_GUST", "Wind Gust (kph)", 0, 20, 15);
    addParameter("WEATHER_WIND_DIRECTION", "Wind Direction", 0, 360, 15);
    addParameter("WEATHER_WIND_CHILL", "Wind Chill (C)", -20, 35, 15);
    addParameter("WEATHER_RAIN_RATE", "Rain (mm/h)", 0, 0, 15);

    setCriticalParameter("WEATHER_TEMPERATURE");
    setCriticalParameter("WEATHER_WIND_SPEED");
    setCriticalParameter("WEATHER_RAIN_RATE");

    addDebugControl();

    return true;
}

void WeewxJSON::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    static bool once = true;
    if (once)
    {
        once = false;
        defineProperty(weewxJsonUrl);
        loadConfig(true, weewxJsonUrl.getName());
    }
}

bool WeewxJSON::updateProperties()
{
    INDI::Weather::updateProperties();

    if (isConnected())
    {
        defineProperty(weewxJsonUrl);
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(weewxJsonUrl.getName());
    }

    return true;
}

bool WeewxJSON::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (weewxJsonUrl.isNameMatch(name))
        {
            weewxJsonUrl.update(texts, names, n);
            weewxJsonUrl.setState(IPS_OK);
            weewxJsonUrl.apply();
            return true;
        }
    }

    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

void WeewxJSON::handleTemperatureData(json value, std::string key)
{
    double temperatureValue = 0.0;
    std::string units;

    value["value"].get_to(temperatureValue);
    value["units"].get_to(units);

    if (strcmp(units.c_str(), "°F") == 0)
    {
        temperatureValue = (temperatureValue - 32.0) * 5.0 / 9.0;
    }

    setParameterValue(key, temperatureValue);
}

void WeewxJSON::handleRawData(json value, std::string key)
{
    double rawValue = 0.0;

    value["value"].get_to(rawValue);

    setParameterValue(key, rawValue);
}

void WeewxJSON::handleBarometerData(json value, std::string key)
{
    double pressureValue = 0.0;
    std::string units;

    value["value"].get_to(pressureValue);
    value["units"].get_to(units);

    if (strcmp(units.c_str(), "inHg") == 0)
    {
        pressureValue = pressureValue * 33.864;
    }

    setParameterValue(key, pressureValue);
}

void WeewxJSON::handleWindSpeedData(json value, std::string key)
{
    double speedValue = 0.0;
    std::string units;

    value["value"].get_to(speedValue);
    value["units"].get_to(units);

    if (strcmp(units.c_str(), "mph") == 0)
    {
        speedValue = speedValue * 1.609;
    }

    setParameterValue(key, speedValue);
}

void WeewxJSON::handleRainRateData(json value, std::string key)
{
    double rainRate = 0.0;
    std::string units;

    value["value"].get_to(rainRate);
    value["units"].get_to(units);

    if (strcmp(units.c_str(), "in/hr") == 0)
    {
        rainRate = rainRate * 25.4;
    }

    setParameterValue(key, rainRate);
}

void WeewxJSON::handleWeatherData(json value)
{
    if (value.contains("temperature"))
        handleTemperatureData(value["temperature"], "WEATHER_TEMPERATURE");
    if (value.contains("dewpoint"))
        handleTemperatureData(value["dewpoint"], "WEATHER_DEW_POINT");
    if (value.contains("humidity"))
        handleRawData(value["humidity"], "WEATHER_HUMIDITY");
    if (value.contains("heat index"))
        handleTemperatureData(value["heat index"], "WEATHER_HEAT_INDEX");
    if (value.contains("barometer"))
        handleBarometerData(value["barometer"], "WEATHER_BAROMETER");
    if (value.contains("wind speed"))
        handleWindSpeedData(value["wind speed"], "WEATHER_WIND_SPEED");
    if (value.contains("wind gust"))
        handleWindSpeedData(value["wind gust"], "WEATHER_WIND_GUST");
    if (value.contains("wind direction"))
        handleRawData(value["wind direction"], "WEATHER_WIND_DIRECTION");
    if (value.contains("wind chill"))
        handleTemperatureData(value["wind chill"], "WEATHER_WIND_CHILL");
    if (value.contains("rain rate"))
        handleRainRateData(value["rain rate"], "WEATHER_RAIN_RATE");
}

IPState WeewxJSON::updateWeather()
{
    if (isDebug())
        IDLog("%s: updateWeather()\n", getDeviceName());

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl)
    {
        struct response_t chunk = { .response = nullptr, .size = 0 };

        curl_easy_setopt(curl, CURLOPT_URL, weewxJsonUrl[WEEWX_URL].text);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLcode::CURLE_OK)
        {
            nlohmann::json report = nlohmann::json::parse(chunk.response);

            if (report.contains("current"))
            {
                handleWeatherData(report["current"]);
            }
            else
            {
                LOG_ERROR("No current weather data found in report.");
                return IPS_ALERT;
            }

            return IPS_OK;
        }
        else
        {
            LOGF_ERROR("HTTP request to %s failed.", weewxJsonUrl[WEEWX_URL].text);
            return IPS_ALERT;
        }
    }
    else
    {
        LOGF_ERROR("Cannot initialize CURL, connection to HTTP server %s failed.", weewxJsonUrl[WEEWX_URL].text);
        return IPS_ALERT;
    }

    return IPS_OK;
}

bool WeewxJSON::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);
    weewxJsonUrl.save(fp);
    return true;
}
