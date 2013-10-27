/*
    LX200 Astro-Physics INDI driver, tested with controller software version D
    Copyright (C) 2007 Markus Wildi based on the work of Jasem Mutlaq 
    (mutlaqja@ikarustech.com)

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


//HAVE_NOVASCC_H: NOVAS-C, Version 2.0.1, Astronomical Applications Dept. 
//U.S. Naval Observatory, Washington, DC  20392-5420 http://aa.usno.navy.mil/AA/
//#define HAVE_NOVASCC_H
//HAVE_NOVA_H: http://libnova.sourceforge.net/
//#define HAVE_NOVA_H

#include "lx200ap.h"
#include "lx200driver.h"
#include "lx200apdriver.h"
#include "lx200aplib.h"

#include <config.h>

#ifdef HAVE_NOVA_H
#include <libnova.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COMM_TAB	 "Communication"
#define BASIC_TAB      "Main Control"
#define MOTION_TAB	 "Motion Control"
#define FIRMWARE_TAB   "Firmware data"
#define SETTINGS_TAB   "Settings"
#define ATMOSPHERE_TAB "Atmosphere"
#define MOUNT_TAB      "Mounting"

/* Handy Macros */
#define currentRA	EquatorialCoordsNP.np[0].value
#define currentDEC	EquatorialCoordsNP.np[1].value
#define currentAZ	HorizontalCoordsNP.np[0].value
#define currentALT	HorizontalCoordsNP.np[1].value

/* SNOOP */

#define CONTROL_TAB    "Control"

/* Communication */

static ISwitch DomeControlS[] = 
{
    {"ON" , "on" , ISS_ON, 0, 0},
    {"OFF" , "off" , ISS_OFF, 0, 0},
};

ISwitchVectorProperty DomeControlSP = 
{ 
    myapdev, "DOMECONTROL" , "Dome control", COMM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, DomeControlS, NARRAY(DomeControlS), "", 0
};

static ISwitch StartUpS[] = 
{
    {"COLD" , "cold" , ISS_OFF, 0, 0},
    {"WARM" , "warm" , ISS_ON, 0, 0},
};

ISwitchVectorProperty StartUpSP = 
{ 
    myapdev, "STARTUP" , "Mount init.", COMM_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, StartUpS, NARRAY(StartUpS), "", 0
};

/* Main control */
#if defined HAVE_NOVA_H || defined HAVE_NOVASCC_H
static ISwitch ApparentToObservedS[] = 
{
    {"NCTC" , "identity" , ISS_ON, 0, 0},
    {"NATR" , "app. to refracted" , ISS_OFF, 0, 0},
    {"NARTT" , "app., refr., telescope" , ISS_OFF, 0, 0},
    {"NARTTO" , "app., refr., tel., observed" , ISS_OFF, 0, 0},
};

ISwitchVectorProperty ApparentToObservedSP = 
{ 
    myapdev, "TRANSFORMATION" , "Transformation", BASIC_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ApparentToObservedS, NARRAY(ApparentToObservedS), "", 0
};
#endif

