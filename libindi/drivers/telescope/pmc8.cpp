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

#include "pmc8.h"

#include "indicom.h"

#include <libnova/sidereal_time.h>

#include <memory>

#include <math.h>
#include <string.h>

/* Simulation Parameters */
#define SLEWRATE 1          /* slew rate, degrees/s */
#define POLLMS   1000       /* poll period, ms */

#define MOUNTINFO_TAB "Mount Info"

// We declare an auto pointer to IEQPro.
std::unique_ptr<PMC8> scope(new PMC8());

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
    set_pmc8_device(getDeviceName());

    //ctor
    currentRA  = ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
    currentDEC = 90;


    // FIXME - (MSF) Not sure we even need a scopeInfo object(?)
//    scopeInfo.gpsStatus    = GPS_OFF;
//    scopeInfo.systemStatus = ST_STOPPED;
//    scopeInfo.trackRate    = TR_SIDEREAL;
//    scopeInfo.slewRate     = SR_1;
//    scopeInfo.timeSource   = TS_RS232;
//    scopeInfo.hemisphere   = HEMI_NORTH;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_TRACK_RATE |
                           TELESCOPE_HAS_LOCATION,
                           4);
}

PMC8::~PMC8()
{
}

const char *PMC8::getDefaultName()
{
    return (const char *)"PMC8";
}

