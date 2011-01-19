/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/
#include "indifocuser.h"

#include <string.h>

INDI::Focuser::Focuser()
{
    //ctor
}

INDI::Focuser::~Focuser()
{
    //dtor
}

int INDI::Focuser::initProperties()
{
    DefaultDriver::initProperties();   //  let the base class flesh in what it wants

    IUFillNumber(&FocusspeedN[0],"FOCUS_SPEED_VALUE","Focus Speed","%3.0f",0.0,255.0,1.0,255.0);
    IUFillNumberVector(&FocusspeedNV,FocusspeedN,1,deviceName(),"FOCUS_SPEED","Speed","Main Control",IP_RW,60,IPS_OK);

    IUFillNumber(&FocustimerN[0],"FOCUS_TIMER_VALUE","Focus Timer","%4.0f",0.0,1000.0,10.0,1000.0);
    IUFillNumberVector(&FocustimerNV,FocustimerN,1,deviceName(),"FOCUS_TIMER","Timer","Main Control",IP_RW,60,IPS_OK);

    IUFillSwitch(&FocusmotionS[0],"FOCUS_INWARD","Focus In",ISS_ON);
    IUFillSwitch(&FocusmotionS[1],"FOCUS_OUTWARD","Focus Out",ISS_OFF);
    IUFillSwitchVector(&FocusmotionSV,FocusmotionS,2,deviceName(),"FOCUS_MOTION","Focus Direction","Main Control",IP_RW,ISR_1OFMANY,60,IPS_OK);


    return 0;
}

void INDI::Focuser::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    IDLog("INDI::Focuser::ISGetProperties %s\n",dev);
    DefaultDriver::ISGetProperties(dev);

    return;
}

bool INDI::Focuser::updateProperties()
{
    if(isConnected())
    {
        //  Now we add our focusser specific stuff
        IDDefSwitch(&FocusmotionSV, NULL);
        IDDefNumber(&FocusspeedNV, NULL);
        IDDefNumber(&FocustimerNV, NULL);
    } else
    {
        deleteProperty(FocusmotionSV.name);
        deleteProperty(FocusspeedNV.name);
        deleteProperty(FocustimerNV.name);
    }
    return true;
}


bool INDI::Focuser::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    IDLog("INDI::Focuser::ISNewNumber %s\n",name);
    if(strcmp(dev,deviceName())==0) {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,"FOCUS_TIMER")==0) {
            //  Ok, gotta move the focusser now
            int dir;
            int speed;
            int t;

            //IDLog(")
            //  first we get all the numbers just sent to us
            FocustimerNV.s=IPS_OK;
            IUUpdateNumber(&FocustimerNV,values,names,n);

            //  Now lets find what we need for this move
            speed=FocusspeedN[0].value;
            if(FocusmotionS[0].s==ISS_ON) dir=1;
            else dir=0;
            t=FocustimerN[0].value;

            Move(dir,speed,t);


            //  Update client display
            IDSetNumber(&FocustimerNV,NULL);
            return true;
        }


        if(strcmp(name,"FOCUS_SPEED")==0) {


            FocusspeedNV.s=IPS_OK;
            IUUpdateNumber(&FocusspeedNV,values,names,n);



            //  Update client display
            IDSetNumber(&FocusspeedNV,NULL);
            return true;
        }

    }


    return DefaultDriver::ISNewNumber(dev,name,values,names,n);
}

bool INDI::Focuser::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    IDLog("Enter IsNewSwitch for %s\n",name);
    //for(int x=0; x<n; x++) {
    //    IDLog("Switch %s %d\n",names[x],states[x]);
    //}

    if(strcmp(dev,deviceName())==0) {
        //  This one is for us
        if(strcmp(name,"FOCUS_MOTION")==0) {
            //  client is telling us what to do with focus direction
            FocusmotionSV.s=IPS_OK;
            IUUpdateSwitch(&FocusmotionSV,states,names,n);
            //  Update client display
            IDSetSwitch(&FocusmotionSV,NULL);

            return true;
        }

    }

    //  Nobody has claimed this, so, ignore it
    return DefaultDriver::ISNewSwitch(dev,name,states,names,n);
}

int INDI::Focuser::Move(int,int,int)
{
    //  This should be a virtual function, because the low level hardware class
    //  must override this
    //  but it's much easier early development if the method actually
    //  exists for now
    return -1;
}

