#if 0
Celestron GPS
Copyright (C) 2003 - 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

Version with experimental pulse guide support. GC 04.12.2015

#endif

#include "celestrongps.h"

#include "indicom.h"

#include <libnova/transform.h>

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>

#include <sys/stat.h>

#include "indilogger.h"
#include "indiutility.h"

//#include <time.h>

// Simulation Parameters
#define GOTO_RATE       5        // slew rate, degrees/s
#define SLEW_RATE       0.5      // slew rate, degrees/s
#define FINE_SLEW_RATE  0.1      // slew rate, degrees/s
#define GOTO_LIMIT      5.5      // Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees
#define SLEW_LIMIT      1        // Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees
#define FINE_SLEW_LIMIT 0.5      // Move at FINE_SLEW_RATE until distance from target is FINE_SLEW_LIMIT degrees

#define MOUNTINFO_TAB "Mount Info"

static std::unique_ptr<CelestronGPS> telescope(new CelestronGPS());

CelestronGPS::CelestronGPS() : GI(this), FI(this)
{
    setVersion(3, 6); // update libindi/drivers.xml as well


    fwInfo.Version           = "Invalid";
    fwInfo.controllerVersion = 0;
    fwInfo.controllerVariant = ISNEXSTAR;
    fwInfo.isGem = false;
    fwInfo.hasFocuser = false;

    INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    currentRA  = 0;
    currentDEC = 90;
    currentAZ  = 0;
    currentALT = 0;
    targetAZ   = 0;
    targetALT  = 0;

    // focuser
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_HAS_BACKLASH);

    focusTrueMax = 0;
    focusTrueMin = 0xffffffff;

    // Set minimum properties.
    // ISGetProperties in INDI::Telescope checks for CanGOTO which must be set.
    SetTelescopeCapability(TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT, 9);
}

bool CelestronGPS::checkMinVersion(double minVersion, const char *feature, bool debug)
{
    if (((fwInfo.controllerVariant == ISSTARSENSE) &&
            (fwInfo.controllerVersion < MINSTSENSVER)) ||
            ((fwInfo.controllerVariant == ISNEXSTAR) &&
             (fwInfo.controllerVersion < minVersion)))
    {
        if (debug)
            LOGF_DEBUG("Firmware v%3.2f does not support %s. Minimum required version is %3.2f",
                       fwInfo.controllerVersion, feature, minVersion);
        else
            LOGF_WARN("Firmware v%3.2f does not support %s. Minimum required version is %3.2f",
                      fwInfo.controllerVersion, feature, minVersion);

        return false;
    }
    return true;
}

const char *CelestronGPS::getDefaultName()
{
    return "Celestron GPS";
}

bool CelestronGPS::initProperties()
{
    INDI::Telescope::initProperties();
    FI::initProperties(FOCUS_TAB);

    // Firmware
    IUFillText(&FirmwareT[FW_MODEL], "Model", "", nullptr);
    IUFillText(&FirmwareT[FW_VERSION], "HC Version", "", nullptr);
    IUFillText(&FirmwareT[FW_RA], "Ra Version", "", nullptr);
    IUFillText(&FirmwareT[FW_DEC], "Dec Version", "", nullptr);
    IUFillText(&FirmwareT[FW_ISGEM], "Mount Type", "", nullptr);
    IUFillText(&FirmwareT[FW_CAN_AUX], "Guide Method", "", nullptr);
    IUFillText(&FirmwareT[FW_HAS_FOC], "Has Focuser", "", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 7, getDeviceName(), "Firmware Info", "", MOUNTINFO_TAB, IP_RO, 0,
                     IPS_IDLE);

    // Celestron Track Modes are Off, AltAz, EQ N, EQ S and Ra and Dec (StarSense only)
    // off is not provided as these are used to set the track mode when tracking is enabled
    // may be required for set up, value will be read from the mount if possible
    IUFillSwitchVector(&CelestronTrackModeSP, CelestronTrackModeS, 4, getDeviceName(), "CELESTRON_TRACK_MODE", "Track Mode",
                       MOUNTINFO_TAB, IP_RO, ISR_1OFMANY, 0, IPS_IDLE);
    IUFillSwitch(&CelestronTrackModeS[0], "MODE_ALTAZ", "Alt Az", ISS_OFF);
    IUFillSwitch(&CelestronTrackModeS[1], "MODE_EQ_N", "EQ N", ISS_ON);
    IUFillSwitch(&CelestronTrackModeS[2], "MODE_EQ_S", "EQ S", ISS_OFF);
    IUFillSwitch(&CelestronTrackModeS[3], "MODE_RA_DEC", "Ra and Dec", ISS_OFF);

    // INDI track modes are sidereal, solar and lunar
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");

    IUFillSwitch(&UseHibernateS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&UseHibernateS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&UseHibernateSP, UseHibernateS, 2, getDeviceName(), "Hibernate", "", OPTIONS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    //GUIDE Define "Use Pulse Cmd" property (Switch).
    //    IUFillSwitch(&UsePulseCmdS[0], "Off", "", ISS_OFF);
    //    IUFillSwitch(&UsePulseCmdS[1], "On", "", ISS_ON);
    //    IUFillSwitchVector(&UsePulseCmdSP, UsePulseCmdS, 2, getDeviceName(), "Use Pulse Cmd", "", MAIN_CONTROL_TAB, IP_RW,
    //                       ISR_1OFMANY, 0, IPS_IDLE);

    // experimental last align control
    IUFillSwitchVector(&LastAlignSP, LastAlignS, 1, getDeviceName(), "Align", "Align", MAIN_CONTROL_TAB,
                       IP_WO, ISR_1OFMANY, 0, IPS_IDLE);
    IUFillSwitch(&LastAlignS[0], "Align", "Align", ISS_OFF);
    // maybe a second switch which confirms the align

    SetParkDataType(PARK_AZ_ALT);

    //GUIDE Initialize guiding properties.
    GI::initProperties(GUIDE_TAB);

    //////////////////////////////////////////////////////////////////////////////////////////////////
    /// Guide Rate; units and min/max as specified in the INDI Standard Properties SLEW_GUIDE
    /// https://indilib.org/developers/developer-manual/101-standard-properties.html#h3-telescopes
    //////////////////////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&GuideRateN[AXIS_RA], "GUIDE_RATE_WE", "W/E Rate", "%0.2f", 0, 1, 0.1, GuideRateN[AXIS_RA].value);
    IUFillNumber(&GuideRateN[AXIS_DE], "GUIDE_RATE_NS", "N/S Rate", "%0.2f", 0, 1, 0.1, GuideRateN[AXIS_DE].value);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guide Rate x sidereal", GUIDE_TAB, IP_RW, 0,
                       IPS_IDLE);

    ////////////////////////////////////////////////////////////////////////////////////////
    /// PEC
    /////////////////////////////////////////////////////////////////////////////////////////

    IUFillSwitch(&PecControlS[PEC_Seek], "PEC_SEEK_INDEX", "Seek Index", ISS_OFF);
    IUFillSwitch(&PecControlS[PEC_Stop], "PEC_STOP", "Stop", ISS_OFF);
    IUFillSwitch(&PecControlS[PEC_Playback], "PEC_PLAYBACK", "Playback", ISS_OFF);
    IUFillSwitch(&PecControlS[PEC_Record], "PEC_RECORD", "Record", ISS_OFF);
    IUFillSwitchVector(&PecControlSP, PecControlS, 4, getDeviceName(), "PEC_CONTROL", "PEC Control", MOTION_TAB, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE);

    IUFillText(&PecInfoT[0], "PEC_STATE", "Pec State", "undefined");
    IUFillText(&PecInfoT[1], "PEC_INDEX", "Pec Index", " ");
    IUFillTextVector(&PecInfoTP, PecInfoT, 2, getDeviceName(), "PEC_INFO", "Pec Info", MOTION_TAB, IP_RO,  60, IPS_IDLE);

    // load Pec data from file
    IUFillText(&PecFileNameT[0], "PEC_FILE_NAME", "File Name", "");
    IUFillTextVector(&PecFileNameTP, PecFileNameT, 1, getDeviceName(), "PEC_LOAD", "Load PEC", MOTION_TAB, IP_WO, 60, IPS_IDLE);

    /////////////////////////////
    /// DST setting
    /////////////////////////////

    IUFillSwitch(&DSTSettingS[0], "DST_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitchVector(&DSTSettingSP, DSTSettingS, 1, getDeviceName(), "DST_STATE", "DST", SITE_TAB, IP_RW, ISR_NOFMANY, 60,
                       IPS_IDLE);

    addAuxControls();

    //GUIDE Set guider interface.
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    //FocuserInterface
    //Initial, these will be updated later.
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(30000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1000);
    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(60000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);

    // Maximum Position Settings, will be read from the hardware
    FocusMaxPosNP[0].setMax(60000);
    FocusMaxPosNP[0].setMin(1000);
    FocusMaxPosNP[0].setValue(60000);
    FocusMaxPosNP.setPermission(IP_RO);

    // Focuser backlash
    // CR this is a value, positive or negative to define the direction.  It is implemented
    // in the driver.

    FocusBacklashNP[0].setMin(-1000);
    FocusBacklashNP[0].setMax(1000);
    FocusBacklashNP[0].setStep(1);
    FocusBacklashNP[0].setValue(0);
    //    IUFillNumber(&FocusBacklashN[0], "STEPS", "Steps", "%.f", -500., 500, 1., 0.);
    //    IUFillNumberVector(&FocusBacklashNP, FocusBacklashN, 1, getDeviceName(), "FOCUS_BACKLASH", "Backlash",
    //                       FOCUS_TAB, IP_RW, 0, IPS_IDLE);

    // Focuser min limit, read from the hardware
    // IUFillNumber(&FocusMinPosN[0], "FOCUS_MIN_VALUE", "Steps", "%.f", 0, 40000., 1., 0.);
    // IUFillNumberVector(&FocusMinPosNP, FocusMinPosN, 1, getDeviceName(), "FOCUS_MIN", "Min. Position",
    //                    FOCUS_TAB, IP_RO, 0, IPS_IDLE);

    return true;
}

