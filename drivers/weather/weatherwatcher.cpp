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

const char *LABELS_TAB  = "Labels";
const char *PARAMETERS_TAB = "Parameters";

// We declare an auto pointer to WeatherWatcher.
static std::unique_ptr<WeatherWatcher> weatherWatcher(new WeatherWatcher());

static size_t write_data(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

WeatherWatcher::WeatherWatcher()
{
    setVersion(2, 1);
    setWeatherConnection(CONNECTION_NONE);
}

const char *WeatherWatcher::getDefaultName()
{
    return "Weather Watcher";
}

bool WeatherWatcher::Connect()
{
    if (watchFileTP[0].isEmpty() || watchFileTP[0].getText()[0] == '\0')
    {
        LOG_ERROR("Watch file must be specified first in options.");
        return false;
    }
    else
        return true;
}

bool WeatherWatcher::Disconnect()
{
    return true;
}

bool WeatherWatcher::createProperties()
{
    if (readWatchFile() == false)
        return false;
    double minOK = 0, maxOK = 0, percWarn = 15;
    for (auto const &x : weatherMap)
    {
        if (x.first == keywordTP[WEATHER_RAIN].getText())
        {
            // Caution: If both OK=0 the parameter never (re)appears in UI! (-> WeatherInterface::addParameter())
            minOK = -1;
            maxOK = 0;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_RAIN_HOUR", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_RAIN_HOUR", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_RAIN_HOUR", "PERC_WARN", &percWarn);

            addParameter("WEATHER_RAIN_HOUR",  labelTP[WEATHER_RAIN].text, minOK, maxOK, percWarn);
            if (criticalSP[WEATHER_RAIN].getState())
                setCriticalParameter("WEATHER_RAIN_HOUR");
        }
        else if (x.first == keywordTP[WEATHER_TEMP].getText())
        {
            minOK = -10;
            maxOK = 30;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_TEMPERATURE", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_TEMPERATURE", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_TEMPERATURE", "PERC_WARN", &percWarn);

            addParameter("WEATHER_TEMPERATURE", labelTP[WEATHER_TEMP].text, minOK, maxOK, percWarn);
            if (criticalSP[WEATHER_TEMP].getState())
                setCriticalParameter("WEATHER_TEMPERATURE");
        }
        else if (x.first == keywordTP[WEATHER_WIND].getText())
        {
            minOK = 0;
            maxOK = 20;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_SPEED", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_SPEED", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_SPEED", "PERC_WARN", &percWarn);

            addParameter("WEATHER_WIND_SPEED", labelTP[WEATHER_WIND].text, minOK, maxOK, percWarn);
            if (criticalSP[WEATHER_WIND].getState())
                setCriticalParameter("WEATHER_WIND_SPEED");
        }
        else if (x.first == keywordTP[WEATHER_GUST].getText())
        {
            minOK = 0;
            maxOK = 20;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "PERC_WARN", &percWarn);

            addParameter("WEATHER_WIND_GUST", labelTP[WEATHER_GUST].text, minOK, maxOK, percWarn);
            if (criticalSP[WEATHER_GUST].getState())
                setCriticalParameter("WEATHER_WIND_GUST");
        }
        else if (x.first == keywordTP[WEATHER_CLOUD].getText())
        {
            minOK = 0;
            maxOK = 20;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_CLOUDS", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_CLOUDS", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_CLOUDS", "PERC_WARN", &percWarn);

            addParameter("WEATHER_CLOUDS",  labelTP[WEATHER_CLOUD].text, minOK, maxOK, percWarn, true);
            if (criticalSP[WEATHER_CLOUD].getState())
                setCriticalParameter("WEATHER_CLOUDS");
        }
        else if (x.first == keywordTP[WEATHER_HUM].getText())
        {
            minOK = 0;
            maxOK = 100;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_HUMIDITY", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_HUMIDITY", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_HUMIDITY", "PERC_WARN", &percWarn);

            addParameter("WEATHER_HUMIDITY", labelTP[WEATHER_HUM].text, minOK, maxOK, percWarn);
            if (criticalSP[WEATHER_HUM].getState())
                setCriticalParameter("WEATHER_HUMIDITY");
        }
        else if (x.first == keywordTP[WEATHER_PRESS].getText())
        {
            minOK = 983;
            maxOK = 1043;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_PRESSURE", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_PRESSURE", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_PRESSURE", "PERC_WARN", &percWarn);

            addParameter("WEATHER_PRESSURE", labelTP[WEATHER_PRESS].text, minOK, maxOK, percWarn);
            if (criticalSP[WEATHER_PRESS].getState())
                setCriticalParameter("WEATHER_PRESSURE");
        }
        else if (x.first == keywordTP[WEATHER_FORECAST].getText())
        {
            minOK = -1;
            maxOK = 0;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_FORECAST", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_FORECAST", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_FORECAST", "PERC_WARN", &percWarn);

            addParameter("WEATHER_FORECAST", labelTP[WEATHER_FORECAST].text, minOK, maxOK, percWarn);
            if (criticalSP[WEATHER_FORECAST].getState())
                setCriticalParameter("WEATHER_FORECAST");
        }
    }
    return true;
}

