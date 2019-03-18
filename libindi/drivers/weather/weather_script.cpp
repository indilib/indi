/*******************************************************************************
  Copyright(c) 2019 Hans Lambermont. All rights reserved.

  INDI Weather Scripting Gateway

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

#include "gason.h"
#include "weather_script.h"

std::unique_ptr<WeatherScript> weatherScript(new WeatherScript());

void ISGetProperties(const char *dev)
{
    weatherScript->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    weatherScript->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    weatherScript->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    weatherScript->ISNewNumber(dev, name, values, names, n);
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
    weatherScript->ISSnoopDevice(root);
}

WeatherScript::WeatherScript()
{
    setVersion(1, 0);

    setWeatherConnection(CONNECTION_NONE);
}

WeatherScript::~WeatherScript() {}

const char *WeatherScript::getDefaultName()
{
    return (const char *)"Weather_Safety_Proxy";
}

bool WeatherScript::Connect()
{
    return true;
}

bool WeatherScript::Disconnect()
{
    return true;
}

bool WeatherScript::initProperties()
{
    INDI::Weather::initProperties();

    IUFillText(&keywordT[0], "WEATHER_CONDITION", "Weather Condition", "condition");
    IUFillTextVector(&keywordTP, keywordT, 1, getDeviceName(), "KEYWORD", "Keywords", OPTIONS_TAB, IP_RW,
                     60, IPS_IDLE);

    IUFillText(&ScriptsT[WEATHER_SCRIPTS_FOLDER], "WEATHER_SCRIPTS_FOLDER", "Weather script folder", "/usr/local/share/indi/scripts");
    IUFillText(&ScriptsT[WEATHER_STATUS_SCRIPT], "WEATHER_STATUS_SCRIPT", "Get weather safety script", "weather_status.py");
    IUFillTextVector(&ScriptsTP, ScriptsT, WEATHER_SCRIPT_COUNT, getDefaultName(), "SCRIPTS", "Scripts", OPTIONS_TAB, IP_RW, 60,
                     IPS_IDLE);

    addParameter("WEATHER_SAFETY", "Weather Safety", 0.9, 1.1, 0); // 0 is unsafe, 1 is safe
    setCriticalParameter("WEATHER_SAFETY");

    addDebugControl();

    return true;
}

bool WeatherScript::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);
    IUSaveConfigText(fp, &ScriptsTP);
    return true;
}

void WeatherScript::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);
    defineText(&ScriptsTP);
}

bool WeatherScript::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, keywordTP.name) == 0)
        {
            IUUpdateText(&keywordTP, texts, names, n);
            keywordTP.s = IPS_OK;
            IDSetText(&keywordTP, nullptr);
            return true;
        }
        if (strcmp(name, ScriptsTP.name) == 0)
        {
            IUUpdateText(&ScriptsTP, texts, names, n);
            ScriptsTP.s = IPS_OK;
            IDSetText(&ScriptsTP, nullptr);
            return true;
        }
    }

    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

IPState WeatherScript::updateWeather()
{
    return executeScript(WEATHER_STATUS_SCRIPT);
}

IPState WeatherScript::executeScript(int script)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s/%s", ScriptsT[WEATHER_SCRIPTS_FOLDER].text, ScriptsT[script].text);

    if (access(cmd, F_OK|X_OK) == -1)
    {
        LOGF_ERROR("Cannot use script [%s], check its existence and permissions", cmd);
        return IPS_ALERT;
    }

    LOGF_DEBUG("Run script: %s", cmd);
    FILE *handle = popen(cmd, "r");
    if (handle == nullptr)
    {
        LOGF_ERROR("Failed to run script [%s]", strerror(errno));
        return IPS_ALERT;
    }
    char buf[BUFSIZ];
    size_t byte_count = fread(buf, 1, BUFSIZ - 1, handle);
    fclose(handle);
    buf[byte_count] = 0;
    if (byte_count == 0)
    {
        LOGF_ERROR("Got no output from script [%s]", cmd);
        return IPS_ALERT;
    }
    LOGF_DEBUG("Read %d bytes output [%s]", byte_count, buf);

    char *source = buf;
    char *endptr;
    JsonValue value;
    JsonAllocator allocator;
    int status = jsonParse(source, &endptr, &value, allocator);
    if (status != JSON_OK)
    {
        LOGF_ERROR("jsonParse %s at position %zd", jsonStrError(status), endptr - source);
        return IPS_ALERT;
    }

    JsonIterator it;
    JsonIterator observationIterator;
    for (it = begin(value); it != end(value); ++it)
    {
        if (!strcmp(it->key, "roof_status"))
        {
            for (observationIterator = begin(it->value); observationIterator != end(it->value); ++observationIterator)
            {
                if (!strcmp(observationIterator->key, "open_ok"))
                {
                    int NewSafety = observationIterator->value.toNumber();
                    if (NewSafety != Safety)
                    {
                        if (NewSafety == 0)
                        {
                            LOG_WARN("Weather is UNSAFE");
                        }
                        else if (NewSafety == 1)
                        {
                            LOG_INFO("Weather is SAFE");
                        }
                        Safety = NewSafety;
                    }
                    // setParameterValue("WEATHER_CONDITION", NewSafety);
                    setParameterValue("WEATHER_SAFETY", NewSafety);
                }
//                else if (!strcmp(observationIterator->key, "reasons"))
//                {
//                    setParameterValue("WEATHER_CONDITION_REASONS", observationIterator->value);
//                }
            }
        }
    }

    return IPS_OK;
}
