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
std::unique_ptr<WeatherWatcher> weatherWatcher(new WeatherWatcher());

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
    setVersion(1, 0);

    setWeatherConnection(CONNECTION_NONE);
}

const char *WeatherWatcher::getDefaultName()
{
    return static_cast<const char *>("Weather Watcher");
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

    for (auto const& x : weatherMap)
    {
        if (x.first == tempT[0].text)
        {
            addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, -20, 40);
            setCriticalParameter("WEATHER_TEMPERATURE");
        }
        else if (x.first == windT[0].text)
        {
            addParameter("WEATHER_WIND_SPEED", "Wind (kph)", 0, 20, 0, 40);
            setCriticalParameter("WEATHER_WIND_SPEED");
        }
        else if (x.first == gustT[0].text)
		{
            addParameter("WEATHER_WIND_GUST", "Gust (kph)", 0, 20, 0, 50);
		}        
		else if (x.first == rainT[0].text)
        {
            addParameter("WEATHER_RAIN_HOUR", "Rain (mm)", 0, 0, 0, 0);
            setCriticalParameter("WEATHER_RAIN_HOUR");
        }
        else if (x.first == forecastT[0].text)
        {
            addParameter("WEATHER_FORECAST", "Weather", 0, 0, 0, 1);
            setCriticalParameter("WEATHER_FORECAST");
        }
     }

    initialParse = true;

    return true;
}

bool WeatherWatcher::initProperties()
{
    INDI::Weather::initProperties();
    
	IUFillText(&rainT[0], "Rain", "Keyword", "precip");
    IUFillTextVector(&rainTP, rainT, 1, getDeviceName(), "RAIN_KEYWORD", "Rain", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

	IUFillText(&tempT[0], "Temp", "Keyword", "temperature");
    IUFillTextVector(&tempTP, tempT, 1, getDeviceName(), "TEMP_KEYWORD", "Temp", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

	IUFillText(&windT[0], "Wind", "Keyword", "wind");
    IUFillTextVector(&windTP, windT, 1, getDeviceName(), "WIND_KEYWORD", "Wind", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

	IUFillText(&gustT[0], "Gust", "Keyword", "gust");
    IUFillTextVector(&gustTP, gustT, 1, getDeviceName(), "GUST_KEYWORD", "Gust", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

	IUFillText(&forecastT[0], "Forecast", "Keyword", "forecast");
    IUFillTextVector(&forecastTP, forecastT, 1, getDeviceName(), "FORECAST_KEYWORD", "Forecast", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    IUFillText(&watchFileT[0], "URL", "File", nullptr);
    IUFillTextVector(&watchFileTP, watchFileT, 1, getDeviceName(), "WATCH_SOURCE", "Source", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    IUFillText(&separatorT[0], "Separator", "Keyword", "=");
    IUFillTextVector(&separatorTP, separatorT, 1, getDeviceName(), "SEPARATOR_KEYWORD", "Separator", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

#if 0
    addParameter("WEATHER_FORECAST", "Weather", 0, 0, 0, 1);
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, -20, 40);
    addParameter("WEATHER_WIND_SPEED", "Wind (kph)", 0, 20, 0, 40);
    addParameter("WEATHER_WIND_GUST", "Gust (kph)", 0, 20, 0, 50);
    addParameter("WEATHER_RAIN_HOUR", "Precip (mm)", 0, 0, 0, 0);

    setCriticalParameter("WEATHER_FORECAST");
    setCriticalParameter("WEATHER_TEMPERATURE");
    setCriticalParameter("WEATHER_WIND_SPEED");
    setCriticalParameter("WEATHER_RAIN_HOUR");
#endif

    addDebugControl();

    return true;
}

void WeatherWatcher::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    defineText(&watchFileTP);
    loadConfig(true, "WATCH_SOURCE");
    
	defineText(&rainTP);
    loadConfig(true, "RAIN_KEYWORD");

	defineText(&tempTP);
    loadConfig(true, "TEMP_KEYWORD");

	defineText(&windTP);
    loadConfig(true, "WIND_KEYWORD");

	defineText(&gustTP);
    loadConfig(true, "GUST_KEYWORD");

	defineText(&forecastTP);
    loadConfig(true, "FORECAST_KEYWORD");

	defineText(&separatorTP);
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
        if (!strcmp(rainTP.name, name))
        {
            IUUpdateText(&rainTP, texts, names, n);
            rainTP.s = IPS_OK;
            IDSetText(&rainTP, nullptr);
            return true;
        }

        if (!strcmp(tempTP.name, name))
        {
            IUUpdateText(&tempTP, texts, names, n);
            tempTP.s = IPS_OK;
            IDSetText(&tempTP, nullptr);
            return true;
        }

        if (!strcmp(windTP.name, name))
        {
            IUUpdateText(&windTP, texts, names, n);
            windTP.s = IPS_OK;
            IDSetText(&windTP, nullptr);
            return true;
        }

        if (!strcmp(gustTP.name, name))
        {
            IUUpdateText(&gustTP, texts, names, n);
            gustTP.s = IPS_OK;
            IDSetText(&gustTP, nullptr);
            return true;
        }

        if (!strcmp(forecastTP.name, name))
        {
            IUUpdateText(&forecastTP, texts, names, n);
            forecastTP.s = IPS_OK;
            IDSetText(&forecastTP, nullptr);
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

    for (auto const& x : weatherMap)
    {
        if (x.first == tempT[0].text)
        {
            setParameterValue("WEATHER_TEMPERATURE", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == windT[0].text)
        {
            setParameterValue("WEATHER_WIND_SPEED", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == gustT[0].text){

            setParameterValue("WEATHER_WIND_GUST", std::strtod(x.second.c_str(), nullptr));
        }
		else if (x.first == rainT[0].text)
        {
            setParameterValue("WEATHER_RAIN_HOUR", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == forecastT[0].text)
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
            return true;
        }
        curl_easy_cleanup(curl);
    }

    return rc;
}

bool WeatherWatcher::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

    IUSaveConfigText(fp, &watchFileTP);
    IUSaveConfigText(fp, &rainTP);
    IUSaveConfigText(fp, &tempTP);
    IUSaveConfigText(fp, &windTP);
    IUSaveConfigText(fp, &gustTP);
    IUSaveConfigText(fp, &forecastTP);
    IUSaveConfigText(fp, &separatorTP);

    return true;
}

std::map<std::string, std::string> WeatherWatcher::createMap(std::string const& s)
{
    std::map<std::string, std::string> m;

    std::string key, val;
    std::istringstream iss(s);

    while(std::getline(std::getline(iss, key, '=') >> std::ws, val))
        m[key] = val;

    return m;
}
