#if 0
    LX200 Generic
    Copyright (C) 2003-2013 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

    2013-10-27: Updated driver to use INDI::Telescope (JM)

#endif

#include <config.h>
#include <libnova.h>
#include <memory>

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
#include "lx200_16.h"
#include "lx200classic.h"

// We declare an auto pointer to LX200Generic.
std::auto_ptr<LX200Generic> telescope(0);

/* There is _one_ binary for all LX200 drivers, but each binary is renamed
** to its device name (i.e. lx200gps, lx200_16..etc). The main function will
** fetch from std args the binary name and ISInit will create the apporpiate
** device afterwards. If the binary name does not match any known devices,
** we simply create a generic device.
*/
extern char* me;

#define LX200_TRACK	0
#define LX200_SYNC	1

/* Simulation Parameters */
#define	SLEWRATE	1		/* slew rate, degrees/s */
#define SIDRATE		0.004178	/* sidereal rate, degrees/s */

/* send client definitions of all properties */
void ISInit()
{
  static int isInit=0;

 if (isInit)
  return;

 isInit = 1;
 
  if (strstr(me, "indi_lx200classic"))
  {
     IDLog("initializing from LX200 classic device...\n");

     if(telescope.get() == 0) telescope.reset(new LX200Classic());


  }
  else if (strstr(me, "indi_lx200gps"))
  {
     IDLog("initializing from LX200 GPS device...\n");

     if(telescope.get() == 0) telescope.reset(new LX200GPS());


  }
  else if (strstr(me, "indi_lx200_16"))
  {

    IDLog("Initializing from LX200 16 device...\n");

   if(telescope.get() == 0) telescope.reset(new LX200_16());


 }
 else if (strstr(me, "indi_lx200autostar"))
 {
   IDLog("initializing from Autostar device...\n");
  
   if(telescope.get() == 0) telescope.reset(new LX200Autostar());


 }
 // be nice and give them a generic device
 else
   if(telescope.get() == 0) telescope.reset(new LX200Generic());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        telescope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        telescope->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        telescope->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        telescope->ISNewNumber(dev, name, values, names, num);
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
    telescope->ISSnoopDevice(root);
}

/**************************************************
*** LX200 Generic Implementation
***************************************************/

LX200Generic::LX200Generic()
{
   setVersion(2, 0);

   currentSiteNum = 1;
   trackingMode   = LX200_TRACK_DEFAULT;
   GuideNSTID     = 0;
   GuideWETID     = 0;

   controller = new INDI::Controller(this);

   controller->setJoystickCallback(joystickHelper);
   controller->setButtonCallback(buttonHelper);

   IDLog("Initializing from generic LX200 device...\n");   
}

LX200Generic::~LX200Generic()
{ 
    delete(controller);
}

const char * LX200Generic::getDefaultName()
{
    return (char *)"LX200 Generic";
}

