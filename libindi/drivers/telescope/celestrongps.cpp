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

#include "celestrongps.h"

/* Simulation Parameters */
#define	SLEWRATE	1		/* slew rate, degrees/s */
#define SIDRATE		0.004178	/* sidereal rate, degrees/s */

#define MOUNTINFO_TAB   "Mount Info"

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

   setVersion(3, 0);

   PortFD = -1;
   fwInfo.Version = "Invalid";

   INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

   controller = new INDI::Controller(this);
   controller->setJoystickCallback(joystickHelper);
   controller->setButtonCallback(buttonHelper);   

   TelescopeCapability cap;

   currentRA    = 0;
   currentDEC   = 90;

   cap.canPark  = true;
   cap.canSync  = true;
   cap.canAbort = true;
   SetTelescopeCapability(&cap);

}

const char * CelestronGPS::getDefaultName()
{
    return ( (const char *) "Celestron GPS");
}

bool CelestronGPS::initProperties()
{
    INDI::Telescope::initProperties();

    IUFillSwitch(&SlewRateS[SR_1], "SLEW_GUIDE", "1x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_2], "2x", "2x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_3], "SLEW_CENTERING", "3x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_4], "4x", "4x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_5], "SLEW_FIND", "5x", ISS_ON);
    IUFillSwitch(&SlewRateS[SR_6], "6x", "6x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_7], "7x", "7x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_8], "8x", "8x", ISS_OFF);
    IUFillSwitch(&SlewRateS[SR_9], "SLEW_MAX", "9x", ISS_OFF);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 9, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Firmware */
    IUFillText(&FirmwareT[FW_MODEL], "Model", "", 0);
    IUFillText(&FirmwareT[FW_VERSION], "Version", "", 0);
    IUFillText(&FirmwareT[FW_GPS], "GPS", "", 0);
    IUFillText(&FirmwareT[FW_RA], "RA", "", 0);
    IUFillText(&FirmwareT[FW_DEC], "DEC", "", 0);
    IUFillTextVector(&FirmwareTP, FirmwareT, 5, getDeviceName(), "Firmware Info", "", MOUNTINFO_TAB, IP_RO, 0, IPS_IDLE);

    controller->mapController("MOTIONDIR", "N/S/W/E Control", INDI::Controller::CONTROLLER_JOYSTICK, "JOYSTICK_1");
    controller->mapController("SLEWPRESET", "Slew Rate", INDI::Controller::CONTROLLER_JOYSTICK, "JOYSTICK_2");
    controller->mapController("ABORTBUTTON", "Abort", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->initProperties();

    controller->initProperties();

    SetParkDataType(PARK_RA_DEC);

    addAuxControls();

    return true;
}

void CelestronGPS::ISGetProperties(const char *dev)
{
    if(dev && strcmp(dev,getDeviceName()))
        return;

    INDI::Telescope::ISGetProperties(dev);

    if (isConnected())
    {
        defineSwitch(&SlewRateSP);
        if (fwInfo.Version != "Invalid")
            defineText(&FirmwareTP);
    }

    controller->ISGetProperties(dev);
}

bool CelestronGPS::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineSwitch(&SlewRateSP);

        if (get_celestron_firmware(PortFD, &fwInfo))
        {
            IUSaveText(&FirmwareT[FW_MODEL], fwInfo.Model.c_str());
            IUSaveText(&FirmwareT[FW_VERSION], fwInfo.Version.c_str());
            IUSaveText(&FirmwareT[FW_GPS], fwInfo.GPSFirmware.c_str());
            IUSaveText(&FirmwareT[FW_RA], fwInfo.RAFirmware.c_str());
            IUSaveText(&FirmwareT[FW_DEC], fwInfo.DEFirmware.c_str());
            defineText(&FirmwareTP);
        }
        else
        {
            fwInfo.Version = "Invalid";
            DEBUG(INDI::Logger::DBG_WARNING, "Failed to retrive firmware information.");
        }

        double HA = ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
        double DEC = (LocationN[LOCATION_LATITUDE].value > 0) ? 90 : -90;

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetRAParkDefault(HA);
            SetDEParkDefault(DEC);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetRAPark(HA);
            SetDEPark(DEC);
            SetRAParkDefault(HA);
            SetDEParkDefault(DEC);
        }
    }
    else
    {
        deleteProperty(SlewRateSP.name);
        if (fwInfo.Version != "Invalid")
            deleteProperty(FirmwareTP.name);
    }

    if (fwInfo.controllerVersion >= 2.3)
    {
        double utc_offset;
        int yy, dd, mm, hh, minute, ss;
        if (get_celestron_utc_date_time(PortFD, &utc_offset, &yy, &mm, &dd, &hh, &minute, &ss))
        {
            char isoDateTime[32];
            char utcOffset[8];

            snprintf(isoDateTime, 32, "%04d-%02d-%02dT%02d:%02d:%02d", yy, mm, dd, hh, minute, ss);
            snprintf(utcOffset, 8, "%4.2f", utc_offset);

            IUSaveText(IUFindText(&TimeTP, "UTC"), isoDateTime);
            IUSaveText(IUFindText(&TimeTP, "OFFSET"), utcOffset);

            DEBUGF(INDI::Logger::DBG_SESSION, "Mount UTC offset is %s. UTC time is %s", utcOffset, isoDateTime);

            IDSetText(&TimeTP, NULL);
        }
    }

    controller->updateProperties();

    return true;
}