void CelestronGPS::ISGetProperties(const char *dev)
{
    static bool configLoaded = false;

    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    INDI::Telescope::ISGetProperties(dev);

    defineProperty(&UseHibernateSP);
    defineProperty(&CelestronTrackModeSP);
    if (configLoaded == false)
    {
        configLoaded = true;
        loadConfig(true, "Hibernate");
    }
}

bool CelestronGPS::updateProperties()
{
    if (isConnected())
    {
        uint32_t cap = TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT;

        if (driver.get_firmware(&fwInfo))
        {
            IUSaveText(&FirmwareT[FW_MODEL], fwInfo.Model.c_str());
            IUSaveText(&FirmwareT[FW_VERSION], fwInfo.Version.c_str());
            IUSaveText(&FirmwareT[FW_RA], fwInfo.RAFirmware.c_str());
            IUSaveText(&FirmwareT[FW_DEC], fwInfo.DEFirmware.c_str());
            IUSaveText(&FirmwareT[FW_ISGEM], fwInfo.isGem ? "GEM" : "Fork");
            canAuxGuide = (atof(fwInfo.RAFirmware.c_str()) >= 6.12 && atof(fwInfo.DEFirmware.c_str()) >= 6.12);
            IUSaveText(&FirmwareT[FW_CAN_AUX], canAuxGuide ? "Mount" : "Time Guide");
            IUSaveText(&FirmwareT[FW_HAS_FOC], fwInfo.hasFocuser ? "True" : "False");

            if (!fwInfo.isGem)
            {
                MountTypeSP.reset();
                MountTypeSP[MOUNT_EQ_FORK].setState(ISS_ON);
            }

            usePreciseCoords = (checkMinVersion(2.2, "usePreciseCoords"));
            // set the default switch index, will be updated from the mount if possible
            fwInfo.celestronTrackMode = static_cast<CELESTRON_TRACK_MODE>(IUFindOnSwitchIndex(&CelestronTrackModeSP) + 1);
        }
        else
        {
            fwInfo.Version = "Invalid";
            LOG_WARN("Failed to retrieve firmware information.");
        }

        // JM 2018-09-28: According to user reports in this thread:
        // http://www.indilib.org/forum/mounts/2208-celestron-avx-mount-and-starsense.html
        // Parking is also supported fine with StarSense
        if (checkMinVersion(2.3, "park"))
            cap |= TELESCOPE_CAN_PARK;

        if (checkMinVersion(4.1, "sync"))
            cap |= TELESCOPE_CAN_SYNC;

        if (checkMinVersion(2.3, "updating time and location settings"))
        {
            cap |= TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION;
        }

        // changing track mode (aka rate) is only available for equatorial mounts

        // StarSense supports track mode
        if (checkMinVersion(2.3, "track on/off"))
            cap |= TELESCOPE_CAN_CONTROL_TRACK;
        else
            LOG_WARN("Mount firmware does not support track on off.");

        if (fwInfo.isGem  && checkMinVersion(4.15, "Pier Side", true))
            cap |= TELESCOPE_HAS_PIER_SIDE;
        else
            LOG_WARN("Mount firmware does not support getting pier side.");

        // Track Mode (t) is only supported for 2.3+
        CELESTRON_TRACK_MODE ctm = CTM_OFF;
        if (checkMinVersion(2.3, "track mode"))
        {
            if (isSimulation())
            {
                if (isParked())
                    driver.set_sim_track_mode(CTM_OFF);
                else
                    driver.set_sim_track_mode(CTM_EQN);
            }
            if (driver.get_track_mode(&ctm))
            {
                if (ctm != CTM_OFF)
                {
                    fwInfo.celestronTrackMode = ctm;
                    IUResetSwitch(&CelestronTrackModeSP);
                    CelestronTrackModeS[ctm - 1].s = ISS_ON;
                    CelestronTrackModeSP.s      = IPS_OK;

                    saveConfig(true, "CELESTRON_TRACK_MODE");
                    LOGF_DEBUG("Celestron mount tracking, mode %s", CelestronTrackModeS[ctm - 1].label);
                }
                else
                {
                    LOG_INFO("Mount tracking is off.");
                    TrackState = isParked() ? SCOPE_PARKED : SCOPE_IDLE;
                }
            }
            else
            {
                LOG_DEBUG("get_track_mode failed");
                CelestronTrackModeSP.s = IPS_ALERT;
            }

            IDSetSwitch(&CelestronTrackModeSP, nullptr);
        }

        // JM 2025.09.19: Celestron Track Mode (EQ-N, EQ-S, ALTAZ) is not related
        // to TELESCOPE_HAS_TRACK_MODE which is about track *rates* like Lunar, Solar, Sidereal..etc
        // if (fwInfo.celestronTrackMode != CTM_ALTAZ)
        //     cap |= TELESCOPE_HAS_TRACK_MODE;
        // else
        // {
        //     TrackModeSP[TRACK_SIDEREAL].setState(ISS_ON);
        //     LOG_WARN("Mount firmware does not support track mode.");
        // }

        SetTelescopeCapability(cap, 9);

        INDI::Telescope::updateProperties();

        if (fwInfo.Version != "Invalid")
            defineProperty(&FirmwareTP);

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].getValue());
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
            SetAxis2Park(LocationNP[LOCATION_LATITUDE].getValue());
            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].getValue());
        }

        // InitPark sets TrackState to IDLE or PARKED so this is the earliest we can
        // update TrackState using the current mount properties
        // Something seems to set IsParked to true, force the correct state if the
        // mount is tracking
        if (ctm != CTM_OFF)
        {
            SetParked(false);
            TrackState = SCOPE_TRACKING;
        }

        //GUIDE Update properties.
        // check if the mount type and version supports guiding
        // Only show the guide information for mounts that
        // support guiding.  That's GEMs and fork mounts in equatorial modes.
        // well, anything in an equatorial mode
        if (fwInfo.celestronTrackMode == CELESTRON_TRACK_MODE::CTM_EQN ||
                fwInfo.celestronTrackMode == CELESTRON_TRACK_MODE::CTM_EQS ||
                fwInfo.celestronTrackMode == CELESTRON_TRACK_MODE::CTM_RADEC)
        {
            defineProperty(&GuideRateNP);
            uint8_t rate;
            if (driver.get_guide_rate(CELESTRON_AXIS::RA_AXIS, &rate))
            {
                GuideRateN[AXIS_RA].value = std::min(std::max(static_cast<double>(rate) / 255.0, 0.0), 1.0);
                LOGF_DEBUG("Get Guide Rate: RA %f", GuideRateN[AXIS_RA].value);
                if (driver.get_guide_rate(CELESTRON_AXIS::DEC_AXIS, &rate))
                {
                    GuideRateN[AXIS_DE].value = std::min(std::max(static_cast<double>(rate) / 255.0, 0.0), 1.0);
                    IDSetNumber(&GuideRateNP, nullptr);
                    LOGF_DEBUG("Get Guide Rate: Dec %f", GuideRateN[AXIS_DE].value);
                }
            }
            else
                LOG_DEBUG("Unable to get guide rates from mount.");

            GI::updateProperties();

            LOG_INFO("Mount supports guiding.");
        }
        else
            LOG_INFO("Mount does not support guiding. Tracking mode must be set in handset to either EQ-North or EQ-South.");


        defineProperty(&CelestronTrackModeSP);

        // JM 2014-04-14: User (davidw) reported AVX mount serial communication times out issuing "h" command with firmware 5.28
        // JM 2018-09-27: User (suramara) reports that it works with AVX mount with Star Sense firmware version 1.19
        //if (fwInfo.controllerVersion >= 2.3 && fwInfo.Model != "AVX" && fwInfo.Model != "CGE Pro")
        if (checkMinVersion(2.3, "date and time setting"))
        {
            double utc_offset;
            int yy, dd, mm, hh, minute, ss;
            bool dst;
            // StarSense doesn't seems to handle the precise time commands
            bool precise = fwInfo.controllerVersion >= 5.28;
            if (driver.get_utc_date_time(&utc_offset, &yy, &mm, &dd, &hh, &minute, &ss, &dst, precise))
            {
                char isoDateTime[32];
                char utcOffset[8];

                snprintf(isoDateTime, 32, "%04d-%02d-%02dT%02d:%02d:%02d", yy, mm, dd, hh, minute, ss);
                snprintf(utcOffset, 8, "%4.2f", utc_offset);

                TimeTP[UTC].setText(isoDateTime);
                TimeTP[OFFSET].setText(utcOffset);

                defineProperty(&DSTSettingSP);
                DSTSettingS[0].s = dst ? ISS_ON : ISS_OFF;

                LOGF_INFO("Mount UTC offset: %s. UTC time: %s. DST: %s", utcOffset, isoDateTime, dst ? "On" : "Off");
                //LOGF_DEBUG("Mount UTC offset is %s. UTC time is %s", utcOffset, isoDateTime);

                TimeTP.setState(IPS_OK);
                TimeTP.apply();
                IDSetSwitch(&DSTSettingSP, nullptr);
            }
            double longitude, latitude;
            if (driver.get_location(&longitude, &latitude))
            {
                LocationNP[LOCATION_LATITUDE].setValue(latitude);
                LocationNP[LOCATION_LONGITUDE].setValue(longitude);
                LocationNP[LOCATION_ELEVATION].setValue(0);
                LocationNP.setState(IPS_OK);
                LOGF_DEBUG("Mount latitude %8.4f longitude %8.4f", latitude, longitude);
            }
        }
        else
            LOG_WARN("Mount does not support retrieval of date, time and location.");

        // last align is only available for mounts with switches that define the start index position
        // At present that is only the CGX and CGX-L mounts so the control is only made available for them
        // comment out this line and rebuild if you want to run with other mounts - at your own risk!
        if (fwInfo.hasHomeIndex)
        {
            defineProperty(&LastAlignSP);
        }

        // Sometimes users start their mount when it is NOT yet aligned and then try to proceed to use it
        // So we check issue and issue error if not aligned.
        checkAlignment();

        // PEC, must have PEC index and be equatorially mounted
        if (fwInfo.canPec && CelestronTrackModeS[CTM_ALTAZ].s != ISS_OFF)
        {
            driver.pecState = PEC_STATE::PEC_AVAILABLE;
            defineProperty(&PecControlSP);
            defineProperty(&PecInfoTP);
            defineProperty(&PecFileNameTP);
        }

        //  handle the focuser
        if (fwInfo.hasFocuser)
        {
            //defineProperty(&FocusBacklashNP);
            //defineProperty(&FocusMinPosNP);

            if (focusReadLimits())
            {
                FocusAbsPosNP.updateMinMax();

                FocusMaxPosNP.apply();
                // IDSetNumber(&FocusMinPosNP, nullptr);
                // focuser move capability is only set if the focus limits are valid
                FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
                setDriverInterface(getDriverInterface() | FOCUSER_INTERFACE);
                syncDriverInfo();

                LOG_INFO("Auxiliary focuser is connected.");
            }
            if (!focuserIsCalibrated)
            {
                LOG_WARN("Focuser not calibrated, moves will not be allowed");
            }
            FI::updateProperties();
        }
    }
    else    // not connected
    {
        INDI::Telescope::updateProperties();

        FI::updateProperties();
        //deleteProperty(FocusMinPosNP.name);

        //GUIDE Delete properties.
        GI::updateProperties();

        deleteProperty(GuideRateNP.name);

        deleteProperty(LastAlignSP.name);
        deleteProperty(CelestronTrackModeSP.name);

        deleteProperty(DSTSettingSP.name);

        deleteProperty(PecInfoTP.name);
        deleteProperty(PecControlSP.name);
        deleteProperty(PecFileNameTP.name);

        if (fwInfo.Version != "Invalid")
            deleteProperty(FirmwareTP.name);
    }

    return true;
}

