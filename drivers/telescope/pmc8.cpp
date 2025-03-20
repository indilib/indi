/*
    INDI Explore Scientific PMC8 driver

    Copyright (C) 2017 Michael Fulbright

    Additional contributors:
        Thomas Olson, Copyright (C) 2019
        Karl Rees, Copyright (C) 2019-2023
        Martin Ruiz, Copyright (C) 2023

    Based on IEQPro driver.

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
/* Experimental Mount selector switch G11 vs EXOS2 by Thomas Olson
 *
 */

#include "pmc8.h"

#include <indicom.h>
#include <connectionplugins/connectionserial.h>
#include <connectionplugins/connectiontcp.h>

#include <libnova/sidereal_time.h>

#include <memory>

#include <math.h>
#include <string.h>

/* Simulation Parameters */
#define SLEWRATE 3          /* slew rate, degrees/s */

#define MOUNTINFO_TAB "Mount Info"

#define PMC8_DEFAULT_PORT 54372
#define PMC8_DEFAULT_IP_ADDRESS "192.168.47.1"
#define PMC8_TRACKING_AUTODETECT_INTERVAL 10
#define PMC8_VERSION_MAJOR 0
#define PMC8_VERSION_MINOR 5

static std::unique_ptr<PMC8> scope(new PMC8());

/* Constructor */
PMC8::PMC8() : GI(this)
{
    currentRA  = ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
    if (LocationNP[LOCATION_LATITUDE].getValue() < 0)
        currentDEC = -90;
    else
        currentDEC = 90;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_TRACK_RATE |
                           TELESCOPE_HAS_LOCATION,
                           9);

    setVersion(PMC8_VERSION_MAJOR, PMC8_VERSION_MINOR);
}

PMC8::~PMC8()
{
}

const char *PMC8::getDefaultName()
{
    return "PMC8";
}

bool PMC8::initProperties()
{
    INDI::Telescope::initProperties();

    // Serial Cable Connection Type
    // Letting them choose standard cable can speed up connection time significantly
    IUFillSwitch(&SerialCableTypeS[0], "SERIAL_CABLE_AUTO", "Auto", ISS_ON);
    IUFillSwitch(&SerialCableTypeS[1], "SERIAL_CABLE_INVERTED", "Inverted", ISS_OFF);
    IUFillSwitch(&SerialCableTypeS[2], "SERIAL_CABLE_STANDARD", "Standard", ISS_OFF);
    IUFillSwitchVector(&SerialCableTypeSP, SerialCableTypeS, 3, getDeviceName(), "SERIAL_CABLE_TYPE", "Serial Cable",
                       CONNECTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Mount Type
    IUFillSwitch(&MountTypeS[MOUNT_G11], "MOUNT_G11", "G11", ISS_OFF);
    IUFillSwitch(&MountTypeS[MOUNT_EXOS2], "MOUNT_EXOS2", "EXOS2", ISS_OFF);
    IUFillSwitch(&MountTypeS[MOUNT_iEXOS100], "MOUNT_iEXOS100", "iEXOS100", ISS_OFF);
    IUFillSwitchVector(&MountTypeSP, MountTypeS, 3, getDeviceName(), "MOUNT_TYPE", "Mount Type", CONNECTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);


    /* Tracking Mode */
    // order is important, since driver assumes solar = 1, lunar = 2
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    //AddTrackMode("TRACK_KING", "King"); // King appears to be effectively the same as Solar, at least for EXOS-2, and a bit of pain to implement with auto-detection
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Set TrackRate limits
    /*TrackRateN[AXIS_RA].min = -PMC8_MAX_TRACK_RATE;
    TrackRateN[AXIS_RA].max = PMC8_MAX_TRACK_RATE;
    TrackRateN[AXIS_DE].min = -0.01;
    TrackRateN[AXIS_DE].max = 0.01;*/

    // what to do after goto operation
    IUFillSwitch(&PostGotoS[0], "GOTO_START_TRACKING", "Start / Resume Tracking", ISS_ON);
    IUFillSwitch(&PostGotoS[1], "GOTO_RESUME_PREVIOUS", "Previous State", ISS_OFF);
    IUFillSwitch(&PostGotoS[2], "GOTO_STOP_TRACKING", "No Tracking", ISS_OFF);
    IUFillSwitchVector(&PostGotoSP, PostGotoS, 3, getDeviceName(), "POST_GOTO_SETTINGS", "Post Goto", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // relabel move speeds
    SlewRateSP[0].setLabel("4x");
    SlewRateSP[1].setLabel("8x");
    SlewRateSP[2].setLabel("16x");
    SlewRateSP[3].setLabel("32x");
    SlewRateSP[4].setLabel("64x");
    SlewRateSP[5].setLabel("128x");
    SlewRateSP[6].setLabel("256x");
    SlewRateSP[7].setLabel("512x");
    SlewRateSP[8].setLabel("833x");

    // settings for ramping up/down when moving
    IUFillNumber(&RampN[0], "RAMP_INTERVAL", "Interval (ms)", "%g", 20, 1000, 5, 200);
    IUFillNumber(&RampN[1], "RAMP_BASESTEP", "Base Step", "%g", 1, 256, 1, 4);
    IUFillNumber(&RampN[2], "RAMP_FACTOR", "Factor", "%g", 1.0, 2.0, 0.1, 1.4);
    IUFillNumberVector(&RampNP, RampN, 3, getDeviceName(), "RAMP_SETTINGS", "Move Ramp", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[0], "GUIDE_RATE_RA", "RA (x Sidereal)", "%g", 0.1, 1.0, 0.1, 0.4);
    IUFillNumber(&GuideRateN[1], "GUIDE_RATE_DE", "DEC (x Sidereal)", "%g", 0.1, 1.0, 0.1, 0.4);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guide Rate", GUIDE_TAB, IP_RW, 0, IPS_IDLE);
    IUFillNumber(&LegacyGuideRateN[0], "LEGACY_GUIDE_RATE", "x Sidereal", "%g", 0.1, 1.0, 0.1, 0.4);
    IUFillNumberVector(&LegacyGuideRateNP, LegacyGuideRateN, 1, getDeviceName(), "LEGACY_GUIDE_RATE", "Guide Rate", GUIDE_TAB,
                       IP_RW, 0, IPS_IDLE);

    GI::initProperties(GUIDE_TAB);

    TrackState = SCOPE_IDLE;

    // Driver does not support custom parking yet.
    SetParkDataType(PARK_NONE);

    addAuxControls();

    set_pmc8_device(getDeviceName());

    IUFillText(&FirmwareT[0], "Version", "Version", "");
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    return true;
}

bool PMC8::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        getStartupData();

        defineProperty(&PostGotoSP);
        loadConfig(true, PostGotoSP.name);

        defineProperty(&RampNP);
        loadConfig(true, RampNP.name);

        if (firmwareInfo.IsRev2Compliant)
        {
            defineProperty(&GuideRateNP);
        }
        else
        {
            defineProperty(&LegacyGuideRateNP);
        }

        defineProperty(&FirmwareTP);

        // do not support park position
        deleteProperty(ParkPositionNP);
        deleteProperty(ParkOptionSP);
    }
    else
    {
        deleteProperty(PostGotoSP.name);

        if (firmwareInfo.IsRev2Compliant)
        {
            deleteProperty(GuideRateNP.name);
        }
        else
        {
            deleteProperty(LegacyGuideRateNP.name);
        }

        deleteProperty(FirmwareTP.name);

        deleteProperty(RampNP.name);
    }

    GI::updateProperties();

    return true;
}

