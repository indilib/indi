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

#include "paramount.h"

#include "indicom.h"

#include <math.h>
#include <memory>
#include <regex>
#include <string.h>

// We declare an auto pointer to Paramount.
std::unique_ptr<Paramount> paramount_mount(new Paramount());

#define GOTO_RATE      5         /* slew rate, degrees/s */
#define SLEW_RATE      0.5       /* slew rate, degrees/s */
#define FINE_SLEW_RATE 0.1       /* slew rate, degrees/s */
#define SID_RATE       0.004178  /* sidereal rate, degrees/s */
#define SID_RATE_MS    0.0150408 /* Sidereal rate, arcseconds/ms */

#define GOTO_LIMIT      5.5 /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT      1   /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */
#define FINE_SLEW_LIMIT 0.5 /* Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees */

#define PARAMOUNT_TIMEOUT 3 /* Timeout in seconds */
#define PARAMOUNT_NORTH   0
#define PARAMOUNT_SOUTH   1
#define PARAMOUNT_EAST    2
#define PARAMOUNT_WEST    3

#define RA_AXIS  0
#define DEC_AXIS 1

#define STELLAR_DAY        86164.098903691
#define TRACKRATE_SIDEREAL ((360.0 * 3600.0) / STELLAR_DAY)
#define SOLAR_DAY          86400
#define TRACKRATE_SOLAR    ((360.0 * 3600.0) / SOLAR_DAY)
#define TRACKRATE_LUNAR    14.511415

/* Preset Slew Speeds */
#define SLEWMODES 9
const double slewspeeds[SLEWMODES] = { 1.0, 2.0, 4.0, 8.0, 32.0, 64.0, 128.0, 256.0, 512.0 };

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    paramount_mount->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    paramount_mount->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    paramount_mount->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    paramount_mount->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
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
void ISSnoopDevice(XMLEle *root)
{
    paramount_mount->ISSnoopDevice(root);
}

Paramount::Paramount()
{
    currentRA  = 0;
    currentDEC = 90;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                               TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION,
                           9);
    setTelescopeConnection(CONNECTION_TCP);
}

Paramount::~Paramount()
{
}

const char *Paramount::getDefaultName()
{
    return (char *)"Paramount";
}

bool Paramount::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    for (int i = 0; i < SlewRateSP.nsp-1; i++)
    {
        sprintf(SlewRateSP.sp[i].label, "%.fx", slewspeeds[i]);
        SlewRateSP.sp[i].aux = (void *)&slewspeeds[i];
    }

    // Set 64x as default speed
    SlewRateSP.sp[5].s = ISS_ON;

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&JogRateN[RA_AXIS], "JOG_RATE_WE", "W/E Rate (arcmin)", "%g", 0, 600, 60, 30);
    IUFillNumber(&JogRateN[DEC_AXIS], "JOG_RATE_NS", "N/S Rate (arcmin)", "%g", 0, 600, 60, 30);
    IUFillNumberVector(&JogRateNP, JogRateN, 2, getDeviceName(), "JOG_RATE", "Jog Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[RA_AXIS], "GUIDE_RATE_WE", "W/E Rate", "%1.1f", 0.0, 1.0, 0.1, 0.5);
    IUFillNumber(&GuideRateN[DEC_AXIS], "GUIDE_RATE_NS", "N/S Rate", "%1.1f", 0.0, 1.0, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Tracking Mode
    IUFillSwitch(&TrackModeS[TRACK_SIDEREAL], "TRACK_SIDEREAL", "Sidereal", ISS_OFF);
    IUFillSwitch(&TrackModeS[TRACK_SOLAR], "TRACK_SOLAR", "Solar", ISS_OFF);
    IUFillSwitch(&TrackModeS[TRACK_LUNAR], "TRACK_LUNAR", "Lunar", ISS_OFF);
    IUFillSwitch(&TrackModeS[TRACK_CUSTOM], "TRACK_CUSTOM", "Custom", ISS_OFF);
    IUFillSwitchVector(&TrackModeSP, TrackModeS, 4, getDeviceName(), "TELESCOPE_TRACK_MODE", "Track Mode",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Custom Tracking Rate
    IUFillNumber(&TrackRateN[0], "TRACK_RATE_RA", "RA (arcsecs/s)", "%.6f", -16384.0, 16384.0, 0.000001, 15.041067);
    IUFillNumber(&TrackRateN[1], "TRACK_RATE_DE", "DE (arcsecs/s)", "%.6f", -16384.0, 16384.0, 0.000001, 0);
    IUFillNumberVector(&TrackRateNP, TrackRateN, 2, getDeviceName(), "TELESCOPE_TRACK_RATE", "Track Rates",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Let's simulate it to be an F/7.5 120mm telescope with 50m 175mm guide scope
    ScopeParametersN[0].value = 120;
    ScopeParametersN[1].value = 900;
    ScopeParametersN[2].value = 50;
    ScopeParametersN[3].value = 175;

    TrackState = SCOPE_IDLE;

    SetParkDataType(PARK_RA_DEC);

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    addAuxControls();

    return true;
}

bool Paramount::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        if (isTheSkyTracking())
        {
            IUResetSwitch(&TrackModeSP);
            TrackModeS[TRACK_SIDEREAL].s = ISS_ON;
            TrackModeSP.s                = IPS_BUSY;
        }
        else
        {
            IUResetSwitch(&TrackModeSP);
            TrackModeSP.s = IPS_IDLE;
        }

        defineSwitch(&TrackModeSP);
        defineNumber(&TrackRateNP);

        defineNumber(&JogRateNP);

        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);
        defineNumber(&GuideRateNP);

        double HA  = ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
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

        SetParked(isTheSkyParked());
    }
    else
    {
        deleteProperty(TrackModeSP.name);
        deleteProperty(TrackRateNP.name);

        deleteProperty(JogRateNP.name);

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.name);
    }

    return true;
}

