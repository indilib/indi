/*
    MaxDome II 
    Copyright (C) 2009 Ferran Casarramona (ferran.casarramona@gmail.com)

    Migrated to INDI::Dome by Jasem Mutlaq (2014)

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

/* MaxDome II Command Set */
#include "maxdomeiidriver.h"

/* Our driver header */
#include "maxdomeii.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <memory>

const int POLLMS = 1000;                            // Period of update, 1 second.
const char *OPERATION_GROUP  = "Operation";			// Operation Group

// We declare an auto pointer to dome.
std::auto_ptr<MaxDomeII> dome(0);


void ISPoll(void *p);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(dome.get() == 0) dome.reset(new MaxDomeII());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        dome->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        dome->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        dome->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        dome->ISNewNumber(dev, name, values, names, num);
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
    dome->ISSnoopDevice(root);
}

MaxDomeII::MaxDomeII()
{

   fd = -1;
   nTicksPerTurn = 360;
   nCurrentTicks = 0;
   nParkPosition = 0.0;
   nHomeAzimuth = 0.0;
   nHomeTicks = 0;
   nCloseShutterBeforePark = 0;
   nTimeSinceShutterStart = -1; // No movement has started
   nTimeSinceAzimuthStart = -1; // No movement has started
   nTargetAzimuth = -1; //Target azimuth not established
   nTimeSinceLastCommunication = 0;

   DomeCapability cap;

   cap.canAbort = true;
   cap.canAbsMove = true;
   cap.canRelMove = false;
   cap.canPark = true;

   // We do have shutter, but we want to override INDI::Dome default shutter operation property
   cap.hasShutter = false;
   cap.hasVariableSpeed = false;

   SetDomeCapability(&cap);
}

bool MaxDomeII::SetupParms()
{
    DomeAbsPosN[0].value = 0;
    //DomeParamN[DOME_HOME].value  = nHomeAzimuth;

    IDSetNumber(&DomeAbsPosNP, NULL);
    IDSetNumber(&DomeParamNP, NULL);

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(0);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(0);
        SetAxis1ParkDefault(0);
    }

    return true;
}

