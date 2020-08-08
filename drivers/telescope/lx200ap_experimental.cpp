/*
    Astro-Physics INDI driver

    Copyright (C) 2014 Jasem Mutlaq, Mike Fulbright
    Copyright (C) 2020 indilib.org, by Markus Wildi

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
/*
2020-08-07, ToDo
AP commands not yet implemented for revision >= G

Sets the centering rate for the N-S-E-W buttons to xxx Rcxxx#
Default command for an equatorial fork mount, which eliminates the meridian flip :FM#
Default command for A German equatorial mount that includes the meridian flip :EM#
Horizon check during slewing functions :ho# and :hq#
*/



#include "lx200ap_experimental.h"

#include "indicom.h"
#include "lx200driver.h"
#include "lx200apdriver.h"
#include "lx200ap_experimentaldriver.h"
#include "connectionplugins/connectiontcp.h"

#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <unistd.h>
#include <termios.h>


// maximum guide pulse request to send to controller
#define MAX_LX200AP_PULSE_LEN 999

void LX200AstroPhysicsExperimental::disclaimerMessage()
{
    LOG_INFO("This is an _EXPERIMENTAL_ driver for Astro-Physics mounts - use at own risk!");
    LOG_INFO("BEFORE USING PLEASE READ the documentation at:");
    LOG_INFO("   http://indilib.org/devices/telescopes/astrophysics.html");
}

/* Constructor */
LX200AstroPhysicsExperimental::LX200AstroPhysicsExperimental() : LX200Generic()
{
    setLX200Capability(LX200_HAS_PULSE_GUIDING);
    SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_PEC | TELESCOPE_CAN_CONTROL_TRACK
                           | TELESCOPE_HAS_TRACK_RATE, 5);

    // defined in lx200telescope.h, unused in this driver
    //sendLocationOnStartup = false;
    //sendTimeOnStartup = false;

    disclaimerMessage();

}

const char *LX200AstroPhysicsExperimental::getDefaultName()
{
    return "AstroPhysics Experimental";
}

bool LX200AstroPhysicsExperimental::Connect()
{
    Connection::Interface *activeConnection = getActiveConnection();
    if (!activeConnection->name().compare("CONNECTION_TCP"))
    {
        // When using a tcp connection, the GTOCP4 adds trailing LF to response.
        // this small hack will get rid of them as they are not expected in the driver. and generated
        // lot of communication errors.
        tty_clr_trailing_read_lf(1);
    }
    return LX200Generic::Connect();
}