void PMC8::getStartupData()
{
    LOG_DEBUG("Getting firmware data...");
    if (get_pmc8_firmware(PortFD, &firmwareInfo))
    {
        const char *c;

        FirmwareTP.s = IPS_OK;
        c = firmwareInfo.MainBoardFirmware.c_str();
        LOGF_INFO("firmware = %s.", c);

        // not sure if there's really a point to the mount switch anymore if we know the mount from the firmware - perhaps remove as newer firmware becomes standard?
        // populate mount type switch in interface from firmware if possible
        if (firmwareInfo.MountType == MOUNT_EXOS2)
        {
            MountTypeS[MOUNT_EXOS2].s = ISS_ON;
            LOG_INFO("Detected mount type as Exos2.");
        }
        else if (firmwareInfo.MountType == MOUNT_G11)
        {
            MountTypeS[MOUNT_G11].s = ISS_ON;
            LOG_INFO("Detected mount type as G11.");
        }
        else if (firmwareInfo.MountType == MOUNT_iEXOS100)
        {
            MountTypeS[MOUNT_iEXOS100].s = ISS_ON;
            LOG_INFO("Detected mount type as iExos100.");
        }
        else
        {
            LOG_INFO("Cannot detect mount type--perhaps this is older firmware?");
            if (strstr(getDeviceName(), "EXOS2"))
            {
                MountTypeS[MOUNT_EXOS2].s = ISS_ON;
                LOG_INFO("Guessing mount is EXOS2 from device name.");
            }
            else if (strstr(getDeviceName(), "iEXOS100"))
            {
                MountTypeS[MOUNT_iEXOS100].s = ISS_ON;
                LOG_INFO("Guessing mount is iEXOS100 from device name.");
            }
            else
            {
                MountTypeS[MOUNT_G11].s = ISS_ON;
                LOG_INFO("Guessing mount is G11.");
            }
        }
        MountTypeSP.s = IPS_OK;
        IDSetSwitch(&MountTypeSP, nullptr);

        IUSaveText(&FirmwareT[0], c);
        IDSetText(&FirmwareTP, nullptr);
    }

    // get SRF values
    if (firmwareInfo.IsRev2Compliant)
    {
        double rate = 0.4;
        if (get_pmc8_guide_rate(PortFD, PMC8_AXIS_RA, rate))
        {
            GuideRateN[0].value = rate;
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);
        }
        if (get_pmc8_guide_rate(PortFD, PMC8_AXIS_DEC, rate))
        {
            GuideRateN[1].value = rate;
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);
        }
    }

    // PMC8 doesn't store location permanently so read from config and set
    // Convert to INDI standard longitude (0 to 360 Eastward)
    double longitude = LocationNP[LOCATION_LONGITUDE].getValue();
    double latitude = LocationNP[LOCATION_LATITUDE].getValue();
    if (latitude < 0)
        currentDEC = -90;
    else
        currentDEC = 90;


    // must also keep "low level" aware of position to convert motor counts to RA/DEC
    set_pmc8_location(latitude, longitude);

    // seems like best place to put a warning that will be seen in log window of EKOS/etc
    LOG_INFO("The PMC-Eight driver is in BETA development currently.");
    LOG_INFO("Be prepared to intervene if something unexpected occurs.");

#if 0
    // FIXME - Need to handle southern hemisphere for DEC?
    double HA  = ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
    double DEC = CurrentDEC;

    // currently only park at motor position (0, 0)
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
#endif

#if 0
    // FIXME - Need to implement simulation functionality
    if (isSimulation())
    {
        if (isParked())
            set_sim_system_status(ST_PARKED);
        else
            set_sim_system_status(ST_STOPPED);
    }
#endif
}

bool PMC8::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Check guider interface
    if (GI::processNumber(dev, name, values, names, n))
        return true;

    if (!strcmp(dev, getDeviceName()))
    {
        // Guiding Rate
        if (!strcmp(name, RampNP.name))
        {
            IUUpdateNumber(&RampNP, values, names, n);
            RampNP.s = IPS_OK;
            IDSetNumber(&RampNP, nullptr);

            return true;
        }
        if (!strcmp(name, LegacyGuideRateNP.name))
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);

            if (set_pmc8_guide_rate(PortFD, PMC8_AXIS_RA, LegacyGuideRateN[0].value))
                LegacyGuideRateNP.s = IPS_OK;
            else
                LegacyGuideRateNP.s = IPS_ALERT;

            IDSetNumber(&LegacyGuideRateNP, nullptr);

            return true;
        }
        if (!strcmp(name, GuideRateNP.name))
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);

            if (set_pmc8_guide_rate(PortFD, PMC8_AXIS_RA, GuideRateN[0].value) &&
                    set_pmc8_guide_rate(PortFD, PMC8_AXIS_DEC, GuideRateN[1].value))
                GuideRateNP.s = IPS_OK;
            else
                GuideRateNP.s = IPS_ALERT;

            IDSetNumber(&GuideRateNP, nullptr);

            return true;
        }
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