bool Paramount::Handshake()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF], pRES[MAXRBUF];

    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "sky6RASCOMTele.ConnectAndDoNotUnpark();"
            "Out = sky6RASCOMTele.IsConnected;",
            MAXRBUF);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error writing to TheSky6 TCP server.");
        return false;
    }

    // Should we read until we encounter string terminator? or what?
    if ((rc == tty_read_section(PortFD, pRES, '\0', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error reading from TheSky6 TCP server.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", pRES);
    int isTelescopeConnected = -1;

    std::regex rgx("(\\d+)\\|(.+)\\. Error = (\\d+)\\.");
    std::smatch match;
    std::string input(pRES);
    if (std::regex_search(input, match, rgx))
        isTelescopeConnected = atoi(match.str(1).c_str());

    if (isTelescopeConnected <= 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to telescope: %s (%d).", match.str(1).c_str(),
               atoi(match.str(2).c_str()));
        return false;
    }

    return true;
}

bool Paramount::getMountRADE()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0, errorCode = 0;
    char pCMD[MAXRBUF], pRES[MAXRBUF];

    //"if (sky6RASCOMTele.IsConnected==0) sky6RASCOMTele.Connect();"
    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "sky6RASCOMTele.GetRaDec();"
            "Out = String(sky6RASCOMTele.dRa) + ',' + String(sky6RASCOMTele.dDec);",
            MAXRBUF);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error writing to TheSky6 TCP server.");
        return false;
    }

    // Should we read until we encounter string terminator? or what?
    if ((rc == tty_read_section(PortFD, pRES, '\0', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error reading from TheSky6 TCP server.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", pRES);

    std::regex rgx("(.+),(.+)\\|(.+)\\. Error = (\\d+)\\.");
    std::smatch match;
    std::string input(pRES);
    bool coordsOK = false;
    if (std::regex_search(input, match, rgx))
    {
        errorCode = atoi(match.str(4).c_str());

        if (errorCode == 0)
        {
            currentRA  = atof(match.str(1).c_str());
            currentDEC = atof(match.str(2).c_str());
            coordsOK   = true;
        }
    }

    if (coordsOK)
        return true;

    DEBUGF(INDI::Logger::DBG_ERROR, "Error reading coordinates %s (%d).", match.str(3).c_str(), errorCode);
    return false;
}

bool Paramount::ReadScopeStatus()
{
    if (isSimulation())
    {
        mountSim();
        return true;
    }

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if (isSlewComplete())
        {
            TrackState = SCOPE_TRACKING;
            IDMessage(getDeviceName(), "Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (isSlewComplete())
        {
            SetParked(true);
            //DEBUG(INDI::Logger::DBG_SESSION, "Mount is parked. Disconnecting...");
            //Disconnect();
        }

        //return true;
    }

    if (getMountRADE() == false)
        return false;

    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, currentRA, 2, 3600);
    fs_sexa(DecStr, currentDEC, 2, 3600);

    DEBUGF(DBG_SCOPE, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(currentRA, currentDEC);
    return true;
}

bool Paramount::Goto(double r, double d)
{
    targetRA  = r;
    targetDEC = d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    ln_equ_posn lnradec;
    lnradec.ra  = (currentRA * 360) / 24.0;
    lnradec.dec = currentDEC;

    ln_get_hrz_from_equ(&lnradec, &lnobserver, ln_get_julian_from_sys(), &lnaltaz);
    /* libnova measures azimuth from south towards west */
//    double current_az = range360(lnaltaz.az + 180);
    //double current_alt =lnaltaz.alt;

    char pCMD[MAXRBUF];
    snprintf(pCMD, MAXRBUF,
             "sky6RASCOMTele.Asynchronous = true;"
             "sky6RASCOMTele.SlewToRaDec(%g, %g,'');",
             targetRA, targetDEC);

    if (sendTheSkyOKCommand(pCMD, "Slewing to target") == false)
        return false;

    TrackState = SCOPE_SLEWING;

    EqNP.s = IPS_BUSY;

    DEBUGF(INDI::Logger::DBG_SESSION, "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool Paramount::isSlewComplete()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0, errorCode = 0;
    char pCMD[MAXRBUF], pRES[MAXRBUF];

    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "Out = sky6RASCOMTele.IsSlewComplete;",
            MAXRBUF);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error writing to TheSky6 TCP server.");
        return false;
    }

    // Should we read until we encounter string terminator? or what?
    if ((rc == tty_read_section(PortFD, pRES, '\0', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error reading from TheSky6 TCP server.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", pRES);

    std::regex rgx("(.+)|(.+)\\. Error = (\\d+)\\.");
    std::smatch match;
    std::string input(pRES);
    if (std::regex_search(input, match, rgx))
    {
        errorCode = atoi(match.str(3).c_str());

        if (errorCode == 0)
        {
            int isComplete = atoi(match.str(1).c_str());
            return (isComplete == 1);
        }
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Error reading isSlewComplete %s (%d).", match.str(2).c_str(), errorCode);
    return false;
}

bool Paramount::isTheSkyParked()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF], pRES[MAXRBUF];

    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "Out = sky6RASCOMTele.IsParked();",
            MAXRBUF);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error writing to TheSky6 TCP server.");
        return false;
    }

    // Should we read until we encounter string terminator? or what?
    if ((rc == tty_read_section(PortFD, pRES, '\0', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error reading from TheSky6 TCP server.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", pRES);

    std::regex rgx("(.+)\\|(.+)\\. Error = (\\d+)\\.");
    std::smatch match;
    std::string input(pRES);
    if (std::regex_search(input, match, rgx))
    {
        if (!strcmp("true", match.str(1).c_str()))
            return true;
        else
            return false;
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Error checking for park. Invalid response: %s", pRES);
    return false;
}

bool Paramount::isTheSkyTracking()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF], pRES[MAXRBUF];

    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "Out = sky6RASCOMTele.IsTracking;",
            MAXRBUF);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error writing to TheSky6 TCP server.");
        return false;
    }

    // Should we read until we encounter string terminator? or what?
    if ((rc == tty_read_section(PortFD, pRES, '\0', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error reading from TheSky6 TCP server.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", pRES);

    std::regex rgx("(.+)\\|(.+)\\. Error = (\\d+)\\.");
    std::smatch match;
    std::string input(pRES);
    if (std::regex_search(input, match, rgx))
    {
        if (!strcmp("1", match.str(1).c_str()))
            return true;
        else
            return false;
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Error checking for tracking. Invalid response: %s", pRES);
    return false;
}

bool Paramount::Sync(double ra, double dec)
{
    char pCMD[MAXRBUF];
    snprintf(pCMD, MAXRBUF, "sky6RASCOMTele.Sync(%g, %g,'');", targetRA, targetDEC);
    if (sendTheSkyOKCommand(pCMD, "Syncing to target") == false)
        return false;

    currentRA  = ra;
    currentDEC = dec;

    DEBUG(INDI::Logger::DBG_SESSION, "Sync is successful.");

    EqNP.s = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool Paramount::Park()
{
    targetRA  = GetAxis1Park();
    targetDEC = GetAxis2Park();

    char pCMD[MAXRBUF];
    strncpy(pCMD, "sky6RASCOMTele.ParkAndDoNotDisconnect();", MAXRBUF);
    if (sendTheSkyOKCommand(pCMD, "Parking mount") == false)
        return false;

    TrackState = SCOPE_PARKING;
    DEBUG(INDI::Logger::DBG_SESSION, "Parking telescope in progress...");
    return true;
}

bool Paramount::UnPark()
{
    if (INDI::Telescope::isLocked())
    {
        DEBUG(INDI::Logger::DBG_SESSION,
              "Cannot unpark mount when dome is locking. See: Dome parking policy, in options tab");
        return false;
    }

    char pCMD[MAXRBUF];
    strncpy(pCMD, "sky6RASCOMTele.Unpark();", MAXRBUF);
    if (sendTheSkyOKCommand(pCMD, "Unparking mount") == false)
        return false;

    SetParked(false);

    return true;
}

bool Paramount::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if (strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "JOG_RATE") == 0)
        {
            IUUpdateNumber(&JogRateNP, values, names, n);
            JogRateNP.s = IPS_OK;
            IDSetNumber(&JogRateNP, nullptr);
            return true;
        }

        // Guiding Rate
        if (!strcmp(name, GuideRateNP.name))
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);
            return true;
        }

        // Tracking Rate
        if (!strcmp(name, TrackRateNP.name))
        {
            IUUpdateNumber(&TrackRateNP, values, names, n);
            if (IUFindOnSwitchIndex(&TrackModeSP) != TRACK_CUSTOM)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Can only set tracking rate if track mode is custom.");
                TrackRateNP.s = IPS_ALERT;
            }
            else
            {
                TrackRateNP.s = setTheSkyTracking(true, false, TrackRateN[RA_AXIS].value, TrackRateN[DEC_AXIS].value) ?
                                    IPS_OK :
                                    IPS_ALERT;
            }

            IDSetNumber(&TrackRateNP, nullptr);
            return true;
        }

        if (!strcmp(name, GuideNSNP.name) || !strcmp(name, GuideWENP.name))
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }
    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool Paramount::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        // Tracking Mode
        if (!strcmp(TrackModeSP.name, name))
        {
            int previousTrackMode = IUFindOnSwitchIndex(&TrackModeSP);

            IUUpdateSwitch(&TrackModeSP, states, names, n);

            int currentTrackMode = IUFindOnSwitchIndex(&TrackModeSP);

            // Engage tracking?
            bool enable     = (currentTrackMode != -1);
            bool isSidereal = (currentTrackMode == TRACK_SIDEREAL);
            double dRA = 0, dDE = 0;
            if (currentTrackMode == TRACK_SOLAR)
                dRA = TRACKRATE_SOLAR;
            else if (currentTrackMode == TRACK_LUNAR)
                dRA = TRACKRATE_LUNAR;
            else if (currentTrackMode == TRACK_CUSTOM)
            {
                dRA = TrackRateN[RA_AXIS].value;
                dDE = TrackRateN[DEC_AXIS].value;
            }

            bool rc = setTheSkyTracking(enable, isSidereal, dRA, dDE);

            if (rc == false)
            {
                TrackModeSP.s = IPS_ALERT;
                if (previousTrackMode != -1)
                {
                    IUResetSwitch(&TrackModeSP);
                    TrackModeS[previousTrackMode].s = ISS_ON;
                }
            }
            else
            {
                TrackModeSP.s = enable ? IPS_BUSY : IPS_IDLE;
            }

            IDSetSwitch(&TrackModeSP, nullptr);
            return true;
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool Paramount::Abort()
{
    char pCMD[MAXRBUF];
    strncpy(pCMD, "sky6RASCOMTele.Abort();", MAXRBUF);
    if (sendTheSkyOKCommand(pCMD, "Abort mount slew") == false)
        return false;

    return true;
}

bool Paramount::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    int motion = (dir == DIRECTION_NORTH) ? PARAMOUNT_NORTH : PARAMOUNT_SOUTH;
    //int rate   = IUFindOnSwitchIndex(&SlewRateSP);
    int rate = slewspeeds[IUFindOnSwitchIndex(&SlewRateSP)];

    switch (command)
    {
        case MOTION_START:
            if (isSimulation() == false && startOpenLoopMotion(motion, rate) == false)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Error setting N/S motion direction.");
                return false;
            }
            else
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving toward %s.", (motion == PARAMOUNT_NORTH) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (isSimulation() == false && stopOpenLoopMotion() == false)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Error stopping N/S motion.");
                return false;
            }
            else
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving toward %s halted.",
                       (motion == PARAMOUNT_NORTH) ? "North" : "South");
            break;
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

    int motion = (dir == DIRECTION_WEST) ? PARAMOUNT_WEST : PARAMOUNT_EAST;
    int rate   = IUFindOnSwitchIndex(&SlewRateSP);

    switch (command)
    {
        case MOTION_START:
            if (isSimulation() == false && startOpenLoopMotion(motion, rate) == false)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Error setting W/E motion direction.");
                return false;
            }
            else
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving toward %s.", (motion == PARAMOUNT_WEST) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (isSimulation() == false && stopOpenLoopMotion() == false)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Error stopping W/E motion.");
                return false;
            }
            else
                DEBUGF(INDI::Logger::DBG_SESSION, "Movement toward %s halted.",
                       (motion == PARAMOUNT_WEST) ? "West" : "East");
            break;
    }

    return true;
}

