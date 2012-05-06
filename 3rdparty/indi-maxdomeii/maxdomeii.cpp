/*
    MaxDome II 
    Copyright (C) 2009 Ferran Casarramona (ferran.casarramona@gmail.com)

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <memory>

/* INDI Common Library Routines */
#include "indicom.h"

/* MaxDome II Command Set */
#include "maxdomeiidriver.h"

/* Our driver header */
#include "maxdomeii.h"

using namespace std;

/* Our dome auto pointer */
auto_ptr<MaxDomeII> dome(0);

const int POLLMS = 1000;				// Period of update, 1 second.
const char *mydev = "MaxDome II";		// Name of our device.

const char *BASIC_GROUP    = "Main Control";		// Main Group
const char *OPTIONS_GROUP  = "Options";			// Options Group
const char *OPERATION_GROUP  = "Operation";			// Operation Group

static void ISPoll(void *);

/**************************************************************************************
** Send client definitions of all properties.
***************************************************************************************/
void ISInit()
{
 static int isInit=0;

 if (isInit)
  return;

 if (dome.get() == 0) dome.reset(new MaxDomeII());

 isInit = 1;
 
 IEAddTimer (POLLMS, ISPoll, NULL);
}

/**************************************************************************************
**
***************************************************************************************/
void ISGetProperties (const char *dev)
{
 ISInit(); 
 dome->ISGetProperties(dev);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
 ISInit();
 dome->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
 ISInit();
 dome->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
 ISInit();
 dome->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISPoll (void *p)
{
 INDI_UNUSED(p);

 dome->ISPoll(); 
 IEAddTimer (POLLMS, ISPoll, NULL);
}

/**************************************************************************************
**
***************************************************************************************/
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

/**************************************************************************************
**
***************************************************************************************/
void ISSnoopDevice (XMLEle *root) 
{
  INDI_UNUSED(root);
}

/**************************************************************************************
** MaxDomeII constructor
***************************************************************************************/
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
    init_properties();

    IDLog("Initilizing from MaxDome II device...\n");
    IDLog("Driver Version: 2012-05-06\n"); 
}

/**************************************************************************************
**
***************************************************************************************/
MaxDomeII::~MaxDomeII()
{
	if (fd)
		Disconnect_MaxDomeII(fd);
}