bool LX200Generic::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    IUFillSwitch(&AlignmentS[0], "Polar", "", ISS_ON);
    IUFillSwitch(&AlignmentS[1], "AltAz", "", ISS_OFF);
    IUFillSwitch(&AlignmentS[2], "Land", "", ISS_OFF);
    IUFillSwitchVector(&AlignmentSP, AlignmentS, 3, getDeviceName(), "Alignment", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&SlewModeS[0], "Max", "", ISS_ON);
    IUFillSwitch(&SlewModeS[1], "Find", "", ISS_OFF);
    IUFillSwitch(&SlewModeS[2], "Centering", "", ISS_OFF);
    IUFillSwitch(&SlewModeS[3], "Guide", "", ISS_OFF);
    IUFillSwitchVector(&SlewModeSP, SlewModeS, 4, getDeviceName(), "Slew Rate", "", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&TrackModeS[0], "Default", "", ISS_ON);
    IUFillSwitch(&TrackModeS[1], "Lunar", "", ISS_OFF);
    IUFillSwitch(&TrackModeS[2], "Manual", "", ISS_OFF);
    IUFillSwitchVector(&TrackModeSP, TrackModeS, 3, getDeviceName(), "Tracking Mode", "", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&TrackFreqN[0], "trackFreq", "Freq", "%g", 56.4,60.1, 0.1, 60.1);
    IUFillNumberVector(&TrackingFreqNP, TrackFreqN, 1, getDeviceName(), "Tracking Frequency", "", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    IUFillSwitch(&UsePulseCmdS[0], "Off", "", ISS_ON);
    IUFillSwitch(&UsePulseCmdS[1], "On", "", ISS_OFF);
    IUFillSwitchVector(&UsePulseCmdSP, UsePulseCmdS, 2, getDeviceName(), "Use Pulse Cmd", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&SiteS[0], "Site 1", "", ISS_ON);
    IUFillSwitch(&SiteS[1], "Site 2", "", ISS_OFF);
    IUFillSwitch(&SiteS[2], "Site 3", "", ISS_OFF);
    IUFillSwitch(&SiteS[3], "Site 4", "", ISS_OFF);
    IUFillSwitchVector(&SiteSP, SiteS, 4, getDeviceName(), "Sites", "", SITE_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&SiteNameT[0], "Name", "", "");
    IUFillTextVector(&SiteNameTP, SiteNameT, 1, getDeviceName(), "Site Name", "", SITE_TAB, IP_RW, 0, IPS_IDLE);


    IUFillSwitch(&FocusMotionS[0], "IN", "Focus in", ISS_OFF);
    IUFillSwitch(&FocusMotionS[1], "OUT", "Focus out", ISS_OFF);
    IUFillSwitchVector(&FocusMotionSP, FocusMotionS, 2, getDeviceName(), "FOCUS_MOTION", "Motion", FOCUS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&FocusTimerN[0], "TIMER", "Timer (ms)", "%g", 0, 10000., 1000., 0);
    IUFillNumberVector(&FocusTimerNP, FocusTimerN, 1, getDeviceName(), "FOCUS_TIMER", "Focus Timer", FOCUS_TAB, IP_RW, 0, IPS_IDLE);

    IUFillSwitch(&FocusModeS[0], "FOCUS_HALT", "Halt", ISS_ON);
    IUFillSwitch(&FocusModeS[1], "FOCUS_SLOW", "Slow", ISS_OFF);
    IUFillSwitch(&FocusModeS[2], "FOCUS_FAST", "Fast", ISS_OFF);
    IUFillSwitchVector(&FocusModeSP, FocusModeS, 3, getDeviceName(), "FOCUS_MODE", "Mode", FOCUS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    controller->mapController("NSWE Control", "NSWE Control", INDI::Controller::CONTROLLER_JOYSTICK, "JOYSTICK_1");
    controller->mapController("Slew Max", "Slew Max", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->mapController("Slew Find", "Slew Find", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_2");
    controller->mapController("Slew Centering", "Slew Centering", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_3");
    controller->mapController("Slew Guide", "Slew Guide", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_4");
    controller->mapController("Abort Motion", "Abort Motion", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_5");

    controller->initProperties();


    TrackState=SCOPE_IDLE;

    initGuiderProperties(getDeviceName(), GUIDE_TAB);

    /* Add debug/simulation/config controls so we may debug driver if necessary */
    addAuxControls();

    return true;
}

void LX200Generic::ISGetProperties(const char *dev)
{
    if(dev && strcmp(dev,getDeviceName()))
        return;

    INDI::Telescope::ISGetProperties(dev);

    if (isConnected())
    {
        defineSwitch(&AlignmentSP);
        defineSwitch(&SlewModeSP);
        defineSwitch(&TrackModeSP);
        defineNumber(&TrackingFreqNP);
        defineSwitch(&UsePulseCmdSP);

        defineSwitch(&SiteSP);
        defineText(&SiteNameTP);

        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);

        defineSwitch(&FocusMotionSP);
        defineNumber(&FocusTimerNP);
        defineSwitch(&FocusModeSP);
    }

    controller->ISGetProperties(dev);

}

bool LX200Generic::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineSwitch(&AlignmentSP);
        defineSwitch(&SlewModeSP);
        defineSwitch(&TrackModeSP);
        defineNumber(&TrackingFreqNP);
        defineSwitch(&UsePulseCmdSP);

        defineSwitch(&SiteSP);
        defineText(&SiteNameTP);

        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);

        defineSwitch(&FocusMotionSP);
        defineNumber(&FocusTimerNP);
        defineSwitch(&FocusModeSP);

        getBasicData();
    }
    else
    {
        deleteProperty(AlignmentSP.name);
        deleteProperty(SlewModeSP.name);
        deleteProperty(TrackModeSP.name);
        deleteProperty(TrackingFreqNP.name);
        deleteProperty(UsePulseCmdSP.name);

        deleteProperty(SiteSP.name);
        deleteProperty(SiteNameTP.name);

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);

        deleteProperty(FocusMotionSP.name);
        deleteProperty(FocusTimerNP.name);
        deleteProperty(FocusModeSP.name);
    }

    controller->updateProperties();

    return true;
}

bool LX200Generic::Connect()
{
    bool rc=false;

    if(isConnected()) return true;

    rc=Connect(PortT[0].text);

    if(rc)
        SetTimer(POLLMS);

    return rc;
}

bool LX200Generic::Connect(char *port)
{
    if (isSimulation())
    {
        IDMessage (getDeviceName(), "Simulated LX200 is online. Retrieving basic data...");
        return true;
    }

    if (tty_connect(port, 9600, 8, 0, 1, &PortFD) != TTY_OK)
    {
      IDMessage(getDeviceName(), "Error connecting to port %s. Make sure you have BOTH write and read permission to your port.", port);
      return false;
    }


    if (check_lx200_connection(PortFD))
    {
        IDMessage(getDeviceName(), "Error connecting to Telescope. Telescope is offline.");
        return false;
    }

   //*((int *) UTCOffsetN[0].aux0) = 0;

   IDMessage (getDeviceName(), "Telescope is online. Retrieving basic data...");

   return true;
}

bool LX200Generic::Disconnect()
{
    if (isSimulation() == false)
        tty_disconnect(PortFD);
    return true;
}

bool LX200Generic::ReadScopeStatus()
{
    if (isConnected() == false)
     return false;

    if (isSimulation())
    {
        mountSim();
        return true;
    }

    //if (check_lx200_connection(PortFD))
        //return false;

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if(isSlewComplete(PortFD) == 0)
        {
            // Set slew mode to "Centering"
            IUResetSwitch(&SlewModeSP);
            SlewModeS[2].s = ISS_ON;
            IDSetSwitch(&SlewModeSP, NULL);

            //  Nothing to do here
            if(TrackState==SCOPE_PARKING)
            {
                TrackState=SCOPE_PARKED;
                IDMessage(getDeviceName(),"Telescope is Parked.");
            }
            else
            {
                TrackState=SCOPE_TRACKING;
                IDMessage(getDeviceName(),"Slew is complete. Tracking...");
            }

        }
    }
    else if(TrackState == SCOPE_PARKING)
    {
        if(isSlewComplete(PortFD) == 0)
        {
            TrackState=SCOPE_PARKED;
            ParkSP.s=IPS_OK;
            IDSetSwitch(&ParkSP,NULL);
            IDMessage(getDeviceName(),"Telescope is Parked.");
        }

    }

    if ( getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
      EqNP.s = IPS_ALERT;
      IDSetNumber(&EqNP, "Error reading RA/DEC.");
      return false;
    }

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200Generic::Goto(double r,double d)
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
        if (setObjectRA(PortFD, targetRA) < 0 || (setObjectDEC(PortFD, targetDEC)) < 0)
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

    //Parked=false;
    TrackState = SCOPE_SLEWING;
    EqNP.s    = IPS_BUSY;

    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool LX200Generic::Sync(double ra, double dec)
{
    char syncString[256];


    if (isSimulation() == false &&
            (setObjectRA(PortFD, ra) < 0 || (setObjectDEC(PortFD, dec)) < 0))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error setting RA/DEC. Unable to Sync.");
        return false;
    }

    if (isSimulation() == false &&  ::Sync(PortFD, syncString) < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP , "Synchronization failed.");
        return false;
    }

    currentRA  = ra;
    currentDEC = dec;

    if (isDebug())
        IDLog("Synchronization successful %s\n", syncString);

    IDMessage(getDeviceName(), "Synchronization successful.");

    TrackState = SCOPE_IDLE;
    EqNP.s    = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200Generic::Park()
{
    if (isSimulation() == false)
    {
        // If scope is moving, let's stop it first.
        if (EqNP.s == IPS_BUSY)
        {
           if (isSimulation() == false && abortSlew(PortFD) < 0)
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

          if (isSimulation() == false && slewToPark(PortFD) < 0)
          {
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, "Parking Failed.");
            return false;
          }

    }

    ParkSP.s = IPS_OK;
    //Parked=true;
    TrackState = SCOPE_PARKING;
    IDMessage(getDeviceName(), "Parking telescope in progress...");
    return true;
}