bool Paramount::startOpenLoopMotion(uint8_t motion, uint16_t rate)
{
    char pCMD[MAXRBUF];
    snprintf(pCMD, MAXRBUF, "sky6RASCOMTele.DoCommand(9,'%d|%d');", motion, rate);

    if (sendTheSkyOKCommand(pCMD, "Starting open loop motion") == false)
        return false;

    return true;
}

bool Paramount::stopOpenLoopMotion()
{
    char pCMD[MAXRBUF];
    strncpy(pCMD, "sky6RASCOMTele.DoCommand(10,'');", MAXRBUF);

    if (sendTheSkyOKCommand(pCMD, "Stopping open loop motion") == false)
        return false;

    return true;
}

bool Paramount::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    // JM: INDI Longitude is 0 to 360 increasing EAST. libnova East is Positive, West is negative
    lnobserver.lng = longitude;

    if (lnobserver.lng > 180)
        lnobserver.lng -= 360;
    lnobserver.lat = latitude;

    DEBUGF(INDI::Logger::DBG_SESSION, "Location updated: Longitude (%g) Latitude (%g)", lnobserver.lng, lnobserver.lat);
    return true;
}

bool Paramount::updateTime(ln_date *utc, double utc_offset)
{
    INDI_UNUSED(utc);
    INDI_UNUSED(utc_offset);
    return true;
}

