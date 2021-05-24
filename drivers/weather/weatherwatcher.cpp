/*******************************************************************************
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

  INDI Weather Watcher Driver

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

#include "weatherwatcher.h"
#include "locale_compat.h"

#include <curl/curl.h>

#include <memory>
#include <cstring>

// We declare an auto pointer to WeatherWatcher.
static std::unique_ptr<WeatherWatcher> weatherWatcher(new WeatherWatcher());

static size_t write_data(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

WeatherWatcher::WeatherWatcher()
{
    setVersion(1, 2);

    setWeatherConnection(CONNECTION_NONE);
}

const char *WeatherWatcher::getDefaultName()
{
    return "Weather Watcher";
}

bool WeatherWatcher::Connect()
{
    if (watchFileT[0].text == nullptr || watchFileT[0].text[0] == '\0')
    {
        LOG_ERROR("Watch file must be specified first in options.");
        return false;
    }

    return createPropertiesFromMap();
}

bool WeatherWatcher::Disconnect()
{
    return true;
}

bool WeatherWatcher::createPropertiesFromMap()
{
    // already parsed
    if (initialParse)
        return true;

    if (readWatchFile() == false)
        return false;

    double minOK = 0, maxOK = 0, percWarn = 15;
    for (auto const &x : weatherMap)
    {
        if (x.first == keywordT[0].text)
        {
            minOK = 0;
            maxOK = 0;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_RAIN_HOUR", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_RAIN_HOUR", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_RAIN_HOUR", "PERC_WARN", &percWarn);

            addParameter("WEATHER_RAIN_HOUR", "Rain (mm)", minOK, maxOK, percWarn);
            setCriticalParameter("WEATHER_RAIN_HOUR");
        }
        else if (x.first == keywordT[1].text)
        {
            minOK = -10;
            maxOK = 30;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_TEMPERATURE", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_TEMPERATURE", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_TEMPERATURE", "PERC_WARN", &percWarn);

            addParameter("WEATHER_TEMPERATURE", "Temperature (C)", minOK, maxOK, percWarn);
            setCriticalParameter("WEATHER_TEMPERATURE");
        }
        else if (x.first == keywordT[2].text)
        {
            minOK = 0;
            maxOK = 20;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_SPEED", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_SPEED", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_SPEED", "PERC_WARN", &percWarn);

            addParameter("WEATHER_WIND_SPEED", "Wind (kph)", minOK, maxOK, percWarn);
            setCriticalParameter("WEATHER_WIND_SPEED");
        }
        else if (x.first == keywordT[3].text)
        {
            minOK = 0;
            maxOK = 20;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "PERC_WARN", &percWarn);

            addParameter("WEATHER_WIND_GUST", "Gust (kph)", minOK, maxOK, percWarn);
        }
        else if (x.first == keywordT[4].text)
        {
            minOK = 0;
            maxOK = 20;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_CLOUDS", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_CLOUDS", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_CLOUDS", "PERC_WARN", &percWarn);

            addParameter("WEATHER_CLOUDS", "Clouds (%)", minOK, maxOK, percWarn);
            setCriticalParameter("WEATHER_CLOUDS");
        }
        else if (x.first == keywordT[5].text)
        {
            minOK = 0;
            maxOK = 100;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_HUMIDITY", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_HUMIDITY", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_HUMIDITY", "PERC_WARN", &percWarn);

            addParameter("WEATHER_HUMIDITY", "Humidity (%)", minOK, maxOK, percWarn);
            setCriticalParameter("WEATHER_HUMIDITY");
        }
        else if (x.first == keywordT[6].text)
        {
            minOK = 983;
            maxOK = 1043;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_PRESSURE", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_PRESSURE", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_PRESSURE", "PERC_WARN", &percWarn);

            addParameter("WEATHER_PRESSURE", "Pressure (hPa)", minOK, maxOK, percWarn);
            setCriticalParameter("WEATHER_PRESSURE");
        }
        else if (x.first == keywordT[7].text)
        {
            minOK = 0;
            maxOK = 0;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_FORECAST", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_FORECAST", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_FORECAST", "PERC_WARN", &percWarn);

            addParameter("WEATHER_FORECAST", "Weather", minOK, maxOK, percWarn);
            setCriticalParameter("WEATHER_FORECAST");
        }
    }

    initialParse = true;

    return true;
}

bool WeatherWatcher::initProperties()
{
    INDI::Weather::initProperties();

    IUFillText(&keywordT[0], "RAIN", "Rain", "precip");
    IUFillText(&keywordT[1], "TEMP", "Temperature", "temperature");
    IUFillText(&keywordT[2], "WIND", "Wind", "wind");
    IUFillText(&keywordT[3], "GUST", "Gust", "gust");
    IUFillText(&keywordT[4], "CLOUDS", "Clouds", "clouds");
    IUFillText(&keywordT[5], "HUMIDITY", "Humidity", "humidity");
    IUFillText(&keywordT[6], "PRESSURE", "Pressure", "pressure");
    IUFillText(&keywordT[7], "FORECAST", "Forecast", "forecast");
    IUFillTextVector(&keywordTP, keywordT, 8, getDeviceName(), "KEYWORD", "Keywords", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    IUFillText(&watchFileT[0], "URL", "File", nullptr);
    IUFillTextVector(&watchFileTP, watchFileT, 1, getDeviceName(), "WATCH_SOURCE", "Source", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    IUFillText(&separatorT[0], "SEPARATOR", "Separator", "=");
    IUFillTextVector(&separatorTP, separatorT, 1, getDeviceName(), "SEPARATOR_KEYWORD", "Separator", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    addDebugControl();

    return true;
}

void WeatherWatcher::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    defineProperty(&watchFileTP);
    loadConfig(true, "WATCH_SOURCE");

    defineProperty(&keywordTP);
    loadConfig(true, "KEYWORD");

    defineProperty(&separatorTP);
    loadConfig(true, "SEPARATOR_KEYWORD");

}

bool WeatherWatcher::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(watchFileTP.name, name))
        {
            IUUpdateText(&watchFileTP, texts, names, n);
            watchFileTP.s = IPS_OK;
            IDSetText(&watchFileTP, nullptr);
            return true;
        }
        if (!strcmp(keywordTP.name, name))
        {
            IUUpdateText(&keywordTP, texts, names, n);
            keywordTP.s = IPS_OK;
            IDSetText(&keywordTP, nullptr);
            return true;
        }

        if (!strcmp(separatorTP.name, name))
        {
            IUUpdateText(&separatorTP, texts, names, n);
            separatorTP.s = IPS_OK;
            IDSetText(&separatorTP, nullptr);
            return true;
        }
    }

    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

IPState WeatherWatcher::updateWeather()
{

    if (readWatchFile() == false)
        return IPS_BUSY;

    for (auto const &x : weatherMap)
    {
        if (x.first == keywordT[0].text)
        {
            setParameterValue("WEATHER_RAIN_HOUR", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordT[1].text)
        {
            setParameterValue("WEATHER_TEMPERATURE", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordT[2].text)
        {
            setParameterValue("WEATHER_WIND_SPEED", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordT[3].text)
        {
            setParameterValue("WEATHER_WIND_GUST", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordT[4].text)
        {
            setParameterValue("WEATHER_CLOUDS", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordT[5].text)
        {
            setParameterValue("WEATHER_HUMIDITY", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordT[6].text)
        {
            setParameterValue("WEATHER_PRESSURE", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordT[7].text)
        {
            setParameterValue("WEATHER_FORECAST", std::strtod(x.second.c_str(), nullptr));
        }
    }

    return IPS_OK;
}

bool WeatherWatcher::readWatchFile()
{
    CURL *curl;
    CURLcode res;
    bool rc = false;
    char requestURL[MAXRBUF];

    AutoCNumeric locale;

    if (std::string(watchFileT[0].text).find("http") == 0)
        snprintf(requestURL, MAXRBUF, "%s", watchFileT[0].text);
    else
        snprintf(requestURL, MAXRBUF, "file://%s", watchFileT[0].text);

    curl = curl_easy_init();

    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, requestURL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);

        if (res == CURLE_OK)
        {
            weatherMap = createMap(readBuffer);
            rc = true;
        }
        curl_easy_cleanup(curl);
    }

    return rc;
}

bool WeatherWatcher::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

    IUSaveConfigText(fp, &watchFileTP);
    IUSaveConfigText(fp, &keywordTP);
    IUSaveConfigText(fp, &separatorTP);

    return true;
}

std::map<std::string, std::string> WeatherWatcher::createMap(std::string const &s)
{
    std::map<std::string, std::string> m;

    std::string key, val;
    std::istringstream iss(s);

    while(std::getline(std::getline(iss, key, separatorT[0].text[0]) >> std::ws, val))
        m[key] = val;

    return m;
}