static INumber HourangleCoordsN[] = 
{
    {"HA",  "HA H:M:S", "%10.6m",  0.,  24., 0., 0., 0, 0, 0},
    {"Dec",  "Dec D:M:S", "%10.6m",  -90.,  90., 0., 0., 0, 0, 0},
};
static INumberVectorProperty  HourangleCoordsNP = 
{
    myapdev, "HOURANGLE_COORD", "Hourangle Coords", BASIC_TAB, IP_RO, 0., IPS_IDLE, HourangleCoordsN, NARRAY( HourangleCoordsN), "", 0
};
static INumber HorizontalCoordsN[] =
{
    {"AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0., 0, 0, 0},
    {"ALT",  "Alt  D:M:S", "%10.6m",  -90., 90., 0., 0., 0, 0, 0}
};
static INumberVectorProperty HorizontalCoordsNP =
{
    myapdev, "HORIZONTAL_COORD", "Horizontal Coords", BASIC_TAB, IP_RW, 120, IPS_IDLE, HorizontalCoordsN, NARRAY(HorizontalCoordsN), "", 0
};

/* Difference of the equatorial coordinates, used to estimate the applied corrections */
static INumber DiffEquatorialCoordsN[] = 
{
    {"RA",  "RA H:M:S", "%10.6m",  0.,  24., 0., 0., 0, 0, 0},
    {"Dec",  "Dec D:M:S", "%10.6m",  -90.,  90., 0., 0., 0, 0, 0},
};
static INumberVectorProperty  DiffEquatorialCoordsNP = 
{
    myapdev, "DIFFEQUATORIAL_COORD", "Diff. Eq.", BASIC_TAB, IP_RO, 0., IPS_IDLE, DiffEquatorialCoordsN, NARRAY( DiffEquatorialCoordsN), "", 0
};

static ISwitch TrackModeS[] = {
    {"LUNAR" , "lunar" , ISS_OFF, 0, 0},
    {"SOLAR" , "solar" , ISS_OFF, 0, 0},
    {"SIDEREAL", "sidereal" , ISS_OFF, 0, 0},
    {"ZERO" , "zero" , ISS_ON, 0, 0},
};
ISwitchVectorProperty TrackModeSP = 
{ 
    myapdev, "TRACKINGMODE" , "Tracking mode", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, TrackModeS, NARRAY(TrackModeS), "", 0
};
static ISwitch MoveToRateS[] = {
    {"1200" , "1200x" , ISS_OFF, 0, 0},
    {"600" , "600x" , ISS_OFF, 0, 0},
    {"64" , "64x" , ISS_ON, 0, 0},
    {"12" , "12x" , ISS_OFF, 0, 0},
};

ISwitchVectorProperty MoveToRateSP = 
{ 
    myapdev, "MOVETORATE" , "Move to rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, MoveToRateS, NARRAY(MoveToRateS), "", 0
};
static ISwitch SlewRateS[] = {
    {"1200" , "1200x" , ISS_OFF, 0, 0},
    {"900" , "900x" , ISS_OFF, 0, 0},
    {"600" , "600x" , ISS_ON, 0, 0},
};

ISwitchVectorProperty SlewRateSP = 
{ 
    myapdev, "SLEWRATE" , "Slew rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SlewRateS, NARRAY(SlewRateS), "", 0
};
static ISwitch SwapS[] = {
    {"NS" , "North/South" , ISS_OFF, 0, 0},
    {"EW" , "East/West" , ISS_OFF, 0, 0},
};

ISwitchVectorProperty SwapSP = 
{ 
    myapdev, "SWAP" , "Swap buttons", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SwapS, NARRAY(SwapS), "", 0
};
static ISwitch SyncCMRS[] = {
    {":CM#" , ":CM#" , ISS_ON, 0, 0},
    {":CMR#" , ":CMR#" , ISS_OFF, 0, 0},
};

ISwitchVectorProperty SyncCMRSP = 
{ 
    myapdev, "SYNCCMR" , "Sync", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SyncCMRS, NARRAY(SyncCMRS), "", 0
};

/* Firmware data */

static IText   VersionT[] =
{
// AP has only versionnumber
    { "Number", "", 0, 0, 0 ,0}
};

static ITextVectorProperty VersionInfo = 
{
    myapdev, "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE, VersionT, NARRAY(VersionT), "" ,0
};

/* Mount */

static IText   DeclinationAxisT[] =
{
    { "RELHA", "rel. to HA", 0, 0, 0, 0} ,
};

static ITextVectorProperty DeclinationAxisTP = 
{
    myapdev, "DECLINATIONAXIS", "Declination axis", MOUNT_TAB, IP_RO, 0, IPS_IDLE, DeclinationAxisT, NARRAY(DeclinationAxisT), "" ,0
};
static INumber APLocalTimeN[] = 
{
    {"VALUE",  "H:M:S", "%10.6m",  0.,  24., 0., 0., 0, 0, 0},
};
static INumberVectorProperty APLocalTimeNP = 
{
    myapdev, "APLOCALTIME", "AP local time", MOUNT_TAB, IP_RO, 0., IPS_IDLE, APLocalTimeN, NARRAY(APLocalTimeN), "", 0
};
static INumber APSiderealTimeN[] = 
{
    {"VALUE",  "H:M:S", "%10.6m",  0.,  24., 0., 0., 0, 0, 0},
};
static INumberVectorProperty APSiderealTimeNP = 
{
    myapdev, "APSIDEREALTIME", "AP sidereal time", MOUNT_TAB, IP_RO, 0., IPS_IDLE, APSiderealTimeN, NARRAY(APSiderealTimeN), "", 0
};

static INumber APUTCOffsetN[] = 
{
    {"VALUE",  "H:M:S", "%10.6m",  0.,  24., 0., 0., 0, 0, 0},
};
static INumberVectorProperty APUTCOffsetNP = 
{
    myapdev, "APUTCOFFSET", "AP UTC Offset", MOUNT_TAB, IP_RW, 0., IPS_IDLE, APUTCOffsetN, NARRAY(APUTCOffsetN), "", 0
};

static INumber HourAxisN[] = 
{
    {"THETA",  "Theta D:M:S", "%10.6m",  0.,  360., 0., 0., 0, 0, 0}, // 0., it points to the apparent pole
    {"GAMMA",  "Gamma D:M:S", "%10.6m",  0.,  90., 0., 0., 0, 0, 0},
};
static INumberVectorProperty HourAxisNP = 
{
    myapdev, "HOURAXIS", "Hour axis", MOUNT_TAB, IP_RW, 0., IPS_IDLE, HourAxisN, NARRAY(HourAxisN), "", 0
};

/* Atmosphere */
static INumber AirN[] = 
{
    {"TEMPERATURE",  "Temperature K", "%10.2f",  0.,  383.1, 0., 283.1, 0, 0, 0},
    {"PRESSURE",  "Pressure hPa", "%10.2f",  0.,  1300., 0., 975., 0, 0, 0},
    {"HUMIDITY",  "Humidity Perc.", "%10.2f",  0.,  100., 0., 70., 0, 0, 0},
};
static INumberVectorProperty AirNP = 
{
    myapdev, "ATMOSPHERE", "Atmosphere", ATMOSPHERE_TAB, IP_RW, 0., IPS_IDLE, AirN, NARRAY(AirN), "", 0
};

/* Settings Group */
static IText   ConnectionDCODT[] =
{
    { "DEVICE", "Device", 0, 0, 0, 0} ,
    { "PROPERTY", "Property", 0, 0, 0, 0}
};
static ITextVectorProperty ConnectionDCODTP = 
{
    myapdev, "SNOOPCONNECTIONDC", "Snoop dc connection", SETTINGS_TAB, IP_RW, 0, IPS_IDLE, ConnectionDCODT, NARRAY(ConnectionDCODT), "" ,0
};
static IText MasterAlarmODT[]= 
{
    { "DEVICE", "Device", 0, 0, 0, 0} ,
    { "PROPERTY", "Property", 0, 0, 0, 0}
};
static ITextVectorProperty MasterAlarmODTP = 
{
    myapdev, "SNOOPMASTERALARM", "Snoop dc master alarm", SETTINGS_TAB, IP_RW, 0, IPS_IDLE, MasterAlarmODT, NARRAY(MasterAlarmODT), "" ,0
};
static IText   ModeDCODT[] =
{
    { "DEVICE", "Device", 0, 0, 0, 0} ,
    { "PROPERTY", "Property", 0, 0, 0, 0}
};
static ITextVectorProperty ModeDCODTP = 
{
    myapdev, "SNOOPMODEDC", "Snoop dc mode", SETTINGS_TAB, IP_RW, 0, IPS_IDLE, ModeDCODT, NARRAY(ModeDCODT), "" ,0
};
/********************************************
 Property: Park telescope to HOME
*********************************************/

static ISwitch ParkS[] = 
{ 
    {"PARK", "Park", ISS_OFF, 0, 0}, 
};
static ISwitchVectorProperty ParkSP = 
{
    myapdev, "TELESCOPE_PARK", "Park Scope", BASIC_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE, ParkS, NARRAY(ParkS), "", 0
};

/* SNOOPed properties */

static ISwitch ConnectionDCS[] = 
{  
  {"CONNECT", "Connect", ISS_OFF, 0, 0},                         
  {"DISCONNECT", "Disconnect", ISS_OFF, 0, 0},
};

static ISwitchVectorProperty ConnectionDCSP = 
{ 
  "dc", "CONNECTION", "Connection", CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ConnectionDCS, NARRAY(ConnectionDCS), "", 0
};	

static ISwitch MasterAlarmS[] = 
{  
  {"OFF", "off", ISS_OFF, 0, 0},                         
  {"DANGER", "danger", ISS_OFF, 0, 0},
  {"ON", "ON", ISS_OFF, 0, 0},
  {"RESET", "reset", ISS_OFF, 0, 0},
};

static ISwitchVectorProperty MasterAlarmSP = 
{ 
  "dc", "MASTERALARM", "Master alarm", CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, MasterAlarmS, NARRAY(MasterAlarmS), "", 0
};
static ISwitch ModeS[] = {  
  {"MANUAL", "manual", ISS_ON, 0, 0},                         
  {"DOMECONTROL", "dome control", ISS_OFF, 0, 0}
};

static ISwitchVectorProperty ModeSP = { 
  "dc", "MODE", "Mode", CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ModeS, NARRAY(ModeS), "", 0
};	


/* Constructor */

LX200AstroPhysics::LX200AstroPhysics() : LX200Generic()
{
    const char dev[]= "/dev/apmount" ;
    const char status[]= "undefined" ;
    IUSaveText(&ConnectionDCODT[0], ConnectionDCSP.device);
    IUSaveText(&ConnectionDCODT[1], ConnectionDCSP.name);
    IUSaveText(&MasterAlarmODT[0], MasterAlarmSP.device);
    IUSaveText(&MasterAlarmODT[1], MasterAlarmSP.name);
    IUSaveText(&ModeDCODT[0], ModeSP.device);
    IUSaveText(&ModeDCODT[1], ModeSP.name);


    IUSaveText(&PortTP.tp[0], dev);
    IUSaveText(&DeclinationAxisTP.tp[0], status);
}
void LX200AstroPhysics::ISGetProperties (const char *dev)
{
    if (dev && strcmp (thisDevice, dev))
	return;

    LX200Generic::ISGetProperties(dev);

    IDDefSwitch (&ParkSP, NULL);
    IDDefText(&VersionInfo, NULL);

/* Communication group */

    // AstroPhysics has no alignment mode
    IDDelete(thisDevice, "Alignment", NULL);
    IDDefSwitch (&StartUpSP, NULL);
    IDDefSwitch (&DomeControlSP, NULL);

/* Main Group */
#if defined HAVE_NOVA_H || defined HAVE_NOVASCC_H
    IDDefSwitch(&ApparentToObservedSP, NULL);
#endif
    IDDefNumber(&HourangleCoordsNP, NULL) ;
    IDDefNumber(&HorizontalCoordsNP, NULL);
    IDDelete(thisDevice, "EQUATORIAL_EOD_COORD", NULL);
    IDDefNumber(&EquatorialCoordsNP, NULL);
    IDDefNumber(&DiffEquatorialCoordsNP, NULL);
/* Date&Time group */
    IDDelete(thisDevice, "TIME_UTC_OFFSET", NULL);
    IDDefText(&DeclinationAxisTP, NULL);
/* Mount group */

    IDDefText(&DeclinationAxisTP, NULL);
    IDDefNumber(&APLocalTimeNP, NULL);
    IDDefNumber(&APSiderealTimeNP, NULL);
    IDDefNumber(&APUTCOffsetNP, NULL);
    IDDefNumber(&HourAxisNP, NULL);

/* Atmosphere group */

    IDDefNumber   (&AirNP, NULL);

/* Settings group */

    IDDefText   (&ConnectionDCODTP, NULL);
    IDDefText   (&MasterAlarmODTP, NULL);
    IDDefText   (&ModeDCODTP, NULL);

    // AstroPhysics, we have no focuser, therefore, we don't need the classical one
    IDDelete(thisDevice, "FOCUS_MODE", NULL);
    IDDelete(thisDevice, "FOCUS_MOTION", NULL);
    IDDelete(thisDevice, "FOCUS_TIMER", NULL);

/* Motion group */
    IDDelete(thisDevice, "Slew rate", NULL);
    IDDelete(thisDevice, "Tracking Mode", NULL);
    IDDelete(thisDevice, "Tracking Frequency", NULL); /* AP does not have :GT, :ST commands */

    IDDefSwitch (&TrackModeSP, NULL);
    IDDefSwitch (&MovementNSSP, NULL);
    IDDefSwitch (&MovementWESP, NULL);
    IDDefSwitch (&MoveToRateSP, NULL);
    IDDefSwitch (&SlewRateSP, NULL);
    IDDefSwitch (&SwapSP, NULL);
    IDDefSwitch (&SyncCMRSP, NULL);

/* Site management */
    /* Astro Physics has no commands to retrieve the values */
    /* True for all three control boxes and the software  and C-KE1, G-L*/
    IDDelete(thisDevice, "Sites", NULL);
    IDDelete(thisDevice, "Site Name", NULL);

}

bool LX200AstroPhysics::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    IText *tp=NULL;

    // ignore if not ours //
    if (strcmp (dev, thisDevice))
	return;

    // ===================================
    // Snoop DC Connection
    // ===================================
    if (!strcmp(name, ConnectionDCODTP.name) )
    {
	tp = IUFindText( &ConnectionDCODTP, names[0] );
	if (!tp)
	    return;

	IUSaveText(tp, texts[0]);
	tp = IUFindText( &ConnectionDCODTP, names[1] );
	if (!tp)
	    return;

	ConnectionDCODTP.s = IPS_OK;
	IUSaveText(tp, texts[1]);

	IDSnoopDevice( ConnectionDCODT[0].text, ConnectionDCODT[1].text);

	ConnectionDCODTP.s= IPS_OK ;
	IDSetText (&ConnectionDCODTP, "Snooping property %s at device %s", ConnectionDCODT[1].text, ConnectionDCODT[0].text);
	return;
    }

    // ===================================
    // Master Alarm
    // ===================================
    if (!strcmp(name, MasterAlarmODTP.name) )
    {
	tp = IUFindText( &MasterAlarmODTP, names[0] );
	if (!tp)
	    return;
	    
	IUSaveText(tp, texts[0]);

	tp = IUFindText( &MasterAlarmODTP, names[1] );
	if (!tp)
	    return;
	IUSaveText(tp, texts[1]);

	IDSnoopDevice( MasterAlarmODT[0].text, MasterAlarmODT[1].text);

	MasterAlarmODTP.s= IPS_OK ;
	IDSetText (&MasterAlarmODTP, "Snooping property %s at device %s", MasterAlarmODT[1].text, MasterAlarmODT[0].text) ;
	return;
    }

    // ===================================
    // Snope DC Mode
    // ===================================
    if (!strcmp(name, ModeDCODTP.name) )
    {
	tp = IUFindText( &ModeDCODTP, names[0] );
	if (!tp)
	    return;

	IUSaveText(tp, texts[0]);
	tp = IUFindText( &ModeDCODTP, names[1] );
	if (!tp)
	    return;

	ModeDCODTP.s = IPS_OK;
	IUSaveText(tp, texts[1]);

	IDSnoopDevice( ModeDCODT[0].text, ModeDCODT[1].text);

	ModeDCODTP.s= IPS_OK ;
	IDSetText (&ModeDCODTP, "Snooping property %s at device %s", ModeDCODT[1].text, ModeDCODT[0].text);
	return;
    }

    return LX200Generic::ISNewText (dev, name, texts, names, n);
}