void PMC8::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);
    defineProperty(&MountTypeSP);
    defineProperty(&SerialCableTypeSP);
    loadConfig(true, SerialCableTypeSP.name);

    // set default connection parameters
    // unfortunately, the only way I've found to set these is after calling ISGetProperties on base class
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    tcpConnection->setDefaultHost(PMC8_DEFAULT_IP_ADDRESS);
    tcpConnection->setDefaultPort(PMC8_DEFAULT_PORT);

    // reload config here, even though it was already loaded in call to base class
    // since defaults may have overridden saved properties
    loadConfig(false, nullptr);
}

bool PMC8::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, MountTypeSP.name) == 0)
        {
            IUUpdateSwitch(&MountTypeSP, states, names, n);
            int currentMountIndex = IUFindOnSwitchIndex(&MountTypeSP);
            LOGF_INFO("Selected mount is %s", MountTypeS[currentMountIndex].label);

            //right now, this lets the user override the parameters for the detected mount.  Perhaps we should prevent the user from doing so?
            set_pmc8_mountParameters(currentMountIndex);
            MountTypeSP.s = IPS_OK;
            IDSetSwitch(&MountTypeSP, nullptr);
            return true;
        }
        if (strcmp(name, SerialCableTypeSP.name) == 0)
        {
            IUUpdateSwitch(&SerialCableTypeSP, states, names, n);
            SerialCableTypeSP.s = IPS_OK;
            IDSetSwitch(&SerialCableTypeSP, nullptr);
            return true;
        }
        if (strcmp(name, PostGotoSP.name) == 0)
        {
            IUUpdateSwitch(&PostGotoSP, states, names, n);
            // for v2 firmware, if halt after goto is selected, tell driver to use ESPt2
            set_pmc8_goto_resume(!((IUFindOnSwitchIndex(&PostGotoSP) == 2) && firmwareInfo.IsRev2Compliant));
            PostGotoSP.s = IPS_OK;
            IDSetSwitch(&PostGotoSP, nullptr);
            return true;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool PMC8::ReadScopeStatus()
{
    bool rc = false;

    // try to disconnect and reconnect if reconnect flag is set

    if (get_pmc8_reconnect_flag())
    {
        int rc = Disconnect();
        if (rc) setConnected(false);
        rc = Connect();
        if (rc) setConnected(true, IPS_OK);
        return false;
    }

    if (isSimulation())
        mountSim();

    // avoid unnecessary status calls to mount while pulse guiding so we don't lock up the mount for 40+ ms right when it needs to start/stop
    if (isPulsingNS || isPulsingWE) return true;

    bool slewing = false;

    switch (TrackState)
    {
        case SCOPE_SLEWING:
            // are we done?
            // check slew state
            rc = get_pmc8_is_scope_slewing(PortFD, slewing);
            if (!rc)
            {
                LOG_ERROR("PMC8::ReadScopeStatus() - unable to check slew state");
            }
            else
            {
                if (slewing == false)
                {
                    if ((IUFindOnSwitchIndex(&PostGotoSP) == 0) ||
                            ((IUFindOnSwitchIndex(&PostGotoSP) == 1) && (RememberTrackState == SCOPE_TRACKING)))
                    {
                        LOG_INFO("Slew complete, tracking...");
                        TrackState = SCOPE_TRACKING;
                        TrackStateSP.setState(IPS_IDLE);

                        // Don't want to restart tracking after goto with v2 firmware, since mount does automatically
                        // and we might detect that slewing has stopped before it fully settles
                        if (!firmwareInfo.IsRev2Compliant)
                        {
                            if (!SetTrackEnabled(true))
                            {
                                LOG_ERROR("slew complete - unable to enable tracking");
                                return false;
                            }
                        }
                    }
                    else
                    {
                        LOG_INFO("Slew complete.");
                        TrackState = RememberTrackState;
                    }
                }
            }

            break;

        case SCOPE_PARKING:
            // are we done?
            // check slew state
            rc = get_pmc8_is_scope_slewing(PortFD, slewing);
            if (!rc)
            {
                LOG_ERROR("PMC8::ReadScopeStatus() - unable to check slew state");
            }
            else
            {
                if (slewing == false)
                {
                    if (stop_pmc8_tracking_motion(PortFD))
                        LOG_DEBUG("Mount tracking is off.");

                    SetParked(true);

                    saveConfig(true);
                }
            }
            break;

        case SCOPE_IDLE:
            //periodically check to see if we've entered tracking state (e.g. at startup or from other client)
            if (!trackingPollCounter--)
            {

                trackingPollCounter = PMC8_TRACKING_AUTODETECT_INTERVAL;

                // make sure we aren't moving manually to avoid false positives
                if (moveInfoDEC.state == PMC8_MOVE_INACTIVE && moveInfoRA.state == PMC8_MOVE_INACTIVE)
                {

                    double track_rate;
                    uint8_t track_mode;

                    rc = get_pmc8_tracking_data(PortFD, track_rate, track_mode);

                    // N.B. PMC8 rates are arcseconds per sidereal second
                    // INDI uses arcseconds per solar second
                    track_rate *= SOLAR_SECOND;

                    if (rc && ((int)track_rate > 0) && ((int)track_rate <= PMC8_MAX_TRACK_RATE))
                    {
                        TrackModeSP.reset();
                        TrackModeSP[convertFromPMC8TrackMode(track_mode)].setState(ISS_ON);
                        TrackModeSP.setState(IPS_OK);
                        TrackModeSP.apply();
                        TrackState = SCOPE_TRACKING;
                        LOGF_INFO("Mount has started tracking at %f arcsec / sec", track_rate);
                        TrackRateNP.setState(IPS_IDLE);
                        TrackRateNP[AXIS_RA].setValue(track_rate);
                        TrackRateNP.apply();
                    }
                }
            }
            break;

        case SCOPE_TRACKING:
            //periodically check to see if we've stopped tracking or changed speed (e.g. from other client)
            if (!trackingPollCounter--)
            {
                trackingPollCounter = PMC8_TRACKING_AUTODETECT_INTERVAL;

                // make sure we aren't moving manually to avoid false positives
                if (moveInfoDEC.state == PMC8_MOVE_INACTIVE && moveInfoRA.state == PMC8_MOVE_INACTIVE)
                {

                    double track_rate;
                    uint8_t track_mode;

                    rc = get_pmc8_tracking_data(PortFD, track_rate, track_mode);

                    // N.B. PMC8 rates are arcseconds per sidereal second
                    // INDI uses arcseconds per solar second
                    track_rate *= SOLAR_SECOND;

                    if (rc && ((int)track_rate == 0))
                    {
                        LOG_INFO("Mount appears to have stopped tracking");
                        TrackState = SCOPE_IDLE;
                    }
                    else if (rc && ((int)track_rate <= PMC8_MAX_TRACK_RATE))
                    {
                        if (TrackModeSP[convertFromPMC8TrackMode(track_mode)].getState() != ISS_ON)
                        {
                            TrackModeSP.reset();
                            TrackModeSP[convertFromPMC8TrackMode(track_mode)].setState(ISS_ON);
                            TrackModeSP.apply();
                        }
                        if (TrackRateNP[AXIS_RA].getValue() != track_rate)
                        {
                            TrackState = SCOPE_TRACKING;
                            TrackRateNP.setState(IPS_IDLE);
                            TrackRateNP[AXIS_RA].setValue(track_rate);
                            TrackRateNP.apply();
                            LOGF_INFO("Mount now tracking at %f arcsec / sec", track_rate);
                        }
                    }
                }
            }

        default:
            break;
    }

    rc = get_pmc8_coords(PortFD, currentRA, currentDEC);

    if (rc)
        NewRaDec(currentRA, currentDEC);

    return rc;
}

bool PMC8::Goto(double r, double d)
{
    if (isPulsingNS ||
            isPulsingWE ||
            moveInfoDEC.state != PMC8_MOVE_INACTIVE ||
            moveInfoRA.state != PMC8_MOVE_INACTIVE ||
            (TrackState == SCOPE_SLEWING && !firmwareInfo.IsRev2Compliant))
    {
        LOG_ERROR("Cannot slew while moving or guiding.  Please stop moving or guiding first");
        return false;
    }
    else if (TrackState == SCOPE_SLEWING)
    {
        targetRA  = r;
        targetDEC = d;
        abort_pmc8_goto(PortFD);
        //Supposedly the goto should abort in 2s, but we'll give it a little bit more time just in case
        IEAddTimer(2500, AbortGotoTimeoutHelper, this);
        LOG_INFO("Goto called while already slewing.  Stopping slew and will try goto again in 2.5 seconds");
        return true;
    }

    // start tracking if we're idle, so mount will track at correct rate post-goto
    RememberTrackState = TrackState;
    if ((TrackState != SCOPE_TRACKING) && (IUFindOnSwitchIndex(&PostGotoSP) == 0) && firmwareInfo.IsRev2Compliant)
    {
        SetTrackEnabled(true);
    }
    else if (IUFindOnSwitchIndex(&PostGotoSP) == 2)
    {
        RememberTrackState = SCOPE_IDLE;
    }

    char RAStr[64] = {0}, DecStr[64] = {0};

    targetRA  = r;
    targetDEC = d;

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    LOGF_DEBUG("Slewing to RA: %s - DEC: %s", RAStr, DecStr);

    if (slew_pmc8(PortFD, r, d) == false)
    {
        LOG_ERROR("Failed to slew.");
        return false;
    }

    TrackState = SCOPE_SLEWING;

    return true;
}

bool PMC8::Sync(double ra, double dec)
{

    targetRA  = ra;
    targetDEC = dec;
    char RAStr[64] = {0}, DecStr[64] = {0};

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    LOGF_DEBUG("Syncing to RA: %s - DEC: %s", RAStr, DecStr);

    if (sync_pmc8(PortFD, ra, dec) == false)
    {
        LOG_ERROR("Failed to sync.");
    }

    EqNP.setState(IPS_OK);

    currentRA  = ra;
    currentDEC = dec;

    NewRaDec(currentRA, currentDEC);

    return true;
}

void PMC8::AbortGotoTimeoutHelper(void *p)
{
    //static_cast<PMC8*>(p)->TrackState = static_cast<PMC8*>(p)->RememberTrackState;
    static_cast<PMC8*>(p)->Goto(static_cast<PMC8*>(p)->targetRA, static_cast<PMC8*>(p)->targetDEC);
}

bool PMC8::Abort()
{
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
            GuideNSTID = 0;
        }

        LOG_INFO("Guide aborted.");
        GuideNSNP.apply();
        GuideWENP.apply();
        return true;
    }


    //GOTO Abort slew operations.
    if (TrackState == SCOPE_SLEWING)
    {
        abort_pmc8_goto(PortFD);
        //It will take about 2s to abort; we'll rely on ReadScopeStatus to detect when that occurs
        LOG_INFO("Goto aborted.");
        return true;
    }

    //MOVE Abort move operations.
    if ((moveInfoDEC.state == PMC8_MOVE_ACTIVE) || (moveInfoRA.state == PMC8_MOVE_ACTIVE))
    {
        if (moveInfoDEC.state == PMC8_MOVE_ACTIVE)
        {
            MoveNS((INDI_DIR_NS)moveInfoDEC.moveDir, MOTION_STOP);
        }
        if (moveInfoRA.state == PMC8_MOVE_ACTIVE)
        {
            MoveWE((INDI_DIR_WE)moveInfoRA.moveDir, MOTION_STOP);
        }
        LOG_INFO("Move aborted.");
        return true;
    }

    LOG_INFO("Abort called--stopping all motion.");
    if (abort_pmc8(PortFD))
    {
        TrackState = SCOPE_IDLE;
        return true;
    }
    else return false;
}

