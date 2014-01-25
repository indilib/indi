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
#include <sys/time.h>

#include <memory>

#include "celestronprotocol.h"
#include "celestrongps.h"

/* Simulation Parameters */
#define	SLEWRATE	1		/* slew rate, degrees/s */
#define SIDRATE		0.004178	/* sidereal rate, degrees/s */

std::auto_ptr<CelestronGPS> telescope(0);

/* send client definitions of all properties */
void ISInit()
{
  static int isInit=0;

 if (isInit)
  return;

 isInit = 1;

 if(telescope.get() == 0) telescope.reset(new CelestronGPS());

}

void ISGetProperties (const char *dev)
{ ISInit(); telescope->ISGetProperties(dev);}
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{ ISInit(); telescope->ISNewSwitch(dev, name, states, names, n);}
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{ ISInit(); telescope->ISNewText(dev, name, texts, names, n);}
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{ ISInit(); telescope->ISNewNumber(dev, name, values, names, n);}
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

CelestronGPS::CelestronGPS()
{

   setVersion(2, 0);

   lastRA = 0;
   lastDEC = 0;
   currentSet   = 0;
   lastSet      = -1;

   controller = new INDI::Controller(this);

   controller->setJoystickCallback(joystickHelper);
   controller->setButtonCallback(buttonHelper);

   IDLog("initializing from Celeston GPS device...\n");

}

const char * CelestronGPS::getDefaultName()
{
    return ( (const char *) "Celestron GPS");
}

