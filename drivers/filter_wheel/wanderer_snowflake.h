/*******************************************************************************
  Copyright(c) 2026 Jérémie Klein. All rights reserved.
  Copyright(c) 2026 WandererAstro. All rights reserved. (modifications)

  Wanderer Snowflake Filter Wheel Driver

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License version 2 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indifilterwheel.h"

class WandererSnowflakeFW : public INDI::FilterWheel
{
    public:
        WandererSnowflakeFW();

    protected:

        const char *getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool Handshake() override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

        int QueryFilter() override;
        bool SelectFilter(int position) override;
        bool GetFilterNames() override;
        bool SetFilterNames() override;
        bool saveConfigItems(FILE *fp) override;

    private:
        bool readCurrentFilterFromStatus(int &position);
        bool sendCommand(const char *command, char *response, int responseLen, int timeoutSeconds);
        bool sendAutomaticCalibration();
        bool sendZeroDetection();

        ISwitch CalibrationCmdS[2] {};
        ISwitchVectorProperty CalibrationCmdSP {};

        int mDeviceID {0};
        INumber DeviceIDN[1] {};
        INumberVectorProperty DeviceIDNP {};

        char mFilterLetters[8] {};

        char mModel[16] {};
        char mFirmware[16] {};
        IText DeviceInfoT[2] {};
        ITextVectorProperty DeviceInfoTP {};
};