bool PMC8::initProperties()
{
    INDI::Telescope::initProperties();

    /* Firmware */
    // FIXME - (MSF) Need to figure out which fields we're keeping and initialize
//    IUFillText(&FirmwareT[FW_MODEL], "Model", "", 0);
//    IUFillText(&FirmwareT[FW_BOARD], "Board", "", 0);
//    IUFillText(&FirmwareT[FW_CONTROLLER], "Controller", "", 0);
//    IUFillText(&FirmwareT[FW_RA], "RA", "", 0);
//    IUFillText(&FirmwareT[FW_DEC], "DEC", "", 0);
//    IUFillTextVector(&FirmwareTP, FirmwareT, 5, getDeviceName(), "Firmware Info", "", MOUNTINFO_TAB, IP_RO, 0,
//                     IPS_IDLE);

    /* Tracking Mode */
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
//    AddTrackMode("TRACK_KING", "King");
//    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Set TrackRate limits within +/- 0.0100 of Sidereal rate
//    TrackRateN[AXIS_RA].min = TRACKRATE_SIDEREAL - 0.01;
//    TrackRateN[AXIS_RA].max = TRACKRATE_SIDEREAL + 0.01;
//    TrackRateN[AXIS_DE].min = -0.01;
//    TrackRateN[AXIS_DE].max = 0.01;


    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[0], "GUIDE_RATE", "x Sidereal", "%g", 0.1, 0.9, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 1, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    TrackState = SCOPE_IDLE;

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    SetParkDataType(PARK_RA_DEC);

    addAuxControls();

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
    DEBUG(INDI::Logger::DBG_DEBUG, "Getting firmware data...");
    if (get_pmc8_firmware(PortFD, &firmwareInfo))
    {
        // FIXME - (MSF) Need to add code to get firmware data
//        IUSaveText(&FirmwareT[0], firmwareInfo.Model.c_str());
//        IUSaveText(&FirmwareT[1], firmwareInfo.MainBoardFirmware.c_str());
//        IUSaveText(&FirmwareT[2], firmwareInfo.ControllerFirmware.c_str());
//        IUSaveText(&FirmwareT[3], firmwareInfo.RAFirmware.c_str());
//        IUSaveText(&FirmwareT[4], firmwareInfo.DEFirmware.c_str());

        FirmwareTP.s = IPS_OK;
        IDSetText(&FirmwareTP, nullptr);
    }

#if 0
    // FIXME - (MSF) Need to implement guide rate functions
    DEBUG(INDI::Logger::DBG_DEBUG, "Getting guiding rate...");
    double guideRate = 0;
    if (get_pmc8_guide_rate(PortFD, &guideRate))
    {
        GuideRateN[0].value = guideRate;
        IDSetNumber(&GuideRateNP, nullptr);
    }
#endif

    // PMC8 doesn't store location permanently so read from config and set
    // Convert to INDI standard longitude (0 to 360 Eastward)

    double longitude;
    double latitude;

#if 0
    // for testing
    longitude = -78.0;
    latitude = 35.5;

    if (longitude < 0)
        longitude += 360;

    LocationN[LOCATION_LATITUDE].value  = latitude;
    LocationN[LOCATION_LONGITUDE].value = longitude;
    LocationNP.s                        = IPS_OK;
    IDSetNumber(&LocationNP, nullptr);
#endif

    longitude = LocationN[LOCATION_LONGITUDE].value;
    latitude = LocationN[LOCATION_LATITUDE].value;

    // must also keep "low level" aware of position to convert motor counts to RA/DEC
    set_pmc8_location(latitude, longitude);


#if 0
    // FIXEME - (MSF) Need to handle southern hemisphere for DEC?
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
    // FIXME - (MSF) Need to implement simulation functionality
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
        // FIXME - (MSF) will add setting guide rate when firmware supports
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

bool PMC8::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(getDeviceName(), dev))
    {

    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool PMC8::ReadScopeStatus()
{
    bool rc = false;

    PMC8Info newInfo;

    if (isSimulation())
        mountSim();

    rc = get_pmc8_status(PortFD, &newInfo);

    if (rc)
    {
        /*
        TelescopeTrackMode trackMode = TRACK_SIDEREAL;

        switch (newInfo.trackRate)
        {
            case TR_SIDEREAL:
                trackMode = TRACK_SIDEREAL;
                break;
            case TR_SOLAR:
                trackMode = TRACK_SOLAR;
                break;
            case TR_LUNAR:
                trackMode = TRACK_LUNAR;
                break;
            case TR_KING:
                trackMode = TRACK_SIDEREAL;
                break;
            case TR_CUSTOM:
                trackMode = TRACK_CUSTOM;
                break;
        }*/

#if 0
        // FIXME - (MSF) do we need a status for the mount now we have new tracking code?
        switch (newInfo.systemStatus)
        {
            case ST_STOPPED:
                TrackModeSP.s = IPS_IDLE;
                TrackState    = SCOPE_IDLE;
                break;
            case ST_PARKED:
                TrackModeSP.s = IPS_IDLE;
                TrackState    = SCOPE_PARKED;
                if (isParked() == false)
                    SetParked(true);
                break;
            case ST_HOME:
                TrackModeSP.s = IPS_IDLE;
                TrackState    = SCOPE_IDLE;
                break;
            case ST_SLEWING:
            case ST_MERIDIAN_FLIPPING:
                if (TrackState != SCOPE_SLEWING && TrackState != SCOPE_PARKING)
                    TrackState = SCOPE_SLEWING;
                break;
            case ST_TRACKING_PEC_OFF:
            case ST_TRACKING_PEC_ON:
            case ST_GUIDING:
                TrackModeSP.s = IPS_BUSY;
                TrackState    = SCOPE_TRACKING;
                if (scopeInfo.systemStatus == ST_SLEWING)
                    DEBUG(INDI::Logger::DBG_SESSION, "Slew complete, tracking...");
                else if (scopeInfo.systemStatus == ST_MERIDIAN_FLIPPING)
                    DEBUG(INDI::Logger::DBG_SESSION, "Meridian flip complete, tracking...");
                break;
        }
        IUResetSwitch(&TrackModeSP);
        TrackModeS[newInfo.trackRate].s = ISS_ON;
        IDSetSwitch(&TrackModeSP, nullptr);
#endif

        scopeInfo = newInfo;
    }

    rc = get_pmc8_coords(PortFD, currentRA, currentDEC);

    if (rc)
        NewRaDec(currentRA, currentDEC);

    return rc;
}

bool PMC8::Goto(double r, double d)
{
    targetRA  = r;
    targetDEC = d;
    char RAStr[64]={0}, DecStr[64]={0};

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);

    if (slew_pmc8(PortFD, r, d) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to slew.");
        return false;
    }

    TrackState = SCOPE_SLEWING;

    return true;
}

