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

#include "indicom.h"

#include <cmath>
#include <memory>
#include <unistd.h>

// We declare an auto pointer to domeSim.
static std::unique_ptr<DomeSim> domeSim(new DomeSim());

#define DOME_SPEED    10.0 /* 10 degrees per second, constant */
#define SHUTTER_TIMER 5.0  /* Shutter closes/open in 5 seconds */

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    domeSim->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    domeSim->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    domeSim->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    domeSim->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
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

void ISSnoopDevice(XMLEle *root)
{
    domeSim->ISSnoopDevice(root);
}

DomeSim::DomeSim()
{
    targetAz        = 0;
    shutterTimer    = 0;
    prev_az         = 0;
    prev_alt        = 0;

    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_CAN_REL_MOVE | DOME_CAN_PARK | DOME_HAS_SHUTTER);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DomeSim::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_AZ);

    addAuxControls();

    return true;
}

bool DomeSim::SetupParms()
{
    targetAz     = 0;
    shutterTimer = SHUTTER_TIMER;

    DomeAbsPosNP[0].setValue(0);

    DomeParamNP[0].setValue(5);

    DomeAbsPosNP.apply();
    DomeParamNP.apply();

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(90);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(90);
        SetAxis1ParkDefault(90);
    }

    return true;
}

const char *DomeSim::getDefaultName()
{
    return "Dome Simulator";
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

bool DomeSim::Connect()
{
    SetTimer(1000); //  start the timer
    return true;
}

bool DomeSim::Disconnect()
{
    return true;
}

void DomeSim::TimerHit()
{
    int nexttimer = 1000;

    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    if (DomeAbsPosNP.getState() == IPS_BUSY)
    {
        if (targetAz > DomeAbsPosNP[0].getValue())
        {
            DomeAbsPosNP[0].value += DOME_SPEED;
        }
        else if (targetAz < DomeAbsPosNP[0].getValue())
        {
            DomeAbsPosNP[0].value -= DOME_SPEED;
        }

        DomeAbsPosNP[0].setValue(range360(DomeAbsPosNP[0].value));

        if (fabs(targetAz - DomeAbsPosNP[0].getValue()) <= DOME_SPEED)
        {
            DomeAbsPosNP[0].setValue(targetAz);
            LOG_INFO("Dome reached requested azimuth angle.");

            if (getDomeState() == DOME_PARKING)
                SetParked(true);
            else if (getDomeState() == DOME_UNPARKING)
                SetParked(false);
            else
                setDomeState(DOME_SYNCED);
        }

        DomeAbsPosNP.apply();
    }

    if (DomeShutterSP.getState() == IPS_BUSY)
    {
        if (shutterTimer-- <= 0)
        {
            shutterTimer    = 0;
            DomeShutterSP.setState(IPS_OK);
            LOGF_INFO("Shutter is %s.", (DomeShutterSP[0].getState() == ISS_ON ? "open" : "closed"));
            DomeShutterSP.apply();

            if (getDomeState() == DOME_UNPARKING)
                SetParked(false);
        }
    }
    SetTimer(nexttimer);
}

IPState DomeSim::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        targetAz       = (dir == DOME_CW) ? 1e6 : -1e6;
        DomeAbsPosNP.setState(IPS_BUSY);
    }
    else
    {
        targetAz       = 0;
        DomeAbsPosNP.setState(IPS_IDLE);
    }

    DomeAbsPosNP.apply();
    return ((operation == MOTION_START) ? IPS_BUSY : IPS_OK);
}

IPState DomeSim::MoveAbs(double az)
{
    targetAz = az;

    // Requested position is within one cycle, let's declare it done
    if (fabs(az - DomeAbsPosNP[0].getValue()) < DOME_SPEED)
        return IPS_OK;

    // It will take a few cycles to reach final position
    return IPS_BUSY;
}

IPState DomeSim::MoveRel(double azDiff)
{
    targetAz = DomeAbsPosNP[0].getValue() + azDiff;
    ;

    if (targetAz < DomeAbsPosNP[0].min)
        targetAz += DomeAbsPosNP[0].getMax();
    if (targetAz > DomeAbsPosNP[0].max)
        targetAz -= DomeAbsPosNP[0].getMax();

    // Requested position is within one cycle, let's declare it done
    if (fabs(targetAz - DomeAbsPosNP[0].getValue()) < DOME_SPEED)
        return IPS_OK;

    // It will take a few cycles to reach final position
    return IPS_BUSY;
}

IPState DomeSim::Park()
{
    targetAz = DomeParamNP[0].getValue();
    Dome::ControlShutter(SHUTTER_CLOSE);
    Dome::MoveAbs(GetAxis1Park());

    return IPS_BUSY;
}

IPState DomeSim::UnPark()
{
    return Dome::ControlShutter(SHUTTER_OPEN);
}

IPState DomeSim::ControlShutter(ShutterOperation operation)
{
    INDI_UNUSED(operation);
    shutterTimer = SHUTTER_TIMER;
    return IPS_BUSY;
}

bool DomeSim::Abort()
{
    // If we abort while in the middle of opening/closing shutter, alert.
    if (DomeShutterSP.getState() == IPS_BUSY)
    {
        DomeShutterSP.setState(IPS_ALERT);
        DomeShutterSP.apply("Shutter operation aborted. Status: unknown.");
        return false;
    }

    return true;
}

bool DomeSim::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosNP[0].value);
    return true;
}

bool DomeSim::SetDefaultPark()
{
    // By default set position to 90
    SetAxis1Park(90);
    return true;
}