bool CelestronGPS::Goto(double ra, double dec)
{
    targetRA  = ra;
    targetDEC = dec;

    if (EqNP.getState() == IPS_BUSY || MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
    {
        driver.abort();
        // sleep for 500 mseconds
        usleep(500000);
    }

    if (driver.slew_radec(targetRA + SlewOffsetRa, targetDEC, usePreciseCoords) == false)
    {
        LOG_ERROR("Failed to slew telescope in RA/DEC.");
        return false;
    }

    TrackState = SCOPE_SLEWING;

    char RAStr[32], DecStr[32];
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);
    LOGF_INFO("Slewing to JNOW RA %s - DEC %s SlewOffsetRa %4.1f arcsec", RAStr, DecStr, SlewOffsetRa * 3600 * 15);

    return true;
}

bool CelestronGPS::Sync(double ra, double dec)
{
    if (!checkMinVersion(4.1, "sync"))
        return false;

    if (driver.sync(ra, dec, usePreciseCoords) == false)
    {
        LOG_ERROR("Sync failed.");
        return false;
    }

    currentRA  = ra;
    currentDEC = dec;

    char RAStr[32], DecStr[32];
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);
    LOGF_INFO("Sync to %s, %s successful.", RAStr, DecStr);

    return true;
}

/*
bool CelestronGPS::GotoAzAlt(double az, double alt)
{
    if (isSimulation())
    {
        INDI::IHorizontalCoordinates horizontalPos;
        // Libnova south = 0, west = 90, north = 180, east = 270
        horizontalPos.az = az + 180;
        if (horizontalPos.az >= 360)
             horizontalPos.az -= 360;
        horizontalPos.alt = alt;

        IGeographicCoordinates observer;

        observer.lat = LocationN[LOCATION_LATITUDE].value;
        observer.lng = LocationN[LOCATION_LONGITUDE].value;

        if (observer.lng > 180)
            observer.lng -= 360;

        INDI::IEquatorialCoordinates equatorialPos;
        ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

        targetRA  = equatorialPos.rightascension/15.0;
        targetDEC = equatorialPos.dec;
    }

    if (driver.slew_azalt(LocationN[LOCATION_LATITUDE].value, az, alt) == false)
    {
        LOG_ERROR("Failed to slew telescope in Az/Alt.");
        return false;
    }

    targetAZ = az;
    targetALT= alt;

    TrackState = SCOPE_SLEWING;

    HorizontalCoordsNP.s = IPS_BUSY;

    char AZStr[16], ALTStr[16];
    fs_sexa(AZStr, targetAZ, 3, 3600);
    fs_sexa(ALTStr, targetALT, 2, 3600);
    LOGF_INFO("Slewing to Az %s - Alt %s", AZStr, ALTStr);

    return true;
}
*/