bool LX200AstroPhysics::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    int i=0, nset=0;
    INumber *np=NULL;

    // ignore if not ours
    if (strcmp (dev, thisDevice))
	    return;

    // ===================================
    // Atmosphere
    // ===================================
    if (!strcmp (name, AirNP.name))
    {
	double newTemperature ;
	double newPressure ;
	double newHumidity ;
	if (checkPower(&AirNP))
	    return;

	for (nset = i = 0; i < n; i++)
	{
	    np = IUFindNumber (&AirNP, names[i]);
	    if( np == &AirN[0])
	    {
		newTemperature = values[i];
		nset++ ;
	    } 
	    else if( np == &AirN[1])
	    {
		newPressure = values[i];
		nset++ ;
	    }
	    else if( np == &AirN[2])
	    {
		newHumidity = values[i];
		nset++  ;
	    }
	}

	if (nset == 3)
	{
	    AirNP.s = IPS_OK;
	    AirN[0].value = newTemperature;
	    AirN[1].value = newPressure;
	    AirN[2].value = newHumidity;

	    IDSetNumber(&AirNP, NULL);
	} 
	else
	{
	    AirNP.s = IPS_ALERT;
	    IDSetNumber(&AirNP, "Temperature, Pressure or Humidity missing or invalid");
	}
	return;
    }

    // ===================================
    // AP UTC Offset
    // ===================================
    if ( !strcmp (name, APUTCOffsetNP.name) )
    {
	int ret ;
        if (checkPower(&APUTCOffsetNP))
            return;

        if (values[0] <= 0.0 || values[0] > 24.0)
        {
            APUTCOffsetNP.s = IPS_IDLE;
            IDSetNumber(&APUTCOffsetNP , "UTC offset invalid");
            return;
        }

        if(( ret = setAPUTCOffset(fd, values[0]) < 0) )
        {
            handleError(&APUTCOffsetNP, ret, "Setting AP UTC offset");
            return;
        }

        APUTCOffsetN[0].value = values[0];
        APUTCOffsetNP.s = IPS_OK;

        IDSetNumber(&APUTCOffsetNP, NULL);

        return;
    }

    // =======================================
    // Hour axis' intersection with the sphere
    // =======================================
    if (!strcmp (name, HourAxisNP.name))
    {
        int i=0, nset=0;
	double newTheta, newGamma ;

        if (checkPower(&HourAxisNP))
            return;

        for (nset = i = 0; i < n; i++)
        {
            np = IUFindNumber (&HourAxisNP, names[i]);
            if ( np == &HourAxisN[0])
            {
                newTheta = values[i];
                nset += newTheta >= 0 && newTheta <= 360.0;
            }
	    else if ( np == &HourAxisN[1])
            {
                newGamma = values[i];
                nset += newGamma >= 0. && newGamma <= 90.0;
            }
        }

        if (nset == 2)
        {
            HourAxisNP.s = IPS_OK;
            HourAxisN[0].value = newTheta;
            HourAxisN[1].value = newGamma;
            IDSetNumber(&HourAxisNP, NULL);
        }
        else
        {
            HourAxisNP.s = IPS_ALERT;
            IDSetNumber(&HourAxisNP, "Theta or gamma missing or invalid");
        }

        return;
    }

    // =======================================
    // Equatorial Coord - SET
    // =======================================
    if (!strcmp (name, EquatorialCoordsNP.name))
    {
	int err ;
	int i=0, nset=0;
	double newRA, newDEC ;
    if (checkPower(&EquatorialCoordsNP))
	    return;

	for (nset = i = 0; i < n; i++)
	{
        INumber *np = IUFindNumber (&EquatorialCoordsNP, names[i]);
        if (np == &EquatorialCoordsNP.np[0])
	    {
		newRA = values[i];
		nset += newRA >= 0 && newRA <= 24.0;
        } else if (np == &EquatorialCoordsNP.np[1])
	    {
		newDEC = values[i];
		nset += newDEC >= -90.0 && newDEC <= 90.0;
	    }
	}

	if (nset == 2)
	{
	    int ret ;
#if defined HAVE_NOVA_H || defined HAVE_NOVASCC_H

	    double geo[6] ;
	    double eqt[2] ;
	    double eqn[2] ;
	    double hxt[2] ;
#endif 
	    /*EquatorialCoordsWNP.s = IPS_BUSY;*/
	    char RAStr[32], DecStr[32];

	    fs_sexa(RAStr, newRA, 2, 3600);
	    fs_sexa(DecStr, newDEC, 2, 3600);

#ifdef INDI_DEBUG
	    IDLog("We received JNOW RA %g - DEC %g\n", newRA, newDEC);
	    IDLog("We received JNOW RA %s - DEC %s\n", RAStr, DecStr);
#endif
#if defined HAVE_NOVA_H || defined HAVE_NOVASCC_H
	    // Transfor the coordinates
	    /* Get the current time. */
	    geo[0]= geoNP.np[0].value ;
	    geo[1]= geoNP.np[1].value ;
	    geo[2]= geoNP.np[2].value ;
	    geo[3]= AirN[0].value ;
	    geo[4]= AirN[1].value ;
	    geo[5]= AirN[2].value ;

	    eqn[0]= newRA ;
	    eqn[1]= newDEC ;
	    hxt[0]= HourAxisN[0].value ;
	    hxt[1]= HourAxisN[1].value ;

	    if((ret = LDAppToX(  getOnSwitch(&ApparentToObservedSP), eqn, ln_get_julian_from_sys(), geo, hxt, eqt)) != 0)
	    {
		IDMessage( myapdev, "ISNewNumber: transformation %d failed", getOnSwitch(&ApparentToObservedSP)) ;
		exit(1) ;
	    } ;
	    /*EquatorialCoordsWNP.s = IPS_BUSY;*/
	    targetRA= eqt[0];
	    targetDEC= eqt[1];
#else
	    targetRA= newRA;
            targetDEC= newDEC;
#endif
	    if ( (ret = setAPObjectRA(fd, targetRA)) < 0 || ( ret = setAPObjectDEC(fd, targetDEC)) < 0)
	    {
		DiffEquatorialCoordsNP.s= IPS_ALERT;
		IDSetNumber(&DiffEquatorialCoordsNP, NULL);
        handleError(&EquatorialCoordsNP, err, "Setting RA/DEC");
		return;
	    }
        EquatorialCoordsNP.s = IPS_OK;
        IDSetNumber(&EquatorialCoordsNP, NULL);

	    DiffEquatorialCoordsNP.np[0].value= targetRA - currentRA ;
	    DiffEquatorialCoordsNP.np[1].value= targetDEC - currentDEC;
	    DiffEquatorialCoordsNP.s= IPS_OK;
	    IDSetNumber(&DiffEquatorialCoordsNP, NULL);

	    // ToDo don't we need to stop the motion (:Q#)?
	    if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
	    {
		IUResetSwitch(&MovementNSSP);
		IUResetSwitch(&MovementWESP);
		MovementNSSP.s = MovementWESP.s = IPS_IDLE;
		IDSetSwitch(&MovementNSSP, NULL);
		IDSetSwitch(&MovementWESP, NULL);
	    }
	    handleEqCoordSet() ;
//ToDo : conversion to boolean	    {
// 		EquatorialCoordsWNP.s = IPS_ALERT;
// 		IDSetNumber(&EquatorialCoordsWNP, NULL);

// 	    }
        } // end nset
        else
        {
        EquatorialCoordsNP.s = IPS_ALERT;
        IDSetNumber(&EquatorialCoordsNP, "RA or Dec missing or invalid");
        }
	return ;
    }

    // =======================================
    // Horizontal Coords - SET
    // =======================================
    if ( !strcmp (name, HorizontalCoordsNP.name) )
    {
	int i=0, nset=0;
	double newAz, newAlt ;
	int ret ;
	char altStr[64], azStr[64];

    if (checkPower(&HorizontalCoordsNP))
	    return;

        for (nset = i = 0; i < n; i++)
	{
        np = IUFindNumber (&HorizontalCoordsNP, names[i]);
        if (np == &HorizontalCoordsN[0])
	    {
        newAz = values[i];
		nset += newAz >= 0. && newAz <= 360.0;
	    } 
        else if (np == &HorizontalCoordsN[1])
	    {
		newAlt = values[i];
		nset += newAlt >= -90. && newAlt <= 90.0;
	    }
	}
	if (nset == 2)
	{
	    if ( (ret = setAPObjectAZ(fd, newAz)) < 0 || (ret = setAPObjectAlt(fd, newAlt)) < 0)
	    {
        handleError(&HorizontalCoordsNP, ret, "Setting Alt/Az");
		return;
	    }
	    targetAZ= newAz;
	    targetALT= newAlt;
        HorizontalCoordsNP.s = IPS_OK;
        IDSetNumber(&HorizontalCoordsNP, NULL) ;

	    fs_sexa(azStr, targetAZ, 2, 3600);
	    fs_sexa(altStr, targetALT, 2, 3600);

	    //IDSetNumber (&HorizontalCoordsWNP, "Attempting to slew to Alt %s - Az %s", altStr, azStr);
	    handleAltAzSlew();
	}
	else
	{
        HorizontalCoordsNP.s = IPS_ALERT;
        IDSetNumber(&HorizontalCoordsNP, "Altitude or Azimuth missing or invalid");
	}

	return;
    }

    // =======================================
    // Geographical Location
    // =======================================
    if (!strcmp (name, geoNP.name))
    {
	// new geographic coords
	double newLong = 0, newLat = 0;
	int i, nset, err;
	char msg[128];

	if (checkPower(&geoNP))
	    return;

	for (nset = i = 0; i < n; i++)
	{
	    np = IUFindNumber (&geoNP, names[i]);
	    if (np == &geoNP.np[0])
	    {
		newLat = values[i];
		nset += newLat >= -90.0 && newLat <= 90.0;
	    } else if (np == &geoNP.np[1])
	    {
		newLong = values[i];
		nset += newLong >= 0.0 && newLong < 360.0;
	    }
	}

	if (nset == 2)
	{
	    char l[32], L[32];
	    geoNP.s = IPS_OK;
	    fs_sexa (l, newLat, 3, 3600);
	    fs_sexa (L, newLong, 4, 3600);
	    
	    if ( ( err = setAPSiteLongitude(fd, 360.0 - newLong) < 0) )
	    {
		handleError(&geoNP, err, "Setting site coordinates");
		return;
	    }
	    if ( ( err = setAPSiteLatitude(fd, newLat) < 0) )
	    {
		handleError(&geoNP, err, "Setting site coordinates");
		return;
	    }

	    geoNP.np[0].value = newLat;
	    geoNP.np[1].value = newLong;
	    snprintf (msg, sizeof(msg), "Site location updated to Lat %.32s - Long %.32s", l, L);
	} 
	else
	{
	    geoNP.s = IPS_IDLE;
	    strcpy(msg, "Lat or Long missing or invalid");
	}
	IDSetNumber (&geoNP, "%s", msg);
	return;
    }

    return LX200Generic::ISNewNumber (dev, name, values, names, n);
}

