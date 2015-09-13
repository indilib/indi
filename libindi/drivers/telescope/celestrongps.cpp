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
#define	GOTO_RATE       5				/* slew rate, degrees/s */
#define	SLEW_RATE       0.5				/* slew rate, degrees/s */
#define FINE_SLEW_RATE  0.1             /* slew rate, degrees/s */
#define SID_RATE        0.004178		/* sidereal rate, degrees/s */
#define GOTO_LIMIT      5.5             /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT      1               /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */
#define FINE_SLEW_LIMIT 0.5             /* Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees */

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
   fwInfo.controllerVersion = 0;

   INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

   currentRA    = 0;
   currentDEC   = 90;
   currentAZ    = 0;
   currentALT   = 0;
   targetAZ     = 0;
   targetALT    = 0;

}

const char * CelestronGPS::getDefaultName()
{
    return ( (const char *) "Celestron GPS");
}

bool CelestronGPS::initProperties()
{
    INDI::Telescope::initProperties();

    /* Firmware */
    IUFillText(&FirmwareT[FW_MODEL], "Model", "", 0);
    IUFillText(&FirmwareT[FW_VERSION], "Version", "", 0);
    IUFillText(&FirmwareT[FW_GPS], "GPS", "", 0);
    IUFillText(&FirmwareT[FW_RA], "RA", "", 0);
    IUFillText(&FirmwareT[FW_DEC], "DEC", "", 0);
    IUFillTextVector(&FirmwareTP, FirmwareT, 5, getDeviceName(), "Firmware Info", "", MOUNTINFO_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&HorizontalCoordsN[AXIS_AZ], "AZ", "Az D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
    IUFillNumber(&HorizontalCoordsN[AXIS_ALT], "ALT", "Alt  D:M:S", "%10.6m", -90., 90.0, 0.0, 0);
    IUFillNumberVector(&HorizontalCoordsNP, HorizontalCoordsN, 2, getDeviceName(), "HORIZONTAL_COORD", "Horizontal Coord", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    SetParkDataType(PARK_AZ_ALT);

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
        defineNumber(&HorizontalCoordsNP);
        defineSwitch(&SlewRateSP);
        if (fwInfo.Version != "Invalid")
            defineText(&FirmwareTP);
    }    
}

bool CelestronGPS::updateProperties()
{
    TelescopeCapability cap;
    cap.canPark  = true;
    cap.canSync  = true;
    cap.canAbort = true;
    cap.hasTime  = true;
    cap.hasLocation = true;
    cap.nSlewRate= 9;


    if (get_celestron_firmware(PortFD, &fwInfo))
    {
        IUSaveText(&FirmwareT[FW_MODEL], fwInfo.Model.c_str());
        IUSaveText(&FirmwareT[FW_VERSION], fwInfo.Version.c_str());
        IUSaveText(&FirmwareT[FW_GPS], fwInfo.GPSFirmware.c_str());
        IUSaveText(&FirmwareT[FW_RA], fwInfo.RAFirmware.c_str());
        IUSaveText(&FirmwareT[FW_DEC], fwInfo.DEFirmware.c_str());
    }
    else
    {
        fwInfo.Version = "Invalid";
        DEBUG(INDI::Logger::DBG_WARNING, "Failed to retrive firmware information.");
    }

    if (fwInfo.controllerVersion <= 4.1)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Mount firmware does not support sync.");
        cap.canSync  = false;
    }

    if (fwInfo.controllerVersion < 2.3)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Mount firmware does not support update of time and location settings.");
        cap.hasTime = cap.hasLocation = false;
    }

    SetTelescopeCapability(&cap);

    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineNumber(&HorizontalCoordsNP);
        if (fwInfo.Version != "Invalid")
            defineText(&FirmwareTP);


        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(LocationN[LOCATION_LATITUDE].value);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(0);
            SetAxis2Park(LocationN[LOCATION_LATITUDE].value);
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(LocationN[LOCATION_LATITUDE].value);
        }
    }
    else
    {
        deleteProperty(HorizontalCoordsNP.name);
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

    return true;
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
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to slew telescope in RA/DEC.");
        return false;
    }

    HorizontalCoordsNP.s = IPS_BUSY;

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