bool CelestronGPS::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    CELESTRON_DIRECTION move;

    if (currentPierSide == PIER_WEST)
        move = (dir == DIRECTION_NORTH) ? CELESTRON_N : CELESTRON_S;
    else
        move = (dir == DIRECTION_NORTH) ? CELESTRON_S : CELESTRON_N;

    CELESTRON_SLEW_RATE rate = static_cast<CELESTRON_SLEW_RATE>(SlewRateSP.findOnSwitchIndex());

    switch (command)
    {
        case MOTION_START:
            if (driver.start_motion(move, rate) == false)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (move == CELESTRON_N) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (driver.stop_motion(move) == false)
            {
                LOG_ERROR("Error stopping N/S motion.");
                return false;
            }
            else
                LOGF_INFO("Movement toward %s halted.", (move == CELESTRON_N) ? "North" : "South");
            break;
    }

    return true;
}

bool CelestronGPS::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    CELESTRON_DIRECTION move = (dir == DIRECTION_WEST) ? CELESTRON_W : CELESTRON_E;
    CELESTRON_SLEW_RATE rate = static_cast<CELESTRON_SLEW_RATE>(SlewRateSP.findOnSwitchIndex());

    switch (command)
    {
        case MOTION_START:
            if (driver.start_motion(move, rate) == false)
            {
                LOG_ERROR("Error setting W/E motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (move == CELESTRON_W) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (driver.stop_motion(move) == false)
            {
                LOG_ERROR("Error stopping W/E motion.");
                return false;
            }
            else
                LOGF_INFO("Movement toward %s halted.", (move == CELESTRON_W) ? "West" : "East");
            break;
    }

    return true;
}

bool CelestronGPS::ReadScopeStatus()
{
    INDI::Telescope::TelescopePierSide pierSide = PIER_UNKNOWN;

    if (isSimulation())
        mountSim();

    if (driver.get_radec(&currentRA, &currentDEC, usePreciseCoords) == false)
    {
        LOG_ERROR("Failed to read RA/DEC values.");
        return false;
    }

    if (HasPierSide())
    {
        // read the pier side close to reading the Radec so they should match
        char sop = '?';
        char psc = 'u';
        if (driver.get_pier_side(&sop))
        {
            // manage version and hemisphere nonsense
            // HC versions less than 5.24 reverse the side of pier if the mount
            // is in the Southern hemisphere.  StarSense doesn't
            if (LocationNP[LOCATION_LATITUDE].getValue() < 0)
            {
                if (fwInfo.controllerVersion <= 5.24 && fwInfo.controllerVariant != ISSTARSENSE)
                {
                    // swap the char reported
                    if (sop == 'E')
                        sop = 'W';
                    else if (sop == 'W')
                        sop = 'E';
                }
            }
            // The Celestron and INDI pointing states are opposite
            if (sop == 'W')
            {
                pierSide = PIER_EAST;
                psc = 'E';
            }
            else if (sop == 'E')
            {
                pierSide = PIER_WEST;
                psc = 'W';
            }
            // pier side and Ha don't match at +-90 deg dec
            if (currentDEC > 89.999 || currentDEC < -89.999)
            {
                pierSide = PIER_UNKNOWN;
                psc = 'U';
            }
        }

        LOGF_DEBUG("latitude %g, sop %c, PierSide %c",
                   LocationNP[LOCATION_LATITUDE].getValue(),
                   sop, psc);
    }


    // aligning
    if (slewToIndex)
    {
        bool atIndex;
        if (!driver.indexreached(&atIndex))
        {
            LOG_ERROR("IndexReached Failure");
            slewToIndex = false;
            return false;
        }
        if (atIndex)
        {
            slewToIndex = false;
            // reached the index position.

            // do an alignment
            if (!fwInfo.hasHomeIndex)
            {
                // put another dire warning here
                LOG_WARN("This mount does not have index switches, the alignment assumes it is at the index position.");
            }

            if (!driver.lastalign())
            {
                LOG_ERROR("LastAlign failed");
                return false;
            }

            LastAlignSP.s = IPS_IDLE;
            IDSetSwitch(&LastAlignSP, "Align finished");

            bool isAligned;
            if (!driver.check_aligned(&isAligned))
            {
                LOG_WARN("get Alignment Failed!");
            }
            else
            {
                if (isAligned)
                    LOG_INFO("Mount is aligned");
                else
                    LOG_WARN("Alignment Failed!");
            }

            return true;
        }
    }

    switch (TrackState)
    {
        case SCOPE_SLEWING:
            // are we done?
            bool slewing;
            if (driver.is_slewing(&slewing) && !slewing)
            {
                LOG_INFO("Slew complete, tracking...");
                SetTrackEnabled(true);
                // update ra offset
                double raoffset = targetRA - currentRA + SlewOffsetRa;
                if (raoffset > 0.0 || raoffset < 10.0 / 3600.0)
                {
                    // average last two values
                    SlewOffsetRa = SlewOffsetRa > 0 ? (SlewOffsetRa + raoffset) / 2 : raoffset;

                    LOGF_DEBUG("raoffset %4.1f, SlewOffsetRa %4.1f arcsec", raoffset * 3600 * 15, SlewOffsetRa * 3600 * 15);
                }
            }
            break;

        case SCOPE_PARKING:
            // are we done?
            if (driver.is_slewing(&slewing) && !slewing)
            {
                if (driver.set_track_mode(CTM_OFF))
                    LOG_DEBUG("Mount tracking is off.");

                SetParked(true);
                saveConfig(true);

                // Check if we need to hibernate
                if (UseHibernateS[0].s == ISS_ON)
                {
                    LOG_INFO("Hibernating mount...");
                    if (driver.hibernate())
                        LOG_INFO("Mount hibernated. Please disconnect now and turn off your mount.");
                    else
                        LOG_ERROR("Hibernating mount failed!");
                }
            }
            break;

        default:
            break;
    }

    // update pier side and RaDec close together to minimise the possibility of
    // a mismatch causing an Ha limit error during a pier flip slew.
    if (HasPierSide())
        setPierSide(pierSide);
    NewRaDec(currentRA, currentDEC);

    // is PEC Handling required
    if (driver.pecState >= PEC_STATE::PEC_AVAILABLE)
    {
        static PEC_STATE lastPecState = PEC_STATE::NotKnown;
        static size_t lastPecIndex = 1000;
        static size_t numRecordPoints;

        if (driver.pecState >= PEC_STATE::PEC_INDEXED)
        {
            if (numPecBins < 88)
            {
                numPecBins = driver.getPecNumBins();
            }
            // get and show the current PEC index
            size_t pecIndex = driver.pecIndex();

            if (pecIndex != lastPecIndex)
            {
                LOGF_DEBUG("PEC state %s, index %d", driver.PecStateStr(), pecIndex);
                IUSaveText(&PecInfoT[1], std::to_string(pecIndex).c_str());
                IDSetText(&PecInfoTP, nullptr);
                lastPecIndex = pecIndex;

                // count the PEC records
                if (driver.pecState == PEC_STATE::PEC_RECORDING)
                    numRecordPoints++;
                else
                    numRecordPoints = 0;
            }
        }

        // update the PEC state
        if (driver.updatePecState() != lastPecState)
        {
            // and handle the change, if there was one
            LOGF_DEBUG("PEC last state %s, new State %s", driver.PecStateStr(lastPecState), driver.PecStateStr());

            // update the state string
            IUSaveText(&PecInfoT[0], driver.PecStateStr());
            IDSetText(&PecInfoTP, nullptr);

            // no need to check both current and last because they must be different
            switch (lastPecState)
            {
                case PEC_STATE::PEC_SEEKING:
                    // finished seeking
                    PecControlS[PEC_Seek].s = ISS_OFF;
                    PecControlSP.s = IPS_IDLE;
                    IDSetSwitch(&PecControlSP, nullptr);
                    LOG_INFO("PEC index Seek completed.");
                    break;
                case PEC_STATE::PEC_PLAYBACK:
                    // finished playback
                    PecControlS[PEC_Playback].s = ISS_OFF;
                    PecControlSP.s = IPS_IDLE;
                    IDSetSwitch(&PecControlSP, nullptr);
                    LOG_INFO("PEC playback finished");
                    break;
                case PEC_STATE::PEC_RECORDING:
                    // finished recording
                    LOGF_DEBUG("PEC record stopped, %d records", numRecordPoints);

                    if (numRecordPoints >= numPecBins)
                    {
                        savePecData();
                    }

                    PecControlS[PEC_Record].s = ISS_OFF;
                    PecControlSP.s = IPS_IDLE;
                    LOG_INFO("PEC record finished");
                    IDSetSwitch(&PecControlSP, nullptr);

                    break;
                default:
                    break;
            }
            lastPecState = driver.pecState;
        }
    }

    // focuser
    if (fwInfo.hasFocuser)
    {
        // Check position
        double lastPosition = FocusAbsPosNP[0].getValue();

        int absPos = focusTrue2Abs(driver.foc_position());
        if (absPos >= 0)
        {
            FocusAbsPosNP[0].setValue(absPos);
            // Only update if there is actual change
            if (fabs(lastPosition - FocusAbsPosNP[0].getValue()) > 1)
                FocusAbsPosNP.apply();
        }

        if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
        {
            // The backlash handling is done here, if the move state
            // shows that a backlash move has been done then the final move needs to be started
            // and the states left at IPS_BUSY

            if (!driver.foc_moving())
            {
                if (focusBacklashMove)
                {
                    focusBacklashMove = false;
                    if (driver.foc_move(focusAbs2True(focusAbsPosition)))
                        LOGF_INFO("Focus final move %i", focusAbsPosition);
                    else
                        LOG_INFO("Backlash move failed");
                }
                else
                {
                    FocusAbsPosNP.setState(IPS_OK);
                    FocusRelPosNP.setState(IPS_OK);
                    FocusAbsPosNP.apply();
                    FocusRelPosNP.apply();
                    LOG_INFO("Focuser reached requested position.");
                }
            }
        }
    }

    return true;
}

bool CelestronGPS::Abort()
{
    driver.stop_motion(CELESTRON_N);
    driver.stop_motion(CELESTRON_S);
    driver.stop_motion(CELESTRON_W);
    driver.stop_motion(CELESTRON_E);

    //GUIDE Abort guide operations.
    if (GuideNSNP.getState() == IPS_BUSY || GuideWENP.getState() == IPS_BUSY)
    {
        GuideNSNP.setState(IPS_IDLE);
        GuideWENP.setState(IPS_IDLE);
        GuideNSNP[0].setValue(0);
        GuideNSNP[1].setValue(0);
        GuideWENP[0].setValue(0);
        GuideWENP[1].setValue(0);

        if (GuideNSTID)
        {
            IERmTimer(GuideNSTID);
            GuideNSTID = 0;
        }

        if (GuideWETID)
        {
            IERmTimer(GuideWETID);
            GuideWETID = 0;
        }

        LOG_INFO("Guide aborted.");
        GuideNSNP.apply();
        GuideWENP.apply();

        return true;
    }

    return driver.abort();
}

bool CelestronGPS::Handshake()
{
    driver.set_device(getDeviceName());
    driver.set_port_fd(PortFD);

    if (isSimulation())
    {
        driver.set_simulation(true);
        driver.set_sim_slew_rate(SR_5);
        driver.set_sim_ra(0);
        driver.set_sim_dec(90);
    }

    if (driver.check_connection() == false)
    {
        LOG_ERROR("Failed to communicate with the mount, check the logs for details.");
        return false;
    }

    return true;
}

bool CelestronGPS::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && std::string(getDeviceName()) == dev)
    {
        // Enable/Disable hibernate
        if (name && std::string(name) == UseHibernateSP.name)
        {
            IUUpdateSwitch(&UseHibernateSP, states, names, n);
            if (fwInfo.controllerVersion > 0)
            {
                if (UseHibernateS[0].s == ISS_ON && (checkMinVersion(4.22, "hibernation", true) == false))
                {
                    UseHibernateS[0].s = ISS_OFF;
                    UseHibernateS[1].s = ISS_ON;
                    UseHibernateSP.s = IPS_ALERT;
                }
                else
                    UseHibernateSP.s = IPS_OK;
            }
            IDSetSwitch(&UseHibernateSP, nullptr);
            return true;
        }

        // start a last align
        // the process is:
        //  start move to switch position
        //  wait for the move to finish
        //  set the time from the PC - maybe
        //  send a Last Align command "Y"


        if (name && std::string(name) == LastAlignSP.name)
        {
            if (!fwInfo.hasHomeIndex)
            {
                // put the dire warning here
                LOG_WARN("This mount does not have index switches, make sure that it is at the index position.");
            }
            LOG_DEBUG("Start Align");
            // start move to switch positions
            if (!driver.startmovetoindex())
            {
                LastAlignSP.s = IPS_ALERT;
                return false;
            }
            // wait for the move to finish
            // done in ReadScopeStatus
            slewToIndex = true;
            LastAlignSP.s = IPS_BUSY;
            IDSetSwitch(&LastAlignSP, "Align in progress");
            return true;
        }

        // handle the PEC commands
        if (name && std::string(name) == PecControlSP.name)
        {
            IUUpdateSwitch(&PecControlSP, states, names, n);
            int idx = IUFindOnSwitchIndex(&PecControlSP);

            switch(idx)
            {
                case PEC_Stop:
                    LOG_DEBUG(" stop PEC record or playback");
                    bool playback;
                    if ((playback = driver.pecState == PEC_PLAYBACK) || driver.pecState == PEC_RECORDING)
                    {
                        if (playback ? driver.PecPlayback(false) : driver.PecRecord(false))
                        {
                            PecControlSP.s = IPS_IDLE;
                        }
                        else
                        {
                            PecControlSP.s = IPS_ALERT;
                        }
                    }
                    else
                    {
                        LOG_WARN("Incorrect state to stop PEC Playback or Record");
                        PecControlSP.s = IPS_ALERT;
                    }
                    IUResetSwitch(&PecControlSP);
                    break;
                case PEC_Playback:
                    LOG_DEBUG("start PEC Playback");
                    if (driver.pecState == PEC_STATE::PEC_INDEXED)
                    {
                        // start playback
                        if (driver.PecPlayback(true))
                        {
                            PecControlSP.s = IPS_BUSY;
                            LOG_INFO("PEC Playback started");
                        }
                        else
                        {
                            PecControlSP.s = IPS_ALERT;
                            return false;
                        }
                    }
                    else
                    {
                        LOG_WARN("Incorrect state to start PEC Playback");
                    }
                    break;
                case PEC_Record:
                    LOG_DEBUG("start PEC record");
                    if (TrackState != TelescopeStatus::SCOPE_TRACKING)
                    {
                        LOG_WARN("Mount must be Tracking to record PEC");
                        break;
                    }
                    if (driver.pecState == PEC_STATE::PEC_INDEXED)
                    {
                        if (driver.PecRecord(true))
                        {
                            PecControlSP.s = IPS_BUSY;
                            LOG_INFO("PEC Record started");
                        }
                        else
                        {
                            PecControlSP.s = IPS_ALERT;
                            return false;
                        }
                    }
                    else
                    {
                        LOG_WARN("Incorrect state to start PEC Recording");
                    }
                    break;
                case PEC_Seek:
                    LOG_DEBUG("Seek PEC Index");
                    if (driver.isPecAtIndex(true))
                    {
                        LOG_INFO("PEC index already found");
                        PecControlS[PEC_Seek].s = ISS_OFF;
                    }
                    else if (driver.pecState == PEC_STATE::PEC_AVAILABLE)
                    {
                        // start seek, moves up to 2 degrees in Ra
                        if (driver.PecSeekIndex())
                        {
                            PecControlSP.s = IPS_BUSY;
                            LOG_INFO("Seek PEC index started");
                        }
                        else
                        {
                            PecControlSP.s = IPS_ALERT;
                            return false;
                        }
                    }
                    break;
            }
            IDSetSwitch(&PecControlSP, nullptr);
            return true;
        }

        // Focuser
        if (strstr(name, "FOCUS"))
        {
            return FI::processSwitch(dev, name, states, names, n);
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool CelestronGPS::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Check focuser interface
    if (FI::processNumber(dev, name, values, names, n))
        return true;
    // Check guider interface
    if (GI::processNumber(dev, name, values, names, n))
        return true;

    if (dev && std::string(dev) == getDeviceName())
    {
        // Guide Rate
        if (strcmp(name, "GUIDE_RATE") == 0)
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);
            uint8_t grRa  = static_cast<uint8_t>(std::min(GuideRateN[AXIS_RA].value * 256.0, 255.0));
            uint8_t grDec = static_cast<uint8_t>(std::min(GuideRateN[AXIS_DE].value * 256.0, 255.0));
            //LOGF_DEBUG("Set Guide Rates (0-1x sidereal): Ra %f, Dec %f", GuideRateN[AXIS_RA].value, GuideRateN[AXIS_DE].value);
            //LOGF_DEBUG("Set Guide Rates         (0-255): Ra %i, Dec %i", grRa, grDec);
            LOGF_DEBUG("Set Guide Rates: Ra %f, Dec %f", GuideRateN[AXIS_RA].value, GuideRateN[AXIS_DE].value);
            driver.set_guide_rate(CELESTRON_AXIS::RA_AXIS, grRa);
            driver.set_guide_rate(CELESTRON_AXIS::DEC_AXIS, grDec);
            LOG_WARN("Changing guide rates may require recalibration of guiding.");
            return true;
        }
    }

    INDI::Telescope::ISNewNumber(dev, name, values, names, n);
    return true;
}