bool LX200AstroPhysicsExperimental::initProperties()
{
    LX200Generic::initProperties();

    timeFormat = LX200_24;

    IUFillSwitch(&StartUpS[0], "COLD", "Cold", ISS_OFF);
    IUFillSwitch(&StartUpS[1], "WARM", "Warm", ISS_OFF);
    IUFillSwitchVector(&StartUpSP, StartUpS, 2, getDeviceName(), "STARTUP", "Startup", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&HourangleCoordsN[0], "HA", "HA H:M:S", "%10.6m", -24., 24., 0., 0.);
    IUFillNumber(&HourangleCoordsN[1], "DEC", "Dec D:M:S", "%10.6m", -90.0, 90.0, 0., 0.);
    IUFillNumberVector(&HourangleCoordsNP, HourangleCoordsN, 2, getDeviceName(), "HOURANGLE_COORD", "Hourangle Coords",
                       MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&HorizontalCoordsN[0], "AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0.);
    IUFillNumber(&HorizontalCoordsN[1], "ALT", "Alt D:M:S", "%10.6m", -90., 90., 0., 0.);
    IUFillNumberVector(&HorizontalCoordsNP, HorizontalCoordsN, 2, getDeviceName(), "HORIZONTAL_COORD",
                       "Horizontal Coords", MAIN_CONTROL_TAB, IP_RW, 120, IPS_IDLE);

    // Max rate is 999.99999X for the GTOCP4.
    // Using :RR998.9999#  just to be safe. 15.041067*998.99999 = 15026.02578
    TrackRateN[AXIS_RA].min = -15026.0258;
    TrackRateN[AXIS_RA].max = 15026.0258;
    TrackRateN[AXIS_DE].min = -998.9999;
    TrackRateN[AXIS_DE].max = 998.9999;

    // Motion speed of axis when pressing NSWE buttons
    IUFillSwitch(&SlewRateS[0], "1", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[1], "12", "12x", ISS_OFF);
    IUFillSwitch(&SlewRateS[2], "64", "64x", ISS_ON);
    IUFillSwitch(&SlewRateS[3], "600", "600x", ISS_OFF);
    IUFillSwitch(&SlewRateS[4], "1200", "1200x", ISS_OFF);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 5, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Slew speed when performing regular GOTO
    IUFillSwitch(&APSlewSpeedS[0], "600", "600x", ISS_ON);
    IUFillSwitch(&APSlewSpeedS[1], "900", "900x", ISS_OFF);
    IUFillSwitch(&APSlewSpeedS[2], "1200", "1200x", ISS_OFF);
    IUFillSwitchVector(&APSlewSpeedSP, APSlewSpeedS, 3, getDeviceName(), "GOTO Rate", "", MOTION_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    IUFillSwitch(&SwapS[0], "NS", "North/South", ISS_OFF);
    IUFillSwitch(&SwapS[1], "EW", "East/West", ISS_OFF);
    IUFillSwitchVector(&SwapSP, SwapS, 2, getDeviceName(), "SWAP", "Swap buttons", MOTION_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    //2020-06-02, wildi, I'm not fond of this CMR sync
    IUFillSwitch(&SyncCMRS[USE_REGULAR_SYNC], ":CM#", ":CM#", ISS_ON);
    IUFillSwitch(&SyncCMRS[USE_CMR_SYNC], ":CMR#", ":CMR#", ISS_OFF);
    IUFillSwitchVector(&SyncCMRSP, SyncCMRS, 2, getDeviceName(), "SYNCCMR", "Sync", MOTION_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // guide speed
    IUFillSwitch(&APGuideSpeedS[0], "0.25", "0.25x", ISS_OFF);
    IUFillSwitch(&APGuideSpeedS[1], "0.5", "0.50x", ISS_OFF);
    IUFillSwitch(&APGuideSpeedS[2], "1.0", "1.0x", ISS_ON);
    IUFillSwitchVector(&APGuideSpeedSP, APGuideSpeedS, 3, getDeviceName(), "Guide Rate", "", GUIDE_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // Unpark from?
    IUFillSwitch(&UnparkFromS[0], "Last", "Last Parked", ISS_OFF);
    IUFillSwitch(&UnparkFromS[1], "Park1", "Park1", ISS_OFF);
    IUFillSwitch(&UnparkFromS[2], "Park2", "Park2", ISS_ON);
    IUFillSwitch(&UnparkFromS[3], "Park3", "Park3", ISS_OFF);
    IUFillSwitch(&UnparkFromS[4], "Park4", "Park4", ISS_OFF);
    IUFillSwitchVector(&UnparkFromSP, UnparkFromS, 5, getDeviceName(), "UNPARK_FROM", "Unpark From?", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // park presets
    IUFillSwitch(&ParkToS[0], "Custom", "Custom", ISS_OFF);
    IUFillSwitch(&ParkToS[1], "Park1", "Park1", ISS_OFF);
    IUFillSwitch(&ParkToS[2], "Park2", "Park2", ISS_ON);
    IUFillSwitch(&ParkToS[3], "Park3", "Park3", ISS_OFF);
    IUFillSwitch(&ParkToS[4], "Park4", "Park4", ISS_OFF);
    IUFillSwitchVector(&ParkToSP, ParkToS, 5, getDeviceName(), "PARK_TO", "Park To?", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillText(&VersionT[0], "Version", "Version", "");
    IUFillTextVector(&VersionInfo, VersionT, 1, getDeviceName(), "Firmware", "Firmware", SITE_TAB, IP_RO, 0, IPS_IDLE);

    // UTC offset
    IUFillNumber(&APUTCOffsetN[0], "APUTC_OFFSET", "AP UTC offset", "%8.5f", 0.0, 24.0, 0.0, 0.);
    IUFillNumberVector(&APUTCOffsetNP, APUTCOffsetN, 1, getDeviceName(), "APUTC_OFFSET", "AP UTC offset", SITE_TAB,
                       IP_RW, 60, IPS_OK);
    // sidereal time, ToDO move define where it belongs to
    IUFillNumber(&APSiderealTimeN[0], "AP_SIDEREAL_TIME", "AP sidereal time", "%10.6m", 0.0, 24.0, 0.0, 0.0);
    IUFillNumberVector(&APSiderealTimeNP, APSiderealTimeN, 1, getDeviceName(), "AP_SIDEREAL_TIME", "ap sidereal time", SITE_TAB,
                       IP_RO, 60, IPS_OK);

    SetParkDataType(PARK_AZ_ALT);

    return true;
}

void LX200AstroPhysicsExperimental::ISGetProperties(const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    defineSwitch(&StartUpSP);

    // MSF 2018/04/10 - disable this behavior for now - we want to have
    //                  UnparkFromSP to always start out as "Last Parked" for safety
    // load config to get unpark from position user wants BEFORE we connect to mount
    //if (!isConnected())
    //{
    //    LOG_DEBUG("Loading unpark from location from config file");
    //    loadConfig(true, UnparkFromSP.name);
    //}

    if (isConnected())
    {
        defineSwitch(&UnparkFromSP);
        defineSwitch(&ParkToSP);
        defineText(&VersionInfo);
        defineSwitch(&APSlewSpeedSP);
        defineSwitch(&SwapSP);
        defineSwitch(&SyncCMRSP);
        defineSwitch(&APGuideSpeedSP);
    }
}

bool LX200AstroPhysicsExperimental::updateProperties()
{
    LX200Generic::updateProperties();

    defineSwitch(&StartUpSP);

    if (isConnected())
    {
        deleteProperty("TELESCOPE_PIER_SIDE");
        if (firmwareVersion < MCV_G)
        {
            deleteProperty(UsePulseCmdSP.name);
            deleteProperty(TrackRateNP.name);
        }
        defineText(&VersionInfo);
        defineSwitch(&UnparkFromSP);
        /* Motion group */
        defineSwitch(&APSlewSpeedSP);
        defineSwitch(&SwapSP);
        defineSwitch(&SyncCMRSP);
        defineSwitch(&APGuideSpeedSP);
        defineSwitch(&ParkToSP);
        defineNumber(&APSiderealTimeNP);
        defineNumber(&HourangleCoordsNP);
        defineNumber(&APUTCOffsetNP);

#ifdef no
        if(!InitPark())
        {
            LOG_WARN("No valid park position found or ParkData.xml missing");
        }
        else
        {
            // wildi: too early, no time, site information
            //loadConfig(true, ParkToSP.name);
            //loadConfig(true, UnparkFromSP.name);
            SetParked(true);
        }

        // 2020-05-30, wildi, this is the wrong place to load config data,
        // see celestron driver.
        // The RA/Dec values are 0,90 at startup and should not be
        // displayed until they are set in UnPark()
        // load in config value for park to and initialize park position
        loadConfig(true, ParkToSP.name);
        ParkPosition parkPos = static_cast<ParkPosition>(IUFindOnSwitchIndex(&ParkToSP));
        LOGF_DEBUG("park position = %d", parkPos);

        // setup location
        double longitude = -1000, latitude = -1000;
        // Get value from config file if it exists.
        IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
        IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
        if (longitude != -1000 && latitude != -1000)
            updateLocation(latitude, longitude, 0);

        //2020-05-30, wildi, LocationN will be overwritten
        // initialize park position
        if (InitPark())
        {
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

        // override with predefined position if selected
        if (parkPos != PARK_CUSTOM)
        {
            double parkAz, parkAlt;
            if (calcParkPosition(parkPos, &parkAlt, &parkAz))
            {
                SetAxis1Park(parkAz);
                SetAxis2Park(parkAlt);
                LOGF_DEBUG("Set park position %d to az=%f alt=%f", parkPos, parkAz, parkAlt);
            }
            else
            {
                LOGF_ERROR("Unable to set park position %d!!", parkPos);
            }
        }
#endif
    }
    else
    {
        deleteProperty(UnparkFromSP.name);
        deleteProperty(VersionInfo.name);
        deleteProperty(APSlewSpeedSP.name);
        deleteProperty(SwapSP.name);
        deleteProperty(SyncCMRSP.name);
        deleteProperty(APGuideSpeedSP.name);
        deleteProperty(ParkToSP.name);
        deleteProperty(APUTCOffsetNP.name);
        deleteProperty(APSiderealTimeNP.name);
        deleteProperty(HourangleCoordsNP.name);
    }

    return true;
}

bool LX200AstroPhysicsExperimental::getFirmwareVersion()
{
    bool success;
    char rev[8];
    char versionString[128];

    success = false;

    if (isSimulation())
        strncpy(versionString, "VCP4-P01-01", 128);
    else
        getAPVersionNumber(PortFD, versionString);

    VersionInfo.s = IPS_OK;
    IUSaveText(&VersionT[0], versionString);
    IDSetText(&VersionInfo, nullptr);

    // Check controller version
    // example "VCP4-P01-01" for CP4 or newer
    //         single or double letter like "T" or "V1" for CP3 and older

    // CP4
    // 2020-06-02, wildi, ToDo unify all versions
    if (strstr(versionString, "VCP4"))
    {
        firmwareVersion = MCV_V;
        servoType = GTOCP4;
        strcpy(rev, "V");
        success = true;
    }
    else if (strlen(versionString) == 1 || strlen(versionString) == 2)
    {
        // Check earlier versions
        // FIXME could probably use better range checking in case we get a letter like 'Z' that doesn't map to anything!
        int typeIndex = VersionT[0].text[0] - 'D';
        if (typeIndex >= 0)
        {
            firmwareVersion = static_cast<ControllerVersion>(typeIndex);
            LOGF_DEBUG("Firmware version index: %d", typeIndex);
            if (firmwareVersion < MCV_G)
                servoType = GTOCP2;
            else
                servoType = GTOCP3;

            strcpy(rev, versionString);

            success = true;
        }
        else
        {
            LOGF_WARN("unknown AP controller version %s", VersionT[0].text);
        }
    }

    if (success)
    {
        LOGF_INFO("Servo Box Controller: GTOCP%d.", servoType);
        LOGF_INFO("Firmware Version: '%s' - %s", rev, versionString + 5);
    }

    return success;
}

#ifdef no
bool LX200AstroPhysicsExperimental::initMount()
{
    // Make sure that the mount is setup according to the properties
    int err = 0;
    if (!IsMountInitialized(&mountInitialized))
    {
        LOG_ERROR("Error determining if mount is initialized!");
        return false;
    }
    if (!IsMountParked(&mountParked))
    {
        return false;
    }
    // 2020-03-17, wildi, AP unpark after the mount is initialized
    // and more importantly, do not do it twice
    if (!mountInitialized)
    {
        LOG_DEBUG("Mount is not yet initialized. Initializing it...");
        if (!isSimulation())
        {
            // This is how to init the mount in case RA/DE are missing.
            // :PO#
            if (APUnParkMount(PortFD) < 0)
            {
                LOG_ERROR("UnParking Failed.");
                return false;
            }

            // Stop :Q#
            abortSlew(PortFD);
        }
    }

    mountInitialized = true;


    // Astrophysics mount is always unparked on startup
    // In this driver, unpark only sets the tracking ON.
    // APParkMount() is NOT called as this function, despite its name, is only used for initialization purposes.
    // 2020-05-22, wildi, that might be too early
    UnPark();
    // On most mounts SlewRateS defines the MoveTo AND Slew (GOTO) speeds
    // lx200ap is different - some of the MoveTo speeds are not VALID
    // Slew speeds so we have to keep two lists.
    //
    // SlewRateS is used as the MoveTo speed
    if (!isSimulation() && (err = selectAPCenterRate(PortFD, IUFindOnSwitchIndex(&SlewRateSP))) < 0)
    {
        LOGF_ERROR("Error setting center (MoveTo) rate (%d).", err);
        return false;
    }

    SlewRateSP.s = IPS_OK;
    IDSetSwitch(&SlewRateSP, nullptr);

    // APSlewSpeedsS defines the Slew (GOTO) speeds valid on the AP mounts
    if (!isSimulation() && (err = selectAPSlewRate(PortFD, IUFindOnSwitchIndex(&APSlewSpeedSP))) < 0)
    {
        LOGF_ERROR("Error setting slew to rate (%d).", err);
        return false;
    }

    APSlewSpeedSP.s = IPS_OK;
    IDSetSwitch(&APSlewSpeedSP, nullptr);

    return true;
}
#endif

/**************************************************************************************
**
***************************************************************************************/
bool LX200AstroPhysicsExperimental::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(getDeviceName(), dev))
        return false;

    if (!strcmp(name, APUTCOffsetNP.name))
    {
        if (IUUpdateNumber(&APUTCOffsetNP, values, names, n) < 0)
            return false;

        float mdelay;
        int err;

        mdelay = APUTCOffsetN[0].value;

        if (!isSimulation() && (err = setAPUTCOffset(PortFD, mdelay) < 0))
        {
            LOGF_ERROR("lx200ap_experimental: Error setting UTC offset (%d).", err);
            return false;
        }

        APUTCOffsetNP.s  = IPS_OK;
        IDSetNumber(&APUTCOffsetNP, nullptr);

        return true;
    }
    if (!strcmp(name, HourangleCoordsNP.name))
    {

        if (IUUpdateNumber(&HourangleCoordsNP, values, names, n) < 0)
            return false;

        double lng = LocationN[LOCATION_LONGITUDE].value;
        double lst = get_local_sidereal_time(lng);
        double ra = lst - HourangleCoordsN[0].value;
        double dec = HourangleCoordsN[1].value;
        bool success = false;
        if ((ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s) || (ISS_ON == IUFindSwitch(&CoordSP, "SLEW")->s))
        {
            success = Goto(ra, dec);
        }
        else
        {
            success = Sync(ra, dec);
        }
        if (success)
        {
            HourangleCoordsNP.s = IPS_OK;
        }
        else
        {
            HourangleCoordsNP.s = IPS_ALERT;
        }
        IDSetNumber(&HourangleCoordsNP,  nullptr);
        return true;
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200AstroPhysicsExperimental::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int err = 0;

    // ignore if not ours //
    if (strcmp(getDeviceName(), dev))
        return false;

    // Reintroduce cold/warm start from lx200ap.cpp in order to check if a valid park position is available.
    // AP's startup sequence is not mandatory with the exception of :PO.
    // Button Startup is defined outside "connected" state only to make it more visible to the user.
    if (!strcmp(name, StartUpSP.name))
    {
        if (!isConnected())
        {
            StartUpSP.s = IPS_ALERT;
            LOG_ERROR("Connect first before reading mount's park configuration");
            IDSetSwitch(&StartUpSP, nullptr);
            return false;
        }
        int switch_nr;

        IUUpdateSwitch(&StartUpSP, states, names, n);

        if (StartUpSP.s == IPS_OK)
        {
            LOG_INFO("Mount's park configuration already read");
            StartUpSP.s = IPS_OK;
            IDSetSwitch(&StartUpSP, nullptr);
            return true;
        }
        // rely on INDI's logic
        if (!(TimeTP.s == IPS_OK && LocationNP.s == IPS_OK))
        {
            StartUpSP.s = IPS_ALERT;
            LOGF_ERROR("Time is %s ok and location is %s ok must be set before mount initialization is invoked.",
                       TimeTP.s == IPS_OK ? "" : "not ", LocationNP.s == IPS_OK ? "" : "not " );
            IDSetSwitch(&StartUpSP, nullptr);
            return false;
        }

        // check only the status of the ParkData.xml, do not set ParkSP
        bool parkDataValid = (LoadParkData() == nullptr);
        if (parkDataValid && isParked())
        {
            if (!loadConfig(true, UnparkFromSP.name))
            {
                LOG_DEBUG("could not load config data for UnparkFromSP.name");
            }
            if (!loadConfig(true, ParkToSP.name))
            {
                LOG_DEBUG("could not load config data for ParkTo.name");
            }

            if(UnparkFromS[PARK_LAST].s == ISS_ON)
            {
                LOGF_INFO("Driver's config 'Unpark From ?' is set to 'Last Parked': will unpark from  Alt=%f Az=%f", GetAxis2Park(),
                          GetAxis1Park());
            }
            // forcing mount being parked from INDI's perspective
            SetParked(true);
        }
        else
        {
            if (StartUpSP.sp[0].s == ISS_ON)
            {
                LOG_WARN("Set 'Park To' and 'Unpark From ?' manually and save dirver's config and park data");
                SetParked(true);

                StartUpSP.s = IPS_OK;
                IDSetSwitch(&StartUpSP, nullptr);
            }
            else
            {
                LOG_WARN("No ParkData.xml or parkstatus is false, do a cold start");
                StartUpSP.s = IPS_ALERT;
                IDSetSwitch(&StartUpSP, nullptr);
            }
            return false;
        }

        if (isSimulation())
        {
            SlewRateSP.s = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);

            APSlewSpeedSP.s = IPS_OK;
            IDSetSwitch(&APSlewSpeedSP, nullptr);

            IUSaveText(&VersionT[0], "1.0");
            VersionInfo.s = IPS_OK;
            IDSetText(&VersionInfo, nullptr);

            StartUpSP.s = IPS_OK;
            IDSetSwitch(&StartUpSP, "Mount initialized.");

            return true;

        }
        // Make sure that the mount is setup according to the properties
        switch_nr = IUFindOnSwitchIndex(&TrackModeSP);

        if ( (err = selectAPTrackingMode(PortFD, switch_nr)) < 0)
        {
            LOGF_ERROR("StartUpSP: Error setting tracking mode (%d).", err);
            return false;
        }

        //TrackState = (switch_nr != AP_TRACKING_OFF) ? SCOPE_TRACKING : SCOPE_IDLE;

        // On most mounts SlewRateS defines the MoveTo AND Slew (GOTO) speeds
        // lx200ap is different - some of the MoveTo speeds are not VALID
        // Slew speeds so we have to keep two lists.
        //
        // SlewRateS is used as the MoveTo speed
        switch_nr = IUFindOnSwitchIndex(&SlewRateSP);
        if ( (err = selectAPMoveToRate(PortFD, switch_nr)) < 0)
        {
            LOGF_ERROR("StartUpSP: Error setting move rate (%d).", err);
            return false;
        }

        SlewRateSP.s = IPS_OK;
        IDSetSwitch(&SlewRateSP, nullptr);

        // APSlewSpeedsS defines the Slew (GOTO) speeds valid on the AP mounts
        switch_nr = IUFindOnSwitchIndex(&APSlewSpeedSP);
        if ( (err = selectAPSlewRate(PortFD, switch_nr)) < 0)
        {
            LOGF_ERROR("StartUpSP: Error setting slew to rate (%d).", err);
            return false;
        }
        APSlewSpeedSP.s = IPS_OK;
        IDSetSwitch(&APSlewSpeedSP, nullptr);

        getLX200RA(PortFD, &currentRA);
        getLX200DEC(PortFD, &currentDEC);

        // make a IDSet in order the dome controller is aware of the initial values
        targetRA  = currentRA;
        targetDEC = currentDEC;

        NewRaDec(currentRA, currentDEC);

        char versionString[64];
        getAPVersionNumber(PortFD, versionString);
        VersionInfo.s = IPS_OK;
        IUSaveText(&VersionT[0], versionString);
        IDSetText(&VersionInfo, nullptr);

        StartUpSP.s = IPS_OK;
        IDSetSwitch(&StartUpSP, "Mount initialized.");

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

        if ((!isSimulation() && (err = swapAPButtons(PortFD, currentSwap)) < 0))
        {
            LOGF_ERROR("Error swapping buttons (%d).", err);
            return false;
        }

        SwapS[0].s = ISS_OFF;
        SwapS[1].s = ISS_OFF;
        SwapSP.s   = IPS_OK;
        IDSetSwitch(&SwapSP, nullptr);
        return true;
    }

    // ===========================================================
    // GOTO ("slew") Speed.
    // ===========================================================
    if (!strcmp(name, APSlewSpeedSP.name))
    {
        IUUpdateSwitch(&APSlewSpeedSP, states, names, n);
        int slewRate = IUFindOnSwitchIndex(&APSlewSpeedSP);

        if (!isSimulation() && (err = selectAPSlewRate(PortFD, slewRate) < 0))
        {
            LOGF_ERROR("Error setting move to rate (%d).", err);
            return false;
        }

        APSlewSpeedSP.s = IPS_OK;
        IDSetSwitch(&APSlewSpeedSP, nullptr);
        return true;
    }

    // ===========================================================
    // Guide Speed.
    // ===========================================================
    if (!strcmp(name, APGuideSpeedSP.name))
    {
        IUUpdateSwitch(&APGuideSpeedSP, states, names, n);
        int guideRate = IUFindOnSwitchIndex(&APGuideSpeedSP);

        if (!isSimulation() && (err = selectAPGuideRate(PortFD, guideRate) < 0))
        {
            LOGF_ERROR("Error setting guiding to rate (%d).", err);
            return false;
        }

        APGuideSpeedSP.s = IPS_OK;
        IDSetSwitch(&APGuideSpeedSP, nullptr);
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

    // =======================================
    // Choose the PEC playback mode
    // =======================================
    if (!strcmp(name, PECStateSP.name))
    {
        IUResetSwitch(&PECStateSP);
        IUUpdateSwitch(&PECStateSP, states, names, n);
        IUFindOnSwitchIndex(&PECStateSP);

        int pecstate = IUFindOnSwitchIndex(&PECStateSP);

        if (!isSimulation() && (err = selectAPPECState(PortFD, pecstate) < 0))
        {
            LOGF_ERROR("Error setting PEC state (%d).", err);
            return false;
        }

        PECStateSP.s = IPS_OK;
        IDSetSwitch(&PECStateSP, nullptr);

        return true;
    }

    // ===========================================================
    // Unpark from positions
    // ===========================================================
    if (!strcmp(name, UnparkFromSP.name))
    {
        IUUpdateSwitch(&UnparkFromSP, states, names, n);
        ParkPosition unparkPos = static_cast<ParkPosition>(IUFindOnSwitchIndex(&UnparkFromSP));

        UnparkFromSP.s = IPS_OK;
        if( unparkPos != PARK_LAST)
        {
            double unparkAlt, unparkAz;
            if (!calcParkPosition(unparkPos, &unparkAlt, &unparkAz))
            {
                LOG_WARN("Error calculating unpark position!");
                UnparkFromSP.s = IPS_ALERT;
            }
            else
            {
                SetAxis1Park(unparkAz);
                SetAxis2Park(unparkAlt);
                // 2020-06-01, wildi, UnPark() relies on it
                saveConfig(true);
            }
        }
        IDSetSwitch(&UnparkFromSP, nullptr);
        return true;
    }

    // ===========================================================
    // Switch Park(ed), Unpark(ed)
    // ===========================================================

    if (!strcmp(name, ParkSP.name))
    {
        if (StartUpSP.s != IPS_OK)
        {
            LOG_ERROR("Read driver's config first");
            return false;
        }
    }
    // ===========================================================
    // Park To positions
    // ===========================================================
    if (!strcmp(name, ParkToSP.name))
    {
        IUUpdateSwitch(&ParkToSP, states, names, n);
        ParkPosition parkPos = static_cast<ParkPosition>(IUFindOnSwitchIndex(&ParkToSP));

        // override with predefined position if selected
        if (parkPos != PARK_CUSTOM)
        {
            double parkAz, parkAlt;
            if (!(TimeTP.s == IPS_OK && LocationNP.s == IPS_OK))
            {
                LOG_WARN("ParkTo can not calculate park position, latitude, longitude not yet available");
                IUResetSwitch(&ParkToSP);
                ParkToSP.s = IPS_ALERT;
                IDSetSwitch(&ParkToSP, nullptr);
                return false;
            }
            if (calcParkPosition(parkPos, &parkAlt, &parkAz))
            {
                SetAxis1Park(parkAz);
                SetAxis2Park(parkAlt);
                LOGF_INFO("Set predefined park position %d to az=%f alt=%f", parkPos, parkAz, parkAlt);
            }
            else
            {
                LOGF_ERROR("Unable to set predefined park position %d!!", parkPos);
            }
        }
        else
        {
            LOG_WARN("ISNewSwitch: park custom not yet supported");
            IUResetSwitch(&ParkToSP);
            ParkToSP.s = IPS_ALERT;
            IDSetSwitch(&ParkToSP, nullptr);
            return false;
        }
        IUResetSwitch(&ParkToSP);
        ParkToS[(int)parkPos].s = ISS_ON;
        ParkToSP.s = IPS_OK;
        IDSetSwitch(&ParkToSP, nullptr);
        return true;
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200AstroPhysicsExperimental::ReadScopeStatus()
{
    double lng = LocationN[LOCATION_LONGITUDE].value;
    double lst = get_local_sidereal_time(lng);
    // 2020-06-02, wildi, isParked is reserved for the state in ParkData.xml
    // see method isParked()
    bool isAPParked ;
    IsMountParked(&isAPParked);
    if (!isAPParked)
    {
        double ha = get_local_hour_angle(lst, currentRA);

        // No need to spam log until we have some actual changes.
        if (std::fabs(HourangleCoordsN[0].value - ha) > 0.0001 ||
                std::fabs(HourangleCoordsN[1].value - currentDEC) > 0.0001)
        {
            // in case of simulation, the coordinates are set on parking
            HourangleCoordsN[0].value = ha;
            HourangleCoordsN[1].value = currentDEC;
            HourangleCoordsNP.s = IPS_OK;
            IDSetNumber(&HourangleCoordsNP, nullptr );
        }
    }
    double val;
    if ((!isSimulation()) && (getSDTime(PortFD, &val) < 0))
    {
        LOG_ERROR("Reading sidereal time failed %d");
        return false;
    }
    if (isSimulation())
    {
        val = lst;
    }
    APSiderealTimeNP.np[0].value = val;
    APSiderealTimeNP.s           = IPS_IDLE;
    IDSetNumber(&APSiderealTimeNP, nullptr);

    if (isSimulation())
    {
        mountSim();
        return true;
    }

    double val_utc_offset;
    if (getAPUTCOffset(PortFD, &val_utc_offset) < 0)
    {
        LOG_ERROR("Error reading UTC Offset.");
        return false;
    }
    if (getLocalTime24(PortFD, &val) < 0)
    {
        LOG_DEBUG("Reading local time failed :GL %d");
    }
    if (firmwareVersion >= MCV_G)
    {
        char buf[64];
        if (getCalendarDate(PortFD, buf) < 0)
        {
            LOG_DEBUG("Reading calendar day failed :GC");
        }
    }
    if (getLX200Az(PortFD, &val) < 0)
    {
        LOG_DEBUG("Reading Az failed :GZ %d");
    }
    if (getLX200Alt(PortFD, &val) < 0)
    {
        LOG_DEBUG("Reading Alt failed :GA %d");
    }
    int ddd = 0;
    int fmm = 0;
    if (getSiteLongitude(PortFD, &ddd, &fmm) < 0)
    {
        LOG_DEBUG("Reading longitude failed :Gg %d");
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
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
        // new way
        char parkStatus;
        char slewStatus;
        bool slewcomplete;
        double PARKTHRES = 0.1; // max difference from parked position to consider mount PARKED

        slewcomplete = false;
        // wildi, downgrade
        if ((firmwareVersion != MCV_UNKNOWN) && (firmwareVersion >= MCV_T))
        {
            if (check_lx200ap_status(PortFD, &parkStatus, &slewStatus) == 0)
            {
                LOGF_DEBUG("parkStatus: %c slewStatus: %c", parkStatus, slewStatus);

                if (slewStatus == '0')
                    slewcomplete = true;
            }
        }
        // old way
        if (getLX200Az(PortFD, &currentAz) < 0 || getLX200Alt(PortFD, &currentAlt) < 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error reading Az/Alt.");
            return false;
        }

        double dx = lastAZ - currentAz;
        double dy = lastAL - currentAlt;

        LOGF_DEBUG("Parking... currentAz: %g dx: %g currentAlt: %g dy: %g", currentAz, dx, currentAlt, dy);

        // if for some reason we check slew status BEFORE park motion starts make sure we dont consider park
        // action complete too early by checking how far from park position we are!
        if (slewcomplete && (dx > PARKTHRES || dy > PARKTHRES))
        {
            LOG_WARN("Parking... slew status indicates mount stopped by dx/dy too far from mount - continuing!");

            slewcomplete = false;
        }

        if (slewcomplete || (dx <= PARKTHRES && dy <= PARKTHRES))
        {
            LOG_DEBUG("Parking slew is complete. Asking astrophysics mount to park...");

            if (APParkMount(PortFD) < 0)
            {
                LOG_ERROR("Parking Failed.");
                return false;
            }

            slewcomplete = true;
            // Turn off tracking.
            SetTrackEnabled(false);
            SetParked(true);
            saveConfig(true);
            // wildi: at this point park data can not be saved: LOG_INFO("Please, save park data, disconnect and power off the mount");
            LOG_INFO("Please, disconnect and power off the mount");
        }

        lastAZ = currentAz;
        lastAL = currentAlt;
    }

    NewRaDec(currentRA, currentDEC);

    syncSideOfPier();

    return true;
}

#ifdef no
// 2020-06-02, wildi, the init is done in UnPArk()
// experimental function needs testing!!!
bool LX200AstroPhysicsExperimental::IsMountInitialized(bool *initialized)
{
    LOG_DEBUG("IsMountInitialized entry");
    if(*initialized)
    {
        LOG_DEBUG("IsMountInitialized  I am initialized");
        return true;
    }
    double ra, dec;
    bool raZE, deZE, de90;

    double epscheck = 1e-5; // two doubles this close are considered equal

    if (isSimulation())
    {
        ra = get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
        dec = LocationN[LOCATION_LATITUDE].value > 0 ? 90 : -90;
    }
    else if (getLX200RA(PortFD, &ra) || getLX200DEC(PortFD, &dec))
    {
        return false;
    }
    LOGF_DEBUG("IsMountInitialized: RA: %f - DEC: %f", ra, dec);

    raZE = (fabs(ra) < epscheck);
    deZE = (fabs(dec) < epscheck);
    de90 = (fabs(dec - 90) < epscheck);

    LOGF_DEBUG("IsMountInitialized: raZE: %s - deZE: %s - de90: %s, (raZE && deZE): %s, (raZE && de90): %s",
               raZE ? "true" : "false", deZE ? "true" : "false", de90 ? "true" : "false", (raZE && deZE) ? "true" : "false", (raZE
                       && de90) ? "true" : "false");

    // RA is zero and DEC is zero or 90
    // then mount is not initialized and we need to initialized it.
    //if ( (raZE && deZE) || (raZE && de90))
    if ( (raZE  || deZE || de90))
    {
        LOG_WARN("IsMountInitialized: Mount is not yet initialized.");
        *initialized = false;
        return true;
    }

    // mount is initialized
    LOG_INFO("IsMountInitialized: Mount is initialized.");
    *initialized = true;
#ifdef no
    LOGF_DEBUG("IsMountInitialized: Mount is initialized,  loc: %s time: %s mount: %s", locationUpdated ? "true" : "false",
               timeUpdated ? "true" : "false", mountInitialized ? "true" : "false");
#endif

    return true;
}
#endif
// experimental function needs testing!!!
bool LX200AstroPhysicsExperimental::IsMountParked(bool *isAPParked)
{
    // 2020-06-02, wildi, ToDo unify for GTOCPX
    if (isSimulation())
    {
        // 2030-05-30, if Unparked is selected, this condition is not met
        *isAPParked = (ParkS[0].s == ISS_ON);
        return true;
    }
    // check for newer
    if ((firmwareVersion != MCV_UNKNOWN) && (firmwareVersion >= MCV_T))
    {
        // try one method
        return getMountStatus(isAPParked);
    }
    const struct timespec timeout = {0, 250000000L};
    double ra1, ra2;
    // fallback for older controllers
    if (getLX200RA(PortFD, &ra1))
        return false;

    // wait 250ms
    nanosleep(&timeout, nullptr);

    if (getLX200RA(PortFD, &ra2))
        return false;

    // if within an arcsec then assume RA is constant
    if (fabs(ra1 - ra2) < (1.0 / (15.0 * 3600.0)))
    {
        *isAPParked = false;
        return true;
    }
    else
    {
        *isAPParked = true;
        return true;
    }

    // can't determine
    LOG_ERROR("IsMountParked: park status undefined");
    return false;
}

bool LX200AstroPhysicsExperimental::getMountStatus(bool *isAPParked)
{
    char parkStatus;
    char slewStatus;

    if (check_lx200ap_status(PortFD, &parkStatus, &slewStatus) == 0)
    {
        LOGF_DEBUG("parkStatus: %c", parkStatus);

        *isAPParked = (parkStatus == 'P');
        return true;
    }

    return false;
}

bool LX200AstroPhysicsExperimental::Goto(double r, double d)
{
    const struct timespec timeout = {0, 100000000L};

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
        nanosleep(&timeout, nullptr);
    }

    if (!isSimulation())
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
#ifdef no
        motionCommanded = true;
#endif
        lastRA = targetRA;
        lastDE = targetDEC;
    }

    TrackState = SCOPE_SLEWING;
    //EqNP.s     = IPS_BUSY;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}


bool LX200AstroPhysicsExperimental::updateAPSlewRate(int index)
{
    //    if (IUFindOnSwitchIndex(&SlewRateSP) == index)
    //    {
    //        LOGF_DEBUG("updateAPSlewRate: slew rate %d already chosen so ignoring.", index);
    //        return true;
    //    }

    if (!isSimulation() && selectAPCenterRate(PortFD, index) < 0)
    {
        SlewRateSP.s = IPS_ALERT;
        IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
        return false;
    }

    IUResetSwitch(&SlewRateSP);
    SlewRateS[index].s = ISS_ON;
    SlewRateSP.s = IPS_OK;
    IDSetSwitch(&SlewRateSP, nullptr);
    return true;
}


// AP mounts handle guide commands differently enough from the "generic" LX200 we need to override some
// functions related to the GuiderInterface

IPState LX200AstroPhysicsExperimental::GuideNorth(uint32_t ms)
{
    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (usePulseCommand && ms <= MAX_LX200AP_PULSE_LEN)
    {
        LOGF_DEBUG("GuideNorth using SendPulseCmd() for duration %d", ms);
        SendPulseCmd(LX200_NORTH, ms);
        GuideNSTID      = IEAddTimer(static_cast<int>(ms), pulseGuideTimeoutHelperNS, this);
    }
    else
    {
        LOGF_DEBUG("GuideNorth using simulated pulse for duration %d", ms);

        if (rememberSlewRate == -1)
            rememberSlewRate = IUFindOnSwitchIndex(&SlewRateSP);

        // Set slew to guiding
        updateAPSlewRate(AP_SLEW_GUIDE);

        // Set to dummy value to that MoveNS does not reset slew rate to rememberSlewRate
        GuideNSTID = 1;

        ISState states[] = { ISS_ON, ISS_OFF };
        const char *names[] = { MovementNSS[DIRECTION_NORTH].name, MovementNSS[DIRECTION_SOUTH].name};
        ISNewSwitch(MovementNSSP.device, MovementNSSP.name, states, const_cast<char **>(names), 2);
        GuideNSTID      = IEAddTimer(static_cast<int>(ms), simulGuideTimeoutHelperNS, this);
    }

    return IPS_BUSY;
}

IPState LX200AstroPhysicsExperimental::GuideSouth(uint32_t ms)
{
    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (usePulseCommand && ms <= MAX_LX200AP_PULSE_LEN)
    {
        SendPulseCmd(LX200_SOUTH, ms);
        GuideNSTID      = IEAddTimer(static_cast<int>(ms), pulseGuideTimeoutHelperNS, this);
    }
    else
    {
        LOGF_DEBUG("GuideSouth using simulated pulse for duration %d", ms);

        if (rememberSlewRate == -1)
            rememberSlewRate = IUFindOnSwitchIndex(&SlewRateSP);

        // Set slew to guiding
        updateAPSlewRate(AP_SLEW_GUIDE);

        // Set to dummy value to that MoveNS does not reset slew rate to rememberSlewRate
        GuideNSTID = 1;

        ISState states[] = { ISS_OFF, ISS_ON };
        const char *names[] = { MovementNSS[DIRECTION_NORTH].name, MovementNSS[DIRECTION_SOUTH].name};
        ISNewSwitch(MovementNSSP.device, MovementNSSP.name, states, const_cast<char **>(names), 2);
        GuideNSTID      = IEAddTimer(static_cast<int>(ms), simulGuideTimeoutHelperNS, this);
    }

    return IPS_BUSY;
}

IPState LX200AstroPhysicsExperimental::GuideEast(uint32_t ms)
{
    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if (usePulseCommand && ms <= MAX_LX200AP_PULSE_LEN)
    {
        SendPulseCmd(LX200_EAST, ms);
        GuideWETID      = IEAddTimer(static_cast<int>(ms), pulseGuideTimeoutHelperWE, this);
    }
    else
    {
        LOGF_DEBUG("GuideEast using simulated pulse for duration %d", ms);

        if (rememberSlewRate == -1)
            rememberSlewRate = IUFindOnSwitchIndex(&SlewRateSP);

        // Set slew to guiding
        updateAPSlewRate(AP_SLEW_GUIDE);

        // Set to dummy value to that MoveWE does not reset slew rate to rememberSlewRate
        GuideWETID = 1;

        ISState states[] = { ISS_OFF, ISS_ON };
        const char *names[] = { MovementWES[DIRECTION_WEST].name, MovementWES[DIRECTION_EAST].name};
        ISNewSwitch(MovementWESP.device, MovementWESP.name, states, const_cast<char **>(names), 2);
        GuideWETID      = IEAddTimer(static_cast<int>(ms), simulGuideTimeoutHelperWE, this);
    }

    return IPS_BUSY;
}

IPState LX200AstroPhysicsExperimental::GuideWest(uint32_t ms)
{
    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if (usePulseCommand && ms <= MAX_LX200AP_PULSE_LEN)
    {
        SendPulseCmd(LX200_WEST, ms);
        GuideWETID      = IEAddTimer(static_cast<int>(ms), pulseGuideTimeoutHelperWE, this);
    }
    else
    {
        LOGF_DEBUG("GuideWest using simulated pulse for duration %d", ms);

        if (rememberSlewRate == -1)
            rememberSlewRate = IUFindOnSwitchIndex(&SlewRateSP);

        // Set slew to guiding
        updateAPSlewRate(AP_SLEW_GUIDE);

        // Set to dummy value to that MoveWE does not reset slew rate to rememberSlewRate
        GuideWETID = 1;

        ISState states[] = { ISS_ON, ISS_OFF };
        const char *names[] = { MovementWES[DIRECTION_WEST].name, MovementWES[DIRECTION_EAST].name};
        ISNewSwitch(MovementWESP.device, MovementWESP.name, states, const_cast<char **>(names), 2);
        GuideWETID      = IEAddTimer(static_cast<int>(ms), simulGuideTimeoutHelperWE, this);
    }

    return IPS_BUSY;
}

void LX200AstroPhysicsExperimental::pulseGuideTimeoutHelperNS(void * p)
{
    static_cast<LX200AstroPhysicsExperimental *>(p)->AstroPhysicsGuideTimeoutNS(false);
}

void LX200AstroPhysicsExperimental::pulseGuideTimeoutHelperWE(void * p)
{
    static_cast<LX200AstroPhysicsExperimental *>(p)->AstroPhysicsGuideTimeoutWE(false);
}

void LX200AstroPhysicsExperimental::simulGuideTimeoutHelperNS(void * p)
{
    static_cast<LX200AstroPhysicsExperimental *>(p)->AstroPhysicsGuideTimeoutNS(true);
}

void LX200AstroPhysicsExperimental::simulGuideTimeoutHelperWE(void * p)
{
    static_cast<LX200AstroPhysicsExperimental *>(p)->AstroPhysicsGuideTimeoutWE(true);
}

void LX200AstroPhysicsExperimental::AstroPhysicsGuideTimeoutWE(bool simul)
{
    LOGF_DEBUG("AstroPhysicsGuideTimeoutWE() pulse guide simul = %d", simul);

    if (simul == true)
    {
        ISState states[] = { ISS_OFF, ISS_OFF };
        const char *names[] = { MovementWES[DIRECTION_WEST].name, MovementWES[DIRECTION_EAST].name};
        ISNewSwitch(MovementWESP.device, MovementWESP.name, states, const_cast<char **>(names), 2);
    }

    GuideWENP.np[DIRECTION_WEST].value = 0;
    GuideWENP.np[DIRECTION_EAST].value = 0;
    GuideWENP.s           = IPS_IDLE;
    GuideWETID            = 0;
    IDSetNumber(&GuideWENP, nullptr);
}

void LX200AstroPhysicsExperimental::AstroPhysicsGuideTimeoutNS(bool simul)
{
    LOGF_DEBUG("AstroPhysicsGuideTimeoutNS() pulse guide simul = %d", simul);

    if (simul == true)
    {
        ISState states[] = { ISS_OFF, ISS_OFF };
        const char *names[] = { MovementNSS[DIRECTION_NORTH].name, MovementNSS[DIRECTION_SOUTH].name};
        ISNewSwitch(MovementNSSP.device, MovementNSSP.name, states, const_cast<char **>(names), 2);
    }

    GuideNSNP.np[0].value = 0;
    GuideNSNP.np[1].value = 0;
    GuideNSNP.s           = IPS_IDLE;
    GuideNSTID            = 0;
    IDSetNumber(&GuideNSNP, nullptr);
}

int LX200AstroPhysicsExperimental::SendPulseCmd(int8_t direction, uint32_t duration_msec)
{
    return APSendPulseCmd(PortFD, direction, duration_msec);
}

bool LX200AstroPhysicsExperimental::Handshake()
{
    if (isSimulation())
    {
        LOG_INFO("Simulated Astrophysics is online. Retrieving basic data...");
        getFirmwareVersion();
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
            // wildi, downgrade
            //return false;
        }
    }

    // get firmware version
    bool rc = false;

    rc = getFirmwareVersion();
#ifdef no
    // see if firmware is 'V' or not
    if (!rc || firmwareVersion == MCV_UNKNOWN || firmwareVersion < MCV_V)
    {
        LOG_ERROR("Firmware version is not 'V' - too old to use the experimental driver!");
        // wildi, downgrade
        //return false;
    }
    else
    {
        LOG_INFO("Firmware level 'V' detected - driver loaded.");
    }
#endif
    if(!rc || firmwareVersion == MCV_UNKNOWN)
    {
        LOG_ERROR("Firmware detection failed or is unknown");
        return false;

    }
    if(firmwareVersion == MCV_V)
    {
        LOG_INFO("Firmware level 'V' detected - driver loaded.");
    }
    else if(firmwareVersion == MCV_D)
    {
        LOG_INFO("Firmware level 'D' detected - driver loaded.");
    }
    // do not track until mount is umparked
    if ((err = selectAPTrackingMode(PortFD, AP_TRACKING_OFF)) < 0)
    {
        LOGF_ERROR("Handshake: Error setting tracking mode to zero (%d).", err);
        return false;

    }
    else
    {
        LOG_INFO("Stopped tracking");
    }

    disclaimerMessage();

    // Detect and set fomat. It should be LONG.
    return (checkLX200Format(PortFD) == 0);
}

bool LX200AstroPhysicsExperimental::Disconnect()
{
#ifdef no
    timeUpdated     = false;
    locationUpdated = false;
    mountInitialized = false;
#endif
    IUResetSwitch(&StartUpSP);
    StartUpS[0].s = ISS_OFF;
    StartUpSP.s = IPS_IDLE;
    IDSetSwitch(&StartUpSP, nullptr);

    return LX200Generic::Disconnect();
}
bool LX200AstroPhysicsExperimental::Sync(double ra, double dec)
{
    char syncString[256] = ""; // simulation needs UTF-8

    int syncType = IUFindOnSwitchIndex(&SyncCMRSP);
    if (!isSimulation())
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

        if (!syncOK)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Synchronization failed.");
            return false;
        }

    }

    currentRA  = ra;
    currentDEC = dec;
    LOGF_DEBUG("%s Synchronization successful %s", (syncType == USE_REGULAR_SYNC ? "CM" : "CMR"), syncString);

    EqNP.s     = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200AstroPhysicsExperimental::updateTime(ln_date *utc, double utc_offset)
{

    LOG_DEBUG("LX200AstroPhysicsExperimental::updateTime entry");
    // 2020-06-02, wildi, ToDo, time obtained from KStars differs up to a couple
    // of 5 seconds from system time.
    struct ln_zonedate ltm;

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600.0);

    JD = ln_get_julian_day(utc);

    LOGF_DEBUG("New JD is %f, local time: %d, %d, %d, utc offset: %f", JD, ltm.hours, ltm.minutes, (int)ltm.seconds,
               utc_offset);

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

    // 2020-05-30, wildi, after a very long journey
    // AP:  TZ (0,12): West, East (-12.,-0), (>12,24)
    // Peru, Lima:
    //(TX=':Gg#'), RX='+77*01:42#
    //(TX=':SG05:00:00#'), RX='1'
    // Linux/Windows TZ values: West: -12,0, East 0,12
    // AP GTOCPX accepts a converted float including 24.
    double ap_utc_offset = - utc_offset;
    if (!isSimulation() && setAPUTCOffset(PortFD, ap_utc_offset) < 0)
    {
        LOG_ERROR("Error setting UTC Offset.");
        return false;
    }
    APUTCOffsetN[0].value = ap_utc_offset ;
    APUTCOffsetNP.s  = IPS_OK;
    IDSetNumber(&APUTCOffsetNP, nullptr);

    LOGF_DEBUG("Set UTC Offset %g as AP UTC Offset %g is successful.", utc_offset, ap_utc_offset);

    LOG_DEBUG("Time updated.");
#ifdef no
    // 2020-06-02, wildi, done in UnPark()
    timeUpdated = true;
    // 2020-05-22, wildi, do it once and at last
    if (locationUpdated && timeUpdated && !mountInitialized)
        initMount();
#endif
    return true;
}

bool LX200AstroPhysicsExperimental::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    LOG_DEBUG("LX200AstroPhysicsExperimental::updateLocation entry");

    if ((latitude == 0.) && (longitude == 0.))
    {
        LOG_DEBUG("updateLocation: latitude, longitude both zero");
        return false;
    }

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

    LOGF_DEBUG("Site location updated to Lat %.32s - Long %.32s, deg: %f, %f", l, L, latitude, longitude);
#ifdef no
    // 2020-06-02, wildi, done in UnPark()
    locationUpdated = true;

    if (locationUpdated && timeUpdated && !mountInitialized)
    {
        if (!initMount())
        {
            return false;
        }
    }
#endif
    return true;
}

void LX200AstroPhysicsExperimental::debugTriggered(bool enable)
{
    LX200Generic::debugTriggered(enable);

    // we use routines from legacy AP driver routines and newer experimental driver routines
    set_lx200ap_name(getDeviceName(), DBG_SCOPE);
    set_lx200ap_exp_name(getDeviceName(), DBG_SCOPE);

}

// For most mounts the SetSlewRate() method sets both the MoveTo and Slew (GOTO) speeds.
// For AP mounts these two speeds are handled separately - so SetSlewRate() actually sets the MoveTo speed for AP mounts - confusing!
// ApSetSlew
bool LX200AstroPhysicsExperimental::SetSlewRate(int index)
{
    if (!isSimulation() && selectAPCenterRate(PortFD, index) < 0)
    {
        LOG_ERROR("Error setting slew mode.");
        return false;
    }

    return true;
}

bool LX200AstroPhysicsExperimental::Park()
{
    LOG_DEBUG("Park entry");

    ParkPosition parkPos = static_cast<ParkPosition>(IUFindOnSwitchIndex(&ParkToSP));
    double parkAz, parkAlt;
    if (calcParkPosition(parkPos, &parkAlt, &parkAz))
    {
        SetAxis1Park(parkAz);
        SetAxis2Park(parkAlt);
        LOGF_DEBUG("Set park position %d to az=%f alt=%f", parkPos, parkAz, parkAlt);
    }
    else
    {
        LOGF_ERROR("Unable to set park position %d!!", parkPos);
    }

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_INFO("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    ln_lnlat_posn observer;
    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;
    if (observer.lng > 180)
        observer.lng -= 360;

    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270
    horizontalPos.az = parkAz + 180;
    if (horizontalPos.az > 360)
        horizontalPos.az -= 360;
    horizontalPos.alt = parkAlt;

    ln_equ_posn equatorialPos;
    ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);
    double lst = get_local_sidereal_time(observer.lng);
    double ha = get_local_hour_angle(lst, equatorialPos.ra / 15.);

    HourangleCoordsNP.s = IPS_OK;
    HourangleCoordsN[0].value = ha;
    HourangleCoordsN[1].value = equatorialPos.dec;
    IDSetNumber(&HourangleCoordsNP, nullptr);

    if (isSimulation())
    {
        Goto(equatorialPos.ra / 15.0, equatorialPos.dec);
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
#ifdef no
        motionCommanded = true;
#endif
        lastAZ = parkAz;
        lastAL = parkAlt;
    }

    EqNP.s     = IPS_BUSY;
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking is in progress...");

    return true;
}

bool LX200AstroPhysicsExperimental::calcParkPosition(ParkPosition pos, double *parkAlt, double *parkAz)
{
    switch (pos)
    {
        // last unparked
        case PARK_CUSTOM:
            LOG_ERROR("Called calcParkPosition with PARK_CUSTOM!");
            return false;
            break;

        // Park 1
        // Northern Hemisphere should be pointing at ALT=0 AZ=0 with scope on WEST side of pier
        // Southern Hemisphere should be pointing at ALT=0 AZ=180 with scope on WEST side of pier
        case 1:
            LOG_INFO("Computing PARK1 position...");
            *parkAlt = 0;
            *parkAz = LocationN[LOCATION_LATITUDE].value > 0 ? 359.1 : 180.1;
            break;

        // Park 2
        // Northern Hemisphere should be pointing at ALT=0 AZ=90 with scope pointing EAST
        // Southern Hemisphere should be pointing at ALT=0 AZ=90 with scope pointing EAST
        case 2:
            LOG_INFO("Computing PARK2 position...");
            *parkAlt = 0;
            *parkAz = 90;
            break;

        // Park 3
        // Northern Hemisphere should be pointing at ALT=LAT AZ=0 with scope pointing NORTH with CW down
        // Southern Hemisphere should be pointing at ALT=LAT AZ=180 with scope pointing SOUTH with CW down
        // wildi: the hour angle is undefined if AZ = 0,180 and ALT=LAT is chosen, adding .1 to Az sets PARK3
        //        as close as possible to to HA = -6 hours (CW down), valid for both hemispheres.
        case 3:
            *parkAlt = fabs(LocationN[LOCATION_LATITUDE].value);
            *parkAz = LocationN[LOCATION_LATITUDE].value > 0 ? 0.1 : 179.9;
            LOG_INFO("Computing PARK3 position");
            break;

        // Park 4
        // Northern Hemisphere should be pointing at ALT=0 AZ=180 with scope on EAST side of pier
        // Southern Hemisphere should be pointing at ALT=0 AZ=0 with scope on EAST side of pier
        case 4:
            LOG_INFO("Computing PARK4 position...");
            *parkAlt = 0;
            *parkAz = LocationN[LOCATION_LATITUDE].value > 0 ? 180.1 : 359.1;
            break;

        default:
            LOG_ERROR("Unknown park position!");
            return false;
            break;
    }

    LOGF_DEBUG("calcParkPosition: parkPos=%d parkAlt=%f parkAz=%f", pos, *parkAlt, *parkAz);

    return true;

}

bool LX200AstroPhysicsExperimental::UnPark()
{
    // 2020-05-30, wildi, NO: if (!(locationUpdated && timeUpdated)) {
    if (!(TimeTP.s == IPS_OK && LocationNP.s == IPS_OK))
    {
        LOG_WARN("UnPark: can not unpark, either missing location or time data");
        //wildi IUResetSwitch(&UnparkFromSP);
        UnparkFromSP.s = IPS_ALERT;
        IDSetSwitch(&UnparkFromSP, nullptr);
        return false;
    }
    bool parkDataValid = InitPark() ;

    bool parkDataValid_and_parked = (parkDataValid && isParked());
    bool unpark_from_last_config = false;

    unpark_from_last_config = (PARK_LAST == IUFindOnSwitchIndex(&UnparkFromSP));
    double unparkAlt, unparkAz;
    if (parkDataValid_and_parked && unpark_from_last_config)
    {

        LOG_INFO("UnPark: mount is parked, has valid park data and driver config is set to Last Parked");
        unparkAz = GetAxis1Park(); //Az
        unparkAlt = GetAxis2Park(); //Alt
    }
    else if(!unpark_from_last_config)
    {
        ParkPosition unparkfromPos = static_cast<ParkPosition>(IUFindOnSwitchIndex(&UnparkFromSP));
        LOGF_DEBUG("UnPark: park position = %d from current driver", unparkfromPos);
        if (!calcParkPosition(unparkfromPos, &unparkAlt, &unparkAz))
        {
            LOG_ERROR("UnPark: Error calculating unpark position!");
            UnparkFromSP.s = IPS_ALERT;
            IDSetSwitch(&UnparkFromSP, nullptr);
            return false;
        }
        LOGF_DEBUG("UnPark: parkPos=%d parkAlt=%f parkAz=%f", unparkfromPos, unparkAlt, unparkAz);
    }
    SetAxis1ParkDefault(unparkAz);
    SetAxis2ParkDefault(unparkAlt);
    if(!parkDataValid)
    {
        SetAxis1ParkDefault(unparkAz);
        SetAxis2ParkDefault(unparkAlt);
    }

    bool isAPParked = true;
    if(!isSimulation())
    {
        if (firmwareVersion == MCV_D)
        {
            // no :GOS command
            // 2020-06-27, wildi, revision D slews without :PO after power cycle
            isAPParked = true ;
        }
        else
        {
            if (!IsMountParked(&isAPParked))
            {
                LOG_WARN("UnPark:could not determine AP park status");
                UnparkFromSP.s = IPS_ALERT;
                IDSetSwitch(&UnparkFromSP, nullptr);
                return false;
            }
        }
        if(!isAPParked)
        {
            LOG_WARN("UnPark: AP mount status: unparked, park first");
            UnparkFromSP.s = IPS_ALERT;
            IDSetSwitch(&UnparkFromSP, nullptr);
            return false;
        }
        // The AP :PO# should only be used during initilization and not here as indicated by email from Preston on 2017-12-12
        // 2020-05-27, wildi, ToDo taking care of above comment later
        if (APUnParkMount(PortFD) < 0)
        {
            IUResetSwitch(&ParkSP);
            ParkS[0].s = ISS_ON;
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, nullptr);
            LOG_ERROR("UnParking AP mount failed.");
            return false;
        }

        SetParked(false);
        // Stop :Q#
        if ( abortSlew(PortFD) < 0)
        {
            IUResetSwitch(&ParkSP);
            ParkS[0].s = ISS_ON;
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, nullptr);
            LOG_WARN("Abort motion Failed");
            return false;
        }
        // NO: Enable tracking
        //2020-03-17, wildi, was SetTrackEnabled(true);
        SetTrackEnabled(false);
        TrackState = SCOPE_IDLE;
    }
    else
    {
        SetParked(false);
        SetTrackEnabled(false);
        TrackState = SCOPE_IDLE;
    }

    ln_lnlat_posn observer;
    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;
    if (observer.lng > 180)
        observer.lng -= 360;

    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270
    horizontalPos.az = unparkAz + 180;
    if (horizontalPos.az > 360)
        horizontalPos.az -= 360;
    horizontalPos.alt = unparkAlt;

    ln_equ_posn equatorialPos;
    ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, unparkAz, 2, 3600);
    fs_sexa(AltStr, unparkAlt, 2, 3600);
    char RaStr[16], DecStr[16];
    fs_sexa(RaStr, equatorialPos.ra / 15., 2, 3600);
    fs_sexa(DecStr, equatorialPos.dec, 2, 3600);

    double lst = get_local_sidereal_time(observer.lng);
    double ha = get_local_hour_angle(lst, equatorialPos.ra / 15.);
    char HaStr[16];
    fs_sexa(HaStr, ha, 2, 3600);
    LOGF_INFO("UnPark: Current parking position Az (%s) Alt (%s), HA (%s) RA (%s) Dec (%s)", AzStr, AltStr, HaStr,
              RaStr, DecStr);

    HourangleCoordsNP.s = IPS_OK;
    HourangleCoordsN[0].value = ha;
    HourangleCoordsN[1].value = equatorialPos.dec;
    IDSetNumber(&HourangleCoordsNP, nullptr);

    bool success = Sync(equatorialPos.ra / 15.0, equatorialPos.dec);
    if(!success)
    {
        LOG_WARN("Could not sync mount");
        return false;
    }
#ifdef no
    if (isSimulation())
    {
        // does not sync being in simulation
        Sync(equatorialPos.ra / 15.0, equatorialPos.dec);
    }
    else
    {
        // 2020-03-17, wildi, why not sync in RA/dec?
        if ((setAPObjectAZ(PortFD, unparkAz) < 0 || (setAPObjectAlt(PortFD, unparkAlt)) < 0))
        {
            LOG_ERROR("Error setting Az/Alt.");
            return false;
        }

        char syncString[256];
        if (APSyncCM(PortFD, syncString) < 0)
        {
            LOG_WARN("Sync failed.");
            return false;
        }
    }
#endif

    if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
    {
        MovementNSSP.s = MovementWESP.s = IPS_IDLE;
        EqNP.s                          = IPS_IDLE;
        IUResetSwitch(&MovementNSSP);
        IUResetSwitch(&MovementWESP);
        IDSetSwitch(&MovementNSSP, nullptr);
        IDSetSwitch(&MovementWESP, nullptr);
    }
    //
#ifdef no
    timeUpdated = false;
    locationUpdated = false;
    //mountInitialized=false;
    if (!IsMountParked(&mountParked))
    {
        return false;
    }
#endif
    UnparkFromSP.s = IPS_OK;
    IDSetSwitch(&UnparkFromSP, nullptr);
    // SlewRateS is used as the MoveTo speed
    int err;
    if (!isSimulation() && (err = selectAPCenterRate(PortFD, IUFindOnSwitchIndex(&SlewRateSP))) < 0)
    {
        LOGF_ERROR("Error setting center (MoveTo) rate (%d).", err);
        return false;
    }

    SlewRateSP.s = IPS_OK;
    IDSetSwitch(&SlewRateSP, nullptr);

    // APSlewSpeedsS defines the Slew (GOTO) speeds valid on the AP mounts
    if (!isSimulation() && (err = selectAPSlewRate(PortFD, IUFindOnSwitchIndex(&APSlewSpeedSP))) < 0)
    {
        LOGF_ERROR("Error setting slew to rate (%d).", err);
        return false;
    }

    APSlewSpeedSP.s = IPS_OK;
    IDSetSwitch(&APSlewSpeedSP, nullptr);

    LOG_DEBUG("UnPark: Mount unparked successfully");
    return true;
}

bool LX200AstroPhysicsExperimental::SetCurrentPark()
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

    LOGF_DEBUG("SetCurrentPark: Setting current parking position to coordinates Az (%s) Alt (%s)", AzStr, AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool LX200AstroPhysicsExperimental::SetDefaultPark()
{
    // Az = 0 for North hemisphere, Az = 180 for South
    SetAxis1Park(LocationN[LOCATION_LATITUDE].value > 0.1 ? 0 : 179.1);

    // Alt = Latitude
    SetAxis2Park(fabs(LocationN[LOCATION_LATITUDE].value));

    return true;
}

void LX200AstroPhysicsExperimental::syncSideOfPier()
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

bool LX200AstroPhysicsExperimental::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &SyncCMRSP);
    IUSaveConfigSwitch(fp, &APSlewSpeedSP);
    IUSaveConfigSwitch(fp, &APGuideSpeedSP);
    IUSaveConfigSwitch(fp, &ParkToSP);
    IUSaveConfigSwitch(fp, &UnparkFromSP);
    IUSaveConfigSwitch(fp, &TrackStateSP);

    return true;
}

