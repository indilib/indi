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

#pragma once

#include "indiweather.h"

#include <map>

class AAGSolo : public INDI::Weather
{
    public:
        AAGSolo();

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

        INDI::PropertySwitch criticalSP {9};

        typedef enum { CLOUDS, TEMP, WIND, GUST, RAIN,
                       LIGHTMPSAS, SAFE, HUM, RELPRESS } WeatherValue;
        struct key {
            std::string key;          // key in solo output
            std::string parameter;    // parameter name
        };
        const static int numKeys=9;
        key keys[numKeys] = {
            { "clouds", "WEATHER_CLOUDS" },
            { "temp", "WEATHER_TEMPERATURE" },
            { "wind", "WEATHER_WIND_SPEED" },
            { "gust", "WEATHER_WIND_GUST" },
            { "rain", "WEATHER_RAIN" },
            { "lightmpsas", "WEATHER_LIGHT" },
            { "safe", "WEATHER_ISSAFE" },
            { "hum", "WEATHER_HUMIDITY" },
            { "relpress", "WEATHER_PRESSURE" }
        };


    private:
        std::map<std::string, std::string> createMap(std::string const &s);
        bool readWatchFile();
        bool createProperties();

        INDI::PropertyText soloHostTP {1};
        INDI::PropertyText soloInfoTP {2};

        std::map<std::string, std::string> weatherMap;
};
