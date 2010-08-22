#if 0
    LX200 Basic Driver
    Copyright (C) 2005 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <memory>

#include <config.h>

/* INDI Common Library Routines */
#include "indicom.h"

/* LX200 Command Set */
#include "lx200driver.h"

/* Our driver header */
#include "lx200basic.h"

using namespace std;

/* Our telescope auto pointer */
auto_ptr<LX200Basic> telescope(0);

const int POLLMS = 1000;				// Period of update, 1 second.
const char *mydev = "LX200 Basic";			// Name of our device.

const char *BASIC_GROUP    = "Main Control";		// Main Group
const char *OPTIONS_GROUP  = "Options";			// Options Group

/* Handy Macros */
#define currentRA	EquatorialCoordsRN[0].value
#define currentDEC	EquatorialCoordsRN[1].value
#define targetRA	EquatorialCoordsWN[0].value
#define targetDEC	EquatorialCoordsWN[1].value

static void ISPoll(void *);
static void retry_connection(void *);

/**************************************************************************************
** Send client definitions of all properties.
***************************************************************************************/
void ISInit()
{
 static int isInit=0;

 if (isInit)
  return;

 if (telescope.get() == 0) telescope.reset(new LX200Basic());

 isInit = 1;
 
 IEAddTimer (POLLMS, ISPoll, NULL);
}

