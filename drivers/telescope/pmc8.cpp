/*
    INDI Explore Scientific PMC8 driver

    Copyright (C) 2017 Michael Fulbright

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

#include <libnova/sidereal_time.h>

#include <memory>

#include <math.h>
#include <string.h>

/* Simulation Parameters */
#define SLEWRATE 3          /* slew rate, degrees/s */

#define MOUNTINFO_TAB "Mount Info"

static std::unique_ptr<PMC8> scope(new PMC8());

void ISGetProperties(const char *dev)
{
    scope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    scope->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    scope->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    scope->ISNewNumber(dev, name, values, names, num);
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
    scope->ISSnoopDevice(root);
}

/* Constructor */
PMC8::PMC8()
{
    currentRA  = ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
    currentDEC = 90;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_TRACK_RATE |
                           TELESCOPE_HAS_LOCATION,
                           4);

    setVersion(0, 2);
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

    // Mount Type
    IUFillSwitch(&MountTypeS[MOUNT_G11], "MOUNT_G11", "G11", ISS_OFF);
    IUFillSwitch(&MountTypeS[MOUNT_EXOS2], "MOUNT_EXOS2", "EXOS2", ISS_OFF);
    IUFillSwitch(&MountTypeS[MOUNT_iEXOS100], "MOUNT_iEXOS100", "iEXOS100", ISS_OFF);
    IUFillSwitchVector(&MountTypeSP, MountTypeS, 3, getDeviceName(), "MOUNT_TYPE", "Mount Type", CONNECTION_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Guess type from device name
    if (strstr(getDeviceName(), "EXOS2"))
        MountTypeS[MOUNT_EXOS2].s = ISS_ON;
    else if (strstr(getDeviceName(), "iEXOS100"))
        MountTypeS[MOUNT_iEXOS100].s = ISS_ON;
    else
        MountTypeS[MOUNT_G11].s = ISS_ON;


    /* Tracking Mode */
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Set TrackRate limits within +/- 0.0100 of Sidereal rate
    //    TrackRateN[AXIS_RA].min = TRACKRATE_SIDEREAL - 0.01;
    //    TrackRateN[AXIS_RA].max = TRACKRATE_SIDEREAL + 0.01;
    //    TrackRateN[AXIS_DE].min = -0.01;
    //    TrackRateN[AXIS_DE].max = 0.01;

    // relabel move speeds
    strcpy(SlewRateSP.sp[0].label, "4x");
    strcpy(SlewRateSP.sp[1].label, "16x");
    strcpy(SlewRateSP.sp[2].label, "64x");
    strcpy(SlewRateSP.sp[3].label, "256x");

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[0], "GUIDE_RATE", "x Sidereal", "%g", 0.1, 1.0, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 1, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    initGuiderProperties(getDeviceName(), MOTION_TAB);

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
        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);
        defineNumber(&GuideRateNP);

        defineText(&FirmwareTP);

        // do not support park position
        deleteProperty(ParkPositionNP.name);
        deleteProperty(ParkOptionSP.name);

        getStartupData();
    }
    else
    {
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.name);

        deleteProperty(FirmwareTP.name);
    }

    return true;
}

void PMC8::getStartupData()
{
    LOG_DEBUG("Getting firmware data...");
    if (get_pmc8_firmware(PortFD, &firmwareInfo))
    {
        const char *c;

        // FIXME - Need to add code to get firmware data
        FirmwareTP.s = IPS_OK;
        c = firmwareInfo.MainBoardFirmware.c_str();
        LOGF_INFO("firmware = %s.", c);
        IUSaveText(&FirmwareT[0], c);
        IDSetText(&FirmwareTP, nullptr);
    }

    // PMC8 doesn't store location permanently so read from config and set
    // Convert to INDI standard longitude (0 to 360 Eastward)
    double longitude = LocationN[LOCATION_LONGITUDE].value;
    double latitude = LocationN[LOCATION_LATITUDE].value;

    // must also keep "low level" aware of position to convert motor counts to RA/DEC
    set_pmc8_location(latitude, longitude);

    // seems like best place to put a warning that will be seen in log window of EKOS/etc
    LOG_INFO("The PMC-Eight driver is in BETA development currently.");
    LOG_INFO("Be prepared to intervene if something unexpected occurs.");

#if 0
    // FIXEME - Need to handle southern hemisphere for DEC?
    double HA  = ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
    double DEC = 90;

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
    if (!strcmp(dev, getDeviceName()))
    {
        // FIXME - will add setting guide rate when firmware supports
        // Guiding Rate
        if (!strcmp(name, GuideRateNP.name))
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);

            if (set_pmc8_guide_rate(PortFD, GuideRateN[0].value))
                GuideRateNP.s = IPS_OK;
            else
                GuideRateNP.s = IPS_ALERT;

            IDSetNumber(&GuideRateNP, nullptr);

            return true;
        }

        if (!strcmp(name, GuideNSNP.name) || !strcmp(name, GuideWENP.name))
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

