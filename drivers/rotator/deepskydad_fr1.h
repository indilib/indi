/*******************************************************************************
  Copyright(c) 2020 Jasem Mutlaq. All rights reserved.

  Pegasus Falcon Rotator

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

#include "indirotator.h"
#include <stdint.h>

class DeepSkyDadFR1 : public INDI::Rotator
{
    public:
        DeepSkyDadFR1();

		typedef enum { Slow, Fast } SpeedMode;
		typedef enum { One, Two, Four, Eight } StepSize;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        const char *getDefaultName() override;

        // Event loop
        virtual void TimerHit() override;

        // Rotator Overrides
        virtual IPState MoveRotator(double angle) override;
        virtual bool ReverseRotator(bool enabled) override;
        virtual bool SyncRotator(double angle) override;
        virtual bool AbortRotator() override;

    private:
        bool Handshake() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool getInitialStatusData();
        bool getStatusData();

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////

        bool sendCommand(const char *cmd, char *res);

        ////////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ////////////////////////////////////////////////////////////////////////////////////

        // Firmware version
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[1] {};
		
        // Speed mode
        ISwitch SpeedModeS[2];
        ISwitchVectorProperty SpeedModeSP;
		// Step size
        ISwitch StepSizeS[4];
        ISwitchVectorProperty StepSizeSP;
};