bool CelestronGPS::initProperties()
{
    INDI::Telescope::initProperties();

    IUFillSwitch(&SlewModeS[0], "Max", "", ISS_ON);
    IUFillSwitch(&SlewModeS[1], "Find", "", ISS_OFF);
    IUFillSwitch(&SlewModeS[2], "Centering", "", ISS_OFF);
    IUFillSwitch(&SlewModeS[3], "Guide", "", ISS_OFF);
    IUFillSwitchVector(&SlewModeSP, SlewModeS, 4, getDeviceName(), "Slew Rate", "", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    controller->mapController("NSWE Control","NSWE Control", INDI::Controller::CONTROLLER_JOYSTICK, "JOYSTICK_1");
    controller->mapController("Slew Max", "Slew Max", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->mapController("Slew Find","Slew Find", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_2");
    controller->mapController("Slew Centering", "Slew Centering", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_3");
    controller->mapController("Slew Guide", "Slew Guide", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_4");
    controller->mapController("Abort Motion", "Abort Motion", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_5");

    controller->initProperties();

    addAuxControls();

    return true;
}

void CelestronGPS::ISGetProperties(const char *dev)
{
    if(dev && strcmp(dev,getDeviceName()))
        return;

    INDI::Telescope::ISGetProperties(dev);

    if (isConnected())
        defineSwitch(&SlewModeSP);

    controller->ISGetProperties(dev);
}

bool CelestronGPS::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineSwitch(&SlewModeSP);
        loadConfig(true);
    }
    else
        deleteProperty(SlewModeSP.name);

    controller->updateProperties();

    return true;
}

bool CelestronGPS::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // Slew mode
        if (!strcmp (name, SlewModeSP.name))
        {
          int index=0;

          IUResetSwitch(&SlewModeSP);
          IUUpdateSwitch(&SlewModeSP, states, names, n);
          index = IUFindOnSwitchIndex(&SlewModeSP);
          if (isSimulation() == false)
            SetRate(index);

          SlewModeSP.s = IPS_OK;
          IDSetSwitch(&SlewModeSP, NULL);
          return true;
        }

    }

    controller->ISNewSwitch(dev, name, states, names, n);

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}


bool CelestronGPS::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    controller->ISNewText(dev, name, texts, names, n);

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool CelestronGPS::Goto(double ra, double dec)
{
    int i=0;
    char RAStr[32], DecStr[32];

    targetRA  = ra;
    targetDEC = dec;

    if (EqNP.s == IPS_BUSY)
    {
        if (isSimulation() == false)
            StopNSEW();
       // sleep for 500 mseconds
       usleep(500000);
    }

    if (isSimulation() == false && (i = SlewToCoords(targetRA, targetDEC)))
    {
      slewError(i);
      return false;
    }

    TrackState = SCOPE_SLEWING;
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);
    IDMessage(getDeviceName(), "Slewing to JNOW RA %s - DEC %s", RAStr, DecStr);

    if (isDebug())
        IDLog("Slewing to JNOW RA %s - DEC %s", RAStr, DecStr);

    return true;


}

bool CelestronGPS::Sync(double targetRA, double targetDEC)
{
    if (isSimulation())
    {
        currentRA  = targetRA;
        currentDEC = targetDEC;
    }
    else if (SyncToCoords(targetRA, targetDEC))
    {
        IDMessage(getDeviceName(), "Sync failed.");
        return false;
    }

    IDMessage(getDeviceName(), "Synchronization successful.");
    return true;
}

bool CelestronGPS::MoveNS(TelescopeMotionNS dir)
{
    static int last_move=-1;

     int current_move = -1;

    current_move = IUFindOnSwitchIndex(&MovementNSSP);

    // Previosuly active switch clicked again, so let's stop.
    if (current_move == last_move && current_move != -1)
    {
        if (isSimulation() == false)
            StopSlew((current_move == 0) ? NORTH : SOUTH);

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
    current_move = (dir == MOTION_NORTH) ? NORTH : SOUTH;

    if (isSimulation() == false)
        StartSlew(current_move);

      MovementNSSP.s = IPS_BUSY;
      IDSetSwitch(&MovementNSSP, "Moving toward %s.", (current_move == NORTH) ? "North" : "South");
      return true;
}

bool CelestronGPS::MoveWE(TelescopeMotionWE dir)
{
   static int last_move=-1;

   int current_move = -1;

   current_move = IUFindOnSwitchIndex(&MovementWESP);

  // Previosuly active switch clicked again, so let's stop.
  if (current_move == last_move && current_move != -1)
  {
      if (isSimulation() == false)
        StopSlew((current_move == 0) ? WEST : EAST);
      IUResetSwitch(&MovementWESP);
      MovementWESP.s = IPS_IDLE;
      IDSetSwitch(&MovementWESP, "Movement toward %s halted.", (current_move == 0) ? "West" : "East");
      last_move = -1;
      return true;
  }

  last_move = current_move;

  if (isDebug())
      IDLog("Current Move: %d - Previous Move: %d\n", current_move, last_move);

  current_move = (dir == MOTION_WEST) ? WEST : EAST;

  if (isSimulation() == false)
      StartSlew(current_move);


    MovementWESP.s = IPS_BUSY;
    IDSetSwitch(&MovementWESP, "Moving toward %s.", (current_move == WEST) ? "West" : "East");
    return true;
}

bool CelestronGPS::ReadScopeStatus()
{
    if (isSimulation())
    {
        mountSim();
        return true;
    }

    if (CheckConnectTel() == -1)
    {
        IDMessage(getDeviceName(), "Problem communication with the mount.");
        return false;
    }

    switch (TrackState)
    {
        case SCOPE_SLEWING:
        // are we done?
        if (isScopeSlewing() == 0)
        {
            IDMessage(getDeviceName(), "Slew complete, tracking...");
            TrackState = SCOPE_TRACKING;
        }
        break;

    default:
        break;
    }


    currentRA = GetRA();
    currentDEC = GetDec();

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool CelestronGPS::Abort()
{
    if (isSimulation() == false)
    {
        StopNSEW();
        StopSlew(NORTH);
        StopSlew(SOUTH);
        StopSlew(WEST);
        StopSlew(EAST);
    }

    TrackState = SCOPE_IDLE;

    if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY || EqNP.s == IPS_BUSY)
    {
        MovementNSSP.s = MovementWESP.s = EqNP.s = IPS_IDLE;
        IUResetSwitch(&MovementNSSP);
        IUResetSwitch(&MovementWESP);

        IDSetSwitch(&MovementNSSP, NULL);
        IDSetSwitch(&MovementWESP, NULL);
        IDSetNumber(&EqNP, NULL);

        IDMessage(getDeviceName(), "Slew stopped.");
    }

    return true;
}

bool CelestronGPS::Connect()
{
    bool rc=false;

    if(isConnected()) return true;

    rc=Connect(PortT[0].text);

    if(rc)
        SetTimer(POLLMS);

    return rc;
}

bool CelestronGPS::Connect(char *port)
{
    if (isSimulation())
    {
        IDMessage (getDeviceName(), "Simulated Celestron GPS is online. Retrieving basic data...");
        currentRA = 0; currentDEC=90;
        return true;
    }

    if (ConnectTel(port) < 0)
    {
      IDMessage(getDeviceName(), "Error connecting to port %s. Make sure you have BOTH write and read permission to your port.", port);
      return false;
    }

   IDMessage (getDeviceName(), "Telescope is online.");

   return true;
}

bool CelestronGPS::Disconnect()
{
    if (isSimulation() == false)
        DisconnectTel();

    return true;
}

void CelestronGPS::mountSim ()
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
        {
            IDMessage(getDeviceName(), "Simulated telescope slew is complete, tracking...");
             TrackState = SCOPE_TRACKING;
        }


        break;

    default:
        break;
    }

    NewRaDec(currentRA, currentDEC);


}

