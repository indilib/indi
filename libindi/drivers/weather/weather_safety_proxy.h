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

#pragma once

#include "indiweather.h"

typedef enum
{
    WEATHER_SCRIPTS_FOLDER = 0,
    WEATHER_STATUS_SCRIPT,
    WEATHER_SCRIPT_COUNT
} weather_scripts_enum;

class WeatherSafetyProxy : public INDI::Weather
{
  public:
    WeatherSafetyProxy();
    virtual ~WeatherSafetyProxy();

    //  Generic indi device entries
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;

    virtual bool initProperties() override;
    virtual void ISGetProperties(const char *dev);
//    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

  protected:
    virtual IPState updateWeather() override;
    virtual bool saveConfigItems(FILE *fp) override;

  private:
    IPState executeScript(int script);

    int Safety = -1; // TODO make this a Light

    IText keywordT[1] {};
    ITextVectorProperty keywordTP;

    IText ScriptsT[2] {};
    ITextVectorProperty ScriptsTP;

};