bool CelestronGPS::GotoAzAlt(double az, double alt)
{
    if (isSimulation())
    {
        ln_hrz_posn horizontalPos;
        // Libnova south = 0, west = 90, north = 180, east = 270
        horizontalPos.az = az + 180;
        if (horizontalPos.az >= 360)
             horizontalPos.az -= 360;
        horizontalPos.alt = alt;

        ln_lnlat_posn observer;

        observer.lat = LocationN[LOCATION_LATITUDE].value;
        observer.lng = LocationN[LOCATION_LONGITUDE].value;

        if (observer.lng > 180)
            observer.lng -= 360;

        ln_equ_posn equatorialPos;
        ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

        targetRA  = equatorialPos.ra/15.0;
        targetDEC = equatorialPos.dec;
    }

    if (slew_celestron_azalt(PortFD, LocationN[LOCATION_LATITUDE].value, az, alt) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to slew telescope in Az/Alt.");
        return false;
    }

    targetAZ = az;
    targetALT= alt;

    TrackState = SCOPE_SLEWING;

    HorizontalCoordsNP.s = IPS_BUSY;

    char AZStr[16], ALTStr[16];
    fs_sexa(AZStr, targetAZ, 3, 3600);
    fs_sexa(ALTStr, targetALT, 2, 3600);
    DEBUGF(INDI::Logger::DBG_SESSION, "Slewing to Az %s - Alt %s", AZStr, ALTStr);

    return true;
}

bool CelestronGPS::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    CELESTRON_DIRECTION current_move = (dir == DIRECTION_NORTH) ? CELESTRON_N : CELESTRON_S;
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

bool CelestronGPS::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    CELESTRON_DIRECTION current_move = (dir == DIRECTION_WEST) ? CELESTRON_W : CELESTRON_E;
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

    if (get_celestron_coords_azalt(PortFD, LocationN[LOCATION_LATITUDE].value, &currentAZ, &currentALT) == false)
        DEBUG(INDI::Logger::DBG_WARNING, "Failed to read AZ/ALT values.");
    else
    {
        HorizontalCoordsN[AXIS_AZ].value  = currentAZ;
        HorizontalCoordsN[AXIS_ALT].value = currentALT;
    }

    switch (TrackState)
    {
        case SCOPE_SLEWING:
        // are we done?
        if (is_scope_slewing(PortFD) == false)
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Slew complete, tracking...");
            TrackState = SCOPE_TRACKING;
            HorizontalCoordsNP.s = IPS_OK;
        }
        break;

       case SCOPE_PARKING:
        // are we done?
        if (is_scope_slewing(PortFD) == false)
        {
            SetParked(true);
            HorizontalCoordsNP.s = IPS_OK;
        }
        break;

    default:
        break;
    }

    IDSetNumber(&HorizontalCoordsNP, NULL);
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

    rc=Connect(PortT[0].text, atoi(IUFindOnSwitch(&BaudRateSP)->name));

    if(rc)
        SetTimer(POLLMS);

    return rc;
}