bool LX200Generic::MoveNS(TelescopeMotionNS dir)
{
    static int last_move=-1;

     int current_move = -1;

    current_move = IUFindOnSwitchIndex(&MovementNSSP);

    // Previosuly active switch clicked again, so let's stop.
    if (current_move == last_move && current_move != -1)
    {
        if (isSimulation() == false)
            HaltMovement(PortFD, (current_move == 0) ? LX200_NORTH : LX200_SOUTH);

        IUResetSwitch(&MovementNSSP);
        MovementNSSP.s = IPS_IDLE;
        IDSetSwitch(&MovementNSSP, "Movement toward %s halted.", (current_move == 0) ? "North" : "South");
        last_move = -1;
        return true;
    }

    last_move = current_move;

    if (isDebug())
        IDLog("Current Move: %d - Previous Move: %d\n", current_move, last_move);

    // Correction for LX200 Driver: North 0 - South 3
    current_move = (dir == MOTION_NORTH) ? LX200_NORTH : LX200_SOUTH;

    if (isSimulation() == false && MoveTo(PortFD, current_move) < 0)
    {
        MovementNSSP.s = IPS_ALERT;
        IDSetSwitch(&MovementNSSP, "Error setting N/S motion direction.");
        return false;
    }


      MovementNSSP.s = IPS_BUSY;
      IDSetSwitch(&MovementNSSP, "Moving toward %s.", (current_move == LX200_NORTH) ? "North" : "South");
      return true;
}

bool LX200Generic::MoveWE(TelescopeMotionWE dir)
{
    static int last_move=-1;

   int current_move = -1;

   current_move = IUFindOnSwitchIndex(&MovementWESP);

  // Previosuly active switch clicked again, so let's stop.
  if (current_move == last_move && current_move != -1)
  {
      if (isSimulation() == false)
        HaltMovement(PortFD, (current_move == 0) ? LX200_WEST : LX200_EAST);
      IUResetSwitch(&MovementWESP);
      MovementWESP.s = IPS_IDLE;
      IDSetSwitch(&MovementWESP, "Movement toward %s halted.", (current_move == 0) ? "West" : "East");
      last_move = -1;
      return true;
  }

  last_move = current_move;

  if (isDebug())
      IDLog("Current Move: %d - Previous Move: %d\n", current_move, last_move);

  current_move = (dir == MOTION_WEST) ? LX200_WEST : LX200_EAST;

  if (isSimulation() == false && MoveTo(PortFD, current_move) < 0)
  {
      MovementWESP.s = IPS_ALERT;
      IDSetSwitch(&MovementWESP, "Error setting W/E motion direction.");
      return false;
  }

    MovementWESP.s = IPS_BUSY;
    IDSetSwitch(&MovementWESP, "Moving toward %s.", (current_move == LX200_WEST) ? "West" : "East");
    return true;
}

