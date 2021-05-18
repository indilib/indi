/*******************************************************************************
  Copyright(c) 2019 Hans Lambermont. All rights reserved.

  INDI Weather Safety Proxy

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

#include <memory>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <curl/curl.h>

#include "gason.h"
#include "weather_safety_proxy.h"

std::unique_ptr<WeatherSafetyProxy> weatherSafetyProxy(new WeatherSafetyProxy());

static size_t WSP_WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

WeatherSafetyProxy::WeatherSafetyProxy()
{
    setVersion(1, 0);

    setWeatherConnection(CONNECTION_NONE);
}

WeatherSafetyProxy::~WeatherSafetyProxy() {}

const char *WeatherSafetyProxy::getDefaultName()
{
    return "Weather Safety Proxy";
}

bool WeatherSafetyProxy::Connect()
{
    return true;
}

bool WeatherSafetyProxy::Disconnect()
{
    return true;
}

bool WeatherSafetyProxy::initProperties()
{
    INDI::Weather::initProperties();

    IUFillText(&keywordT[0], "WEATHER_CONDITION", "Weather Condition", "condition");
    IUFillTextVector(&keywordTP, keywordT, 1, getDeviceName(), "KEYWORD", "Keywords", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillText(&ScriptsT[WSP_SCRIPT], "WEATHER_SAFETY_SCRIPT", "Weather safety script", "/usr/local/share/indi/scripts/weather_status.py");
    IUFillTextVector(&ScriptsTP, ScriptsT, WSP_SCRIPT_COUNT, getDefaultName(), "WEATHER_SAFETY_SCRIPTS", "Script", OPTIONS_TAB, IP_RW, 100, IPS_IDLE);

    IUFillText(&UrlT[WSP_URL], "WEATHER_SAFETY_URL", "Weather safety URL", "http://0.0.0.0:5000/weather/safety");
    IUFillTextVector(&UrlTP, UrlT, WSP_URL_COUNT, getDefaultName(), "WEATHER_SAFETY_URLS", "Url", OPTIONS_TAB, IP_RW, 100, IPS_IDLE);

    IUFillSwitch(&ScriptOrCurlS[WSP_USE_SCRIPT], "Use script", "", ISS_ON);
    IUFillSwitch(&ScriptOrCurlS[WSP_USE_CURL], "Use url", "", ISS_OFF);
    IUFillSwitchVector(&ScriptOrCurlSP, ScriptOrCurlS, WSP_USE_COUNT, getDeviceName(), "SCRIPT_OR_CURL", "Script or url", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&softErrorHysteresisN[WSP_SOFT_ERROR_MAX], "SOFT_ERROR_MAX", "Max soft errors", "%g", 0.0, 1000.0, 1.0, 30.0);
    IUFillNumber(&softErrorHysteresisN[WSP_SOFT_ERROR_RECOVERY], "SOFT_ERROR_RECOVERY", "Minimum soft error for recovery", "%g", 0.0, 1000.0, 1.0, 7.0);
    IUFillNumberVector(&softErrorHysteresisNP, softErrorHysteresisN, 2, getDeviceName(), "SOFT_ERROR_HYSTERESIS", "Soft error hysterese", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    addParameter("WEATHER_SAFETY", "Weather Safety", 0.9, 1.1, 0); // 0 is unsafe, 1 is safe
    setCriticalParameter("WEATHER_SAFETY");

    IUFillText(&reasonsT[0], "Reasons", "", nullptr);
    IUFillTextVector(&reasonsTP, reasonsT, 1, getDeviceName(), "WEATHER_SAFETY_REASONS", "Weather Safety Reasons", MAIN_CONTROL_TAB, IP_RO, 120, IPS_IDLE);

    addDebugControl();

    return true;
}

bool WeatherSafetyProxy::updateProperties()
{
    INDI::Weather::updateProperties();

    if (isConnected())
    {
        defineProperty(&reasonsTP);
    }
    else
    {
        deleteProperty(reasonsTP.name);
    }

    return true;
}

bool WeatherSafetyProxy::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);
    IUSaveConfigText(fp, &ScriptsTP);
    IUSaveConfigText(fp, &UrlTP);
    IUSaveConfigSwitch(fp, &ScriptOrCurlSP);
    IUSaveConfigNumber(fp, &softErrorHysteresisNP);
    return true;
}

void WeatherSafetyProxy::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);
    static bool once = true;
    if (once)
    {
        once = false;
        defineProperty(&ScriptsTP);
        defineProperty(&UrlTP);
        defineProperty(&ScriptOrCurlSP);
        defineProperty(&softErrorHysteresisNP);
        loadConfig(false, "WEATHER_SAFETY_SCRIPTS");
        loadConfig(false, "WEATHER_SAFETY_URLS");
        loadConfig(false, "SCRIPT_OR_CURL");
        loadConfig(false, "SOFT_ERROR_HYSTERESIS");
    }
}

bool WeatherSafetyProxy::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, softErrorHysteresisNP.name) == 0)
        {
            LOG_DEBUG("WeatherSafetyProxy::ISNewNumber");
            IUUpdateNumber(&softErrorHysteresisNP, values, names, n);
            softErrorHysteresisNP.s = IPS_OK;
            IDSetNumber(&softErrorHysteresisNP, nullptr);
            return true;
        }
    }
    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

bool WeatherSafetyProxy::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, keywordTP.name) == 0)
        {
            keywordTP.s = IPS_OK;
            IUUpdateText(&keywordTP, texts, names, n);
            // update client display
            IDSetText(&keywordTP, nullptr);
            return true;
        }
        if (strcmp(name, ScriptsTP.name) == 0)
        {
            ScriptsTP.s = IPS_OK;
            IUUpdateText(&ScriptsTP, texts, names, n);
            // update client display
            IDSetText(&ScriptsTP, nullptr);
            return true;
        }
        if (strcmp(name, UrlTP.name) == 0)
        {
            UrlTP.s = IPS_OK;
            IUUpdateText(&UrlTP, texts, names, n);
            // update client display
            IDSetText(&UrlTP, nullptr);
            return true;
        }
    }

    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

bool WeatherSafetyProxy::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(getDeviceName(), dev))
    {
        if (!strcmp(name, ScriptOrCurlSP.name))
        {
            LOG_DEBUG("WeatherSafetyProxy::ISNewSwitch");
            IUUpdateSwitch(&ScriptOrCurlSP, states, names, n);
            ScriptOrCurlSP.s = IPS_OK;
            IDSetSwitch(&ScriptOrCurlSP, nullptr);
            return true;
        }
    }

    return INDI::Weather::ISNewSwitch(dev, name, states, names, n);
}

// Called by Weather::TimerHit every UpdatePeriodN[0].value seconds if we return IPS_OK or every 5 seconds otherwise
IPState WeatherSafetyProxy::updateWeather()
{
    IPState ret = IPS_ALERT;
    if (ScriptOrCurlS[WSP_USE_SCRIPT].s == ISS_ON)
    {
        ret = executeScript();
    }
    else
    {
        ret = executeCurl();
    }
    if (ret != IPS_OK)
    {
        if (Safety == WSP_SAFE)
        {
            SofterrorCount++;
            LOGF_WARN("Soft error %d occurred during SAFE conditions, counting", SofterrorCount);
            if (SofterrorCount > softErrorHysteresisN[WSP_SOFT_ERROR_MAX].value)
            {
                char Warning[] = "Max softerrors reached while Weather was SAFE";
                LOG_WARN(Warning);
                Safety = WSP_UNSAFE;
                setParameterValue("WEATHER_SAFETY", WSP_UNSAFE);
                IUSaveText(&reasonsT[0], Warning);
                reasonsTP.s = IPS_OK;
                IDSetText(&reasonsTP, nullptr);
                SofterrorRecoveryMode = true;
                ret = IPS_OK; // So that indiweather actually syncs the CriticalParameters we just set
            }
        }
        else
        {
            LOG_WARN("Soft error occurred during UNSAFE conditions, ignore");
            SofterrorCount = 0;
            SofterrorRecoveryCount = 0;
        }
    }
    else
    {
        SofterrorCount = 0;
    }
    return ret;
}

IPState WeatherSafetyProxy::executeScript()
{
    const char *cmd = ScriptsT[WSP_SCRIPT].text;

    if (access(cmd, F_OK|X_OK) == -1)
    {
        LOGF_ERROR("Cannot use script [%s], check its existence and permissions", cmd);
        LastParseSuccess = false;
        return IPS_ALERT;
    }

    LOGF_DEBUG("Run script: %s", cmd);
    FILE *handle = popen(cmd, "r");
    if (handle == nullptr)
    {
        LOGF_ERROR("Failed to run script [%s]", strerror(errno));
        LastParseSuccess = false;
        return IPS_ALERT;
    }
    char buf[BUFSIZ];
    size_t byte_count = fread(buf, 1, BUFSIZ - 1, handle);
    fclose(handle);
    buf[byte_count] = 0;
    if (byte_count == 0)
    {
        LOGF_ERROR("Got no output from script [%s]", cmd);
        LastParseSuccess = false;
        return IPS_ALERT;
    }
    LOGF_DEBUG("Read %d bytes output [%s]", byte_count, buf);

    return parseSafetyJSON(buf, byte_count);
}

IPState WeatherSafetyProxy::executeCurl()
{
    CURL *curl_handle;
    CURLcode res;
    std::string readBuffer;

    curl_handle = curl_easy_init();
    if (curl_handle)
    {
        curl_easy_setopt(curl_handle, CURLOPT_URL, UrlT[WSP_URL].text);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WSP_WriteCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        LOGF_DEBUG("Call curl %s", UrlT[WSP_URL].text);
        res = curl_easy_perform(curl_handle);
        if (res != CURLE_OK)
        {
            LOGF_ERROR("curl_easy_perform failed with [%s]", curl_easy_strerror(res));
            return IPS_ALERT;
        }
        curl_easy_cleanup(curl_handle);
        LOGF_DEBUG("Read %d bytes output [%s]", readBuffer.size(), readBuffer.c_str());
        return parseSafetyJSON(readBuffer.c_str(), readBuffer.size());
    }
    else
    {
        LOG_ERROR("curl_easy_init failed");
        return IPS_ALERT;
    }
}

IPState WeatherSafetyProxy::parseSafetyJSON(const char *clean_buf, int byte_count)
{
    // copy clean_buf to buf which jsonParse can destroy
    char buf[BUFSIZ];
    strncpy(buf, clean_buf, byte_count);

    char *source = buf;
    char *endptr;
    JsonValue value;
    JsonAllocator allocator;
    int status = jsonParse(source, &endptr, &value, allocator);
    if (status != JSON_OK)
    {
        LOGF_ERROR("jsonParse %s at position %zd", jsonStrError(status), endptr - source);
        LastParseSuccess = false;
        return IPS_ALERT;
    }

    JsonIterator it;
    JsonIterator observationIterator;
    bool roof_status_found = false;
    bool open_ok_found = false;
    bool reasons_found = false;
    bool error_found = false;
    for (it = begin(value); it != end(value); ++it)
    {
        if (!strcmp(it->key, "roof_status"))
        {
            roof_status_found = true;
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                if (!strcmp(observationIterator->key, "open_ok"))
                {
                    open_ok_found = true;
                    int NewSafety = observationIterator->value.toNumber();
                    if (NewSafety != Safety)
                    {
                        if (NewSafety == WSP_UNSAFE)
                        {
                            LOG_WARN("Weather is UNSAFE");
                        }
                        else if (NewSafety == WSP_SAFE)
                        {
                            if (SofterrorRecoveryMode == true)
                            {
                                SofterrorRecoveryCount++;
                                if (SofterrorRecoveryCount > softErrorHysteresisN[WSP_SOFT_ERROR_RECOVERY].value)
                                {
                                    LOG_INFO("Minimum soft recovery errors reached while Weather was SAFE");
                                    SofterrorRecoveryCount = 0;
                                    SofterrorRecoveryMode = false;
                                }
                                else
                                {
                                    LOGF_INFO("Weather is SAFE but soft error recovery %d is still counting", SofterrorRecoveryCount);
                                    NewSafety = WSP_UNSAFE;
                                }
                            }
                            else
                            {
                                LOG_INFO("Weather is SAFE");
                            }
                        }
                        Safety = NewSafety;
                    }
                    setParameterValue("WEATHER_SAFETY", NewSafety);
                }
                else if (!strcmp(observationIterator->key, "reasons"))
                {
                    reasons_found = true;
                    char *reasons = observationIterator->value.toString();
                    if (SofterrorRecoveryMode == true)
                    {
                        char newReasons[MAXRBUF];
                        snprintf(newReasons, MAXRBUF, "SofterrorRecoveryMode, %s", reasons);
                        IUSaveText(&reasonsT[0], newReasons);
                    }
                    else
                    {
                        IUSaveText(&reasonsT[0], reasons);
                    }
                    reasonsTP.s = IPS_OK;
                    IDSetText(&reasonsTP, nullptr);
                }
            }
        }
        if (!strcmp(it->key, "error"))
        {
            error_found = true;
        }
    }

    if (error_found)
    {
        LOGF_ERROR("Error hint found in JSON [%s]", clean_buf);
        LastParseSuccess = false;
        return IPS_ALERT;
    }
    if (!roof_status_found)
    {
        LOGF_ERROR("Found no roof_status field in JSON [%s]", clean_buf);
        LastParseSuccess = false;
        return IPS_ALERT;
    }
    if (!open_ok_found)
    {
        LOGF_ERROR("Found no open_ok field in roof_status JSON [%s]", clean_buf);
        LastParseSuccess = false;
        return IPS_ALERT;
    }
    // do not error if reasons are missing, they're not required for safety
    if (!LastParseSuccess)
    {
        // show the good news. Once.
        LOGF_INFO("Script output fully parsed, weather is %s", (Safety == 1) ? "SAFE" : "UNSAFE");
        LastParseSuccess = true;
    }
    return IPS_OK;
}
