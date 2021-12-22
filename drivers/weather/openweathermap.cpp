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

#include "gason.h"
#include "locale_compat.h"

#include <curl/curl.h>

#include <memory>
#include <cstring>

// We declare an auto pointer to OpenWeatherMap.
std::unique_ptr<OpenWeatherMap> openWeatherMap(new OpenWeatherMap());

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

OpenWeatherMap::OpenWeatherMap()
{
    setVersion(1, 1);

    owmLat  = -1000;
    owmLong = -1000;

    setWeatherConnection(CONNECTION_NONE);
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

OpenWeatherMap::~OpenWeatherMap()
{
    curl_global_cleanup();
}

const char *OpenWeatherMap::getDefaultName()
{
    return (const char *)"OpenWeatherMap";
}

bool OpenWeatherMap::Connect()
{
    if (strlen(owmAPIKeyTP[0].getText()) == 0)
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

    owmAPIKeyTP[0].fill("API_KEY", "API Key", nullptr);
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

    addDebugControl();

    return true;
}

void OpenWeatherMap::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    static bool once = true;

    if (once)
    {
        once = false;
        defineProperty(&owmAPIKeyTP);

        loadConfig(true, "OWM_API_KEY");
    }
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
    CURL *curl;
    CURLcode res;
    std::string readBuffer;
    char errorBuffer[CURL_ERROR_SIZE];
    char requestURL[MAXRBUF];

    // If location is not updated yet, return busy
    if (owmLat == -1000 || owmLong == -1000)
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

    char srcBuffer[readBuffer.size()];
    strncpy(srcBuffer, readBuffer.c_str(), readBuffer.size());
    char *source = srcBuffer;
    // do not forget terminate source string with 0
    char *endptr;
    JsonValue value;
    JsonAllocator allocator;
    int status = jsonParse(source, &endptr, &value, allocator);
    if (status != JSON_OK)
    {
        LOGF_ERROR("%s at %zd", jsonStrError(status), endptr - source);
        LOGF_DEBUG("%s", requestURL);
        LOGF_DEBUG("%s", readBuffer.c_str());
        return IPS_ALERT;
    }

    JsonIterator it;
    JsonIterator observationIterator;

    double forecast = 0;
    double temperature = 0;
    double pressure = 0;
    double humidity = 0;
    double wind = 0;
    double rain = 0;
    double snow = 0;
    double clouds = 0;
    int code = 0;

    auto checkKey = [&observationIterator](const char* key, double & value) -> bool
    {
        if(!strcmp(observationIterator->key, key))
        {
            if (observationIterator->value.isDouble())
                value = observationIterator->value.toNumber();
            else
                value = atof(observationIterator->value.toString());
            return true;
        }
        return false;
    };

    auto checkWildCardKey = [&observationIterator](const char* key, double & value) -> bool
    {
        if(strstr(observationIterator->key, key))
        {
            if (observationIterator->value.isDouble())
                value = observationIterator->value.toNumber();
            else
                value = atof(observationIterator->value.toString());
            return true;
        }
        return false;
    };

    for (it = begin(value); it != end(value); ++it)
    {
        if (!strcmp(it->key, "weather"))
        {
            // This is an array of weather conditions
            for (auto i : it->value)
            {
                for (observationIterator = begin(i->value); observationIterator != end(i->value); ++observationIterator)
                {
                    if (!strcmp(observationIterator->key, "id"))
                    {
                        int id;
                        if (observationIterator->value.isDouble())
                            id = observationIterator->value.toNumber();
                        else
                            id = atoi(observationIterator->value.toString());

                        code = id;

                        if (id >= 200 && id < 300)
                        {
                            // Thunderstorm
                            forecast = 2;
                        }
                        else if (id >= 300 && id < 400)
                        {
                            // Drizzle
                            forecast = 2;
                        }
                        else if (id >= 500 && id < 700)
                        {
                            // Rain and snow
                            forecast = 2;
                        }
                        else if (id >= 700 && id < 800)
                        {
                            // Mist and so on
                            forecast = 1;
                        }
                        else if (id == 800)
                        {
                            // Clear!
                            forecast = 0;
                        }
                        else if (id >= 801 && id <= 803)
                        {
                            // Some clouds
                            forecast = 1;
                        }
                        else if (id >= 804 && id < 900)
                        {
                            // Overcast
                            forecast = 2;
                        }
                    }
                }
            }
        }
        else if (!strcmp(it->key, "main"))
        {
            // Temperature, pressure, humidity
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                if(checkKey("temp", temperature)) continue;
                if(checkKey("pressure", pressure)) continue;
                checkKey("humidity", humidity);
            }
        }
        else if (!strcmp(it->key, "wind"))
        {
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                checkKey("speed", wind);
            }
        }
        else if (!strcmp(it->key, "clouds"))
        {
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                checkKey("all", clouds);
            }
        }
        else if (!strcmp(it->key, "rain"))
        {
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                checkWildCardKey("h", rain);
            }
        }
        else if (!strcmp(it->key, "snow"))
        {
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                checkWildCardKey("h", snow);
            }
        }
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

    IUSaveConfigText(fp, &owmAPIKeyTP);

    return true;
}
