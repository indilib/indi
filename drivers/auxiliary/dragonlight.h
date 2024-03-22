/*******************************************************************************
  Copyright(c) 2024 Rick Bassham. All rights reserved.

  Dark Dragons Astronomy DragonLIGHT

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

#include <indibase/defaultdevice.h>
#include <indibase/indilightboxinterface.h>

class DragonLIGHT : public INDI::DefaultDevice, public INDI::LightBoxInterface
{
    public:
        DragonLIGHT();
        virtual ~DragonLIGHT() override = default;

        const char *getDefaultName() override;
        void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;
        virtual void TimerHit() override;

    protected:
        virtual bool SetLightBoxBrightness(uint16_t value) override;
        virtual bool EnableLightBox(bool enable) override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        void updateStatus();
        void discoverDevices();

        INDI::PropertyText FirmwareTP {2};
        INDI::PropertyText IPAddressTP {1};
        INDI::PropertySwitch DiscoverSwitchSP {1};
};