bool LX200Generic::Abort()
{
     if (isSimulation() == false && abortSlew(PortFD) < 0)
     {
         IDMessage(getDeviceName(), "Failed to abort slew.");
         return false;
     }

     if (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
     {
        GuideNSNP.s  = GuideWENP.s =  IPS_IDLE;
        GuideNSN[0].value = GuideNSN[1].value = 0.0;
        GuideWEN[0].value = GuideWEN[1].value = 0.0;

        if (GuideNSTID)
        {
            IERmTimer(GuideNSTID);
            GuideNSTID = 0;
        }

        if (GuideWETID)
        {
            IERmTimer(GuideWETID);
            GuideNSTID = 0;
        }

        IDMessage(getDeviceName(), "Guide aborted.");
        IDSetNumber(&GuideNSNP, NULL);
        IDSetNumber(&GuideWENP, NULL);

        return true;
     }

     IUResetSwitch(&MovementNSSP);
     IUResetSwitch(&MovementWESP);
     MovementNSSP.s = IPS_IDLE;
     MovementWESP.s = IPS_IDLE;
     EqNP.s = IPS_IDLE;

     TrackState = SCOPE_IDLE;

     IDSetSwitch(&MovementNSSP, NULL);
     IDSetSwitch(&MovementWESP, NULL);
     IDSetNumber(&EqNP, NULL);
     IDMessage(getDeviceName(), "Slew aborted.");
     return true;
}

bool LX200Generic::updateTime(ln_date * utc, double utc_offset)
{
    struct ln_zonedate ltm;

    if (isSimulation())
        return true;

    ln_date_to_zonedate(utc, &ltm, utc_offset*3600.0);

    JD = ln_get_julian_day(utc);

    if (isDebug())
      IDLog("New JD is %f\n", (float) JD);

    // Set Local Time
    if (setLocalTime(PortFD, ltm.hours, ltm.minutes, ltm.seconds) < 0)
    {
        IDMessage(getDeviceName(), "Error setting local time.");
        return false;
    }

    // Special case for LX200 GPS
    if (!strcmp(getDeviceName(), "LX200 GPS"))
    {
      if (setCalenderDate(PortFD, utc->days, utc->months, utc->years) < 0)
      {
          IDMessage(getDeviceName(), "Error setting UTC calender date.");
          return false;
      }
   }
   else
   {
      if (setCalenderDate(PortFD, ltm.days, ltm.months, ltm.years) < 0)
      {
          IDMessage(getDeviceName(), "Error setting local date.");
          return false;
      }
   }

    // Meade defines UTC Offset as the offset ADDED to local time to yield UTC, which
    // is the opposite of the standard definition of UTC offset!
    if (setUTCOffset(PortFD, (utc_offset * -1.0)) < 0)
    {
        IDMessage(getDeviceName() , "Error setting UTC Offset.");
        return false;
    }

   IDMessage(getDeviceName() , "Time updated, updating planetary data...");
   return true;
}

bool LX200Generic::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    if (isSimulation() == false && setSiteLongitude(PortFD, 360.0 - longitude) < 0)
    {
        IDMessage(getDeviceName(), "Error setting site longitude coordinates");
        return false;
    }

    if (isSimulation() == false && setSiteLatitude(PortFD, latitude) < 0)
    {
        IDMessage(getDeviceName(), "Error setting site latitude coordinates");
        return false;
    }

    char l[32], L[32];
    fs_sexa (l, latitude, 3, 3600);
    fs_sexa (L, longitude, 4, 3600);

    IDMessage(getDeviceName(), "Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

bool LX200Generic::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp (name, SiteNameTP.name) )
        {
          if (isSimulation() == false && setSiteName(PortFD, texts[0], currentSiteNum) < 0)
          {
              SiteNameTP.s = IPS_ALERT;
              IDSetText(&SiteNameTP, "Setting site name");
              return false;
          }

          SiteNameTP.s = IPS_OK;
          IText *tp = IUFindText(&SiteNameTP, names[0]);
          IUSaveText(tp, texts[0]);
          IDSetText(&SiteNameTP , "Site name updated");
          return true;
         }

    }

    controller->ISNewText(dev, name, texts, names, n);

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool LX200Generic::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // Update Frequency
        if ( !strcmp (name, TrackingFreqNP.name) )
        {

            if (isDebug())
                IDLog("Trying to set track freq of: %f\n", values[0]);

          if (isSimulation() == false && setTrackFreq(PortFD, values[0]) < 0)
          {
              TrackingFreqNP.s = IPS_ALERT;
              IDSetNumber(&TrackingFreqNP, "Error setting tracking frequency");
              return false;
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
              selectTrackingMode(PortFD, trackingMode);
              IDSetSwitch(&TrackModeSP, NULL);
         }

          return true;
        }

        if (!strcmp(name, FocusTimerNP.name))
        {
          // Don't update if busy
          if (FocusTimerNP.s == IPS_BUSY)
           return true;

          IUUpdateNumber(&FocusTimerNP, values, names, n);

          FocusTimerNP.s = IPS_OK;

          IDSetNumber(&FocusTimerNP, NULL);

          if (isDebug())
            IDLog("Setting focus timer to %g\n", FocusTimerN[0].value);

          return true;

        }

        processGuiderProperties(name, values, names, n);
    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);

}

