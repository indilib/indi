#if 0
    Celestron GPS
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "celestronprotocol.h"
#include "celestrongps.h"

#define mydev 		"Celestron GPS"

/* Handy Macros */
#define currentRA	EquatorialCoordsRN[0].value
#define currentDEC	EquatorialCoordsRN[1].value

/* Enable to log debug statements 
#define CELESTRON_DEBUG	1
*/

CelestronGPS *telescope = NULL;


/* There is _one_ binary for all LX200 drivers, but each binary is renamed
** to its device name (i.e. lx200gps, lx200_16..etc). The main function will
** fetch from std args the binary name and ISInit will create the apporpiate
** device afterwards. If the binary name does not match any known devices,
** we simply create a generic device
*/
extern char* me;

#define COMM_GROUP	"Communication"
#define BASIC_GROUP	"Main Control"
#define MOVE_GROUP	"Movement Control"

static void ISPoll(void *);

/*INDI controls */
static ISwitch SlewModeS[]       = {{"Slew", "", ISS_ON, 0, 0}, {"Find", "", ISS_OFF, 0, 0}, {"Centering", "", ISS_OFF, 0, 0}, {"Guide", "", ISS_OFF, 0, 0}};

/* Equatorial Coordinates: Info */
static INumber EquatorialCoordsRN[] = {    {"RA",  "RA  H:M:S", "%10.6m",  0., 24., 0., 0., 0, 0, 0},    {"DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0., 0, 0, 0}};
static INumberVectorProperty EquatorialCoordsRNP = {  mydev, "EQUATORIAL_EOD_COORD", "Equatorial JNow", BASIC_GROUP, IP_RO, 120, IPS_IDLE,  EquatorialCoordsRN, NARRAY(EquatorialCoordsRN), "", 0};

/* Tracking precision */
INumber TrackingAccuracyN[] = {
    {"TrackRA",  "RA (arcmin)", "%10.6m",  0., 60., 1., 3.0, 0, 0, 0},
    {"TrackDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0, 0, 0, 0},
};
static INumberVectorProperty TrackingAccuracyNP = {mydev, "Tracking Accuracy", "", MOVE_GROUP, IP_RW, 0, IPS_IDLE, TrackingAccuracyN, NARRAY(TrackingAccuracyN), "", 0};

/* Slew precision */
INumber SlewAccuracyN[] = {
    {"SlewRA",  "RA (arcmin)", "%10.6m",  0., 60., 1., 3.0, 0, 0, 0},
    {"SlewDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0, 0, 0, 0},
};
static INumberVectorProperty SlewAccuracyNP = {mydev, "Slew Accuracy", "", MOVE_GROUP, IP_RW, 0, IPS_IDLE, SlewAccuracyN, NARRAY(SlewAccuracyN), "", 0};

/* Fundamental group */
static ISwitch ConnectS[]          = {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty ConnectSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ConnectS, NARRAY(ConnectS), "", 0};

static IText PortT[]			= {{"PORT", "Port", 0, 0, 0, 0}};
static ITextVectorProperty PortTP		= { mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT), "", 0};

/* Movement group */
static ISwitch OnCoordSetS[]     = {{"SLEW", "Slew", ISS_ON, 0 , 0}, {"TRACK", "Track", ISS_OFF, 0, 0}, {"SYNC", "Sync", ISS_OFF, 0, 0}};
static ISwitchVectorProperty OnCoordSetSP    = { mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, OnCoordSetS, NARRAY(OnCoordSetS), "", 0};

static ISwitch AbortSlewS[]      = {{"ABORT", "Abort", ISS_OFF, 0, 0}};
static ISwitchVectorProperty AbortSlewSP     = { mydev, "TELESCOPE_ABORT_MOTION", "Abort Slew/Track", BASIC_GROUP, IP_RW, ISR_ATMOST1, 0, IPS_IDLE, AbortSlewS, NARRAY(AbortSlewS), "", 0};
static ISwitchVectorProperty SlewModeSP      = { mydev, "Slew rate", "", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SlewModeS, NARRAY(SlewModeS), "", 0};

/* Movement (Arrow keys on handset). North/South */
static ISwitch MovementNSS[]       = {{"MOTION_NORTH", "North", ISS_OFF, 0, 0}, {"MOTION_SOUTH", "South", ISS_OFF, 0, 0}};

static ISwitchVectorProperty MovementNSSP      = { mydev, "TELESCOPE_MOTION_NS", "North/South", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, MovementNSS, NARRAY(MovementNSS), "", 0};

/* Movement (Arrow keys on handset). West/East */
static ISwitch MovementWES[]       = {{"MOTION_WEST", "West", ISS_OFF, 0, 0}, {"MOTION_EAST", "East", ISS_OFF, 0, 0}};

static ISwitchVectorProperty MovementWESP      = { mydev, "TELESCOPE_MOTION_WE", "West/East", MOVE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, MovementWES, NARRAY(MovementWES), "", 0};


/* send client definitions of all properties */
void ISInit()
{
  static int isInit=0;

 if (isInit)
  return;

 isInit = 1;

  IUSaveText(&PortT[0], "/dev/ttyS0");
  
  telescope = new CelestronGPS();

  IEAddTimer (POLLMS, ISPoll, NULL);
}

void ISGetProperties (const char *dev)
{ ISInit(); telescope->ISGetProperties(dev);}
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{ ISInit(); telescope->ISNewSwitch(dev, name, states, names, n);}
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{ ISInit(); telescope->ISNewText(dev, name, texts, names, n);}
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{ ISInit(); telescope->ISNewNumber(dev, name, values, names, n);}
void ISPoll (void *p) { telescope->ISPoll(); IEAddTimer (POLLMS, ISPoll, NULL); p=p;}
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
  INDI_UNUSED(root);
}

/**************************************************
*** LX200 Generic Implementation
***************************************************/

CelestronGPS::CelestronGPS()
{

   lastRA = 0;
   lastDEC = 0;
   currentSet   = 0;
   lastSet      = -1;

   // Children call parent routines, this is the default
   IDLog("initializing from Celeston GPS device...\n");

}

void CelestronGPS::ISGetProperties(const char *dev)
{

 if (dev && strcmp (mydev, dev))
    return;

  // COMM_GROUP
  IDDefSwitch (&ConnectSP, NULL);
  IDDefText   (&PortTP, NULL);
  
  // BASIC_GROUP
  IDDefNumber (&EquatorialCoordsRNP, NULL);
  IDDefSwitch (&OnCoordSetSP, NULL);
  IDDefSwitch (&AbortSlewSP, NULL);
  IDDefSwitch (&SlewModeSP, NULL);

  // Movement group
  IDDefSwitch (&MovementNSSP, NULL);
  IDDefSwitch (&MovementWESP, NULL);
  IDDefNumber (&TrackingAccuracyNP, NULL);
  IDDefNumber (&SlewAccuracyNP, NULL);
  
  /* Send the basic data to the new client if the previous client(s) are already connected. */		
  if (ConnectSP.s == IPS_OK)
        getBasicData();

}

void CelestronGPS::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
        IText *tp;

	INDI_UNUSED(n);

	// ignore if not ours //
	if (strcmp (dev, mydev))
	    return;

	if (!strcmp(name, PortTP.name) )
	{
	  PortTP.s = IPS_OK;

	  tp = IUFindText( &PortTP, names[0] );
	  if (!tp)
	   return;

	  IUSaveText(&PortT[0], texts[0]);
	  IDSetText (&PortTP, NULL);
	  return;
	}
}

int CelestronGPS::handleCoordSet()
{

  int i=0;
  char RAStr[32], DecStr[32];

  switch (currentSet)
  {

    // Slew
    case 0:
          lastSet = 0;
      if (EquatorialCoordsRNP.s == IPS_BUSY)
	  {
	     StopNSEW();
	     // sleep for 500 mseconds
	     usleep(500000);
	  }

	  if ((i = SlewToCoords(targetRA, targetDEC)))
	  {
	    slewError(i);
	    return (-1);
	  }

	  EquatorialCoordsRNP.s = IPS_BUSY;
	  fs_sexa(RAStr, targetRA, 2, 3600);
	  fs_sexa(DecStr, targetDEC, 2, 3600);
      IDSetNumber(&EquatorialCoordsRNP, "Slewing to JNOW RA %s - DEC %s", RAStr, DecStr);
	  IDLog("Slewing to JNOW RA %s - DEC %s", RAStr, DecStr);
	  break;


  // Track
  case 1: 
          if (EquatorialCoordsRNP.s == IPS_BUSY)
	  {
	      StopNSEW();
	     // sleep for 500 mseconds
	     usleep(500000);
	  }

	  if ( (fabs ( targetRA - currentRA ) >= (TrackingAccuracyN[0].value/(15.0*60.0))) ||
 	       (fabs (targetDEC - currentDEC) >= (TrackingAccuracyN[1].value)/60.0))
	  {

		#ifdef CELESTRON_DEBUG
	        IDLog("Exceeded Tracking threshold, will attempt to slew to the new target.\n");
		IDLog("targetRA is %g, currentRA is %g\n", targetRA, currentRA);
	        IDLog("targetDEC is %g, currentDEC is %g\n*************************\n", targetDEC, currentDEC);
		#endif

          	if (( i =  SlewToCoords(targetRA, targetDEC)))
	  	{
	    		slewError(i);
	    		return (-1);
	  	}
		
		fs_sexa(RAStr, targetRA, 2, 3600);
        fs_sexa(DecStr, targetDEC, 2, 3600);
		EquatorialCoordsRNP.s = IPS_BUSY;
        IDSetNumber(&EquatorialCoordsRNP, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
		IDLog("Slewing to JNOW RA %s - DEC %s", RAStr, DecStr);
	  }
	  else
	  {
	    #ifdef CELESTRON_DEBUG
	    IDLog("Tracking called, but tracking threshold not reached yet.\n");
	    #endif
        EquatorialCoordsRNP.s = IPS_OK;
            if (lastSet != 1)
          IDSetNumber(&EquatorialCoordsRNP, "Tracking...");
	    else
          IDSetNumber(&EquatorialCoordsRNP, NULL);

	  }
	  lastSet = 1;
      break;
      
    // Sync
    case 2:
          lastSet = 2;
	  OnCoordSetSP.s = IPS_OK;
	  SyncToCoords(targetRA, targetDEC);   
	  EquatorialCoordsRNP.s = IPS_OK;
      IDSetNumber(&EquatorialCoordsRNP, "Synchronization successful.");
	  break;
   }

   return (0);

}

void CelestronGPS::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
        double newRA=0, newDEC=0;

        // ignore if not ours //
	if (strcmp (dev, mydev))
	    return;

	struct tm *tp;
	time_t t;

	time (&t);
	tp = gmtime (&t);

        if (!strcmp (name, TrackingAccuracyNP.name))
	{
		if (!IUUpdateNumber(&TrackingAccuracyNP, values, names, n))
		{
			TrackingAccuracyNP.s = IPS_OK;
			IDSetNumber(&TrackingAccuracyNP, NULL);
			return;
		}
		
		TrackingAccuracyNP.s = IPS_ALERT;
		IDSetNumber(&TrackingAccuracyNP, "unknown error while setting tracking precision");
		return;
	}

	if (!strcmp(name, SlewAccuracyNP.name))
	{
		IUUpdateNumber(&SlewAccuracyNP, values, names, n);
		{
			SlewAccuracyNP.s = IPS_OK;
			IDSetNumber(&SlewAccuracyNP, NULL);
			return;
		}
		
		SlewAccuracyNP.s = IPS_ALERT;
		IDSetNumber(&SlewAccuracyNP, "unknown error while setting slew precision");
		return;
	}

    if (!strcmp (name, EquatorialCoordsRNP.name))
	{
	  int i=0, nset=0;

      if (checkPower(&EquatorialCoordsRNP))
	   return;

	    for (nset = i = 0; i < n; i++)
	    {
        INumber *eqp = IUFindNumber (&EquatorialCoordsRNP, names[i]);
        if (eqp == &EquatorialCoordsRN[0])
		{
                    newRA = values[i];
		    nset += newRA >= 0 && newRA <= 24.0;
        } else if (eqp == &EquatorialCoordsRN[1])
		{
		    newDEC = values[i];
		    nset += newDEC >= -90.0 && newDEC <= 90.0;
		}
	    }

	  if (nset == 2)
	  {
	   //EquatorialCoordsNP.s = IPS_BUSY;

	   tp->tm_mon   += 1;
	   tp->tm_year  += 1900;

	   targetRA  = newRA;
	   targetDEC = newDEC;
	       
	   if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
	   {
	   	IUResetSwitch(&MovementNSSP);
		IUResetSwitch(&MovementWESP);
		MovementNSSP.s = MovementWESP.s = IPS_IDLE;
		IDSetSwitch(&MovementNSSP, NULL);
		IDSetSwitch(&MovementWESP, NULL);
	   }
	   
	       if (handleCoordSet())
	       {
            EquatorialCoordsRNP.s = IPS_ALERT;
            IDSetNumber(&EquatorialCoordsRNP, NULL);
	       }
	    }
	    else
	    {
        EquatorialCoordsRNP.s = IPS_ALERT;
        IDSetNumber(&EquatorialCoordsRNP, "RA or Dec missing or invalid.");
	    }

	    return;
	 }
}

void CelestronGPS::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

        int index;

	INDI_UNUSED(names);

	// ignore if not ours //
	if (strcmp (dev, mydev))
	    return;

	// FIRST Switch ALWAYS for power
	if (!strcmp (name, ConnectSP.name))
	{
	 if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0) return;
   	 connectTelescope();
	 return;
	}

	if (!strcmp(name, OnCoordSetSP.name))
	{
  	  if (checkPower(&OnCoordSetSP))
	   return;

	  
	  if (IUUpdateSwitch(&OnCoordSetSP, states, names, n) < 0) return;
          OnCoordSetSP.s = IPS_OK;
          IDSetSwitch(&OnCoordSetSP, NULL);
	  currentSet = getOnSwitch(&OnCoordSetSP);
	}
	
	// Abort Slew
	if (!strcmp (name, AbortSlewSP.name))
	{
	  if (checkPower(&AbortSlewSP))
	  {
	    AbortSlewSP.s = IPS_ALERT;
	    IDSetSwitch(&AbortSlewSP, NULL);
	    return;
	  }
	  
	  IUResetSwitch(&AbortSlewSP);
	  StopNSEW();

        if (EquatorialCoordsRNP.s == IPS_BUSY)
	    {
		AbortSlewSP.s = IPS_OK;
		EquatorialCoordsRNP.s       = IPS_IDLE;
		IDSetSwitch(&AbortSlewSP, "Slew aborted.");
		IDSetNumber(&EquatorialCoordsRNP, NULL);
        }
        else if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
	    {
		MovementNSSP.s  = MovementWESP.s =  IPS_IDLE; 
	
		AbortSlewSP.s = IPS_OK;		
		EquatorialCoordsRNP.s       = IPS_IDLE;
		IUResetSwitch(&MovementNSSP);
		IUResetSwitch(&MovementWESP);
		IUResetSwitch(&AbortSlewSP);

		IDSetSwitch(&AbortSlewSP, "Slew aborted.");
		IDSetSwitch(&MovementNSSP, NULL);
		IDSetSwitch(&MovementWESP, NULL);
		IDSetNumber(&EquatorialCoordsRNP, NULL);
	    }
	    else
	    {
	        AbortSlewSP.s = IPS_OK;
	        IDSetSwitch(&AbortSlewSP, NULL);
	    }

	    return;
	}

	// Slew mode
	if (!strcmp (name, SlewModeSP.name))
	{
	  if (checkPower(&SlewModeSP))
	   return;

	  IUResetSwitch(&SlewModeSP);
	  IUUpdateSwitch(&SlewModeSP, states, names, n);
	  index = getOnSwitch(&SlewModeSP);
	  SetRate(index);
          
	  SlewModeSP.s = IPS_OK;
	  IDSetSwitch(&SlewModeSP, NULL);
	  return;
	}

	// Movement (North/South)
	if (!strcmp (name, MovementNSSP.name))
	{
	  if (checkPower(&MovementNSSP))
	   return;

	 int last_move=-1;
         int current_move = -1;

	// -1 means all off previously
	 last_move = getOnSwitch(&MovementNSSP);

         IUUpdateSwitch(&MovementNSSP, states, names, n);

	current_move = getOnSwitch(&MovementNSSP);

	// Previosuly active switch clicked again, so let's stop.
	if (current_move == last_move)
	{
		StopSlew((current_move == 0) ? NORTH : SOUTH);
		IUResetSwitch(&MovementNSSP);
	    	MovementNSSP.s = IPS_IDLE;
	    	IDSetSwitch(&MovementNSSP, NULL);
		return;
	}

	#ifdef CELESTRON_DEBUG
        IDLog("Current Move: %d - Previous Move: %d\n", current_move, last_move);
	#endif

	// 0 (North) or 1 (South)
	last_move      = current_move;

	// Correction for Celestron Driver: North 0 - South 3
	current_move = (current_move == 0) ? NORTH : SOUTH;

	StartSlew(current_move);
	
	  MovementNSSP.s = IPS_BUSY;
	  IDSetSwitch(&MovementNSSP, "Moving toward %s", (current_move == NORTH) ? "North" : "South");
	  return;
	}

	// Movement (West/East)
	if (!strcmp (name, MovementWESP.name))
	{
	  if (checkPower(&MovementWESP))
	   return;

	 int last_move=-1;
         int current_move = -1;

	// -1 means all off previously
	 last_move = getOnSwitch(&MovementWESP);

	 if (IUUpdateSwitch(&MovementWESP, states, names, n) < 0)
        IDLog("fixme!!! - IUUpdateSwitch MovementWESP\n");
		//return;

	current_move = getOnSwitch(&MovementWESP);

	// Previosuly active switch clicked again, so let's stop.
	if (current_move == last_move)
	{
		StopSlew((current_move ==0) ? WEST : EAST);
		IUResetSwitch(&MovementWESP);
	    	MovementWESP.s = IPS_IDLE;
	    	IDSetSwitch(&MovementWESP, NULL);
		return;
	}

	#ifdef CELESTRON_DEBUG
        IDLog("Current Move: %d - Previous Move: %d\n", current_move, last_move);
	#endif

	// 0 (West) or 1 (East)
	last_move      = current_move;

	// Correction for Celestron Driver: West 1 - East 2
	current_move = (current_move == 0) ? WEST : EAST;

	StartSlew(current_move);
	
	  MovementWESP.s = IPS_BUSY;
	  IDSetSwitch(&MovementWESP, "Moving toward %s", (current_move == WEST) ? "West" : "East");
	  return;
	}
}