bool CelestronGPS::ISNewText(const char *dev, const char *name, char **texts, char **names, int n)
{
    // the idea is that pressing "Set" on the PEC_LOAD text will load the data in the file specified in the text
    if (dev && std::string(dev) == getDeviceName())
    {
        LOGF_DEBUG("ISNewText name %s, text %s, names %s, n %d", name, texts[0], names[0], n);

        if (name && std::string(name) == "PEC_LOAD")
        {

            IUUpdateText(&PecFileNameTP, texts, names, n);
            IDSetText(&PecFileNameTP, nullptr);

            LOGF_DEBUG("PEC Set %s", PecFileNameT[0].text);

            PecData pecData;

            // load from file
            if (!pecData.Load(PecFileNameT[0].text))
            {
                LOGF_WARN("File %s load failed", PecFileNameT[0].text);
                return false;
            }
            // save to mount
            if (!pecData.Save(&driver))
            {
                LOGF_WARN("PEC Data file %s save to mount failed", PecFileNameT[0].text);
                return false;
            }
            LOGF_INFO("PEC Data file %s sent to mount", PecFileNameT[0].text);
        }
    }

    INDI::Telescope::ISNewText(dev, name, texts, names, n);
    return true;
}


bool CelestronGPS::SetFocuserBacklash(int32_t steps)
{
    // Just update the number
    INDI_UNUSED(steps);
    return true;
}

