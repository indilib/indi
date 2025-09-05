/*******************************************************************************
  Copyright(c) 2022 RBFocus. All rights reserved.
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

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

#include "indilightboxinterface.h"
#include "defaultdevice.h"
#include "indidustcapinterface.h"

class Excalibur : public INDI::DefaultDevice, public INDI::LightBoxInterface, public INDI::DustCapInterface
{
    public:
        Excalibur();
        virtual ~Excalibur() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;
        virtual void TimerHit() override;

        virtual bool Disconnect() override;

    protected:
        virtual bool SetLightBoxBrightness(uint16_t value) override;
        virtual bool EnableLightBox(bool enable) override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual IPState ParkCap() override;
        virtual IPState UnParkCap() override;


    private:
        bool Ack();
        int PortFD{ -1 };
        bool sendCommand(const char * cmd, char * res = nullptr);
        void getParkingStatus();
        void getLightIntensity();

        static const uint32_t DRIVER_RES { 32 };
        static const char DRIVER_DEL { '#' };
        static const char DRIVER_DEL2 { ' ' };
        static const uint8_t DRIVER_TIMEOUT { 10 };

        Connection::Serial *serialConnection{ nullptr };
};