bool PMC8::Sync(double ra, double dec)
{
    if (sync_pmc8(PortFD, ra, dec) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to sync.");
    }

    EqNP.s     = IPS_OK;

    currentRA  = ra;
    currentDEC = dec;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool PMC8::Abort()
{
    return abort_pmc8(PortFD);
}

bool PMC8::Park()
{
#if 0
    // FIXME - (MSF) Currently only support parking at motor position (0, 0)
    targetRA  = GetAxis1Park();
    targetDEC = GetAxis2Park();
    if (set_pmc8_radec(PortFD, r, d) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting RA/DEC.");
        return false;
    }
#endif

    if (park_pmc8(PortFD))
    {
//        char RAStr[64]={0}, DecStr[64]={0};
//        fs_sexa(RAStr, targetRA, 2, 3600);
//        fs_sexa(DecStr, targetDEC, 2, 3600);

        TrackState = SCOPE_PARKING;
//        DEBUGF(INDI::Logger::DBG_SESSION, "Telescope parking in progress to RA: %s DEC: %s", RAStr, DecStr);
        DEBUG(INDI::Logger::DBG_SESSION, "Telescope parking in progress to motor position (0, 0)");
        return true;
    }
    else
        return false;
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
        return false;
}

bool PMC8::Handshake()
{
    if (isSimulation())
    {
        // FIXME - (MSF) Need to handle handshake for simulation
//        set_sim_gps_status(GPS_DATA_OK);
//        set_sim_system_status(ST_STOPPED);
//        set_sim_track_rate(TR_SIDEREAL);
//        set_sim_slew_rate(SR_3);
//        set_sim_time_source(TS_GPS);
//        set_sim_hemisphere(HEMI_NORTH);
    }

    if (check_pmc8_connection(PortFD) == false)
        return false;

    return true;
}

bool PMC8::updateTime(ln_date *utc, double utc_offset)
{
#if 0
    struct ln_zonedate ltm;

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600.0);

    // Set Local Time
    if (set_ieqpro_local_time(PortFD, ltm.hours, ltm.minutes, ltm.seconds) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting local time.");
        return false;
    }

    // Send it as YY (i.e. 2015 --> 15)
    ltm.years -= 2000;

    // Set Local date
    if (set_ieqpro_local_date(PortFD, ltm.years, ltm.months, ltm.days) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting local date.");
        return false;
    }

    // UTC Offset
    if (set_ieqpro_utc_offset(PortFD, utc_offset) == false)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting UTC Offset.");
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Time and date updated.");

    return true;
#else
    DEBUG(INDI::Logger::DBG_ERROR, "PMC8::updateTime() not implemented!");
    return false;
#endif
}

bool PMC8::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (longitude > 180)
        longitude -= 360;

    // must also keep "low level" aware of position to convert motor counts to RA/DEC
    set_pmc8_location(latitude, longitude);

    char l[32]={0}, L[32]={0};
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    DEBUGF(INDI::Logger::DBG_SESSION, "Site location updated to Lat %.32s - Long %.32s", l, L);

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
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    switch (command)
    {
        case MOTION_START:
            if (start_pmc8_motion(PortFD, (dir == DIRECTION_NORTH ? PMC8_N : PMC8_S)) == false)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Error setting N/S motion direction.");
                return false;
            }
            else
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving toward %s.", (dir == DIRECTION_NORTH) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (stop_pmc8_motion(PortFD, (dir == DIRECTION_NORTH ? PMC8_N : PMC8_S)) == false)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Error stopping N/S motion.");
                return false;
            }
            else
                DEBUGF(INDI::Logger::DBG_SESSION, "%s motion stopped.", (dir == DIRECTION_NORTH) ? "North" : "South");
            break;
    }

    return true;
}

bool PMC8::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Please unpark the mount before issuing any motion commands.");
        return false;
    }

    switch (command)
    {
        case MOTION_START:
            if (start_pmc8_motion(PortFD, (dir == DIRECTION_WEST ? PMC8_W : PMC8_E)) == false)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Error setting N/S motion direction.");
                return false;
            }
            else
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving toward %s.", (dir == DIRECTION_WEST) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (stop_pmc8_motion(PortFD, (dir == DIRECTION_WEST ? PMC8_W : PMC8_E)) == false)
            {
                DEBUG(INDI::Logger::DBG_ERROR, "Error stopping W/E motion.");
                return false;
            }
            else
                DEBUGF(INDI::Logger::DBG_SESSION, "%s motion stopped.", (dir == DIRECTION_WEST) ? "West" : "East");
            break;
    }

    return true;
}

