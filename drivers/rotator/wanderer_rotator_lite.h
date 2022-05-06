/*
   WandererAstro WandererRotatorLite
   Copyright (C) 2022 Frank Wang (1010965596@qq.com)

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#pragma once

#include "defaultdevice.h"
#include "indirotator.h"
#include "indirotatorinterface.h"
#include "indipropertyswitch.h"
class WandererRotatorLite : public INDI::Rotator
{
public:
    WandererRotatorLite();
            
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[],int n) override;




protected:
        const char * getDefaultName() override;
         virtual IPState MoveRotator(double angle) override;
         virtual IPState HomeRotator() override;
         virtual bool ReverseRotator(bool enabled) override;

         virtual bool AbortRotator() override;
         virtual void TimerHit() override;
        virtual bool SetRotatorBacklash(int32_t steps) override;
        virtual bool SetRotatorBacklashEnabled(bool enabled) override;


         
     private:
         bool Handshake() override;
        bool sendCommand(const char *cmd);
        bool Move(const char *cmd);
        ISwitchVectorProperty HomeSP;
        ISwitch HomeS[1];
        bool SetHomePosition();
        bool haltcommand=false;
        bool ReverseState;
        double positiontemp;
        int reversecoefficient;
        double backlash;
        double positionhistory;
        double backlashcompensation;
        double backlashcompensationcount;
        int positioncount;


        

};
 
  
 

  
 
