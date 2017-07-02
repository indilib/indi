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

#include "lx200ap.h"

#include "indicom.h"
#include "lx200driver.h"
#include "lx200apdriver.h"

#include <libnova/transform.h>

#include <math.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define FIRMWARE_TAB "Firmware data"
#define MOUNT_TAB    "Mount"

/* Constructor */
LX200AstroPhysics::LX200AstroPhysics() : LX200Generic()
{
    timeUpdated = locationUpdated = false;
    initStatus                    = MOUNTNOTINITIALIZED;

    setLX200Capability(LX200_HAS_PULSE_GUIDING | LX200_HAS_TRACK_MODE);

    SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_HAS_PIER_SIDE, 4);

    //ctor
    currentRA  = get_local_sideral_time(0);
    currentDEC = 90;
}

const char *LX200AstroPhysics::getDefaultName()
{
    return (const char *)"AstroPhysics";
}

bool LX200AstroPhysics::initProperties()
{
    LX200Generic::initProperties();

    IUFillSwitch(&StartUpS[0], "COLD", "Cold", ISS_OFF);
    IUFillSwitch(&StartUpS[1], "WARM", "Warm", ISS_OFF);
    IUFillSwitchVector(&StartUpSP, StartUpS, 2, getDeviceName(), "STARTUP", "Mount init.", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&HourangleCoordsN[0], "HA", "HA H:M:S", "%10.6m", 0., 24., 0., 0.);
    IUFillNumber(&HourangleCoordsN[1], "DEC", "Dec D:M:S", "%10.6m", -90.0, 90.0, 0., 0.);
    IUFillNumberVector(&HourangleCoordsNP, HourangleCoordsN, 2, getDeviceName(), "HOURANGLE_COORD", "Hourangle Coords",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&HorizontalCoordsN[0], "AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0.);
    IUFillNumber(&HorizontalCoordsN[1], "ALT", "Alt D:M:S", "%10.6m", -90., 90., 0., 0.);
    IUFillNumberVector(&HorizontalCoordsNP, HorizontalCoordsN, 2, getDeviceName(), "HORIZONTAL_COORD",
                       "Horizontal Coords", MAIN_CONTROL_TAB, IP_RW, 120, IPS_IDLE);

    // Slew speed when performing regular GOTO
    IUFillSwitch(&SlewRateS[0], "600", "600x", ISS_OFF);
    IUFillSwitch(&SlewRateS[1], "900", "900x", ISS_OFF);
    IUFillSwitch(&SlewRateS[2], "1200", "1200x", ISS_ON);

    // Motion speed of axis when pressing NSWE buttons
    IUFillSwitch(&MotionSpeedS[0], "12", "12x", ISS_OFF);
    IUFillSwitch(&MotionSpeedS[1], "64", "64x", ISS_ON);
    IUFillSwitch(&MotionSpeedS[2], "600", "600x", ISS_OFF);
    IUFillSwitch(&MotionSpeedS[3], "1200", "1200x", ISS_OFF);
    IUFillSwitchVector(&MotionSpeedSP, MotionSpeedS, 4, getDeviceName(), "Motion Speed", "", MOTION_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    IUFillSwitch(&SwapS[0], "NS", "North/South", ISS_OFF);
    IUFillSwitch(&SwapS[1], "EW", "East/West", ISS_OFF);
    IUFillSwitchVector(&SwapSP, SwapS, 2, getDeviceName(), "SWAP", "Swap buttons", MOTION_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillSwitch(&SyncCMRS[USE_REGULAR_SYNC], ":CM#", ":CM#", ISS_ON);
    IUFillSwitch(&SyncCMRS[USE_CMR_SYNC], ":CMR#", ":CMR#", ISS_OFF);
    IUFillSwitchVector(&SyncCMRSP, SyncCMRS, 2, getDeviceName(), "SYNCCMR", "Sync", MOTION_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillText(&VersionT[0], "Number", "", 0);
    IUFillTextVector(&VersionInfo, VersionT, 1, getDeviceName(), "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE);

    IUFillText(&DeclinationAxisT[0], "RELHA", "rel. to HA", "undefined");
    IUFillTextVector(&DeclinationAxisTP, DeclinationAxisT, 1, getDeviceName(), "DECLINATIONAXIS", "Declination axis",
                     MOUNT_TAB, IP_RO, 0, IPS_IDLE);

    // Slew threshold
    IUFillNumber(&SlewAccuracyN[0], "SlewRA", "RA (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    IUFillNumber(&SlewAccuracyN[1], "SlewDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    IUFillNumberVector(&SlewAccuracyNP, SlewAccuracyN, 2, getDeviceName(), "Slew Accuracy", "", MOUNT_TAB, IP_RW, 0,
                       IPS_IDLE);

    SetParkDataType(PARK_AZ_ALT);

    return true;
}

void LX200AstroPhysics::ISGetProperties(const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    if (isConnected())
    {
        defineSwitch(&StartUpSP);
        defineText(&VersionInfo);

        //defineText(&DeclinationAxisTP);

        /* Motion group */
        defineSwitch(&TrackModeSP);
        defineSwitch(&MotionSpeedSP);
        defineSwitch(&SwapSP);
        defineSwitch(&SyncCMRSP);
        defineNumber(&SlewAccuracyNP);

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

        //defineText(&DeclinationAxisTP);

        /* Motion group */
        defineSwitch(&TrackModeSP);
        defineSwitch(&MotionSpeedSP);
        defineSwitch(&SwapSP);
        defineSwitch(&SyncCMRSP);
        defineNumber(&SlewAccuracyNP);

        DEBUG(INDI::Logger::DBG_SESSION, "Please initialize the mount before issuing any command.");
    }
    else
    {
        deleteProperty(StartUpSP.name);
        deleteProperty(VersionInfo.name);
        //deleteProperty(DeclinationAxisTP.name);
        deleteProperty(TrackModeSP.name);
        deleteProperty(MotionSpeedSP.name);
        deleteProperty(SwapSP.name);
        deleteProperty(SyncCMRSP.name);
        deleteProperty(SlewAccuracyNP.name);
    }

    return true;
}

bool LX200AstroPhysics::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int err = 0;

    // ignore if not ours //
    if (strcmp(getDeviceName(), dev))
        return false;

    // ============================================================
    // Satisfy AP mount initialization, see AP key pad manual p. 76
    // ============================================================
    if (!strcmp(name, StartUpSP.name))
    {
        int switch_nr;

        IUUpdateSwitch(&StartUpSP, states, names, n);

        if (initStatus == MOUNTNOTINITIALIZED)
        {
            if (timeUpdated == false || locationUpdated == false)
            {
                StartUpSP.s = IPS_ALERT;
                DEBUG(INDI::Logger::DBG_ERROR, "Time and location must be set before mount initialization is invoked.");
                IDSetSwitch(&StartUpSP, nullptr);
                return false;
            }

            if (StartUpSP.sp[0].s == ISS_ON) // do it only in case a power on (cold start)
            {
                if (setBasicDataPart1() == false)
                {
                    StartUpSP.s = IPS_ALERT;
                    IDSetSwitch(&StartUpSP, "Cold mount initialization failed.");
                    return false;
                }
            }

            initStatus = MOUNTINITIALIZED;

            if (isSimulation())
            {
                TrackModeSP.s = IPS_OK;
                IDSetSwitch(&TrackModeSP, nullptr);

                SlewRateSP.s = IPS_OK;
                IDSetSwitch(&SlewRateSP, nullptr);

                MotionSpeedSP.s = IPS_OK;
                IDSetSwitch(&MotionSpeedSP, nullptr);

                IUSaveText(&VersionT[0], "1.0");
                VersionInfo.s = IPS_OK;
                IDSetText(&VersionInfo, nullptr);

                StartUpSP.s = IPS_OK;
                IDSetSwitch(&StartUpSP, "Mount initialized.");

                currentRA  = 0;
                currentDEC = 90;
            }
            else
            {
                // Make sure that the mount is setup according to the properties
                switch_nr = IUFindOnSwitchIndex(&TrackModeSP);

                if ( (err = selectAPTrackingMode(PortFD, switch_nr)) < 0)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "StartUpSP: Error setting tracking mode (%d).", err);
                    return false;
                }

                TrackModeSP.s = IPS_OK;
                IDSetSwitch(&TrackModeSP, nullptr);

                switch_nr = IUFindOnSwitchIndex(&SlewRateSP);
                if ( (err = selectAPSlewRate(PortFD, switch_nr)) < 0)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "StartUpSP: Error setting slew rate (%d).", err);
                    return false;
                }

                SlewRateSP.s = IPS_OK;
                IDSetSwitch(&SlewRateSP, nullptr);

                switch_nr = IUFindOnSwitchIndex(&MotionSpeedSP);
                if ( (err = selectAPMoveToRate(PortFD, switch_nr)) < 0)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "StartUpSP: Error setting move to rate (%d).", err);
                    return false;
                }
                MotionSpeedSP.s = IPS_OK;
                IDSetSwitch(&MotionSpeedSP, nullptr);

                getLX200RA(PortFD, &currentRA);
                getLX200DEC(PortFD, &currentDEC);

                // make a IDSet in order the dome controller is aware of the initial values
                targetRA  = currentRA;
                targetDEC = currentDEC;

                NewRaDec(currentRA, currentDEC);

                VersionInfo.tp[0].text = new char[64];

                getAPVersionNumber(PortFD, VersionInfo.tp[0].text);

                VersionInfo.s = IPS_OK;
                IDSetText(&VersionInfo, nullptr);

                StartUpSP.s = IPS_OK;
                IDSetSwitch(&StartUpSP, "Mount initialized.");

            }
        }
        else
        {
            StartUpSP.s = IPS_OK;
            IDSetSwitch(&StartUpSP, "Mount is already initialized.");
        }
        return true;
    }

    // =======================================
    // Tracking Mode
    // =======================================
    if (!strcmp(name, TrackModeSP.name))
    {
        IUResetSwitch(&TrackModeSP);
        IUUpdateSwitch(&TrackModeSP, states, names, n);
        trackingMode = IUFindOnSwitchIndex(&TrackModeSP);

        if (isSimulation() == false && (err = selectAPTrackingMode(PortFD, trackingMode) < 0))
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error setting tracking mode (%d).", err);
            return false;
        }
        TrackModeSP.s = IPS_OK;
        IDSetSwitch(&TrackModeSP, nullptr);

        /* What is this for?
        if( trackingMode != 3) // not zero
        {
            AbortSlewSP.s = IPS_IDLE;
            IDSetSwitch(&AbortSlewSP, nullptr);
        }
        */

        return true;
    }

    // =======================================
    // Swap Buttons
    // =======================================
    if (!strcmp(name, SwapSP.name))
    {
        int currentSwap;

        IUResetSwitch(&SwapSP);
        IUUpdateSwitch(&SwapSP, states, names, n);
        currentSwap = IUFindOnSwitchIndex(&SwapSP);

        if ((isSimulation() == false && (err = swapAPButtons(PortFD, currentSwap)) < 0))
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error swapping buttons (%d).", err);
            return false;
        }

        SwapS[0].s = ISS_OFF;
        SwapS[1].s = ISS_OFF;
        SwapSP.s   = IPS_OK;
        IDSetSwitch(&SwapSP, nullptr);
        return true;
    }

    // ===========================================================
    // Motion Speed. For setting NSWE speed only. Does not affect slewing speed
    // ===========================================================
    if (!strcmp(name, MotionSpeedSP.name))
    {
        IUUpdateSwitch(&MotionSpeedSP, states, names, n);
        int moveRate = IUFindOnSwitchIndex(&MotionSpeedSP);

        if (isSimulation() == false && (err = selectAPMoveToRate(PortFD, moveRate) < 0))
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error setting move to rate (%d).", err);
            return false;
        }

        MotionSpeedSP.s = IPS_OK;
        IDSetSwitch(&MotionSpeedSP, nullptr);
        return true;
    }

    // =======================================
    // Choose the appropriate sync command
    // =======================================
    if (!strcmp(name, SyncCMRSP.name))
    {
        IUResetSwitch(&SyncCMRSP);
        IUUpdateSwitch(&SyncCMRSP, states, names, n);
        IUFindOnSwitchIndex(&SyncCMRSP);
        SyncCMRSP.s = IPS_OK;
        IDSetSwitch(&SyncCMRSP, nullptr);
        return true;
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200AstroPhysics::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(getDeviceName(), dev))
        return false;

    // Update slew precision limit
    if (!strcmp(name, SlewAccuracyNP.name))
    {
        if (IUUpdateNumber(&SlewAccuracyNP, values, names, n) < 0)
            return false;

        SlewAccuracyNP.s = IPS_OK;

        if (SlewAccuracyN[0].value < 3 || SlewAccuracyN[1].value < 3)
            IDSetNumber(&SlewAccuracyNP, "Warning: Setting the slew accuracy too low may result in a dead lock");

        IDSetNumber(&SlewAccuracyNP, nullptr);
        return true;
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200AstroPhysics::isMountInit()
{
    return (StartUpSP.s != IPS_IDLE);
}

bool LX200AstroPhysics::ReadScopeStatus()
{
    if (!isMountInit())
        return false;

    if (isSimulation())
    {
        mountSim();
        return true;
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
        return false;
    }

    if (TrackState == SCOPE_SLEWING)
    {
        double dx = targetRA - currentRA;
        double dy = targetDEC - currentDEC;

        // Wait until acknowledged or within threshold
        if (fabs(dx) <= (SlewAccuracyN[0].value / (900.0)) && fabs(dy) <= (SlewAccuracyN[1].value / 60.0))
        {
            TrackState = SCOPE_TRACKING;
            DEBUG(INDI::Logger::DBG_SESSION, "Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        double currentAlt, currentAz;
        if (getLX200Az(PortFD, &currentAz) < 0 || getLX200Alt(PortFD, &currentAlt) < 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error reading Az/Alt.");
            return false;
        }

        double dx = GetAxis1Park() - currentAz;
        double dy = GetAxis2Park() - currentAlt;

        DEBUGF(INDI::Logger::DBG_DEBUG,
               "Parking... targetAz: %g currentAz: %g dx: %g targetAlt: %g currentAlt: %g dy: %g", GetAxis1Park(),
               currentAz, dx, GetAxis2Park(), currentAlt, dy);

        if (fabs(dx) <= (SlewAccuracyN[0].value / (60.0)) && fabs(dy) <= (SlewAccuracyN[1].value / 60.0))
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Parking slew is complete. Asking astrophysics mount to park...");

            if (isSimulation() == false && setAPPark(PortFD) < 0)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Parking Failed.");
                return false;
            }

            SetParked(true);
        }
    }

    NewRaDec(currentRA, currentDEC);

    syncSideOfPier();

    return true;
}

bool LX200AstroPhysics::setBasicDataPart0()
{
    int err;
    //struct ln_date utm;
    //struct ln_zonedate ltm;

    if (isSimulation() == true)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "setBasicDataPart0 simulation complete.");
        return true;
    }

    if ((err = setAPClearBuffer(PortFD)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error clearing the buffer (%d): %s", err, strerror(err));
        return false;
    }

    if ((err = setAPLongFormat(PortFD)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error setting long format failed (%d): %s", err, strerror(err));
        return false;
    }

    if ((err = setAPBackLashCompensation(PortFD, 0, 0, 0)) < 0)
    {
        // It seems we need to send it twice before it works!
        if ((err = setAPBackLashCompensation(PortFD, 0, 0, 0)) < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error setting back lash compensation (%d): %s.", err, strerror(err));
            return false;
        }
    }

    // Detect and set fomat. It should be LONG.
    checkLX200Format(PortFD);

    return true;
}

bool LX200AstroPhysics::setBasicDataPart1()
{
    int err = 0;

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
        SetAxis2ParkDefault(LocationN[LOCATION_LATITUDE].value);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
        SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value);

        SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
        SetAxis2ParkDefault(LocationN[LOCATION_LATITUDE].value);
    }

    // Unpark
    UnPark();

    // Stop
    if (isSimulation() == false && (err = setAPMotionStop(PortFD)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Stop motion (:Q#) failed, check the mount (%d): %s", strerror(err));
        return false;
    }

    return true;
}

bool LX200AstroPhysics::Goto(double r, double d)
{
    targetRA  = r;
    targetDEC = d;
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
        EqNP.s    = IPS_IDLE;
        IDSetSwitch(&AbortSP, "Slew aborted.");
        IDSetNumber(&EqNP, nullptr);

        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
            MovementNSSP.s = MovementWESP.s = IPS_IDLE;
            EqNP.s                          = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
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

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = Slew(PortFD)))
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
            slewError(err);
            return false;
        }
    }

    TrackState = SCOPE_SLEWING;
    EqNP.s     = IPS_BUSY;

    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool LX200AstroPhysics::Handshake()
{
    if (isSimulation())
    {
        IDMessage(getDeviceName(), "Simulated Astrophysics is online. Retrieving basic data...");
        return true;
    }

    return setBasicDataPart0();
}