int CelestronGPS::getOnSwitch(ISwitchVectorProperty *sp)
{
 for (int i=0; i < sp->nsp ; i++)
     if (sp->sp[i].s == ISS_ON)
      return i;

 return -1;
}


int CelestronGPS::checkPower(ISwitchVectorProperty *sp)
{
  if (ConnectSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
       IDMessage (mydev, "Cannot change property %s while the telescope is offline.", sp->name);
    else
       IDMessage (mydev, "Cannot change property %s while the telescope is offline.", sp->label);
       
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int CelestronGPS::checkPower(INumberVectorProperty *np)
{
  if (ConnectSP.s != IPS_OK)
  {
    if (!strcmp(np->label, ""))
       IDMessage (mydev, "Cannot change property %s while the telescope is offline.", np->name);
    else
       IDMessage (mydev, "Cannot change property %s while the telescope is offline.", np->label);
       
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }
  return 0;
}

int CelestronGPS::checkPower(ITextVectorProperty *tp)
{

  if (ConnectSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
       IDMessage (mydev, "Cannot change property %s while the telescope is offline.", tp->name);
    else
       IDMessage (mydev, "Cannot change property %s while the telescope is offline.", tp->label);
       
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void CelestronGPS::ISPoll()
{
       double dx, dy;
       int status;

    switch (EquatorialCoordsRNP.s)
	{
	case IPS_IDLE:
	if (ConnectSP.s != IPS_OK)
	 break;
	currentRA = GetRA();
	currentDEC = GetDec();

        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
	{
		lastRA  = currentRA;
		lastDEC = currentDEC;
		IDSetNumber (&EquatorialCoordsRNP, NULL);

	}
        break;

        case IPS_BUSY:
	    currentRA = GetRA();
	    currentDEC = GetDec();
	    dx = targetRA - currentRA;
	    dy = targetDEC - currentDEC;

            #ifdef CELESTRON_DEBUG
	    IDLog("targetRA is %f, currentRA is %f\n", (float) targetRA, (float) currentRA);
	    IDLog("targetDEC is %f, currentDEC is %f\n****************************\n", (float) targetDEC, (float) currentDEC);
	    #endif

	    status = CheckCoords(targetRA, targetDEC, SlewAccuracyN[0].value/(15.0*60.0) , SlewAccuracyN[1].value/60.0);

	    // Wait until acknowledged or within 3.6', change as desired.
	    switch (status)
	    {
	    case 0:		/* goto in progress */
		IDSetNumber (&EquatorialCoordsRNP, NULL);
		break;
	    case 1:		/* goto complete within tolerance */
	    case 2:		/* goto complete but outside tolerance */
		currentRA = targetRA;
		currentDEC = targetDEC;

        EquatorialCoordsRNP.s = IPS_OK;

		if (currentSet == 0)
          IDSetNumber (&EquatorialCoordsRNP, "Slew is complete.");
		else
          IDSetNumber (&EquatorialCoordsRNP, "Slew is complete. Tracking...");
		
		break;
	    }   
	    break;

	case IPS_OK:
	if (ConnectSP.s != IPS_OK)
	 break;
	currentRA = GetRA();
	currentDEC = GetDec();

        if ( fabs (currentRA - lastRA) > 0.01 || fabs (currentDEC - lastDEC) > 0.01)
	{
		lastRA  = currentRA;
		lastDEC = currentDEC;
		IDSetNumber (&EquatorialCoordsRNP, NULL);

	}
        break;


	case IPS_ALERT:
	    break;
	}

	switch (MovementNSSP.s)
	{
	  case IPS_IDLE:
	   break;
	 case IPS_BUSY:
	     currentRA = GetRA();
	     currentDEC = GetDec();
	     IDSetNumber (&EquatorialCoordsRNP, NULL);

	     break;
	 case IPS_OK:
	   break;
	 case IPS_ALERT:
	   break;
	 }

	switch (MovementWESP.s)
	{
	  case IPS_IDLE:
	   break;
	 case IPS_BUSY:
	     currentRA = GetRA();
	     currentDEC = GetDec();
	     IDSetNumber (&EquatorialCoordsRNP, NULL);

	     break;
	 case IPS_OK:
	   break;
	 case IPS_ALERT:
	   break;
	 }

}

void CelestronGPS::getBasicData()
{

  currentRA = GetRA();
  currentDEC = GetDec();

  IDSetNumber(&EquatorialCoordsRNP, NULL);

}

void CelestronGPS::connectTelescope()
{

     switch (ConnectSP.sp[0].s)
     {
      case ISS_ON:

         if (ConnectTel(PortTP.tp[0].text) < 0)
	 {
	   ConnectS[0].s = ISS_OFF;
	   ConnectS[1].s = ISS_ON;
	   IDSetSwitch (&ConnectSP, "Error connecting to port %s. Make sure you have BOTH write and read permission to the port.", PortTP.tp[0].text);
	   return;
	 }

	ConnectSP.s = IPS_OK;
	IDSetSwitch (&ConnectSP, "Telescope is online. Retrieving basic data...");
	getBasicData();
	break;

     case ISS_OFF:
         IDSetSwitch (&ConnectSP, "Telescope is offline.");
	 IDLog("Telescope is offline.");
	 DisconnectTel();
	 break;

    }
}

void CelestronGPS::slewError(int slewCode)
{
    EquatorialCoordsRNP.s = IPS_ALERT;

    switch (slewCode)
    {
      case 1:
       IDSetNumber (&EquatorialCoordsRNP, "Invalid newDec in SlewToCoords");
       break;
      case 2:
       IDSetNumber (&EquatorialCoordsRNP, "RA count overflow in SlewToCoords");
       break;
      case 3:
       IDSetNumber (&EquatorialCoordsRNP, "Dec count overflow in SlewToCoords");
       break;
      case 4:
       IDSetNumber (&EquatorialCoordsRNP, "No acknowledgment from telescope after SlewToCoords");
       break;
      default:
       IDSetNumber (&EquatorialCoordsRNP, "Unknown error");
       break;
    }

}