bool CelestronGPS::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev,getDeviceName()))
    {
        // Slew mode
        if (!strcmp (name, SlewRateSP.name))
        {         
          IUUpdateSwitch(&SlewRateSP, states, names, n);
          SlewRateSP.s = IPS_OK;
          IDSetSwitch(&SlewRateSP, NULL);
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
    char RAStr[32], DecStr[32];

    targetRA  = ra;
    targetDEC = dec;

    if (EqNP.s == IPS_BUSY || MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
    {
        abort_celestron(PortFD);
       // sleep for 500 mseconds
       usleep(500000);
    }

    if (slew_celestron(PortFD, targetRA, targetDEC) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to slew telescope.");
        return false;
    }

    set_sim_slewing(true);

    TrackState = SCOPE_SLEWING;
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);
    DEBUGF(INDI::Logger::DBG_SESSION, "Slewing to JNOW RA %s - DEC %s", RAStr, DecStr);

    return true;

}

bool CelestronGPS::Sync(double ra, double dec)
{    
    if (fwInfo.controllerVersion <= 4.1)
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Firmwre version 4.1 or higher is required to sync. Current version is %3.1f", fwInfo.controllerVersion);
        return false;
    }

    if (sync_celestron(PortFD, ra, dec) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Sync failed.");
        return false;
    }

    currentRA  = ra;
    currentDEC = dec;

    DEBUG(INDI::Logger::DBG_SESSION, "Sync successful.");

    return true;
}

bool CelestronGPS::MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand command)
{
    CELESTRON_DIRECTION current_move = (dir == MOTION_NORTH) ? CELESTRON_N : CELESTRON_S;
    CELESTRON_SLEW_RATE rate         = (CELESTRON_SLEW_RATE) IUFindOnSwitchIndex(&SlewRateSP);

    switch (command)
    {
        case MOTION_START:
        if (start_celestron_motion(PortFD, current_move, rate) == false)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error setting N/S motion direction.");
            return false;
        }
        else
           DEBUGF(INDI::Logger::DBG_SESSION,"Moving toward %s.", (current_move == CELESTRON_N) ? "North" : "South");
        break;

        case MOTION_STOP:
        if (stop_celestron_motion(PortFD, current_move) == false)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error stopping N/S motion.");
            return false;
        }
        else
            DEBUGF(INDI::Logger::DBG_SESSION, "Movement toward %s halted.", (current_move == CELESTRON_N) ? "North" : "South");
        break;
    }

    return true;

}

bool CelestronGPS::MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand command)
{
    CELESTRON_DIRECTION current_move = (dir == MOTION_WEST) ? CELESTRON_W : CELESTRON_E;
    CELESTRON_SLEW_RATE rate         = (CELESTRON_SLEW_RATE) IUFindOnSwitchIndex(&SlewRateSP);

    switch (command)
    {
        case MOTION_START:
        if (start_celestron_motion(PortFD, current_move, rate) == false)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error setting W/E motion direction.");
            return false;
        }
        else
           DEBUGF(INDI::Logger::DBG_SESSION,"Moving toward %s.", (current_move == CELESTRON_W) ? "West" : "East");
        break;

        case MOTION_STOP:
        if (stop_celestron_motion(PortFD, current_move) == false)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Error stopping W/E motion.");
            return false;
        }
        else
            DEBUGF(INDI::Logger::DBG_SESSION, "Movement toward %s halted.", (current_move == CELESTRON_W) ? "West" : "East");
        break;
    }

    return true;
}

bool CelestronGPS::ReadScopeStatus()
{
    if (isSimulation())
        mountSim();        

    if (get_celestron_coords(PortFD, &currentRA, &currentDEC) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to read RA/DEC values.");
        return false;
    }

    switch (TrackState)
    {
        case SCOPE_SLEWING:
        // are we done?
        if (is_scope_slewing(PortFD) == false)
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Slew complete, tracking...");
            TrackState = SCOPE_TRACKING;
        }
        break;

       case SCOPE_PARKING:
        // are we done?
        if (is_scope_slewing(PortFD) == false)
            SetParked(true);
        break;

    default:
        break;
    }

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool CelestronGPS::Abort()
{
    stop_celestron_motion(PortFD, CELESTRON_N);
    stop_celestron_motion(PortFD, CELESTRON_S);
    stop_celestron_motion(PortFD, CELESTRON_W);
    stop_celestron_motion(PortFD, CELESTRON_E);
    return abort_celestron(PortFD);
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
        set_celestron_device(getDeviceName());
        set_celestron_simulation(true);
        set_sim_slew_rate(SR_5);
        set_sim_ra(0);
        set_sim_dec(90);
    }
    else if (tty_connect(port, 9600, 8, 0, 1, &PortFD) != TTY_OK)
    {
      DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to port %s. Make sure you have BOTH write and read permission to the port.", port);
      return false;
    }

    if (check_celestron_connection(PortFD) == false)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect to telescope port %s. Ensure telescope is powered and connected.", port);
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Telescope is online.");

    return true;

}