bool LX200AstroPhysics::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int err=0;

    // ignore if not ours //
    if (strcmp (thisDevice, dev))
	    return;

    // =======================================
    // Connect
    // =======================================
    if (!strcmp (name, ConnectSP.name))
    {
	IUUpdateSwitch(&ConnectSP, states, names, n) ;

	connectTelescope();
	return;
    }

    // ============================================================
    // Satisfy AP mount initialization, see AP key pad manual p. 76
    // ============================================================
    if (!strcmp (name, StartUpSP.name))
    {
	int ret ;
	int switch_nr ;
	static int mount_status= MOUNTNOTINITIALIZED ;

	IUUpdateSwitch(&StartUpSP, states, names, n) ;

       	if( mount_status == MOUNTNOTINITIALIZED)
	{
	    mount_status=MOUNTINITIALIZED;
	
	    if(( ret= setBasicDataPart0())< 0)
	    {
		// ToDo do something if needed
		StartUpSP.s = IPS_ALERT ;
		IDSetSwitch(&StartUpSP, "Mount initialization failed") ;
		return ;
	    }
	    if( StartUpSP.sp[0].s== ISS_ON) // do it only in case a power on (cold start) 
	    {
		if(( ret= setBasicDataPart1())< 0)
		{
		    // ToDo do something if needed
		    StartUpSP.s = IPS_ALERT ;
		    IDSetSwitch(&StartUpSP, "Mount initialization failed") ;
		    return ;
		}
	    }
	    // Make sure that the mount is setup according to the properties	    
	    switch_nr = getOnSwitch(&TrackModeSP);

	    if ( ( err = selectAPTrackingMode(fd, switch_nr) < 0) )
	    {
		handleError(&TrackModeSP, err, "StartUpSP: Setting tracking mode.");
		return;
	    }
	    TrackModeSP.s = IPS_OK;
	    IDSetSwitch(&TrackModeSP, NULL);
	    //ToDo set button swapping acording telescope east west
	    // ... some code here

	    switch_nr = getOnSwitch(&MoveToRateSP);
	  
	    if ( ( err = selectAPMoveToRate(fd, switch_nr) < 0) )
	    {
		handleError(&MoveToRateSP, err, "StartUpSP: Setting move to rate.");
		return;
	    }
	    MoveToRateSP.s = IPS_OK;

	    IDSetSwitch(&MoveToRateSP, NULL);
	    switch_nr = getOnSwitch(&SlewRateSP);
	  
	    if ( ( err = selectAPSlewRate(fd, switch_nr) < 0) )
	    {
		handleError(&SlewRateSP, err, "StartUpSP: Setting slew rate.");
		return;
	    }
	    SlewRateSP.s = IPS_OK;
	    IDSetSwitch(&SlewRateSP, NULL);

	    StartUpSP.s = IPS_OK ;
	    IDSetSwitch(&StartUpSP, "Mount initialized") ;
	    // Fetch the coordinates and set RNP and WNP
	    getLX200RA( fd, &currentRA); 
	    getLX200DEC(fd, &currentDEC);
 
	    // make a IDSet in order the dome controller is aware of the initial values
	    targetRA= currentRA ;
	    targetDEC= currentDEC ;
        EquatorialCoordsNP.s = IPS_BUSY; /* dome controller sets target only if this state is busy */
        IDSetNumber(&EquatorialCoordsNP,  "EquatorialCoordsWNP.s = IPS_BUSY after initialization");

	    // sleep for 100 mseconds
	    usleep(100000);

        EquatorialCoordsNP.s = IPS_OK;
        IDSetNumber(&EquatorialCoordsNP, NULL);

	    getLX200Az(fd, &currentAZ);
	    getLX200Alt(fd, &currentALT);

        HorizontalCoordsNP.s = IPS_OK ;
        IDSetNumber (&HorizontalCoordsNP, NULL);

	    VersionInfo.tp[0].text = new char[64];
	    getAPVersionNumber(fd, VersionInfo.tp[0].text);

	    VersionInfo.s = IPS_OK ;
	    IDSetText(&VersionInfo, NULL);

	    if(( getOnSwitch(&DomeControlSP))== DOMECONTROL)
	    {
	        //ToDo compare that with other driver, not the best way
		IDSnoopDevice( MasterAlarmODTP.tp[0].text, MasterAlarmODTP.tp[1].text);
		MasterAlarmODTP.s= IPS_OK ;
		IDSetText (&MasterAlarmODTP, "SNOOPing %s on device %s", MasterAlarmODTP.tp[1].text, MasterAlarmODTP.tp[0].text);

		IDSnoopDevice( ConnectionDCODTP.tp[0].text, ConnectionDCODTP.tp[1].text);
		ConnectionDCODTP.s= IPS_OK ;
		IDSetText (&ConnectionDCODTP, "SNOOPing %s on device %s", ConnectionDCODTP.tp[1].text, ConnectionDCODTP.tp[0].text);
		IDSnoopDevice( ConnectionDCODTP.tp[0].text, ConnectionDCODTP.tp[1].text);

		IDSnoopDevice( ModeDCODTP.tp[0].text, ModeDCODTP.tp[1].text);
		ModeDCODTP.s= IPS_OK ;
		IDSetText (&ModeDCODTP, "SNOOPing %s on device %s", ModeDCODTP.tp[1].text, ModeDCODTP.tp[0].text);

	        // once chosen no unsnooping is possible
		DomeControlSP.s= IPS_OK ;
		IDSetSwitch(&DomeControlSP, NULL) ;
	    }
	    else
	    {
		IDMessage( myapdev, "Not operating in dome control mode") ;
	    }
	     #ifndef HAVE_NOVA_H
	    IDMessage( myapdev, "Libnova not compiled in, consider to install it (http://libnova.sourceforge.net/)") ;
	    #endif
	    #ifndef HAVE_NOVASCC_H
	    IDMessage( myapdev, "NOVASCC not compiled in, consider to install it (http://aa.usno.navy.mil/AA/)") ;
	    #endif
	}
	else
	{
	    StartUpSP.s = IPS_ALERT ;
	    IDSetSwitch(&StartUpSP, "Mount is already initialized") ;
	}
	return;
    }

    // =======================================
    // Park
    // =======================================
    if (!strcmp(name, ParkSP.name))
    {
	if (checkPower(&ParkSP))
	    return;
	
    if (EquatorialCoordsNP.s == IPS_BUSY)
	{
	    // ToDo handle return value
	    abortSlew(fd);
	    // sleep for 200 mseconds
	    usleep(200000);
	    AbortSlewSP.s = IPS_OK;
            IDSetSwitch(&AbortSlewSP, NULL);
 
	}

	if (setAPPark(fd) < 0)
	{
	    ParkSP.s = IPS_ALERT;
	    IDSetSwitch(&ParkSP, "Parking Failed.");
	    return;
	}

	ParkSP.s = IPS_OK;
	IDSetSwitch(&ParkSP, "The telescope is parked. Turn off the telescope. Disconnecting...");
        // avoid sending anything to the mount controller
	tty_disconnect( fd);
	ConnectSP.s   = IPS_IDLE;
	ConnectSP.sp[0].s = ISS_OFF;
	ConnectSP.sp[1].s = ISS_ON;
        
	IDSetSwitch(&ConnectSP, NULL);

	StartUpSP.s = IPS_IDLE ;
	IDSetSwitch(&StartUpSP, NULL) ;

	// ToDo reset all values

	return;
    }

    // =======================================
    // Tracking Mode
    // =======================================
    if (!strcmp (name, TrackModeSP.name))
    {
	if (checkPower(&TrackModeSP))
	    return;

	IUResetSwitch(&TrackModeSP);
	IUUpdateSwitch(&TrackModeSP, states, names, n);
	trackingMode = getOnSwitch(&TrackModeSP);
	  
	if ( ( err = selectAPTrackingMode(fd, trackingMode) < 0) )
	{
	    handleError(&TrackModeSP, err, "Setting tracking mode.");
	    return;
	}     
	TrackModeSP.s = IPS_OK;
	IDSetSwitch(&TrackModeSP, NULL);
	if( trackingMode != 3) /* not zero */
	{
	    AbortSlewSP.s = IPS_IDLE;
	    IDSetSwitch(&AbortSlewSP, NULL);
	}
	return;
    }

    // =======================================
    // Swap Buttons
    // =======================================
    if (!strcmp(name, SwapSP.name))
    {
	int currentSwap ;

	if (checkPower(&SwapSP))
	    return;
	
	IUResetSwitch(&SwapSP);
	IUUpdateSwitch(&SwapSP, states, names, n);
	currentSwap = getOnSwitch(&SwapSP);

	if(( err = swapAPButtons(fd, currentSwap) < 0) )
	{
	    handleError(&SwapSP, err, "Swapping buttons.");
	    return;
	}

	SwapS[0].s= ISS_OFF ;
	SwapS[1].s= ISS_OFF ;
	SwapSP.s = IPS_OK;
	IDSetSwitch(&SwapSP, NULL);
	return ;
    }

    // =======================================
    // Swap to rate
    // =======================================
    if (!strcmp (name, MoveToRateSP.name))
    {
	int moveToRate ;

	if (checkPower(&MoveToRateSP))
	    return;

	IUResetSwitch(&MoveToRateSP);
	IUUpdateSwitch(&MoveToRateSP, states, names, n);
	moveToRate = getOnSwitch(&MoveToRateSP);
	  
	if ( ( err = selectAPMoveToRate(fd, moveToRate) < 0) )
	{
	    handleError(&MoveToRateSP, err, "Setting move to rate.");
	    return;
	}
	MoveToRateSP.s = IPS_OK;
	IDSetSwitch(&MoveToRateSP, NULL);
	return;
    }

    // =======================================
    // Slew Rate
    // =======================================
    if (!strcmp (name, SlewRateSP.name))
    {
	int slewRate ;
	
	if (checkPower(&SlewRateSP))
	    return;

	IUResetSwitch(&SlewRateSP);
	IUUpdateSwitch(&SlewRateSP, states, names, n);
	slewRate = getOnSwitch(&SlewRateSP);
	  
	if ( ( err = selectAPSlewRate(fd, slewRate) < 0) )
	{
	    handleError(&SlewRateSP, err, "Setting slew rate.");
	    return;
	}
	  
          
	SlewRateSP.s = IPS_OK;
	IDSetSwitch(&SlewRateSP, NULL);
	return;
    }

    // =======================================
    // Choose the appropriate sync command
    // =======================================
    if (!strcmp(name, SyncCMRSP.name))
    {
	int currentSync ;
	if (checkPower(&SyncCMRSP))
	    return;

	IUResetSwitch(&SyncCMRSP);
	IUUpdateSwitch(&SyncCMRSP, states, names, n);
	currentSync = getOnSwitch(&SyncCMRSP);
	SyncCMRSP.s = IPS_OK;
	IDSetSwitch(&SyncCMRSP, NULL);
	return ;
    }

    #if defined HAVE_NOVA_H || defined HAVE_NOVASCC_H
    // =======================================
    // Set various transformations
    // =======================================
    if (!strcmp (name, ApparentToObservedSP.name))
    {
	int trans_to ;
	if (checkPower(&ApparentToObservedSP))
	    return;

	IUResetSwitch(&ApparentToObservedSP);
	IUUpdateSwitch(&ApparentToObservedSP, states, names, n);
	trans_to = getOnSwitch(&ApparentToObservedSP);

	ApparentToObservedSP.s = IPS_OK;
	IDSetSwitch(&ApparentToObservedSP, "Transformation %d", trans_to);
	return;
    }
    #endif
    if (!strcmp (name, DomeControlSP.name))
    {
	if((DomeControlSP.s== IPS_OK) &&( DomeControlSP.sp[0].s== ISS_ON))
	{
    #ifdef INDI_DEBUG
	    IDLog("Once in dome control mode no return is possible (INDI has no \"unsnoop\")\n") ;
    #endif
	    DomeControlSP.s= IPS_ALERT ;
            IDSetSwitch(&DomeControlSP, "Once in dome control mode no return is possible (INDI has no \"unsnoop\")") ;
	}
	else
	{
	    int last_state= getOnSwitch(&DomeControlSP) ;
	    IUResetSwitch(&DomeControlSP);
	    IUUpdateSwitch(&DomeControlSP, states, names, n) ;
	    DomeControlSP.s= IPS_OK ;
	    IDSetSwitch(&DomeControlSP, NULL) ;

	    if(( DomeControlSP.s== IPS_OK) &&( last_state== NOTDOMECONTROL)) /* dome control mode after mount init. */
	    {
		//ToDo compare that with other driver, not the best way
                IDSnoopDevice( MasterAlarmODTP.tp[0].text, MasterAlarmODTP.tp[1].text);
                MasterAlarmODTP.s= IPS_OK ;
                IDSetText (&MasterAlarmODTP, "SNOOPing %s on device %s", MasterAlarmODTP.tp[1].text, MasterAlarmODTP.tp[0].text);

                IDSnoopDevice( ConnectionDCODTP.tp[0].text, ConnectionDCODTP.tp[1].text);
                ConnectionDCODTP.s= IPS_OK ;
                IDSetText (&ConnectionDCODTP, "SNOOPing %s on device %s", ConnectionDCODTP.tp[1].text, ConnectionDCODTP.tp[0].text);
                IDSnoopDevice( ConnectionDCODTP.tp[0].text, ConnectionDCODTP.tp[1].text);

                IDSnoopDevice( ModeDCODTP.tp[0].text, ModeDCODTP.tp[1].text);
                ModeDCODTP.s= IPS_OK ;
                IDSetText (&ModeDCODTP, "SNOOPing %s on device %s", ModeDCODTP.tp[1].text, ModeDCODTP.tp[0].text);
	    }
	}
	return;
    }

    return LX200Generic::ISNewSwitch (dev, name, states, names,  n);
 }
