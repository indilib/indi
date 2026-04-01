/*******************************************************************************
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
 *
 * The device uses a numeric serial protocol described in
 * Wanderer_Snowflake_Filter_Wheel_Serial_Protocol_v20260124_Rev1.pdf.
 */
class WandererSnowflakeFW : public INDI::FilterWheel
{
    public:
        WandererSnowflakeFW();

    protected:
        // INDI::DefaultDevice overrides
        const char *getDefaultName() override;
        bool initProperties() override;
        bool Handshake() override;

        // INDI::FilterInterface overrides
        int QueryFilter() override;
        bool SelectFilter(int position) override;

    private:
        bool readCurrentFilterFromStatus(int &position);
        bool sendCommand(const char *command, char *response, int responseLen, int timeoutSeconds);
};