bool LX200AstroPhysics::Disconnect()
{
    timeUpdated     = false;
    locationUpdated = false;

    return LX200Generic::Disconnect();
}

bool LX200AstroPhysics::Sync(double ra, double dec)
{
    char syncString[256];

    int syncType = IUFindOnSwitchIndex(&SyncCMRSP);

    if (isSimulation() == false)
    {
        if (setAPObjectRA(PortFD, ra) < 0 || setAPObjectDEC(PortFD, dec) < 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error setting RA/DEC. Unable to Sync.");
            return false;
        }

        bool syncOK = true;

        switch (syncType)
        {
        case USE_REGULAR_SYNC:
            if (::Sync(PortFD, syncString) < 0)
                syncOK = false;
            break;

        case USE_CMR_SYNC:
            if (APSyncCMR(PortFD, syncString) < 0)
                syncOK = false;
            break;

        default:
            break;
        }

        if (syncOK == false)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Synchronization failed.");
            return false;
        }

    }

    currentRA  = ra;
    currentDEC = dec;

    DEBUGF(INDI::Logger::DBG_DEBUG, "%s Synchronization successful %s", (syncType == USE_REGULAR_SYNC ? "CM" : "CMR"), syncString);
    DEBUG(INDI::Logger::DBG_SESSION, "Synchronization successful.");

    TrackState = SCOPE_IDLE;
    EqNP.s     = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200AstroPhysics::updateTime(ln_date *utc, double utc_offset)
{
    struct ln_zonedate ltm;

    if (isSimulation())
    {
        timeUpdated = true;
        return true;
    }

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600.0);

    JD = ln_get_julian_day(utc);

    DEBUGF(INDI::Logger::DBG_DEBUG, "New JD is %f", (float)JD);

    // Set Local Time
    if (setLocalTime(PortFD, ltm.hours, ltm.minutes, (int)ltm.seconds) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting local time.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Set Local Time %02d:%02d:%02d is successful.", ltm.hours, ltm.minutes,
           (int)ltm.seconds);

    if (setCalenderDate(PortFD, ltm.days, ltm.months, ltm.years) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting local date.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Set Local Date %02d/%02d/%02d is successful.", ltm.days, ltm.months, ltm.years);

    if (setAPUTCOffset(PortFD, fabs(utc_offset)) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting UTC Offset.");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Set UTC Offset %g (always positive for AP) is successful.", fabs(utc_offset));

    DEBUG(INDI::Logger::DBG_SESSION, "Time updated.");

    timeUpdated = true;

    return true;
}

