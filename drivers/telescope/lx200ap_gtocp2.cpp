/*
    Astro-Physics INDI driver

    Tailored for GTOCP2

    Copyright (C) 2018 Jasem Mutlaq

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

#include "lx200ap_gtocp2.h"

#include "indicom.h"
#include "lx200driver.h"
#include "lx200apdriver.h"

#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <unistd.h>
#include <termios.h>

/* Constructor */
LX200AstroPhysicsGTOCP2::LX200AstroPhysicsGTOCP2() : LX200Generic()
{
    setLX200Capability(LX200_HAS_PULSE_GUIDING);
    SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_PEC | TELESCOPE_CAN_CONTROL_TRACK
                           | TELESCOPE_HAS_TRACK_RATE, 4);

    sendLocationOnStartup = false;
    sendTimeOnStartup = false;
}

const char *LX200AstroPhysicsGTOCP2::getDefaultName()
{
    return "AstroPhysics GTOCP2";
}

bool LX200AstroPhysicsGTOCP2::initProperties()
{
    LX200Generic::initProperties();

    timeFormat = LX200_24;

    HourangleCoordsNP[0].fill("HA", "HA H:M:S", "%10.6m", 0., 24., 0., 0.);
    HourangleCoordsNP[1].fill("DEC", "Dec D:M:S", "%10.6m", -90.0, 90.0, 0., 0.);
    HourangleCoordsNP.fill(getDeviceName(), "HOURANGLE_COORD", "Hourangle Coords", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    HorizontalCoordsNP[0].fill("AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0.);
    HorizontalCoordsNP[1].fill("ALT", "Alt D:M:S", "%10.6m", -90., 90., 0., 0.);
    HorizontalCoordsNP.fill(getDeviceName(), "HORIZONTAL_COORD", "Horizontal Coords", MAIN_CONTROL_TAB, IP_RW, 120, IPS_IDLE);

    // Max rate is 999.99999X for the GTOCP4.
    // Using :RR998.9999#  just to be safe. 15.041067*998.99999 = 15026.02578
    TrackRateNP[AXIS_RA].setMin(-15026.0258);
    TrackRateNP[AXIS_RA].setMax(15026.0258);
    TrackRateNP[AXIS_DE].setMin(-998.9999);
    TrackRateNP[AXIS_DE].setMax(998.9999);

    // Motion speed of axis when pressing NSWE buttons
    // Note: SlewRateSP is defined in the base class LX200Generic
    // We just set the labels here for AP specific values
    SlewRateSP[0].setLabel("12x");
    SlewRateSP[1].setLabel("64x");
    SlewRateSP[1].setState(ISS_ON);
    SlewRateSP[2].setLabel("600x");
    SlewRateSP[3].setLabel("1200x");

    // Slew speed when performing regular GOTO
    APSlewSpeedSP[0].fill("600", "600x", ISS_ON);
    APSlewSpeedSP[1].fill("900", "900x", ISS_OFF);
    APSlewSpeedSP[2].fill("1200", "1200x", ISS_OFF);
    APSlewSpeedSP.fill(getDeviceName(), "GOTO Rate", "", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    SwapSP[0].fill("NS", "North/South", ISS_OFF);
    SwapSP[1].fill("EW", "East/West", ISS_OFF);
    SwapSP.fill(getDeviceName(), "SWAP", "Swap buttons", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    SyncCMRSP[USE_REGULAR_SYNC].fill(":CM#", ":CM#", ISS_ON);
    SyncCMRSP[USE_CMR_SYNC].fill(":CMR#", ":CMR#", ISS_OFF);
    SyncCMRSP.fill(getDeviceName(), "SYNCCMR", "Sync", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // guide speed
    APGuideSpeedSP[0].fill("0.25", "0.25x", ISS_OFF);
    APGuideSpeedSP[1].fill("0.5", "0.50x", ISS_ON);
    APGuideSpeedSP[2].fill("1.0", "1.0x", ISS_OFF);
    APGuideSpeedSP.fill(getDeviceName(), "Guide Rate", "", GUIDE_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    VersionTP[0].fill("Version", "Version", "");
    VersionTP.fill(getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    SetParkDataType(PARK_AZ_ALT);

    return true;
}

void LX200AstroPhysicsGTOCP2::ISGetProperties(const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    if (isConnected())
    {
        defineProperty(VersionTP);

        /* Motion group */
        defineProperty(APSlewSpeedSP);
        defineProperty(SwapSP);
        defineProperty(SyncCMRSP);
        defineProperty(APGuideSpeedSP);
    }
}

bool LX200AstroPhysicsGTOCP2::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        defineProperty(VersionTP);
        VersionTP.load();

        /* Motion group */
        defineProperty(APSlewSpeedSP);
        defineProperty(SwapSP);
        defineProperty(SyncCMRSP);
        defineProperty(APGuideSpeedSP);

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            double currentLatitude = LocationNP[LOCATION_LATITUDE].getValue();
            SetAxis1ParkDefault(currentLatitude >= 0 ? 0 : 180);
            SetAxis2ParkDefault(currentLatitude);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            double currentLatitude = LocationNP[LOCATION_LATITUDE].getValue();
            SetAxis1Park(currentLatitude >= 0 ? 0 : 180);
            // Use the correct default values based on latitude
            SetAxis1ParkDefault(currentLatitude >= 0 ? 0 : 180);
            SetAxis2ParkDefault(currentLatitude);
        }
    }
    else
    {
        deleteProperty(VersionTP);
        deleteProperty(APSlewSpeedSP);
        deleteProperty(SwapSP);
        deleteProperty(SyncCMRSP);
        deleteProperty(APGuideSpeedSP);
    }

    return true;
}

bool LX200AstroPhysicsGTOCP2::initMount()
{
    // Make sure that the mount is setup according to the properties
    int err = 0;

    bool raOK = false, deOK = false;
    if (isSimulation())
    {
        raOK = deOK = true;
    }
    else
    {
        raOK = (getLX200RA(PortFD, &currentRA) == 0);
        deOK = (getLX200DEC(PortFD, &currentDEC) == 0);
    }

    // If we either failed to get coords; OR
    // RA and DE are zero, then mount is not initialized and we need to initialized it.
    if ( (raOK == false && deOK == false) || (currentRA == 0 && (currentDEC == 0 || currentDEC == 90)))
    {
        LOG_DEBUG("Mount is not yet initialized. Initializing it...");

        if (isSimulation() == false)
        {
            // This is how to init the mount in case RA/DE are missing.
            // :PO#
            if (setAPUnPark(PortFD) < 0)
            {
                LOG_ERROR("UnParking Failed.");
                return false;
            }

            // Stop :Q#
            abortSlew(PortFD);
        }
    }

    mountInitialized = true;

    LOG_DEBUG("Mount is initialized.");

    // Astrophysics mount is always unparked on startup
    // In this driver, unpark only sets the tracking ON.
    // setAPUnPark() is NOT called as this function, despite its name, is only used for initialization purposes.
    UnPark();

    // On most mounts SlewRateS defines the MoveTo AND Slew (GOTO) speeds
    // lx200ap is different - some of the MoveTo speeds are not VALID
    // Slew speeds so we have to keep two lists.
    //
    // SlewRateS is used as the MoveTo speed
    if (isSimulation() == false && (err = selectAPMoveToRate(PortFD, SlewRateSP.findOnSwitchIndex())) < 0)
    {
        LOGF_ERROR("Error setting move rate (%d).", err);
        SlewRateSP.setState(IPS_ALERT);
        SlewRateSP.apply("Error setting move rate.");
        return false;
    }
    SlewRateSP.setState(IPS_OK);
    SlewRateSP.apply();

    // APSlewSpeedsS defines the Slew (GOTO) speeds valid on the AP mounts
    if (isSimulation() == false && (err = selectAPSlewRate(PortFD, APSlewSpeedSP.findOnSwitchIndex())) < 0)
    {
        LOGF_ERROR("Error setting slew to rate (%d).", err);
        APSlewSpeedSP.setState(IPS_ALERT);
        APSlewSpeedSP.apply("Error setting GOTO rate.");
        return false;
    }
    APSlewSpeedSP.setState(IPS_OK);
    APSlewSpeedSP.apply();

    char versionString[128];
    if (isSimulation())
        strncpy(versionString, "E", 128);
    else
        getAPVersionNumber(PortFD, versionString);

    VersionTP[0].setText(versionString);
    VersionTP.setState(IPS_OK);
    VersionTP.apply();

    if (strlen(versionString) != 1)
    {
        LOGF_ERROR("Version not supported GTOCP2 driver: %s", versionString);
        return false;
    }

    // Use the text from the property now
    int typeIndex = VersionTP[0].getText()[0] - 'E';
    if (typeIndex >= 0)
    {
        firmwareVersion = static_cast<ControllerVersion>(typeIndex);
        LOGF_DEBUG("Firmware version index: %d", typeIndex);
        LOGF_INFO("Firmware Version: %c", VersionTP[0].getText()[0]);
    }
    else
    {
        LOGF_ERROR("Invalid version: %s", versionString);
        return false;
    }

    return true;
}

bool LX200AstroPhysicsGTOCP2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int err = 0;

    // ignore if not ours //
    if (strcmp(getDeviceName(), dev))
        return false;

    // =======================================
    // Swap Buttons
    // =======================================
    if (SwapSP.isNameMatch(name))
    {
        if (!SwapSP.update(states, names, n))
            return false;

        int currentSwap = SwapSP.findOnSwitchIndex();

        if ((!isSimulation() && (err = swapAPButtons(PortFD, currentSwap)) < 0))
        {
            LOGF_ERROR("Error swapping buttons (%d).", err);
            SwapSP.setState(IPS_ALERT);
            SwapSP.apply("Error swapping buttons.");
            return false;
        }

        SwapSP.reset();
        SwapSP.setState(IPS_OK);
        SwapSP.apply();
        return true;
    }

    // ===========================================================
    // GOTO ("slew") Speed.
    // ===========================================================
    if (APSlewSpeedSP.isNameMatch(name))
    {
        if (!APSlewSpeedSP.update(states, names, n))
            return false;

        int slewRate = APSlewSpeedSP.findOnSwitchIndex();

        if (!isSimulation() && (err = selectAPSlewRate(PortFD, slewRate) < 0))
        {
            LOGF_ERROR("Error setting GOTO rate (%d).", err);
            APSlewSpeedSP.setState(IPS_ALERT);
            APSlewSpeedSP.apply("Error setting GOTO rate.");
            return false;
        }

        APSlewSpeedSP.setState(IPS_OK);
        APSlewSpeedSP.apply();
        return true;
    }

    // ===========================================================
    // Guide Speed.
    // ===========================================================
    if (APGuideSpeedSP.isNameMatch(name))
    {
        if (!APGuideSpeedSP.update(states, names, n))
            return false;

        int guideRate = APGuideSpeedSP.findOnSwitchIndex();

        if (!isSimulation() && (err = selectAPGuideRate(PortFD, guideRate) < 0))
        {
            LOGF_ERROR("Error setting guide rate (%d).", err);
            APGuideSpeedSP.setState(IPS_ALERT);
            APGuideSpeedSP.apply("Error setting guide rate.");
            return false;
        }

        APGuideSpeedSP.setState(IPS_OK);
        APGuideSpeedSP.apply();
        return true;
    }

    // =======================================
    // Choose the appropriate sync command
    // =======================================
    if (SyncCMRSP.isNameMatch(name))
    {
        if (!SyncCMRSP.update(states, names, n))
            return false;

        // No hardware command needed here, just update state
        SyncCMRSP.setState(IPS_OK);
        SyncCMRSP.apply();
        return true;
    }

    // =======================================
    // Choose the PEC playback mode
    // =======================================
    if (PECStateSP.isNameMatch(name))
    {
        PECStateSP.reset();
        PECStateSP.update(states, names, n);

        int pecstate = PECStateSP.findOnSwitchIndex();

        if (!isSimulation() && (err = selectAPPECState(PortFD, pecstate) < 0))
        {
            LOGF_ERROR("Error setting PEC state (%d).", err);
            return false;
        }

        PECStateSP.setState(IPS_OK);
        PECStateSP.apply();

        return true;
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200AstroPhysicsGTOCP2::ReadScopeStatus()
{
    if (isSimulation())
    {
        mountSim();
        return true;
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.setState(IPS_ALERT);
        EqNP.apply("Error reading RA/DEC.");
        return false;
    }

    if (TrackState == SCOPE_SLEWING)
    {
        double dx = lastRA - currentRA;
        double dy = lastDE - currentDEC;

        LOGF_DEBUG("Slewing... currentRA: %g dx: %g currentDE: %g dy: %g", currentRA, dx, currentDEC, dy);

        // Wait until acknowledged
        if (dx == 0 && dy == 0)
        {
            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }

        // Keep try of last values to determine if the mount settled.
        lastRA = currentRA;
        lastDE = currentDEC;
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (getLX200Az(PortFD, &currentAz) < 0 || getLX200Alt(PortFD, &currentAlt) < 0)
        {
            EqNP.setState(IPS_ALERT);
            EqNP.apply("Error reading Az/Alt.");
            return false;
        }

        double dx = lastAZ - currentAz;
        double dy = lastAL - currentAlt;

        LOGF_DEBUG("Parking... currentAz: %g dx: %g currentAlt: %g dy: %g", currentAz, dx, currentAlt, dy);

        if (dx == 0 && dy == 0)
        {
            LOG_DEBUG("Parking slew is complete. Asking astrophysics mount to park...");

            if (!isSimulation() && setAPPark(PortFD) < 0)
            {
                LOG_ERROR("Parking Failed.");
                return false;
            }

            // Turn off tracking.
            SetTrackEnabled(false);

            SetParked(true);

            LOG_INFO("Please disconnect and power off the mount.");
        }

        lastAZ = currentAz;
        lastAL = currentAlt;
    }

    NewRaDec(currentRA, currentDEC);

    syncSideOfPier();

    return true;
}

bool LX200AstroPhysicsGTOCP2::Goto(double r, double d)
{
    const struct timespec timeout = {0, 100000000L};

    targetRA  = r;
    targetDEC = d;

    char RAStr[64], DecStr[64];
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // If moving, let's stop it first.
    if (EqNP.getState() == IPS_BUSY)
    {
        if (!isSimulation() && abortSlew(PortFD) < 0)
        {
            AbortSP.setState(IPS_ALERT);
            AbortSP.apply("Abort slew failed.");
            return false;
        }

        AbortSP.setState(IPS_OK);
        EqNP.setState(IPS_IDLE);
        AbortSP.apply("Slew aborted.");
        EqNP.apply();

        if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
        {
            MovementNSSP.setState(IPS_IDLE);
            MovementWESP.setState(IPS_IDLE);
            EqNP.setState(IPS_IDLE);
            MovementNSSP.reset();
            MovementWESP.reset();
            MovementNSSP.apply();
            MovementWESP.apply();
        }

        // sleep for 100 mseconds
        nanosleep(&timeout, nullptr);
    }

    if (!isSimulation())
    {
        if (setAPObjectRA(PortFD, targetRA) < 0 || (setAPObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Error setting target RA/DEC.");
            EqNP.apply();
            return false;
        }

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = Slew(PortFD)))
        {
            EqNP.setState(IPS_ALERT);
            LOGF_ERROR("Error Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
            EqNP.apply();
            slewError(err);
            return false;
        }

        motionCommanded = true;
        lastRA = targetRA;
        lastDE = targetDEC;
    }

    TrackState = SCOPE_SLEWING;
    EqNP.setState(IPS_BUSY); // Set state to BUSY when slewing starts
    EqNP.apply();

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}


int LX200AstroPhysicsGTOCP2::SendPulseCmd(int8_t direction, uint32_t duration_msec)
{
    // handleGTOCP2MotionBug needs to be called *before* the pulse command
    if (firmwareVersion == MCV_E)
        handleGTOCP2MotionBug();

    return APSendPulseCmd(PortFD, direction, duration_msec);
}

bool LX200AstroPhysicsGTOCP2::Handshake()
{
    if (isSimulation())
    {
        LOG_INFO("Simulated Astrophysics is online. Retrieving basic data...");
        return true;
    }

    int err = 0;

    if ((err = setAPClearBuffer(PortFD)) < 0)
    {
        LOGF_ERROR("Error clearing the buffer (%d): %s", err, strerror(err));
        return false;
    }

    if ((err = setAPBackLashCompensation(PortFD, 0, 0, 0)) < 0)
    {
        // It seems we need to send it twice before it works!
        if ((err = setAPBackLashCompensation(PortFD, 0, 0, 0)) < 0)
        {
            LOGF_ERROR("Error setting back lash compensation (%d): %s.", err, strerror(err));
            return false;
        }
    }

    // Detect and set fomat. It should be LONG.
    return (checkLX200EquatorialFormat(PortFD) == 0);
}

bool LX200AstroPhysicsGTOCP2::Disconnect()
{
    timeUpdated     = false;
    //locationUpdated = false;
    mountInitialized = false;

    return LX200Generic::Disconnect();
}

bool LX200AstroPhysicsGTOCP2::Sync(double ra, double dec)
{
    char syncString[256];

    int syncType = SyncCMRSP.findOnSwitchIndex();

    if (!isSimulation())
    {
        if (setAPObjectRA(PortFD, ra) < 0 || setAPObjectDEC(PortFD, dec) < 0)
        {
            EqNP.setState(IPS_ALERT);
            EqNP.apply("Error setting sync RA/DEC.");
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
                // Should not happen, but handle defensively
                LOG_ERROR("Invalid sync type selected.");
                syncOK = false;
                break;
        }

        if (syncOK == false)
        {
            EqNP.setState(IPS_ALERT);
            EqNP.apply("Synchronization failed.");
            return false;
        }

    }

    currentRA  = ra;
    currentDEC = dec;

    LOGF_DEBUG("%s Synchronization successful %s", (syncType == USE_REGULAR_SYNC ? "CM" : "CMR"), syncString);
    LOG_INFO("Synchronization successful.");

    EqNP.setState(IPS_OK); // Set state to OK after successful sync

    NewRaDec(currentRA, currentDEC); // This should update and apply EqNP

    return true;
}

bool LX200AstroPhysicsGTOCP2::updateTime(ln_date *utc, double utc_offset)
{
    struct ln_zonedate ltm;

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600.0);

    JD = ln_get_julian_day(utc);

    LOGF_DEBUG("New JD is %.2f", JD);

    // Set Local Time
    if (isSimulation() == false && setLocalTime(PortFD, ltm.hours, ltm.minutes, (int)ltm.seconds) < 0)
    {
        LOG_ERROR("Error setting local time.");
        return false;
    }

    LOGF_DEBUG("Set Local Time %02d:%02d:%02d is successful.", ltm.hours, ltm.minutes,
               (int)ltm.seconds);

    if (isSimulation() == false && setCalenderDate(PortFD, ltm.days, ltm.months, ltm.years) < 0)
    {
        LOG_ERROR("Error setting local date.");
        return false;
    }

    LOGF_DEBUG("Set Local Date %02d/%02d/%02d is successful.", ltm.days, ltm.months, ltm.years);

    if (isSimulation() == false && setAPUTCOffset(PortFD, fabs(utc_offset)) < 0)
    {
        LOG_ERROR("Error setting UTC Offset.");
        return false;
    }

    LOGF_DEBUG("Set UTC Offset %g (always positive for AP) is successful.", fabs(utc_offset));

    LOG_INFO("Time updated.");

    timeUpdated = true;

    if (locationUpdated && timeUpdated && mountInitialized == false)
        initMount();

    return true;
}

bool LX200AstroPhysicsGTOCP2::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (!isSimulation() && setAPSiteLongitude(PortFD, 360.0 - longitude) < 0)
    {
        LOG_ERROR("Error setting site longitude coordinates");
        return false;
    }

    if (!isSimulation() && setAPSiteLatitude(PortFD, latitude) < 0)
    {
        LOG_ERROR("Error setting site latitude coordinates");
        return false;
    }

    char l[32], L[32];
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L);

    locationUpdated = true;

    if (locationUpdated && timeUpdated && mountInitialized == false)
        initMount();

    return true;
}

void LX200AstroPhysicsGTOCP2::debugTriggered(bool enable)
{
    LX200Generic::debugTriggered(enable);
    set_lx200ap_name(getDeviceName(), DBG_SCOPE);
}

// For most mounts the SetSlewRate() method sets both the MoveTo and Slew (GOTO) speeds.
// For AP mounts these two speeds are handled separately - so SetSlewRate() actually sets the MoveTo speed for AP mounts - confusing!
// ApSetSlew
// For most mounts the SetSlewRate() method sets both the MoveTo and Slew (GOTO) speeds.
// For AP mounts these two speeds are handled separately - so SetSlewRate() actually sets the MoveTo speed for AP mounts - confusing!
// ApSetSlew
bool LX200AstroPhysicsGTOCP2::SetSlewRate(int index)
{
    if (!isSimulation() && selectAPMoveToRate(PortFD, index) < 0)
    {
        SlewRateSP.setState(IPS_ALERT);
        SlewRateSP.apply("Error setting move rate.");
        return false;
    }

    SlewRateSP.setState(IPS_OK);
    SlewRateSP.apply();
    return true;
}

bool LX200AstroPhysicsGTOCP2::Park()
{
    double parkAz  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    if (isSimulation())
    {
        INDI::IEquatorialCoordinates equatorialCoords {0, 0};
        INDI::IHorizontalCoordinates horizontalCoords {parkAz, parkAlt};
        INDI::HorizontalToEquatorial(&horizontalCoords, &m_Location, ln_get_julian_from_sys(), &equatorialCoords);
        Goto(equatorialCoords.rightascension, equatorialCoords.declination);
    }
    else
    {
        if (setAPObjectAZ(PortFD, parkAz) < 0 || setAPObjectAlt(PortFD, parkAlt) < 0)
        {
            LOG_ERROR("Error setting Az/Alt.");
            return false;
        }

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = Slew(PortFD)))
        {
            LOGF_ERROR("Error Slewing to Az %s - Alt %s", AzStr, AltStr);
            slewError(err);
            return false;
        }

        motionCommanded = true;
        lastAZ = parkAz;
        lastAL = parkAlt;
    }

    EqNP.setState(IPS_BUSY); // Set state to BUSY when parking starts
    EqNP.apply();
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking is in progress...");

    return true;
}

