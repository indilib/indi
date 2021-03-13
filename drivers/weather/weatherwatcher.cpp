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

void ISGetProperties(const char *dev)
{
    weatherWatcher->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    weatherWatcher->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    weatherWatcher->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    weatherWatcher->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}
void ISSnoopDevice(XMLEle *root)
{
    weatherWatcher->ISSnoopDevice(root);
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
    if (watchFileTP[0].text == nullptr || watchFileTP[0].text[0] == '\0')
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
        if (x.first == keywordTP[0].getText())
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
        else if (x.first == keywordTP[1].getText())
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
        else if (x.first == keywordTP[2].getText())
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
        else if (x.first == keywordTP[3].getText())
        {
            minOK = 0;
            maxOK = 20;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "PERC_WARN", &percWarn);

            addParameter("WEATHER_WIND_GUST", "Gust (kph)", minOK, maxOK, percWarn);
        }
        else if (x.first == keywordTP[4].getText())
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
        else if (x.first == keywordTP[5].getText())
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
        else if (x.first == keywordTP[6].getText())
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
        else if (x.first == keywordTP[7].getText())
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

    keywordTP[0].fill("RAIN", "Rain", "precip");
    keywordTP[1].fill("TEMP", "Temperature", "temperature");
    keywordTP[2].fill("WIND", "Wind", "wind");
    keywordTP[3].fill("GUST", "Gust", "gust");
    keywordTP[4].fill("CLOUDS", "Clouds", "clouds");
    keywordTP[5].fill("HUMIDITY", "Humidity", "humidity");
    keywordTP[6].fill("PRESSURE", "Pressure", "pressure");
    keywordTP[7].fill("FORECAST", "Forecast", "forecast");
    keywordTP.fill(getDeviceName(), "KEYWORD", "Keywords", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    watchFileTP[0].fill("URL", "File", nullptr);
    watchFileTP.fill(getDeviceName(), "WATCH_SOURCE", "Source", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    separatorTP[0].fill("SEPARATOR", "Separator", "=");
    separatorTP.fill(getDeviceName(), "SEPARATOR_KEYWORD", "Separator", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    addDebugControl();

    return true;
}

void WeatherWatcher::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    defineProperty(watchFileTP);
    loadConfig(true, "WATCH_SOURCE");

    defineProperty(keywordTP);
    loadConfig(true, "KEYWORD");

    defineProperty(separatorTP);
    loadConfig(true, "SEPARATOR_KEYWORD");

}

bool WeatherWatcher::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (watchFileTP.isNameMatch(name))
        {
            watchFileTP.update(texts, names, n);
            watchFileTP.setState(IPS_OK);
            watchFileTP.apply();
            return true;
        }
        if (keywordTP.isNameMatch(name))
        {
            keywordTP.update(texts, names, n);
            keywordTP.setState(IPS_OK);
            keywordTP.apply();
            return true;
        }

        if (separatorTP.isNameMatch(name))
        {
            separatorTP.update(texts, names, n);
            separatorTP.setState(IPS_OK);
            separatorTP.apply();
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
        if (x.first == keywordTP[0].getText())
        {
            setParameterValue("WEATHER_RAIN_HOUR", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[1].getText())
        {
            setParameterValue("WEATHER_TEMPERATURE", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[2].getText())
        {
            setParameterValue("WEATHER_WIND_SPEED", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[3].getText())
        {
            setParameterValue("WEATHER_WIND_GUST", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[4].getText())
        {
            setParameterValue("WEATHER_CLOUDS", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[5].getText())
        {
            setParameterValue("WEATHER_HUMIDITY", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[6].getText())
        {
            setParameterValue("WEATHER_PRESSURE", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[7].getText())
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

    if (std::string(watchFileTP[0].getText()).find("http") == 0)
        snprintf(requestURL, MAXRBUF, "%s", watchFileTP[0].getText());
    else
        snprintf(requestURL, MAXRBUF, "file://%s", watchFileTP[0].getText());

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

    watchFileTP.save(fp);
    keywordTP.save(fp);
    separatorTP.save(fp);

    return true;
}

std::map<std::string, std::string> WeatherWatcher::createMap(std::string const &s)
{
    std::map<std::string, std::string> m;

    std::string key, val;
    std::istringstream iss(s);

    while(std::getline(std::getline(iss, key, separatorTP[0].text[0]) >> std::ws, val))
        m[key] = val;

    return m;
}
