/*******************************************************************************
  Copyright(c) 2025 Peter Englmaier. All rights reserved.

  INDI AAG Solo CloudWatcher Driver

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

#include "aagsolo.h"
#include "locale_compat.h"

#include <curl/curl.h>

#include <memory>
#include <cstring>


// We declare an auto pointer to AAGSolo.
static std::unique_ptr<AAGSolo> aag_solo(new AAGSolo());

static size_t write_data(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

AAGSolo::AAGSolo()
{
    setVersion(0, 1);
    setWeatherConnection(CONNECTION_NONE);
}

const char *AAGSolo::getDefaultName()
{
    return "AAG Solo Cloudwatcher";
}

bool AAGSolo::Connect()
{
    if (soloHostTP[0].getText() == nullptr
            || soloHostTP[0].getText()[0] == '\0')
    {
        LOG_ERROR("AAG Solo Cloudwatcher host name or IP must be specified in options tab. Example: aagsolo.local.net");
        return false;
    }
    else
        return true;
}

bool AAGSolo::Disconnect()
{
    return true;
}

bool AAGSolo::createProperties()
{
    if (readWatchFile() == false)
        return false;
    double minOK = 0, maxOK = 0, percWarn = 15;
    for (auto const &x : weatherMap)
    {
        if (x.first == keys[RAIN].key)
        {
            // Caution: If both OK=0 the parameter never (re)appears in UI! (-> WeatherInterface::addParameter())
            minOK = 3150;
            maxOK = 5000;
            percWarn = 0;
            IUGetConfigNumber(getDeviceName(), "WEATHER_RAIN", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_RAIN", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_RAIN", "PERC_WARN", &percWarn);

            addParameter("WEATHER_RAIN",  "Rain", minOK, maxOK, percWarn);
            if (criticalSP[0].getState()) // TODO: do not specify index here
                setCriticalParameter("WEATHER_RAIN");
        }
        if (x.first == keys[LIGHTMPSAS].key)
        {
            minOK = 15;
            maxOK = 30;
            percWarn = 30;
            IUGetConfigNumber(getDeviceName(), "WEATHER_LIGHT", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_LIGHT", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_LIGHT", "PERC_WARN", &percWarn);

            addParameter("WEATHER_LIGHT",  "Light", minOK, maxOK, percWarn);
            if (criticalSP[7].getState()) // TODO: do not specify index here
                setCriticalParameter("WEATHER_LIGHT");
        }
        else if (x.first == keys[TEMP].key)
        {
            minOK = -10;
            maxOK = 30;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_TEMPERATURE", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_TEMPERATURE", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_TEMPERATURE", "PERC_WARN", &percWarn);

            addParameter("WEATHER_TEMPERATURE", "Temperature", minOK, maxOK, percWarn);
            if (criticalSP[1].getState())
                setCriticalParameter("WEATHER_TEMPERATURE");
        }
        else if (x.first == keys[WIND].key)
        {
            minOK = 0;
            maxOK = 20;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_SPEED", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_SPEED", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_SPEED", "PERC_WARN", &percWarn);

            addParameter("WEATHER_WIND_SPEED", "Wind speed", minOK, maxOK, percWarn);
            if (criticalSP[2].getState())
                setCriticalParameter("WEATHER_WIND_SPEED");
        }
        else if (x.first == keys[GUST].key)
        {
            minOK = 0;
            maxOK = 20;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_WIND_GUST", "PERC_WARN", &percWarn);

            addParameter("WEATHER_WIND_GUST", "Gust", minOK, maxOK, percWarn);
            if (criticalSP[3].getState())
                setCriticalParameter("WEATHER_WIND_GUST");
        }
        else if (x.first == keys[CLOUDS].key)
        {
            minOK = -30;
            maxOK = -10;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_CLOUDS", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_CLOUDS", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_CLOUDS", "PERC_WARN", &percWarn);

            addParameter("WEATHER_CLOUDS",  "Clouds", minOK, maxOK, percWarn);
            if (criticalSP[4].getState())
                setCriticalParameter("WEATHER_CLOUDS");
        }
        else if (x.first == keys[HUM].key)
        {
            minOK = 20;
            maxOK = 95;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_HUMIDITY", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_HUMIDITY", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_HUMIDITY", "PERC_WARN", &percWarn);

            addParameter("WEATHER_HUMIDITY", "Humidity", minOK, maxOK, percWarn);
            if (criticalSP[5].getState())
                setCriticalParameter("WEATHER_HUMIDITY");
        }
        else if (x.first == keys[RELPRESS].key)
        {
            minOK = 983;
            maxOK = 1043;
            percWarn = 15;
            IUGetConfigNumber(getDeviceName(), "WEATHER_PRESSURE", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_PRESSURE", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_PRESSURE", "PERC_WARN", &percWarn);

            addParameter("WEATHER_PRESSURE", "rel. pressure", minOK, maxOK, percWarn);
            if (criticalSP[6].getState())
                setCriticalParameter("WEATHER_PRESSURE");
        }
        else if (x.first == keys[SAFE].key)
        {
            minOK = 0.5;
            maxOK = 1.5;
            percWarn = 0;
            IUGetConfigNumber(getDeviceName(), "WEATHER_ISSAFE", "MIN_OK", &minOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_ISSAFE", "MAX_OK", &maxOK);
            IUGetConfigNumber(getDeviceName(), "WEATHER_ISSAFE", "PERC_WARN", &percWarn);

            addParameter("WEATHER_ISSAFE", "Safe", minOK, maxOK, percWarn);
            if (criticalSP[8].getState())
                setCriticalParameter("WEATHER_ISSAFE");
        }
    }
    return true;
}

// initialize properties on launching indiserver
bool AAGSolo::initProperties()
{
    INDI::Weather::initProperties(); // FIXME: parameters shoudl be statusGroup and paramsGroup
    addDebugControl();

    // Critical Parameters --------------------------------------------------------------------
    criticalSP[0].fill("CRITICAL_1", "Rain", ISS_ON);
    criticalSP[1].fill("CRITICAL_2", "Temperature", ISS_OFF);
    criticalSP[2].fill("CRITICAL_3", "Wind", ISS_ON);
    criticalSP[3].fill("CRITICAL_4", "Gust", ISS_ON);
    criticalSP[4].fill("CRITICAL_5", "Clouds", ISS_ON);
    criticalSP[5].fill("CRITICAL_6", "Humidity", ISS_OFF);
    criticalSP[6].fill("CRITICAL_7", "Pressure", ISS_OFF);
    criticalSP[7].fill("CRITICAL_8", "Light", ISS_OFF);
    criticalSP[8].fill("CRITICAL_9", "Safe", ISS_OFF);
    criticalSP.fill(getDeviceName(), "CRITICALS", "Criticals", OPTIONS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);
    criticalSP.load();

    soloInfoTP[0].fill("CWINFO", "cwinfo", nullptr);
    soloInfoTP[1].fill("DATATIME", "GMT Time", nullptr);
    soloInfoTP.fill(getDeviceName(), "DEVICEINFO", "Device Info", INFO_TAB, IP_RO, 60, IPS_IDLE);
    return true;
}

// initialize properties after connection/disconnection
bool AAGSolo::updateProperties()
{
    if (isConnected())
    {
        createProperties();
        INDI::Weather::updateProperties(); // define inherited properties
    }
    else
    {
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
    defineProperty(criticalSP);
    defineProperty(soloInfoTP);

    return true;
}

// get properties from configuation
void AAGSolo::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    soloHostTP[0].fill("HOSTNAME", "HOSTNAME", nullptr);
    soloHostTP.fill(getDeviceName(), "SOLO_HOST", "Solo", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    defineProperty(soloHostTP);
    loadConfig(true, "SOLO_HOST");
}

bool AAGSolo::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (soloHostTP.isNameMatch(name))
        {
            soloHostTP.update(texts, names, n);
            soloHostTP.setState(IPS_OK);
            soloHostTP.apply();
            return true;
        }
    }
    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

bool AAGSolo::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
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
            if (isConnected())
            {
                LOG_WARN("Changing criticals requires driver reconnect");
            }
            return true;
        }
    }
    return INDI::Weather::ISNewSwitch(dev, name, states, names, n);
}

IPState AAGSolo::updateWeather()
{
    if (readWatchFile() == false)
        return IPS_BUSY;

    for (auto const &x : weatherMap)
    {
        for (int i = 0; i < numKeys; i++)
        {
            if (x.first == keys[i].key)
            {
                setParameterValue(keys[i].parameter, std::strtod(x.second.c_str(), nullptr));
                break;
            }
        }
        if (x.first == "cwinfo")
        {
            soloInfoTP[0].fill("CWINFO", "cwinfo", x.second.c_str());
            soloInfoTP.setState(IPS_OK);
            soloInfoTP.apply();
        }
        else if (x.first == "dataGMTTime")
        {
            soloInfoTP[1].fill("DATATIME", "GMT Time", x.second.c_str());
            soloInfoTP.setState(IPS_OK);
            soloInfoTP.apply();
        }
    }
    return IPS_OK;
}

bool AAGSolo::readWatchFile()
{
    CURL *curl;
    CURLcode res;
    bool rc = false;
    char requestURL[MAXRBUF];

    AutoCNumeric locale;

    snprintf(requestURL, MAXRBUF, "http://%s/cgi-bin/cgiLastData", soloHostTP[0].getText());

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

bool AAGSolo::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

    criticalSP.save(fp);
    soloHostTP.save(fp);

    return true;
}

std::map<std::string, std::string> AAGSolo::createMap(std::string const &s)
{
    std::map<std::string, std::string> m;

    std::string key, val;
    std::istringstream iss(s);

    while(std::getline(std::getline(iss, key, '=') >> std::ws, val))
        m[key] = val;

    return m;
}
