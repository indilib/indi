/*******************************************************************************
 Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Driver for using TheSky6 Pro Scripted operations for mounts via the TCP server.
 While this technically can operate any mount connected to the TheSky6 Pro, it is
 intended for Paramount mounts control.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <regex>

#include "paramount.h"
#include "indicom.h"

#include <memory>

// We declare an auto pointer to Paramount.
std::unique_ptr<Paramount> paramount_mount(new Paramount());

#define	GOTO_RATE           5				/* slew rate, degrees/s */
#define	SLEW_RATE           0.5             /* slew rate, degrees/s */
#define FINE_SLEW_RATE      0.1             /* slew rate, degrees/s */
#define SID_RATE            0.004178        /* sidereal rate, degrees/s */

#define GOTO_LIMIT          5.5             /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT          1               /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */
#define FINE_SLEW_LIMIT     0.5             /* Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees */

#define PARAMOUNT_TIMEOUT   3               /* Timeout in seconds */

#define RA_AXIS             0
#define DEC_AXIS            1

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
        paramount_mount->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        paramount_mount->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        paramount_mount->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        paramount_mount->ISNewNumber(dev, name, values, names, num);
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
   paramount_mount->ISSnoopDevice(root);
}

Paramount::Paramount()
{
    currentRA=0;
    currentDEC=90;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");   

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION , 0);
    setTelescopeConnection(CONNECTION_TCP);
}

Paramount::~Paramount()
{

}

const char * Paramount::getDefaultName()
{
    return (char *)"Paramount";
}

bool Paramount::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&JogRateN[RA_AXIS], "JOG_RATE_WE", "W/E Rate", "%g", 0, 1, 0.1, 0.3);
    IUFillNumber(&JogRateN[DEC_AXIS], "JOG_RATE_NS", "N/S Rate", "%g", 0, 1, 0.1, 0.3);
    IUFillNumberVector(&JogRateNP, JogRateN, 2, getDeviceName(), "JOG_RATE", "Motion Rate", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    // Let's simulate it to be an F/7.5 120mm telescope with 50m 175mm guide scope
    ScopeParametersN[0].value = 120;
    ScopeParametersN[1].value = 900;
    ScopeParametersN[2].value = 50;
    ScopeParametersN[3].value = 175;

    TrackState=SCOPE_IDLE;

    SetParkDataType(PARK_RA_DEC);

    addAuxControls();

    return true;
}

void Paramount::ISGetProperties (const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

    if(isConnected())
    {
        defineNumber(&JogRateNP);
    }

    return;
}

bool Paramount::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineNumber(&JogRateNP);

        double HA = ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
        double DEC = 90;

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(HA);
            SetAxis2ParkDefault(DEC);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(HA);
            SetAxis2Park(DEC);
            SetAxis1ParkDefault(HA);
            SetAxis2ParkDefault(DEC);
        }

    }
    else
    {
        deleteProperty(JogRateNP.name);
    }

    return true;
}