bool CelestronGPS::Connect(const char *port, uint16_t baud)
{
    if (isSimulation())
    {
        set_celestron_device(getDeviceName());
        set_celestron_simulation(true);
        set_sim_slew_rate(SR_5);
        set_sim_ra(0);
        set_sim_dec(90);
    }
    else if (tty_connect(port, baud, 8, 0, 1, &PortFD) != TTY_OK)
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


bool CelestronGPS::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
  double newAlt=0, newAz=0;

  if(!strcmp(dev,getDeviceName()))
  {
      if ( !strcmp (name, HorizontalCoordsNP.name) )
      {
          int i=0, nset=0;

            for (nset = i = 0; i < n; i++)
            {
                INumber *horp = IUFindNumber (&HorizontalCoordsNP, names[i]);
                if (horp == &HorizontalCoordsN[AXIS_AZ])
                {
                    newAz = values[i];
                    nset += newAz >= 0. && newAz <= 360.0;

                } else if (horp == &HorizontalCoordsN[AXIS_ALT])
                {
                    newAlt = values[i];
                    nset += newAlt >= -90. && newAlt <= 90.0;
                }
            }

          if (nset == 2)
          {
              char AzStr[16], AltStr[16];
              fs_sexa(AzStr, newAz, 3, 3600);
              fs_sexa(AltStr, newAlt, 2, 3600);

           if (GotoAzAlt(newAz, newAlt) == false)
           {
               HorizontalCoordsNP.s = IPS_ALERT;
               DEBUGF(INDI::Logger::DBG_ERROR, "Error slewing to Az: %s Alt: %s", AzStr, AltStr);
               IDSetNumber(&HorizontalCoordsNP, NULL);
               return false;
           }

           return true;

          }
          else
          {
            HorizontalCoordsNP.s = IPS_ALERT;
            DEBUG(INDI::Logger::DBG_ERROR, "Altitude or Azimuth missing or invalid");
            IDSetNumber(&HorizontalCoordsNP, NULL);
            return false;
          }
      }
  }

    INDI::Telescope::ISNewNumber (dev, name, values, names, n);
    return true;
}

void CelestronGPS::mountSim ()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt,dx,da_ra=0, da_dec=0;
    int nlocked;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday (&tv, NULL);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
    ltv = tv;

    if ( fabs(targetRA - currentRA)*15. >= GOTO_LIMIT )
        da_ra = GOTO_RATE *dt;
    else if ( fabs(targetRA - currentRA)*15. >= SLEW_LIMIT )
        da_ra = SLEW_RATE *dt;
    else
        da_ra = FINE_SLEW_RATE *dt;

    if ( fabs(targetDEC - currentDEC) >= GOTO_LIMIT )
        da_dec = GOTO_RATE *dt;
    else if ( fabs(targetDEC - currentDEC) >= SLEW_LIMIT )
        da_dec = SLEW_RATE *dt;
    else
        da_dec = FINE_SLEW_RATE *dt;

    if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
    {
        int rate = IUFindOnSwitchIndex(&SlewRateSP);

        switch (rate)
        {
            case SLEW_GUIDE:
                da_ra  = FINE_SLEW_RATE *dt*0.05;
                da_dec = FINE_SLEW_RATE *dt*0.05;
                break;

            case SLEW_CENTERING:
                da_ra  = FINE_SLEW_RATE *dt*.1;
                da_dec = FINE_SLEW_RATE *dt*.1;
                break;

            case SLEW_FIND:
                da_ra  = SLEW_RATE *dt;
                da_dec = SLEW_RATE *dt;
                break;

            default:
                da_ra  = GOTO_RATE *dt;
                da_dec = GOTO_RATE *dt;
                break;
        }

        switch (MovementNSSP.s)
        {
           case IPS_BUSY:
            if (MovementNSS[DIRECTION_NORTH].s == ISS_ON)
                currentDEC += da_dec;
            else if (MovementNSS[DIRECTION_SOUTH].s == ISS_ON)
                currentDEC -= da_dec;

            break;
        }

        switch (MovementWESP.s)
        {
            case IPS_BUSY:

            if (MovementWES[DIRECTION_WEST].s == ISS_ON)
                currentRA += da_ra/15.;
            else if (MovementWES[DIRECTION_EAST].s == ISS_ON)
                currentRA -= da_ra/15.;

            break;
        }

        set_sim_ra(currentRA);
        set_sim_dec(currentDEC);

        ln_equ_posn equatorialPos;
        equatorialPos.ra  = currentRA * 15;
        equatorialPos.dec = currentDEC;

        ln_lnlat_posn observer;

        observer.lat = LocationN[LOCATION_LATITUDE].value;
        observer.lng = LocationN[LOCATION_LONGITUDE].value;

        if (observer.lng > 180)
            observer.lng -= 360;

        ln_hrz_posn horizontalPos;

        ln_get_hrz_from_equ(&equatorialPos, &observer, ln_get_julian_from_sys(), &horizontalPos);

        // Libnova south = 0, west = 90, north = 180, east = 270
        horizontalPos.az -= 180;
        if (horizontalPos.az < 0)
            horizontalPos.az += 360;

        set_sim_az(horizontalPos.az);
        set_sim_alt(horizontalPos.alt);

        NewRaDec(currentRA, currentDEC);
        return;
    }

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
    switch (TrackState)
    {

    case SCOPE_TRACKING:
        /* RA moves at sidereal, Dec stands still */
        currentRA += (SID_RATE*dt/15.);
        break;

    case SCOPE_SLEWING:
    case SCOPE_PARKING:
        /* slewing - nail it when both within one pulse @ SLEWRATE */
        nlocked = 0;

        dx = targetRA - currentRA;

        // Take shortest path
        if (fabs(dx) > 12)
            dx *= -1;

        if (fabs(dx) <= da_ra)
        {
        currentRA = targetRA;
        nlocked++;
        }
        else if (dx > 0)
            currentRA += da_ra/15.;
        else
            currentRA -= da_ra/15.;

        if (currentRA < 0)
            currentRA += 24;
        else if (currentRA > 24)
            currentRA -= 24;

        dx = targetDEC - currentDEC;
        if (fabs(dx) <= da_dec)
        {
        currentDEC = targetDEC;
        nlocked++;
        }
        else if (dx > 0)
          currentDEC += da_dec;
        else
          currentDEC -= da_dec;

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

    ln_equ_posn equatorialPos;
    equatorialPos.ra  = currentRA * 15;
    equatorialPos.dec = currentDEC;

    ln_lnlat_posn observer;

    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;

    if (observer.lng > 180)
        observer.lng -= 360;

    ln_hrz_posn horizontalPos;

    ln_get_hrz_from_equ(&equatorialPos, &observer, ln_get_julian_from_sys(), &horizontalPos);

    // Libnova south = 0, west = 90, north = 180, east = 270
    horizontalPos.az -= 180;
    if (horizontalPos.az < 0)
        horizontalPos.az += 360;

    set_sim_az(horizontalPos.az);
    set_sim_alt(horizontalPos.alt);
}