bool LX200AstroPhysics::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
    {
        locationUpdated = true;
        return true;
    }

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
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    IDMessage(getDeviceName(), "Site location updated to Lat %.32s - Long %.32s", l, L);

    locationUpdated = true;

    return true;
}

void LX200AstroPhysics::debugTriggered(bool enable)
{
    INDI_UNUSED(enable);
    LX200Generic::debugTriggered(enable);
    set_lx200ap_name(getDeviceName(), DBG_SCOPE);
}

bool LX200AstroPhysics::SetSlewRate(int index)
{
    if (isSimulation() == false && selectAPSlewRate(PortFD, index) < 0)
    {
        SlewRateSP.s = IPS_ALERT;
        IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
        return false;
    }

    SlewRateSP.s = IPS_OK;
    IDSetSwitch(&SlewRateSP, nullptr);
    return true;
}

bool LX200AstroPhysics::Park()
{
    if (initStatus == MOUNTNOTINITIALIZED)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "You must initialize the mount before parking.");
        return false;
    }

    double parkAz  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    DEBUGF(INDI::Logger::DBG_DEBUG, "Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    if (setAPObjectAZ(PortFD, parkAz) < 0 || setAPObjectAlt(PortFD, parkAlt) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting Az/Alt.");
        return false;
    }

    int err = 0;

    /* Slew reads the '0', that is not the end of the slew */
    if ((err = Slew(PortFD)))
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error Slewing to Az %s - Alt %s", AzStr, AltStr);
        slewError(err);
        return false;
    }

    EqNP.s     = IPS_BUSY;
    TrackState = SCOPE_PARKING;
    DEBUG(INDI::Logger::DBG_SESSION, "Parking is in progress...");

    return true;
}