bool LX200AstroPhysics::ISSnoopDevice (XMLEle *root)
{
    int err ;
    if (IUSnoopSwitch(root, &MasterAlarmSP) == 0)
    {
        //IDMessage( myapdev, "ISCollisionStatusS received new values %s: %d, %s: %d, %s: %d.", names[0], states[0],names[1], states[1],names[2], states[2]);
	if( MasterAlarmS[1].s == ISS_ON) /* Approaching a critical situation */
	{
/* Stop if possible any motion */
	    // ToDo
	    abortSlew(fd);
	    AbortSlewSP.s = IPS_OK;
            IDSetSwitch(&AbortSlewSP, NULL);

	    if ( ( err = selectAPTrackingMode(fd, 3) < 0) ) /* Tracking Mode 3 = zero */
	    {
		IDMessage( myapdev, "FAILED: Setting tracking mode ZERO.");
		return;
	    }
	    IUResetSwitch(&TrackModeSP);
	    TrackModeSP.sp[0].s= ISS_OFF ; /* lunar */
	    TrackModeSP.sp[1].s= ISS_OFF ; /* solar */
	    TrackModeSP.sp[2].s= ISS_OFF ; /* sidereal */
	    TrackModeSP.sp[3].s= ISS_ON ;  /* zero */
	    
	    TrackModeSP.s = IPS_ALERT;
	    IDSetSwitch(&TrackModeSP, "Device %s MasterAlarm OFF: approaching a critical situation, avoided by apmount, stopped motors, no tracking!", MasterAlarmODT[0].text);   
	}
	else if( MasterAlarmS[2].s == ISS_ON) /* a critical situation */
	{

/* If Master Alarm is on it is "too" late. So we do the same as under MasterAlarmS[1].s == ISS_ON*/
/* The device setting up the Master Alarm should switch power off*/
	    // ToDo
	    abortSlew(fd);
	    AbortSlewSP.s = IPS_OK;
            IDSetSwitch(&AbortSlewSP, NULL);


	    if ( ( err = selectAPTrackingMode(fd, 3) < 0) ) /* Tracking Mode 3 = zero */
	    {
		IDMessage( myapdev, "FAILED: Setting tracking mode ZERO.");
		return;
	    }
	    IUResetSwitch(&TrackModeSP);
	    TrackModeSP.sp[0].s= ISS_OFF ; /* lunar */
	    TrackModeSP.sp[1].s= ISS_OFF ; /* solar */
	    TrackModeSP.sp[2].s= ISS_OFF ; /* sidereal */
	    TrackModeSP.sp[3].s= ISS_ON  ;  /* zero */

	    TrackModeSP.s = IPS_ALERT;
	    IDSetSwitch(&TrackModeSP, "Device %s MasterAlarm ON: critical situation avoided, stopped motors, no tracking!", MasterAlarmODT[0].text);
	}
	else if((MasterAlarmS[2].s == ISS_OFF) && (MasterAlarmS[1].s == ISS_OFF) && (MasterAlarmS[0].s == ISS_OFF))
	{
	    //ToDo make the mastar alarm indicator more visible!!
	    TrackModeSP.s = IPS_OK;
	    IDSetSwitch(&TrackModeSP, "MasterAlarm Status ok");
	    // values obtained via ISPoll
        IDSetNumber(&EquatorialCoordsNP, "Setting (sending) EquatorialCoordsNP on reset MasterAlarm");
	}
	else
	{
	    if ( ( err = selectAPTrackingMode(fd, 3) < 0) ) /* Tracking Mode 3 = zero */
	    {
		IDMessage( myapdev, "FAILED: Setting tracking mode ZERO.");
		return;
	    }
	    IUResetSwitch(&TrackModeSP);
	    TrackModeSP.sp[0].s= ISS_OFF ; /* lunar */
	    TrackModeSP.sp[1].s= ISS_OFF ; /* solar */
	    TrackModeSP.sp[2].s= ISS_OFF ; /* sidereal */
	    TrackModeSP.sp[3].s= ISS_ON  ;  /* zero */

	    TrackModeSP.s = IPS_ALERT;
	    TrackModeSP.s = IPS_ALERT;
	    IDSetSwitch(&TrackModeSP, "Device %s MASTER ALARM Unknown Status", MasterAlarmODT[0].text);
	}
    }
    else if (IUSnoopSwitch(root, &ConnectionDCSP) == 0)
    {
	if( ConnectionDCS[0].s != ISS_ON)
	{
	    // ToDo
	    abortSlew(fd);
	    AbortSlewSP.s = IPS_OK;
            IDSetSwitch(&AbortSlewSP, NULL);


	    if ( ( err = selectAPTrackingMode(fd, 3) < 0) ) /* Tracking Mode 3 = zero */
	    {
		IDMessage( myapdev, "FAILED: Setting tracking mode ZERO.");
		return;
	    }
	    IUResetSwitch(&TrackModeSP);
	    TrackModeSP.sp[0].s= ISS_OFF ; /* lunar */
	    TrackModeSP.sp[1].s= ISS_OFF ; /* solar */
	    TrackModeSP.sp[2].s= ISS_OFF ; /* sidereal */
	    TrackModeSP.sp[3].s= ISS_ON ;  /* zero */

	    TrackModeSP.s = IPS_ALERT;
	    IDSetSwitch(&TrackModeSP, "Driver %s disconnected: critical situation avoided, stopped motors, no tracking!", MasterAlarmODT[0].text);
	}
    }
    else if (IUSnoopSwitch(root, &ModeSP) == 0)
    {
	if( ModeS[1].s == ISS_ON) /* late dome control mode */
	{
	    getLX200RA( fd, &currentRA); 
	    getLX200DEC(fd, &currentDEC);
 
	    targetRA= currentRA ;
	    targetDEC= currentDEC ;
        EquatorialCoordsNP.s = IPS_BUSY; /* dome controller sets target only if this state is busy */
        IDSetNumber(&EquatorialCoordsNP, "Setting (sending) EquatorialCoordsNP on ModeS[1].s != ISS_ON");
	    // sleep for 100 mseconds
	    usleep(100000);
        EquatorialCoordsNP.s = IPS_OK;
        IDSetNumber(&EquatorialCoordsNP, NULL) ;
	}
    }
}

