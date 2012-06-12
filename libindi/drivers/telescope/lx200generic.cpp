#if 0
    LX200 Generic
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
#include <sys/time.h>

#include "indicom.h"
#include "lx200driver.h"
#include "lx200gps.h"
#include "lx200ap.h"
#include "lx200classic.h"
#include "lx200fs2.h"

#include <config.h>

#ifdef HAVE_NOVA_H
#include <libnova.h>
#endif

LX200Generic *telescope = NULL;
int MaxReticleFlashRate = 3;

/* There is _one_ binary for all LX200 drivers, but each binary is renamed
** to its device name (i.e. lx200gps, lx200_16..etc). The main function will
** fetch from std args the binary name and ISInit will create the apporpiate
** device afterwards. If the binary name does not match any known devices,
** we simply create a generic device.
*/
extern char* me;

#define COMM_GROUP	"Communication"
#define BASIC_GROUP	"Main Control"
#define MOTION_GROUP	"Motion Control"
#define DATETIME_GROUP	"Date/Time"
#define SITE_GROUP	"Site Management"
#define FOCUS_GROUP	"Focus Control"

#define LX200_TRACK	0
#define LX200_SYNC	1

/* Simulation Parameters */
#define	SLEWRATE	1		/* slew rate, degrees/s */
#define SIDRATE		0.004178	/* sidereal rate, degrees/s */

/* Handy Macros */
#define currentRA	EquatorialCoordsRN[0].value
#define currentDEC	EquatorialCoordsRN[1].value
#define targetRA	EquatorialCoordsWN[0].value
#define targetDEC	EquatorialCoordsWN[1].value

static void ISPoll(void *);
static void retryConnection(void *);

/*INDI Propertries */

/**********************************************************************************************/
/************************************ GROUP: Communication ************************************/
/**********************************************************************************************/

/********************************************
 Property: Connection
*********************************************/
static ISwitch ConnectS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
ISwitchVectorProperty ConnectSP		= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ConnectS, NARRAY(ConnectS), "", 0};

/********************************************
 Property: Device Port
*********************************************/
/*wildi removed static */
static IText PortT[]			= {{"PORT", "Port", 0, 0, 0, 0}};
ITextVectorProperty PortTP	= { mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT), "", 0};