bool CelestronGPS::Disconnect()
{
    if (PortFD > 0)
        tty_disconnect(PortFD);

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
    case SCOPE_PARKING:
        /* slewing - nail it when both within one pulse @ SLEWRATE */
        nlocked = 0;

        dx = targetRA - currentRA;

        // Take shortest path
        if (fabs(dx) > 12)
            dx *= -1;

        if (fabs(dx) <= da)
        {
        currentRA = targetRA;
        nlocked++;
        }
        else if (dx > 0)
            currentRA += da/15.;
        else
            currentRA -= da/15.;

        if (currentRA < 0)
            currentRA += 24;
        else if (currentRA > 24)
            currentRA -= 24;

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
            set_sim_slewing(false);
        }

        break;

    default:
        break;
    }

    set_sim_ra(currentRA);
    set_sim_dec(currentDEC);
}

void CelestronGPS::simulationTriggered(bool enable)
{
    set_celestron_simulation(enable);
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

    if (fwInfo.controllerVersion <= 2.3)
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Firmwre version 2.3 or higher is required to update location. Current version is %3.1f", fwInfo.controllerVersion);
        return false;
    }

    return (set_celestron_location(PortFD, longitude, latitude));
}

bool CelestronGPS::updateTime(ln_date *utc, double utc_offset)
{
    if (fwInfo.controllerVersion <= 2.3)
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Firmwre version 2.3 or higher is required to update time. Current version is %3.1f", fwInfo.controllerVersion);
        return false;
    }

    return (set_celestron_datetime(PortFD, utc, utc_offset));
}

bool CelestronGPS::Park()
{
    if (Goto(GetRAPark(), GetDEPark()))
    {
        TrackState = SCOPE_PARKING;
        DEBUG(INDI::Logger::DBG_SESSION, "Parking is in progress...");
        return true;
    }
    else
        return false;
}

bool CelestronGPS::UnPark()
{
    if (Sync(GetRAPark(), GetDEPark()))
    {
        SetParked(false);
        return true;
    }
    else
        return false;

}

void CelestronGPS::SetCurrentPark()
{
    SetRAPark(currentRA);
    SetDEPark(currentDEC);
}

void CelestronGPS::SetDefaultPark()
{
    // By default set RA to HA
    SetRAPark(ln_get_apparent_sidereal_time(ln_get_julian_from_sys()));

    // Set DEC to 90 or -90 depending on the hemisphere
    SetDEPark( (LocationN[LOCATION_LATITUDE].value > 0) ? 90 : -90);
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

    if (!strcmp(button_n, "Abort Motion"))
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
                MoveNS(MOTION_NORTH, MOTION_START);

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
            MoveNS(MOTION_SOUTH, MOTION_START);

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
                MoveWE(MOTION_EAST, MOTION_START);

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
                MoveWE(MOTION_WEST, MOTION_START);

           MovementWESP.s = IPS_BUSY;
           MovementWESP.sp[0].s = ISS_ON;
           MovementWESP.sp[1].s = ISS_OFF;
           IDSetSwitch(&MovementWESP, NULL);
        }
    }

}

void CelestronGPS::processSlewPresets(double mag, double angle)
{
    // high threshold, only 1 is accepted
    if (mag != 1)
        return;

    int currentIndex = IUFindOnSwitchIndex(&SlewRateSP);

    // Up
    if (angle > 0 && angle < 180)
    {
        if (currentIndex <= 0)
            return;

        IUResetSwitch(&SlewRateSP);
        SlewRateS[currentIndex-1].s = ISS_ON;
    }
    // Down
    else
    {
        if (currentIndex >= SlewRateSP.nsp-1)
            return;

         IUResetSwitch(&SlewRateSP);
         SlewRateS[currentIndex+1].s = ISS_ON;

    }
    IDSetSwitch(&SlewRateSP, NULL);

}

void CelestronGPS::processJoystick(const char * joystick_n, double mag, double angle)
{
    if (!strcmp(joystick_n, "MOTIONDIR"))
        processNSWE(mag, angle);
    else if (!strcmp(joystick_n, "SLEWPRESET"))
        processSlewPresets(mag, angle);
}


void CelestronGPS::joystickHelper(const char * joystick_n, double mag, double angle, void *context)
{
    static_cast<CelestronGPS *>(context)->processJoystick(joystick_n, mag, angle);
}

void CelestronGPS::buttonHelper(const char * button_n, ISState state, void *context)
{
    static_cast<CelestronGPS *>(context)->processButton(button_n, state);
}