bool LX200AstroPhysicsExperimental::SetTrackMode(uint8_t mode)
{
    int err = 0;

    LOGF_DEBUG("LX200AstroPhysicsExperimental::SetTrackMode(%d)", mode);

    if (mode == TRACK_CUSTOM)
    {
        if (!isSimulation() && (err = selectAPTrackingMode(PortFD, AP_TRACKING_SIDEREAL)) < 0)
        {
            LOGF_ERROR("Error setting tracking mode (%d).", err);
            return false;
        }

        return SetTrackRate(TrackRateN[AXIS_RA].value, TrackRateN[AXIS_DE].value);
    }

    if (!isSimulation() && (err = selectAPTrackingMode(PortFD, mode)) < 0)
    {
        LOGF_ERROR("Error setting tracking mode (%d).", err);
        return false;
    }

    return true;
}

bool LX200AstroPhysicsExperimental::SetTrackEnabled(bool enabled)
{
    bool rc;

    LOGF_DEBUG("LX200AstroPhysicsExperimental::SetTrackEnabled(%d)", enabled);

    rc = SetTrackMode(enabled ? IUFindOnSwitchIndex(&TrackModeSP) : AP_TRACKING_OFF);

    LOGF_DEBUG("LX200AstroPhysicsExperimental::SetTrackMode() returned %d", rc);

    return rc;
}

