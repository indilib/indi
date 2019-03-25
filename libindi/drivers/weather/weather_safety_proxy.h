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
    WSP_SCRIPT = 0,
    WSP_SCRIPT_COUNT
} wsp_scripts_enum;

typedef enum
{
    WSP_URL = 0,
    WSP_URL_COUNT
} wsp_urls_enum;

typedef enum
{
    WSP_USE_SCRIPT = 0,
    WSP_USE_CURL,
    WSP_USE_COUNT
} wsp_use_enum;

typedef enum
{
    WSP_UNSAFE = 0,
    WSP_SAFE
} wsp_safety;

typedef enum
{
    WSP_SOFT_ERROR_MAX = 0,
    WSP_SOFT_ERROR_RECOVERY,
    WSP_SOFT_ERROR_COUNT
} wsp_soft_error_enum;

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
    virtual bool updateProperties() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;


  protected:
    virtual IPState updateWeather() override;
    virtual bool saveConfigItems(FILE *fp) override;

  private:
    IPState executeScript();
    IPState executeCurl();
    IPState parseSafetyJSON(const char *buf, int byte_count);

    IText keywordT[1] {};
    ITextVectorProperty keywordTP;

    IText ScriptsT[WSP_SCRIPT_COUNT] {};
    ITextVectorProperty ScriptsTP;

    IText UrlT[WSP_URL_COUNT] {};
    ITextVectorProperty UrlTP;

    ISwitch ScriptOrCurlS[WSP_USE_COUNT];
    ISwitchVectorProperty ScriptOrCurlSP;

    IText reasonsT[1] {};
    ITextVectorProperty reasonsTP;

    INumber softErrorHysteresisN[WSP_SOFT_ERROR_COUNT];
    INumberVectorProperty softErrorHysteresisNP;

    int Safety = -1;
    int SofterrorCount = 0;
    int SofterrorRecoveryCount = 0;
    bool SofterrorRecoveryMode = false;
    bool LastParseSuccess = false;
};