void PMC8::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);
    defineSwitch(&MountTypeSP);
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

            // Set iEXOS100 baud rate to 115200
            if (!isConnected() && currentMountIndex == MOUNT_iEXOS100)
                serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

            set_pmc8_myMount(currentMountIndex);
            MountTypeSP.s = IPS_OK;
            IDSetSwitch(&MountTypeSP, nullptr);
            //		defineSwitch(&MountTypeSP);
            return true;
        }


    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool PMC8::ReadScopeStatus()
{
    bool rc = false;

    if (isSimulation())
        mountSim();

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
                    LOG_INFO("Slew complete, tracking...");
                    TrackState = SCOPE_TRACKING;

                    if (!SetTrackEnabled(true))
                    {
                        LOG_ERROR("slew complete - unable to enable tracking");
                        return false;
                    }

                    if (!SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP)))
                    {
                        LOG_ERROR("slew complete - unable to set track mode");
                        return false;
                    }
                }
            }

            break;

        case SCOPE_PARKING:
            // are we done?
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
    char RAStr[64] = {0}, DecStr[64] = {0};

    targetRA  = r;
    targetDEC = d;

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    DEBUGF(INDI::Logger::DBG_SESSION, "Slewing to RA: %s - DEC: %s", RAStr, DecStr);


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

    DEBUGF(INDI::Logger::DBG_SESSION, "Syncing to RA: %s - DEC: %s", RAStr, DecStr);

    if (sync_pmc8(PortFD, ra, dec) == false)
    {
        LOG_ERROR("Failed to sync.");
    }

    EqNP.s     = IPS_OK;

    currentRA  = ra;
    currentDEC = dec;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool PMC8::Abort()
{

    //GUIDE Abort guide operations.
    if (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
    {
        GuideNSNP.s = GuideWENP.s = IPS_IDLE;
        GuideNSN[0].value = GuideNSN[1].value = 0.0;
        GuideWEN[0].value = GuideWEN[1].value = 0.0;

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
        IDSetNumber(&GuideNSNP, nullptr);
        IDSetNumber(&GuideWENP, nullptr);

        return true;
    }

    return abort_pmc8(PortFD);
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
        set_pmc8_sim_move_rate(PMC8_MOVE_64X);
        //        set_pmc8_sim_hemisphere(HEMI_NORTH);
    }

    if (check_pmc8_connection(PortFD) == false)
        return false;

    return true;
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

    // do not support Southern Hemisphere yet!
    if (latitude < 0)
    {
        LOG_ERROR("Southern Hemisphere not currently supported!");
        return false;
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

bool PMC8::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    // read desired move rate
    int currentIndex = IUFindOnSwitchIndex(&SlewRateSP);

    LOGF_DEBUG("MoveNS at slew index %d", currentIndex);

    switch (command)
    {
        case MOTION_START:
            if (start_pmc8_motion(PortFD, (dir == DIRECTION_NORTH ? PMC8_N : PMC8_S), currentIndex) == false)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
            {
                LOGF_INFO("Moving toward %s.", (dir == DIRECTION_NORTH) ? "North" : "South");
            }
            break;

        case MOTION_STOP:
            if (stop_pmc8_motion(PortFD, (dir == DIRECTION_NORTH ? PMC8_N : PMC8_S)) == false)
            {
                LOG_ERROR("Error stopping N/S motion.");
                return false;
            }
            else
            {
                LOGF_INFO("%s motion stopped.", (dir == DIRECTION_NORTH) ? "North" : "South");
            }
            break;
    }

    return true;
}

bool PMC8::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    // read desired move rate
    int currentIndex = IUFindOnSwitchIndex(&SlewRateSP);

    LOGF_DEBUG("MoveWE at slew index %d", currentIndex);

    switch (command)
    {
        case MOTION_START:
            if (start_pmc8_motion(PortFD, (dir == DIRECTION_WEST ? PMC8_W : PMC8_E), currentIndex) == false)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
            {
                LOGF_INFO("Moving toward %s.", (dir == DIRECTION_WEST) ? "West" : "East");
            }
            break;

        case MOTION_STOP:
            if (stop_pmc8_motion(PortFD, (dir == DIRECTION_WEST ? PMC8_W : PMC8_E)) == false)
            {
                LOG_ERROR("Error stopping W/E motion.");
                return false;
            }
            else
            {
                LOGF_INFO("%s motion stopped.", (dir == DIRECTION_WEST) ? "West" : "East");

                // restore tracking

                if (TrackState == SCOPE_TRACKING)
                {
                    LOG_INFO("Move E/W complete, tracking...");

                    if (!SetTrackEnabled(true))
                    {
                        LOG_ERROR("slew complete - unable to enable tracking");
                        return false;
                    }

                    if (!SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP)))
                    {
                        LOG_ERROR("slew complete - unable to set track mode");
                        return false;
                    }
                }
            }
            break;
    }

    return true;
}