bool LX200AstroPhysics::isMountInit(void)
{
    return (StartUpSP.s != IPS_IDLE);
}

void LX200AstroPhysics::ISPoll()
{
     int ret ;
//     int ddd, mm ;
     if (!isMountInit())
	 return;
//     #ifdef INDI_DEBUG
//     getSiteLongitude(fd, &ddd, &mm) ;
//     IDLog("longitude %d:%d\n", ddd, mm);
//     getSiteLatitude(fd, &ddd, &mm) ;
//     IDLog("latitude %d:%d\n", ddd, mm);
//     getAPUTCOffset( fd, &APUTCOffsetN[0].value) ;
//     IDLog("UTC offset %10.6f\n", APUTCOffsetN[0].value);
//     #endif


     //============================================================
     // #1 Call LX200Generic ISPoll
     //============================================================
     LX200Generic::ISPoll();


     //============================================================
     // #2 Get Local Time
     //============================================================
     if(( ret= getLocalTime24( fd, &APLocalTimeN[0].value)) == 0)
     {
 	 APLocalTimeNP.s = IPS_OK ;
     }
     else
     {
 	 APLocalTimeNP.s = IPS_ALERT ;
     }
     IDSetNumber(&APLocalTimeNP, NULL);

//     #ifdef INDI_DEBUG
//     IDLog("localtime %f\n", APLocalTimeN[0].value) ;
//     #endif

     //============================================================
     // #3 Get Sidereal Time
     //============================================================
     if(( ret= getSDTime( fd, &APSiderealTimeN[0].value)) == 0)
     {
 	 APSiderealTimeNP.s = IPS_OK ;
     }
     else
     {
 	 APSiderealTimeNP.s = IPS_ALERT ;
     }
     IDSetNumber(&APSiderealTimeNP, NULL);
//     #ifdef INDI_DEBUG
//     IDLog("siderealtime %f\n", APSiderealTimeN[0].value) ;
//     #endif

     //============================================================
     // #4 Get UTC Offset
     //============================================================
     if(( ret= getAPUTCOffset( fd, &APUTCOffsetN[0].value)) == 0)
     {
 	 APUTCOffsetNP.s = IPS_OK ;
     }
     else
     {
 	 APUTCOffsetNP.s = IPS_ALERT ;
     }
     IDSetNumber(&APUTCOffsetNP, NULL);

     //============================================================
     // #5 
     //============================================================
     if(( ret= getAPDeclinationAxis( fd, DeclinationAxisT[0].text)) == 0)
     {
	 DeclinationAxisTP.s = IPS_OK ;
     }
     else

     {
	 DeclinationAxisTP.s = IPS_ALERT ;
     }
     IDSetText(&DeclinationAxisTP, NULL) ;


     /* LX200Generic should take care of this */
     //getLX200RA(fd, &currentRA );
     //getLX200DEC(fd, &currentDEC );
     //EquatorialCoordsNP.s = IPS_OK ;
     //IDSetNumber (&EquatorialCoordsNP, NULL);
     
   
/* Calculate the hour angle */

     HourangleCoordsNP.np[0].value= 180. /M_PI/15. * LDRAtoHA( 15.* currentRA /180. *M_PI, -geoNP.np[1].value/180. *M_PI);
     HourangleCoordsNP.np[1].value= currentDEC;

     HourangleCoordsNP.s = IPS_OK;
     IDSetNumber(&HourangleCoordsNP, NULL);

     getLX200Az(fd, &currentAZ);
     getLX200Alt(fd, &currentALT);

     /* The state of RNP is coupled to the WNP 
     HorizontalCoordsNP.s = IPS_OK ;
     IDSetNumber (&HorizontalCoordsNP, NULL);
     */

     // LX200generic has no  HorizontalCoords(R|W)NP
     if( HorizontalCoordsNP.s == IPS_BUSY) /* telescope is syncing or slewing */
     {
	 
	 double dx = fabs ( targetAZ  - currentAZ);
	 double dy = fabs ( targetALT  - currentALT);

	 if (dx <= (SlewAccuracyNP.np[0].value/(60.0*15.0)) && (dy <= SlewAccuracyNP.np[1].value/60.0)) 
	 {
             #ifdef INDI_DEBUG
	     IDLog("Slew completed.\n");
             #endif
         HorizontalCoordsNP.s = IPS_OK ;
         IDSetNumber(&HorizontalCoordsNP, "Slew completed") ;
	 }
	 else
	 {
             #ifdef INDI_DEBUG
	     IDLog("Slew in progress.\n");
             #endif
	 }
     }
     if(StartUpSP.s== IPS_OK) /* the dome controller needs to be informed even in case dc has been started long after lx200ap*/
     {
	 StartUpSP.s = IPS_OK ;
	 IDSetSwitch(&StartUpSP, NULL) ;
     }
     else
     {
	 StartUpSP.s = IPS_ALERT ;
	 IDSetSwitch(&StartUpSP, NULL) ;
     }

}
int LX200AstroPhysics::setBasicDataPart0()
{
    int err ;
#ifdef HAVE_NOVA_H
    struct ln_date utm;
    struct ln_zonedate ltm;
#endif
    if(setAPClearBuffer( fd) < 0)
    {
	handleError(&ConnectSP, err, "Clearing the buffer");
	return -1;
    }
    if(setAPLongFormat( fd) < 0)
    {
	IDMessage( myapdev, "Setting long format failed") ;
	return -1;
    }
    
    // ToDo make a property
    if(setAPBackLashCompensation(fd, 0,0,0) < 0)
    {
	handleError(&ConnectSP, err, "Setting back lash compensation");
	return -1;
    }
#if defined HAVE_NOVA_H
    ln_get_date_from_sys( &utm) ;
    ln_date_to_zonedate(&utm, &ltm, 3600);

    if((  err = setLocalTime(fd, ltm.hours, ltm.minutes, (int) ltm.seconds) < 0))
    {
	handleError(&ConnectSP, err, "Setting local time");
	return -1;
    }
    if ( ( err = setCalenderDate(fd, ltm.days, ltm.months, ltm.years) < 0) )
    {
	handleError(&ConnectSP, err, "Setting local date");
	return -1;
    }
    #ifdef INDI_DEBUG
    IDLog("UT time is: %04d/%02d/%02d T %02d:%02d:%02d\n", utm.years, utm.months, utm.days, utm.hours, utm.minutes, (int)utm.seconds);
    IDLog("Local time is: %04d/%02d/%02d T %02d:%02d:%02d\n", ltm.years, ltm.months, ltm.days, ltm.hours, ltm.minutes, (int)ltm.seconds);
    #endif

    // ToDo: strange but true offset 22:56:07, -1:03:53 (valid for obs Vermes)
    // Understand what happens with AP controller sidereal time, azimut coordinates
    if((err = setAPUTCOffset( fd, -1.0647222)) < 0)
    {
        handleError(&ConnectSP, err,"Setting AP UTC offset") ;
        return -1;
    }
#else
    IDMessage( myapdev, "Initialize %s manually or install libnova", myapdev) ;
#endif

    return 0 ;
}
int LX200AstroPhysics::setBasicDataPart1()
{
    int err ;

    if((err = setAPUnPark( fd)) < 0)
    {
	handleError(&ConnectSP, err,"Unparking failed") ;
	return -1;
    }
#ifdef INDI_DEBUG
    IDLog("Unparking successful\n");
#endif

    if((err = setAPMotionStop( fd)) < 0)
    {
	handleError(&ConnectSP, err, "Stop motion (:Q#) failed, check the mount") ;
	return -1;
    }
#ifdef INDI_DEBUG
    IDLog("Stopped any motion (:Q#)\n");
#endif

    return 0 ;
}
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
    return;
}
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
        if (EquatorialCoordsNP.s == IPS_BUSY)
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
        EquatorialCoordsNP.s = IPS_BUSY;
	    fs_sexa(RAStr,  targetRA, 2, 3600);
	    fs_sexa(DecStr, targetDEC, 2, 3600);
        IDSetNumber(&EquatorialCoordsNP, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
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
            EquatorialCoordsNP.s = IPS_ALERT ;
            IDSetNumber( &EquatorialCoordsNP , "Synchronization failed.");
			// ToDo handle with handleError function
			return ;
		    }
		}
		else if((sync=getOnSwitch(&SyncCMRSP))==SYNCCMR)
		{
		    if ( ( err = APSyncCMR(fd, syncString) < 0) )
		    {
            EquatorialCoordsNP.s = IPS_ALERT ;
            IDSetNumber( &EquatorialCoordsNP, "Synchronization failed.");
			// ToDo handle with handleError function
			return ;
		    }
		}
		else
		{
            EquatorialCoordsNP.s = IPS_ALERT ;
            IDSetNumber( &EquatorialCoordsNP , "SYNC NOK no valid SYNCCM, SYNCCMR");
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
                EquatorialCoordsNP.s = IPS_BUSY; /* dome controller sets target only if this state is busy */
                IDSetNumber(&EquatorialCoordsNP,  "EquatorialCoordsWNP.s = IPS_BUSY after SYNC");
	    }
	    else
	    {
#ifdef INDI_DEBUG
		IDLog("Synchronization not allowed\n") ;
#endif

        EquatorialCoordsNP.s = IPS_ALERT;
        IDSetNumber(&EquatorialCoordsNP,  "Synchronization not allowed" );

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
        EquatorialCoordsNP.s = IPS_OK; /* see above for dome controller dc */
        IDSetNumber(&EquatorialCoordsNP, "Synchronization successful, EquatorialCoordsWNP.s = IPS_OK after SYNC");
	    break;
    }
   return ;
}

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
// Transformation from apparent to various coordinate systems 
int LDAppToX( int trans_to, double *star_cat, double tjd, double *loc, double *hxt, double *star_trans)
{
#if defined HAVE_NOVASCC_H
    short int error = 0;

/* 'deltat' is the difference in time scales, TT - UT1. */

    double deltat = 60.0;
    double gst ;

/* Set x,y in case where sub arcsec precission is required */

    double x=0. ;
    double y=0. ;
    short int ref_option ;


    double ra_ar, dec_ar;            /* apparent and refracted EQ coordinate system */
    double az_ar, zd_ar ;    /* apparent and refracted, AltAz coordinate system */
    double ra_art, dec_art ; /* apparent, refracted and telescope EQ coordinate system */

    double ha_ar ;
    double ha_art ;

    cat_entry star   = {"FK5", "NONAME", 0, star_cat[0], star_cat[1], 0., 0., 0., 0.};

    site_info geo_loc= {loc[0], loc[1], loc[2], loc[3], loc[4]} ;

    /* A structure containing the body designation for Earth.*/

    body earth ;

    /* Set up the structure containing the body designation for Earth. */

    if ((error = set_body (0,3,"Earth", &earth)))
    {
	IDMessage( myapdev, "LDAppToX: Error %d from set_body.\n", error);
        return -1;
    }
    switch (trans_to)
    {
        case ATA:  /* identity */

            star_trans[0]= star.ra ;
            star_trans[1]= star.dec ;
            break ;

        case ATR: /* T4, apparent to refracted */

/* Alt Azimut refraction */

            ref_option= 2 ;

/* Set x,y in case where sub arcsec precission is required */

            equ2hor(tjd, deltat, x, y, &geo_loc, star.ra, star.dec, ref_option, &zd_ar, &az_ar, &ra_ar, &dec_ar) ;

            if(ra_ar <0.)
            {
                ra_ar += 24. ;
            }

            star_trans[0]= ra_ar ;
            star_trans[1]= dec_ar ;

            break ;

        case ARTT: /* T4, apparent, refracted to telescope */

/* Alt Azimut refraction */

            ref_option= 2 ;

/* Set x,y in case where sub arcsec precission is required */

            equ2hor(tjd, deltat, x, y, &geo_loc, star.ra, star.dec, ref_option, &zd_ar, &az_ar, &ra_ar, &dec_ar) ;

/* Calculate the apparent refracted hour angle  */

            ha_ar= (gst + geo_loc.longitude/180. * 12. - ra_ar) ;

            if( ha_ar < 0.)
            {
                ha_ar += 24. ;
            }
/* Equatorial system of the telescope, these are ra_art,  dec_art needed for the setting */
/* To be defined: sign of rotation   theta= -7.5 ; */
/* The values of hxt are defined in the local hour system, ha [hour]*/

            if(( error= LDEqToEqT( ha_ar, dec_ar, hxt, &ha_art, &dec_art)) != 0)
            {
                IDMessage( myapdev, "LDAppToX: \nError in calculation\n\n");
                return -1 ;
            }
            ra_art = -ha_art + gst  + geo_loc.longitude/180. * 12. ;

            if(ra_art <0.)
            {
                ra_art += 24. ;
            }

            star_trans[0]= ra_art ;
            star_trans[1]= dec_art ;

            break ;
        case ARTTO: /* T5, apparent, refracted, telescope to observed*/
            IDMessage( myapdev, "LDAppToX: Not yet implemented, exiting...\n") ;
            return -1;
            break ;
        default:
            IDMessage(myapdev, "LDAppToX: No default, exiting\n") ;
            return -1;
    }
    return 0;
#elif defined HAVE_NOVA_H
    IDMessage( myapdev, "Only identity transformation is supported without HAVE_NOVASCC_H") ;
    star_trans[0]= star_cat[0];
    star_trans[1]= star_cat[1];
    return 0 ;
#else
    IDMessage( myapdev, "Install libnova to use this feature") ; // we never get here
    return 0 ;
#endif
}

