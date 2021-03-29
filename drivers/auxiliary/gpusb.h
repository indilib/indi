/*******************************************************************************
  Copyright(c) 2012-2019 Jasem Mutlaq. All rights reserved.

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
#include "indiguiderinterface.h"

#include <chrono>

class GPUSBDriver;

class GPUSB : public INDI::GuiderInterface, public INDI::DefaultDevice
{
    public:
        GPUSB();
        virtual ~GPUSB();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

        static void NSTimerHelper(void *context);
        static void WETimerHelper(void *context);

    protected:
        bool Connect() override;
        bool Disconnect() override;
        const char *getDefaultName() override;
        void debugTriggered(bool enable) override;

        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

    private:
        std::chrono::system_clock::time_point NSGuideTS, WEGuideTS;
        uint32_t NSPulseRequest = 0, WEPulseRequest = 0;
        int NSDirection = -1, WEDirection = -1, NSTimerID = -1, WETimerID = -1;

        void NSTimerCallback();
        void WETimerCallback();

        GPUSBDriver *driver;
};