bool LX200AstroPhysicsGTOCP2::UnPark()
{
    // The AP :PO# should only be used during initilization and not here as indicated by email from Preston on 2017-12-12

    // Enable tracking
    SetTrackEnabled(true);

    SetParked(false);

    return true;
}

bool LX200AstroPhysicsGTOCP2::SetCurrentPark()
{
    INDI::IHorizontalCoordinates horizontalPos;
    INDI::IEquatorialCoordinates equatorialPos {currentRA, currentDEC};

    INDI::EquatorialToHorizontal(&equatorialPos, &m_Location, ln_get_julian_from_sys(), &horizontalPos);
    double parkAZ = horizontalPos.azimuth;
    double parkAlt = horizontalPos.altitude;

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)", AzStr, AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool LX200AstroPhysicsGTOCP2::SetDefaultPark()
{
    // Use LocationNP which is the modern property vector
    double currentLatitude = LocationNP[LOCATION_LATITUDE].getValue();
    // Az = 0 for North hemisphere, 180 for South
    SetAxis1ParkDefault(currentLatitude > 0 ? 0 : 180);

    // Alt = Latitude
    SetAxis2ParkDefault(currentLatitude);

    // Apply the defaults to the current park position as well initially
    SetAxis1Park(GetAxis1ParkDefault());
    SetAxis2Park(GetAxis2ParkDefault());

    return true;
}

