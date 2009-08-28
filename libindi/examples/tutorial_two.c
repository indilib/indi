/*
   INDI Developers Manual
   Tutorial #2
   
   "Simple Telescope Simulator"
   
   In this tutorial, we create a simple simulator. We will use a few handy utility functions provided by INDI
   like timers, string <---> number conversion, and more.
   
   Refer to README, which contains instruction on how to build this driver, and use it 
   with an INDI-compatible client.

*/

/** \file tutorial_two.c
    \brief Implement a simple telescope simulator using more complex INDI concepts.
    \author Jasem Mutlaq
*/

/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include <sys/time.h>
#include <time.h>

void show_runtime(int state) {
	static struct timeval tv;
	struct timeval tv1;
	double x, y;

	if (state) {
		gettimeofday(&tv, NULL);
	} else {
		gettimeofday(&tv1, NULL);
		fprintf(stderr, "Ran for: %fmsec\n", (double)(tv1.tv_sec * 1000.0 + tv1.tv_usec / 1000.0) - (double)(tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0));
	}
}

/* INDI Core headers */

/* indidevapi.h contains API declerations */
#include "indidevapi.h"

/* INDI Eventloop mechanism */
#include "eventloop.h"

/* INDI Common Routines */
#include "indicom.h"

/* Definitions */

#define	mydev			"Telescope Simulator"			/* Device name */
#define 	MAIN_GROUP		"Main Control"					/* Group name */
#define	SLEWRATE		1							/* slew rate, degrees/s */
#define	POLLMS			250							/* poll period, ms */
#define 	SIDRATE			0.004178						/* sidereal rate, degrees/s */

/* Function protptypes */
static void connectTelescope (void);
static void mountSim (void *);

/* operational info */
static double targetRA;
static double targetDEC;

/* main connection switch
  Note that the switch will appear to the user as On and Off (versus Connect and Disconnect in tutorial one)
  Nevertheless, the members _names_ are still CONNECT and DISCONNECT and therefore this is a perfectly legal standard property decleration
   */
static ISwitch connectS[] = {
    {"CONNECT",  "On",  ISS_OFF, 0, 0}, {"DISCONNECT", "Off", ISS_ON, 0, 0}};

static ISwitchVectorProperty connectSP = { mydev, "CONNECTION", "Connection",  MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE,  connectS, NARRAY(connectS), "", 0 };