bool LX200Generic::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int index=0;

    if(strcmp(dev,getDeviceName())==0)
    {
        // Alignment
        if (!strcmp (name, AlignmentSP.name))
        {

          if (IUUpdateSwitch(&AlignmentSP, states, names, n) < 0)
              return false;

          index = IUFindOnSwitchIndex(&AlignmentSP);

          if (isSimulation() == false && setAlignmentMode(PortFD, index) < 0)
          {
             AlignmentSP.s = IPS_ALERT;
             IDSetSwitch(&AlignmentSP, "Error setting alignment mode.");
             return false;
          }

          AlignmentSP.s = IPS_OK;
          IDSetSwitch (&AlignmentSP, NULL);
          return true;
        }

        // Sites
        if (!strcmp (name, SiteSP.name))
        {
          if (IUUpdateSwitch(&SiteSP, states, names, n) < 0)
              return false;

          currentSiteNum = IUFindOnSwitchIndex(&SiteSP) + 1;

          if (isSimulation() == false && selectSite(PortFD, currentSiteNum) < 0)
          {
              SiteSP.s = IPS_ALERT;
              IDSetSwitch(&SiteSP, "Error selecting sites.");
              return false;
          }

          if (isSimulation())
              IUSaveText(&SiteNameTP.tp[0], "Sample Site");
          else
              getSiteName(PortFD, SiteNameTP.tp[0].text, currentSiteNum);

          if (isDebug())
            IDLog("Selecting site %d\n", currentSiteNum);

          sendScopeLocation();

          SiteNameTP.s = SiteSP.s = IPS_OK;

          IDSetText   (&SiteNameTP, NULL);
          IDSetSwitch (&SiteSP, NULL);

          return false;
        }

        // Focus Motion
        if (!strcmp (name, FocusMotionSP.name))
        {
          // If mode is "halt"
          if (FocusModeS[0].s == ISS_ON)
          {
            FocusMotionSP.s = IPS_IDLE;
            IDSetSwitch(&FocusMotionSP, "Focus mode is halt. Select slow or fast mode");
            return true;
          }

          int last_motion = IUFindOnSwitchIndex(&FocusMotionSP);

          if (IUUpdateSwitch(&FocusMotionSP, states, names, n) < 0)
              return false;

          index = IUFindOnSwitchIndex(&FocusMotionSP);

          // If same direction and we're busy, stop
          if (last_motion == index && FocusMotionSP.s == IPS_BUSY)
          {
              IUResetSwitch(&FocusMotionSP);
              FocusMotionSP.s = IPS_IDLE;
              setFocuserSpeedMode(PortFD, 0);
              IDSetSwitch(&FocusMotionSP, NULL);
              return true;
          }

          if (isSimulation() == false && setFocuserMotion(PortFD, index) < 0)
          {
              FocusMotionSP.s = IPS_ALERT;
              IDSetSwitch(&FocusMotionSP, "Error setting focuser speed.");
              return false;
          }

          // with a timer
          if (FocusTimerN[0].value > 0)
          {
             FocusTimerNP.s  = IPS_BUSY;
             FocusMotionSP.s = IPS_BUSY;
             IEAddTimer(50, LX200Generic::updateFocusHelper, this);
          }

          FocusMotionSP.s = IPS_OK;
          IDSetSwitch(&FocusMotionSP, NULL);
          return true;
        }

        // Slew mode
        if (!strcmp (name, SlewModeSP.name))
        {

          if (IUUpdateSwitch(&SlewModeSP, states, names, n) < 0)
              return false;

          index = IUFindOnSwitchIndex(&SlewModeSP);

          if (isSimulation() == false && setSlewMode(PortFD, index) < 0)
          {
              SlewModeSP.s = IPS_ALERT;
              IDSetSwitch(&SlewModeSP, "Error setting slew mode.");
              return false;
          }

          SlewModeSP.s = IPS_OK;
          IDSetSwitch(&SlewModeSP, NULL);
          return true;
        }

        // Tracking mode
        if (!strcmp (name, TrackModeSP.name))
        {
          IUResetSwitch(&TrackModeSP);
          IUUpdateSwitch(&TrackModeSP, states, names, n);
          trackingMode = IUFindOnSwitchIndex(&TrackModeSP);

          if (isSimulation() == false && selectTrackingMode(PortFD, trackingMode) < 0)
          {
              TrackModeSP.s = IPS_ALERT;
              IDSetSwitch(&TrackModeSP, "Error setting tracking mode.");
              return false;
          }

          if (isSimulation() == false)
            getTrackFreq(PortFD, &TrackFreqN[0].value);
          TrackModeSP.s = IPS_OK;
          IDSetNumber(&TrackingFreqNP, NULL);
          IDSetSwitch(&TrackModeSP, NULL);
          return true;
        }

        // Focus speed
        if (!strcmp (name, FocusModeSP.name))
        {
          IUResetSwitch(&FocusModeSP);
          IUUpdateSwitch(&FocusModeSP, states, names, n);

          index = IUFindOnSwitchIndex(&FocusModeSP);

          /* disable timer and motion */
          if (index == 0)
          {
            IUResetSwitch(&FocusMotionSP);
            FocusMotionSP.s = IPS_IDLE;
            FocusTimerNP.s  = IPS_IDLE;
            IDSetSwitch(&FocusMotionSP, NULL);
            IDSetNumber(&FocusTimerNP, NULL);
          }

          if (isSimulation() == false)
            setFocuserSpeedMode(PortFD, index);
          FocusModeSP.s = IPS_OK;
          IDSetSwitch(&FocusModeSP, NULL);
          return true;
        }

        // Pulse-Guide command support
        if (!strcmp (name, UsePulseCmdSP.name))
        {

          IUResetSwitch(&UsePulseCmdSP);
          IUUpdateSwitch(&UsePulseCmdSP, states, names, n);

          UsePulseCmdSP.s = IPS_OK;
          IDSetSwitch(&UsePulseCmdSP, NULL);
          return true;
        }

    }

    controller->ISNewSwitch(dev, name, states, names, n);

    //  Nobody has claimed this, so pass it to the parent
    return INDI::Telescope::ISNewSwitch(dev,name,states,names,n);

}

void LX200Generic::updateFocusHelper(void *p)
{
    ((LX200Generic *) p)->updateFocusTimer();
}

