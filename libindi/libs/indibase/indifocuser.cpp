/*******************************************************************************
  Copyright(c) 2011 Gerry Rozema. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "indifocuser.h"

#include <string.h>

INDI::Focuser::Focuser()
{
}

INDI::Focuser::~Focuser()
{
}

bool INDI::Focuser::initProperties()
{
    DefaultDevice::initProperties();   //  let the base class flesh in what it wants

    IUFillNumber(&FocusSpeedN[0],"FOCUS_SPEED_VALUE","Focus Speed","%3.0f",0.0,255.0,1.0,255.0);
    IUFillNumberVector(&FocusSpeedNP,FocusSpeedN,1,getDeviceName(),"FOCUS_SPEED","Speed",MAIN_CONTROL_TAB,IP_RW,60,IPS_OK);

    IUFillNumber(&FocusTimerN[0],"FOCUS_TIMER_VALUE","Focus Timer","%4.0f",0.0,1000.0,10.0,1000.0);
    IUFillNumberVector(&FocusTimerNP,FocusTimerN,1,getDeviceName(),"FOCUS_TIMER","Timer",MAIN_CONTROL_TAB,IP_RW,60,IPS_OK);

    IUFillNumber(&FocusAbsPosN[0],"FOCUS_ABSOLUTE_POSITION","Ticks","%4.0f",0.0,100000.0,1000.0,50000.0);
    IUFillNumberVector(&FocusAbsPosNP,FocusAbsPosN,1,getDeviceName(),"FOCUS_POSITION","Absolute Position",MAIN_CONTROL_TAB,IP_RW,60,IPS_OK);

    IUFillSwitch(&FocusMotionS[0],"FOCUS_INWARD","Focus In",ISS_ON);
    IUFillSwitch(&FocusMotionS[1],"FOCUS_OUTWARD","Focus Out",ISS_OFF);
    IUFillSwitchVector(&FocusMotionSP,FocusMotionS,2,getDeviceName(),"FOCUS_MOTION","Direction",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_OK);

    addDebugControl();

    return true;
}

void INDI::Focuser::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    DefaultDevice::ISGetProperties(dev);

    return;
}

bool INDI::Focuser::updateProperties()
{
    if(isConnected())
    {
        //  Now we add our focusser specific stuff
        defineSwitch(&FocusMotionSP);
        defineNumber(&FocusSpeedNP);
        defineNumber(&FocusTimerNP);
    } else
    {
        deleteProperty(FocusMotionSP.name);
        deleteProperty(FocusSpeedNP.name);
        deleteProperty(FocusTimerNP.name);
    }
    return true;
}


bool INDI::Focuser::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,"FOCUS_TIMER")==0)
        {
            //  Ok, gotta move the focusser now
            FocusDirection dir;
            int speed;
            int t;

            //IDLog(")
            //  first we get all the numbers just sent to us
            FocusTimerNP.s=IPS_OK;
            IUUpdateNumber(&FocusTimerNP,values,names,n);

            //  Now lets find what we need for this move
            speed=FocusSpeedN[0].value;
            if(FocusMotionS[0].s==ISS_ON) dir=FOCUS_INWARD;
            else dir=FOCUS_OUTWARD;
            t=FocusTimerN[0].value;

            if (Move(dir,speed,t) == false)
                FocusTimerNP.s = IPS_ALERT;

            IDSetNumber(&FocusTimerNP,NULL);
            return true;
        }


        if(strcmp(name,"FOCUS_SPEED")==0)
        {
            FocusSpeedNP.s=IPS_OK;
            IUUpdateNumber(&FocusSpeedNP,values,names,n);



            //  Update client display
            IDSetNumber(&FocusSpeedNP,NULL);
            return true;
        }

        if(strcmp(name,"FOCUS_POSITION")==0)
        {

            int newPos = (int) values[0];

            if (MoveAbs(newPos) == true)
            {
               FocusAbsPosNP.s=IPS_OK;
               IUUpdateNumber(&FocusAbsPosNP,values,names,n);
               IDSetNumber(&FocusAbsPosNP, "Focuser moved to position %d", newPos);
               return true;
            }

            FocusAbsPosNP.s = IPS_ALERT;
            IDSetNumber(&FocusAbsPosNP, "Focuser failed to move to new requested position.");
            return false;
        }

    }


    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool INDI::Focuser::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if(strcmp(dev,getDeviceName())==0)
    {
        //  This one is for us
        if(strcmp(name,"FOCUS_MOTION")==0)
        {
            //  client is telling us what to do with focus direction
            FocusMotionSP.s=IPS_OK;
            IUUpdateSwitch(&FocusMotionSP,states,names,n);
            //  Update client display
            IDSetSwitch(&FocusMotionSP,NULL);

            return true;
        }

    }

    //  Nobody has claimed this, so, ignore it
    return DefaultDevice::ISNewSwitch(dev,name,states,names,n);
}

bool INDI::Focuser::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool INDI::Focuser::Move(FocusDirection dir, int speed, int duration)
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    //  but it's much easier early development if the method actually
    //  exists for now
    return false;
}

bool INDI::Focuser::MoveAbs(int ticks)
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    //  but it's much easier early development if the method actually
    //  exists for now
    return false;
}

bool INDI::Focuser::ISSnoopDevice (XMLEle *root)
{
    return false;
}