/********************************************
 Property: Telescope Alignment Mode
*********************************************/
static ISwitch AlignmentS []		= {{"Polar", "", ISS_ON, 0, 0}, {"AltAz", "", ISS_OFF, 0, 0}, {"Land", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty AlignmentSw= { mydev, "Alignment", "", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AlignmentS, NARRAY(AlignmentS), "", 0};

/**********************************************************************************************/
/************************************ GROUP: Main Control *************************************/
/**********************************************************************************************/

/********************************************
 Property: Equatorial Coordinates JNow
 Perm: Transient WO.
 Timeout: 120 seconds.
*********************************************/
INumber EquatorialCoordsWN[] 		= { {"RA",  "RA  H:M:S", "%10.6m",  0., 24., 0., 0., 0, 0, 0}, {"DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0., 0, 0, 0} };
INumberVectorProperty EquatorialCoordsWNP= { mydev, "EQUATORIAL_EOD_COORD_REQUEST", "Equatorial JNow", BASIC_GROUP, IP_WO, 120, IPS_IDLE, EquatorialCoordsWN, NARRAY(EquatorialCoordsWN), "", 0};

/********************************************
 Property: Equatorial Coordinates JNow
 Perm: RO
*********************************************/
INumber EquatorialCoordsRN[]	 	= { {"RA",  "RA  H:M:S", "%10.6m",  0., 24., 0., 0., 0, 0, 0}, {"DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0., 0, 0, 0}};
INumberVectorProperty EquatorialCoordsRNP= { mydev, "EQUATORIAL_EOD_COORD", "Equatorial JNow", BASIC_GROUP, IP_RO, 120, IPS_IDLE, EquatorialCoordsRN, NARRAY(EquatorialCoordsRN), "", 0};

/********************************************
 Property: On Coord Set
 Description: This property decides what happens
             when we receive a new equatorial coord
             value. We either track, or sync
	     to the new coordinates.
*********************************************/
static ISwitch OnCoordSetS[]		 = {{"SLEW", "Slew", ISS_ON, 0, 0 }, {"SYNC", "Sync", ISS_OFF, 0 , 0}};
ISwitchVectorProperty OnCoordSetSP= { mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, OnCoordSetS, NARRAY(OnCoordSetS), "", 0};

/********************************************
 Property: Abort telescope motion
*********************************************/
static ISwitch AbortSlewS[]		= {{"ABORT", "Abort", ISS_OFF, 0, 0 }};
ISwitchVectorProperty AbortSlewSP= { mydev, "TELESCOPE_ABORT_MOTION", "Abort Slew", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AbortSlewS, NARRAY(AbortSlewS), "", 0};

/**********************************************************************************************/
/************************************** GROUP: Motion *****************************************/
/**********************************************************************************************/

/********************************************
 Property: Slew Speed
*********************************************/
static ISwitch SlewModeS[]		= {{"Max", "", ISS_ON, 0, 0}, {"Find", "", ISS_OFF, 0, 0}, {"Centering", "", ISS_OFF, 0, 0}, {"Guide", "", ISS_OFF, 0 , 0}};
ISwitchVectorProperty SlewModeSP	= { mydev, "Slew rate", "", MOTION_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SlewModeS, NARRAY(SlewModeS), "", 0};

/********************************************
 Property: Tracking Mode
*********************************************/
static ISwitch TrackModeS[]		= {{ "Default", "", ISS_ON, 0, 0} , { "Lunar", "", ISS_OFF, 0, 0}, {"Manual", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty TrackModeSP= { mydev, "Tracking Mode", "", MOTION_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, TrackModeS, NARRAY(TrackModeS), "", 0};

/********************************************
 Property: Tracking Frequency
*********************************************/
static INumber TrackFreqN[] 		 = {{ "trackFreq", "Freq", "%g", 56.4, 60.1, 0.1, 60.1, 0, 0, 0}};
static INumberVectorProperty TrackingFreqNP= { mydev, "Tracking Frequency", "", MOTION_GROUP, IP_RW, 0, IPS_IDLE, TrackFreqN, NARRAY(TrackFreqN), "", 0};

/********************************************
 Property: Movement (Arrow keys on handset). North/South
*********************************************/
static ISwitch MovementNSS[]       = {{"MOTION_NORTH", "North", ISS_OFF, 0, 0}, {"MOTION_SOUTH", "South", ISS_OFF, 0, 0}};
ISwitchVectorProperty MovementNSSP      = { mydev, "TELESCOPE_MOTION_NS", "North/South", MOTION_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, MovementNSS, NARRAY(MovementNSS), "", 0};

/********************************************
 Property: Movement (Arrow keys on handset). West/East
*********************************************/
static ISwitch MovementWES[]       = {{"MOTION_WEST", "West", ISS_OFF, 0, 0}, {"MOTION_EAST", "East", ISS_OFF, 0, 0}};
ISwitchVectorProperty MovementWESP      = { mydev, "TELESCOPE_MOTION_WE", "West/East", MOTION_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, MovementWES, NARRAY(MovementWES), "", 0};

/********************************************
 Property: Timed Guide movement. North/South
*********************************************/
static INumber GuideNSN[]       = {{"TIMED_GUIDE_N", "North (sec)", "%g", 0, 10, 0.001, 0, 0, 0}, {"TIMED_GUIDE_S", "South (sec)", "%g", 0, 10, 0.001, 0, 0, 0}};
INumberVectorProperty GuideNSNP      = { mydev, "TELESCOPE_TIMED_GUIDE_NS", "Guide North/South", MOTION_GROUP, IP_RW, 0, IPS_IDLE, GuideNSN, NARRAY(GuideNSN), "", 0};

/********************************************
 Property: Timed Guide movement. West/East
*********************************************/
static INumber GuideWEN[]       = {{"TIMED_GUIDE_W", "West (sec)", "%g", 0, 10, 0.001, 0, 0, 0}, {"TIMED_GUIDE_E", "East (sec)", "%g", 0, 10, 0.001, 0, 0, 0}};
INumberVectorProperty GuideWENP      = { mydev, "TELESCOPE_TIMED_GUIDE_WE", "Guide West/East", MOTION_GROUP, IP_RW, 0, IPS_IDLE, GuideWEN, NARRAY(GuideWEN), "", 0};

/********************************************
 Property: Slew Accuracy
 Desciption: How close the scope have to be with
	     respect to the requested coords for 
	     the tracking operation to be successull
	     i.e. returns OK
*********************************************/
INumber SlewAccuracyN[] = {
    {"SlewRA",  "RA (arcmin)", "%g",  0., 60., 1., 3.0, 0, 0, 0},
    {"SlewkDEC", "Dec (arcmin)", "%g", 0., 60., 1., 3.0, 0, 0, 0},
};
INumberVectorProperty SlewAccuracyNP = {mydev, "Slew Accuracy", "", MOTION_GROUP, IP_RW, 0, IPS_IDLE, SlewAccuracyN, NARRAY(SlewAccuracyN), "", 0};

/********************************************
 Property: Use pulse-guide commands
 Desciption: Set to on if this mount can support
             pulse guide commands.  There appears to
             be no way to query this information from
             the mount
*********************************************/
static ISwitch UsePulseCmdS[]		= {{ "Off", "", ISS_ON, 0, 0} , { "On", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty UsePulseCmdSP= { mydev, "Use Pulse Cmd", "", MOTION_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, UsePulseCmdS, NARRAY(UsePulseCmdS), "", 0};

/**********************************************************************************************/
/************************************** GROUP: Focus ******************************************/
/**********************************************************************************************/

/********************************************
 Property: Focus Direction
*********************************************/
ISwitch  FocusMotionS[]	 = { {"IN", "Focus in", ISS_OFF, 0, 0}, {"OUT", "Focus out", ISS_OFF, 0, 0}};
ISwitchVectorProperty	FocusMotionSP = {mydev, "FOCUS_MOTION", "Motion", FOCUS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FocusMotionS, NARRAY(FocusMotionS), "", 0};

/********************************************
 Property: Focus Timer
*********************************************/
INumber  FocusTimerN[]    = { {"TIMER", "Timer (ms)", "%g", 0., 10000., 1000., 50., 0, 0, 0 }};
INumberVectorProperty FocusTimerNP = { mydev, "FOCUS_TIMER", "Focus Timer", FOCUS_GROUP, IP_RW, 0, IPS_IDLE, FocusTimerN, NARRAY(FocusTimerN), "", 0};

/********************************************
 Property: Focus Mode
*********************************************/
static ISwitch  FocusModeS[]	 = { {"FOCUS_HALT", "Halt", ISS_ON, 0, 0},
				     {"FOCUS_SLOW", "Slow", ISS_OFF, 0, 0},
				     {"FOCUS_FAST", "Fast", ISS_OFF, 0, 0}};
static ISwitchVectorProperty FocusModeSP = {mydev, "FOCUS_MODE", "Mode", FOCUS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FocusModeS, NARRAY(FocusModeS), "", 0};

/**********************************************************************************************/
/*********************************** GROUP: Date & Time ***************************************/
/**********************************************************************************************/

/********************************************
 Property: UTC Time
*********************************************/
static IText TimeT[] = {{"UTC", "UTC", 0, 0, 0, 0}};
ITextVectorProperty TimeTP = { mydev, "TIME_UTC", "UTC Time", DATETIME_GROUP, IP_RW, 0, IPS_IDLE, TimeT, NARRAY(TimeT), "", 0};

/********************************************
 Property: DST Corrected UTC Offfset
*********************************************/
static INumber UTCOffsetN[] = {{"OFFSET", "Offset", "%0.3g" , -12.,12.,0.5,0., 0, 0, 0}};
INumberVectorProperty UTCOffsetNP = { mydev, "TIME_UTC_OFFSET", "UTC Offset", DATETIME_GROUP, IP_RW, 0, IPS_IDLE, UTCOffsetN , NARRAY(UTCOffsetN), "", 0};

/********************************************
 Property: Sidereal Time
*********************************************/
static INumber SDTimeN[] = {{"LST", "Sidereal time", "%10.6m" , 0.,24.,0.,0., 0, 0, 0}};
INumberVectorProperty SDTimeNP = { mydev, "TIME_LST", "Sidereal Time", DATETIME_GROUP, IP_RW, 0, IPS_IDLE, SDTimeN, NARRAY(SDTimeN), "", 0};

/**********************************************************************************************/
/************************************* GROUP: Sites *******************************************/
/**********************************************************************************************/

/********************************************
 Property: Site Management
*********************************************/
static ISwitch SitesS[]          = {{"Site 1", "", ISS_ON, 0, 0}, {"Site 2", "", ISS_OFF, 0, 0},  {"Site 3", "", ISS_OFF, 0, 0},  {"Site 4", "", ISS_OFF, 0 ,0}};
static ISwitchVectorProperty SitesSP  = { mydev, "Sites", "", SITE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, SitesS, NARRAY(SitesS), "", 0};

/********************************************
 Property: Site Name
*********************************************/
static IText   SiteNameT[] = {{"Name", "", 0, 0, 0, 0}};
static ITextVectorProperty SiteNameTP = { mydev, "Site Name", "", SITE_GROUP, IP_RW, 0 , IPS_IDLE, SiteNameT, NARRAY(SiteNameT), "", 0};

/********************************************
 Property: Geographical Location
*********************************************/

static INumber geo[] = {
    {"LAT",  "Lat.  D:M:S +N", "%10.6m",  -90.,  90., 0., 0., 0, 0, 0},
    {"LONG", "Long. D:M:S +E", "%10.6m", 0., 360., 0., 0., 0, 0, 0},
    {"HEIGHT", "Height m", "%10.2f", -300., 6000., 0., 610., 0, 0, 0},
};
INumberVectorProperty geoNP = {
    mydev, "GEOGRAPHIC_COORD", "Geographic Location", SITE_GROUP, IP_RW, 0., IPS_IDLE,
    geo, NARRAY(geo), "", 0};

/*****************************************************************************************************/
/**************************************** END PROPERTIES *********************************************/
/*****************************************************************************************************/

void changeLX200GenericDeviceName(const char * newName)
{
  // COMM_GROUP
  strcpy(ConnectSP.device , newName);
  strcpy(PortTP.device , newName);
  strcpy(AlignmentSw.device, newName);

  // BASIC_GROUP
  strcpy(EquatorialCoordsWNP.device, newName);
  strcpy(EquatorialCoordsRNP.device, newName);
  strcpy(OnCoordSetSP.device , newName );
  strcpy(AbortSlewSP.device , newName );

  // MOTION_GROUP
  strcpy(SlewModeSP.device , newName );
  strcpy(TrackModeSP.device , newName );
  strcpy(TrackingFreqNP.device , newName );
  strcpy(MovementNSSP.device , newName );
  strcpy(MovementWESP.device , newName );
  strcpy(GuideNSNP.device , newName );
  strcpy(GuideWENP.device , newName );
  strcpy(SlewAccuracyNP.device, newName);
  strcpy(UsePulseCmdSP.device, newName);

  // FOCUS_GROUP
  strcpy(FocusModeSP.device , newName );
  strcpy(FocusMotionSP.device , newName );
  strcpy(FocusTimerNP.device, newName);

  // DATETIME_GROUP
  strcpy(TimeTP.device , newName );
  strcpy(UTCOffsetNP.device , newName );
  strcpy(SDTimeNP.device , newName );

  // SITE_GROUP
  strcpy(SitesSP.device , newName );
  strcpy(SiteNameTP.device , newName );
  strcpy(geoNP.device , newName );
  
}

void changeAllDeviceNames(const char *newName)
{
  changeLX200GenericDeviceName(newName);
  changeLX200AutostarDeviceName(newName);
  changeLX200AstroPhysicsDeviceName(newName);
  changeLX200_16DeviceName(newName);
  changeLX200ClassicDeviceName(newName);
  changeLX200GPSDeviceName(newName);
  changeLX200FS2DeviceName(newName);
}


/* send client definitions of all properties */
void ISInit()
{
  static int isInit=0;
  char *envDev = getenv("INDIDEV");

 if (isInit)
  return;

 isInit = 1;
 
  IUSaveText(&PortT[0], "/dev/ttyS0");
  IUSaveText(&TimeT[0], "YYYY-MM-DDTHH:MM:SS");

  // We need to check if UTCOffset has been set by user or not
  UTCOffsetN[0].aux0 = (int *) malloc(sizeof(int));
  *((int *) UTCOffsetN[0].aux0) = 0;
  
  
  if (strstr(me, "indi_lx200classic"))
  {
     fprintf(stderr , "initilizaing from LX200 classic device...\n");

     telescope = new LX200Classic();
     if (envDev != NULL)
     {
         changeAllDeviceNames(envDev);
         telescope->setCurrentDeviceName(envDev);
     }
     else
     {
        // 1. mydev = device_name
        changeAllDeviceNames("LX200 Classic");
        telescope->setCurrentDeviceName("LX200 Classic");
     }

     MaxReticleFlashRate = 3;
  }

  else if (strstr(me, "indi_lx200gps"))
  {
     fprintf(stderr , "initilizaing from LX200 GPS device...\n");

     // 2. device = sub_class
     telescope = new LX200GPS();

     if (envDev != NULL)
     {
         // 1. mydev = device_name
         changeAllDeviceNames(envDev);
         telescope->setCurrentDeviceName(envDev);
     }
     else
     {
         // 1. mydev = device_name
         changeAllDeviceNames("LX200 GPS");
         telescope->setCurrentDeviceName("LX200 GPS");
     }



     MaxReticleFlashRate = 9;
  }
  else if (strstr(me, "indi_lx200_16"))
  {

    IDLog("Initilizaing from LX200 16 device...\n");

    // 2. device = sub_class
   telescope = new LX200_16();

   if (envDev != NULL)
   {
       // 1. mydev = device_name
       changeAllDeviceNames(envDev);
       telescope->setCurrentDeviceName(envDev);
   }
   else
   {
       changeAllDeviceNames("LX200 16");
       telescope->setCurrentDeviceName("LX200 16");
   }

   MaxReticleFlashRate = 3;
 }
 else if (strstr(me, "indi_lx200autostar"))
 {
   fprintf(stderr , "initilizaing from autostar device...\n");
  
   // 2. device = sub_class
   telescope = new LX200Autostar();

   if (envDev != NULL)
   {
       // 1. change device name
       changeAllDeviceNames(envDev);
       telescope->setCurrentDeviceName(envDev);
   }
   else
   {
       // 1. change device name
       changeAllDeviceNames("LX200 Autostar");
       telescope->setCurrentDeviceName("LX200 Autostar");
   }

   MaxReticleFlashRate = 9;
 }
 else if (strstr(me, "indi_lx200ap"))
 {
   fprintf(stderr , "initilizaing from ap device...\n");
  

   // 2. device = sub_class
   telescope = new LX200AstroPhysics();

   if (envDev != NULL)
   {
       // 1. change device name
       changeAllDeviceNames(envDev);
       telescope->setCurrentDeviceName(envDev);
   }
   else
   {
       // 1. change device name
       changeAllDeviceNames("LX200 Astro-Physics");
        telescope->setCurrentDeviceName("LX200 Astro-Physics");
    }


   MaxReticleFlashRate = 9;
 }
 else if (strstr(me, "indi_lx200fs2"))
 {
   fprintf(stderr , "initilizaing from fs2 device...\n");
  
   // 2. device = sub_class
   telescope = new LX200Fs2();
	 
   if (envDev != NULL)
   {
	   // 1. change device name
	   changeAllDeviceNames(envDev);
	   telescope->setCurrentDeviceName(envDev);
   }
   else
   {
	  // 1. change device name
	  changeAllDeviceNames("LX200 FS2");
      telescope->setCurrentDeviceName("LX200 FS2");
   }
   
 }
 // be nice and give them a generic device
 else
 {
  telescope = new LX200Generic();

  if (envDev != NULL)
  {
      // 1. change device name
      changeAllDeviceNames(envDev);
      telescope->setCurrentDeviceName(envDev);
  }
  else
  {
      // 1. change device name
      changeAllDeviceNames("LX200 Generic");
      telescope->setCurrentDeviceName("LX200 Generic");
  }

 }

}

void ISGetProperties (const char *dev)
{ ISInit(); telescope->ISGetProperties(dev); IEAddTimer (POLLMS, ISPoll, NULL);}
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
  telescope->ISSnoopDevice(root);
}

/**************************************************
*** LX200 Generic Implementation
***************************************************/

LX200Generic::LX200Generic()
{
   currentSiteNum = 1;
   trackingMode   = LX200_TRACK_DEFAULT;
   lastSet        = -1;
   fault          = false;
   simulation     = false;
   currentSet     = 0;
   fd             = -1;
   GuideNSTID     = 0;
   GuideWETID     = 0;

   // Children call parent routines, this is the default
   IDLog("INDI Library v%g\n", INDI_LIBV);
   IDLog("initilizaing from generic LX200 device...\n");
   IDLog("Driver Version: 2008-05-21\n");
 
   enableSimulation(true);
}

LX200Generic::~LX200Generic()
{
}

void LX200Generic::setCurrentDeviceName(const char * devName)
{
  strcpy(thisDevice, devName);
}

void LX200Generic::ISGetProperties(const char *dev)
{

 if (dev && strcmp (thisDevice, dev))
    return;

  // COMM_GROUP
  IDDefSwitch (&ConnectSP, NULL);
  IDDefText   (&PortTP, NULL);
  IDDefSwitch (&AlignmentSw, NULL);

  // BASIC_GROUP
  IDDefNumber (&EquatorialCoordsWNP, NULL);
  IDDefNumber (&EquatorialCoordsRNP, NULL);
  IDDefSwitch (&OnCoordSetSP, NULL);
  IDDefSwitch (&AbortSlewSP, NULL);

  // MOTION_GROUP
  IDDefNumber (&TrackingFreqNP, NULL);
  IDDefSwitch (&SlewModeSP, NULL);
  IDDefSwitch (&TrackModeSP, NULL);
  IDDefSwitch (&MovementNSSP, NULL);
  IDDefSwitch (&MovementWESP, NULL);
  IDDefNumber (&GuideNSNP, NULL );
  IDDefNumber (&GuideWENP, NULL );
  IDDefNumber (&SlewAccuracyNP, NULL);
  IDDefSwitch (&UsePulseCmdSP, NULL);

  // FOCUS_GROUP
  IDDefSwitch(&FocusModeSP, NULL);
  IDDefSwitch(&FocusMotionSP, NULL);
  IDDefNumber(&FocusTimerNP, NULL);

  // DATETIME_GROUP
  #ifdef HAVE_NOVA_H
  IDDefText   (&TimeTP, NULL);
  IDDefNumber(&UTCOffsetNP, NULL);
  #endif

  IDDefNumber (&SDTimeNP, NULL);

  // SITE_GROUP
  IDDefSwitch (&SitesSP, NULL);
  IDDefText   (&SiteNameTP, NULL);
  IDDefNumber (&geoNP, NULL);
  
  /* Send the basic data to the new client if the previous client(s) are already connected. */		
   if (ConnectSP.s == IPS_OK)
       getBasicData();

}

void LX200Generic::ISSnoopDevice (XMLEle *root)
{
  INDI_UNUSED(root);
}

void LX200Generic::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	int err;
	IText *tp;

	// ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;

	// suppress warning
	n=n;

	if (!strcmp(name, PortTP.name) )
	{
	  PortTP.s = IPS_OK;
	  tp = IUFindText( &PortTP, names[0] );
	  if (!tp)
	   return;

	  IUSaveText(&PortTP.tp[0], texts[0]);
	  IDSetText (&PortTP, NULL);
	  return;
	}

	if (!strcmp (name, SiteNameTP.name) )
	{
	  if (checkPower(&SiteNameTP))
	   return;

	  if ( ( err = setSiteName(fd, texts[0], currentSiteNum) < 0) )
	  {
	     handleError(&SiteNameTP, err, "Setting site name");
	     return;
	  }
	     SiteNameTP.s = IPS_OK;
	     tp = IUFindText(&SiteNameTP, names[0]);
	     tp->text = new char[strlen(texts[0])+1];
	     strcpy(tp->text, texts[0]);
   	     IDSetText(&SiteNameTP , "Site name updated");
	     return;
       }

       #ifdef HAVE_NOVA_H
       if (!strcmp (name, TimeTP.name))
       {
	  if (checkPower(&TimeTP))
	   return;

	 if (simulation)
	 {
		TimeTP.s = IPS_OK;
		IUSaveText(&TimeTP.tp[0], texts[0]);
		IDSetText(&TimeTP, "Simulated time updated.");
		return;
	 }

	 struct ln_date utm;
	 struct ln_zonedate ltm;

        if (*((int *) UTCOffsetN[0].aux0) == 0)
	{
		TimeTP.s = IPS_IDLE;
		IDSetText(&TimeTP, "You must set the UTC Offset property first.");
		return;
	}

	  if (extractISOTime(texts[0], &utm) < 0)
	  {
	    TimeTP.s = IPS_IDLE;
	    IDSetText(&TimeTP , "Time invalid");
	    return;
	  }

	 // update JD
         JD = ln_get_julian_day(&utm);
	 IDLog("New JD is %f\n", (float) JD);

	ln_date_to_zonedate(&utm, &ltm, UTCOffsetN[0].value*3600.0);

	// Set Local Time
	if ( ( err = setLocalTime(fd, ltm.hours, ltm.minutes, ltm.seconds) < 0) )
	{
	          handleError(&TimeTP, err, "Setting local time");
        	  return;
	}

	if (!strcmp(dev, "LX200 GPS"))
	{
			if ( ( err = setCalenderDate(fd, utm.days, utm.months, utm.years) < 0) )
	  		{
		  		handleError(&TimeTP, err, "Setting TimeT date.");
		  		return;
			}
	}
	else
	{
			if ( ( err = setCalenderDate(fd, ltm.days, ltm.months, ltm.years) < 0) )
	  		{
		  		handleError(&TimeTP, err, "Setting local date.");
		  		return;
			}
	}
	
	// Everything Ok, save time value	
	if (IUUpdateText(&TimeTP, texts, names, n) < 0)
		return;

	TimeTP.s = IPS_OK;
 	IDSetText(&TimeTP , "Time updated to %s, updating planetary data...", texts[0]);

	// Also update telescope's sidereal time
	getSDTime(fd, &SDTimeN[0].value);
	IDSetNumber(&SDTimeNP, NULL);
	}
	#endif
}


void LX200Generic::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	int h =0, m =0, s=0, err;
	double newRA =0, newDEC =0;
	
	// ignore if not ours //
	if (strcmp (dev, thisDevice))
	    return;

        // Slewing Accuracy
        if (!strcmp (name, SlewAccuracyNP.name))
	{
		if (!IUUpdateNumber(&SlewAccuracyNP, values, names, n))
		{
			SlewAccuracyNP.s = IPS_OK;
			IDSetNumber(&SlewAccuracyNP, NULL);
			return;
		}
		
		SlewAccuracyNP.s = IPS_ALERT;
		IDSetNumber(&SlewAccuracyNP, "unknown error while setting tracking precision");
		return;
	}

	#ifdef HAVE_NOVA_H
	// DST Correct TimeT Offset
	if (!strcmp (name, UTCOffsetNP.name))
	{
		if (strcmp(names[0], UTCOffsetN[0].name))
		{
			UTCOffsetNP.s = IPS_ALERT;
			IDSetNumber( &UTCOffsetNP , "Unknown element %s for property %s.", names[0], UTCOffsetNP.label);
			return;
		}

		if (!simulation)
			if ( ( err = setUTCOffset(fd, (values[0] * -1.0)) < 0) )
			{
		        	UTCOffsetNP.s = IPS_ALERT;
	        		IDSetNumber( &UTCOffsetNP , "Setting UTC Offset failed.");
				return;
			}
		
		*((int *) UTCOffsetN[0].aux0) = 1;
		IUUpdateNumber(&UTCOffsetNP, values, names, n);
		UTCOffsetNP.s = IPS_OK;
		IDSetNumber(&UTCOffsetNP, NULL);
		return;
	}
	#endif

	if (!strcmp (name, EquatorialCoordsWNP.name))
	{
	  int i=0, nset=0;

	  if (checkPower(&EquatorialCoordsWNP))
	   return;

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
	   /*EquatorialCoordsWNP.s = IPS_BUSY;*/
	   char RAStr[32], DecStr[32];

	   fs_sexa(RAStr, newRA, 2, 3600);
	   fs_sexa(DecStr, newDEC, 2, 3600);
	  
           #ifdef INDI_DEBUG
	   IDLog("We received JNOW RA %g - DEC %g\n", newRA, newDEC);
	   IDLog("We received JNOW RA %s - DEC %s\n", RAStr, DecStr);
	   #endif

	  if (!simulation)
	   if ( (err = setObjectRA(fd, newRA)) < 0 || ( err = setObjectDEC(fd, newDEC)) < 0)
	   {
	     EquatorialCoordsWNP.s = IPS_ALERT ;
	     IDSetNumber(&EquatorialCoordsWNP, NULL);
	     handleError(&EquatorialCoordsWNP, err, "Setting RA/DEC");
	     return;
	   } 
	   /* wildi In principle this line is according to the discussion */
	   /* In case the telescope is slewing, we have to abort that. No status change here */
           /* EquatorialCoordsWNP.s = IPS_OK; */
           IDSetNumber(&EquatorialCoordsWNP, NULL);
	   targetRA  = newRA;
	   targetDEC = newDEC;

	   if (handleCoordSet())
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

	// Update Sidereal Time
        if ( !strcmp (name, SDTimeNP.name) )
	{
	  if (checkPower(&SDTimeNP))
	   return;


	  if (values[0] < 0.0 || values[0] > 24.0)
	  {
	    SDTimeNP.s = IPS_IDLE;
	    IDSetNumber(&SDTimeNP , "Time invalid");
	    return;
	  }

	  getSexComponents(values[0], &h, &m, &s);
	  IDLog("Siderial Time is %02d:%02d:%02d\n", h, m, s);
	  
	  if ( ( err = setSDTime(fd, h, m, s) < 0) )
	  {
	    handleError(&SDTimeNP, err, "Setting siderial time"); 
            return;
	  }
	  
	  SDTimeNP.np[0].value = values[0];
	  SDTimeNP.s = IPS_OK;

	  IDSetNumber(&SDTimeNP , "Sidereal time updated to %02d:%02d:%02d", h, m, s);

	  return;
        }

	// Update Geographical Location
	if (!strcmp (name, geoNP.name))
	{
	    // new geographic coords
	    double newLong = 0, newLat = 0;
	    int i, nset;
	    char msg[128];

	  if (checkPower(&geoNP))
	   return;


	    for (nset = i = 0; i < n; i++)
	    {
		INumber *geop = IUFindNumber (&geoNP, names[i]);
		if (geop == &geo[0])
		{
		    newLat = values[i];
		    nset += newLat >= -90.0 && newLat <= 90.0;
		} else if (geop == &geo[1])
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
		
		if (!simulation)
		{
			if ( ( err = setSiteLongitude(fd, 360.0 - newLong) < 0) )
	        	{	
		   		handleError(&geoNP, err, "Setting site longitude coordinates");	
		   		return;
	         	}	
			if ( ( err = setSiteLatitude(fd, newLat) < 0) )
	        	{
		   		handleError(&geoNP, err, "Setting site latitude coordinates");
		   		return;
	        	}
		}
		
		geoNP.np[0].value = newLat;
		geoNP.np[1].value = newLong;
		snprintf (msg, sizeof(msg), "Site location updated to Lat %.32s - Long %.32s", l, L);
	    } else
	    {
		geoNP.s = IPS_IDLE;
		strcpy(msg, "Lat or Long missing or invalid");
	    }
	    IDSetNumber (&geoNP, "%s", msg);
	    return;
	}

	// Update Frequency
	if ( !strcmp (name, TrackingFreqNP.name) )
	{

	 if (checkPower(&TrackingFreqNP))
	  return;

	  IDLog("Trying to set track freq of: %f\n", values[0]);

	  if ( ( err = setTrackFreq(fd, values[0])) < 0) 
	  {
             handleError(&TrackingFreqNP, err, "Setting tracking frequency");
	     return;
	 }
	 
	 TrackingFreqNP.s = IPS_OK;
	 TrackingFreqNP.np[0].value = values[0];
	 IDSetNumber(&TrackingFreqNP, "Tracking frequency set to %04.1f", values[0]);
	 if (trackingMode != LX200_TRACK_MANUAL)
	 {
	      trackingMode = LX200_TRACK_MANUAL;
	      TrackModeS[0].s = ISS_OFF;
	      TrackModeS[1].s = ISS_OFF;
	      TrackModeS[2].s = ISS_ON;
	      TrackModeSP.s   = IPS_OK;
	      selectTrackingMode(fd, trackingMode);
	      IDSetSwitch(&TrackModeSP, NULL);
	 }
	 
	  return;
	}
	
	if (!strcmp(name, FocusTimerNP.name))
	{
	  if (checkPower(&FocusTimerNP))
	   return;
	   
	  // Don't update if busy
	  if (FocusTimerNP.s == IPS_BUSY)
	   return;
	   
	  IUUpdateNumber(&FocusTimerNP, values, names, n);
	  
	  FocusTimerNP.s = IPS_OK;
	  
	  IDSetNumber(&FocusTimerNP, NULL);
	  IDLog("Setting focus timer to %g\n", FocusTimerN[0].value);
	  
	  return;

	}

	if (!strcmp(name, GuideNSNP.name))
	{
	  long direction;
	  int duration_msec;
	  int use_pulse_cmd;
	  if (checkPower(&GuideNSNP))
	   return;
	  if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
	  {
		handleError(&GuideNSNP, err, "Can't guide while moving");
		return;
	  }
	  if (GuideNSNP.s == IPS_BUSY)
	  {
		// Already guiding so stop before restarting timer
		HaltMovement(fd, LX200_NORTH);
		HaltMovement(fd, LX200_SOUTH);
	  }
	  if (GuideNSTID) {
		IERmTimer(GuideNSTID);
		GuideNSTID = 0;
	  }
	  IUUpdateNumber(&GuideNSNP, values, names, n);

	  if (GuideNSNP.np[0].value > 0) {
		duration_msec = GuideNSNP.np[0].value * 1000;
		direction = LX200_NORTH;
	  } else {
		duration_msec = GuideNSNP.np[1].value * 1000;
		direction = LX200_SOUTH;
	  }
	  if (duration_msec <= 0) {
		GuideNSNP.s = IPS_IDLE;
		IDSetNumber (&GuideNSNP, NULL);
		return;
	  }
	  use_pulse_cmd = getOnSwitch(&UsePulseCmdSP);
	  // fprintf(stderr, "Using %s mode to move %dmsec %s\n",
	  //         use_pulse_cmd ? "Pulse" : "Legacy",
	  //         duration_msec, direction == LX200_NORTH ? "North" : "South");
	  if (use_pulse_cmd) {
		SendPulseCmd(fd, direction, duration_msec);
	  } else {
		if ( ( err = setSlewMode(fd, LX200_SLEW_GUIDE) < 0) )
		{
			handleError(&SlewModeSP, err, "Setting slew mode");
			return;
		}
		MoveTo(fd, direction);
	  }
	  GuideNSTID = IEAddTimer (duration_msec, guideTimeout, (void *)direction);
	  GuideNSNP.s = IPS_BUSY;
	  IDSetNumber(&GuideNSNP, NULL);
	}
	if (!strcmp(name, GuideWENP.name))
	{
	  long direction;
	  int duration_msec;
	  int use_pulse_cmd;

	  if (checkPower(&GuideWENP))
	   return;
	  if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
	  {
		handleError(&GuideWENP, err, "Can't guide while moving");
		return;
	  }
	  if (GuideWENP.s == IPS_BUSY)
	  {
		// Already guiding so stop before restarting timer
		HaltMovement(fd, LX200_WEST);
		HaltMovement(fd, LX200_EAST);
	  }
	  if (GuideWETID) {
		IERmTimer(GuideWETID);
		GuideWETID = 0;
	  }
	  IUUpdateNumber(&GuideWENP, values, names, n);

	  if (GuideWENP.np[0].value > 0) {
		duration_msec = GuideWENP.np[0].value * 1000;
		direction = LX200_WEST;
	  } else {
		duration_msec = GuideWENP.np[1].value * 1000;
		direction = LX200_EAST;
	  }
	  if (duration_msec <= 0) {
		GuideWENP.s = IPS_IDLE;
		IDSetNumber (&GuideWENP, NULL);
		return;
	  }
	  use_pulse_cmd = getOnSwitch(&UsePulseCmdSP);
	  // fprintf(stderr, "Using %s mode to move %dmsec %s\n",
	  //         use_pulse_cmd ? "Pulse" : "Legacy",
          //         duration_msec, direction == LX200_WEST ? "West" : "East");
	  
	  if (use_pulse_cmd) {
		SendPulseCmd(fd, direction, duration_msec);
	  } else {
		if ( ( err = setSlewMode(fd, LX200_SLEW_GUIDE) < 0) )
		{
			handleError(&SlewModeSP, err, "Setting slew mode");
			return;
		}
		MoveTo(fd, direction);
	  }
	  GuideWETID = IEAddTimer (duration_msec, guideTimeout, (void *)direction);
	  GuideWENP.s = IPS_BUSY;
	  IDSetNumber(&GuideWENP, NULL);
	}
}

void LX200Generic::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	int err=0, index=0;
	INDI_UNUSED(names);

	// ignore if not ours //
	if (strcmp (thisDevice, dev))
	    return;

	// FIRST Switch ALWAYS for power
	if (!strcmp (name, ConnectSP.name))
	{
         bool connectionEstablished = (ConnectS[0].s == ISS_ON);
	 if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0) return;
	 if ( (connectionEstablished && ConnectS[0].s == ISS_ON) || (!connectionEstablished && ConnectS[1].s == ISS_ON))
	 {
		ConnectSP.s = IPS_OK;
		IDSetSwitch(&ConnectSP, NULL);
		return;
	 }
   	 connectTelescope();
	 return;
	}

	// Coord set
	if (!strcmp(name, OnCoordSetSP.name))
	{
  	  if (checkPower(&OnCoordSetSP))
	   return;

	  if (IUUpdateSwitch(&OnCoordSetSP, states, names, n) < 0) return;
	  currentSet = getOnSwitch(&OnCoordSetSP);
	  OnCoordSetSP.s = IPS_OK;
	  IDSetSwitch(&OnCoordSetSP, NULL);
	}
	
	// Abort Slew
	if (!strcmp (name, AbortSlewSP.name))
	{
	  if (checkPower(&AbortSlewSP))
	  {
	    AbortSlewSP.s = IPS_IDLE;
	    IDSetSwitch(&AbortSlewSP, NULL);
	    return;
	  }
	  
	  IUResetSwitch(&AbortSlewSP);
	  if (abortSlew(fd) < 0)
	  {
		AbortSlewSP.s = IPS_ALERT;
		IDSetSwitch(&AbortSlewSP, NULL);
		return;
	  }

	    if (EquatorialCoordsWNP.s == IPS_BUSY)
	    {
		AbortSlewSP.s = IPS_OK;
		EquatorialCoordsWNP.s       = IPS_IDLE;
		IDSetSwitch(&AbortSlewSP, "Slew aborted.");
		IDSetNumber(&EquatorialCoordsWNP, NULL);
            }
	    else if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
	    {
		MovementNSSP.s  = MovementWESP.s =  IPS_IDLE; 
	
		AbortSlewSP.s = IPS_OK;		
		EquatorialCoordsWNP.s       = IPS_IDLE;
		IUResetSwitch(&MovementNSSP);
		IUResetSwitch(&MovementWESP);
		IUResetSwitch(&AbortSlewSP);

		IDSetSwitch(&AbortSlewSP, "Slew aborted.");
		IDSetSwitch(&MovementNSSP, NULL);
		IDSetSwitch(&MovementWESP, NULL);
		IDSetNumber(&EquatorialCoordsWNP, NULL);
	    }
	    else if (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
	    {
		GuideNSNP.s  = GuideWENP.s =  IPS_IDLE; 
		GuideNSN[0].value = GuideNSN[1].value = 0.0;
		GuideWEN[0].value = GuideWEN[1].value = 0.0;
		if (GuideNSTID) {
			IERmTimer(GuideNSTID);
			GuideNSTID = 0;
		}
		if (GuideWETID) {
			IERmTimer(GuideWETID);
			GuideNSTID = 0;
		}

		AbortSlewSP.s = IPS_OK;
		EquatorialCoordsWNP.s       = IPS_IDLE;
		IUResetSwitch(&AbortSlewSP);

		IDSetSwitch(&AbortSlewSP, "Guide aborted.");
		IDSetNumber(&GuideNSNP, NULL);
		IDSetNumber(&GuideWENP, NULL);
		IDSetNumber(&EquatorialCoordsWNP, NULL);
	    }
	    else
	    {
	        AbortSlewSP.s = IPS_OK;
	        IDSetSwitch(&AbortSlewSP, NULL);
	    }

	    return;
	}

	// Alignment
	if (!strcmp (name, AlignmentSw.name))
	{
	  if (checkPower(&AlignmentSw))
	   return;

	  if (IUUpdateSwitch(&AlignmentSw, states, names, n) < 0) return;
	  index = getOnSwitch(&AlignmentSw);

	  if ( ( err = setAlignmentMode(fd, index) < 0) )
	  {
	     handleError(&AlignmentSw, err, "Setting alignment");
             return;
	  }
	  
	  AlignmentSw.s = IPS_OK;
          IDSetSwitch (&AlignmentSw, NULL);
	  return;

	}

        // Sites
	if (!strcmp (name, SitesSP.name))
	{
	  int dd=0, mm=0;

	  if (checkPower(&SitesSP))
	   return;

	  if (IUUpdateSwitch(&SitesSP, states, names, n) < 0) return;
	  currentSiteNum = getOnSwitch(&SitesSP) + 1;
	  
	  if ( ( err = selectSite(fd, currentSiteNum) < 0) )
	  {
   	      handleError(&SitesSP, err, "Selecting sites");
	      return;
	  }

	  if ( ( err = getSiteLatitude(fd, &dd, &mm) < 0))
	  {
	      handleError(&SitesSP, err, "Selecting sites");
	      return;
	  }

	  if (dd > 0) geoNP.np[0].value = dd + mm / 60.0;
	  else geoNP.np[0].value = dd - mm / 60.0;
	  
	  if ( ( err = getSiteLongitude(fd, &dd, &mm) < 0))
	  {
	        handleError(&SitesSP, err, "Selecting sites");
		return;
	  }
	  
	  if (dd > 0) geoNP.np[1].value = 360.0 - (dd + mm / 60.0);
	  else geoNP.np[1].value = (dd - mm / 60.0) * -1.0;
	  
	  getSiteName(fd, SiteNameTP.tp[0].text, currentSiteNum);

	  IDLog("Selecting site %d\n", currentSiteNum);
	  
	  geoNP.s = SiteNameTP.s = SitesSP.s = IPS_OK;

	  IDSetNumber (&geoNP, NULL);
	  IDSetText   (&SiteNameTP, NULL);
          IDSetSwitch (&SitesSP, NULL);
	  return;
	}

	// Focus Motion
	if (!strcmp (name, FocusMotionSP.name))
	{
	  if (checkPower(&FocusMotionSP))
	   return;

	  // If mode is "halt"
	  if (FocusModeS[0].s == ISS_ON)
	  {
	    FocusMotionSP.s = IPS_IDLE;
	    IDSetSwitch(&FocusMotionSP, NULL);
	    return;
	  }
	  
	  if (IUUpdateSwitch(&FocusMotionSP, states, names, n) < 0) return;
	  index = getOnSwitch(&FocusMotionSP);
	  
	  if ( ( err = setFocuserMotion(fd, index) < 0) )
	  {
	     handleError(&FocusMotionSP, err, "Setting focuser speed");
             return;
	  }

	  FocusMotionSP.s = IPS_BUSY;
	  
	  // with a timer 
	  if (FocusTimerN[0].value > 0)  
	  {
	     FocusTimerNP.s  = IPS_BUSY;
	     IEAddTimer(50, LX200Generic::updateFocusTimer, this);
	  }
	  
	  IDSetSwitch(&FocusMotionSP, NULL);
	  return;
	}

	// Slew mode
	if (!strcmp (name, SlewModeSP.name))
	{
	  if (checkPower(&SlewModeSP))
	   return;

	  if (IUUpdateSwitch(&SlewModeSP, states, names, n) < 0) return;
	  index = getOnSwitch(&SlewModeSP);
	   
	  if ( ( err = setSlewMode(fd, index) < 0) )
	  {
              handleError(&SlewModeSP, err, "Setting slew mode");
              return;
	  }
	  
          SlewModeSP.s = IPS_OK;
	  IDSetSwitch(&SlewModeSP, NULL);
	  return;
	}

	// Movement (North/South)
	if (!strcmp (name, MovementNSSP.name))
	{
	  if (checkPower(&MovementNSSP))
	   return;
	  if (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
	  {
              handleError(&MovementNSSP, err, "Can't move while guiding");
	      return;
	  }

	 int last_move=-1;
         int current_move = -1;

	// -1 means all off previously
	 last_move = getOnSwitch(&MovementNSSP);

	 if (IUUpdateSwitch(&MovementNSSP, states, names, n) < 0)
		return;

	current_move = getOnSwitch(&MovementNSSP);

	// Previosuly active switch clicked again, so let's stop.
	if (current_move == last_move)
	{
		HaltMovement(fd, (current_move == 0) ? LX200_NORTH : LX200_SOUTH);
		IUResetSwitch(&MovementNSSP);
	    	MovementNSSP.s = IPS_IDLE;
	    	IDSetSwitch(&MovementNSSP, NULL);
		return;
	}

	#ifdef INDI_DEBUG
        IDLog("Current Move: %d - Previous Move: %d\n", current_move, last_move);
	#endif

	// 0 (North) or 1 (South)
	last_move      = current_move;

	// Correction for LX200 Driver: North 0 - South 3
	current_move = (current_move == 0) ? LX200_NORTH : LX200_SOUTH;

        if ( ( err = MoveTo(fd, current_move) < 0) )
	{
	        	 handleError(&MovementNSSP, err, "Setting motion direction");
 		 	return;
	}
	
	  MovementNSSP.s = IPS_BUSY;
	  IDSetSwitch(&MovementNSSP, "Moving toward %s", (current_move == LX200_NORTH) ? "North" : "South");
	  return;
	}

	// Movement (West/East)
	if (!strcmp (name, MovementWESP.name))
	{
	  if (checkPower(&MovementWESP))
	   return;
	  if (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
	  {
              handleError(&MovementWESP, err, "Can't move while guiding");
	      return;
	  }

	 int last_move=-1;
         int current_move = -1;

	// -1 means all off previously
	 last_move = getOnSwitch(&MovementWESP);

	 if (IUUpdateSwitch(&MovementWESP, states, names, n) < 0)
		return;

	current_move = getOnSwitch(&MovementWESP);

	// Previosuly active switch clicked again, so let's stop.
	if (current_move == last_move)
	{
		HaltMovement(fd, (current_move ==0) ? LX200_WEST : LX200_EAST);
		IUResetSwitch(&MovementWESP);
	    	MovementWESP.s = IPS_IDLE;
	    	IDSetSwitch(&MovementWESP, NULL);
		return;
	}

	#ifdef INDI_DEBUG
        IDLog("Current Move: %d - Previous Move: %d\n", current_move, last_move);
	#endif

	// 0 (West) or 1 (East)
	last_move      = current_move;

	// Correction for LX200 Driver: West 1 - East 2
	current_move = (current_move == 0) ? LX200_WEST : LX200_EAST;

        if ( ( err = MoveTo(fd, current_move) < 0) )
	{
	        	 handleError(&MovementWESP, err, "Setting motion direction");
 		 	return;
	}
	
	  MovementWESP.s = IPS_BUSY;
	  IDSetSwitch(&MovementWESP, "Moving toward %s", (current_move == LX200_WEST) ? "West" : "East");
	  return;
	}

	// Tracking mode
	if (!strcmp (name, TrackModeSP.name))
	{
	  if (checkPower(&TrackModeSP))
	   return;

	  IUResetSwitch(&TrackModeSP);
	  IUUpdateSwitch(&TrackModeSP, states, names, n);
	  trackingMode = getOnSwitch(&TrackModeSP);
	  
	  if ( ( err = selectTrackingMode(fd, trackingMode) < 0) )
	  {
	         handleError(&TrackModeSP, err, "Setting tracking mode.");
		 return;
	  }
	  
          getTrackFreq(fd, &TrackFreqN[0].value);
	  TrackModeSP.s = IPS_OK;
	  IDSetNumber(&TrackingFreqNP, NULL);
	  IDSetSwitch(&TrackModeSP, NULL);
	  return;
	}

        // Focus speed
	if (!strcmp (name, FocusModeSP.name))
	{
	  if (checkPower(&FocusModeSP))
	   return;

	  IUResetSwitch(&FocusModeSP);
	  IUUpdateSwitch(&FocusModeSP, states, names, n);

	  index = getOnSwitch(&FocusModeSP);

	  /* disable timer and motion */
	  if (index == 0)
	  {
	    IUResetSwitch(&FocusMotionSP);
	    FocusMotionSP.s = IPS_IDLE;
	    FocusTimerNP.s  = IPS_IDLE;
	    IDSetSwitch(&FocusMotionSP, NULL);
	    IDSetNumber(&FocusTimerNP, NULL);
	  }
	    
	  setFocuserSpeedMode(fd, index);
	  FocusModeSP.s = IPS_OK;
	  IDSetSwitch(&FocusModeSP, NULL);
	  return;
	}

        // Pulse-Guide command support
	if (!strcmp (name, UsePulseCmdSP.name))
	{
	  if (checkPower(&UsePulseCmdSP))
	   return;

	  IUResetSwitch(&UsePulseCmdSP);
	  IUUpdateSwitch(&UsePulseCmdSP, states, names, n);

	  UsePulseCmdSP.s = IPS_OK;
	  IDSetSwitch(&UsePulseCmdSP, NULL);
	  return;
	}
}

void LX200Generic::handleError(ISwitchVectorProperty *svp, int err, const char *msg)
{
  
  svp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (check_lx200_connection(fd))
    {
      /* The telescope is off locally */
      ConnectS[0].s = ISS_OFF;
      ConnectS[1].s = ISS_ON;
      ConnectSP.s = IPS_BUSY;
      IDSetSwitch(&ConnectSP, "Telescope is not responding to commands, will retry in 10 seconds.");
      
      IDSetSwitch(svp, NULL);
      IEAddTimer(10000, retryConnection, &fd);
      return;
    }
    
   /* If the error is a time out, then the device doesn't support this property or busy*/
      if (err == -2)
      {
       svp->s = IPS_ALERT;
       IDSetSwitch(svp, "Device timed out. Current device may be busy or does not support %s. Will retry again.", msg);
      }
      else
    /* Changing property failed, user should retry. */
       IDSetSwitch( svp , "%s failed.", msg);
       
       fault = true;
}

void LX200Generic::handleError(INumberVectorProperty *nvp, int err, const char *msg)
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
      IEAddTimer(10000, retryConnection, &fd);
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

void LX200Generic::handleError(ITextVectorProperty *tvp, int err, const char *msg)
{
  
  tvp->s = IPS_ALERT;
  
  /* First check to see if the telescope is connected */
    if (check_lx200_connection(fd))
    {
      /* The telescope is off locally */
      ConnectS[0].s = ISS_OFF;
      ConnectS[1].s = ISS_ON;
      ConnectSP.s = IPS_BUSY;
      IDSetSwitch(&ConnectSP, "Telescope is not responding to commands, will retry in 10 seconds.");
      
      IDSetText(tvp, NULL);
      IEAddTimer(10000, retryConnection, &fd);
      return;
    }
    
   /* If the error is a time out, then the device doesn't support this property */
      if (err == -2)
      {
       tvp->s = IPS_ALERT;
       IDSetText(tvp, "Device timed out. Current device may be busy or does not support %s. Will retry again.", msg);
      }
       
      else
    /* Changing property failed, user should retry. */
       IDSetText( tvp , "%s failed.", msg);
       
       fault = true;
}

 void LX200Generic::correctFault()
 {
 
   fault = false;
   IDMessage(thisDevice, "Telescope is online.");
   
 }

bool LX200Generic::isTelescopeOn(void)
{
  //if (simulation) return true;
  
  return (ConnectSP.sp[0].s == ISS_ON);
}

static void retryConnection(void * p)
{
  int fd = *( (int *) p);
  
  if (check_lx200_connection(fd))
  {
    ConnectSP.s = IPS_IDLE;
    IDSetSwitch(&ConnectSP, "The connection to the telescope is lost.");
    return;
  }
  
  ConnectS[0].s = ISS_ON;
  ConnectS[1].s = ISS_OFF;
  ConnectSP.s = IPS_OK;
   
  IDSetSwitch(&ConnectSP, "The connection to the telescope has been resumed.");

}

void LX200Generic::updateFocusTimer(void *p)
{
   int err=0;

    switch (FocusTimerNP.s)
    {

      case IPS_IDLE:
	   break;
	     
      case IPS_BUSY:
      IDLog("Focus Timer Value is %g\n", FocusTimerN[0].value);
	    FocusTimerN[0].value-=50;
	    
	    if (FocusTimerN[0].value <= 0)
	    {
	      IDLog("Focus Timer Expired\n");
	      if ( ( err = setFocuserSpeedMode(telescope->fd, 0) < 0) )
              {
	        telescope->handleError(&FocusModeSP, err, "setting focuser mode");
                IDLog("Error setting focuser mode\n");
                return;
	      } 
         
	      
	      FocusMotionSP.s = IPS_IDLE;
	      FocusTimerNP.s  = IPS_OK;
	      FocusModeSP.s   = IPS_OK;
	      
              IUResetSwitch(&FocusMotionSP);
              IUResetSwitch(&FocusModeSP);
	      FocusModeS[0].s = ISS_ON;
	      
	      IDSetSwitch(&FocusModeSP, NULL);
	      IDSetSwitch(&FocusMotionSP, NULL);
	    }
	    
         IDSetNumber(&FocusTimerNP, NULL);

	  if (FocusTimerN[0].value > 0)
		IEAddTimer(50, LX200Generic::updateFocusTimer, p);
	    break;
	    
       case IPS_OK:
	    break;
	    
	case IPS_ALERT:
	    break;
     }

}

void LX200Generic::guideTimeout(void *p)
{
    long direction = (long)p;
    int use_pulse_cmd;

    use_pulse_cmd = telescope->getOnSwitch(&UsePulseCmdSP);
    if (direction == -1)
    {
	HaltMovement(telescope->fd, LX200_NORTH);
	HaltMovement(telescope->fd, LX200_SOUTH);
	HaltMovement(telescope->fd, LX200_EAST);
	HaltMovement(telescope->fd, LX200_WEST);
	IERmTimer(telescope->GuideNSTID);
	IERmTimer(telescope->GuideWETID);
	
    }
    else if (! use_pulse_cmd)
    {
	HaltMovement(telescope->fd, direction);
    }
    if (direction == LX200_NORTH || direction == LX200_SOUTH || direction == -1)
    {
        GuideNSNP.np[0].value = 0;
        GuideNSNP.np[1].value = 0;
	GuideNSNP.s = IPS_IDLE;
	telescope->GuideNSTID = 0;
	IDSetNumber(&GuideNSNP, NULL);
    }
    if (direction == LX200_WEST || direction == LX200_EAST || direction == -1)
    {
        GuideWENP.np[0].value = 0;
        GuideWENP.np[1].value = 0;
	GuideWENP.s = IPS_IDLE;
	telescope->GuideWETID = 0;
	IDSetNumber(&GuideWENP, NULL);
    }
}

void LX200Generic::ISPoll()
{
        double dx, dy;
	/*static int okCounter = 3;*/
	int err=0;
	
	if (!isTelescopeOn())
	 return;

	if (simulation)
	{
		mountSim();
		return;
        }

	if ( (err = getLX200RA(fd, &currentRA)) < 0 || (err = getLX200DEC(fd, &currentDEC)) < 0)
	{
	  EquatorialCoordsRNP.s = IPS_ALERT;
	  IDSetNumber(&EquatorialCoordsRNP, NULL);
	  handleError(&EquatorialCoordsRNP, err, "Getting RA/DEC");
	  return;
	}
	
	if (fault)
	  correctFault();

	EquatorialCoordsRNP.s = IPS_OK;

	if ( fabs(lastRA - currentRA) > (SlewAccuracyN[0].value/(60.0*15.0)) || fabs(lastDEC - currentDEC) > (SlewAccuracyN[1].value/60.0))
	{
	  	lastRA  = currentRA;
		lastDEC = currentDEC;
		IDSetNumber (&EquatorialCoordsRNP, NULL);
	}

	switch (EquatorialCoordsWNP.s)
	{
	case IPS_IDLE:
        break;

        case IPS_BUSY:
	    dx = targetRA - currentRA;
	    dy = targetDEC - currentDEC;

	    // Wait until acknowledged or within threshold
	    if ( fabs(dx) <= (SlewAccuracyN[0].value/(60.0*15.0)) && fabs(dy) <= (SlewAccuracyN[1].value/60.0))
	    {
	       lastRA  = currentRA;
	       lastDEC = currentDEC;
	       
	       EquatorialCoordsWNP.s = IPS_OK;
	       IDSetNumber(&EquatorialCoordsWNP, "Slew is complete, target locked...");
	       
	    break;

	case IPS_OK:
        break;


	case IPS_ALERT:
	    break;
	}
}

}
// wildi nothing changed in LX200Generic::mountSim
void LX200Generic::mountSim ()
{
	static struct timeval ltv;
	struct timeval tv;
	double dt, da, dx;
	int nlocked;

	/* update elapsed time since last poll, don't presume exactly POLLMS */
	gettimeofday (&tv, NULL);
	
	if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
	    ltv = tv;
	    
	dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
	ltv = tv;
	da = SLEWRATE*dt;

	/* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
	switch (EquatorialCoordsWNP.s)
	{
	
	/* #1 State is idle, update telesocpe at sidereal rate */
	case IPS_IDLE:
	    /* RA moves at sidereal, Dec stands still */
	    currentRA += (SIDRATE*dt/15.);
	    
	   IDSetNumber(&EquatorialCoordsRNP, NULL);

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
	    

	    dx = targetDEC - currentDEC;
	    if (fabs(dx) <= da)
	    {
		currentDEC = targetDEC;
		nlocked++;
	    }
	    else if (dx > 0)
	      currentDEC += da;
	    else
	      currentDEC -= da;

	    if (nlocked == 2)
	    {
		EquatorialCoordsRNP.s = IPS_OK;
		EquatorialCoordsWNP.s = IPS_OK;
		IDSetNumber(&EquatorialCoordsWNP, "Now tracking");
		IDSetNumber(&EquatorialCoordsRNP, NULL);
	    } else
		IDSetNumber(&EquatorialCoordsRNP, NULL);

	    break;

	case IPS_OK:
	    /* tracking */
	   IDSetNumber(&EquatorialCoordsRNP, NULL);
	    break;

	case IPS_ALERT:
	    break;
	}

}

void LX200Generic::getBasicData()
{

  int err;
  #ifdef HAVE_NOVA_H
  struct tm *timep;
  time_t ut;
  time (&ut);
  timep = gmtime (&ut);
  strftime (TimeTP.tp[0].text, strlen(TimeTP.tp[0].text), "%Y-%m-%dT%H:%M:%S", timep);

  IDLog("PC UTC time is %s\n", TimeTP.tp[0].text);
  #endif

  getAlignment();
  
  checkLX200Format(fd);
  
  if ( (err = getTimeFormat(fd, &timeFormat)) < 0)
     IDMessage(thisDevice, "Failed to retrieve time format from device.");
  else
  {
    timeFormat = (timeFormat == 24) ? LX200_24 : LX200_AM;
    // We always do 24 hours
    if (timeFormat != LX200_24)
      err = toggleTimeFormat(fd);
  }
// wildi proposal 
  if ( (err = getLX200RA(fd, &targetRA)) < 0 || (err = getLX200DEC(fd, &targetDEC)) < 0)
  {
     EquatorialCoordsRNP.s = IPS_ALERT;
     IDSetNumber(&EquatorialCoordsRNP, NULL);
     handleError(&EquatorialCoordsRNP, err, "Getting RA/DEC");
     return;
  }
	
  if (fault)
	correctFault();

//  getLX200RA(fd, &targetRA);
//  getLX200DEC(fd, &targetDEC);

  EquatorialCoordsRNP.np[0].value = targetRA;
  EquatorialCoordsRNP.np[1].value = targetDEC;
  
  EquatorialCoordsRNP.s = IPS_OK;
  IDSetNumber (&EquatorialCoordsRNP, NULL);  
  
  SiteNameT[0].text = new char[64];
  
  if ( (err = getSiteName(fd, SiteNameT[0].text, currentSiteNum)) < 0)
    IDMessage(thisDevice, "Failed to get site name from device");
  else
    IDSetText   (&SiteNameTP, NULL);
  
  if ( (err = getTrackFreq(fd, &TrackFreqN[0].value)) < 0)
     IDMessage(thisDevice, "Failed to get tracking frequency from device.");
  else
     IDSetNumber (&TrackingFreqNP, NULL);
     
  /*updateLocation();
  updateTime();*/
  
}

int LX200Generic::handleCoordSet()
{

  int  err;
  char syncString[256];
  char RAStr[32], DecStr[32];

  switch (currentSet)
  {
    // Slew
    case LX200_TRACK:
          lastSet = LX200_TRACK;
	  if (EquatorialCoordsWNP.s == IPS_BUSY)
	  {
	     #ifdef INDI_DEBUG
	     IDLog("Aboring Slew\n");
	     #endif
	     if (abortSlew(fd) < 0)
	     {
		AbortSlewSP.s = IPS_ALERT;
		IDSetSwitch(&AbortSlewSP, NULL);
                slewError(err);
		return (-1);
	     }

	     AbortSlewSP.s = IPS_OK;
	     EquatorialCoordsWNP.s       = IPS_IDLE;
             IDSetSwitch(&AbortSlewSP, "Slew aborted.");
	     IDSetNumber(&EquatorialCoordsWNP, NULL);

	     if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
	     {
		MovementNSSP.s  = MovementWESP.s =  IPS_IDLE; 
		EquatorialCoordsWNP.s       = IPS_IDLE;
		IUResetSwitch(&MovementNSSP);
		IUResetSwitch(&MovementWESP);
		IUResetSwitch(&AbortSlewSP);

		IDSetSwitch(&MovementNSSP, NULL);
		IDSetSwitch(&MovementWESP, NULL);
	      }

	// sleep for 100 mseconds
	usleep(100000);
	}

	if ((err = Slew(fd))) /* Slew reads the '0', that is not the end of the slew */
	{
	    IDMessage(mydev "ERROR Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
	    slewError(err);
	    return  -1;
	}

	EquatorialCoordsWNP.s = IPS_BUSY;
	fs_sexa(RAStr, targetRA, 2, 3600);
	fs_sexa(DecStr, targetDEC, 2, 3600);
	IDSetNumber(&EquatorialCoordsWNP, "Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
	#ifdef INDI_DEBUG
	IDLog("Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
        #endif
	break;

    // Sync
    case LX200_SYNC:
          lastSet = LX200_SYNC;
	   
	if (!simulation)
	  if ( ( err = Sync(fd, syncString) < 0) )
	  {
		EquatorialCoordsWNP.s = IPS_ALERT;
	        IDSetNumber(&EquatorialCoordsWNP , "Synchronization failed.");
		return (-1);
	  }

	  EquatorialCoordsWNP.s = IPS_OK;
	  IDLog("Synchronization successful %s\n", syncString);
	  IDSetNumber(&EquatorialCoordsWNP, "Synchronization successful.");
	  break;
	  
   }
   
   return (0);

}

int LX200Generic::getOnSwitch(ISwitchVectorProperty *sp)
{
 for (int i=0; i < sp->nsp ; i++)
     if (sp->sp[i].s == ISS_ON)
      return i;

 return -1;
}


int LX200Generic::checkPower(ISwitchVectorProperty *sp)
{
  if (simulation) return 0;
  
  if (ConnectSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
    	IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", sp->name);
    else
        IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", sp->label);
	
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int LX200Generic::checkPower(INumberVectorProperty *np)
{
  if (simulation) return 0;
  
  if (ConnectSP.s != IPS_OK)
  {
    if (!strcmp(np->label, ""))
    	IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", np->name);
    else
        IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", np->label);
	
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;

}

int LX200Generic::checkPower(ITextVectorProperty *tp)
{

  if (simulation) return 0;
  
  if (ConnectSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
    	IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", tp->name);
    else
        IDMessage (thisDevice, "Cannot change property %s while the telescope is offline.", tp->label);
	
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void LX200Generic::connectTelescope()
{
     switch (ConnectSP.sp[0].s)
     {
      case ISS_ON:  
	
        if (simulation)
	{
	  ConnectSP.s = IPS_OK;
	  IDSetSwitch (&ConnectSP, "Simulated telescope is online.");
	  //updateTime();
	  return;
	}
	
	 if (tty_connect(PortTP.tp[0].text, 9600, 8, 0, 1, &fd) != TTY_OK)
	 {
	   ConnectS[0].s = ISS_OFF;
	   ConnectS[1].s = ISS_ON;
	   IDSetSwitch (&ConnectSP, "Error connecting to port %s. Make sure you have BOTH write and read permission to your port.\n", PortTP.tp[0].text);
	   return;
	 }
	 if (check_lx200_connection(fd))
	 {   
	   ConnectS[0].s = ISS_OFF;
	   ConnectS[1].s = ISS_ON;
	   IDSetSwitch (&ConnectSP, "Error connecting to Telescope. Telescope is offline.");
	   return;
	 }

        #ifdef INDI_DEBUG
        IDLog("Telescope test successfful.\n");
	#endif

        *((int *) UTCOffsetN[0].aux0) = 0;
	ConnectSP.s = IPS_OK;
	IDSetSwitch (&ConnectSP, "Telescope is online. Retrieving basic data...");
	getBasicData();
	break;

     case ISS_OFF:
         ConnectS[0].s = ISS_OFF;
	 ConnectS[1].s = ISS_ON;
         ConnectSP.s = IPS_IDLE;
         IDSetSwitch (&ConnectSP, "Telescope is offline.");
	 IDLog("Telescope is offline.");
	 tty_disconnect(fd);
	 break;

    }

}

void LX200Generic::slewError(int slewCode)
{
    EquatorialCoordsWNP.s = IPS_ALERT;

    if (slewCode == 1)
	IDSetNumber(&EquatorialCoordsWNP, "Object below horizon.");
    else if (slewCode == 2)
	IDSetNumber(&EquatorialCoordsWNP, "Object below the minimum elevation limit.");
    else
	IDSetNumber(&EquatorialCoordsWNP, "Slew failed.");

}

void LX200Generic::getAlignment()
{

   if (ConnectSP.s != IPS_OK)
    return;

   signed char align = ACK(fd);
   if (align < 0)
   {
     IDSetSwitch (&AlignmentSw, "Failed to get telescope alignment.");
     return;
   }

   AlignmentS[0].s = ISS_OFF;
   AlignmentS[1].s = ISS_OFF;
   AlignmentS[2].s = ISS_OFF;

    switch (align)
    {
      case 'P': AlignmentS[0].s = ISS_ON;
       		break;
      case 'A': AlignmentS[1].s = ISS_ON;
      		break;
      case 'L': AlignmentS[2].s = ISS_ON;
            	break;
    }

    AlignmentSw.s = IPS_OK;
    IDSetSwitch (&AlignmentSw, NULL);
    IDLog("ACK success %c\n", align);
}

void LX200Generic::enableSimulation(bool enable)
{
   simulation = enable;
   
   if (simulation)
     IDLog("Warning: Simulation is activated.\n");
   else
     IDLog("Simulation is disabled.\n");
}

void LX200Generic::updateTime()
{
  #ifdef HAVE_NOVA_H
  char cdate[32];
  double ctime;
  int h, m, s, lx200_utc_offset=0;
  int day, month, year, result;
  struct tm ltm;
  struct tm utm;
  time_t time_epoch;
  
  if (simulation)
  {
    sprintf(TimeT[0].text, "%d-%02d-%02dT%02d:%02d:%02d", 1979, 6, 25, 3, 30, 30);
    IDLog("Telescope ISO date and time: %s\n", TimeT[0].text);
    IDSetText(&TimeTP, NULL);
    return;
  }

  getUTCOffset(fd, &lx200_utc_offset);

  // LX200 TimeT Offset is defined at the number of hours added to LOCAL TIME to get TimeT. This is contrary to the normal definition.
  UTCOffsetN[0].value = lx200_utc_offset*-1;

  // We got a valid value for UTCOffset now
  *((int *) UTCOffsetN[0].aux0) = 1;  

  #ifdef INDI_DEBUG
  IDLog("Telescope TimeT Offset: %g\n", UTCOffsetN[0].value);
  #endif

  getLocalTime24(fd, &ctime);
  getSexComponents(ctime, &h, &m, &s);

  if ( (result = getSDTime(fd, &SDTimeN[0].value)) < 0)
    IDMessage(thisDevice, "Failed to retrieve siderial time from device.");
  
  getCalenderDate(fd, cdate);
  result = sscanf(cdate, "%d/%d/%d", &year, &month, &day);
  if (result != 3) return;

  // Let's fill in the local time
  ltm.tm_sec = s;
  ltm.tm_min = m;
  ltm.tm_hour = h;
  ltm.tm_mday = day;
  ltm.tm_mon = month - 1;
  ltm.tm_year = year - 1900;

  // Get time epoch
  time_epoch = mktime(&ltm);

  // Convert to TimeT
  time_epoch -= (int) (UTCOffsetN[0].value * 60.0 * 60.0);

  // Get UTC (we're using localtime_r, but since we shifted time_epoch above by UTCOffset, we should be getting the real UTC time)
  localtime_r(&time_epoch, &utm);

  /* Format it into ISO 8601 */
  strftime(cdate, 32, "%Y-%m-%dT%H:%M:%S", &utm);
  IUSaveText(&TimeT[0], cdate);
  
  #ifdef INDI_DEBUG
  IDLog("Telescope Local Time: %02d:%02d:%02d\n", h, m , s);
  IDLog("Telescope SD Time is: %g\n", SDTimeN[0].value);
  IDLog("Telescope UTC Time: %s\n", TimeT[0].text);
  #endif

  // Let's send everything to the client
  IDSetText(&TimeTP, NULL);
  IDSetNumber(&SDTimeNP, NULL);
  IDSetNumber(&UTCOffsetNP, NULL);
  #endif

}

void LX200Generic::updateLocation()
{

 int dd = 0, mm = 0, err = 0;

 if (simulation)
	return;
 
 if ( (err = getSiteLatitude(fd, &dd, &mm)) < 0)
    IDMessage(thisDevice, "Failed to get site latitude from device.");
  else
  {
    if (dd > 0)
    	geoNP.np[0].value = dd + mm/60.0;
    else
        geoNP.np[0].value = dd - mm/60.0;
  
      IDLog("Autostar Latitude: %d:%d\n", dd, mm);
      IDLog("INDI Latitude: %g\n", geoNP.np[0].value);
  }
  
  if ( (err = getSiteLongitude(fd, &dd, &mm)) < 0)
    IDMessage(thisDevice, "Failed to get site longitude from device.");
  else
  {
    if (dd > 0) geoNP.np[1].value = 360.0 - (dd + mm/60.0);
    else geoNP.np[1].value = (dd - mm/60.0) * -1.0;
    
    IDLog("Autostar Longitude: %d:%d\n", dd, mm);
    IDLog("INDI Longitude: %g\n", geoNP.np[1].value);
  }
  
  IDSetNumber (&geoNP, NULL);

}