bool PMC8::Park()
{
#if 0
    // FIXME - Currently only support parking at motor position (0, 0)
    targetRA  = GetAxis1Park();
    targetDEC = GetAxis2Park();
    if (set_pmc8_radec(PortFD, r, d) == false)
    {
        LOG_ERROR("Error setting RA/DEC.");
        return false;
    }
#endif

    //if we're already parking, no need to do anything
    if (TrackState == SCOPE_PARKING)
    {
        return true;
    }

    if (park_pmc8(PortFD))
    {
        TrackState = SCOPE_PARKING;
        LOG_INFO("Telescope parking in progress to motor position (0, 0)");
        return true;
    }
    else
    {
        return false;
    }
}

bool PMC8::UnPark()
{
    if (unpark_pmc8(PortFD))
    {
        SetParked(false);
        TrackState = SCOPE_IDLE;
        return true;
    }
    else
    {
        return false;
    }
}

bool PMC8::Handshake()
{
    if (isSimulation())
    {
        set_pmc8_sim_system_status(ST_STOPPED);
        set_pmc8_sim_track_rate(PMC8_TRACK_SIDEREAL);
        set_pmc8_sim_move_rate(64 * 15);
        //        set_pmc8_sim_hemisphere(HEMI_NORTH);
    }

    PMC8_CONNECTION_TYPE conn = PMC8_SERIAL_AUTO;
    if (getActiveConnection() == serialConnection)
    {
        if (IUFindOnSwitchIndex(&SerialCableTypeSP) == 1) conn = PMC8_SERIAL_INVERTED;
        if (IUFindOnSwitchIndex(&SerialCableTypeSP) == 2) conn = PMC8_SERIAL_STANDARD;
    }
    else
    {
        conn = PMC8_ETHERNET;
    }

    return check_pmc8_connection(PortFD, conn);
}