bool Paramount::Handshake()
{
    int rc=0, nbytes_written=0, nbytes_read=0, errorCode=0;
    char pCMD[MAXRBUF], pRES[MAXRBUF];

    strncpy(pCMD, "/* Java Script */"
                  "var Out;"
                  "sky6RASCOMTele.Connect();"
                  "Out = sky6RASCOMTele.IsConnected;", MAXRBUF);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", pCMD);

    if ( (rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error writing to TheSky6 TCP server.");
        return false;
    }

    // Should we read until we encounter string terminator? or what?
    if ( (rc == tty_read_section(PortFD, pRES, '\0', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error reading from TheSky6 TCP server.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", pRES);
    int isTelescopeConnected=-1;

    std::regex rgx("(\\d+)\\|(.+)\\. Error = (\\d+)\\.");
    std::smatch match;
    std::string input(pRES);
    if (std::regex_search(input, match, rgx))
        isTelescopeConnected = atoi(match.str(1).c_str());

    if (isTelescopeConnected <= 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to telescope: %s (%d).", match.str(1).c_str(), atoi(match.str(2).c_str()));
        return false;
    }

    return true;
}

bool Paramount::ReadScopeStatus()
{
    if (isSimulation())
    {
        mountSim();
        return true;
    }

    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, currentRA, 2, 3600);
    fs_sexa(DecStr, currentDEC, 2, 3600);

    DEBUGF(DBG_SCOPE, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(currentRA, currentDEC);
    return true;
}

bool Paramount::Goto(double r,double d)
{
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

   ln_equ_posn lnradec;
   lnradec.ra = (currentRA * 360) / 24.0;
   lnradec.dec =currentDEC;

   ln_get_hrz_from_equ(&lnradec, &lnobserver, ln_get_julian_from_sys(), &lnaltaz);
   /* libnova measures azimuth from south towards west */
   double current_az  =range360(lnaltaz.az + 180);
   //double current_alt =lnaltaz.alt;

   TrackState = SCOPE_SLEWING;

   EqNP.s    = IPS_BUSY;

   DEBUGF(INDI::Logger::DBG_SESSION,"Slewing to RA: %s - DEC: %s", RAStr, DecStr);
   return true;
}

bool Paramount::Sync(double ra, double dec)
{
    currentRA  = ra;
    currentDEC = dec;

    DEBUG(INDI::Logger::DBG_SESSION,"Sync is successful.");

    EqNP.s    = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool Paramount::Park()
{
    targetRA= GetAxis1Park();
    targetDEC= GetAxis2Park();
    TrackState = SCOPE_PARKING;
    DEBUG(INDI::Logger::DBG_SESSION,"Parking telescope in progress...");
    return true;
}

bool Paramount::UnPark()
{
    if (INDI::Telescope::isLocked())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Cannot unpark mount when dome is locking. See: Dome parking policy, in options tab");
        return false;
    }
    SetParked(false);
    return true;
}

bool Paramount::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if(strcmp(dev,getDeviceName())==0)
    {
         if(strcmp(name,"JOG_RATE")==0)
         {
             IUUpdateNumber(&JogRateNP, values, names, n);
             JogRateNP.s = IPS_OK;
             IDSetNumber(&JogRateNP, NULL);
             return true;
         }

    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}

bool Paramount::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {

    }

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev,name,states,names,n);
}

bool Paramount::Abort()
{
    return true;
}


bool Paramount::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    return true;
}

bool Paramount::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    return true;
}

bool Paramount::updateLocation(double latitude, double longitude, double elevation)
{
  INDI_UNUSED(elevation);
  // JM: INDI Longitude is 0 to 360 increasing EAST. libnova East is Positive, West is negative
  lnobserver.lng =  longitude;

  if (lnobserver.lng > 180)
      lnobserver.lng -= 360;
  lnobserver.lat =  latitude;

  DEBUGF(INDI::Logger::DBG_SESSION,"Location updated: Longitude (%g) Latitude (%g)", lnobserver.lng, lnobserver.lat);
  return true;
}

bool Paramount::updateTime(ln_date *utc, double utc_offset)
{
    return true;
}

void Paramount::SetCurrentPark()
{
    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);
}

void Paramount::SetDefaultPark()
{
    // By default set RA to HA
    SetAxis1Park(ln_get_apparent_sidereal_time(ln_get_julian_from_sys()));

    // Set DEC to 90 or -90 depending on the hemisphere
    SetAxis2Park( (LocationN[LOCATION_LATITUDE].value > 0) ? 90 : -90);

}

void Paramount::mountSim ()
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

    double motionRate=0;

    if (MovementNSSP.s == IPS_BUSY)
        motionRate = JogRateN[0].value;
    else if (MovementWESP.s == IPS_BUSY)
        motionRate = JogRateN[1].value;

    if (motionRate != 0)
    {
        da_ra  = motionRate *dt*0.05;
        da_dec = motionRate *dt*0.05;

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
            if (TrackState == SCOPE_SLEWING)
                TrackState = SCOPE_TRACKING;
            else
                SetParked(true);

        }
        break;

    default:
        break;
    }

    NewRaDec(currentRA, currentDEC);
}



