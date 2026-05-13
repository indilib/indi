/*******************************************************************************
  Copyright(c) 2025 cfuture81. All rights reserved.

  Wanderer ETA M54 - Electronic Tilt Adjuster

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

#include "defaultdevice.h"
#include <mutex>

namespace Connection
{
class Serial;
}

class WandererETA : public INDI::DefaultDevice
{
    public:
        WandererETA();
        virtual ~WandererETA() = default;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        const char *getDefaultName() override;
        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        // Serial communication
        bool sendCommand(std::string command);
        bool getData();
        bool parseDeviceData(const char *data);

        // State
        int firmware = 0;

        // Motor position targets (RW) - individual properties for independent control
        INDI::PropertyNumber Position1NP{1};
        INDI::PropertyNumber Position2NP{1};
        INDI::PropertyNumber Position3NP{1};

        enum
        {
            POINT_1,
            POINT_2,
            POINT_3,
        };

        // Motor position readback (RO) - what the encoders report
        INDI::PropertyNumber PositionReadNP{3};

        // Firmware information
        INDI::PropertyText FirmwareTP{1};
        enum
        {
            FIRMWARE_VERSION,
        };

        // Zero all points
        INDI::PropertySwitch ZeroAllSP{1};
        enum
        {
            ZERO_ALL,
        };

        // Backfocus offset - apply uniform offset to all points
        INDI::PropertyNumber BackfocusOffsetNP{1};
        INDI::PropertySwitch ApplyOffsetSP{1};
        enum
        {
            APPLY_OFFSET,
        };

        int PortFD{ -1 };
        Connection::Serial *serialConnection{ nullptr };

        // Mutex for thread safety
        std::timed_mutex serialPortMutex;
        bool m_SendingCommand {false};
        bool m_Initializing {true};
        bool waitForPosition(int pointIndex, double target, int timeoutMs);
        void setPositionNP(int index, double values[], char *names[], int n, IPState state);
        void setPositionState(int index, IPState state);
};
