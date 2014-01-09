/*******************************************************************************
  Copyright(c) 2012 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#include "focus_simulator.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <memory>


// We declare an auto pointer to focusSim.
std::auto_ptr<FocusSim> focusSim(0);

#define SIM_SEEING  0
#define SIM_FWHM    1
#define FOCUS_MOTION_DELAY  100                /* Focuser takes 100 microsecond to move for each step, completing 100,000 steps in 10 seconds */

void ISPoll(void *p);


void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(focusSim.get() == 0) focusSim.reset(new FocusSim());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        focusSim->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        focusSim->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        focusSim->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        focusSim->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    ISInit();
    focusSim->ISSnoopDevice(root);
}

FocusSim::FocusSim()
{
    setFocuserFeatures(true, false, false, true);

}

bool FocusSim::SetupParms()
{
    IDSetNumber(&FWHMNP, NULL);
    return true;
}

bool FocusSim::Connect()
{
    SetTimer(1000);     //  start the timer
    return true;
}

FocusSim::~FocusSim()
{
    //dtor
}

const char * FocusSim::getDefaultName()
{
        return (char *)"Focuser Simulator";
}

bool FocusSim::initProperties()
{
    //  Most hardware layers wont actually have indi properties defined
    //  but the simulators are a special case
    INDI::Focuser::initProperties();

    IUFillNumber(&SeeingN[0],"SIM_SEEING","arcseconds","%4.2f",0,60,0,3.5);
    IUFillNumberVector(&SeeingNP,SeeingN,1,getDeviceName(),"SEEING_SETTINGS","Seeing",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&FWHMN[0],"SIM_FWHM","arcseconds","%4.2f",0,60,0,7.5);
    IUFillNumberVector(&FWHMNP,FWHMN,1,getDeviceName(), "FWHM","FWHM",MAIN_CONTROL_TAB,IP_RO,60,IPS_IDLE);

    ticks = initTicks = sqrt(FWHMN[0].value - SeeingN[0].value) / 0.75;

    if (isDebug())
        IDLog("Initial Ticks is %g\n", ticks);

    return true;
}

void FocusSim::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    INDI::Focuser::ISGetProperties(dev);



    return;
}

bool FocusSim::updateProperties()
{

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&FocusAbsPosNP);
        defineNumber(&SeeingNP);
        defineNumber(&FWHMNP);
        SetupParms();
    }
    else
    {
        deleteProperty(FocusAbsPosNP.name);
        deleteProperty(SeeingNP.name);
        deleteProperty(FWHMNP.name);
    }

    return true;
}


bool FocusSim::Disconnect()
{
    return true;
}


void FocusSim::TimerHit()
{
    int nexttimer=1000;

    if(isConnected() == false) return;  //  No need to reset timer if we are not connected anymore

    SetTimer(nexttimer);
    return;
}

bool FocusSim::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,"SEEING_SETTINGS")==0)
        {
            SeeingNP.s = IPS_OK;
            IUUpdateNumber(&SeeingNP, values, names, n);

            IDSetNumber(&SeeingNP,NULL);
            saveConfig();

            return true;

        }

    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Focuser::ISNewNumber(dev,name,values,names,n);
}


bool FocusSim::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    //  Nobody has claimed this, so, ignore it
    return INDI::Focuser::ISNewSwitch(dev,name,states,names,n);
}

bool FocusSim::Move(FocusDirection dir, int speed, int duration)
{
    double targetTicks = (speed * duration) / (FocusSpeedN[0].max * FocusTimerN[0].max);
    double plannedTicks=ticks;
    double plannedAbsPos=0;

    if (dir == FOCUS_INWARD)
        plannedTicks -= targetTicks;
    else
        plannedTicks += targetTicks;

    if (isDebug())
        IDLog("Current ticks: %g - target Ticks: %g, plannedTicks %g\n", ticks, targetTicks, plannedTicks);

    plannedAbsPos = (plannedTicks - initTicks) * 5000 + (FocusAbsPosN[0].max - FocusAbsPosN[0].min)/2;

    if (plannedAbsPos < FocusAbsPosN[0].min || plannedAbsPos > FocusAbsPosN[0].max)
    {
        IDMessage(getDeviceName(), "Error, requested position is out of range.");
        return false;
    }

    ticks = plannedTicks;
    if (isDebug())
          IDLog("Current absolute position: %g, current ticks is %g\n", plannedAbsPos, ticks);


    FWHMN[0].value = 0.5625*ticks*ticks +  SeeingN[0].value;
    FocusAbsPosN[0].value = plannedAbsPos;


    if (FWHMN[0].value < SeeingN[0].value)
        FWHMN[0].value = SeeingN[0].value;

    IDSetNumber(&FWHMNP, NULL);
    IDSetNumber(&FocusAbsPosNP, NULL);

    return true;

}

int FocusSim::MoveAbs(int targetTicks)
{
    if (targetTicks < FocusAbsPosN[0].min || targetTicks > FocusAbsPosN[0].max)
    {
        IDMessage(getDeviceName(), "Error, requested absolute position is out of range.");
        return -1;
    }

    double mid = (FocusAbsPosN[0].max - FocusAbsPosN[0].min)/2;

    IDMessage(getDeviceName() , "Focuser is moving to requested position...");

    // Limit to +/- 10 from initTicks
    ticks = initTicks + (targetTicks - mid) / 5000.0;

    if (isDebug())
        IDLog("Current ticks: %g\n", ticks);

    // simulate delay in motion as the focuser moves to the new position

    usleep( abs(targetTicks - FocusAbsPosN[0].value) * FOCUS_MOTION_DELAY);

    FWHMN[0].value = 0.5625*ticks*ticks +  SeeingN[0].value;

    if (FWHMN[0].value < SeeingN[0].value)
        FWHMN[0].value = SeeingN[0].value;

    IDSetNumber(&FWHMNP, NULL);

    return 0;

}