void CelestronGPS::simulationTriggered(bool enable)
{
    set_celestron_simulation(enable);
}

bool CelestronGPS::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (fwInfo.controllerVersion < 2.3)
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Firmwre version 2.3 or higher is required to update location. Current version is %3.1f", fwInfo.controllerVersion);
        return false;
    }

    return (set_celestron_location(PortFD, longitude, latitude));
}

bool CelestronGPS::updateTime(ln_date *utc, double utc_offset)
{
    if (fwInfo.controllerVersion < 2.3)
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Firmwre version 2.3 or higher is required to update time. Current version is %3.1f", fwInfo.controllerVersion);
        return false;
    }

    return (set_celestron_datetime(PortFD, utc, utc_offset));
}

bool CelestronGPS::Park()
{    
    if (GotoAzAlt(GetAxis1Park(), GetAxis2Park()))
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
    double parkAZ  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    DEBUGF(INDI::Logger::DBG_DEBUG, "Unparking from Az (%s) Alt (%s)...", AzStr, AltStr);

    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270
    horizontalPos.az = parkAZ + 180;
    if (horizontalPos.az >= 360)
        horizontalPos.az -= 360;
    horizontalPos.alt = parkAlt;

    ln_lnlat_posn observer;

    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;

    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;

    ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

    char RAStr[16], DEStr[16];
    fs_sexa(RAStr, equatorialPos.ra/15.0, 2, 3600);
    fs_sexa(DEStr, equatorialPos.dec, 2, 3600);
    DEBUGF(INDI::Logger::DBG_DEBUG, "Syncing to parked coordinates RA (%s) DEC (%s)...", RAStr, DEStr);

    if (Sync(equatorialPos.ra/15.0, equatorialPos.dec))
    {
        SetParked(false);
        return true;
    }
    else
        return false;

}

void CelestronGPS::SetCurrentPark()
{
    SetAxis1Park(currentAZ);
    SetAxis2Park(currentALT);
}

void CelestronGPS::SetDefaultPark()
{
    // By defualt azimuth 0
    SetAxis1Park(0);

    // Altitude = latitude of observer
    SetAxis2Park(LocationN[LOCATION_LATITUDE].value);
}

