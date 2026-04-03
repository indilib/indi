/*******************************************************************************
  Copyright(c) 2026 Jérémie Klein. All rights reserved.

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

/**
 * @brief Driver for the Wanderer Snowflake filter wheel family.
 */
class WandererSnowflakeFW : public INDI::FilterWheel
{
    public:
        WandererSnowflakeFW();

    protected:
        // INDI::DefaultDevice overrides
        const char *getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool Handshake() override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        // INDI::FilterInterface overrides
        int QueryFilter() override;
        bool SelectFilter(int position) override;

    private:
        bool readCurrentFilterFromStatus(int &position);
        bool sendCommand(const char *command, char *response, int responseLen, int timeoutSeconds);
        /** Protocol command 1500002 — automatic calibration before next filter change. */
        bool sendAutomaticCalibration();
        /** Zero position detection (command in .cpp — verify against protocol PDF). */
        bool sendZeroDetection();

        ISwitch CalibrationCmdS[2] {};
        ISwitchVectorProperty CalibrationCmdSP {};
};