bool MaxDomeII::Connect()
{
    int error;

    if (fd >= 0)
        Disconnect_MaxDomeII(fd);

    DEBUG(INDI::Logger::DBG_SESSION, "Opening port ...");

    fd = Connect_MaxDomeII(PortT[0].text);

    if (fd < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to port %s. Make sure you have BOTH write and read permission to your port.", PortT[0].text);
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Connecting ...");

    error = Ack_MaxDomeII(fd);
    if (error)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to dome (%s).", ErrorMessages[-error]);
        Disconnect_MaxDomeII(fd);
        fd = -1;
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Dome is online.");

    SetTimer(POLLMS);

    return true;
}

MaxDomeII::~MaxDomeII()
{
    prev_az = prev_alt = 0;
}

const char * MaxDomeII::getDefaultName()
{
        return (char *)"MaxDome II";
}

bool MaxDomeII::initProperties()
{
    INDI::Dome::initProperties();

    // Type of movement
    IUFillSwitch(&OnMovementTypeS[0], "FREE_MOVE", "Free Move", ISS_ON);
    IUFillSwitch(&OnMovementTypeS[1], "WIRED_MOVE", "Wired Move", ISS_OFF);
    IUFillSwitchVector(&OnMovementTypeSP, OnMovementTypeS, NARRAY(OnMovementTypeS), getDeviceName(), "ON_Movement_TYPE", "Movement type", OPERATION_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Home position - GET (for debug purpouses)
    IUFillNumber(&HomePosRN[0], "HOME_POS", "Home Position", "%5.2f",  0., 360., 0., 0.);
    IUFillNumberVector(&HomePosRNP, HomePosRN, NARRAY(HomePosRN), getDeviceName(), "HOME_POS_GET" , "Home Position", OPERATION_GROUP, IP_RO, 0, IPS_IDLE);

    // Ticks per turn
    TicksPerTurnN[0].value = nTicksPerTurn;
    IUFillNumber(&TicksPerTurnN[0], "TICKS_PER_TURN", "Ticks per turn", "%d",  0., 360., 0., 0.);
    IUFillNumberVector(&TicksPerTurnNP, TicksPerTurnN, NARRAY(TicksPerTurnN), getDeviceName(), "TICKS_PER_TURN" , "Ticks per turn", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Park on shutter
    IUFillSwitch(&ParkOnShutterS[0], "PARK", "Park", ISS_ON);
    IUFillSwitch(&ParkOnShutterS[1], "NO_PARK", "No park", ISS_OFF);
    IUFillSwitchVector(&ParkOnShutterSP, ParkOnShutterS, NARRAY(ParkOnShutterS), getDeviceName(), "PARK_ON_SHUTTER", "Park before operating shutter", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Shutter - SET
    IUFillSwitch(&ShutterS[0], "OPEN_SHUTTER", "Open shutter", ISS_OFF);
    IUFillSwitch(&ShutterS[1], "OPEN_UPPER_SHUTTER", "Open upper shutter", ISS_OFF);
    IUFillSwitch(&ShutterS[2], "CLOSE_SHUTTER", "Close shutter", ISS_ON);
    IUFillSwitchVector(&ShutterSP, ShutterS, NARRAY(ShutterS), getDeviceName(), "SHUTTER", "Shutter", OPERATION_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Watch Dog
    IUFillNumber(&WatchDogN[0], "WATCH_DOG_TIME", "Watch dog time", "%5.2f",  0., 3600., 0., 0.);
    IUFillNumberVector(&WatchDogNP, WatchDogN, NARRAY(WatchDogN), getDeviceName(), "WATCH_DOG_TIME_SET" , "Watch dog time set", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    return true;
}

bool MaxDomeII::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        defineSwitch(&OnMovementTypeSP);
        defineNumber(&HomePosRNP);
        defineNumber(&TicksPerTurnNP);
        defineSwitch(&ParkOnShutterSP);
        defineSwitch(&ShutterSP);
        defineNumber(&WatchDogNP);

        SetupParms();
    }
    else
    {
        deleteProperty(OnMovementTypeSP.name);
        deleteProperty(HomePosRNP.name);
        deleteProperty(TicksPerTurnNP.name);
        deleteProperty(ParkOnShutterSP.name);
        deleteProperty(ShutterSP.name);
        deleteProperty(WatchDogNP.name);
    }

    return true;
}

bool MaxDomeII::Disconnect()
{
    if (fd >= 0)
        Disconnect_MaxDomeII(fd);

    return true;
}


void MaxDomeII::TimerHit()
{
    SH_Status nShutterStatus;
    AZ_Status nAzimuthStatus;
    unsigned nHomePosition;
    float nAz;
    int nError;
    int nRetry = 1;

    if(isConnected() == false)
        return;  //  No need to reset timer if we are not connected anymore

    nError = Status_MaxDomeII(fd, &nShutterStatus, &nAzimuthStatus, &nCurrentTicks, &nHomePosition);
    handle_driver_error(&nError, &nRetry); // This is a timer, we will not repeat in order to not delay the execution.

    // Increment movment time counter
    if (nTimeSinceShutterStart >= 0)
        nTimeSinceShutterStart++;
    if (nTimeSinceAzimuthStart >= 0)
        nTimeSinceAzimuthStart++;

    // Watch dog
    nTimeSinceLastCommunication++;
    if (WatchDogNP.np[0].value > 0 && WatchDogNP.np[0].value <= nTimeSinceLastCommunication)
    {
        // Close Shutter if it is not
        if (nShutterStatus != Ss_CLOSED)
        {
            int error;

            error = Close_Shutter_MaxDomeII(fd);
            handle_driver_error(&error, &nRetry); // This is a timer, we will not repeat in order to not delay the execution.

            nTimeSinceShutterStart = 0;	// Init movement timer
            if (error)
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "MAX DOME II: %s",ErrorMessages[-error]);
                ShutterSP.s = IPS_ALERT;
                IDSetSwitch(&ShutterSP, "Error closing shutter");
            }
            else
            {
                nTimeSinceLastCommunication = 0;
                ShutterS[2].s = ISS_ON;
                ShutterS[1].s = ISS_OFF;
                ShutterS[0].s = ISS_OFF;
                ShutterSP.s = IPS_BUSY;
                IDSetSwitch(&ShutterSP, "Closing shutter due watch dog");
            }
        }

    }

    if (!nError)
    {
        // Shutter
        switch(nShutterStatus)
        {
        case Ss_CLOSED:
            if (ShutterS[2].s == ISS_ON) // Close Shutter
            {
                if (ShutterSP.s == IPS_BUSY || ShutterSP.s == IPS_ALERT)
                {
                    ShutterSP.s = IPS_OK; // Shutter close movement ends.
                    nTimeSinceShutterStart = -1;
                    IDSetSwitch (&ShutterSP, "Shutter is closed");
                }
            }
            else
            {
                if (nTimeSinceShutterStart >= 0)
                { // A movement has started. Warn but don't change
                    if (nTimeSinceShutterStart >= 4)
                    {
                        ShutterSP.s = IPS_ALERT; // Shutter close movement ends.
                        IDSetSwitch (&ShutterSP, "Shutter still closed");
                    }
                }
                else
                { // For some reason (manual operation?) the shutter has closed
                    ShutterSP.s = IPS_IDLE;
                    ShutterS[2].s = ISS_ON;
                    ShutterS[1].s = ISS_OFF;
                    ShutterS[0].s = ISS_OFF;
                    IDSetSwitch (&ShutterSP, "Unexpected shutter closed");
                }
            }
            break;
        case Ss_OPENING:
            if (ShutterS[0].s == ISS_OFF && ShutterS[1].s == ISS_OFF) // not opening Shutter
            {  // For some reason the shutter is opening (manual operation?)
                ShutterSP.s = IPS_ALERT;
                ShutterS[2].s = ISS_OFF;
                ShutterS[1].s = ISS_OFF;
                ShutterS[0].s = ISS_OFF;
                IDSetSwitch (&ShutterSP, "Unexpected shutter opening");
            }
            else if (nTimeSinceShutterStart < 0)
            {	// For some reason the shutter is opening (manual operation?)
                ShutterSP.s = IPS_ALERT;
                nTimeSinceShutterStart = 0;
                IDSetSwitch (&ShutterSP, "Unexpected shutter opening");
            }
            else if (ShutterSP.s == IPS_ALERT)
            {	// The alert has corrected
                ShutterSP.s = IPS_BUSY;
                IDSetSwitch (&ShutterSP, "Shutter is opening");
            }
            break;
        case Ss_OPEN:
            if (ShutterS[0].s == ISS_ON) // Open Shutter
            {
                if (ShutterSP.s == IPS_BUSY || ShutterSP.s == IPS_ALERT)
                {
                    ShutterSP.s = IPS_OK; // Shutter open movement ends.
                    nTimeSinceShutterStart = -1;
                    IDSetSwitch (&ShutterSP, "Shutter is open");
                }
            }
            else if (ShutterS[1].s == ISS_ON) // Open Upper Shutter
            {
                if (ShutterSP.s == IPS_BUSY || ShutterSP.s == IPS_ALERT)
                {
                    ShutterSP.s = IPS_OK; // Shutter open movement ends.
                    nTimeSinceShutterStart = -1;
                    IDSetSwitch (&ShutterSP, "Upper shutter is open");
                }
            }
            else
            {
                if (nTimeSinceShutterStart >= 0)
                { // A movement has started. Warn but don't change
                    if (nTimeSinceShutterStart >= 4)
                    {
                        ShutterSP.s = IPS_ALERT; // Shutter close movement ends.
                        IDSetSwitch (&ShutterSP, "Shutter still open");
                    }
                }
                else
                { // For some reason (manual operation?) the shutter has open
                    ShutterSP.s = IPS_IDLE;
                    ShutterS[2].s = ISS_ON;
                    ShutterS[1].s = ISS_OFF;
                    ShutterS[0].s = ISS_OFF;
                    IDSetSwitch (&ShutterSP, "Unexpected shutter open");
                }
            }
            break;
        case Ss_CLOSING:
            if (ShutterS[2].s == ISS_OFF) // Not closing Shutter
            {	// For some reason the shutter is closing (manual operation?)
                ShutterSP.s = IPS_ALERT;
                ShutterS[2].s = ISS_ON;
                ShutterS[1].s = ISS_OFF;
                ShutterS[0].s = ISS_OFF;
                IDSetSwitch (&ShutterSP, "Unexpected shutter closing");
            }
            else  if (nTimeSinceShutterStart < 0)
            {	// For some reason the shutter is opening (manual operation?)
                ShutterSP.s = IPS_ALERT;
                nTimeSinceShutterStart = 0;
                IDSetSwitch (&ShutterSP, "Unexpected shutter closing");
            }
            else if (ShutterSP.s == IPS_ALERT)
            {	// The alert has corrected
                ShutterSP.s = IPS_BUSY;
                IDSetSwitch (&ShutterSP, "Shutter is closing");
            }
            break;
        case Ss_ABORTED:
        case Ss_ERROR:
        default:
            if (nTimeSinceShutterStart >= 0)
            {
                ShutterSP.s = IPS_ALERT; // Shutter movement aborted.
                ShutterS[2].s = ISS_OFF;
                ShutterS[1].s = ISS_OFF;
                ShutterS[0].s = ISS_OFF;
                nTimeSinceShutterStart = -1;
                IDSetSwitch (&ShutterSP, "Unknown shutter status");
            }
            break;
        }

        /*if (DomeParamN[DOME_HOME].value != nHomePosition)
        {	// Only refresh position if it changed
            DomeParamN[DOME_HOME].value = nHomePosition;
            //sprintf(buf,"%d", nHomePosition);
            IDSetNumber(&HomePosRNP, NULL);
        }*/

        // Azimuth
        nAz = TicksToAzimuth(nCurrentTicks);
        if (DomeAbsPosN[0].value != nAz)
        {	// Only refresh position if it changed
            DomeAbsPosN[0].value = nAz;
            //sprintf(buf,"%d", nCurrentTicks);
            IDSetNumber(&DomeAbsPosNP, NULL);
        }

        switch(nAzimuthStatus)
        {
        case As_IDLE:
        case As_IDEL2:
            if (nTimeSinceAzimuthStart > 3)
            {
                if (nTargetAzimuth >= 0 && AzimuthDistance(nTargetAzimuth,nCurrentTicks) > 3) // Maximum difference allowed: 3 ticks
                {
                    DomeAbsPosNP.s = IPS_ALERT;
                    nTimeSinceAzimuthStart = -1;
                    IDSetNumber(&DomeAbsPosNP, "Could not position right");
                }
                else
                {   // Succesfull end of movement
                    if (DomeAbsPosNP.s != IPS_OK)
                    {
                        DomeAbsPosNP.s = IPS_OK;
                        nTimeSinceAzimuthStart = -1;
                        IDSetNumber(&DomeAbsPosNP, "Dome is on target position");
                    }
                    if (getDomeState() == DOME_PARKING)
                    {
                        nTimeSinceAzimuthStart = -1;
                        SetParked(true);
                    }

                    /*if (DomeGotoS[DOME_HOME].s == ISS_ON)
                    {
                        IUResetSwitch(&DomeGotoSP);
                        DomeGotoSP.s = IPS_OK;
                        nTimeSinceAzimuthStart = -1;
                        IDSetSwitch(&DomeGotoSP, "Dome is homed");
                    }*/
                }
            }
            break;
        case As_MOVING_WE:
        case As_MOVING_EW:
            if (nTimeSinceAzimuthStart < 0)
            {
                nTimeSinceAzimuthStart = 0;
                nTargetAzimuth = -1;
                DomeAbsPosNP.s = IPS_ALERT;
                IDSetNumber(&DomeAbsPosNP, "Unexpected dome moving");
            }
            break;
        case As_ERROR:
            if (nTimeSinceAzimuthStart >= 0)
            {
                DomeAbsPosNP.s = IPS_ALERT;
                nTimeSinceAzimuthStart = -1;
                nTargetAzimuth = -1;
                IDSetNumber(&DomeAbsPosNP, "Dome Error");
            }
        default:
            break;
        }


    }
    else
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Error: %s. Please reconnect and try again.",ErrorMessages[-nError]);
        return;
    }

    SetTimer(POLLMS);
    return;
}

IPState MaxDomeII::MoveAbs(double newAZ)
{
    double currAZ = 0;
    int newPos = 0, nDir = 0;
    int error;
    int nRetry = 3;

    currAZ = DomeAbsPosN[0].value;

    // Take the shortest path
    if (newAZ > currAZ)
    {
        if (newAZ - currAZ > 180.0)
            nDir = MAXDOMEII_WE_DIR;
        else
            nDir = MAXDOMEII_EW_DIR;
    }
    else
    {
        if (currAZ - newAZ > 180.0)
            nDir = MAXDOMEII_EW_DIR;
        else
            nDir = MAXDOMEII_WE_DIR;
    }

    newPos = AzimuthToTicks(newAZ);

    while (nRetry)
    {
        error = Goto_Azimuth_MaxDomeII(fd, nDir, newPos);
        handle_driver_error(&error, &nRetry);
    }

    if (error != 0)
        return IPS_ALERT;

    nTargetAzimuth = newPos;
    nTimeSinceAzimuthStart = 0;	// Init movement timer

    // It will take a few cycles to reach final position
    return IPS_BUSY;

}

/*IPState MaxDomeII::Home()
{
    int error;
    int nRetry = 3;

    while (nRetry)
    {
        error = Home_Azimuth_MaxDomeII(fd);
        handle_driver_error(&error, &nRetry);
    }

    nTimeSinceAzimuthStart = 0;
    nTargetAzimuth = -1;

    if (error)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error: %s", ErrorMessages[-error]);
        return IPS_ALERT;
    }

    return IPS_BUSY;

}*/

bool MaxDomeII::Abort()
{
    int error=0;
    int nRetry = 3;

    while (nRetry)
    {
            error = Abort_Azimuth_MaxDomeII(fd);
            handle_driver_error(&error, &nRetry);
    }

    while (nRetry)
    {
            error = Abort_Shutter_MaxDomeII(fd);
            handle_driver_error(&error, &nRetry);
    }

    /*if (DomeGotoSP.s == IPS_BUSY)
    {
        IUResetSwitch(&DomeGotoSP);
        DomeGotoSP.s = IPS_IDLE;
        IDSetSwitch(&DomeGotoSP, "Dome Goto aborted.");
    }*/

    DomeAbsPosNP.s = IPS_IDLE;
    IDSetNumber(&DomeAbsPosNP, NULL);

    // If we abort while in the middle of opening/closing shutter, alert.
    if (DomeShutterSP.s == IPS_BUSY)
    {
        DomeShutterSP.s = IPS_ALERT;
        IDSetSwitch(&DomeShutterSP, "Shutter operation aborted.");
        return false;
    }

    return true;
}


/**************************************************************************************
**
***************************************************************************************/
bool MaxDomeII::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	
	// Ignore if not ours
    if (strcmp (dev, getDeviceName()))
        return false;
	
	nTimeSinceLastCommunication = 0;
	
	// ===================================
    // TicksPerTurn
	// ===================================
	if (!strcmp (name, TicksPerTurnNP.name))
	{
		double nVal;
		char cLog[255];
		int error;
		int nRetry = 3;
		
		if (IUUpdateNumber(&TicksPerTurnNP, values, names, n) < 0)
            return false;
			
		nVal = values[0];
		if (nVal >= 100 && nVal <=500)
		{
            while (nRetry)
            {
				error = SetTicksPerCount_MaxDomeII(fd, nVal);
				handle_driver_error(&error,&nRetry);
			}
			if (error >= 0)
			{
				sprintf(cLog, "New Ticks Per Turn set: %lf", nVal);
				nTicksPerTurn = nVal;
				nHomeTicks = floor(0.5 + nHomeAzimuth * nTicksPerTurn / 360.0); // Calculate Home ticks again
				TicksPerTurnNP.s = IPS_OK;
				TicksPerTurnNP.np[0].value = nVal;
				IDSetNumber(&TicksPerTurnNP, "%s", cLog);
			}
            else
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "MAX DOME II: %s",ErrorMessages[-error]);
				TicksPerTurnNP.s = IPS_ALERT;
                IDSetNumber(&TicksPerTurnNP, NULL);
			}

            return true;
		}

		// Incorrect value. 
		TicksPerTurnNP.s = IPS_ALERT;
		IDSetNumber(&TicksPerTurnNP, "Invalid Ticks Per Turn");
		
        return false;
	}
	
   /* if (!strcmp(name, DomeParamNP.name))
    {
        IUUpdateNumber(&DomeParamNP, values, names, n);

        double nVal = DomeParamN[DOME_HOME].value;

        // Only update home azimuth if there is change
        if (nVal != nHomeAzimuth)
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "New home azimuth set: %lf", nVal);
            nHomeAzimuth = nVal;
            nHomeTicks = floor(0.5 + nHomeAzimuth * nTicksPerTurn / 360.0);
        }

        DomeParamNP.s = IPS_OK;
        IDSetNumber(&DomeParamNP, NULL);
        return true;
    }*/
		
	// ===================================
    // Watch dog
	// ===================================
    if (!strcmp (name, WatchDogNP.name))
	{
		double nVal;
		char cLog[255];
		
        if (IUUpdateNumber(&WatchDogNP, values, names, n) < 0)
            return false;
		
		nVal = values[0];
		if (nVal >= 0 && nVal <=3600)
		{
			sprintf(cLog, "New watch dog set: %lf", nVal);
            WatchDogNP.s = IPS_OK;
            WatchDogNP.np[0].value = nVal;
            IDSetNumber(&WatchDogNP, "%s", cLog);
            return false;
		}
		// Incorrect value. 
        WatchDogNP.s = IPS_ALERT;
        IDSetNumber(&WatchDogNP, "Invalid watch dog time");
		
        return true;
	}

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);

}