bool PMC8::updateTime(ln_date *utc, double utc_offset)
{
    // mark unused
    INDI_UNUSED(utc);
    INDI_UNUSED(utc_offset);

    LOG_ERROR("PMC8::updateTime() not implemented!");
    return false;

}

bool PMC8::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (longitude > 180)
        longitude -= 360;

    // experimental support for Southern Hemisphere!
    if (latitude < 0)
    {
        LOG_WARN("Southern Hemisphere support still experimental!");
        //return false;
    }

    // must also keep "low level" aware of position to convert motor counts to RA/DEC
    set_pmc8_location(latitude, longitude);

    char l[32] = {0}, L[32] = {0};
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

void PMC8::debugTriggered(bool enable)
{
    set_pmc8_debug(enable);
}

void PMC8::simulationTriggered(bool enable)
{
    set_pmc8_simulation(enable);
}

int PMC8::getSlewRate()
{
    int mode = SlewRateSP.findOnSwitchIndex();
    if (mode >= 8) return PMC8_MAX_MOVE_RATE;
    return 4 * pow(2, mode) * 15;
}


bool PMC8::ramp_movement(PMC8_DIRECTION dir)
{

    PMC8MoveInfo *moveInfo = ((dir == PMC8_N) | (dir == PMC8_S)) ? &moveInfoDEC : &moveInfoRA;

    if (moveInfo->state != PMC8_MOVE_RAMPING)
    {
        return false; //shouldn't be here
        LOG_ERROR("Ramp function called while not in ramp state");
    }

    int newrate = moveInfo->rampLastStep;

    if (moveInfo->rampDir == PMC8_RAMP_UP)
    {
        newrate += RampN[1].value * pow(RampN[2].value, moveInfo->rampIteration++) * 15;
    }
    else
    {
        newrate -= RampN[1].value * pow(RampN[2].value, --moveInfo->rampIteration) * 15;
    }

    int adjrate = newrate;

    //check to see if we're done
    if (newrate >= moveInfo->targetRate)
    {
        adjrate = moveInfo->targetRate;
        moveInfo->state = PMC8_MOVE_ACTIVE;
    }
    else if (newrate <= 0)
    {
        adjrate = 0;
        moveInfo->state = PMC8_MOVE_INACTIVE;
        //restore tracking if we're at 0
        if ((dir == PMC8_E) || (dir == PMC8_W))
        {
            if (TrackState == SCOPE_TRACKING)
            {
                if (!SetTrackEnabled(true))
                {
                    LOG_ERROR("slew complete - unable to enable tracking");
                    return false;
                }
            }

            return true;
        }
    }

    //adjust for current tracking rate
    if (dir == PMC8_E) adjrate += round(TrackRateNP[AXIS_RA].getValue());
    else if (dir == PMC8_W) adjrate -= round(TrackRateNP[AXIS_RA].getValue());

    // Solar second to Sideral second conversion
    adjrate /= SOLAR_SECOND;

    LOGF_EXTRA3("Ramping: mount dir %d, ramping dir %d, iteration %d, step to %d", dir, moveInfo->rampDir,
                moveInfo->rampIteration, adjrate);

    if (!set_pmc8_move_rate_axis(PortFD, dir, adjrate))
    {
        LOGF_ERROR("Error ramping move rate: mount dir %d, ramping dir %d, iteration %d, step to %d", dir, moveInfo->rampDir,
                   moveInfo->rampIteration, adjrate);
        moveInfo->state = PMC8_MOVE_INACTIVE;
        return false;
    }

    moveInfo->rampLastStep = newrate;

    return true;
}

//MOVE The timer helper functions.
void PMC8::rampTimeoutHelperN(void *p)
{
    PMC8* pmc8 = static_cast<PMC8*>(p);
    if (pmc8->ramp_movement(PMC8_N) && (pmc8->moveInfoDEC.state == PMC8_MOVE_RAMPING))
        pmc8->moveInfoDEC.timer = IEAddTimer(pmc8->RampN[0].value, rampTimeoutHelperN, p);
}
void PMC8::rampTimeoutHelperS(void *p)
{
    PMC8* pmc8 = static_cast<PMC8*>(p);
    if (pmc8->ramp_movement(PMC8_S) && (pmc8->moveInfoDEC.state == PMC8_MOVE_RAMPING))
        pmc8->moveInfoDEC.timer = IEAddTimer(pmc8->RampN[0].value, rampTimeoutHelperS, p);
}
void PMC8::rampTimeoutHelperW(void *p)
{
    PMC8* pmc8 = static_cast<PMC8*>(p);
    if (pmc8->ramp_movement(PMC8_W) && (pmc8->moveInfoRA.state == PMC8_MOVE_RAMPING))
        pmc8->moveInfoRA.timer = IEAddTimer(pmc8->RampN[0].value, rampTimeoutHelperW, p);
}
void PMC8::rampTimeoutHelperE(void *p)
{
    PMC8* pmc8 = static_cast<PMC8*>(p);
    if (pmc8->ramp_movement(PMC8_E) && (pmc8->moveInfoRA.state == PMC8_MOVE_RAMPING))
        pmc8->moveInfoRA.timer = IEAddTimer(pmc8->RampN[0].value, rampTimeoutHelperE, p);
}