bool LX200AstroPhysics::UnPark()
{
    // First we unpark astrophysics
    if (isSimulation() == false)
    {
        if (setAPUnPark(PortFD) < 0)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "UnParking Failed.");
            return false;
        }
    }

    // Then we sync with to our last stored position
    double parkAz  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    DEBUGF(INDI::Logger::DBG_DEBUG, "Syncing to parked coordinates Az (%s) Alt (%s)...", AzStr, AltStr);

    if (setAPObjectAZ(PortFD, parkAz) < 0 || (setAPObjectAlt(PortFD, parkAlt)) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting Az/Alt.");
        return false;
    }

    char syncString[256];
    if (APSyncCM(PortFD, syncString) < 0)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Sync failed.");
        return false;
    }

    SetParked(false);
    return true;
}

bool LX200AstroPhysics::SetCurrentPark()
{
    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270

    ln_lnlat_posn observer;
    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;
    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;
    equatorialPos.ra  = currentRA * 15;
    equatorialPos.dec = currentDEC;
    ln_get_hrz_from_equ(&equatorialPos, &observer, ln_get_julian_from_sys(), &horizontalPos);

    double parkAZ = horizontalPos.az - 180;
    if (parkAZ < 0)
        parkAZ += 360;
    double parkAlt = horizontalPos.alt;

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    DEBUGF(INDI::Logger::DBG_DEBUG, "Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr,
           AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool LX200AstroPhysics::SetDefaultPark()
{
    // Az = 0 for North hemisphere
    SetAxis1Park(LocationN[LOCATION_LATITUDE].value > 0 ? 0 : 180);

    // Alt = Latitude
    SetAxis2Park(LocationN[LOCATION_LATITUDE].value);

    return true;
}

void LX200AstroPhysics::syncSideOfPier()
{
    const char *cmd = ":pS#";
    // Response
    char response[16] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return;
    }

    // Read Side
    if ((rc = tty_read_section(PortFD, response, '#', 3, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error reading from device %s (%d)", errmsg, rc);
        return;
    }

    response[nbytes_read - 1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: <%s>", response);

    if (!strcmp(response, "East"))
        setPierSide(INDI::Telescope::PIER_EAST);
    else
        setPierSide(INDI::Telescope::PIER_WEST);

}

bool LX200AstroPhysics::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &SyncCMRSP);
    IUSaveConfigSwitch(fp, &MotionSpeedSP);

    return true;
}