void CelestronGPS::mountSim()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt, dx, da_ra = 0, da_dec = 0;
    int nlocked;

    // update elapsed time since last poll, don't presume exactly POLLMS
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

    if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
    {
        int rate = SlewRateSP.findOnSwitchIndex();

        switch (rate)
        {
            case SLEW_GUIDE:
                da_ra  = FINE_SLEW_RATE * dt * 0.05;
                da_dec = FINE_SLEW_RATE * dt * 0.05;
                break;

            case SLEW_CENTERING:
                da_ra  = FINE_SLEW_RATE * dt * .1;
                da_dec = FINE_SLEW_RATE * dt * .1;
                break;

            case SLEW_FIND:
                da_ra  = SLEW_RATE * dt;
                da_dec = SLEW_RATE * dt;
                break;

            default:
                da_ra  = GOTO_RATE * dt;
                da_dec = GOTO_RATE * dt;
                break;
        }

        switch (MovementNSSP.getState())
        {
            case IPS_BUSY:
                if (MovementNSSP[DIRECTION_NORTH].getState() == ISS_ON)
                    currentDEC += da_dec;
                else if (MovementNSSP[DIRECTION_SOUTH].getState() == ISS_ON)
                    currentDEC -= da_dec;
                break;

            default:
                break;
        }

        switch (MovementWESP.getState())
        {
            case IPS_BUSY:
                if (MovementWESP[DIRECTION_WEST].getState() == ISS_ON)
                    currentRA += da_ra / 15.;
                else if (MovementWESP[DIRECTION_EAST].getState() == ISS_ON)
                    currentRA -= da_ra / 15.;
                break;

            default:
                break;
        }

        driver.set_sim_ra(currentRA);
        driver.set_sim_dec(currentDEC);

        NewRaDec(currentRA, currentDEC);

        return;
    }

    // Process per current state. We check the state of EQUATORIAL_COORDS and act accordingly
    switch (TrackState)
    {
        case SCOPE_IDLE:
            currentRA = driver.get_sim_ra() + (TRACKRATE_SIDEREAL / 3600.0 * dt) / 15.0;
            currentRA = range24(currentRA);
            break;

        case SCOPE_SLEWING:
        case SCOPE_PARKING:
            // slewing - nail it when both within one pulse @ SLEWRATE
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
                driver.set_sim_slewing(false);
            }

            break;

        default:
            break;
    }

    driver.set_sim_ra(currentRA);
    driver.set_sim_dec(currentDEC);
}

void CelestronGPS::simulationTriggered(bool enable)
{
    driver.set_simulation(enable);
}

// Update Location and time are disabled if the mount is aligned.  This is because
// changing either will change the mount model because at least the local sidereal time
// will be changed. StarSense will set the mount to unaligned but it isn't a good idea even
// with the NexStar HCs

bool CelestronGPS::updateLocation(double latitude, double longitude, double elevation)
{
    if (!isConnected())
    {
        LOG_DEBUG("updateLocation called before we are connected");
        return false;
    }

    if (!checkMinVersion(2.3, "updating location"))
        return false;

    bool isAligned;
    if (!driver.check_aligned(&isAligned))
    {
        LOG_INFO("Update location - check_aligned failed");
        return false;
    }

    if (isAligned)
    {
        LOG_INFO("Updating location is not necessary since mount is already aligned.");
        return false;
    }

    LOGF_DEBUG("Update location %8.3f, %8.3f, %4.0f", latitude, longitude, elevation);

    return driver.set_location(longitude, latitude);
}

bool CelestronGPS::updateTime(ln_date *utc, double utc_offset)
{
    if (!isConnected())
    {
        LOG_DEBUG("updateTime called before we are connected");
        return false;
    }

    if (!checkMinVersion(2.3, "updating time"))
        return false;

    // setting time on StarSense seems to make it not aligned
    bool isAligned;
    if (!driver.check_aligned(&isAligned))
    {
        LOG_INFO("UpdateTime - check_aligned failed");
        return false;
    }
    if (isAligned)
    {
        LOG_INFO("Updating time is not necessary since mount is already aligned.");
        return false;
    }

    // starsense HC doesn't seem to support the precise time setting
    bool precise = fwInfo.controllerVersion >= 5.28;

    bool dst = DSTSettingS[0].s == ISS_ON;

    LOGF_DEBUG("Update time: offset %f %s UTC %i-%02i-%02iT%02i:%02i:%02.0f", utc_offset, dst ? "DST" : "", utc->years,
               utc->months, utc->days,
               utc->hours, utc->minutes, utc->seconds);

    return (driver.set_datetime(utc, utc_offset, dst, precise));
}