bool PMC8::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }
    if (TrackState == SCOPE_SLEWING)
    {
        LOG_ERROR("Mount is slewing.  Wait to issue move command until goto completes.");
        return false;
    }
    if ((moveInfoDEC.state == PMC8_MOVE_ACTIVE) && (moveInfoDEC.moveDir != dir))
    {
        LOG_ERROR("Mount received command to move in opposite direction before stopping.  This shouldn't happen.");
        return false;
    }

    // read desired move rate
    int currentIndex = SlewRateSP.findOnSwitchIndex();
    LOGF_DEBUG("MoveNS at slew index %d", currentIndex);

    switch (command)
    {
        case MOTION_START:
            moveInfoDEC.rampDir = PMC8_RAMP_UP;
            moveInfoDEC.targetRate = getSlewRate();
            // if we're still ramping down, we can bypass resetting the state and adding a timer
            // but we do need to make sure it's the same direction first (if not, kill our previous timer)
            if (moveInfoDEC.state == PMC8_MOVE_RAMPING)
            {
                if (moveInfoDEC.moveDir == dir) return true;
                IERmTimer(moveInfoDEC.timer);
                LOG_WARN("Started moving other direction before ramp down completed.  This *may* cause mechanical problems with mount.  It is adviseable to wait for axis movement to settle before switching directions.");
            }
            moveInfoDEC.moveDir = dir;
            moveInfoDEC.state = PMC8_MOVE_RAMPING;
            moveInfoDEC.rampIteration = 0;
            moveInfoDEC.rampLastStep = 0;

            LOGF_INFO("Moving toward %s.", (dir == DIRECTION_NORTH) ? "North" : "South");

            break;

        case MOTION_STOP:
            // if we've already started moving other direction, no need to stop
            if (moveInfoDEC.moveDir != dir)
            {
                LOGF_DEBUG("Stop command issued for direction %d, but we're not moving that way", dir);
                return false;
            }

            moveInfoDEC.rampDir = PMC8_RAMP_DOWN;
            // if we're still ramping up, we can bypass adding a timer
            if (moveInfoDEC.state == PMC8_MOVE_RAMPING) return true;
            moveInfoDEC.state = PMC8_MOVE_RAMPING;

            LOGF_INFO("%s motion stopping.", (dir == DIRECTION_NORTH) ? "North" : "South");

            break;
    }

    if (dir == DIRECTION_NORTH)
        rampTimeoutHelperN(this);
    else
        rampTimeoutHelperS(this);

    return true;
}


bool PMC8::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }
    if (TrackState == SCOPE_SLEWING)
    {
        LOG_ERROR("Mount is already slewing.  Wait to issue move command until done slewing.");
        return false;
    }
    if ((moveInfoRA.state == PMC8_MOVE_ACTIVE) && (moveInfoRA.moveDir != dir))
    {
        LOG_ERROR("Mount received command to move in opposite direction before stopping.  This shouldn't happen.");
        return false;
    }

    // read desired move rate
    int currentIndex = SlewRateSP.findOnSwitchIndex();
    LOGF_DEBUG("MoveWE at slew index %d", currentIndex);

    switch (command)
    {
        case MOTION_START:
            moveInfoRA.rampDir = PMC8_RAMP_UP;
            moveInfoRA.targetRate = getSlewRate();
            // if we're still ramping down, we can bypass resetting the state and adding a timer
            // but we do need to make sure it's the same direction first (if not, kill our previous timer)
            if (moveInfoRA.state == PMC8_MOVE_RAMPING)
            {
                if (moveInfoRA.moveDir == dir) return true;
                IERmTimer(moveInfoRA.timer);
                LOG_WARN("Started moving other direction before ramp down completed.  This *may* cause mechanical problems with mount.  It is adviseable to wait for axis movement to settle before switching directions.");
            }
            moveInfoRA.moveDir = dir;
            moveInfoRA.state = PMC8_MOVE_RAMPING;
            moveInfoRA.rampIteration = 0;
            moveInfoRA.rampLastStep = 0;

            LOGF_INFO("Moving toward %s.", (dir == DIRECTION_WEST) ? "West" : "East");

            break;

        case MOTION_STOP:
            // if we've already started moving other direction, no need to stop
            if (moveInfoRA.moveDir != dir)
            {
                LOGF_DEBUG("Stop command issued for direction %d, but we're not moving that way", dir);
                return false;
            }

            moveInfoRA.rampDir = PMC8_RAMP_DOWN;
            // if we're still ramping up, we can bypass adding a timer
            if (moveInfoRA.state == PMC8_MOVE_RAMPING) return true;
            moveInfoRA.state = PMC8_MOVE_RAMPING;

            LOGF_INFO("%s motion stopping.", (dir == DIRECTION_WEST) ? "West" : "East");

            break;
    }

    if (dir == DIRECTION_EAST)
        rampTimeoutHelperE(this);
    else
        rampTimeoutHelperW(this);

    return true;
}

