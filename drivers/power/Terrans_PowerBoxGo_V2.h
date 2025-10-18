/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  TerransPowerBoxGoV2

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

#include <vector>
#include <stdint.h>

#include "basedevice.h"

namespace Connection
{
  class Serial;
}

class TerransPowerBoxGoV2 : public INDI::DefaultDevice
{
public:
    TerransPowerBoxGoV2();
    public:
        virtual ~TerransPowerBoxGoV2() = default;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void TimerHit() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
             
    protected:
        virtual const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;
        
    private:           
        bool Handshake();
        bool sendCommand(const char * cmd, char * res);
        int PortFD{-1};
       
        bool setupComplete { false };
        bool processButtonSwitch(const char *dev, const char *name, ISState *states, char *names[],int n);
        
        void Get_State();

        Connection::Serial *serialConnection { nullptr };
        
        const char *ConfigRenameDCA { nullptr };
        const char *ConfigRenameDCB { nullptr };
        const char *ConfigRenameDCC { nullptr };
        const char *ConfigRenameDCD { nullptr };
        const char *ConfigRenameDCE { nullptr };
        const char *ConfigRenameUSBA { nullptr };
        const char *ConfigRenameUSBB { nullptr };
        const char *ConfigRenameUSBE { nullptr };
        const char *ConfigRenameUSBF { nullptr };
         
        //Power Switch
        INDI::PropertySwitch DCASP {2};
        INDI::PropertySwitch DCBSP {2};
        INDI::PropertySwitch DCCSP {2};
        INDI::PropertySwitch DCDSP {2};
        INDI::PropertySwitch DCESP {2};
        
        INDI::PropertySwitch USBASP {2};
        INDI::PropertySwitch USBBSP {2};
        INDI::PropertySwitch USBESP {2};
        INDI::PropertySwitch USBFSP {2};
        
        INDI::PropertySwitch StateSaveSP {2};     
               
        //Sensor Date
        INDI::PropertyNumber InputVotageNP {1};
        
        INDI::PropertyNumber InputCurrentNP {1};
        
        INDI::PropertyNumber PowerNP {4};     
  
        INDI::PropertyNumber MCUTempNP {1};
        
        //save name
        INDI::PropertyText RenameTP {14};
        
        static constexpr const char *ADD_SETTING_TAB {"Additional Settings"};
        
};