// initialize properties on launching indiserver
bool WeatherWatcher::initProperties()
{
    INDI::Weather::initProperties();
    addDebugControl();
    return true;
}

// initialize properties after connection/disconnection
bool WeatherWatcher::updateProperties()
{
    if (isConnected())
    {
        getProperties();
        createProperties();
        INDI::Weather::updateProperties(); // define inherited properties
    }
    else
    {
        deleteProperty(keywordTP);
        deleteProperty(criticalSP);
        // call deliberatly here to prevent reorder of fields in indicontrol interface
        INDI::Weather::updateProperties(); // delete inherited properties
        // deleteProperty() does not reset widgets array to 0!! So we do it manually:
        INDI::Weather::critialParametersLP.resize(0);
        for (auto  &oneProperty : INDI::Weather::ParametersRangeNP )
            oneProperty.resize(0);
        INDI::Weather::ParametersNP.resize(0);
        // clear array of "ParametersRangeNP"
        INDI::Weather::ParametersRangeNP.clear();
    }

    return true;
}

// get dynamic properties
bool WeatherWatcher::getProperties()
{
    // Labels for Parameters-------------------------------------------------------------------------------------
    labelTP[WEATHER_RAIN].fill("LABEL_1", "WEATHER_RAIN_HOUR", "Rain");
    labelTP[WEATHER_TEMP].fill("LABEL_2", "WEATHER_TEMPERATURE", "Temperature");
    labelTP[WEATHER_WIND].fill("LABEL_3", "WEATHER_WIND_SPEED", "Wind");
    labelTP[WEATHER_GUST].fill("LABEL_4", "WEATHER_WIND_GUST", "Gust");
    labelTP[WEATHER_CLOUD].fill("LABEL_5", "WEATHER_CLOUDS", "Clouds");
    labelTP[WEATHER_HUM].fill("LABEL_6", "WEATHER_HUMIDITY", "Humidity");
    labelTP[WEATHER_PRESS].fill("lABEL_7", "WEATHER_PRESSURE", "Pressure");
    labelTP[WEATHER_FORECAST].fill("LABEL_8", "WEATHER_FORECAST", "Forecast");
    labelTP.fill(getDeviceName(), "LABELS", "Property Label", LABELS_TAB, IP_RW, 60, IPS_IDLE);
    defineProperty(labelTP);
    loadConfig(true, "LABELS");

    // Keywords for Parameters with dynamic labels ----------------------------------------------------------------
    keywordTP[WEATHER_RAIN].fill("KEY_1", labelTP[WEATHER_RAIN].text, "precip");
    keywordTP[WEATHER_TEMP].fill("KEY_2", labelTP[WEATHER_TEMP].text, "temperature");
    keywordTP[WEATHER_WIND].fill("KEY_3", labelTP[WEATHER_WIND].text, "wind");
    keywordTP[WEATHER_GUST].fill("KEY_4", labelTP[WEATHER_GUST].text, "gust");
    keywordTP[WEATHER_CLOUD].fill("KEY_5", labelTP[WEATHER_CLOUD].text, "clouds");
    keywordTP[WEATHER_HUM].fill("KEY_6", labelTP[WEATHER_HUM].text, "humidity");
    keywordTP[WEATHER_PRESS].fill("KEY_7", labelTP[WEATHER_PRESS].text, "pressure");
    keywordTP[WEATHER_FORECAST].fill("KEY_8", labelTP[WEATHER_FORECAST].text, "forecast");
    keywordTP.fill(getDeviceName(), "KEYWORDS", "Keywords", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    defineProperty(keywordTP);
    loadConfig(true, "KEYWORDS");

    // Critical Parameters with dynamic labels --------------------------------------------------------------------
    criticalSP[WEATHER_RAIN].fill("CRITICAL_1", labelTP[WEATHER_RAIN].text, ISS_OFF);
    criticalSP[WEATHER_TEMP].fill("CRITICAL_2", labelTP[WEATHER_TEMP].text, ISS_OFF);
    criticalSP[WEATHER_WIND].fill("CRITICAL_3", labelTP[WEATHER_WIND].text, ISS_OFF);
    criticalSP[WEATHER_GUST].fill("CRITICAL_4", labelTP[WEATHER_GUST].text, ISS_OFF);
    criticalSP[WEATHER_CLOUD].fill("CRITICAL_5", labelTP[WEATHER_CLOUD].text, ISS_OFF);
    criticalSP[WEATHER_HUM].fill("CRITICAL_6", labelTP[WEATHER_HUM].text, ISS_OFF);
    criticalSP[WEATHER_PRESS].fill("CRITICAL_7", labelTP[WEATHER_PRESS].text, ISS_OFF);
    criticalSP[WEATHER_FORECAST].fill("CRITICAL_8", labelTP[WEATHER_FORECAST].text, ISS_OFF);
    criticalSP.fill(getDeviceName(), "CRITICALS", "Criticals", PARAMETERS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);
    defineProperty(criticalSP);
    loadConfig(true, "CRITICALS");

    return true;
}
// get properties from configuation
void WeatherWatcher::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    watchFileTP[0].fill("URL", "URL", nullptr);
    watchFileTP.fill(getDeviceName(), "WATCH_SOURCE", "Source", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    defineProperty(watchFileTP);
    loadConfig(true, "WATCH_SOURCE");

    separatorTP[0].fill("SEPARATOR", "Separator", "=");
    separatorTP.fill(getDeviceName(), "SEPARATOR_KEYWORD", "Separator", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    defineProperty(separatorTP);
    loadConfig(true, "SEPARATOR_KEYWORD");
}

bool WeatherWatcher::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (labelTP.isNameMatch(name))
        {
            labelTP.update(texts, names, n);
            labelTP.setState(IPS_OK);
            labelTP.apply();
            return true;
        }
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

bool WeatherWatcher::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (criticalSP.isNameMatch(name))
        {
            // pthread_mutex_lock(&lock);
            criticalSP.update(states, names, n);
            criticalSP.setState(IPS_OK);
            criticalSP.apply();
            // pthread_mutex_unlock(&lock);
            return true;
        }
    }
    return INDI::Weather::ISNewSwitch(dev, name, states, names, n);
}

