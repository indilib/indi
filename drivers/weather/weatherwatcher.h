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

#pragma once

#include "indiweather.h"

#include <map>

class WeatherWatcher : public INDI::Weather
{
    public:
        // Enum for weather parameters
        enum WeatherParamIndex
        {
            WEATHER_RAIN = 0,
            WEATHER_TEMP = 1,
            WEATHER_WIND = 2,
            WEATHER_GUST = 3,
            WEATHER_CLOUD = 4,
            WEATHER_HUM = 5,
            WEATHER_PRESS = 6,
            WEATHER_FORECAST = 7
        };

        WeatherWatcher();

        //  Generic indi device entries
        bool Connect() override;
        bool Disconnect() override;
        const char *getDefaultName() override;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual IPState updateWeather() override;
        virtual bool saveConfigItems(FILE *fp) override;

        INDI::PropertyText keywordTP {8};
        INDI::PropertyText separatorTP {1};
        INDI::PropertyText labelTP {8};
        INDI::PropertySwitch criticalSP {8};

    private:
        std::map<std::string, std::string> createMap(std::string const &s);
        bool readWatchFile();
        bool createProperties();
        bool getProperties();

        INDI::PropertyText watchFileTP {1};

        std::map<std::string, std::string> weatherMap;
};