IPState PMC8::GuideNorth(uint32_t ms)
{
    IPState ret = IPS_IDLE;
    long timetaken_us = 0;
    int timeremain_ms = 0;

    //only guide if tracking
    if (TrackState == SCOPE_TRACKING)
    {

        // If already moving, then stop movement
        if (MovementNSSP.getState() == IPS_BUSY)
        {
            int dir = MovementNSSP.findOnSwitchIndex();
            MoveNS(dir == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);
        }

        if (GuideNSTID)
        {
            IERmTimer(GuideNSTID);
            GuideNSTID = 0;
        }

        isPulsingNS = true;
        start_pmc8_guide(PortFD, PMC8_N, (int)ms, timetaken_us, 0);

        timeremain_ms = (int)(ms - ((float)timetaken_us) / 1000.0);

        if (timeremain_ms < 0)
            timeremain_ms = 0;

        ret = IPS_BUSY;
    }
    else
    {
        LOG_INFO("Mount not tracking--cannot guide.");
    }
    GuideNSTID      = IEAddTimer(timeremain_ms, guideTimeoutHelperN, this);
    return ret;
}

IPState PMC8::GuideSouth(uint32_t ms)
{
    IPState ret = IPS_IDLE;
    long timetaken_us = 0;
    int timeremain_ms = 0;

    //only guide if tracking
    if (TrackState == SCOPE_TRACKING)
    {

        // If already moving, then stop movement
        if (MovementNSSP.getState() == IPS_BUSY)
        {
            int dir = MovementNSSP.findOnSwitchIndex();
            MoveNS(dir == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);
        }

        if (GuideNSTID)
        {
            IERmTimer(GuideNSTID);
            GuideNSTID = 0;
        }

        isPulsingNS = true;
        start_pmc8_guide(PortFD, PMC8_S, (int)ms, timetaken_us, 0);

        timeremain_ms = (int)(ms - ((float)timetaken_us) / 1000.0);

        if (timeremain_ms < 0)
            timeremain_ms = 0;

        ret = IPS_BUSY;
    }
    else
    {
        LOG_INFO("Mount not tracking--cannot guide.");
    }
    GuideNSTID      = IEAddTimer(timeremain_ms, guideTimeoutHelperS, this);
    return ret;
}

IPState PMC8::GuideEast(uint32_t ms)
{
    IPState ret = IPS_IDLE;
    long timetaken_us = 0;
    int timeremain_ms = 0;

    //only guide if tracking
    if (TrackState == SCOPE_TRACKING)
    {

        // If already moving (no pulse command), then stop movement
        if (MovementWESP.getState() == IPS_BUSY)
        {
            int dir = MovementWESP.findOnSwitchIndex();
            MoveWE(dir == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
        }

        if (GuideWETID)
        {
            IERmTimer(GuideWETID);
            GuideWETID = 0;
        }

        isPulsingWE = true;

        start_pmc8_guide(PortFD, PMC8_E, (int)ms, timetaken_us, TrackRateNP[AXIS_RA].getValue() / SOLAR_SECOND);

        timeremain_ms = (int)(ms - ((float)timetaken_us) / 1000.0);

        if (timeremain_ms < 0)
            timeremain_ms = 0;

        ret = IPS_BUSY;
    }
    else
    {
        LOG_INFO("Mount not tracking--cannot guide.");
    }
    GuideWETID      = IEAddTimer(timeremain_ms, guideTimeoutHelperE, this);
    return ret;
}

IPState PMC8::GuideWest(uint32_t ms)
{
    IPState ret = IPS_IDLE;
    long timetaken_us = 0;
    int timeremain_ms = 0;

    //only guide if tracking
    if (TrackState == SCOPE_TRACKING)
    {

        // If already moving (no pulse command), then stop movement
        if (MovementWESP.getState() == IPS_BUSY)
        {
            int dir = MovementWESP.findOnSwitchIndex();
            MoveWE(dir == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
        }

        if (GuideWETID)
        {
            IERmTimer(GuideWETID);
            GuideWETID = 0;
        }

        isPulsingWE = true;
        start_pmc8_guide(PortFD, PMC8_W, (int)ms, timetaken_us, TrackRateNP[AXIS_RA].getValue() / SOLAR_SECOND);

        timeremain_ms = (int)(ms - ((float)timetaken_us) / 1000.0);

        if (timeremain_ms < 0)
            timeremain_ms = 0;

        ret = IPS_BUSY;
    }
    else
    {
        LOG_INFO("Mount not tracking--cannot guide.");
    }
    GuideWETID      = IEAddTimer(timeremain_ms, guideTimeoutHelperW, this);
    return ret;
}

void PMC8::guideTimeout(PMC8_DIRECTION calldir)
{
    // end previous pulse command
    stop_pmc8_guide(PortFD, calldir);

    if (calldir == PMC8_N || calldir == PMC8_S)
    {
        isPulsingNS = false;
        GuideNSNP[0].setValue(0);
        GuideNSNP[1].setValue(0);
        GuideNSNP.setState(IPS_IDLE);
        GuideNSTID            = 0;
        GuideNSNP.apply();
    }
    if (calldir == PMC8_W || calldir == PMC8_E)
    {
        isPulsingWE = false;
        GuideWENP[0].setValue(0);
        GuideWENP[1].setValue(0);
        GuideWENP.setState(IPS_IDLE);
        GuideWETID            = 0;
        GuideWENP.apply();
    }

    LOG_DEBUG("GUIDE CMD COMPLETED");
}

//GUIDE The timer helper functions.
void PMC8::guideTimeoutHelperN(void *p)
{
    static_cast<PMC8*>(p)->guideTimeout(PMC8_N);
}
void PMC8::guideTimeoutHelperS(void *p)
{
    static_cast<PMC8*>(p)->guideTimeout(PMC8_S);
}
void PMC8::guideTimeoutHelperW(void *p)
{
    static_cast<PMC8*>(p)->guideTimeout(PMC8_W);
}
void PMC8::guideTimeoutHelperE(void *p)
{
    static_cast<PMC8*>(p)->guideTimeout(PMC8_E);
}

bool PMC8::SetSlewRate(int index)
{

    INDI_UNUSED(index);

    // slew rate is rate for MoveEW/MOVENE commands - not for GOTOs!!!

    // just return true - we will check SlewRateSP when we do actually moves
    return true;
}

bool PMC8::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &SerialCableTypeSP);
    IUSaveConfigSwitch(fp, &MountTypeSP);
    IUSaveConfigNumber(fp, &RampNP);
    IUSaveConfigNumber(fp, &LegacyGuideRateNP);
    IUSaveConfigSwitch(fp, &PostGotoSP);

    return true;
}

