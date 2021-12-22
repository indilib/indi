/*******************************************************************************
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

  INDI Weather Simulator

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

#include "weathersimulator.h"

#include <memory>
#include <cstring>

// We declare an auto pointer to WeatherSimulator.
std::unique_ptr<WeatherSimulator> weatherSimulator(new WeatherSimulator());

WeatherSimulator::WeatherSimulator()
{
    setVersion(1, 0);

    setWeatherConnection(CONNECTION_NONE);
}

const char *WeatherSimulator::getDefaultName()
{
    return (const char *)"Weather Simulator";
}

bool WeatherSimulator::Connect()
{    
    return true;
}

bool WeatherSimulator::Disconnect()
{
    return true;
}

bool WeatherSimulator::initProperties()
{
    INDI::Weather::initProperties();

    IUFillNumber(&ControlWeatherN[CONTROL_WEATHER], "Weather", "Weather", "%.f", 0, 1, 1, 0);
    IUFillNumber(&ControlWeatherN[CONTROL_TEMPERATURE], "Temperature", "Temperature", "%.2f", -50, 70, 10, 15);
    IUFillNumber(&ControlWeatherN[CONTROL_WIND], "Wind", "Wind", "%.2f", 0, 100, 5, 0);
    IUFillNumber(&ControlWeatherN[CONTROL_GUST], "Gust", "Gust", "%.2f", 0, 50, 5, 0);
    IUFillNumber(&ControlWeatherN[CONTROL_RAIN], "Precip", "Precip", "%.f", 0, 100, 10, 0);
    IUFillNumberVector(&ControlWeatherNP, ControlWeatherN, 5, getDeviceName(), "WEATHER_CONTROL", "Control", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    addParameter("WEATHER_FORECAST", "Weather", 0, 0, 15);
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, 15);
    addParameter("WEATHER_WIND_SPEED", "Wind (kph)", 0, 20, 15);
    addParameter("WEATHER_WIND_GUST", "Gust (kph)", 0, 20, 15);
    addParameter("WEATHER_RAIN_HOUR", "Precip (mm)", 0, 0, 15);

    setCriticalParameter("WEATHER_FORECAST");
    setCriticalParameter("WEATHER_TEMPERATURE");
    setCriticalParameter("WEATHER_WIND_SPEED");
    setCriticalParameter("WEATHER_RAIN_HOUR");

    addDebugControl();

    return true;
}

bool WeatherSimulator::updateProperties()
{
    INDI::Weather::updateProperties();

    if (isConnected())
        defineProperty(&ControlWeatherNP);
    else
        deleteProperty(ControlWeatherNP.name);

    return true;
}

IPState WeatherSimulator::updateWeather()
{
    setParameterValue("WEATHER_FORECAST", ControlWeatherN[CONTROL_WEATHER].value);
    setParameterValue("WEATHER_TEMPERATURE", ControlWeatherN[CONTROL_TEMPERATURE].value);
    setParameterValue("WEATHER_WIND_SPEED", ControlWeatherN[CONTROL_WIND].value);
    setParameterValue("WEATHER_WIND_GUST", ControlWeatherN[CONTROL_GUST].value);
    setParameterValue("WEATHER_RAIN_HOUR", ControlWeatherN[CONTROL_RAIN].value);

    return IPS_OK;
}

bool WeatherSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, ControlWeatherNP.name))
        {
            IUUpdateNumber(&ControlWeatherNP, values, names, n);
            ControlWeatherNP.s = IPS_OK;
            IDSetNumber(&ControlWeatherNP, nullptr);
            LOG_INFO("Values are updated and should be active on the next weather update.");
            return true;
        }
    }

    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

bool WeatherSimulator::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &ControlWeatherNP);

    return true;
}