void LX200Generic::updateFocusTimer()
{
    switch (FocusTimerNP.s)
    {

      case IPS_IDLE:
	   break;
	     
      case IPS_BUSY:
        if (isDebug())
        IDLog("Focus Timer Value is %g\n", FocusTimerN[0].value);

	    FocusTimerN[0].value-=50;
	    
	    if (FocusTimerN[0].value <= 0)
	    {
            if (isDebug())
                IDLog("Focus Timer Expired\n");

          if (isSimulation() == false && setFocuserSpeedMode(telescope->PortFD, 0) < 0)
           {
              FocusModeSP.s = IPS_ALERT;
              IDSetSwitch(&FocusModeSP, "Error setting focuser mode.");

              if (isDebug())
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
        IEAddTimer(50, LX200Generic::updateFocusHelper, this);
	    break;
	    
       case IPS_OK:
	    break;
	    
	case IPS_ALERT:
	    break;
     }

}

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
    switch (TrackState)
	{

    case SCOPE_TRACKING:
	    /* RA moves at sidereal, Dec stands still */
	    currentRA += (SIDRATE*dt/15.);
	    break;

    case SCOPE_SLEWING:
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
             TrackState = SCOPE_TRACKING;


	    break;

    default:
	    break;
	}

    NewRaDec(currentRA, currentDEC);


}

void LX200Generic::getBasicData()
{
  if (isSimulation() == false)
  {
      getAlignment();

      checkLX200Format(PortFD);

      if (getTimeFormat(PortFD, &timeFormat) < 0)
          IDMessage(getDeviceName(), "Failed to retrieve time format from device.");
      else
      {
        timeFormat = (timeFormat == 24) ? LX200_24 : LX200_AM;
        // We always do 24 hours
        if (timeFormat != LX200_24)
          toggleTimeFormat(PortFD);
      }

      SiteNameT[0].text = new char[64];

      if (getSiteName(PortFD, SiteNameT[0].text, currentSiteNum) < 0)
        IDMessage(getDeviceName(), "Failed to get site name from device");
      else
        IDSetText   (&SiteNameTP, NULL);

      if (getTrackFreq(PortFD, &TrackFreqN[0].value) < 0)
          IDMessage(getDeviceName(), "Failed to get tracking frequency from device.");
      else
         IDSetNumber (&TrackingFreqNP, NULL);
  }
     
  sendScopeLocation();
  sendScopeTime();
  
}

void LX200Generic::slewError(int slewCode)
{
    EqNP.s = IPS_ALERT;

    if (slewCode == 1)
    IDSetNumber(&EqNP, "Object below horizon.");
    else if (slewCode == 2)
    IDSetNumber(&EqNP, "Object below the minimum elevation limit.");
    else
    IDSetNumber(&EqNP, "Slew failed.");

}

void LX200Generic::getAlignment()
{

   signed char align = ACK(PortFD);
   if (align < 0)
   {
     IDSetSwitch (&AlignmentSP, "Failed to get telescope alignment.");
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

    AlignmentSP.s = IPS_OK;
    IDSetSwitch (&AlignmentSP, NULL);
}

void LX200Generic::sendScopeTime()
{
  char cdate[32];
  double ctime;
  int h, m, s, lx200_utc_offset=0;
  int day, month, year, result;
  struct tm ltm;
  struct tm utm;
  time_t time_epoch;
  
  if (isSimulation())
  {
    snprintf(cdate, 32, "%d-%02d-%02dT%02d:%02d:%02d", 1979, 6, 25, 3, 30, 30);
    IDLog("Telescope ISO date and time: %s\n", cdate);
    IUSaveText(&TimeT[0], cdate);
    IUSaveText(&TimeT[1], "3");
    IDSetText(&TimeTP, NULL);
    return;
  }

  getUTCOffset(PortFD, &lx200_utc_offset);

  // LX200 TimeT Offset is defined at the number of hours added to LOCAL TIME to get TimeT. This is contrary to the normal definition.
  char utcStr[8];
  snprintf(utcStr, 8, "%02d", lx200_utc_offset*-1);
  IUSaveText(&TimeT[1], utcStr);

  if (isDebug())
      IDLog("Telescope TimeT Offset: %s\n", TimeT[1].text);

  getLocalTime24(PortFD, &ctime);
  getSexComponents(ctime, &h, &m, &s);

  getCalenderDate(PortFD, cdate);
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
  time_epoch -= (int) (atof(TimeT[1].text) * 3600.0);

  // Get UTC (we're using localtime_r, but since we shifted time_epoch above by UTCOffset, we should be getting the real UTC time
  localtime_r(&time_epoch, &utm);

  /* Format it into ISO 8601 */
  strftime(cdate, 32, "%Y-%m-%dT%H:%M:%S", &utm);
  IUSaveText(&TimeT[0], cdate);
  
  if (isDebug())
  {
    IDLog("Telescope Local Time: %02d:%02d:%02d\n", h, m , s);
    IDLog("Telescope UTC Time: %s\n", TimeT[0].text);
  }

  // Let's send everything to the client
  IDSetText(&TimeTP, NULL);


}

void LX200Generic::sendScopeLocation()
{
 int dd = 0, mm = 0;

 if (isSimulation())
 {
     LocationNP.np[0].value = 29.5;
     LocationNP.np[1].value = 48.0;
     LocationNP.s = IPS_OK;
     IDSetNumber(&LocationNP, NULL);
     return;
 }
 
 if (getSiteLatitude(PortFD, &dd, &mm) < 0)
     IDMessage(getDeviceName(), "Failed to get site latitude from device.");
  else
  {
    if (dd > 0)
        LocationNP.np[0].value = dd + mm/60.0;
    else
        LocationNP.np[0].value = dd - mm/60.0;
  
    if (isDebug())
    {
      IDLog("Autostar Latitude: %d:%d\n", dd, mm);
      IDLog("INDI Latitude: %g\n", LocationNP.np[0].value);
    }
  }
  
  if (getSiteLongitude(PortFD, &dd, &mm) < 0)
    IDMessage(getDeviceName(), "Failed to get site longitude from device.");
  else
  {
    if (dd > 0) LocationNP.np[1].value = 360.0 - (dd + mm/60.0);
    else LocationNP.np[1].value = (dd - mm/60.0) * -1.0;
    
    if (isDebug())
    {
        IDLog("Autostar Longitude: %d:%d\n", dd, mm);
        IDLog("INDI Longitude: %g\n", LocationNP.np[1].value);
    }
  }
  
  IDSetNumber (&LocationNP, NULL);
}

bool LX200Generic::GuideNorth(float ms)
{
      int use_pulse_cmd;

      use_pulse_cmd = IUFindOnSwitchIndex(&UsePulseCmdSP);

      if (!use_pulse_cmd && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
      {
        IDMessage(getDeviceName(), "Cannot guide while moving.");
        return false;
      }

      // If already moving (no pulse command), then stop movement
      if (MovementNSSP.s == IPS_BUSY)
      {
          int dir = IUFindOnSwitchIndex(&MovementNSSP);

          MoveNS(dir == 0 ? MOTION_NORTH : MOTION_SOUTH);

      }

      if (GuideNSTID)
      {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
      }

      if (use_pulse_cmd)
      {
        SendPulseCmd(PortFD, LX200_NORTH, ms);
      } else
      {
        if (setSlewMode(PortFD, LX200_SLEW_GUIDE) < 0)
        {
            SlewModeSP.s = IPS_ALERT;
            IDSetSwitch(&SlewModeSP, "Error setting slew mode.");
            return false;
        }

        MovementNSS[0].s = ISS_ON;
        MoveNS(MOTION_NORTH);
      }

      // Set slew to guiding
      IUResetSwitch(&SlewModeSP);
      SlewModeS[3].s = ISS_ON;
      IDSetSwitch(&SlewModeSP, NULL);
      guide_direction = LX200_NORTH;
      GuideNSTID = IEAddTimer (ms, guideTimeoutHelper, this);
      return true;
}

bool LX200Generic::GuideSouth(float ms)
{
    int use_pulse_cmd;

    use_pulse_cmd = IUFindOnSwitchIndex(&UsePulseCmdSP);

    if (!use_pulse_cmd && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
      IDMessage(getDeviceName(), "Cannot guide while moving.");
      return false;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementNSSP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementNSSP);

        MoveNS(dir == 0 ? MOTION_NORTH : MOTION_SOUTH);

    }

    if (GuideNSTID)
    {
      IERmTimer(GuideNSTID);
      GuideNSTID = 0;
    }

    if (use_pulse_cmd)
    {
      SendPulseCmd(PortFD, LX200_SOUTH, ms);
    } else
    {
      if (setSlewMode(PortFD, LX200_SLEW_GUIDE) < 0)
      {
          SlewModeSP.s = IPS_ALERT;
          IDSetSwitch(&SlewModeSP, "Error setting slew mode.");
          return false;
      }

      MovementNSS[1].s = ISS_ON;
      MoveNS(MOTION_SOUTH);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewModeSP);
    SlewModeS[3].s = ISS_ON;
    IDSetSwitch(&SlewModeSP, NULL);
    guide_direction = LX200_SOUTH;
    GuideNSTID = IEAddTimer (ms, guideTimeoutHelper, this);
    return true;

}

bool LX200Generic::GuideEast(float ms)
{

    int use_pulse_cmd;

    use_pulse_cmd = IUFindOnSwitchIndex(&UsePulseCmdSP);

    if (!use_pulse_cmd && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
      IDMessage(getDeviceName(), "Cannot guide while moving.");
      return false;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementWESP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementWESP);

        MoveWE(dir == 0 ? MOTION_WEST : MOTION_EAST);

    }

    if (GuideWETID)
    {
      IERmTimer(GuideWETID);
      GuideWETID = 0;
    }

    if (use_pulse_cmd)
    {
      SendPulseCmd(PortFD, LX200_EAST, ms);
    } else
    {
      if (setSlewMode(PortFD, LX200_SLEW_GUIDE) < 0)
      {
          SlewModeSP.s = IPS_ALERT;
          IDSetSwitch(&SlewModeSP, "Error setting slew mode.");
          return false;
      }

      MovementWES[1].s = ISS_ON;
      MoveWE(MOTION_EAST);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewModeSP);
    SlewModeS[3].s = ISS_ON;
    IDSetSwitch(&SlewModeSP, NULL);
    guide_direction = LX200_EAST;
    GuideWETID = IEAddTimer (ms, guideTimeoutHelper, this);
    return true;

}

bool LX200Generic::GuideWest(float ms)
{

    int use_pulse_cmd;

    use_pulse_cmd = IUFindOnSwitchIndex(&UsePulseCmdSP);

    if (!use_pulse_cmd && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
      IDMessage(getDeviceName(), "Cannot guide while moving.");
      return false;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementWESP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementWESP);

        MoveWE(dir == 0 ? MOTION_WEST : MOTION_EAST);

    }

    if (GuideWETID)
    {
      IERmTimer(GuideWETID);
      GuideWETID = 0;
    }

    if (use_pulse_cmd)
    {
      SendPulseCmd(PortFD, LX200_WEST, ms);
    } else
    {
      if (setSlewMode(PortFD, LX200_SLEW_GUIDE) < 0)
      {
          SlewModeSP.s = IPS_ALERT;
          IDSetSwitch(&SlewModeSP, "Error setting slew mode.");
          return false;
      }

      MovementWES[0].s = ISS_ON;
      MoveWE(MOTION_WEST);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewModeSP);
    SlewModeS[3].s = ISS_ON;
    IDSetSwitch(&SlewModeSP, NULL);
    guide_direction = LX200_WEST;
    GuideWETID = IEAddTimer (ms, guideTimeoutHelper, this);
    return true;

}

