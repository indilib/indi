/*******************************************************************************

  Copyright(c) 2022 Rick Bassham. All rights reserved.

  INDI WeeWx JSON Weather Driver

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

#include <libindi/indiweather.h>
#include <libindi/indipropertytext.h>
#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

class WeewxJSON : public INDI::Weather
{
  public:
    WeewxJSON();
    virtual ~WeewxJSON();

    //  Generic indi device entries
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;

    virtual bool initProperties() override;
    bool updateProperties() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

  protected:
    void handleTemperatureData(json value, std::string key);
    void handleRawData(json value, std::string key);
    void handleBarometerData(json value, std::string key);
    void handleWindSpeedData(json value, std::string key);
    void handleRainRateData(json value, std::string key);
    void handleWeatherData(json value);

    virtual IPState updateWeather() override;

    virtual bool saveConfigItems(FILE *fp) override;

  private:
    INDI::PropertyText weewxJsonUrl{ 1 };
    enum
    {
        WEEWX_URL,
    };
};