/**************************************************************************************
**
***************************************************************************************/
bool MaxDomeII::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	// ignore if not ours //
    if (strcmp (getDeviceName(), dev))
        return false;

	nTimeSinceLastCommunication = 0;    

	// ===================================
    // Shutter
	// ===================================
	if (!strcmp(name, ShutterSP.name))
	{
		int error;
		int nRetry = 3;
		
	    if (IUUpdateSwitch(&ShutterSP, states, names, n) < 0)
            return false;

		if (ShutterS[0].s == ISS_ON)
		{ // Open Shutter
            while (nRetry)
            {
				error = Open_Shutter_MaxDomeII(fd);
				handle_driver_error(&error, &nRetry);
			}
			nTimeSinceShutterStart = 0;	// Init movement timer
			if (error)
			{
				IDLog("MAX DOME II: %s",ErrorMessages[-error]);
				ShutterSP.s = IPS_ALERT;
				IDSetSwitch(&ShutterSP, "Error opening shutter");
                return false;
			}
			ShutterSP.s = IPS_BUSY;
			IDSetSwitch(&ShutterSP, "Opening shutter");
		}
		else if (ShutterS[1].s == ISS_ON)
		{ // Open upper shutter only
            while (nRetry)
            {
				error = Open_Upper_Shutter_Only_MaxDomeII(fd);
				handle_driver_error(&error, &nRetry);
			}
			nTimeSinceShutterStart = 0;	// Init movement timer
			if (error)
			{
				IDLog("MAX DOME II: %s",ErrorMessages[-error]);
				ShutterSP.s = IPS_ALERT;
				IDSetSwitch(&ShutterSP, "Error opening upper shutter only");
                return false;
			}
			ShutterSP.s = IPS_BUSY;
			IDSetSwitch(&ShutterSP, "Opening upper shutter");
		}
		else if (ShutterS[2].s == ISS_ON)
		{ // Close Shutter
            while (nRetry)
            {
				error = Close_Shutter_MaxDomeII(fd);
				handle_driver_error(&error, &nRetry);
			}
			nTimeSinceShutterStart = 0;	// Init movement timer
			if (error)
			{
				IDLog("MAX DOME II: %s",ErrorMessages[-error]);
				ShutterSP.s = IPS_ALERT;
				IDSetSwitch(&ShutterSP, "Error closing shutter");
                return false;
			}
			ShutterSP.s = IPS_BUSY;
			IDSetSwitch(&ShutterSP, "Closing shutter");
		}
		
        return true;
	}

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
	  	  
}