/**************************************************************************************
** Initialize all properties & set default values.
***************************************************************************************/
void MaxDomeII::init_properties()
{
    // Connection
    IUFillSwitch(&ConnectS[0], "CONNECT", "Connect", ISS_OFF);
    IUFillSwitch(&ConnectS[1], "DISCONNECT", "Disconnect", ISS_ON);
    IUFillSwitchVector(&ConnectSP, ConnectS, NARRAY(ConnectS), mydev, "CONNECTION", "Connection", BASIC_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Type of movement
    IUFillSwitch(&OnMovementTypeS[0], "FREE_MOVE", "Free Move", ISS_ON);
    IUFillSwitch(&OnMovementTypeS[1], "WIRED_MOVE", "Wired Move", ISS_OFF);
    IUFillSwitchVector(&OnMovementTypeSP, OnMovementTypeS, NARRAY(OnMovementTypeS), mydev, "ON_Movement_TYPE", "Movement type", OPERATION_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Abort
    IUFillSwitch(&AbortS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&AbortSP, AbortS, NARRAY(AbortS), mydev, "ABORT_MOTION", "Abort Slew/Track", OPERATION_GROUP, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

	 // Park
    IUFillSwitch(&ParkS[0], "PARK", "Park", ISS_OFF);
    IUFillSwitchVector(&ParkSP, ParkS, NARRAY(ParkS), mydev, "PARK_MOTION", "Park dome", OPERATION_GROUP, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
	
	 // Home
    IUFillSwitch(&HomeS[0], "HOME", "Home", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, NARRAY(HomeS), mydev, "HOME_MOTION", "Home dome", OPERATION_GROUP, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Port
    IUFillText(&PortT[0], "PORT", "Port", "/dev/cu.usbserial-A1000my4");
    IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "DEVICE_PORT", "Ports", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

	// Azimuth - SET
    IUFillNumber(&AzimuthWN[0], "GO_TO_AZIMUTH", "Go to Azimuth", "%5.2f",  0., 360., 0., 0.);
    IUFillNumberVector(&AzimuthWNP, AzimuthWN, NARRAY(AzimuthWN), mydev, "AZIMUTH_SET" , "Azimuth set", OPERATION_GROUP, IP_RW, 0, IPS_IDLE);

	// Azimuth - GET
    IUFillNumber(&AzimuthRN[0], "AZIMUTH", "Azimuth", "%5.2f",  0., 360., 0., 0.);
    IUFillNumberVector(&AzimuthRNP, AzimuthRN, NARRAY(AzimuthRN), mydev, "AZIMUTH_GET" , "Azimuth get", OPERATION_GROUP, IP_RO, 0, IPS_IDLE);

	// Home position - GET (for debug purpouses)
    IUFillNumber(&HomePosRN[0], "HOME_POS", "Home Position", "%5.2f",  0., 360., 0., 0.);
    IUFillNumberVector(&HomePosRNP, HomePosRN, NARRAY(HomePosRN), mydev, "HOME_POS_GET" , "Home Position", OPERATION_GROUP, IP_RO, 0, IPS_IDLE);
	
	// Ticks per turn
	TicksPerTurnN[0].value = nTicksPerTurn;
	IUFillNumber(&TicksPerTurnN[0], "TICKS_PER_TURN", "Ticks per turn", "%d",  0., 360., 0., 0.);
    IUFillNumberVector(&TicksPerTurnNP, TicksPerTurnN, NARRAY(TicksPerTurnN), mydev, "TICKS_PER_TURN" , "Ticks per turn", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);

	// Park position
	ParkPositionN[0].value = nParkPosition;
	IUFillNumber(&ParkPositionN[0], "PARK_POS", "Park position", "%5.2f",  0., 360., 0., 0.);
    IUFillNumberVector(&ParkPositionNP, ParkPositionN, NARRAY(ParkPositionN), mydev, "PARK_POSITION" , "Park position", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);

	// Home Azimuth
	IUFillNumber(&HomeAzimuthN[0], "HOME_AZIMUTH", "Home azimuth", "%5.2f",  0., 360., 0., 0.);
    IUFillNumberVector(&HomeAzimuthNP, HomeAzimuthN, NARRAY(HomeAzimuthN), mydev, "HOME_AZIMUTH" , "Home azimuth", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);


	// Park on shutter
    IUFillSwitch(&ParkOnShutterS[0], "PARK", "Park", ISS_ON);
    IUFillSwitch(&ParkOnShutterS[1], "NO_PARK", "No park", ISS_OFF);
    IUFillSwitchVector(&ParkOnShutterSP, ParkOnShutterS, NARRAY(ParkOnShutterS), mydev, "PARK_ON_SHUTTER", "Park before operating shutter", OPTIONS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

	// Shutter - SET
    IUFillSwitch(&ShutterS[0], "OPEN_SHUTTER", "Open shutter", ISS_OFF);
	IUFillSwitch(&ShutterS[1], "OPEN_UPPER_SHUTTER", "Open upper shutter", ISS_OFF);
    IUFillSwitch(&ShutterS[2], "CLOSE_SHUTTER", "Close shutter", ISS_ON);
    IUFillSwitchVector(&ShutterSP, ShutterS, NARRAY(ShutterS), mydev, "SHUTTER", "Shutter", OPERATION_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
	
	// Watch Dog
    IUFillNumber(&WatchDogN[0], "WATCH_DOG_TIME", "Watch dog time", "%5.2f",  0., 3600., 0., 0.);
    IUFillNumberVector(&WachDogNP, WatchDogN, NARRAY(WatchDogN), mydev, "WATCH_DOG_TIME_SET" , "Watch dog time set", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);
}

/**************************************************************************************
** Set all properties to idle and reset most switches to clean state.
***************************************************************************************/
void MaxDomeII::reset_all_properties()
{
    ConnectSP.s				= IPS_IDLE;
    OnMovementTypeSP.s		= IPS_IDLE; 
    AbortSP.s				= IPS_IDLE;
    PortTP.s				= IPS_IDLE;
    ParkSP.s				= IPS_IDLE;
	HomeSP.s				= IPS_IDLE;
    AzimuthWNP.s			= IPS_IDLE;
    AzimuthRNP.s			= IPS_IDLE;
    TicksPerTurnNP.s		= IPS_IDLE;
    ParkPositionNP.s		= IPS_IDLE;
	ParkOnShutterSP.s		= IPS_IDLE;
	ShutterSP.s				= IPS_IDLE;
	WachDogNP.s				= IPS_IDLE;

    IUResetSwitch(&AbortSP);

	ConnectS[0].s = ISS_OFF;
    ConnectS[1].s = ISS_ON;

    IDSetSwitch(&ConnectSP, NULL);
    IDSetSwitch(&AbortSP, NULL);
	IDSetSwitch(&OnMovementTypeSP, NULL);
	IDSetSwitch(&ParkSP, NULL);
	IDSetSwitch(&HomeSP, NULL);
	IDSetSwitch(&ParkOnShutterSP, NULL);
	IDSetSwitch(&ShutterSP, NULL);
    IDSetText(&PortTP, NULL);
    IDSetNumber(&AzimuthWNP, NULL);
    IDSetNumber(&AzimuthRNP, NULL);
    IDSetNumber(&TicksPerTurnNP, NULL);
    IDSetNumber(&ParkPositionNP, NULL);
	IDSetNumber(&WachDogNP, NULL);
}

/**************************************************************************************
** Define properties to clients.
***************************************************************************************/
void MaxDomeII::ISGetProperties(const char *dev)
{

 if (dev && strcmp (mydev, dev))
    return;

  // Basic Group
  IDDefSwitch(&ConnectSP, NULL);
  IDDefText(&PortTP, NULL);
  // Operation group 
  IDDefSwitch(&HomeSP, NULL);
  IDDefSwitch(&OnMovementTypeSP, NULL);
  IDDefNumber(&AzimuthWNP, NULL);
  IDDefNumber(&AzimuthRNP, NULL);
  IDDefSwitch(&ShutterSP, NULL);
  IDDefSwitch(&ParkSP, NULL);
  IDDefSwitch(&AbortSP, NULL);

  // Options
  IDDefNumber(&TicksPerTurnNP, NULL);
  IDDefNumber(&ParkPositionNP, NULL);
  IDDefSwitch(&ParkOnShutterSP, NULL);
  IDDefNumber(&WachDogNP, NULL);
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
void MaxDomeII::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Ignore if not ours 
	if (strcmp (dev, mydev))
	    return;

	nTimeSinceLastCommunication = 0;
	// ===================================
	// Port Name
	// ===================================
	if (!strcmp(name, PortTP.name) )
	{
	  if (IUUpdateText(&PortTP, texts, names, n) < 0)
		return;

	  PortTP.s = IPS_OK;
	  IDSetText (&PortTP, NULL);
	  return;
	}

}

/**************************************************************************************
**
***************************************************************************************/
int MaxDomeII::GotoAzimuth(double newAZ)
{
	double currAZ = 0;
	int newPos = 0, nDir = 0;
	int error;
		
	currAZ = AzimuthRN[0].value;
	
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
	error = Goto_Azimuth_MaxDomeII(fd, nDir, newPos);
	nTargetAzimuth = newPos;
	nTimeSinceAzimuthStart = 0;	// Init movement timer
		
	return error;
}

/**************************************************************************************
**
***************************************************************************************/
void MaxDomeII::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	
	// Ignore if not ours
	if (strcmp (dev, mydev))
	    return;
	
	nTimeSinceLastCommunication = 0;

	if (is_connected() == false)
        {
		IDMessage(mydev, "MaxDome II is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

	// ===================================
    // Azimuth
	// ===================================
	if (!strcmp (name, AzimuthWNP.name))
	{
		double newAZ = 0;
		int error;
		
		if (IUUpdateNumber(&AzimuthWNP, values, names, n) < 0)
			return;
			
		newAZ = values[0];
		if (newAZ >= 0.0 && newAZ <= 360.0)
		{ // Correct Azimuth
		    error = GotoAzimuth(newAZ);
			if (error < 0)
			{
				IDLog("MAX DOME II: %s",ErrorMessages[-error]);
				AzimuthWNP.s = IPS_ALERT;
                                IDSetNumber(&AzimuthWNP, "%s", ErrorMessages[-error]);
			}
			else
			{
				AzimuthWNP.np[0].value = newAZ;
				AzimuthWNP.s = IPS_BUSY;
                                IDSetNumber(&AzimuthWNP, NULL);
			}
		}
		else
		{ // Wrong azimuth
			IDLog("MAX DOME II: Invalid azimuth");
			AzimuthWNP.s = IPS_ALERT;
			IDSetNumber(&AzimuthWNP, "Invalid azimuth");
		}
		return;
	} // End Azimuth command
	
	
	// ===================================
    // TicksPerTurn
	// ===================================
	if (!strcmp (name, TicksPerTurnNP.name))
	{
		double nVal;
		char cLog[255];
		
		if (IUUpdateNumber(&TicksPerTurnNP, values, names, n) < 0)
			return;
			
		nVal = values[0];
		if (nVal >= 100 && nVal <=10000)
		{
			sprintf(cLog, "New Ticks Per Turn set: %lf", nVal);
			nTicksPerTurn = nVal;
			nHomeTicks = floor(0.5 + nHomeAzimuth * nTicksPerTurn / 360.0); // Calculate Home ticks again
			TicksPerTurnNP.s = IPS_OK;
			TicksPerTurnNP.np[0].value = nVal;
                        IDSetNumber(&TicksPerTurnNP, "%s", cLog);
			return;
		}
		// Incorrect value. 
		TicksPerTurnNP.s = IPS_ALERT;
		IDSetNumber(&TicksPerTurnNP, "Invalid Ticks Per Turn");
		
		return;
	}
	
	// ===================================
    // Park position
	// ===================================
	if (!strcmp (name, ParkPositionNP.name))
	{
		double nVal;
		int error;
		
		if (IUUpdateNumber(&ParkPositionNP, values, names, n) < 0)
			return;
			
		nVal = values[0];
		if (nVal >= 0 && nVal <= nTicksPerTurn)
		{
			error = SetPark_MaxDomeII(fd, nCloseShutterBeforePark, nVal);
			if (error >= 0)
			{
				nParkPosition = nVal;
				ParkPositionNP.s = IPS_OK;
				ParkPositionNP.np[0].value = nVal;
				IDSetNumber(&ParkPositionNP, "New park position set");
			}
			else
			{
				IDLog("MAX DOME II: %s",ErrorMessages[-error]);
				ParkPositionNP.s = IPS_ALERT;
                                IDSetNumber(&ParkPositionNP, "%s", ErrorMessages[-error]);
			}
			
			return;
		}
		// Incorrect value.
		ParkPositionNP.s = IPS_ALERT;
		IDSetNumber(&ParkPositionNP, "Invalid park position");
		
		return;
	}
	
	// ===================================
    // HomeAzimuth
	// ===================================
	if (!strcmp (name, HomeAzimuthNP.name))
	{
		double nVal;
		char cLog[255];
		
		if (IUUpdateNumber(&HomeAzimuthNP, values, names, n) < 0)
			return;
			
		nVal = values[0];
		if (nVal >= 0 && nVal <=360)
		{
			sprintf(cLog, "New home azimuth set: %lf", nVal);
			nHomeAzimuth = nVal;
			nHomeTicks = floor(0.5 + nHomeAzimuth * nTicksPerTurn / 360.0);
			HomeAzimuthNP.s = IPS_OK;
			HomeAzimuthNP.np[0].value = nVal;
                        IDSetNumber(&HomeAzimuthNP, "%s", cLog);
			return;
		}
		// Incorrect value. 
		HomeAzimuthNP.s = IPS_ALERT;
		IDSetNumber(&HomeAzimuthNP, "Invalid home azimuth");
		
		return;
	}
	
	// ===================================
    // Watch dog
	// ===================================
	if (!strcmp (name, WachDogNP.name))
	{
		double nVal;
		char cLog[255];
		
		if (IUUpdateNumber(&WachDogNP, values, names, n) < 0)
			return;
		
		nVal = values[0];
		if (nVal >= 0 && nVal <=3600)
		{
			sprintf(cLog, "New watch dog set: %lf", nVal);
			WachDogNP.s = IPS_OK;
			WachDogNP.np[0].value = nVal;
                        IDSetNumber(&WachDogNP, "%s", cLog);
			return;
		}
		// Incorrect value. 
		WachDogNP.s = IPS_ALERT;
		IDSetNumber(&WachDogNP, "Invalid watch dog time");
		
		return;
	}

}

/**************************************************************************************
**
***************************************************************************************/
void MaxDomeII::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	// ignore if not ours //
	if (strcmp (mydev, dev))
	    return;

	nTimeSinceLastCommunication = 0;
	// ===================================
    // Connect Switch
	// ===================================
	if (!strcmp (name, ConnectSP.name))
	{
	    if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0)
		return;
		
		int error;

		if (fd < 0 && ConnectS[0].s == ISS_ON)
		{
			ConnectSP.s = IPS_BUSY;
			IDSetSwitch (&ConnectSP, "Openning port ...");
			IDLog("MAX DOME II: Openning port ...");
			fd = Connect_MaxDomeII(PortT[0].text);
			if (fd < 0)
			{
				ConnectS[0].s = ISS_OFF;
				ConnectS[1].s = ISS_ON;
				ConnectSP.s = IPS_ALERT;
				IDSetSwitch (&ConnectSP, "Error connecting to port %s. Make sure you have BOTH write and read permission to your port.\n", PortT[0].text);
				return;
			}
			IDSetSwitch (&ConnectSP, "Connecting ...");
			IDLog("MAX DOME II: Connecting ...");
			error = Ack_MaxDomeII(fd);
			if (error)
			{
				IDLog("MAX DOME II: %s",ErrorMessages[-error]);
				ConnectS[0].s = ISS_OFF;
				ConnectS[1].s = ISS_ON;
				ConnectSP.s = IPS_ALERT;
                                IDSetSwitch (&ConnectSP, "%s", ErrorMessages[-error]);//"Error connecting to Dome. Dome is offline.");
				Disconnect_MaxDomeII(fd);
				fd = -1;
				return;
			}
			
			ConnectSP.s = IPS_OK;
			IDSetSwitch (&ConnectSP, "Dome is online.");
			IDLog("Dome is online.");
		} 
		else if (fd >= 0 &&  ConnectS[1].s == ISS_ON)
		{
			Disconnect_MaxDomeII(fd);
			fd = -1;
			ConnectS[0].s = ISS_OFF;
			ConnectS[1].s = ISS_ON;
			ConnectSP.s = IPS_IDLE;
			IDSetSwitch (&ConnectSP, "Dome is offline.");
			IDLog("Dome is offline.");
		}
	
	    return;
	}

	if (fd < 0)
	{
		IDMessage(mydev, "MaxDome II is offline. Please connect before issuing any commands.");
		return;
	}

	// ===================================
    // Abort all
	// ===================================
	if (!strcmp (name, AbortSP.name))
	{

		IUResetSwitch(&AbortSP);
		Abort_Azimuth_MaxDomeII(fd);
		Abort_Shutter_MaxDomeII(fd);

	   if (AzimuthWNP.s == IPS_BUSY)
	    {
		AbortSP.s = IPS_OK;
		AzimuthWNP.s       = IPS_IDLE;
        AzimuthRNP.s       = IPS_IDLE;
		IDSetSwitch(&AbortSP, "Movement aborted.");
		IDSetNumber(&AzimuthWNP, NULL);
		IDSetNumber(&AzimuthRNP, NULL);
        }

	    return;
	}

	// ===================================
    // Shutter
	// ===================================
	if (!strcmp(name, ShutterSP.name))
	{
		int error;
		
	    if (IUUpdateSwitch(&ShutterSP, states, names, n) < 0)
			return;

		if (ShutterS[0].s == ISS_ON)
		{ // Open Shutter
			error = Open_Shutter_MaxDomeII(fd);
			nTimeSinceShutterStart = 0;	// Init movement timer
			if (error)
			{
				IDLog("MAX DOME II: %s",ErrorMessages[-error]);
				ShutterSP.s = IPS_ALERT;
				IDSetSwitch(&ShutterSP, "Error opening shutter");
				return;
			}
			ShutterSP.s = IPS_BUSY;
			IDSetSwitch(&ShutterSP, "Opening shutter");
		}
		else if (ShutterS[1].s == ISS_ON)
		{ // Open upper shutter only
			error = Open_Upper_Shutter_Only_MaxDomeII(fd);
			nTimeSinceShutterStart = 0;	// Init movement timer
			if (error)
			{
				IDLog("MAX DOME II: %s",ErrorMessages[-error]);
				ShutterSP.s = IPS_ALERT;
				IDSetSwitch(&ShutterSP, "Error opening upper shutter only");
				return;
			}
			ShutterSP.s = IPS_BUSY;
			IDSetSwitch(&ShutterSP, "Opening upper shutter");
		}
		else if (ShutterS[2].s == ISS_ON)
		{ // Close Shutter
			error = Close_Shutter_MaxDomeII(fd);
			nTimeSinceShutterStart = 0;	// Init movement timer
			if (error)
			{
				IDLog("MAX DOME II: %s",ErrorMessages[-error]);
				ShutterSP.s = IPS_ALERT;
				IDSetSwitch(&ShutterSP, "Error closing shutter");
				return;
			}
			ShutterSP.s = IPS_BUSY;
			IDSetSwitch(&ShutterSP, "Closing shutter");
		}
		
		return;
	}
	  
	// ===================================
    // Home
	// ===================================
	if (!strcmp(name, HomeSP.name))
	{
		if (IUUpdateSwitch(&HomeSP, states, names, n) < 0)
			return;
			
		int error;
		
		error = Home_Azimuth_MaxDomeII(fd);
		nTimeSinceAzimuthStart = 0;
		nTargetAzimuth = -1;
		if (error)
		{
			IDLog("MAX DOME II: %s",ErrorMessages[-error]);
			HomeSP.s = IPS_ALERT;
			IDSetSwitch(&HomeSP, "Error Homing Azimuth");
			return;
		}
		HomeSP.s = IPS_BUSY;
		IDSetSwitch(&HomeSP, "Homing dome");
		
		return;
	}
	
	  
	// ===================================
    // Park
	// ===================================
	if (!strcmp(name, ParkSP.name))
	{
		if (IUUpdateSwitch(&ParkSP, states, names, n) < 0)
			return;
			
		int error;
				
		error = GotoAzimuth(nParkPosition);
		if (error)
		{
			IDLog("MAX DOME II: %s",ErrorMessages[-error]);
			ParkSP.s = IPS_ALERT;
			IDSetSwitch(&ParkSP, "Error Parking");
			return;
		}
		ParkSP.s = IPS_BUSY;
		IDSetSwitch(&ParkSP, "Parking dome");
		
		return;
	}

}

/**************************************************************************************
**
***************************************************************************************/
bool MaxDomeII::is_connected()
{
   return (ConnectSP.sp[0].s == ISS_ON);
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
void MaxDomeII::ISPoll()
{	
    char buf[32];
    
	if (is_connected() == false)
	 return;

	SH_Status nShutterStatus;
	AZ_Status nAzimuthStatus;
	unsigned nHomePosition;
	float nAz;
	int nError;

	nError = Status_MaxDomeII(fd, &nShutterStatus, &nAzimuthStatus, &nCurrentTicks, &nHomePosition);

	// Increment movment time counter
	if (nTimeSinceShutterStart >= 0)
		nTimeSinceShutterStart++;
	if (nTimeSinceAzimuthStart >= 0)
		nTimeSinceAzimuthStart++;
	
	// Watch dog
	nTimeSinceLastCommunication++;
	if (WachDogNP.np[0].value > 0 && WachDogNP.np[0].value >= nTimeSinceLastCommunication)
	{		
		// Close Shutter if it is not
		if (nShutterStatus != Ss_CLOSED)
		{
			int error;
			
			error = Close_Shutter_MaxDomeII(fd);
			nTimeSinceShutterStart = 0;	// Init movement timer
			if (error)
			{
				IDLog("MAX DOME II: %s",ErrorMessages[-error]);
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
				IDSetSwitch (&ShutterSP, "Unexpexted shutter opening");
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
				IDSetSwitch (&ShutterSP, "Unexpexted shutter closing");
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
				IDSetSwitch (&ShutterSP, "Unknow shutter status");
			}
			break;
		}
		
		if (HomePosRN[0].value != nAzimuthStatus)//nHomePosition)
		{	// Only refresh position if it changed
			HomePosRN[0].value = nAzimuthStatus; //nHomePosition;
            //sprintf(buf,"%d", nHomePosition);
			IDSetNumber(&HomePosRNP, NULL);
		}
		
		// Azimuth
		nAz = TicksToAzimuth(nCurrentTicks);
		if (AzimuthRN[0].value != nAz)
		{	// Only refresh position if it changed
			AzimuthRN[0].value = nAz;
            sprintf(buf,"%d", nCurrentTicks);
			IDSetNumber(&AzimuthRNP, buf);
		}
		
		switch(nAzimuthStatus)
		{
		case As_IDLE:
		case As_IDEL2:
			if (nTimeSinceAzimuthStart > 3)
			{
				if (nTargetAzimuth >= 0 && AzimuthDistance(nTargetAzimuth,nCurrentTicks) > 3) // Maximum difference allowed: 3 ticks
				{
					AzimuthWNP.s = IPS_ALERT;
					nTimeSinceAzimuthStart = -1;
					IDSetNumber(&AzimuthWNP, "Could not position right");
				}
				else
				{   // Succesfull end of movement				
					if (AzimuthWNP.s != IPS_OK)
					{
						AzimuthWNP.s = IPS_OK;
						nTimeSinceAzimuthStart = -1;
						IDSetNumber(&AzimuthWNP, "Dome is on target position");
					}
					if (ParkS[0].s == ISS_ON)
					{
						ParkS[0].s = ISS_OFF;
						ParkSP.s = IPS_OK;
						nTimeSinceAzimuthStart = -1;
						IDSetSwitch(&ParkSP, "Dome is parked");
					}
					if (HomeS[0].s == ISS_ON)
					{
						HomeS[0].s = ISS_OFF;
						HomeSP.s = IPS_OK;
						nTimeSinceAzimuthStart = -1;
						IDSetSwitch(&HomeSP, "Dome is homed");
					}
				}
			}
			break;
		case As_MOVING_WE:
		case As_MOVING_EW:
			if (nTimeSinceAzimuthStart < 0)
			{
				nTimeSinceAzimuthStart = 0;
				nTargetAzimuth = -1;
				AzimuthWNP.s = IPS_ALERT;
				IDSetNumber(&AzimuthWNP, "Unexpected dome moving");
			}
			break;
		case As_ERROR:
			if (nTimeSinceAzimuthStart >= 0)
			{
				AzimuthWNP.s = IPS_ALERT;
				nTimeSinceAzimuthStart = -1;
				nTargetAzimuth = -1;
				IDSetNumber(&AzimuthWNP, "Dome Error");
			}
		default:
			break;
		}
		
		
	}
	else
	{
		IDLog("MAX DOME II: %s",ErrorMessages[-nError]);
		ConnectSP.s = IPS_ALERT;
                IDSetSwitch (&ConnectSP,  "%s", ErrorMessages[-nError]);
	}
	
}

/**************************************************************************************
**
***************************************************************************************/
int MaxDomeII::get_switch_index(ISwitchVectorProperty *sp)
{
 for (int i=0; i < sp->nsp ; i++)
     if (sp->sp[i].s == ISS_ON)
      return i;

 return -1;
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