bool CelestronGPS::Park()
{
    double parkAZ  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    // unsync is only for NS+ 5.29 or more and not StarSense
    if (fwInfo.controllerVersion >= 5.29 && !driver.unsync())
        return false;

    LOGF_DEBUG("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    if (driver.slew_azalt(parkAZ, parkAlt, usePreciseCoords))
    {
        TrackState = SCOPE_PARKING;
        LOG_INFO("Parking is in progress...");
        return true;
    }

    return false;
}

bool CelestronGPS::UnPark()
{
    bool parkDataValid = (LoadParkData() == nullptr);
    // Check if we need to wake up IF:
    // 1. Park data exists in ParkData.xml
    // 2. Mount is currently parked
    // 3. Hibernate option is enabled
    if (parkDataValid && isParked() && UseHibernateS[0].s == ISS_ON)
    {
        LOG_INFO("Waking up mount...");

        if (!driver.wakeup())
        {
            LOG_ERROR("Waking up mount failed! Make sure mount is powered and connected. "
                      "Hibernate requires firmware version >= 5.21");
            return false;
        }
    }

    // Set tracking mode to whatever it was stored before
    SetParked(false);

    //loadConfig(true, "TELESCOPE_TRACK_MODE");
    // Read Saved Track State from config file
    for (size_t i = 0; i < TrackStateSP.count(); i++)
        IUGetConfigSwitch(getDeviceName(), TrackStateSP.getName(), TrackStateSP[i].getName(), &(TrackStateSP[i].s));

    // set the mount tracking state
    LOGF_DEBUG("track state %s", TrackStateSP.getLabel());
    SetTrackEnabled(TrackStateSP.findOnSwitchIndex() == TRACK_ON);

    // reinit PEC
    if (driver.pecState >= PEC_STATE::PEC_AVAILABLE)
        driver.pecState = PEC_AVAILABLE;

    return true;
}

bool CelestronGPS::SetCurrentPark()
{
    // The Goto Alt-Az and Get Alt-Az menu items have been renamed Goto Axis Postn and Get Axis Postn
    // where Postn is an abbreviation for Position. Since this feature doesn't actually refer
    // to altitude and azimuth when mounted on a wedge, the new designation is more accurate.
    // Source  : NexStarHandControlVersion4UsersGuide.pdf

    if (driver.get_azalt(&currentAZ, &currentALT, usePreciseCoords) == false)
    {
        LOG_ERROR("Failed to read AZ/ALT values.");
        return false;
    }

    double parkAZ = currentAZ;
    double parkAlt = currentALT;

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr,
               AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool CelestronGPS::SetDefaultPark()
{
    // The Goto Alt-Az and Get Alt-Az menu items have been renamed Goto Axis Postn and Get Axis Postn
    // where Postn is an abbreviation for Position. Since this feature doesn't actually refer
    // to altitude and azimuth when mounted on a wedge, the new designation is more accurate.
    // Source  : NexStarHandControlVersion4UsersGuide.pdf

    // By default azimuth 90° ( hemisphere doesn't matter)
    SetAxis1Park(90);

    // Altitude = 90° (latitude doesn't matter)
    SetAxis2Park(90);

    return true;
}

bool CelestronGPS::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    FI::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &UseHibernateSP);
    IUSaveConfigSwitch(fp, &CelestronTrackModeSP);
    IUSaveConfigSwitch(fp, &DSTSettingSP);

    //  IUSaveConfigNumber(fp, &FocusMinPosNP);

    return true;
}

bool CelestronGPS::setCelestronTrackMode(CELESTRON_TRACK_MODE mode)
{
    if (driver.set_track_mode(mode))
    {
        TrackState = (mode == CTM_OFF) ? SCOPE_IDLE : SCOPE_TRACKING;
        LOGF_DEBUG("Tracking mode set to %i, %s.", mode, CelestronTrackModeS[mode - 1].label);
        return true;
    }

    return false;
}

//GUIDE Guiding functions.

// There have been substantial changes at version 3.3, Nov 2019
//
// The mount controlled Aux Guide is used if it is available, this is
// if the mount firmware version for both axes is 6.12 or better.  Other
// mounts use a timed guide method.
// The mount Aux Guide command has a maximum vulue of 2.55 seconds but if
// a longer guide is needed then multiple Aux Guide commands are sent.
//
// The start guide and stop guide functions use helper functions to avoid
// code duplication.

IPState CelestronGPS::GuideNorth(uint32_t ms)
{
    return Guide(CELESTRON_DIRECTION::CELESTRON_N, ms);
}

IPState CelestronGPS::GuideSouth(uint32_t ms)
{
    return Guide(CELESTRON_DIRECTION::CELESTRON_S, ms);
}

IPState CelestronGPS::GuideEast(uint32_t ms)
{
    return Guide(CELESTRON_DIRECTION::CELESTRON_E, ms);
}

IPState CelestronGPS::GuideWest(uint32_t ms)
{
    return Guide(CELESTRON_DIRECTION::CELESTRON_W, ms);
}