void LX200AstroPhysicsGTOCP2::syncSideOfPier()
{
    const char *cmd = ":pS#";
    // Response
    char response[16] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return;
    }

    // Read Side
    if ((rc = tty_read_section(PortFD, response, '#', 3, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return;
    }

    response[nbytes_read - 1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    if (!strcmp(response, "East"))
        setPierSide(INDI::Telescope::PIER_EAST);
    else if (!strcmp(response, "West"))
        setPierSide(INDI::Telescope::PIER_WEST);
    else
        LOGF_ERROR("Invalid pier side response from device-> %s", response);

}

bool LX200AstroPhysicsGTOCP2::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    SyncCMRSP.save(fp);
    APSlewSpeedSP.save(fp);
    APGuideSpeedSP.save(fp);

    return true;
}

bool LX200AstroPhysicsGTOCP2::SetTrackMode(uint8_t mode)
{
    int err = 0;

    if (mode == TRACK_CUSTOM)
    {
        if (!isSimulation() && (err = selectAPTrackingMode(PortFD, AP_TRACKING_SIDEREAL)) < 0)
        {
            LOGF_ERROR("Error setting tracking mode to Sidereal for Custom Rate (%d).", err);
            TrackModeSP.setState(IPS_ALERT);
            TrackModeSP.apply();
            return false;
        }
        // Use TrackRateNP now
        return SetTrackRate(TrackRateNP[AXIS_RA].getValue(), TrackRateNP[AXIS_DE].getValue());
    }

    if (!isSimulation() && (err = selectAPTrackingMode(PortFD, mode)) < 0)
    {
        LOGF_ERROR("Error setting tracking mode (%d).", err);
        return false;
    }

    return true;
}

bool LX200AstroPhysicsGTOCP2::SetTrackEnabled(bool enabled)
{
    return SetTrackMode(enabled ? TrackModeSP.findOnSwitchIndex() : AP_TRACKING_OFF);
}

bool LX200AstroPhysicsGTOCP2::SetTrackRate(double raRate, double deRate)
{
    // Convert arcsecs/s to AP sidereal multiplier
    double APRARate = (raRate - TRACKRATE_SIDEREAL) / TRACKRATE_SIDEREAL;
    double APDERate = deRate / TRACKRATE_SIDEREAL;

    // Update the TrackRateNP property values before sending to mount
    TrackRateNP[AXIS_RA].setValue(raRate);
    TrackRateNP[AXIS_DE].setValue(deRate);
    // Don't apply() here as it might trigger unwanted updates, let ReadScopeStatus handle it.

    if (!isSimulation())
    {
        if (setAPRATrackRate(PortFD, APRARate) < 0 || setAPDETrackRate(PortFD, APDERate) < 0)
            return false;
    }

    return true;
}

bool LX200AstroPhysicsGTOCP2::getUTFOffset(double *offset)
{
    return (getAPUTCOffset(PortFD, offset) == 0);
}

bool LX200AstroPhysicsGTOCP2::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    bool rc = LX200Generic::MoveNS(dir, command);

    if (command == MOTION_START)
        motionCommanded = true;

    return rc;
}

bool LX200AstroPhysicsGTOCP2::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    bool rc = LX200Generic::MoveWE(dir, command);

    if (command == MOTION_START)
        motionCommanded = true;

    return rc;
}

void LX200AstroPhysicsGTOCP2::handleGTOCP2MotionBug()
{
    LOGF_DEBUG("%s: Motion commanded? %s", __FUNCTION__, motionCommanded ? "True" : "False");

    // GTOCP2 (Version 'E' and earilar) has a bug that would reset the guide rate to whatever last motion took place
    // So it must be reset to the user setting in order for guiding to work properly.
    if (motionCommanded)
    {
        int guideRateIndex = APGuideSpeedSP.findOnSwitchIndex();
        LOGF_DEBUG("%s: Issuing select guide rate index: %d", __FUNCTION__, guideRateIndex);
        selectAPGuideRate(PortFD, guideRateIndex);
        motionCommanded = false;
    }
}