/**************************************************************************************
**
***************************************************************************************/
int MaxDomeII::AzimuthDistance(int nPos1, int nPos2)
{
	int nDif;
	
	nDif = fabs(nPos1 - nPos2);
	if (nDif > nTicksPerTurn / 2)
		nDif = nTicksPerTurn - nDif;
		
	return nDif;	
}

/**************************************************************************************
 **
 ***************************************************************************************/
double MaxDomeII::TicksToAzimuth(int nTicks)
{
	double nAz;
	
	nAz = nHomeAzimuth + nTicks * 360.0 / nTicksPerTurn;
	while (nAz < 0) nAz += 360;
	while (nAz >= 360) nAz -= 360;
	
	return nAz;
}
/**************************************************************************************
 **
 ***************************************************************************************/
int MaxDomeII::AzimuthToTicks(double nAzimuth)
{
	int nTicks;
	
	nTicks = floor(0.5 + (nAzimuth - nHomeAzimuth) * nTicksPerTurn / 360.0);
	while (nTicks > nTicksPerTurn) nTicks -= nTicksPerTurn;
	while (nTicks < 0) nTicks += nTicksPerTurn;
	
	return nTicks;
}

/**************************************************************************************
 **
 ***************************************************************************************/

int MaxDomeII::handle_driver_error(int *error, int *nRetry)
{	
	(*nRetry)--;
	switch (*error) {
		case 0:	// No error. Continue
			*nRetry = 0;
			break;
		case -5: // can't connect
			// This error can happen if port connection is lost, i.e. a usb-serial reconnection 
			// Reconnect
			IDLog("MAX DOME II: Reconnecting ...");
			Connect();
			if (fd < 0)
				*nRetry = 0; // Can't open the port. Don't retry anymore. 
			break;
			
		default:  // Do nothing in all other errors.
			break;
	}
	
	return *nRetry;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState MaxDomeII::Park()
{
    int error=0;
    int nRetry = 3;

    double nVal = AzimuthToTicks(GetAxis1Park());

    // Only update park position if there is change
    if (nVal != nParkPosition)
    {
        while (nRetry)
        {
            error =SetPark_MaxDomeII(fd, nCloseShutterBeforePark, nVal);
            handle_driver_error(&error,&nRetry);
        }
        if (error >= 0)
        {
            nParkPosition = nVal;
            DEBUG(INDI::Logger::DBG_SESSION, "New park position set.");
        }
        else
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "MAX DOME II: %s",ErrorMessages[-error]);
            return IPS_ALERT;
        }
    }

    return MoveAbs(nParkPosition);
}

/************************************************************************************
 * Anything to do to unpark? Need to check!
* ***********************************************************************************/
IPState MaxDomeII::UnPark()
{
    return IPS_OK;
}

/************************************************************************************
 *
* ***********************************************************************************/
void MaxDomeII::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosN[0].value);
}
/************************************************************************************
 *
* ***********************************************************************************/

void MaxDomeII::SetDefaultPark()
{
    // By default set position to 0
    SetAxis1Park(0);
}