bool LX200AstroPhysicsExperimental::SetTrackRate(double raRate, double deRate)
{
    // Convert to arcsecs/s to AP sidereal multiplier
    /*
    :RR0.0000#      =       normal sidereal tracking in RA - similar to  :RT2#
    :RR+1.0000#     =       1 + normal sidereal     =       2X sidereal
    :RR+9.0000#     =       9 + normal sidereal     =       10X sidereal
    :RR-1.0000#     =       normal sidereal - 1     =       0 or Stop - similar to  :RT9#
    :RR-11.0000#    =       normal sidereal - 11    =       -10X sidereal (East at 10X)

    :RD0.0000#      =       normal zero rate for Dec.
    :RD5.0000#      =       5 + normal zero rate    =       5X sidereal clockwise from above - equivalent to South
    :RD-5.0000#     =       normal zero rate - 5    =       5X sidereal counter-clockwise from above - equivalent to North
    */

    double APRARate = (raRate - TRACKRATE_SIDEREAL) / TRACKRATE_SIDEREAL;
    double APDERate = deRate / TRACKRATE_SIDEREAL;

    if (!isSimulation())
    {
        if (setAPRATrackRate(PortFD, APRARate) < 0 || setAPDETrackRate(PortFD, APDERate) < 0)
            return false;
    }

    return true;
}