// common function to start guiding for all axes.
IPState CelestronGPS::Guide(CELESTRON_DIRECTION dirn, uint32_t ms)
{
    // set up direction properties
    char dc = 'x';
    int* guideTID = &GuideNSTID;
    int* ticks = &ticksNS;
    uint8_t rate = 50;
    int index = 0;

    // set up pointers to the various things needed
    switch (dirn)
    {
        case CELESTRON_N:
            dc = 'N';
            guideTID = &GuideNSTID;
            ticks = &ticksNS;
            /* Scale guide rates to uint8 in [0..100] for sending to telescopoe, see  CelestronDriver::send_pulse() */
            rate = guideRateDec = static_cast<uint8_t>(GuideRateN[AXIS_DE].value * 100.0);
            break;
        case CELESTRON_S:
            dc = 'S';
            guideTID = &GuideNSTID;
            ticks = &ticksNS;
            index = 1;
            /* Scale guide rates to uint8 in [0..100] for sending to telescopoe, see  CelestronDriver::send_pulse() */
            rate = guideRateDec = static_cast<uint8_t>(GuideRateN[AXIS_DE].value * 100.0);
            break;
        case CELESTRON_E:
            dc = 'E';
            guideTID = &GuideWETID;
            ticks = &ticksWE;
            /* Scale guide rates to uint8 in [0..100] for sending to telescopoe, see  CelestronDriver::send_pulse() */
            rate = guideRateRa = static_cast<uint8_t>(GuideRateN[AXIS_RA].value * 100.0);
            break;
        case CELESTRON_W:
            dc = 'W';
            guideTID = &GuideWETID;
            ticks = &ticksWE;
            index = 1;
            /* Scale guide rates to uint8 in [0..100] for sending to telescopoe, see  CelestronDriver::send_pulse() */
            rate = guideRateRa = static_cast<uint8_t>(GuideRateN[AXIS_RA].value * 100.0);
            break;
    }

    LOGF_DEBUG("GUIDE CMD: %c %u ms, %s guide", dc, ms, canAuxGuide ? "Aux" : "Time");

    if (!canAuxGuide && (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }

    auto directionProperty = dirn > CELESTRON_S ? MovementWESP : MovementNSSP;

    // If already moving (no pulse command), then stop movement
    if (directionProperty.getState() == IPS_BUSY)
    {
        LOG_DEBUG("Already moving - stop");
        driver.stop_motion(dirn);
    }

    if (*guideTID)
    {
        LOGF_DEBUG("Stop timer %c", dc);
        IERmTimer(*guideTID);
        *guideTID = 0;
    }

    if (canAuxGuide)
    {
        // get the number of 10ms hardware ticks
        *ticks = ms / 10;

        // send the first Aux Guide command,
        if (driver.send_pulse(dirn, rate, static_cast<char>(std::min(255, *ticks))) == 0)
        {
            LOGF_ERROR("send_pulse %c error", dc);
            return IPS_ALERT;
        }
        // decrease ticks and ms values
        *ticks -= 255;
        ms = ms > 2550 ? 2550 : ms;
    }
    else
    {
        directionProperty[index].setState(ISS_ON);
        // start movement at HC button rate 1
        if (!driver.start_motion(dirn, CELESTRON_SLEW_RATE::SR_1))
        {
            LOGF_ERROR("StartMotion %c failed", dc);
            return IPS_ALERT;
        }
        *ticks = 0;
    }

    // Set slew to guiding
    SlewRateSP.reset();
    SlewRateSP[SLEW_GUIDE].setState(ISS_ON);
    SlewRateSP.apply();
    // start the guide timeout timer
    AddGuideTimer(dirn, static_cast<int>(ms));
    return IPS_BUSY;
}

//GUIDE The timer helper functions.
void CelestronGPS::guideTimerHelperN(void *p)
{
    static_cast<CelestronGPS *>(p)->guideTimer(CELESTRON_N);
}

void CelestronGPS::guideTimerHelperS(void *p)
{
    static_cast<CelestronGPS *>(p)->guideTimer(CELESTRON_S);
}

void CelestronGPS::guideTimerHelperW(void *p)
{
    static_cast<CelestronGPS *>(p)->guideTimer(CELESTRON_W);
}

void CelestronGPS::guideTimerHelperE(void *p)
{
    static_cast<CelestronGPS *>(p)->guideTimer(CELESTRON_E);
}

//GUIDE The timer function

/* Here I splitted the behaviour depending upon the direction
 * of the  guide command which generates the timer;  this was
 * done because the  member variable "guide_direction"  could
 * be  modified by a pulse  command on the  other axis BEFORE
 * the calling pulse command is terminated.
 */

void CelestronGPS::guideTimer(CELESTRON_DIRECTION dirn)
{
    int* ticks = &ticksNS;
    uint8_t rate = 0;

    switch(dirn)
    {
        case CELESTRON_N:
        case CELESTRON_S:
            ticks = &ticksNS;
            rate = guideRateDec;
            break;
        case CELESTRON_E:
        case CELESTRON_W:
            ticks = &ticksWE;
            rate = guideRateRa;
            break;
    }

    LOGF_DEBUG("guideTimer dir %c, ticks %i, rate %i", "NSWE"[dirn], *ticks, rate);

    if (canAuxGuide)
    {
        if (driver.get_pulse_status(dirn))
        {
            // current move not finished, add some more time
            AddGuideTimer(dirn, 100);
            return;
        }
        if (*ticks > 0)
        {
            // do some more guiding and set the timeout
            int dt =  (*ticks > 255) ? 255 : *ticks;
            driver.send_pulse(dirn, rate, static_cast<uint8_t>(dt));
            AddGuideTimer(dirn, dt * 10);
            *ticks -= 255;
            return;
        }
        // we get here if the axis reports guiding finished and all the ticks have been done
    }
    else
    {
        if (!driver.stop_motion(dirn))
            LOGF_ERROR("StopMotion failed dir %c", "NSWE"[dirn]);
    }

    switch(dirn)
    {
        case CELESTRON_N:
        case CELESTRON_S:
            MovementNSSP.reset();
            MovementNSSP.apply();
            GuideNSNP[0].setValue(0);
            GuideNSNP[1].setValue(0);
            GuideNSNP.setState(IPS_IDLE);
            GuideNSTID = 0;
            GuideNSNP.apply();
            break;
        case CELESTRON_E:
        case CELESTRON_W:
            MovementWESP.reset();
            MovementWESP.apply();
            GuideWENP[0].setValue(0);
            GuideWENP[1].setValue(0);
            GuideWENP.setState(IPS_IDLE);
            GuideWETID = 0;
            GuideWENP.apply();
            break;
    }
    LOGF_DEBUG("Guide %c finished", "NSWE"[dirn]);
}

void CelestronGPS::AddGuideTimer(CELESTRON_DIRECTION dirn, int ms)
{
    switch(dirn)
    {
        case CELESTRON_N:
            GuideNSTID = IEAddTimer(ms, guideTimerHelperN, this);
            break;
        case CELESTRON_S:
            GuideNSTID = IEAddTimer(ms, guideTimerHelperS, this);
            break;
        case CELESTRON_E:
            GuideWETID = IEAddTimer(ms, guideTimerHelperE, this);
            break;
        case CELESTRON_W:
            GuideWETID = IEAddTimer(ms, guideTimerHelperW, this);
            break;
    }
}

// end of guiding code

// the INDI overload, expected to set the track rate
// sidereal, solar or lunar and only if the mount is equatorial
bool CelestronGPS::SetTrackMode(uint8_t mode)
{
    CELESTRON_TRACK_RATE rate;

    switch (fwInfo.celestronTrackMode)
    {
        case CTM_OFF:
        case CTM_ALTAZ:
        case CTM_RADEC:
            return false;
        case CTM_EQN:
        case CTM_EQS:
            break;
    }

    switch (mode)
    {
        case 0:
            rate = CTR_SIDEREAL;
            break;
        case 1:
            rate = CTR_SOLAR;
            break;
        case 2:
            rate = CTR_LUNAR;
            break;
        default:
            return false;
    }
    return driver.set_track_rate(rate, fwInfo.celestronTrackMode);
}

bool CelestronGPS::SetTrackEnabled(bool enabled)
{
    return setCelestronTrackMode(enabled ? fwInfo.celestronTrackMode : CTM_OFF);
    //return setTrackMode(enabled ? static_cast<CELESTRON_TRACK_MODE>(IUFindOnSwitchIndex(&TrackModeSP)+1) : TRACKING_OFF);
}

void CelestronGPS::checkAlignment()
{
    ReadScopeStatus();

    bool isAligned;
    if (!driver.check_aligned(&isAligned) || !isAligned)
        LOG_WARN("Mount is NOT aligned. You must align the mount first before you can use it. Disconnect, align the mount, and reconnect again.");
}

bool CelestronGPS::savePecData()
{
    // generate the file name:
    // ~/.indi/pec/yyyy-mm-dd/pecData_hh:mm.log

    char ts_date[32], ts_time[32];
    struct tm *tp;
    time_t t;

    time(&t);
    tp = gmtime(&t);
    strftime(ts_date, sizeof(ts_date), "%Y-%m-%d", tp);
    strftime(ts_time, sizeof(ts_time), "%H:%M", tp);

    char dir[MAXRBUF];
    snprintf(dir, MAXRBUF, "%s/PEC_Data/%s", getenv("HOME"), ts_date);

    if (INDI::mkpath(dir, 0755) == -1)
    {
        LOGF_ERROR("Error creating directory %s (%s)", dir, strerror(errno));
        return false;
    }

    char pecFileBuf[MAXRBUF];
    snprintf(pecFileBuf, MAXRBUF, "%s/pecData_%s.csv", dir, ts_time);

    // show the file name
    IUSaveText(&PecFileNameT[0], pecFileBuf);
    IDSetText(&PecFileNameTP, nullptr);

    // get the PEC data from the mount
    PecData pecdata;

    if (!pecdata.Load(&driver))
    {
        LOG_DEBUG("Load PEC from mount failed");
        return false;
    }
    pecdata.RemoveDrift();
    // and save it
    if (!pecdata.Save(pecFileBuf))
    {
        LOGF_DEBUG("Save PEC file %s failed", pecFileBuf);
        return false;
    }
    LOGF_INFO("PEC data saved to %s", pecFileBuf);
    return true;
}

// focus control
IPState CelestronGPS::MoveAbsFocuser(uint32_t targetTicks)
{
    uint32_t absPosition = targetTicks;

    if (!focuserIsCalibrated)
    {
        LOG_ERROR("Move is not allowed because the focuser is not calibrated");
        return IPS_ALERT;
    }

    // implement backlash
    int delta = static_cast<int>(targetTicks - FocusAbsPosNP[0].getValue());

    if ((FocusBacklashNP[0].getValue() < 0 && delta > 0) ||
            (FocusBacklashNP[0].getValue() > 0 && delta < 0))
    {
        focusBacklashMove = true;
        focusAbsPosition = absPosition;
        absPosition -= FocusBacklashNP[0].getValue();
    }

    LOGF_INFO("Focus %s move %d", focusBacklashMove ? "backlash" : "direct", absPosition);

    if(!driver.foc_move(focusAbs2True(absPosition)))
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState CelestronGPS::MoveRelFocuser(INDI::FocuserInterface::FocusDirection dir, uint32_t ticks)
{
    uint32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = static_cast<uint32_t>(FocusAbsPosNP[0].getValue()) - ticks;
    else
        newPosition = static_cast<uint32_t>(FocusAbsPosNP[0].getValue()) + ticks;

    // Clamp
    newPosition = std::min(static_cast<uint32_t>(FocusAbsPosNP[0].getMax()), newPosition);
    return MoveAbsFocuser(newPosition);
}

bool CelestronGPS::AbortFocuser()
{
    return driver.foc_abort();
}

// read the focuser limits from the hardware
bool CelestronGPS::focusReadLimits()
{
    int low = 0, high = 0;
    bool valid = driver.foc_limits(&low, &high);

    focusTrueMax = high;
    focusTrueMin = low;

    FocusAbsPosNP[0].setMax(focusTrue2Abs(focusTrueMin));
    FocusMaxPosNP[0].setValue(focusTrue2Abs(focusTrueMin));
    FocusAbsPosNP.setState(IPS_OK);
    FocusAbsPosNP.updateMinMax();

    FocusMaxPosNP.setState(IPS_OK);
    FocusMaxPosNP.apply();

    // FocusMinPosNP[0].setValue(low);
    // FocusMinPosNP.s = IPS_OK;
    // IDSetNumber(&FocusMinPosNP, nullptr);

    focuserIsCalibrated = valid;

    LOGF_INFO("Focus Limits: Maximum (%i) Minimum (%i) steps.", high, low);
    return valid;
}