void PMC8::mountSim()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt, da, dx;
    int nlocked;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&tv, nullptr);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt  = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec) / 1e6;
    ltv = tv;
    da  = SLEWRATE * dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act accordingly */
    switch (TrackState)
    {
        case SCOPE_IDLE:
            currentRA += (TrackRateNP[AXIS_RA].getValue() / 3600.0 * dt) / 15.0;
            currentRA = range24(currentRA);
            break;

        case SCOPE_TRACKING:
            if (TrackModeSP[1].getState() == ISS_ON)
            {
                currentRA  += ( ((TRACKRATE_SIDEREAL / 3600.0) - (TrackRateNP[AXIS_RA].getValue() / 3600.0)) * dt) / 15.0;
                currentDEC += ( (TrackRateNP[AXIS_DE].getValue() / 3600.0) * dt);
            }
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
                currentRA += da / 15.;
            else
                currentRA -= da / 15.;

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
                if (TrackState == SCOPE_SLEWING)
                    set_pmc8_sim_system_status(ST_TRACKING);
                else
                    set_pmc8_sim_system_status(ST_PARKED);
            }

            break;

        case SCOPE_PARKED:
            // setting system status to parked will automatically
            // set the simulated RA/DEC to park position so reread
            set_pmc8_sim_system_status(ST_PARKED);
            get_pmc8_coords(PortFD, currentRA, currentDEC);

            break;

        default:
            break;
    }

    set_pmc8_sim_ra(currentRA);
    set_pmc8_sim_dec(currentDEC);
}

#if 0
// PMC8 only parks to motor position (0, 0) currently
bool PMC8::SetCurrentPark()
{
    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);

    return true;
}

bool PMC8::SetDefaultPark()
{
    // By default set RA to HA
    SetAxis1Park(ln_get_apparent_sidereal_time(ln_get_julian_from_sys()));

    // Set DEC to 90 or -90 depending on the hemisphere
    //    SetAxis2Park((HemisphereS[HEMI_NORTH].s == ISS_ON) ? 90 : -90);
    SetAxis2Park(90);

    return true;
}
#else
bool PMC8::SetCurrentPark()
{
    LOG_ERROR("PPMC8::SetCurrentPark() not implemented!");
    return false;
}

bool PMC8::SetDefaultPark()
{
    LOG_ERROR("PMC8::SetDefaultPark() not implemented!");
    return false;
}
#endif

uint8_t PMC8::convertToPMC8TrackMode(uint8_t mode)
{
    switch (mode)
    {
        case TRACK_SIDEREAL:
            return PMC8_TRACK_SIDEREAL;
            break;
        case TRACK_LUNAR:
            return PMC8_TRACK_LUNAR;
            break;
        case TRACK_SOLAR:
            return PMC8_TRACK_SOLAR;
            break;
        case TRACK_CUSTOM:
            return PMC8_TRACK_CUSTOM;
            break;
        default:
            return PMC8_TRACK_UNDEFINED;
    }
}

uint8_t PMC8::convertFromPMC8TrackMode(uint8_t mode)
{
    switch (mode)
    {
        case PMC8_TRACK_SIDEREAL:
            return TRACK_SIDEREAL;
            break;
        case PMC8_TRACK_LUNAR:
            return TRACK_LUNAR;
            break;
        case PMC8_TRACK_SOLAR:
            return TRACK_SOLAR;
            break;
        default:
            return TRACK_CUSTOM;
    }
}

bool PMC8::SetTrackMode(uint8_t mode)
{
    uint8_t pmc8_mode;

    LOGF_DEBUG("PMC8::SetTrackMode called mode=%d", mode);

    pmc8_mode = convertToPMC8TrackMode(mode);

    if (pmc8_mode == PMC8_TRACK_UNDEFINED)
    {
        LOGF_ERROR("PMC8::SetTrackMode mode=%d not supported!", mode);
        return false;
    }

    if (pmc8_mode == PMC8_TRACK_CUSTOM)
    {
        if (set_pmc8_ra_tracking(PortFD, TrackRateNP[AXIS_RA].getValue() / SOLAR_SECOND))
        {
            return true;
        }
    }
    else
    {
        if (set_pmc8_track_mode(PortFD, pmc8_mode))
            return true;
    }

    return false;
}

bool PMC8::SetTrackRate(double raRate, double deRate)
{
    static bool deRateWarning = true;
    double pmc8RARate;

    LOGF_INFO("Custom tracking rate set: raRate=%f  deRate=%f", raRate, deRate);

    // for now just send rate
    pmc8RARate = raRate / SOLAR_SECOND;

    if (deRate != 0 && deRateWarning)
    {
        // Only send warning once per session
        deRateWarning = false;
        LOG_WARN("Custom Declination tracking rate is not implemented yet.");
    }

    if (set_pmc8_ra_tracking(PortFD, pmc8RARate))
        return true;

    LOG_ERROR("PMC8::SetTrackRate not implemented!");
    return false;
}

bool PMC8::SetTrackEnabled(bool enabled)
{

    LOGF_DEBUG("PMC8::SetTrackEnabled called enabled=%d", enabled);

    // need to determine current tracking mode and start tracking
    if (enabled)
    {
        if (!SetTrackMode(TrackModeSP.findOnSwitchIndex()))
        {
            LOG_ERROR("PMC8::SetTrackEnabled - unable to enable tracking");
            return false;
        }
    }
    else
    {
        bool rc;

        rc = set_pmc8_custom_ra_track_rate(PortFD, 0);
        if (!rc)
        {
            LOG_ERROR("PMC8::SetTrackEnabled - unable to set RA track rate to 0");
            return false;
        }

        // currently only support tracking rate in RA
        //        rc=set_pmc8_custom_dec_track_rate(PortFD, 0);
        //        if (!rc)
        //        {
        //            LOG_ERROR("PMC8::SetTrackREnabled - unable to set DEC track rate to 0");
        //            return false;
        //        }
    }

    return true;
}