// Trans form to the ideal telescope coordinate system (no mount defects)
int LDEqToEqT( double ra_h, double dec_d, double *hxt, double *rat_h, double *dect_d)
{
    int res ;
    int i,j;
    double ra = ra_h / 12. * M_PI ; /* novas-c unit is hour */
    double dec= dec_d/180. * M_PI ;

    double theta= hxt[0]/180. * M_PI ;
    double gamma= hxt[1]/180. * M_PI ;

    double unit_vector_in[3]= {cos(dec)*cos(ra), cos(dec)*sin(ra), sin(dec)} ;
    double unit_vector_rot[3]= {0.,0.,0.} ;
    double unit_vector_tmp[3]= {0.,0.,0.} ;
    double rat  ;
    double dect  ;

    /* theta rotation around polar axis, gamma around y axis */
    double rotation[3][3]=
        {
            {cos(gamma)*cos(theta),-(cos(gamma)*sin(theta)),-sin(gamma)},
            {sin(theta),cos(theta),0},
            {cos(theta)*sin(gamma),-(sin(gamma)*sin(theta)), cos(gamma)}
        } ;


    /* minus theta rotation around telescope polar axis */

    /* Despite the above matrix is correct, no body has a telescope */
    /* with fixed setting circles in RA - or a telescope is usually */
    /* calibrated in RA/HA by choosing one star. The matrix below */
    /* takes that into account */

    double rotation_minus_theta[3][3]=
            {
                { cos(theta), sin(theta), 0.},
                {-sin(theta), cos(theta), 0.},
                {         0.,         0., 1.}
            } ;

    unit_vector_in[0]= cos(dec)*cos(ra) ;
    unit_vector_in[1]= cos(dec)*sin(ra) ;
    unit_vector_in[2]= sin(dec) ;

    if( gamma < 0 )
    {
        IDMessage(myapdev, "LDEqToEqT: gamma is the distance from the celestial pole and always positive\n") ;
        return -1 ;
    }
    for( i=0; i < 3 ; i++)
    {
        for( j=0; j < 3 ; j++)
        {
            unit_vector_tmp[i] += rotation[i][j] *  unit_vector_in[j] ;
        }
    }

    for( i=0; i < 3 ; i++)
    {
        for( j=0; j < 3 ; j++)
        {
            unit_vector_rot[i] += rotation_minus_theta[i][j] *  unit_vector_tmp[j] ;
        }
    }

    if(( res= LDCartToSph( unit_vector_rot, &rat, &dect))!= 0)
    {
        return -1 ;
    }
    else
    {
        *rat_h = rat /M_PI * 12. ;
        *dect_d= dect/M_PI * 180. ;
        return 0 ;
    }
}
int LDCartToSph( double *vec, double *ra, double *dec)
{

    if( vec[0] !=0.)
    {
        *ra = atan2( vec[1], vec[0]) ;
    }
    else
    {
        return -1 ;
    }
    *dec= asin( vec[2]) ;
    return 0 ;
}