IPState WeatherWatcher::updateWeather()
{

    if (readWatchFile() == false)
        return IPS_BUSY;

    for (auto const &x : weatherMap)
    {
        if (x.first == keywordTP[WEATHER_RAIN].getText())
        {
            setParameterValue("WEATHER_RAIN_HOUR", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[WEATHER_TEMP].getText())
        {
            setParameterValue("WEATHER_TEMPERATURE", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[WEATHER_WIND].getText())
        {
            setParameterValue("WEATHER_WIND_SPEED", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[WEATHER_GUST].getText())
        {
            setParameterValue("WEATHER_WIND_GUST", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[WEATHER_CLOUD].getText())
        {
            setParameterValue("WEATHER_CLOUDS", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[WEATHER_HUM].getText())
        {
            setParameterValue("WEATHER_HUMIDITY", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[WEATHER_PRESS].getText())
        {
            setParameterValue("WEATHER_PRESSURE", std::strtod(x.second.c_str(), nullptr));
        }
        else if (x.first == keywordTP[WEATHER_FORECAST].getText())
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
        std::string readBuffer;
        curl_easy_setopt(curl, CURLOPT_URL, requestURL);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
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

    labelTP.save(fp);
    criticalSP.save(fp);
    watchFileTP.save(fp);
    keywordTP.save(fp);
    separatorTP.apply();

    return true;
}

std::map<std::string, std::string> WeatherWatcher::createMap(std::string const &s)
{
    std::map<std::string, std::string> m;

    std::string key, val;
    std::istringstream iss(s);

    while(std::getline(std::getline(iss, key, separatorTP[0].getText()[0]) >> std::ws, val))
        m[key] = val;

    return m;
}