void LX200Generic::guideTimeoutHelper(void *p)
{
    ((LX200Generic *)p)->guideTimeout();
}

void LX200Generic::guideTimeout()
{
    int use_pulse_cmd;

    use_pulse_cmd = IUFindOnSwitchIndex(&UsePulseCmdSP);
    if (guide_direction == -1)
    {
    HaltMovement(PortFD, LX200_NORTH);
    HaltMovement(PortFD, LX200_SOUTH);
    HaltMovement(PortFD, LX200_EAST);
    HaltMovement(PortFD, LX200_WEST);

    MovementNSSP.s = IPS_IDLE;
    MovementWESP.s = IPS_IDLE;
    IUResetSwitch(&MovementNSSP);
    IUResetSwitch(&MovementWESP);
    IDSetSwitch(&MovementNSSP, NULL);
    IDSetSwitch(&MovementWESP, NULL);
    IERmTimer(GuideNSTID);
    IERmTimer(GuideWETID);

    }
    else if (! use_pulse_cmd)
    {

        if (guide_direction == LX200_NORTH || guide_direction == LX200_SOUTH)
        {
            MoveNS(guide_direction == LX200_NORTH ? MOTION_NORTH : MOTION_SOUTH);

            if (guide_direction == LX200_NORTH)
                GuideNSNP.np[0].value = 0;
            else
                GuideNSNP.np[1].value = 0;

            GuideNSNP.s = IPS_IDLE;
            IDSetNumber(&GuideNSNP, NULL);
            MovementNSSP.s = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IDSetSwitch(&MovementNSSP, NULL);
        }
        if (guide_direction == LX200_WEST || guide_direction == LX200_EAST)
        {
            MoveWE(guide_direction == LX200_WEST ? MOTION_WEST : MOTION_EAST);
            if (guide_direction == LX200_WEST)
                GuideWENP.np[0].value = 0;
            else
                GuideWENP.np[1].value = 0;

            GuideWENP.s = IPS_IDLE;
            IDSetNumber(&GuideWENP, NULL);
            MovementWESP.s = IPS_IDLE;
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementWESP, NULL);
        }
    }
    if (guide_direction == LX200_NORTH || guide_direction == LX200_SOUTH || guide_direction == -1)
    {
        GuideNSNP.np[0].value = 0;
        GuideNSNP.np[1].value = 0;
        GuideNSNP.s = IPS_IDLE;
        GuideNSTID = 0;
        IDSetNumber(&GuideNSNP, NULL);
    }
    if (guide_direction == LX200_WEST || guide_direction == LX200_EAST || guide_direction == -1)
    {
        GuideWENP.np[0].value = 0;
        GuideWENP.np[1].value = 0;
        GuideWENP.s = IPS_IDLE;
        GuideWETID = 0;
        IDSetNumber(&GuideWENP, NULL);
    }
}