#if 0
// not implemented on PMC8
IPState PMC8::GuideNorth(float ms)
{
    bool rc = start_ieqpro_guide(PortFD, PMC8_N, (int)ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState PMC8::GuideSouth(float ms)
{
    bool rc = start_ieqpro_guide(PortFD, PMC8_S, (int)ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState PMC8::GuideEast(float ms)
{
    bool rc = start_ieqpro_guide(PortFD, PMC8_E, (int)ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState PMC8::GuideWest(float ms)
{
    bool rc = start_ieqpro_guide(PortFD, PMC8_W, (int)ms);
    return (rc ? IPS_OK : IPS_ALERT);
}
#else
IPState PMC8::GuideNorth(float ms)
{
    DEBUG(INDI::Logger::DBG_ERROR, "PMC8::GuideNorth(float ms) not implemented!");
    return IPS_ALERT;
}
IPState PMC8::GuideSouth(float ms)
{
    DEBUG(INDI::Logger::DBG_ERROR, "PMC8::GuideSouth(float ms) not implemented!");
    return IPS_ALERT;
}
IPState PMC8::GuideEast(float ms)
{
    DEBUG(INDI::Logger::DBG_ERROR, "PMC8::GuideEast(int index)) not implemented!");
    return IPS_ALERT;
}
IPState PMC8::GuideWest(float ms)
{
    DEBUG(INDI::Logger::DBG_ERROR, "PMC8::GuideWest(float ms) not implemented!");
    return IPS_ALERT;
}
#endif

bool PMC8::SetSlewRate(int index)
{
    // According to PMC-Eight programmer reference the slew rate is always 25x the tracking rate!
    DEBUG(INDI::Logger::DBG_ERROR, "PMC8::SetSlewRate(int index)) not implemented!");
    return false;
}

bool PMC8::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

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
            currentRA += (TrackRateN[AXIS_RA].value/3600.0 * dt) / 15.0;
            currentRA = range24(currentRA);
            break;

        case SCOPE_TRACKING:
        if (TrackModeS[1].s == ISS_ON)
        {
            currentRA  += ( ((TRACKRATE_SIDEREAL/3600.0) - (TrackRateN[AXIS_RA].value/3600.0)) * dt) / 15.0;
            currentDEC += ( (TrackRateN[AXIS_DE].value/3600.0) * dt);
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
                // FIXME - (MSF) need to implement sim functionality
#if 0
                if (TrackState == SCOPE_SLEWING)
                    set_sim_system_status(ST_TRACKING_PEC_OFF);
                else
                    set_sim_system_status(ST_PARKED);
#endif
            }

            break;

        default:
            break;
    }

    set_sim_ra(currentRA);
    set_sim_dec(currentDEC);
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
    DEBUG(INDI::Logger::DBG_ERROR, "PPMC8::SetCurrentPark() not implemented!");
    return false;
}

bool PMC8::SetDefaultPark()
{
    DEBUG(INDI::Logger::DBG_ERROR, "PMC8::SetDefaultPark() not implemented!");
    return false;
}
#endif

bool PMC8::SetTrackMode(uint8_t mode)
{
    DEBUGF(INDI::Logger::DBG_DEBUG, "PMC8::SetTrackMode called mode=%d", mode);

    // FIXME - (MSF) Need to make sure track modes are handled properly!
    PMC8_TRACK_RATE rate = static_cast<PMC8_TRACK_RATE>(mode);

    if (set_pmc8_track_mode(PortFD, rate))
        return true;

    return false;
}

bool PMC8::SetTrackRate(double raRate, double deRate)
{
    static bool deRateWarning = true;
    double pmc8RARate;

    DEBUGF(INDI::Logger::DBG_DEBUG, "PMC8::SetTrackRate called raRate=%f  deRate=%f", raRate, deRate);

    // Convert to arcsecs/s to +/- 0.0100 accepted by
    //double pmc8RARate = raRate - TRACKRATE_SIDEREAL;

    // (MSF) for now just send rate
    pmc8RARate = raRate;

    if (deRate != 0 && deRateWarning)
    {
        // Only send warning once per session
        deRateWarning = false;
        DEBUG(INDI::Logger::DBG_WARNING, "Custom Declination tracking rate is not implemented yet.");
    }

    if (set_pmc8_custom_ra_track_rate(PortFD, pmc8RARate))
        return true;

    DEBUG(INDI::Logger::DBG_ERROR, "PMC8::SetTrackRate not implemented!");
    return false;
}

bool PMC8::SetTrackEnabled(bool enabled)
{

    DEBUGF(INDI::Logger::DBG_DEBUG, "PMC8::SetTrackEnabled called enabled=%d", enabled);

    // FIXME - (MSF) Need to implement!
#if 0
    return set_pmc8_track_enabled(PortFD, enabled);
#else
    DEBUG(INDI::Logger::DBG_DEBUG, "PMC8::SetTrackEnabled just returns true for now");
    return true;
#endif
}