/* Equatorial position. EQUATORIAL_COORD is one of INDI's reserved Standard  Properties */
static INumber eqN[] = {
                                /* 1st member is Right ascension */
    				{"RA"				/* 1st Number name */
				,"RA  H:M:S"			/* Number label */
				, "%10.6m"			/* Format. Refer to the indiapi.h for details on number formats */
				,0.					/* Minimum value */
				, 24.					/* Maximum value */
				, 0.					/* Steps */
				, 0.					/* Initial value */
				, 0					/* Pointer to parent, we don't use it, so set it to 0 */
				, 0					/* Auxiluary member, set it to 0 */
				, 0},					/* Autxiluar member, set it to 0 */
				
				/* 2nd member is Declination */
    				{"DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0., 0, 0, 0}
};

static INumberVectorProperty eqNP = {  mydev, "EQUATORIAL_EOD_COORD", "Equatorial JNow",  MAIN_GROUP , IP_RW, 0, IPS_IDLE,  eqN, NARRAY(eqN), "", 0};

/* Property naming convention. All property names are lower case with a postfix to indicate their type. connectS is a switch, 
 * connectSP is a switch vector. eqN is a number, eqNP is a number property, and so on. While this is not strictly required, it makes the code easier to read. */

#define 	currentRA			eqN[0].value					/* scope's current simulated RA, rads. Handy macro to right ascension from eqN[] */
#define     currentDec		eqN[1].value					/* scope's current simulated Dec, rads. Handy macro to declination from eqN[] */

/********************************************
 Property: Movement (Arrow keys on handset). North/South
*********************************************/
static ISwitch MovementNSS[]       = {{"MOTION_NORTH", "North", ISS_OFF, 0, 0}, {"MOTION_SOUTH", "South", ISS_OFF, 0, 0}};
ISwitchVectorProperty MovementNSSP      = { mydev, "TELESCOPE_MOTION_NS", "North/South", MAIN_GROUP, IP_RW, ISR_ATMOST1, 0, IPS_IDLE, MovementNSS, NARRAY(MovementNSS), "", 0};

/********************************************
 Property: Movement (Arrow keys on handset). West/East
*********************************************/
static ISwitch MovementWES[]       = {{"MOTION_WEST", "West", ISS_OFF, 0, 0}, {"MOTION_EAST", "East", ISS_OFF, 0, 0}};
ISwitchVectorProperty MovementWESP      = { mydev, "TELESCOPE_MOTION_WE", "West/East", MAIN_GROUP, IP_RW, ISR_ATMOST1, 0, IPS_IDLE, MovementWES, NARRAY(MovementWES), "", 0};

/* Initlization routine */
static void mountInit()
{
	static int inited;		/* set once mountInit is called */

	if (inited)
	    return;
	
	/* start timer to simulate mount motion
	   The timer will call function mountSim after POLLMS milliseconds */
	//IEAddTimer (POLLMS, mountSim, NULL);

	inited = 1;
	
}

/* send client definitions of all properties */
void ISGetProperties (const char *dev)
{
	if (dev && strcmp (mydev, dev))
	    return;

	IDDefSwitch (&connectSP, NULL);
	IDDefNumber (&eqNP, NULL);
	IDDefSwitch (&MovementNSSP, NULL);
	IDDefSwitch (&MovementWESP, NULL);
	
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	return;
}

/* client is sending us a new value for a Numeric vector property */
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	/* Make sure to initalize */
	mountInit();
	
	/* ignore if not ours */
	if (strcmp (dev, mydev))
	    return;

	if (!strcmp (name, eqNP.name)) {
	    /* new equatorial target coords */
	    double newra = 0, newdec = 0;
	    int i, nset;
	    
	    /* Check connectSP, if it is idle, then return */
	    if (connectSP.s == IPS_IDLE)
	    {
		eqNP.s = IPS_IDLE;
		IDSetNumber(&eqNP, "Telescope is offline.");
		return;
	    }
	    
	    for (nset = i = 0; i < n; i++) 
	    {
	        /* Find numbers with the passed names in the eqNP property */
		INumber *eqp = IUFindNumber (&eqNP, names[i]);
		
		/* If the number found is Right ascension (eqN[0]) then process it */
		if (eqp == &eqN[0])
		{
		    newra = (values[i]);
		    nset += newra >= 0 && newra <= 24;
		}
		/* Otherwise, if the number found is Declination (eqN[1]) then process it */ 
		else if (eqp == &eqN[1]) {
		    newdec = (values[i]);
		    nset += newdec >= -90 && newdec <= 90;
		}
	    } /* end for */
	    
	    /* Did we process the two numbers? */
	    if (nset == 2)
	    {
		char r[32], d[32];
		
		/* Set the mount state to BUSY */
		eqNP.s = IPS_BUSY;
		
		/* Set the new target coordinates */
		targetRA = newra;
		targetDEC = newdec;
		
		/* Convert the numeric coordinates to a sexagesmal string (H:M:S) */
		fs_sexa (r, targetRA, 2, 3600);
		fs_sexa (d, targetDEC, 3, 3600);
		
		IDSetNumber(&eqNP, "Moving to RA Dec %s %s", r, d);
	    }
	    /* We didn't process the two number correctly, report an error */
	    else 
	    {
	        /* Set property state to idle */
		eqNP.s = IPS_IDLE;
		
		IDSetNumber(&eqNP, "RA or Dec absent or bogus.");
	    }
	    
	    return;
	}
}

