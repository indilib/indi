/*
    Astro-Physics INDI driver

    Copyright (C) 2014 Jasem Mutlaq

    Based on INDI Astrophysics Driver by Markus Wildi

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

#include <indidevapi.h>

#include "lx200ap.h"
#include "lx200driver.h"
#include "lx200apdriver.h"
#include "lx200aplib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FIRMWARE_TAB   "Firmware data"
#define MOUNT_TAB       "Mount"

/* Constructor */
LX200AstroPhysics::LX200AstroPhysics() : LX200Generic()
{
    timeUpdated = locationUpdated = false;

}

const char *LX200AstroPhysics::getDefaultName()
{
    return (const char *) "AstroPhysics";
}


bool LX200AstroPhysics::initProperties()
{

    LX200Generic::initProperties();

    IUFillSwitch(&StartUpS[0], "COLD", "Cold", ISS_OFF);
    IUFillSwitch(&StartUpS[1], "WARM", "Warm", ISS_OFF);
    IUFillSwitchVector(&StartUpSP, StartUpS, 2, getDeviceName(), "STARTUP", "Mount init.", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&HourangleCoordsN[0], "HA", "HA H:M:S", "%10.6m", 0., 24., 0., 0.);
    IUFillNumber(&HourangleCoordsN[1], "DEC", "Dec D:M:S", "%10.6m",-90.0, 90.0, 0., 0.);
    IUFillNumberVector(&HourangleCoordsNP, HourangleCoordsN, 2, getDeviceName(), "HOURANGLE_COORD", "Hourangle Coords", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&HorizontalCoordsN[0], "AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0. );
    IUFillNumber(&HorizontalCoordsN[1], "ALT", "Alt D:M:S", "%10.6m", -90., 90., 0., 0. );
    IUFillNumberVector(&HorizontalCoordsNP,HorizontalCoordsN, 2, getDeviceName(), "HORIZONTAL_COORD", "Horizontal Coords", MAIN_CONTROL_TAB, IP_RW, 120, IPS_IDLE);

    IUFillSwitch(&MoveToRateS[0], "1200" , "1200x" , ISS_OFF);
    IUFillSwitch(&MoveToRateS[1], "600" , "600x" , ISS_OFF);
    IUFillSwitch(&MoveToRateS[2], "64" , "64x" , ISS_ON);
    IUFillSwitch(&MoveToRateS[3], "12" , "12x" , ISS_OFF);
    IUFillSwitchVector(&MoveToRateSP, MoveToRateS, 4, getDeviceName(), "MOVETORATE", "Move to rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&SlewRateS[0], "1200" , "1200x" , ISS_OFF);
    IUFillSwitch(&SlewRateS[1], "900" , "900x" , ISS_OFF);
    IUFillSwitch(&SlewRateS[2], "600" , "600x" , ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 3, getDeviceName(), "SLEWRATE", "Slew rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&SwapS[0], "NS" , "North/South" , ISS_OFF);
    IUFillSwitch(&SwapS[1], "EW" , "East/West" , ISS_OFF);
    IUFillSwitchVector(&SwapSP, SwapS, 2, getDeviceName(), "SWAP" , "Swap buttons", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&SyncCMRS[0], ":CM#" , ":CM#" , ISS_ON);
    IUFillSwitch(&SyncCMRS[1], ":CMR#" , ":CMR#" , ISS_OFF);
    IUFillSwitchVector(&SyncCMRSP, SyncCMRS, 2, getDeviceName(), "SYNCCMR" , "Sync", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&VersionT[0], "Number", "", 0);
    IUFillTextVector(&VersionInfo, VersionT, 1, getDeviceName(), "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE);

    IUFillText(&DeclinationAxisT[0], "RELHA", "rel. to HA", "undefined");
    IUFillTextVector(&DeclinationAxisTP, DeclinationAxisT, 1, getDeviceName(), "DECLINATIONAXIS", "Declination axis", MOUNT_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&APSiderealTimeN[0], "VALUE",  "H:M:S", "%10.6m",  0.,  24., 0., 0.);
    IUFillNumberVector(&APSiderealTimeNP, APSiderealTimeN, 1, getDeviceName(), "APSIDEREALTIME", "AP sidereal time", MOUNT_TAB, IP_RO, 0, IPS_IDLE);

    return true;
}

void LX200AstroPhysics::ISGetProperties(const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    if (isConnected())
    {
        defineSwitch(&StartUpSP);
        defineText(&VersionInfo);
        defineNumber(&HourangleCoordsNP) ;

        defineText(&DeclinationAxisTP);
        defineNumber(&APSiderealTimeNP);

        /* Motion group */
        defineSwitch (&TrackModeSP);
        defineSwitch (&MoveToRateSP);
        defineSwitch (&SlewRateSP);
        defineSwitch (&SwapSP);
        defineSwitch (&SyncCMRSP);

        loadConfig(true);

        DEBUG(INDI::Logger::DBG_SESSION, "Please initialize the mount before issuing any command.");
    }
}

bool LX200AstroPhysics::updateProperties()
{

    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineSwitch(&StartUpSP);
        defineText(&VersionInfo);
        defineNumber(&HourangleCoordsNP) ;

        defineText(&DeclinationAxisTP);
        defineNumber(&APSiderealTimeNP);

        /* Motion group */        
        defineSwitch (&TrackModeSP);
        defineSwitch (&MoveToRateSP);
        defineSwitch (&SlewRateSP);
        defineSwitch (&SwapSP);
        defineSwitch (&SyncCMRSP);

        loadConfig(true);

        DEBUG(INDI::Logger::DBG_SESSION, "Please initialize the mount before issuing any command.");        
    }
    else
    {
        deleteProperty(StartUpSP.name);
        deleteProperty(VersionInfo.name);
        deleteProperty(HourangleCoordsNP.name);
        deleteProperty(DeclinationAxisTP.name);
        deleteProperty(APSiderealTimeNP.name);
        deleteProperty(TrackModeSP.name);
        deleteProperty(MoveToRateSP.name);
        deleteProperty(SlewRateSP.name);
        deleteProperty(SwapSP.name);
        deleteProperty(SyncCMRSP.name);
    }

    controller->updateProperties();
    return true;
}

bool LX200AstroPhysics::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int err=0;

    // ignore if not ours //
    if (strcmp (getDeviceName(), dev))
        return false;

    // ============================================================
    // Satisfy AP mount initialization, see AP key pad manual p. 76
    // ============================================================
    if (!strcmp (name, StartUpSP.name))
    {
	int switch_nr ;
	static int mount_status= MOUNTNOTINITIALIZED ;

	IUUpdateSwitch(&StartUpSP, states, names, n) ;

    if ( mount_status == MOUNTNOTINITIALIZED)
	{
        if (timeUpdated == false || locationUpdated == false)
        {
            StartUpSP.s = IPS_ALERT;
            DEBUG(INDI::Logger::DBG_ERROR, "Time and location must be set before mount initialization is invoked.");
            IDSetSwitch(&StartUpSP, NULL);
            return false;
        }

        mount_status=MOUNTINITIALIZED;
	
        /*if( setBasicDataPart0() == false)
	    {
            StartUpSP.s = IPS_ALERT ;
            IDSetSwitch(&StartUpSP, "Mount initialization failed") ;
            return false ;
        }*/

	    if( StartUpSP.sp[0].s== ISS_ON) // do it only in case a power on (cold start) 
	    {

            if( setBasicDataPart1() == false)
            {
                StartUpSP.s = IPS_ALERT ;
                IDSetSwitch(&StartUpSP, "Cold mount initialization failed.") ;
                return false ;
            }
	    }

	    // Make sure that the mount is setup according to the properties	    
        switch_nr =  IUFindOnSwitchIndex(&TrackModeSP);

        if (isSimulation() == false && ( err = selectAPTrackingMode(PortFD, switch_nr) < 0) )
	    {
            DEBUGF(INDI::Logger::DBG_ERROR, "StartUpSP: Error setting tracking mode (%d).", err);
            return false;
	    }

        TrackModeSP.s = IPS_OK;
        IDSetSwitch(&TrackModeSP, NULL);

        //ToDo set button swapping acording telescope east west
        // ... some code here

        switch_nr = IUFindOnSwitchIndex(&MoveToRateSP);
	  
        if (isSimulation() == false && ( err = selectAPMoveToRate(PortFD, switch_nr) < 0) )
	    {
            DEBUGF(INDI::Logger::DBG_ERROR, "StartUpSP: Error setting move to rate (%d).", err);
            return false;
	    }
	    MoveToRateSP.s = IPS_OK;

	    IDSetSwitch(&MoveToRateSP, NULL);
        switch_nr = IUFindOnSwitchIndex(&SlewRateSP);
	  
        if (isSimulation() == false && ( err = selectAPSlewRate(PortFD, switch_nr) < 0) )
	    {
            DEBUGF(INDI::Logger::DBG_ERROR, "StartUpSP: Error setting slew rate (%d).", err);
            return false;
	    }
	    SlewRateSP.s = IPS_OK;
	    IDSetSwitch(&SlewRateSP, NULL);

	    StartUpSP.s = IPS_OK ;
        IDSetSwitch(&StartUpSP, "Mount initialized.") ;

        if (isSimulation())
        {
            currentRA = 0;
            currentDEC = 90;

        }
        else
        {
            getLX200RA( PortFD,  &currentRA);
            getLX200DEC(PortFD, &currentDEC);
        }
 
	    // make a IDSet in order the dome controller is aware of the initial values
	    targetRA= currentRA ;
	    targetDEC= currentDEC ;

        NewRaDec(currentRA, currentDEC);

	    // sleep for 100 mseconds
        //usleep(100000);

        //getLX200Az(fd, &currentAZ);
        //getLX200Alt(fd, &currentALT);

        //HorizontalCoordsNP.s = IPS_OK ;
        //IDSetNumber (&HorizontalCoordsNP, NULL);

	    VersionInfo.tp[0].text = new char[64];

        if (isSimulation())
            IUSaveText(&VersionT[0], "1.0");
        else
            getAPVersionNumber(PortFD, VersionInfo.tp[0].text);

	    VersionInfo.s = IPS_OK ;
	    IDSetText(&VersionInfo, NULL);
	}
	else
	{
        StartUpSP.s = IPS_OK ;
        IDSetSwitch(&StartUpSP, "Mount is already initialized.") ;
	}
    return true;
    }

    // =======================================
    // Tracking Mode
    // =======================================
    if (!strcmp (name, TrackModeSP.name))
    {

	IUResetSwitch(&TrackModeSP);
	IUUpdateSwitch(&TrackModeSP, states, names, n);
    trackingMode = IUFindOnSwitchIndex(&TrackModeSP);
	  
    if (isSimulation() == false && ( err = selectAPTrackingMode(PortFD, trackingMode) < 0) )
	{
        DEBUGF(INDI::Logger::DBG_ERROR, "Error setting tracking mode (%d).", err);
        return false;
	}     
	TrackModeSP.s = IPS_OK;
	IDSetSwitch(&TrackModeSP, NULL);

    /* What is this for?
    if( trackingMode != 3) // not zero
	{
	    AbortSlewSP.s = IPS_IDLE;
	    IDSetSwitch(&AbortSlewSP, NULL);
	}
    */

    return true;
    }


    // =======================================
    // Swap Buttons
    // =======================================
    if (!strcmp(name, SwapSP.name))
    {
	int currentSwap ;

	IUResetSwitch(&SwapSP);
	IUUpdateSwitch(&SwapSP, states, names, n);
    currentSwap = IUFindOnSwitchIndex(&SwapSP);

    if((isSimulation() == false && (err = swapAPButtons(PortFD, currentSwap)) < 0) )
	{
        DEBUGF(INDI::Logger::DBG_ERROR, "Error swapping buttons (%d).", err);
        return false;
	}

	SwapS[0].s= ISS_OFF ;
	SwapS[1].s= ISS_OFF ;
	SwapSP.s = IPS_OK;
	IDSetSwitch(&SwapSP, NULL);
    return true ;
    }

    // =======================================
    // Swap to rate
    // =======================================
    if (!strcmp (name, MoveToRateSP.name))
    {
	int moveToRate ;

	IUResetSwitch(&MoveToRateSP);
	IUUpdateSwitch(&MoveToRateSP, states, names, n);
    moveToRate = IUFindOnSwitchIndex(&MoveToRateSP);
	  
    if (isSimulation() == false && ( err = selectAPMoveToRate(PortFD, moveToRate) < 0) )
	{
        DEBUGF(INDI::Logger::DBG_ERROR, "Error setting move to rate (%d).", err);
        return false;
	}
	MoveToRateSP.s = IPS_OK;
	IDSetSwitch(&MoveToRateSP, NULL);
    return true;
    }

    // =======================================
    // Slew Rate
    // =======================================
    if (!strcmp (name, SlewRateSP.name))
    {
	int slewRate ;
	
	IUResetSwitch(&SlewRateSP);
	IUUpdateSwitch(&SlewRateSP, states, names, n);
    slewRate = IUFindOnSwitchIndex(&SlewRateSP);
	  
    if (isSimulation() == false && ( err = selectAPSlewRate(PortFD, slewRate) < 0) )
	{
        DEBUGF(INDI::Logger::DBG_ERROR, "Error setting slew rate (%d).", err);
        return false;
	}
	          
	SlewRateSP.s = IPS_OK;
	IDSetSwitch(&SlewRateSP, NULL);
    return true;
    }

    // =======================================
    // Choose the appropriate sync command
    // =======================================
    if (!strcmp(name, SyncCMRSP.name))
    {
        int currentSync ;

        IUResetSwitch(&SyncCMRSP);
        IUUpdateSwitch(&SyncCMRSP, states, names, n);
        currentSync = IUFindOnSwitchIndex(&SyncCMRSP);
        SyncCMRSP.s = IPS_OK;
        IDSetSwitch(&SyncCMRSP, NULL);
        return true;
    }


    return LX200Generic::ISNewSwitch (dev, name, states, names,  n);
 }

bool LX200AstroPhysics::isMountInit(void)
{
    return (StartUpSP.s != IPS_IDLE);
}

bool LX200AstroPhysics::ReadScopeStatus()
{
     int ret ;
     if (!isMountInit())
        return false;

     //============================================================
     // #1 Call LX200Generic ReadScopeStatus
     //============================================================
     bool rc = LX200Generic::ReadScopeStatus();


     //============================================================
     // #2 Get Sidereal Time
     //============================================================
     if (isSimulation() == false)
     {
        if(( ret= getSDTime( PortFD, &APSiderealTimeN[0].value)) == 0)
        {
            APSiderealTimeNP.s = IPS_OK ;
         }
         else
         {
            APSiderealTimeNP.s = IPS_ALERT ;
         }
         IDSetNumber(&APSiderealTimeNP, NULL);
     }


     //============================================================
     // #3
     //============================================================
     if (isSimulation() == false)
     {
        if(( ret= getAPDeclinationAxis( PortFD, DeclinationAxisT[0].text)) == 0)
        {
        DeclinationAxisTP.s = IPS_OK ;
        }
        else
         {
         DeclinationAxisTP.s = IPS_ALERT ;
         }
         IDSetText(&DeclinationAxisTP, NULL) ;
     }
   
    /* Calculate the hour angle */
     HourangleCoordsNP.np[0].value= 180. /M_PI/15. * LDRAtoHA( 15.* currentRA /180. *M_PI, -LocationNP.np[1].value/180. *M_PI);
     HourangleCoordsNP.np[1].value= currentDEC;

     HourangleCoordsNP.s = IPS_OK;
     IDSetNumber(&HourangleCoordsNP, NULL);

     /*getLX200Az(fd, &currentAZ);
     getLX200Alt(fd, &currentALT);
     IDSetNumber(&HorizontalCoordsNP, NULL);*/

     return rc;
}

bool LX200AstroPhysics::setBasicDataPart0()
{
    int err ;
    //struct ln_date utm;
    //struct ln_zonedate ltm;

    if (isSimulation() == true)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "setBasicDataPart0 simulation complete.");
        return true;
    }

    if( (err = setAPClearBuffer( PortFD)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error clearing the buffer (%d).", err);
        return false;
    }

    if( (err = setAPLongFormat( PortFD)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error setting long format failed (%d).", err);
        return false;
    }
    
    if( (err = setAPBackLashCompensation(PortFD, 0,0,0)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error setting back lash compensation (%d).", err);
        return false;
    }

    /*
    ln_get_date_from_sys( &utm) ;
    ln_date_to_zonedate(&utm, &ltm, 3600);

    if((  err = setLocalTime(PortFD, ltm.hours, ltm.minutes, (int) ltm.seconds) < 0))
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error setting local time (%d).", err);
        return -1;
    }
    if ( ( err = setCalenderDate(PortFD, ltm.days, ltm.months, ltm.years) < 0) )
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error setting local date (%d).", err);
        return -1;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "UT time is: %04d/%02d/%02d T %02d:%02d:%02d", utm.years, utm.months, utm.days, utm.hours, utm.minutes, (int)utm.seconds);
    DEBUGF(INDI::Logger::DBG_DEBUG, "Local time is: %04d/%02d/%02d T %02d:%02d:%02d", ltm.years, ltm.months, ltm.days, ltm.hours, ltm.minutes, (int)ltm.seconds);
    */

    return true;
}

bool LX200AstroPhysics::setBasicDataPart1()
{
    int err ;

    if (isSimulation() == true)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "setBasicDataPart1 simulation complete.");
        return true;
    }

    if((err = setAPUnPark( PortFD)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error unparking failed (%d).", err) ;
        return -1;
    }

    DEBUG(INDI::Logger::DBG_DEBUG, "Unparking successful");

    if((err = setAPMotionStop( PortFD)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Stop motion (:Q#) failed, check the mount (%d).", err) ;
        return -1;
    }

    return true ;
}

#if 0
void LX200AstroPhysics::connectTelescope()
{
    static int established= NOTESTABLISHED ;

    switch (ConnectSP.sp[0].s)
    {
	case ISS_ON:  
	
	    if( ! established)
	    {
		if (tty_connect(PortTP.tp[0].text, 9600, 8, 0, 1, &fd) != TTY_OK)
		{
		    ConnectSP.sp[0].s = ISS_OFF;
		    ConnectSP.sp[1].s = ISS_ON;
		    IDSetSwitch (&ConnectSP, "Error connecting to port %s. Make sure you have BOTH write and read permission to your port.\n", PortTP.tp[0].text);
		    established= NOTESTABLISHED ;
		    return;
		}
		if (check_lx200ap_connection(fd))
		{   
		    ConnectSP.sp[0].s = ISS_OFF;
		    ConnectSP.sp[1].s = ISS_ON;
		    IDSetSwitch (&ConnectSP, "Error connecting to Telescope. Telescope is offline.");
		    established= NOTESTABLISHED ;
		    return;
		}
		established= ESTABLISHED ;
		#ifdef INDI_DEBUG
		IDLog("Telescope test successful\n");
		#endif
		// At this point e.g. no GEOGRAPHIC_COORD are available after the first client connects.
		// Postpone set up of the telescope
		// NO setBasicData() ;

		// ToDo what is that *((int *) UTCOffsetN[0].aux0) = 0;
                // Jasem: This is just a way to know if the client has init UTC Offset, and if he did, then we set aux0 to 1, otherwise it stays at 0. When
                // the client tries to change UTC, but has not set UTFOffset yet (that is, aux0 = 0) then we can tell him to set the UTCOffset first. Btw,
		// the UTF & UTFOffset will be merged in one property for INDI v0.6
		ConnectSP.s = IPS_OK;
		IDSetSwitch (&ConnectSP, "Telescope is online");
	    }
	    else
	    {
		ConnectSP.s = IPS_OK;
		IDSetSwitch (&ConnectSP, "Connection already established.");
	    }
	    break;

	case ISS_OFF:
	    ConnectSP.sp[0].s = ISS_OFF;
	    ConnectSP.sp[1].s = ISS_ON;
	    ConnectSP.s = IPS_IDLE;
	    IDSetSwitch (&ConnectSP, "Telescope is offline.");
	    if (setAPPark(fd) < 0)
	    {
		ParkSP.s = IPS_ALERT;
		IDSetSwitch(&ParkSP, "Parking Failed.");
	    return;
	    }

	    ParkSP.s = IPS_OK;
	    IDSetSwitch(&ParkSP, "The telescope is parked. Turn off the telescope. Disconnecting...");

	    tty_disconnect(fd);
	    established= NOTESTABLISHED ;
#ifdef INDI_DEBUG
	    IDLog("The telescope is parked. Turn off the telescope. Disconnected.\n");
#endif
	    break;
    }
}
#endif

#if 0
// taken from lx200_16
void LX200AstroPhysics::handleAltAzSlew()
{
    int i=0;
    char altStr[64], azStr[64];

    if (HorizontalCoordsNP.s == IPS_BUSY)
    {
        abortSlew(fd);
        AbortSlewSP.s = IPS_OK;
        IDSetSwitch(&AbortSlewSP, NULL);

        // sleep for 100 mseconds
        usleep(100000);
    }

    // ToDo is it ok ?
    //    if ((i = slewToAltAz(fd)))
    if ((i = Slew(fd)))
    {
        HorizontalCoordsNP.s = IPS_ALERT;
        IDSetNumber(&HorizontalCoordsNP, "Slew is not possible.");
        return;
    }

    HorizontalCoordsNP.s = IPS_OK;
    fs_sexa(azStr, targetAZ, 2, 3600);
    fs_sexa(altStr, targetALT, 2, 3600);

    IDSetNumber(&HorizontalCoordsNP, "Slewing to Alt %s - Az %s", altStr, azStr);

    TrackState = SCOPE_SLEWING;

    return;
}
#endif

#if 0
void LX200AstroPhysics::handleEqCoordSet()
{

    int sync ;
    int  err;
    char syncString[256];
    char RAStr[32], DecStr[32];
    double dx, dy;
    int syncOK= 0 ;
    double targetHA ;

#ifdef INDI_DEBUG
    IDLog("In Handle AP EQ Coord Set(), switch %d\n", getOnSwitch(&OnCoordSetSP));
#endif
    switch (getOnSwitch(&OnCoordSetSP))
    {
	// Slew
	case LX200_TRACK:
	    lastSet = LX200_TRACK;
        if (EquatorialCoordsRNP.s == IPS_BUSY)
	    {
#ifdef INDI_DEBUG
		IDLog("Aborting Track\n");
#endif
                // ToDo
		abortSlew(fd);
		AbortSlewSP.s = IPS_OK;
		IDSetSwitch(&AbortSlewSP, NULL);

		// sleep for 100 mseconds
		usleep(100000);
	    }
	    if ((err = Slew(fd))) /* Slew reads the '0', that is not the end of the slew */
	    {
		slewError(err);
		// ToDo handle that with the handleError function
		return ;
	    }
        EquatorialCoordsRNP.s = IPS_BUSY;
	    fs_sexa(RAStr,  targetRA, 2, 3600);
	    fs_sexa(DecStr, targetDEC, 2, 3600);
        IDSetNumber(&EquatorialCoordsRNP, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
#ifdef INDI_DEBUG
	    IDLog("Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
#endif
	    break;


	    // Sync
	case LX200_SYNC:

	    lastSet = LX200_SYNC;

/* Astro-Physics has two sync options. In order that no collision occurs, the SYNCCMR */
/* variant behaves for now identical like SYNCCM. Later this feature will be enabled.*/ 
/* Calculate the hour angle of the target */

	    targetHA= 180. /M_PI/15. * LDRAtoHA( 15.*  targetRA/180. *M_PI, -geoNP.np[1].value/180. *M_PI);

	    if((sync=getOnSwitch(&SyncCMRSP))==SYNCCMR)
	    {
		if (!strcmp("West", DeclinationAxisT[0].text))
		{
		    if(( targetHA > 12.0) && ( targetHA <= 24.0))
		    {
			syncOK= 1 ;
		    }
		    else
		    {
			syncOK= 0 ;
		    }
		}
		else if (!strcmp("East", DeclinationAxisT[0].text))
		{
		    if(( targetHA >= 0.0) && ( targetHA <= 12.0))
		    {
			syncOK= 1 ;
		    }
		    else
		    {
			syncOK= 0 ;
		    }
		}
		else
		{
#ifdef INDI_DEBUG
		    IDLog("handleEqCoordSet(): SYNC NOK not East or West\n") ;
#endif
		    return ;
		}
	    }
	    else if((sync=getOnSwitch(&SyncCMRSP))==SYNCCM)
	    {
		syncOK = 1 ;
	    }
	    else
	    {
#ifdef INDI_DEBUG
		IDLog("handleEqCoordSet(): SYNC NOK not SYNCCM or SYNCCMR\n") ;
#endif
		return ;
	    }
	    if( syncOK == 1)
	    {
		if( (sync=getOnSwitch(&SyncCMRSP))==SYNCCM)
		{
		    if ( ( err = APSyncCM(fd, syncString) < 0) )
		    {
            EquatorialCoordsRNP.s = IPS_ALERT ;
            IDSetNumber( &EquatorialCoordsRNP , "Synchronization failed.");
			// ToDo handle with handleError function
			return ;
		    }
		}
		else if((sync=getOnSwitch(&SyncCMRSP))==SYNCCMR)
		{
		    if ( ( err = APSyncCMR(fd, syncString) < 0) )
		    {
            EquatorialCoordsRNP.s = IPS_ALERT ;
            IDSetNumber( &EquatorialCoordsRNP, "Synchronization failed.");
			// ToDo handle with handleError function
			return ;
		    }
		}
		else
		{
            EquatorialCoordsRNP.s = IPS_ALERT ;
            IDSetNumber( &EquatorialCoordsRNP , "SYNC NOK no valid SYNCCM, SYNCCMR");
#ifdef INDI_DEBUG
		    IDLog("SYNC NOK no valid SYNCCM, SYNCCMR\n") ;
#endif
		    return ;
		}
/* get the property DeclinationAxisTP first */
		if(( err = getAPDeclinationAxis( fd, DeclinationAxisT[0].text)) < 0)
		{
		    //ToDo handleErr
		    DeclinationAxisTP.s = IPS_ALERT ;
		    IDSetText(&DeclinationAxisTP, "Declination axis undefined") ;  
		    return ;
		}

		DeclinationAxisTP.s = IPS_OK ;
#ifdef INDI_DEBUG
		IDLog("Declination axis is on the %s side\n", DeclinationAxisT[0].text) ;
#endif
		IDSetText(&DeclinationAxisTP, NULL) ; 

                getLX200RA( fd, &currentRA); 
                getLX200DEC(fd, &currentDEC);
// The mount executed the sync command, now read back the values, make a IDSet in order the dome controller
// is aware of the new target
                targetRA= currentRA ;
                targetDEC= currentDEC ;
                EquatorialCoordsRNP.s = IPS_BUSY; /* dome controller sets target only if this state is busy */
                IDSetNumber(&EquatorialCoordsRNP,  "EquatorialCoordsWNP.s = IPS_BUSY after SYNC");
	    }
	    else
	    {
#ifdef INDI_DEBUG
		IDLog("Synchronization not allowed\n") ;
#endif

        EquatorialCoordsRNP.s = IPS_ALERT;
        IDSetNumber(&EquatorialCoordsRNP,  "Synchronization not allowed" );

#ifdef INDI_DEBUG
                    IDLog("Telescope is on the wrong side targetHA was %f\n", targetHA) ;
#endif
		DeclinationAxisTP.s = IPS_ALERT ;
		IDSetText(&DeclinationAxisTP, "Telescope is on the wrong side targetHA was %f", targetHA) ;

		return ;
	    }
#ifdef INDI_DEBUG
	    IDLog("Synchronization successful >%s<\n", syncString);
#endif
        EquatorialCoordsRNP.s = IPS_OK; /* see above for dome controller dc */
        IDSetNumber(&EquatorialCoordsRNP, "Synchronization successful, EquatorialCoordsWNP.s = IPS_OK after SYNC");
	    break;
    }
   return ;

}
#endif

#if 0
// ToDo Not yet used
void LX200AstroPhysics::handleAZCoordSet()
{
    int  err;
    char AZStr[32], AltStr[32];

    switch (getOnSwitch(&OnCoordSetSP))
    {
	// Slew
	case LX200_TRACK:
	    lastSet = LX200_TRACK;
        if (HorizontalCoordsNP.s == IPS_BUSY)
	    {
#ifdef INDI_DEBUG
		IDLog("Aborting Slew\n");
#endif
		// ToDo
		abortSlew(fd);
		AbortSlewSP.s = IPS_OK;
		IDSetSwitch(&AbortSlewSP, NULL);
		// sleep for 100 mseconds
		usleep(100000);
	    }

	    if ((err = Slew(fd)))
	    {
		slewError(err);
		//ToDo handle it

		return ;
	    }

        HorizontalCoordsNP.s = IPS_BUSY;
	    fs_sexa(AZStr, targetAZ, 2, 3600);
	    fs_sexa(AltStr, targetALT, 2, 3600);
        IDSetNumber(&HorizontalCoordsNP, "Slewing to AZ %s - Alt %s", AZStr, AltStr);
#ifdef INDI_DEBUG
	    IDLog("Slewing to AZ %s - Alt %s\n", AZStr, AltStr);
#endif
	    break;

	    // Sync
	    /* ToDo DO SYNC */
	case LX200_SYNC:
	    IDMessage( myapdev, "Sync not supported in ALT/AZ mode") ;

	    break;
    }
}
#endif

/*********Library Section**************/
/*********Library Section**************/
/*********Library Section**************/
/*********Library Section**************/

double LDRAtoHA( double RA, double longitude)
{
#ifdef HAVE_NOVA_H
    double HA ;
    double JD ;
    double theta_0= 0. ;
    JD=  ln_get_julian_from_sys() ;

//    #ifdef INDI_DEBUG
//    IDLog("LDRAtoHA: JD %f\n", JD);
//    #endif

    theta_0= 15. * ln_get_mean_sidereal_time( JD) ;
//    #ifdef INDI_DEBUG
//    IDLog("LDRAtoHA:1 theta_0 %f\n", theta_0);
//    #endif
    theta_0= fmod( theta_0, 360.) ;

    theta_0=  theta_0 /180. * M_PI ;

    HA =  fmod(theta_0 - longitude - RA,  2. * M_PI) ;

    if( HA < 0.)
    {
	HA += 2. * M_PI ;
    }
    return HA ;
#else
    IDMessage( myapdev, "Initialize %s manually or install libnova", myapdev) ;
    return 0 ;
#endif

} 

bool LX200AstroPhysics::Goto(double r,double d)
{
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // If moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
         if (!isSimulation() && abortSlew(PortFD) < 0)
         {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
         }

         AbortSP.s = IPS_OK;
         EqNP.s       = IPS_IDLE;
         IDSetSwitch(&AbortSP, "Slew aborted.");
         IDSetNumber(&EqNP, NULL);

         if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
         {
                MovementNSSP.s  = MovementWESP.s =  IPS_IDLE;
                EqNP.s       = IPS_IDLE;
                IUResetSwitch(&MovementNSSP);
                IUResetSwitch(&MovementWESP);
                IDSetSwitch(&MovementNSSP, NULL);
                IDSetSwitch(&MovementWESP, NULL);
          }

       // sleep for 100 mseconds
       usleep(100000);
    }

    if (isSimulation() == false)
    {
        if (setAPObjectRA(PortFD, targetRA) < 0 || (setAPObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error setting RA/DEC.");
            return false;
        }

        int err=0;
        /* Slew reads the '0', that is not the end of the slew */
        if (err = Slew(PortFD))
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
            slewError(err);
            return  false;
        }
    }

    TrackState = SCOPE_SLEWING;
    EqNP.s    = IPS_BUSY;

    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool LX200AstroPhysics::Park()
{
    if (isSimulation() == false)
    {
        // If scope is moving, let's stop it first.
        if (EqNP.s == IPS_BUSY)
        {
           if (abortSlew(PortFD) < 0)
           {
              AbortSP.s = IPS_ALERT;
              IDSetSwitch(&AbortSP, "Abort slew failed.");
              return false;
            }

             AbortSP.s    = IPS_OK;
             EqNP.s       = IPS_IDLE;
             IDSetSwitch(&AbortSP, "Slew aborted.");
             IDSetNumber(&EqNP, NULL);

             if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
             {
                   MovementNSSP.s  = MovementWESP.s =  IPS_IDLE;
                   EqNP.s       = IPS_IDLE;
                   IUResetSwitch(&MovementNSSP);
                   IUResetSwitch(&MovementWESP);

                   IDSetSwitch(&MovementNSSP, NULL);
                   IDSetSwitch(&MovementWESP, NULL);
              }

             // sleep for 100 msec
             usleep(100000);
         }

          if (setAPPark(PortFD) < 0)
          {
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, "Parking Failed.");
            return false;
          }

    }

    ParkSP.s = IPS_OK;
    TrackState = SCOPE_PARKING;
    DEBUG(INDI::Logger::DBG_SESSION, "Telescope parking in progress...");
    return true;
}

bool LX200AstroPhysics::Connect(char *port)
{
    if (isSimulation())
    {
        IDMessage (getDeviceName(), "Simulated Astrophysics is online. Retrieving basic data...");
        return true;
    }

    if (tty_connect(port, 9600, 8, 0, 1, &PortFD) != TTY_OK)
    {
      DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to port %s. Make sure you have BOTH write and read permission to your port.", port);
      return false;
    }

    if (check_lx200ap_connection(PortFD))
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error connecting to Telescope. Telescope is offline.");
        return false;
    }

   DEBUG(INDI::Logger::DBG_SESSION, "Telescope is online.");

   setBasicDataPart0();

   return true;
}

bool LX200AstroPhysics::Disconnect()
{
    timeUpdated = false;
    locationUpdated = false;

    return LX200Generic::Disconnect();
}

bool LX200AstroPhysics::Sync(double ra, double dec)
{
    char syncString[256];

    int i = IUFindOnSwitchIndex(&SyncCMRSP);

    if (i == 0)
        return LX200Generic::Sync(ra, dec);

    if (isSimulation() == false &&
            (setObjectRA(PortFD, ra) < 0 || (setObjectDEC(PortFD, dec)) < 0))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error setting RA/DEC. Unable to Sync.");
        return false;
    }

    if (isSimulation() == false && APSyncCMR(PortFD, syncString) < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP , "Synchronization failed.");
        return false;
    }

    currentRA  = ra;
    currentDEC = dec;

    DEBUGF(INDI::Logger::DBG_DEBUG, "Synchronization successful %s", syncString);

    DEBUG(INDI::Logger::DBG_SESSION, "Synchronization successful.");

    TrackState = SCOPE_IDLE;
    EqNP.s    = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200AstroPhysics::updateTime(ln_date * utc, double utc_offset)
{
    struct ln_zonedate ltm;

    if (isSimulation())
        return true;

    ln_date_to_zonedate(utc, &ltm, utc_offset*3600.0);

    JD = ln_get_julian_day(utc);

    DEBUGF(INDI::Logger::DBG_DEBUG, "New JD is %f", (float) JD);

        // Set Local Time
        if (setLocalTime(PortFD, ltm.hours, ltm.minutes, ltm.seconds) < 0)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error setting local time.");
            return false;
        }

      if (setCalenderDate(PortFD, ltm.days, ltm.months, ltm.years) < 0)
      {
          DEBUG(INDI::Logger::DBG_ERROR, "Error setting local date.");
          return false;
      }

    if (setAPUTCOffset(PortFD, (utc_offset * -1)) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting UTC Offset.");
        return false;
    }

   DEBUG(INDI::Logger::DBG_SESSION, "Time updated.");

   timeUpdated = true;

   return true;
}

bool LX200AstroPhysics::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    if (isSimulation() == false && setAPSiteLongitude(PortFD, 360.0 - longitude) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting site longitude coordinates");
        return false;
    }

    if (isSimulation() == false && setAPSiteLatitude(PortFD, latitude) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting site latitude coordinates");
        return false;
    }

    char l[32], L[32];
    fs_sexa (l, latitude, 3, 3600);
    fs_sexa (L, longitude, 4, 3600);

    IDMessage(getDeviceName(), "Site location updated to Lat %.32s - Long %.32s", l, L);

    locationUpdated = true;

    return true;
}

void LX200AstroPhysics::debugTriggered(bool enable)
{
   setLX200Debug(enable ? 1 : 0);
   set_lx200ap_debug(enable ? 1 : 0);
}

void LX200AstroPhysics::processButton(const char * button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    // Max Slew speed
    if (!strcmp(button_n, "SLEW_MAX"))
    {
        selectAPMoveToRate(PortFD, 0);
        IUResetSwitch(&MoveToRateSP);
        MoveToRateS[0].s = ISS_ON;
        IDSetSwitch(&MoveToRateSP, NULL);
    }
    // Find Slew speed
    else if (!strcmp(button_n, "SLEW_FIND"))
    {
        selectAPMoveToRate(PortFD, 1);
        IUResetSwitch(&MoveToRateSP);
        MoveToRateS[1].s = ISS_ON;
        IDSetSwitch(&MoveToRateSP, NULL);
    }
    // Centering Slew
    else if (!strcmp(button_n, "SLEW_CENTERING"))
    {
        selectAPMoveToRate(PortFD, 2);
        IUResetSwitch(&MoveToRateSP);
        MoveToRateS[2].s = ISS_ON;
        IDSetSwitch(&MoveToRateSP, NULL);

    }
    // Guide Slew
    else if (!strcmp(button_n, "SLEW_GUIDE"))
    {
        selectAPMoveToRate(PortFD, 3);
        IUResetSwitch(&MoveToRateSP);
        MoveToRateS[3].s = ISS_ON;
        IDSetSwitch(&MoveToRateSP, NULL);
    }
    // Abort
    else if (!strcmp(button_n, "Abort Motion"))
    {
        // Only abort if we have some sort of motion going on
        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY || EqNP.s == IPS_BUSY
                || GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
        {

            Abort();
        }
    }

}