bool Paramount::SetCurrentPark()
{
    char pCMD[MAXRBUF];
    strncpy(pCMD, "sky6RASCOMTele.SetParkPosition();", MAXRBUF);
    if (sendTheSkyOKCommand(pCMD, "Setting Park Position") == false)
        return false;

    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);

    return true;
}

bool Paramount::SetDefaultPark()
{
    // By default set RA to HA
    SetAxis1Park(ln_get_apparent_sidereal_time(ln_get_julian_from_sys()));

    // Set DEC to 90 or -90 depending on the hemisphere
    SetAxis2Park((LocationN[LOCATION_LATITUDE].value > 0) ? 90 : -90);

    return true;
}

bool Paramount::SetParkPosition(double Axis1Value, double Axis2Value)
{
    INDI_UNUSED(Axis1Value);
    INDI_UNUSED(Axis2Value);
    DEBUG(INDI::Logger::DBG_ERROR, "Setting custom parking position directly is not supported. Slew to the desired "
                                   "parking position and click Current.");
    return false;
}

void Paramount::mountSim()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt, dx, da_ra = 0, da_dec = 0;
    int nlocked;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&tv, nullptr);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt  = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec) / 1e6;
    ltv = tv;

    if (fabs(targetRA - currentRA) * 15. >= GOTO_LIMIT)
        da_ra = GOTO_RATE * dt;
    else if (fabs(targetRA - currentRA) * 15. >= SLEW_LIMIT)
        da_ra = SLEW_RATE * dt;
    else
        da_ra = FINE_SLEW_RATE * dt;

    if (fabs(targetDEC - currentDEC) >= GOTO_LIMIT)
        da_dec = GOTO_RATE * dt;
    else if (fabs(targetDEC - currentDEC) >= SLEW_LIMIT)
        da_dec = SLEW_RATE * dt;
    else
        da_dec = FINE_SLEW_RATE * dt;

    double motionRate = 0;

    if (MovementNSSP.s == IPS_BUSY)
        motionRate = JogRateN[0].value;
    else if (MovementWESP.s == IPS_BUSY)
        motionRate = JogRateN[1].value;

    if (motionRate != 0)
    {
        da_ra  = motionRate * dt * 0.05;
        da_dec = motionRate * dt * 0.05;

        switch (MovementNSSP.s)
        {
            case IPS_BUSY:
                if (MovementNSS[DIRECTION_NORTH].s == ISS_ON)
                    currentDEC += da_dec;
                else if (MovementNSS[DIRECTION_SOUTH].s == ISS_ON)
                    currentDEC -= da_dec;
                break;

            default:
                break;
        }

        switch (MovementWESP.s)
        {
            case IPS_BUSY:
                if (MovementWES[DIRECTION_WEST].s == ISS_ON)
                    currentRA += da_ra / 15.;
                else if (MovementWES[DIRECTION_EAST].s == ISS_ON)
                    currentRA -= da_ra / 15.;
                break;

            default:
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
            currentRA += (SID_RATE * dt / 15.);
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
                currentRA += da_ra / 15.;
            else
                currentRA -= da_ra / 15.;

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

bool Paramount::sendTheSkyOKCommand(const char *command, const char *errorMessage)
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF], pRES[MAXRBUF];

    snprintf(pCMD, MAXRBUF,
             "/* Java Script */"
             "var Out;"
             "try {"
             "%s"
             "Out  = 'OK'; }"
             "catch (err) {Out = err; }",
             command);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error writing to TheSky6 TCP server.");
        return false;
    }

    if ((rc == tty_read_section(PortFD, pRES, '\0', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error reading from TheSky6 TCP server.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", pRES);

    std::regex rgx("(.+)\\|(.+)\\. Error = (\\d+)\\.");
    std::smatch match;
    std::string input(pRES);
    if (std::regex_search(input, match, rgx))
    {
        // If NOT OK, then fail
        if (strcmp("OK", match.str(1).c_str()))
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error %s %s", errorMessage, match.str(1).c_str());
            return false;
        }
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error %s. Invalid response: %s", errorMessage, pRES);
        return false;
    }

    return true;
}

IPState Paramount::GuideNorth(float ms)
{
    // Movement in arcseconds
    double dDec = GuideRateN[DEC_AXIS].value * SID_RATE_MS * ms;

    char pCMD[MAXRBUF];
    snprintf(pCMD, MAXRBUF, "sky6DirectGuide.MoveTelescope(%g, %g);", 0., dDec);

    if (sendTheSkyOKCommand(pCMD, "Guiding north") == false)
        return IPS_ALERT;

    return IPS_OK;
}

IPState Paramount::GuideSouth(float ms)
{
    // Movement in arcseconds
    double dDec = GuideRateN[DEC_AXIS].value * SID_RATE_MS * ms * -1;

    char pCMD[MAXRBUF];
    snprintf(pCMD, MAXRBUF, "sky6DirectGuide.MoveTelescope(%g, %g);", 0., dDec);

    if (sendTheSkyOKCommand(pCMD, "Guiding south") == false)
        return IPS_ALERT;

    return IPS_OK;
}

IPState Paramount::GuideEast(float ms)
{
    // Movement in arcseconds
    double dRA = GuideRateN[RA_AXIS].value * SID_RATE_MS * ms;

    char pCMD[MAXRBUF];
    snprintf(pCMD, MAXRBUF, "sky6DirectGuide.MoveTelescope(%g, %g);", dRA, 0.);

    if (sendTheSkyOKCommand(pCMD, "Guiding east") == false)
        return IPS_ALERT;

    return IPS_OK;
}

IPState Paramount::GuideWest(float ms)
{
    // Movement in arcseconds
    double dRA = GuideRateN[RA_AXIS].value * SID_RATE_MS * ms * -1;

    char pCMD[MAXRBUF];
    snprintf(pCMD, MAXRBUF, "sky6DirectGuide.MoveTelescope(%g, %g);", dRA, 0.);

    if (sendTheSkyOKCommand(pCMD, "Guiding west") == false)
        return IPS_ALERT;

    return IPS_OK;
}

bool Paramount::setTheSkyTracking(bool enable, bool isSidereal, double raRate, double deRate)
{
    int on     = enable ? 1 : 0;
    int ignore = isSidereal ? 1 : 0;

    char pCMD[MAXRBUF];
    snprintf(pCMD, MAXRBUF, "sky6RASCOMTele.SetTracking(%d, %d, %g, %g);", on, ignore, raRate, deRate);

    if (sendTheSkyOKCommand(pCMD, "Setting tracking rate") == false)
        return false;

    return true;
}
