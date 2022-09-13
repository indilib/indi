/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  Simple GPS Simulator

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
#include "indilightboxinterface.h"
#include "indidustcapinterface.h"

#include <stdint.h>

namespace Connection
{
class Serial;
}

class WandererCover : public INDI::DefaultDevice, public INDI::LightBoxInterface, public INDI::DustCapInterface
{
    public:
        WandererCover();
        virtual ~WandererCover() = default;

        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

    protected:
        const char *getDefaultName() override;

        virtual bool saveConfigItems(FILE *fp) override;

        // From Dust Cap
        virtual IPState ParkCap() override;
        virtual IPState UnParkCap() override;

        // From Light Box
        virtual bool SetLightBoxBrightness(uint16_t value) override;
        virtual bool EnableLightBox(bool enable) override;

    private:
        bool sendCommand(std::string command, char *response, bool waitForAnswer);
        bool getStartupData();
        bool switchOffLightBox();

        bool Handshake();

        // Status
        ITextVectorProperty StatusTP;
        IText StatusT[2] {};


        int PortFD{ -1 };

        Connection::Serial *serialConnection{ nullptr };
};
