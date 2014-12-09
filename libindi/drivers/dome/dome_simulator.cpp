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
#include "dome_simulator.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <memory>


// We declare an auto pointer to domeSim.
std::auto_ptr<DomeSim> domeSim(0);

#define DOME_SPEED      2.0             /* 2 degrees per second, constant */
#define SHUTTER_TIMER   5.0             /* Shutter closes/open in 5 seconds */

void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(domeSim.get() == 0) domeSim.reset(new DomeSim());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        domeSim->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        domeSim->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        domeSim->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        domeSim->ISNewNumber(dev, name, values, names, num);
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
    domeSim->ISSnoopDevice(root);
}

DomeSim::DomeSim()
{

   targetAz = 0;
   shutterTimer=0;
   prev_az=0;
   prev_alt=0;

   DomeCapability cap;

   cap.canAbort = true;
   cap.canAbsMove = true;
   cap.canRelMove = true;
   cap.hasShutter = true;
   cap.variableSpeed = false;

   SetDomeCapability(&cap);

}

bool DomeSim::SetupParms()
{
    targetAz = 0;
    shutterTimer = SHUTTER_TIMER;

    DomeAbsPosN[0].value = 0;
    DomeParamN[0].value  = 90;
    DomeParamN[1].value  = 90;

    IDSetNumber(&DomeAbsPosNP, NULL);
    IDSetNumber(&DomeParamNP, NULL);
    return true;
}

bool DomeSim::Connect()
{
    SetTimer(1000);     //  start the timer
    return true;
}

DomeSim::~DomeSim()
{

}

const char * DomeSim::getDefaultName()
{
        return (char *)"Dome Simulator";
}

bool DomeSim::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        SetupParms();
    }

    return true;
}

bool DomeSim::Disconnect()
{
    return true;
}


void DomeSim::TimerHit()
{
    int nexttimer=1000;

    if(isConnected() == false) return;  //  No need to reset timer if we are not connected anymore    

    if (DomeAbsPosNP.s == IPS_BUSY)
    {
        if (targetAz > DomeAbsPosN[0].value)
        {
            DomeAbsPosN[0].value += DOME_SPEED;
        }
        else if (targetAz < DomeAbsPosN[0].value)
        {
            DomeAbsPosN[0].value -= DOME_SPEED;
        }

        if (DomeAbsPosN[0].value < DomeAbsPosN[0].min)
            DomeAbsPosN[0].value += DomeAbsPosN[0].max;
        if (DomeAbsPosN[0].value > DomeAbsPosN[0].max)
            DomeAbsPosN[0].value -= DomeAbsPosN[0].max;

        if (fabs(targetAz - DomeAbsPosN[0].value) <= DOME_SPEED)
        {
            DomeAbsPosN[0].value = targetAz;
            DomeAbsPosNP.s = IPS_OK;
            DEBUG(INDI::Logger::DBG_SESSION, "Dome reached requested azimuth angle.");
            if (DomeGotoSP.s == IPS_BUSY)
            {
                DomeGotoSP.s = IPS_OK;
                IDSetSwitch(&DomeGotoSP, NULL);
            }
            if (GetDomeCapability().canRelMove && DomeRelPosNP.s == IPS_BUSY)
            {
                DomeRelPosNP.s = IPS_OK;
                IDSetNumber(&DomeRelPosNP, NULL);
            }
        }

        IDSetNumber(&DomeAbsPosNP, NULL);
    }

    if (DomeShutterSP.s == IPS_BUSY)
    {
        if (shutterTimer-- <= 0)
        {
            shutterTimer=0;
            DomeShutterSP.s = IPS_OK;
            DEBUGF(INDI::Logger::DBG_SESSION, "Shutter is %s.", (DomeShutterS[0].s == ISS_ON ? "open" : "closed"));
            IDSetSwitch(&DomeShutterSP, NULL);
        }
    }
    SetTimer(nexttimer);
    return;
}

int DomeSim::MoveAbsDome(double az)
{
    // Requested position is within one cycle, let's declare it done
    if (fabs(az - DomeAbsPosN[0].value) < DOME_SPEED)
        return 0;

    targetAz = az;

    // It will take a few cycles to reach final position
    return 1;

}

int DomeSim::MoveRelDome(DomeDirection dir, double azDiff)
{
    targetAz = DomeAbsPosN[0].value + (azDiff * (dir==DOME_CW ? 1 : -1));

    if (targetAz < DomeAbsPosN[0].min)
        targetAz += DomeAbsPosN[0].max;
    if (targetAz > DomeAbsPosN[0].max)
        targetAz -= DomeAbsPosN[0].max;

    // Requested position is within one cycle, let's declare it done
    if (fabs(targetAz - DomeAbsPosN[0].value) < DOME_SPEED)
        return 0;

    // It will take a few cycles to reach final position
    return 1;

}

int DomeSim::ParkDome()
{
    targetAz = DomeParamN[1].value;
    DomeAbsPosNP.s = IPS_BUSY;
    return 1;
}

int DomeSim::HomeDome()
{
    targetAz = DomeParamN[0].value;
    DomeAbsPosNP.s = IPS_BUSY;
    return 1;
}

int DomeSim::ControlDomeShutter(ShutterStatus operation)
{
    INDI_UNUSED(operation);
    shutterTimer = SHUTTER_TIMER;
    return 1;
}

bool DomeSim::AbortDome()
{
    DomeAbsPosNP.s = IPS_IDLE;
    IDSetNumber(&DomeAbsPosNP, NULL);

    if (DomeGotoSP.s == IPS_BUSY)
    {
        IUResetSwitch(&DomeGotoSP);
        DomeGotoSP.s = IPS_IDLE;
        IDSetSwitch(&DomeGotoSP, "Dome goto aborted.");
    }

    // If we abort while in the middle of opening/closing shutter, alert.
    if (DomeShutterSP.s == IPS_BUSY)
    {
        DomeShutterSP.s = IPS_ALERT;
        IDSetSwitch(&DomeShutterSP, "Shutter operation aborted. Status: unknown.");
        return false;
    }

    return true;
}
