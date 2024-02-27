/*******************************************************************************
  Copyright(c) 2024 Rick Bassham. All rights reserved.

  Dark Dragons Astronomy DragonLAIR

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

#include "indidome.h"

class DragonLAIR : public INDI::Dome
{
    public:
        DragonLAIR();
        virtual ~DragonLAIR() = default;

        virtual bool initProperties() override;
        const char *getDefaultName() override;
        void ISGetProperties(const char *dev) override;
        bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

    protected:
        bool Connect() override;
        bool Disconnect() override;

        void TimerHit() override;

        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
        virtual IPState Park() override;
        virtual IPState UnPark() override;
        virtual bool Abort() override;

    private:
        void updateStatus();
        void openRoof();
        void closeRoof();
        void stopRoof();
        void discoverDevices();

        INDI::PropertyText IPAddressTP {1};
        INDI::PropertySwitch DiscoverSwitchSP {1};
};
