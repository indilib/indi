/*******************************************************************************
  Copyright(c) 2021 Chrysikos Efstathios. All rights reserved.

  Pegasus FlatMaster

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


class PegasusFlatMaster : public INDI::DefaultDevice, public INDI::LightBoxInterface
{
public:
        PegasusFlatMaster();
        virtual ~PegasusFlatMaster() = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;   
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
protected:    
        virtual bool SetLightBoxBrightness(uint16_t value) override;
        virtual bool EnableLightBox(bool enable) override;
        

private:
        bool Ack();
        int PortFD{ -1 };
        bool sendCommand(const char* cmd,char* response);
        void updateFirmwareVersion();

         // Firmware version
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[1] {};
                
        Connection::Serial *serialConnection{ nullptr };
};