/* client is sending us a new value for a Switch property */
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	ISwitch *sp;

	mountInit();
	
	/* ignore if not ours */
	if (strcmp (dev, mydev))
	    return;

	
	if (!strcmp(name, connectSP.name))
	{
		/* We update switches. This is different from the way we used to update switches in tutorial 1. This is 
	 	* to illustrate that there are several ways to update the switches. Here, we try to find the switch with names[0],
	 	* and if found, we update its state to states[0] and call connectTelescope(). We must call IUResetSwitches to erase any previous history */
	 
		sp = IUFindSwitch (&connectSP, names[0]);
	
		if (sp)
		{
	    		IUResetSwitch(&connectSP);
	    		sp->s = states[0];
	    		connectTelescope();
		}
	}
	else if (! strcmp(name, MovementNSSP.name)) {
		sp = IUFindSwitch (&MovementNSSP, names[0]);
		if (sp) {
			IUResetSwitch(&MovementNSSP);
			sp->s = states[0];
			show_runtime(sp->s);
			IDSetSwitch (&MovementNSSP, "Toggle North/South.");
		}
	}
	else if (! strcmp(name, MovementWESP.name)) {
		sp = IUFindSwitch (&MovementWESP, names[0]);
		if (sp) {
			IUResetSwitch(&MovementWESP);
			sp->s = states[0];
			show_runtime(sp->s);
			IDSetSwitch (&MovementWESP, "Toggle West/East.");
		}
	}
	
	
}

/* update the "mount" over time */
void mountSim (void *p)
{
	static struct timeval ltv;
	struct timeval tv;
	double dt, da, dx;
	int nlocked;

	/* If telescope is not on, do not simulate */
	if (connectSP.s == IPS_IDLE)
	{
		IEAddTimer (POLLMS, mountSim, NULL);
		return;
	}

	/* update elapsed time since last poll, don't presume exactly POLLMS */
	gettimeofday (&tv, NULL);
	
	if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
	    ltv = tv;
	    
	dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
	ltv = tv;
	da = SLEWRATE*dt;

	/* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
	switch (eqNP.s)
	{
	
	/* #1 State is idle, update telesocpe at sidereal rate */
	case IPS_IDLE:
	    /* RA moves at sidereal, Dec stands still */
	    currentRA += (SIDRATE*dt/15.);
	    IDSetNumber(&eqNP, NULL);
	    break;

	case IPS_BUSY:
	    /* slewing - nail it when both within one pulse @ SLEWRATE */
	    nlocked = 0;

	    dx = targetRA - currentRA;
	    
	    if (fabs(dx) <= da)
	    {
		currentRA = targetRA;
		nlocked++;
	    }
	    else if (dx > 0)
	    	currentRA += da/15.;
	    else 
	    	currentRA -= da/15.;
	    

	    dx = targetDEC - currentDec;
	    if (fabs(dx) <= da)
	    {
		currentDec = targetDEC;
		nlocked++;
	    }
	    else if (dx > 0)
	      currentDec += da;
	    else
	      currentDec -= da;

	    if (nlocked == 2)
	    {
		eqNP.s = IPS_OK;
		IDSetNumber(&eqNP, "Now tracking");
	    } else
		IDSetNumber(&eqNP, NULL);

	    break;

	case IPS_OK:
	    /* tracking */
	   IDSetNumber(&eqNP, NULL);
	    break;

	case IPS_ALERT:
	    break;
	}

	/* again */
	IEAddTimer (POLLMS, mountSim, NULL);
}

/* Note that we must define ISNewBLOB and ISSnoopDevice even if we don't use them, otherwise, the driver will NOT compile */
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {}
void ISSnoopDevice (XMLEle *root) {}


static void connectTelescope ()
{
	if (connectS[0].s == ISS_ON)
	{
	    connectSP.s   = IPS_OK;
	    IDSetSwitch (&connectSP, "Telescope is connected.");
	} 
	else
	{
	    connectSP.s   = IPS_IDLE;
	    IDSetSwitch (&connectSP, "Telescope is disconnected.");
	}
}