/**************************************************************************************
**
***************************************************************************************/
void ISGetProperties (const char *dev)
{
 ISInit(); 
 telescope->ISGetProperties(dev);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
 ISInit();
 telescope->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
 ISInit();
 telescope->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
 ISInit();
 telescope->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISPoll (void *p)
{
 INDI_UNUSED(p);

 telescope->ISPoll(); 
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
** LX200 Basic constructor
***************************************************************************************/
LX200Basic::LX200Basic()
{
   init_properties();

   lastSet        = -1;
   fd             = -1;
   simulation     = false;
   lastRA 	  = 0;
   lastDEC	  = 0;
   currentSet     = 0;

   IDLog("Initilizing from LX200 Basic device...\n");
   IDLog("Driver Version: 2007-09-28\n");
 
   enable_simulation(false);  
}

/**************************************************************************************
**
***************************************************************************************/
LX200Basic::~LX200Basic()
{

}

/**************************************************************************************
** Initialize all properties & set default values.
***************************************************************************************/
void LX200Basic::init_properties()
{
    // Connection
    IUFillSwitch(&ConnectS[0], "CONNECT", "Connect", ISS_OFF);
    IUFillSwitch(&ConnectS[1], "DISCONNECT", "Disconnect", ISS_ON);
    IUFillSwitchVector(&ConnectSP, ConnectS, NARRAY(ConnectS), mydev, "CONNECTION", "Connection", BASIC_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Coord Set
    IUFillSwitch(&OnCoordSetS[0], "SLEW", "Slew", ISS_ON);
    IUFillSwitch(&OnCoordSetS[1], "TRACK", "Track", ISS_OFF);
    IUFillSwitch(&OnCoordSetS[2], "SYNC", "Sync", ISS_OFF);
    IUFillSwitchVector(&OnCoordSetSP, OnCoordSetS, NARRAY(OnCoordSetS), mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Abort
    IUFillSwitch(&AbortSlewS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&AbortSlewSP, AbortSlewS, NARRAY(AbortSlewS), mydev, "ABORT_MOTION", "Abort Slew/Track", BASIC_GROUP, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Port
    IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyS0");
    IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "DEVICE_PORT", "Ports", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

    // Object Name
    IUFillText(&ObjectT[0], "OBJECT_NAME", "Name", "--");
    IUFillTextVector(&ObjectTP, ObjectT, NARRAY(ObjectT), mydev, "OBJECT_INFO", "Object", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

    // Equatorial Coords - SET
    IUFillNumber(&EquatorialCoordsWN[0], "RA", "RA  H:M:S", "%10.6m",  0., 24., 0., 0.);
    IUFillNumber(&EquatorialCoordsWN[1], "DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0.);
    IUFillNumberVector(&EquatorialCoordsWNP, EquatorialCoordsWN, NARRAY(EquatorialCoordsWN), mydev, "EQUATORIAL_EOD_COORD_REQUEST" , "Equatorial JNow", BASIC_GROUP, IP_RW, 0, IPS_IDLE);

    // Equatorial Coords - READ
    IUFillNumber(&EquatorialCoordsRN[0], "RA", "RA  H:M:S", "%10.6m",  0., 24., 0., 0.);
    IUFillNumber(&EquatorialCoordsRN[1], "DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0.);
    IUFillNumberVector(&EquatorialCoordsRNP, EquatorialCoordsRN, NARRAY(EquatorialCoordsRN), mydev, "EQUATORIAL_EOD_COORD" , "Equatorial JNow", BASIC_GROUP, IP_RO, 0, IPS_IDLE);

    // Slew threshold
    IUFillNumber(&SlewAccuracyN[0], "SlewRA",  "RA (arcmin)", "%10.6m",  0., 60., 1., 3.0);
    IUFillNumber(&SlewAccuracyN[1], "SlewDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    IUFillNumberVector(&SlewAccuracyNP, SlewAccuracyN, NARRAY(SlewAccuracyN), mydev, "Slew Accuracy", "", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);

    // Track threshold
    IUFillNumber(&TrackAccuracyN[0], "TrackRA", "RA (arcmin)", "%10.6m",  0., 60., 1., 3.0);
    IUFillNumber(&TrackAccuracyN[1], "TrackDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    IUFillNumberVector(&TrackAccuracyNP, TrackAccuracyN, NARRAY(TrackAccuracyN), mydev, "Tracking Accuracy", "", OPTIONS_GROUP, IP_RW, 0, IPS_IDLE);
}

/**************************************************************************************
** Define LX200 Basic properties to clients.
***************************************************************************************/
void LX200Basic::ISGetProperties(const char *dev)
{

 if (dev && strcmp (mydev, dev))
    return;

  // Main Control
  IDDefSwitch(&ConnectSP, NULL);
  IDDefText(&PortTP, NULL);
  IDDefText(&ObjectTP, NULL);
  IDDefNumber(&EquatorialCoordsWNP, NULL);
  IDDefNumber(&EquatorialCoordsRNP, NULL);
  IDDefSwitch(&OnCoordSetSP, NULL);
  IDDefSwitch(&AbortSlewSP, NULL);

  // Options
  IDDefNumber(&SlewAccuracyNP, NULL);
  IDDefNumber(&TrackAccuracyNP, NULL);
  
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
void LX200Basic::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Ignore if not ours 
	if (strcmp (dev, mydev))
	    return;

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

	if (is_connected() == false)
        {
		IDMessage(mydev, "LX200 Basic is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

       // ===================================
       // Object Name
       // ===================================
       if (!strcmp (name, ObjectTP.name))
       {

	  if (IUUpdateText(&ObjectTP, texts, names, n) < 0)
		return;

          ObjectTP.s = IPS_OK;
          IDSetText(&ObjectTP, NULL);
          return;
       }
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	
	// Ignore if not ours
	if (strcmp (dev, mydev))
	    return;

	if (is_connected() == false)
        {
		IDMessage(mydev, "LX200 Basic is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

	// ===================================
        // Equatorial Coords
	// ===================================
	if (!strcmp (name, EquatorialCoordsWNP.name))
	{
	  int i=0, nset=0, error_code=0;
	  double newRA =0, newDEC =0;

	    for (nset = i = 0; i < n; i++)
	    {
		INumber *eqp = IUFindNumber (&EquatorialCoordsWNP, names[i]);
		if (eqp == &EquatorialCoordsWN[0])
		{
                    newRA = values[i];
		    nset += newRA >= 0 && newRA <= 24.0;
		} else if (eqp == &EquatorialCoordsWN[1])
		{
		    newDEC = values[i];
		    nset += newDEC >= -90.0 && newDEC <= 90.0;
		}
	    }

	  if (nset == 2)
	  {
	   char RAStr[32], DecStr[32];

	   fs_sexa(RAStr, newRA, 2, 3600);
	   fs_sexa(DecStr, newDEC, 2, 3600);
	  
           #ifdef INDI_DEBUG
	   IDLog("We received JNow RA %g - DEC %g\n", newRA, newDEC);
	   IDLog("We received JNow RA %s - DEC %s\n", RAStr, DecStr);
           #endif
	   
	   if (!simulation && ( (error_code = setObjectRA(fd, newRA)) < 0 || ( error_code = setObjectDEC(fd, newDEC)) < 0))
	   {
	     handle_error(&EquatorialCoordsWNP, error_code, "Setting RA/DEC");
	     return;
	   } 
	   
	   targetRA  = newRA;
	   targetDEC = newDEC;
	   
	   if (process_coords() == false)
	   {
	     EquatorialCoordsWNP.s = IPS_ALERT;
	     IDSetNumber(&EquatorialCoordsWNP, NULL);
	     
	   }
	} // end nset
	else
	{
		EquatorialCoordsWNP.s = IPS_ALERT;
		IDSetNumber(&EquatorialCoordsWNP, "RA or Dec missing or invalid");
	}

	    return;
     } /* end EquatorialCoordsWNP */

	// ===================================
        // Update tracking precision limits
	// ===================================
	if (!strcmp (name, TrackAccuracyNP.name))
	{
		if (IUUpdateNumber(&TrackAccuracyNP, values, names, n) < 0)
			return;

		TrackAccuracyNP.s = IPS_OK;

		if (TrackAccuracyN[0].value < 3 || TrackAccuracyN[1].value < 3)
			IDSetNumber(&TrackAccuracyNP, "Warning: Setting the tracking accuracy too low may result in a dead lock");
		else
			IDSetNumber(&TrackAccuracyNP, NULL);
		return;
	}

	// ===================================
        // Update slew precision limit
	// ===================================
	if (!strcmp(name, SlewAccuracyNP.name))
	{
		if (IUUpdateNumber(&SlewAccuracyNP, values, names, n) < 0)
			return;
		
		SlewAccuracyNP.s = IPS_OK;

		if (SlewAccuracyN[0].value < 3 || SlewAccuracyN[1].value < 3)
			IDSetNumber(&TrackAccuracyNP, "Warning: Setting the slew accuracy too low may result in a dead lock");

		IDSetNumber(&SlewAccuracyNP, NULL);
		return;
		

	}
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	// ignore if not ours //
	if (strcmp (mydev, dev))
	    return;

	// ===================================
        // Connect Switch
	// ===================================
	if (!strcmp (name, ConnectSP.name))
	{
	    if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0)
		return;

   	    connect_telescope();
	    return;
	}

	if (is_connected() == false)
        {
		IDMessage(mydev, "LX200 Basic is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

	// ===================================
        // Coordinate Set
	// ===================================
	if (!strcmp(name, OnCoordSetSP.name))
	{
	   if (IUUpdateSwitch(&OnCoordSetSP, states, names, n) < 0)
		return;

	  currentSet = get_switch_index(&OnCoordSetSP);
	  OnCoordSetSP.s = IPS_OK;
	  IDSetSwitch(&OnCoordSetSP, NULL);
	}
	  
	// ===================================
        // Abort slew
	// ===================================
	if (!strcmp (name, AbortSlewSP.name))
	{

	  IUResetSwitch(&AbortSlewSP);
	  abortSlew(fd);

	    if (EquatorialCoordsWNP.s == IPS_BUSY)
	    {
		AbortSlewSP.s = IPS_OK;
		EquatorialCoordsWNP.s       = IPS_IDLE;
                EquatorialCoordsRNP.s       = IPS_IDLE;
		IDSetSwitch(&AbortSlewSP, "Slew aborted.");
		IDSetNumber(&EquatorialCoordsWNP, NULL);
		IDSetNumber(&EquatorialCoordsRNP, NULL);
            }

	    return;
	}

}

/**************************************************************************************
** Retry connecting to the telescope on error. Give up if there is no hope.
***************************************************************************************/
void LX200Basic::handle_error(INumberVectorProperty *nvp, int err, const char *msg)
{
  
  nvp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (check_lx200_connection(fd))
    {
      /* The telescope is off locally */
      ConnectS[0].s = ISS_OFF;
      ConnectS[1].s = ISS_ON;
      ConnectSP.s = IPS_BUSY;
      IDSetSwitch(&ConnectSP, "Telescope is not responding to commands, will retry in 10 seconds.");
      
      IDSetNumber(nvp, NULL);
      IEAddTimer(10000, retry_connection, &fd);
      return;
    }
    
   /* If the error is a time out, then the device doesn't support this property */
      if (err == -2)
      {
       nvp->s = IPS_ALERT;
       IDSetNumber(nvp, "Device timed out. Current device may be busy or does not support %s. Will retry again.", msg);
      }
      else
    /* Changing property failed, user should retry. */
       IDSetNumber( nvp , "%s failed.", msg);
       
       fault = true;
}

/**************************************************************************************
** Set all properties to idle and reset most switches to clean state.
***************************************************************************************/
void LX200Basic::reset_all_properties()
{
    ConnectSP.s			= IPS_IDLE;
    OnCoordSetSP.s		= IPS_IDLE; 
    AbortSlewSP.s		= IPS_IDLE;
    PortTP.s			= IPS_IDLE;
    ObjectTP.s			= IPS_IDLE;
    EquatorialCoordsWNP.s	= IPS_IDLE;
    EquatorialCoordsRNP.s	= IPS_IDLE;
    SlewAccuracyNP.s		= IPS_IDLE;
    TrackAccuracyNP.s		= IPS_IDLE;

    IUResetSwitch(&OnCoordSetSP);
    IUResetSwitch(&AbortSlewSP);

    OnCoordSetS[0].s = ISS_ON;
    ConnectS[0].s = ISS_OFF;
    ConnectS[1].s = ISS_ON;

    IDSetSwitch(&ConnectSP, NULL);
    IDSetSwitch(&OnCoordSetSP, NULL);
    IDSetSwitch(&AbortSlewSP, NULL);
    IDSetText(&PortTP, NULL);
    IDSetText(&ObjectTP, NULL);
    IDSetNumber(&EquatorialCoordsWNP, NULL);
    IDSetNumber(&EquatorialCoordsRNP, NULL);
    IDSetNumber(&SlewAccuracyNP, NULL);
    IDSetNumber(&TrackAccuracyNP, NULL);
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::correct_fault()
{
  fault = false;
  IDMessage(mydev, "Telescope is online.");
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::is_connected()
{
  if (simulation) return true;
  
  return (ConnectSP.sp[0].s == ISS_ON);
}

/**************************************************************************************
**
***************************************************************************************/
static void retry_connection(void * p)
{
  int fd = *((int *) p);

  if (check_lx200_connection(fd))
	telescope->connection_lost();
  else
	telescope->connection_resumed();
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::ISPoll()
{	
	if (is_connected() == false || simulation)
	 return;

        double dx, dy;
	int error_code=0;

	switch (EquatorialCoordsWNP.s)
	{
		case IPS_IDLE:
		getLX200RA(fd, &currentRA);
		getLX200DEC(fd, &currentDEC);
	
		// Only update values if there are some interesting changes
	        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
		{
	        	lastRA = currentRA;
			lastDEC = currentDEC;
			IDSetNumber (&EquatorialCoordsRNP, NULL);
		}
        	break;

        	case IPS_BUSY:
	    	getLX200RA(fd, &currentRA);
		getLX200DEC(fd, &currentDEC);
	    	dx = targetRA - currentRA;
	    	dy = targetDEC - currentDEC;

	    	// Wait until acknowledged or within threshold
	    	if ( fabs(dx) <= (SlewAccuracyN[0].value/(900.0)) && fabs(dy) <= (SlewAccuracyN[1].value/60.0))
	    	{
		        lastRA  = currentRA;
		        lastDEC = currentDEC;
	       		IUResetSwitch(&OnCoordSetSP);
	       		OnCoordSetSP.s = IPS_OK;
	       		EquatorialCoordsWNP.s = IPS_OK;
			EquatorialCoordsRNP.s = IPS_OK;
	       		IDSetNumber (&EquatorialCoordsWNP, NULL);
			IDSetNumber (&EquatorialCoordsRNP, NULL);

			switch (currentSet)
			{
		  		case LX200_SLEW:
		  		OnCoordSetSP.sp[LX200_SLEW].s = ISS_ON;
		  		IDSetSwitch (&OnCoordSetSP, "Slew is complete.");
		  		break;
		  
		  		case LX200_TRACK:
		  		OnCoordSetSP.sp[LX200_TRACK].s = ISS_ON;
		  		IDSetSwitch (&OnCoordSetSP, "Slew is complete. Tracking...");
				break;
		  
		  		case LX200_SYNC:
		  		break;
			}
		  
	    	}
		else
			IDSetNumber (&EquatorialCoordsRNP, NULL);
	    	break;

		case IPS_OK:
	  
		if ( (error_code = getLX200RA(fd, &currentRA)) < 0 || (error_code = getLX200DEC(fd, &currentDEC)) < 0)
		{
	  		handle_error(&EquatorialCoordsRNP, error_code, "Getting RA/DEC");
	  		return;
		}
	
		if (fault == true)
	  		correct_fault();
	
		if ( (currentRA != lastRA) || (currentDEC != lastDEC))
		{
		  	lastRA  = currentRA;
			lastDEC = currentDEC;
			IDSetNumber (&EquatorialCoordsRNP, NULL);
		}
        	break;

		case IPS_ALERT:
	    	break;
	}
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::process_coords()
{

  int  error_code;
  char syncString[256];
  char RAStr[32], DecStr[32];
  double dx, dy;
  
  switch (currentSet)
  {

    // Slew
    case LX200_SLEW:
          lastSet = LX200_SLEW;
	  if (EquatorialCoordsWNP.s == IPS_BUSY)
	  {
	     IDLog("Aboring Slew\n");
	     abortSlew(fd);

	     // sleep for 100 mseconds
	     usleep(100000);
	  }

	  if ( !simulation && (error_code = Slew(fd)))
	  {
	    slew_error(error_code);
	    return false;
	  }

	  EquatorialCoordsWNP.s = IPS_BUSY;
	  EquatorialCoordsRNP.s = IPS_BUSY;
	  fs_sexa(RAStr, targetRA, 2, 3600);
	  fs_sexa(DecStr, targetDEC, 2, 3600);
	  IDSetNumber(&EquatorialCoordsWNP, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
	  IDSetNumber(&EquatorialCoordsRNP, NULL);
	  IDLog("Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
	  break;

     // Track
     case LX200_TRACK:
          //IDLog("We're in LX200_TRACK\n");
          if (EquatorialCoordsWNP.s == IPS_BUSY)
	  {
	     IDLog("Aboring Slew\n");
	     abortSlew(fd);

	     // sleep for 200 mseconds
	     usleep(200000);
	  }

	  dx = fabs ( targetRA - currentRA );
	  dy = fabs (targetDEC - currentDEC);

	  if (dx >= (TrackAccuracyN[0].value/(60.0*15.0)) || (dy >= TrackAccuracyN[1].value/60.0)) 
	  {
          	if ( !simulation && (error_code = Slew(fd)))
	  	{
	    		slew_error(error_code);
	    		return false;
	  	}

		fs_sexa(RAStr, targetRA, 2, 3600);
	        fs_sexa(DecStr, targetDEC, 2, 3600);
		EquatorialCoordsWNP.s = IPS_BUSY;
		EquatorialCoordsRNP.s = IPS_BUSY;
		IDSetNumber(&EquatorialCoordsWNP, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
		IDSetNumber(&EquatorialCoordsRNP, NULL);
		IDLog("Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
	  }
	  else
	  {
	    //IDLog("Tracking called, but tracking threshold not reached yet.\n");
	    EquatorialCoordsWNP.s = IPS_OK;
	    EquatorialCoordsRNP.s = IPS_OK;

	    if (lastSet != LX200_TRACK)
	      IDSetNumber(&EquatorialCoordsWNP, "Tracking...");
	    else
	      IDSetNumber(&EquatorialCoordsWNP, NULL);

	    IDSetNumber(&EquatorialCoordsRNP, NULL);
	  }
	  lastSet = LX200_TRACK;
      break;

    // Sync
    case LX200_SYNC:
          lastSet = LX200_SYNC;
	  EquatorialCoordsWNP.s = IPS_IDLE;
	   
	  if ( !simulation && ( error_code = Sync(fd, syncString) < 0) )
	  {
	        IDSetNumber( &EquatorialCoordsWNP , "Synchronization failed.");
		return false;
	  }

	  if (simulation)
	  {
             EquatorialCoordsRN[0].value = EquatorialCoordsWN[0].value;
             EquatorialCoordsRN[1].value = EquatorialCoordsWN[1].value;
	  }

	  EquatorialCoordsWNP.s = IPS_OK;
	  EquatorialCoordsRNP.s = IPS_OK;
	  IDSetNumber(&EquatorialCoordsRNP, NULL);
	  IDLog("Synchronization successful %s\n", syncString);
	  IDSetNumber(&EquatorialCoordsWNP, "Synchronization successful.");
	  break;
    }

   return true;

}

/**************************************************************************************
**
***************************************************************************************/
int LX200Basic::get_switch_index(ISwitchVectorProperty *sp)
{
 for (int i=0; i < sp->nsp ; i++)
     if (sp->sp[i].s == ISS_ON)
      return i;

 return -1;
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::connect_telescope()
{
     switch (ConnectSP.sp[0].s)
     {
      case ISS_ON:  
	
        if (simulation)
	{
	  ConnectSP.s = IPS_OK;
	  IDSetSwitch (&ConnectSP, "Simulated telescope is online.");
	  return;
	}
	
         if (tty_connect(PortT[0].text, 9600, 8, 0, 1, &fd) != TTY_OK)
	 {
	   ConnectS[0].s = ISS_OFF;
	   ConnectS[1].s = ISS_ON;
	   IDSetSwitch (&ConnectSP, "Error connecting to port %s. Make sure you have BOTH read and write permission to the port.", PortT[0].text);
	   return;
	 }

	 if (check_lx200_connection(fd))
	 {   
	   ConnectS[0].s = ISS_OFF;
	   ConnectS[1].s = ISS_ON;
	   IDSetSwitch (&ConnectSP, "Error connecting to Telescope. Telescope is offline.");
	   return;
	 }

	ConnectSP.s = IPS_OK;
	IDSetSwitch (&ConnectSP, "Telescope is online. Retrieving basic data...");
	get_initial_data();
	break;

     case ISS_OFF:
         ConnectS[0].s = ISS_OFF;
	 ConnectS[1].s = ISS_ON;
         ConnectSP.s = IPS_IDLE;
	 if (simulation)
         {
	    IDSetSwitch (&ConnectSP, "Simulated Telescope is offline.");
	    return;
         }
         IDSetSwitch (&ConnectSP, "Telescope is offline.");
	 IDLog("Telescope is offline.");
         
	 tty_disconnect(fd);
	 break;
    }
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::get_initial_data()
{

  // Make sure short
  checkLX200Format(fd);

  // Get current RA/DEC
  getLX200RA(fd, &currentRA);
  getLX200DEC(fd, &currentDEC);

  IDSetNumber (&EquatorialCoordsRNP, NULL);  
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::slew_error(int slewCode)
{
    OnCoordSetSP.s = IPS_IDLE;

    if (slewCode == 1)
	IDSetSwitch (&OnCoordSetSP, "Object below horizon.");
    else if (slewCode == 2)
	IDSetSwitch (&OnCoordSetSP, "Object below the minimum elevation limit.");
    else
        IDSetSwitch (&OnCoordSetSP, "Slew failed.");
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::enable_simulation(bool enable)
{
   simulation = enable;
   
   if (simulation)
     IDLog("Warning: Simulation is activated.\n");
   else
     IDLog("Simulation is disabled.\n");
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::connection_lost()
{
    ConnectSP.s = IPS_IDLE;
    IDSetSwitch(&ConnectSP, "The connection to the telescope is lost.");
    return;
 
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::connection_resumed()
{
  ConnectS[0].s = ISS_ON;
  ConnectS[1].s = ISS_OFF;
  ConnectSP.s = IPS_OK;
   
  IDSetSwitch(&ConnectSP, "The connection to the telescope has been resumed.");
}