bool LX200AstroPhysicsExperimental::getUTFOffset(double *offset)
{
    return (getAPUTCOffset(PortFD, offset) == 0);
}

bool LX200AstroPhysicsExperimental::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    // If we are not guiding and we need to restore slew rate, then let's restore it.
    if (command == MOTION_START && GuideNSTID == 0 && rememberSlewRate >= 0)
    {
        ISState states[] = { ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF };
        states[rememberSlewRate] = ISS_ON;
        const char *names[] = { SlewRateS[0].name, SlewRateS[1].name,
                                SlewRateS[2].name, SlewRateS[3].name
                              };
        ISNewSwitch(SlewRateSP.device, SlewRateSP.name, states, const_cast<char **>(names), 4);
        rememberSlewRate = -1;
    }

    bool rc = LX200Generic::MoveNS(dir, command);
#ifdef no
    if (command == MOTION_START)
        motionCommanded = true;
#endif

    return rc;
}

bool LX200AstroPhysicsExperimental::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    // If we are not guiding and we need to restore slew rate, then let's restore it.
    if (command == MOTION_START && GuideWETID == 0 && rememberSlewRate >= 0)
    {
        ISState states[] = { ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF };
        states[rememberSlewRate] = ISS_ON;
        const char *names[] = { SlewRateS[0].name, SlewRateS[1].name,
                                SlewRateS[2].name, SlewRateS[3].name
                              };
        ISNewSwitch(SlewRateSP.device, SlewRateSP.name, states, const_cast<char **>(names), 4);
        rememberSlewRate = -1;
    }

    bool rc = LX200Generic::MoveWE(dir, command);
#ifdef no
    if (command == MOTION_START)
        motionCommanded = true;
#endif
    return rc;
}

bool LX200AstroPhysicsExperimental::GuideNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    // restore guide rate
    selectAPGuideRate(PortFD, IUFindOnSwitchIndex(&APGuideSpeedSP));

    bool rc = LX200Generic::MoveNS(dir, command);
#ifdef no
    if (command == MOTION_START)
        motionCommanded = true;
#endif
    return rc;
}

bool LX200AstroPhysicsExperimental::GuideWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    // restore guide rate
    selectAPGuideRate(PortFD, IUFindOnSwitchIndex(&APGuideSpeedSP));

    bool rc = LX200Generic::MoveWE(dir, command);
#ifdef no
    if (command == MOTION_START)
        motionCommanded = true;
#endif
    return rc;
}
