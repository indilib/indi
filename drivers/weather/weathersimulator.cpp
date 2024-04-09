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

    ControlWeatherNP[CONTROL_WEATHER].fill("Weather", "Weather", "%.f", 0, 1, 1, 0);
    ControlWeatherNP[CONTROL_TEMPERATURE].fill("Temperature", "Temperature", "%.2f", -50, 70, 10, 15);
    ControlWeatherNP[CONTROL_WIND].fill("Wind", "Wind", "%.2f", 0, 100, 5, 0);
    ControlWeatherNP[CONTROL_GUST].fill("Gust", "Gust", "%.2f", 0, 50, 5, 0);
    ControlWeatherNP[CONTROL_RAIN].fill( "Precip", "Precip", "%.f", 0, 100, 10, 0);
    ControlWeatherNP.fill(getDeviceName(), "WEATHER_CONTROL", "Control", MAIN_CONTROL_TAB,
                          IP_RW, 0, IPS_IDLE);

    addParameter("WEATHER_FORECAST", "Weather", 0, 0, 0);
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -10, 30, 15);
    addParameter("WEATHER_WIND_SPEED", "Wind (kph)", 0, 20, 15);
    addParameter("WEATHER_WIND_GUST", "Gust (kph)", 0, 20, 15);
    addParameter("WEATHER_RAIN_HOUR", "Precip (mm)", 0, 0, 0);

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
        defineProperty(ControlWeatherNP);
    else
        deleteProperty(ControlWeatherNP);

    return true;
}

IPState WeatherSimulator::updateWeather()
{
    setParameterValue("WEATHER_FORECAST", ControlWeatherNP[CONTROL_WEATHER].getValue());
    setParameterValue("WEATHER_TEMPERATURE", ControlWeatherNP[CONTROL_TEMPERATURE].getValue());
    setParameterValue("WEATHER_WIND_SPEED", ControlWeatherNP[CONTROL_WIND].getValue());
    setParameterValue("WEATHER_WIND_GUST", ControlWeatherNP[CONTROL_GUST].getValue());
    setParameterValue("WEATHER_RAIN_HOUR", ControlWeatherNP[CONTROL_RAIN].getValue());

    return IPS_OK;
}

bool WeatherSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (ControlWeatherNP.isNameMatch(name))
        {
            ControlWeatherNP.update(values, names, n);
            ControlWeatherNP.setState(IPS_OK);
            ControlWeatherNP.apply();
            LOG_INFO("Values are updated and should be active on the next weather update.");
            return true;
        }
    }

    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

bool WeatherSimulator::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

    ControlWeatherNP.save(fp);

    return true;
}
