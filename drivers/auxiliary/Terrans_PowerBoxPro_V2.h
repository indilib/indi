/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  TerransPowerBoxProV2

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
#include "indiweatherinterface.h"

#include <vector>
#include <stdint.h>

#include "basedevice.h"

namespace Connection
{
  class Serial;
}

class TerransPowerBoxProV2 : public INDI::DefaultDevice, public INDI::WeatherInterface
{
public:
    TerransPowerBoxProV2();
    public:
        virtual ~TerransPowerBoxProV2() = default;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void TimerHit() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        
        // Weather Overrides
        virtual IPState updateWeather() override
        {
            return IPS_OK;
        } 
        
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
        const char *ConfigRenameDCF { nullptr };
        const char *ConfigRenameDC19V { nullptr };
        const char *ConfigRenameUSBA { nullptr };
        const char *ConfigRenameUSBB { nullptr };
        const char *ConfigRenameUSBC { nullptr };
        const char *ConfigRenameUSBD { nullptr };
        const char *ConfigRenameUSBE { nullptr };
        const char *ConfigRenameUSBF { nullptr };
        const char *ConfigRenameADJ { nullptr };      
         
        //Power Switch
        ISwitch DCAS[2];
        ISwitch DCBS[2];
        ISwitch DCCS[2];
        ISwitch DCDS[2];
        ISwitch DCES[2];
        ISwitch DCFS[2];
        ISwitch DC19VS[2];
        
        ISwitch USBAS[2];
        ISwitch USBBS[2];
        ISwitch USBCS[2];
        ISwitch USBDS[2];
        ISwitch USBES[2];
        ISwitch USBFS[2];
        
        ISwitch DCADJS[4];
        ISwitch StateSaveS[2];     
        
        ISwitch AutoHeater12VS[6];  
        ISwitch AutoHeater5VS[6];  
        
        ISwitchVectorProperty DCASP;
        ISwitchVectorProperty DCBSP;
        ISwitchVectorProperty DCCSP;
        ISwitchVectorProperty DCDSP;
        ISwitchVectorProperty DCESP;
        ISwitchVectorProperty DCFSP;
        ISwitchVectorProperty DC19VSP;
        
        ISwitchVectorProperty USBASP;
        ISwitchVectorProperty USBBSP;
        ISwitchVectorProperty USBCSP;
        ISwitchVectorProperty USBDSP;
        ISwitchVectorProperty USBESP;
        ISwitchVectorProperty USBFSP;
        
        ISwitchVectorProperty DCADJSP;
        ISwitchVectorProperty StateSaveSP;
        
        ISwitchVectorProperty AutoHeater12VSP;
        ISwitchVectorProperty AutoHeater5VSP;
        
        //Sensor Date
        INumber InputVotageN[1];
        INumberVectorProperty InputVotageNP;
        
        INumber InputCurrentN[1];
        INumberVectorProperty InputCurrentNP;
        
        INumber PowerN[4];
        INumberVectorProperty PowerNP;        
  
        INumber MCUTempN[1];
        INumberVectorProperty MCUTempNP;
        
        //save name
        IText RenameT[14];
        ITextVectorProperty RenameTP;
        
        static constexpr const char *ENVIRONMENT_TAB {"Environment"};
        static constexpr const char *ADD_SETTING_TAB {"Additional Settings"};
        
};


