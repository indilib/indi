/*
    Losmandy Gemini INDI driver

    Copyright (C) 2017 Jasem Mutlaq

    Difference from LX200 Generic:

    1. Added Side of Pier
    2. Reimplemented isSlewComplete to use :Gv# since it is more reliable

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#include "lx200generic.h"

class LX200Gemini : public LX200Generic
{
  public:
    LX200Gemini();
    ~LX200Gemini() {}

    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

  protected:
    virtual const char *getDefaultName() override;

    virtual bool initProperties() override ;
    virtual bool updateProperties() override;

    virtual bool isSlewComplete() override;
    virtual bool ReadScopeStatus() override;

    virtual bool Park()override ;
    virtual bool UnPark() override;

    virtual bool SetTrackMode(uint8_t mode) override;

    virtual bool checkConnection() override;

    virtual bool saveConfigItems(FILE *fp) override;

  private:
    void syncSideOfPier();
    bool sleepMount();
    bool wakeupMount();

    // Checksum for private commands
    uint8_t calculateChecksum(char *cmd);

    ISwitch ParkSettingsS[3];
    ISwitchVectorProperty ParkSettingsSP;
    enum
    {
        PARK_HOME,
        PARK_STARTUP,
        PARK_ZENITH
    };

    ISwitch StartupModeS[3];
    ISwitchVectorProperty StartupModeSP;
    enum
    {
        COLD_START,
        WARM_START,
        WARM_RESTART
    };

    enum
    {
        GEMINI_TRACK_SIDEREAL,
        GEMINI_TRACK_KING,
        GEMINI_TRACK_LUNAR,
        GEMINI_TRACK_SOLAR

    };
    const uint8_t GEMINI_TIMEOUT = 3;
};