bool LX200Generic::canSync()
{
    return true;
}

// Don't assume generic can, let children decide
bool LX200Generic::canPark()
{
    return false;
}

bool LX200Generic::ISSnoopDevice(XMLEle *root)
{

    controller->ISSnoopDevice(root);

    return INDI::Telescope::ISSnoopDevice(root);

}

void LX200Generic::processButton(const char * button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    // Max Slew speed
    if (!strcmp(button_n, "Slew Max"))
    {
        setSlewMode(PortFD, 0);
        IUResetSwitch(&SlewModeSP);
        SlewModeS[0].s = ISS_ON;
        IDSetSwitch(&SlewModeSP, NULL);
    }
    // Find Slew speed
    else if (!strcmp(button_n, "Slew Find"))
    {
            setSlewMode(PortFD, 1);
            IUResetSwitch(&SlewModeSP);
            SlewModeS[1].s = ISS_ON;
            IDSetSwitch(&SlewModeSP, NULL);
    }
    // Centering Slew
    else if (!strcmp(button_n, "Slew Centering"))
    {
            setSlewMode(PortFD, 2);
            IUResetSwitch(&SlewModeSP);
            SlewModeS[2].s = ISS_ON;
            IDSetSwitch(&SlewModeSP, NULL);
    }
    // Guide Slew
    else if (!strcmp(button_n, "Slew Guide"))
    {
            setSlewMode(PortFD, 3);
            IUResetSwitch(&SlewModeSP);
            SlewModeS[3].s = ISS_ON;
            IDSetSwitch(&SlewModeSP, NULL);
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

void LX200Generic::processNSWE(double mag, double angle)
{
    if (mag < 0.5)
    {
        // Moving in the same direction will make it stop
        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
            Abort();
    }
    // Put high threshold
    else if (mag > 0.9)
    {
        // North
        if (angle > 0 && angle < 180)
        {
            // Don't try to move if you're busy and moving in the same direction
            if (MovementNSSP.s != IPS_BUSY || MovementNSS[0].s != ISS_ON)
                MoveNS(MOTION_NORTH);

            MovementNSSP.s = IPS_BUSY;
            MovementNSSP.sp[0].s = ISS_ON;
            MovementNSSP.sp[1].s = ISS_OFF;
            IDSetSwitch(&MovementNSSP, NULL);
        }
        // South
        if (angle > 180 && angle < 360)
        {
            // Don't try to move if you're busy and moving in the same direction
           if (MovementNSSP.s != IPS_BUSY  || MovementNSS[1].s != ISS_ON)
            MoveNS(MOTION_SOUTH);

            MovementNSSP.s = IPS_BUSY;
            MovementNSSP.sp[0].s = ISS_OFF;
            MovementNSSP.sp[1].s = ISS_ON;
            IDSetSwitch(&MovementNSSP, NULL);
        }
        // East
        if (angle < 90 || angle > 270)
        {
            // Don't try to move if you're busy and moving in the same direction
           if (MovementWESP.s != IPS_BUSY  || MovementWES[1].s != ISS_ON)
                MoveWE(MOTION_EAST);

           MovementWESP.s = IPS_BUSY;
           MovementWESP.sp[0].s = ISS_OFF;
           MovementWESP.sp[1].s = ISS_ON;
           IDSetSwitch(&MovementWESP, NULL);
        }

        // West
        if (angle > 90 && angle < 270)
        {

            // Don't try to move if you're busy and moving in the same direction
           if (MovementWESP.s != IPS_BUSY  || MovementWES[0].s != ISS_ON)
                MoveWE(MOTION_WEST);

           MovementWESP.s = IPS_BUSY;
           MovementWESP.sp[0].s = ISS_ON;
           MovementWESP.sp[1].s = ISS_OFF;
           IDSetSwitch(&MovementWESP, NULL);
        }
    }

}

bool LX200Generic::saveConfigItems(FILE *fp)
{    
    controller->saveConfigItems(fp);

    INDI::Telescope::saveConfigItems(fp);

    return true;
}

void LX200Generic::processJoystick(const char * joystick_n, double mag, double angle)
{
    if (!strcmp(joystick_n, "NSWE Control"))
        processNSWE(mag, angle);
}


void LX200Generic::joystickHelper(const char * joystick_n, double mag, double angle)
{
    telescope->processJoystick(joystick_n, mag, angle);

}

void LX200Generic::buttonHelper(const char * button_n, ISState state)
{
    telescope->processButton(button_n, state);
}