void CelestronGPS::slewError(int slewCode)
{

    switch (slewCode)
    {
      case 1:
       IDMessage (getDeviceName(), "Invalid newDec in SlewToCoords");
       break;
      case 2:
       IDMessage (getDeviceName(), "RA count overflow in SlewToCoords");
       break;
      case 3:
       IDMessage (getDeviceName(), "Dec count overflow in SlewToCoords");
       break;
      case 4:
       IDMessage (getDeviceName(), "No acknowledgment from telescope after SlewToCoords");
       break;
      default:
       IDMessage (getDeviceName(), "Unknown error");
       break;
    }

}

bool CelestronGPS::canSync()
{
    return true;
}

bool CelestronGPS::canPark()
{
    return false;
}

bool CelestronGPS::ISSnoopDevice(XMLEle *root)
{
    controller->ISSnoopDevice(root);

    return INDI::Telescope::ISSnoopDevice(root);
}

void CelestronGPS::processButton(const char *button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    // Max Slew speed
    if (!strcmp(button_n, "Slew Max"))
    {
        SetRate(0);
        IUResetSwitch(&SlewModeSP);
        SlewModeS[0].s = ISS_ON;
        IDSetSwitch(&SlewModeSP, NULL);
    }
    // Find Slew speed
    else if (!strcmp(button_n, "Slew Find"))
    {
            SetRate(1);
            IUResetSwitch(&SlewModeSP);
            SlewModeS[1].s = ISS_ON;
            IDSetSwitch(&SlewModeSP, NULL);
    }
    // Centering Slew
    else if (!strcmp(button_n, "Slew Centering"))
    {
            SetRate(2);
            IUResetSwitch(&SlewModeSP);
            SlewModeS[2].s = ISS_ON;
            IDSetSwitch(&SlewModeSP, NULL);
    }
    // Guide Slew
    else if (!strcmp(button_n, "Slew Guide"))
    {
            SetRate(3);
            IUResetSwitch(&SlewModeSP);
            SlewModeS[3].s = ISS_ON;
            IDSetSwitch(&SlewModeSP, NULL);
    }
    // Abort
    else if (!strcmp(button_n, "Abort Motion"))
    {
        // Only abort if we have some sort of motion going on
        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY || EqNP.s == IPS_BUSY)
        {

            Abort();
        }
    }
}

void CelestronGPS::processNSWE(double mag, double angle)
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

bool CelestronGPS::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    controller->saveConfigItems(fp);

    return true;
}

bool CelestronGPS::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    return (::updateLocation(longitude, latitude) == 0);
}

bool CelestronGPS::updateTime(ln_date *utc, double utc_offset)
{
    if (isSimulation())
        return true;

    return (::updateTime(utc, utc_offset) == 0);
}

void CelestronGPS::processJoystick(const char * joystick_n, double mag, double angle)
{
    if (!strcmp(joystick_n, "NSWE Control"))
        processNSWE(mag, angle);
}


void CelestronGPS::joystickHelper(const char * joystick_n, double mag, double angle)
{
    telescope->processJoystick(joystick_n, mag, angle);

}

void CelestronGPS::buttonHelper(const char * button_n, ISState state)
{
    telescope->processButton(button_n, state);
}

