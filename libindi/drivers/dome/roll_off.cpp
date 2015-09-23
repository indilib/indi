/*******************************************************************************
 Dome Simulator
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

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
#include "roll_off.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <memory>

#include <indicom.h>

// We declare an auto pointer to RollOff.
std::auto_ptr<RollOff> rollOff(0);

#define ROLLOFF_DURATION    10      // 10 seconds until roof is fully opened or closed

void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(rollOff.get() == 0) rollOff.reset(new RollOff());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        rollOff->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        rollOff->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        rollOff->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        rollOff->ISNewNumber(dev, name, values, names, num);
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
    rollOff->ISSnoopDevice(root);
}

RollOff::RollOff()
{
  fullOpenLimitSwitch   = ISS_ON;
  fullClosedLimitSwitch = ISS_OFF;

   MotionRequest=0;

   SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool RollOff::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_NONE);

    addAuxControls();

    return true;
}

bool RollOff::SetupParms()
{
    // If we have parking data
    if (InitPark())
    {
        if (isParked())
        {
            fullOpenLimitSwitch   = ISS_OFF;
            fullClosedLimitSwitch = ISS_ON;
        }
        else
        {
            fullOpenLimitSwitch   = ISS_ON;
            fullClosedLimitSwitch = ISS_OFF;
        }
    }
    // If we don't have parking data
    else
    {
        fullOpenLimitSwitch   = ISS_OFF;
        fullClosedLimitSwitch = ISS_OFF;
    }



    return true;
}

bool RollOff::Connect()
{
    SetTimer(1000);     //  start the timer
    return true;
}

RollOff::~RollOff()
{

}

const char * RollOff::getDefaultName()
{
        return (char *)"RollOff Simulator";
}

bool RollOff::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        SetupParms();
    }

    return true;
}

bool RollOff::Disconnect()
{
    return true;
}


void RollOff::TimerHit()
{

    if(isConnected() == false) return;  //  No need to reset timer if we are not connected anymore    

   if (DomeMotionSP.s == IPS_BUSY)
   {
       // Abort called
       if (MotionRequest < 0)
       {
           DEBUG(INDI::Logger::DBG_SESSION, "Roof motion is stopped.");
           setDomeState(DOME_IDLE);
           SetTimer(1000);
           return;
       }

       // Roll off is opening
       if (DomeMotionS[DOME_CW].s == ISS_ON)
       {
           if (getFullOpenedLimitSwitch())
           {
               DEBUG(INDI::Logger::DBG_SESSION, "Roof is open.");
               SetParked(false);
               return;
           }
       }
       // Roll Off is closing
       else if (DomeMotionS[DOME_CCW].s == ISS_ON)
       {
           if (getFullClosedLimitSwitch())
           {
               DEBUG(INDI::Logger::DBG_SESSION, "Roof is closed.");
               SetParked(true);
               return;
           }
       }

       SetTimer(1000);
   }


    //SetTimer(1000);
}

bool RollOff::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        // DOME_CW --> OPEN. If can we are ask to "open" while we are fully opened as the limit switch indicates, then we simply return false.
        if (dir == DOME_CW && fullOpenLimitSwitch == ISS_ON)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully opened.");
            return false;
        }
        else if (dir == DOME_CW && getWeatherState() == IPS_ALERT)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Weather conditions are in the danger zone. Cannot open roof.");
            return false;
        }
        else if (dir == DOME_CCW && fullClosedLimitSwitch == ISS_ON)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Roof is already fully closed.");
            return false;
        }

        fullOpenLimitSwitch   = ISS_OFF;
        fullClosedLimitSwitch = ISS_OFF;
        MotionRequest = ROLLOFF_DURATION;
        gettimeofday(&MotionStart,NULL);
        SetTimer(1000);
        return true;
    }
    else
    {
        return Dome::Abort();

    }

    return false;

}

IPState RollOff::Park()
{    
    bool rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Roll off is parking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

IPState RollOff::UnPark()
{
    bool rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc)
    {       
           DEBUG(INDI::Logger::DBG_SESSION, "Roll off is unparking...");
           return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

bool RollOff::Abort()
{
    MotionRequest=-1;

    // If both limit switches are off, then we're neither parked nor unparked.
    if (fullOpenLimitSwitch == false && fullClosedLimitSwitch == false)
    {
        IUResetSwitch(&ParkSP);
        ParkSP.s = IPS_IDLE;
        IDSetSwitch(&ParkSP, NULL);
    }

    return true;
}

float RollOff::CalcTimeLeft(timeval start)
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec/1000);
    timesince=timesince/1000;
    timeleft=MotionRequest-timesince;
    return timeleft;
}

bool RollOff::getFullOpenedLimitSwitch()
{

    double timeleft = CalcTimeLeft(MotionStart);

    if (timeleft <= 0)
    {
        fullOpenLimitSwitch = ISS_ON;
        return true;
    }
    else
        return false;
}

bool RollOff::getFullClosedLimitSwitch()
{

    double timeleft = CalcTimeLeft(MotionStart);

    if (timeleft <= 0)
    {
        fullClosedLimitSwitch = ISS_ON;
        return true;
    }
    else
        return false;
}

