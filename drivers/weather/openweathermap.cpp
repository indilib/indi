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
    setVersion(1, 0);

    owmLat  = -1000;
    owmLong = -1000;

    setWeatherConnection(CONNECTION_NONE);
}

OpenWeatherMap::~OpenWeatherMap() {}

const char *OpenWeatherMap::getDefaultName()
{
    return (const char *)"OpenWeatherMap";
}

bool OpenWeatherMap::Connect()
{
    if (owmAPIKeyT[0].text == nullptr)
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

    IUFillText(&owmAPIKeyT[0], "API_KEY", "API Key", nullptr);
    IUFillTextVector(&owmAPIKeyTP, owmAPIKeyT, 1, getDeviceName(), "OWM_API_KEY", "OpenWeatherMap", OPTIONS_TAB, IP_RW, 60,
                     IPS_IDLE);

    addParameter("WEATHER_FORECAST", "Weather", 0, 0, 15);
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, 15);
    addParameter("WEATHER_PRESSURE", "Pressure (hPa)", 900, 1100, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity (%)", 0, 100, 15);
    addParameter("WEATHER_WIND_SPEED", "Wind (kph)", 0, 20, 15);
    //    addParameter("WEATHER_WIND_GUST", "Gust (kph)", 0, 20, 15);
    addParameter("WEATHER_RAIN_HOUR", "Precip (mm)", 0, 0, 15);
    addParameter("WEATHER_SNOW_HOUR", "Precip (mm)", 0, 0, 15);
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
        if (!strcmp(owmAPIKeyTP.name, name))
        {
            IUUpdateText(&owmAPIKeyTP, texts, names, n);
            owmAPIKeyTP.s = IPS_OK;
            IDSetText(&owmAPIKeyTP, nullptr);
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
    char requestURL[MAXRBUF];

    // If location is not updated yet, return busy
    if (owmLat == -1000 || owmLong == -1000)
        return IPS_BUSY;

    AutoCNumeric locale;

    snprintf(requestURL, MAXRBUF, "http://api.openweathermap.org/data/2.5/weather?lat=%g&lon=%g&appid=%s&units=metric",
             owmLat, owmLong, owmAPIKeyT[0].text);

    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, requestURL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
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

                        setParameterValue("WEATHER_CODE", id);

                        if (id >= 200 && id < 300)
                        {
                            // Thunderstorm
                            setParameterValue("WEATHER_FORECAST", 2);
                        }
                        else if (id >= 300 && id < 400)
                        {
                            // Drizzle
                            setParameterValue("WEATHER_FORECAST", 2);
                        }
                        else if (id >= 500 && id < 700)
                        {
                            // Rain and snow
                            setParameterValue("WEATHER_FORECAST", 2);
                        }
                        else if (id >= 700 && id < 800)
                        {
                            // Mist and so on
                            setParameterValue("WEATHER_FORECAST", 1);
                        }
                        else if (id == 800)
                        {
                            // Clear!
                            setParameterValue("WEATHER_FORECAST", 0);
                        }
                        else if (id >= 801 && id <= 803)
                        {
                            // Some clouds
                            setParameterValue("WEATHER_FORECAST", 1);
                        }
                        else if (id >= 804 && id < 900)
                        {
                            // Overcast
                            setParameterValue("WEATHER_FORECAST", 2);
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
                if (!strcmp(observationIterator->key, "temp"))
                {
                    if (observationIterator->value.isDouble())
                        setParameterValue("WEATHER_TEMPERATURE", observationIterator->value.toNumber());
                    else
                        setParameterValue("WEATHER_TEMPERATURE", atof(observationIterator->value.toString()));
                }
                else if (!strcmp(observationIterator->key, "pressure"))
                {
                    if (observationIterator->value.isDouble())
                        setParameterValue("WEATHER_PRESSURE", observationIterator->value.toNumber());
                    else
                        setParameterValue("WEATHER_PRESSURE", atof(observationIterator->value.toString()));
                }
                else if (!strcmp(observationIterator->key, "humidity"))
                {
                    if (observationIterator->value.isDouble())
                        setParameterValue("WEATHER_HUMIDITY", observationIterator->value.toNumber());
                    else
                        setParameterValue("WEATHER_HUMIDITY", atof(observationIterator->value.toString()));
                }
            }
        }
        else if (!strcmp(it->key, "wind"))
        {
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                if (!strcmp(observationIterator->key, "speed"))
                {
                    if (observationIterator->value.isDouble())
                        setParameterValue("WEATHER_WIND_SPEED", observationIterator->value.toNumber());
                    else
                        setParameterValue("WEATHER_WIND_SPEED", atof(observationIterator->value.toString()));
                }
            }
        }
        else if (!strcmp(it->key, "clouds"))
        {
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                if (!strcmp(observationIterator->key, "all"))
                {
                    if (observationIterator->value.isDouble())
                        setParameterValue("WEATHER_CLOUD_COVER", observationIterator->value.toNumber());
                    else
                        setParameterValue("WEATHER_CLOUD_COVER", atof(observationIterator->value.toString()));
                }
            }
        }
        else if (!strcmp(it->key, "rain"))
        {
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                if (!strcmp(observationIterator->key, "3h"))
                {
                    if (observationIterator->value.isDouble())
                        setParameterValue("WEATHER_RAIN_HOUR", observationIterator->value.toNumber());
                    else
                        setParameterValue("WEATHER_RAIN_HOUR", atof(observationIterator->value.toString()));
                }
            }
        }
        else if (!strcmp(it->key, "snow"))
        {
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                if (!strcmp(observationIterator->key, "3h"))
                {
                    if (observationIterator->value.isDouble())
                        setParameterValue("WEATHER_SNOW_HOUR", observationIterator->value.toNumber());
                    else
                        setParameterValue("WEATHER_SNOW_HOUR", atof(observationIterator->value.toString()));
                }
            }
        }
    }

    return IPS_OK;
}

bool OpenWeatherMap::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

    IUSaveConfigText(fp, &owmAPIKeyTP);

    return true;
}