IPState PMC8::GuideNorth(uint32_t ms)
{
    long timetaken_us;
    int timeremain_ms;

    // If already moving, then stop movement
    if (MovementNSSP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementNSSP);
        MoveNS(dir == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    start_pmc8_guide(PortFD, PMC8_N, (int)ms, timetaken_us);

    timeremain_ms = (int)(ms - ((float)timetaken_us) / 1000.0);

    if (timeremain_ms < 0)
        timeremain_ms = 0;

    GuideNSTID = IEAddTimer(timeremain_ms, guideTimeoutHelperN, this);

    return IPS_BUSY;
}

IPState PMC8::GuideSouth(uint32_t ms)
{
    long timetaken_us;
    int timeremain_ms;

    // If already moving, then stop movement
    if (MovementNSSP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementNSSP);
        MoveNS(dir == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    start_pmc8_guide(PortFD, PMC8_S, (int)ms, timetaken_us);

    timeremain_ms = (int)(ms - ((float)timetaken_us) / 1000.0);

    if (timeremain_ms < 0)
        timeremain_ms = 0;

    GuideNSTID      = IEAddTimer(timeremain_ms, guideTimeoutHelperS, this);

    return IPS_BUSY;
}

IPState PMC8::GuideEast(uint32_t ms)
{
    long timetaken_us;
    int timeremain_ms;

    // If already moving (no pulse command), then stop movement
    if (MovementWESP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementWESP);
        MoveWE(dir == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    start_pmc8_guide(PortFD, PMC8_E, (int)ms, timetaken_us);

    timeremain_ms = (int)(ms - ((float)timetaken_us) / 1000.0);

    if (timeremain_ms < 0)
        timeremain_ms = 0;

    GuideWETID      = IEAddTimer(timeremain_ms, guideTimeoutHelperE, this);
    return IPS_BUSY;
}

IPState PMC8::GuideWest(uint32_t ms)
{
    long timetaken_us;
    int timeremain_ms;

    // If already moving (no pulse command), then stop movement
    if (MovementWESP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementWESP);
        MoveWE(dir == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    start_pmc8_guide(PortFD, PMC8_W, (int)ms, timetaken_us);

    timeremain_ms = (int)(ms - ((float)timetaken_us) / 1000.0);

    if (timeremain_ms < 0)
        timeremain_ms = 0;

    GuideWETID      = IEAddTimer(timeremain_ms, guideTimeoutHelperW, this);
    return IPS_BUSY;
}

void PMC8::guideTimeout(PMC8_DIRECTION calldir)
{
    // end previous pulse command
    stop_pmc8_guide(PortFD, calldir);

    if (calldir == PMC8_N || calldir == PMC8_S)
    {
        GuideNSNP.np[0].value = 0;
        GuideNSNP.np[1].value = 0;
        GuideNSNP.s           = IPS_IDLE;
        GuideNSTID            = 0;
        IDSetNumber(&GuideNSNP, nullptr);
    }
    if (calldir == PMC8_W || calldir == PMC8_E)
    {
        GuideWENP.np[0].value = 0;
        GuideWENP.np[1].value = 0;
        GuideWENP.s           = IPS_IDLE;
        GuideWETID            = 0;
        IDSetNumber(&GuideWENP, nullptr);
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

    IUSaveConfigSwitch(fp, &MountTypeSP);
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

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
    switch (TrackState)
    {
        case SCOPE_IDLE:
            currentRA += (TrackRateN[AXIS_RA].value / 3600.0 * dt) / 15.0;
            currentRA = range24(currentRA);
            break;

        case SCOPE_TRACKING:
            if (TrackModeS[1].s == ISS_ON)
            {
                currentRA  += ( ((TRACKRATE_SIDEREAL / 3600.0) - (TrackRateN[AXIS_RA].value / 3600.0)) * dt) / 15.0;
                currentDEC += ( (TrackRateN[AXIS_DE].value / 3600.0) * dt);
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

bool PMC8::SetTrackMode(uint8_t mode)
{
    uint pmc8_mode;

    LOGF_DEBUG("PMC8::SetTrackMode called mode=%d", mode);

    // FIXME - Need to make sure track modes are handled properly!
    //PMC8_TRACK_RATE rate = static_cast<PMC8_TRACK_RATE>(mode);

    switch (mode)
    {
        case TRACK_SIDEREAL:
            pmc8_mode = PMC8_TRACK_SIDEREAL;
            break;
        case TRACK_LUNAR:
            pmc8_mode = PMC8_TRACK_LUNAR;
            break;
        case TRACK_SOLAR:
            pmc8_mode = PMC8_TRACK_SOLAR;
            break;
        case TRACK_CUSTOM:
            pmc8_mode = PMC8_TRACK_CUSTOM;
            break;
        default:
            LOGF_ERROR("PMC8::SetTrackMode mode=%d not supported!", mode);
            return false;
    }

    if (pmc8_mode == PMC8_TRACK_CUSTOM)
    {
        if (set_pmc8_custom_ra_track_rate(PortFD, TrackRateN[AXIS_RA].value))
            return true;
    }
    else
    {
        if (set_pmc8_track_mode(PortFD, mode))
            return true;
    }

    return false;
}

bool PMC8::SetTrackRate(double raRate, double deRate)
{
    static bool deRateWarning = true;
    double pmc8RARate;

    LOGF_DEBUG("PMC8::SetTrackRate called raRate=%f  deRate=%f", raRate, deRate);

    // Convert to arcsecs/s to +/- 0.0100 accepted by
    //double pmc8RARate = raRate - TRACKRATE_SIDEREAL;

    // for now just send rate
    pmc8RARate = raRate;

    if (deRate != 0 && deRateWarning)
    {
        // Only send warning once per session
        deRateWarning = false;
        LOG_WARN("Custom Declination tracking rate is not implemented yet.");
    }

    if (set_pmc8_custom_ra_track_rate(PortFD, pmc8RARate))
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
        if (!SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP)))
        {
            LOG_ERROR("PMC8::SetTrackREnabled - unable to enable tracking");
            return false;
        }
    }
    else
    {
        bool rc;

        rc = set_pmc8_custom_ra_track_rate(PortFD, 0);
        if (!rc)
        {
            LOG_ERROR("PMC8::SetTrackREnabled - unable to set RA track rate to 0");
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

