/*
    INDI IEQ Pro driver

    Copyright (C) 2015 Jasem Mutlaq

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

#include "ieqprolegacy.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>
#include <memory>

#include <cmath>
#include <cstring>

/* Simulation Parameters */
#define SLEWRATE 1          /* slew rate, degrees/s */

#define MOUNTINFO_TAB "Mount Info"

// We declare an auto pointer to IEQPro.
static std::unique_ptr<IEQProLegacy> scope(new IEQProLegacy());

/* Constructor */
IEQProLegacy::IEQProLegacy(): GI(this)
{
    setVersion(1, 7);

    scopeInfo.gpsStatus    = GPS_OFF;
    scopeInfo.systemStatus = ST_STOPPED;
    scopeInfo.trackRate    = TR_SIDEREAL;
    scopeInfo.slewRate     = SR_1;
    scopeInfo.timeSource   = TS_RS232;
    scopeInfo.hemisphere   = HEMI_NORTH;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TRACK_RATE,
                           9);
}

const char *IEQProLegacy::getDefaultName()
{
    return "iEQ";
}

bool IEQProLegacy::initProperties()
{
    INDI::Telescope::initProperties();

    /* Firmware */
    IUFillText(&FirmwareT[FW_MODEL], "Model", "", nullptr);
    IUFillText(&FirmwareT[FW_BOARD], "Board", "", nullptr);
    IUFillText(&FirmwareT[FW_CONTROLLER], "Controller", "", nullptr);
    IUFillText(&FirmwareT[FW_RA], "RA", "", nullptr);
    IUFillText(&FirmwareT[FW_DEC], "DEC", "", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 5, getDeviceName(), "Firmware Info", "", MOUNTINFO_TAB, IP_RO, 0,
                     IPS_IDLE);

    /* Tracking Mode */
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_KING", "King");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Slew Rates
    SlewRateSP[0].setLabel("1x");
    SlewRateSP[1].setLabel("2x");
    SlewRateSP[2].setLabel("8x");
    SlewRateSP[3].setLabel("16x");
    SlewRateSP[4].setLabel("64x");
    SlewRateSP[5].setLabel("128x");
    SlewRateSP[6].setLabel("256x");
    SlewRateSP[7].setLabel("512x");
    SlewRateSP[8].setLabel("MAX");
    SlewRateSP.reset();
    // 64x is the default
    SlewRateSP[4].setState(ISS_ON);

    // Set TrackRate limits within +/- 0.0100 of Sidereal rate
    TrackRateNP[AXIS_RA].setMin(TRACKRATE_SIDEREAL - 0.01);
    TrackRateNP[AXIS_RA].setMax(TRACKRATE_SIDEREAL + 0.01);
    TrackRateNP[AXIS_DE].setMin(-0.01);
    TrackRateNP[AXIS_DE].setMax(0.01);

    /* GPS Status */
    IUFillSwitch(&GPSStatusS[GPS_OFF], "Off", "", ISS_ON);
    IUFillSwitch(&GPSStatusS[GPS_ON], "On", "", ISS_OFF);
    IUFillSwitch(&GPSStatusS[GPS_DATA_OK], "Data OK", "", ISS_OFF);
    IUFillSwitchVector(&GPSStatusSP, GPSStatusS, 3, getDeviceName(), "GPS_STATUS", "GPS", MOUNTINFO_TAB, IP_RO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    /* Time Source */
    IUFillSwitch(&TimeSourceS[TS_RS232], "RS232", "", ISS_ON);
    IUFillSwitch(&TimeSourceS[TS_CONTROLLER], "Controller", "", ISS_OFF);
    IUFillSwitch(&TimeSourceS[TS_GPS], "GPS", "", ISS_OFF);
    IUFillSwitchVector(&TimeSourceSP, TimeSourceS, 3, getDeviceName(), "TIME_SOURCE", "Time Source", MOUNTINFO_TAB,
                       IP_RO, ISR_1OFMANY, 0, IPS_IDLE);

    /* Hemisphere */
    IUFillSwitch(&HemisphereS[HEMI_SOUTH], "South", "", ISS_OFF);
    IUFillSwitch(&HemisphereS[HEMI_NORTH], "North", "", ISS_ON);
    IUFillSwitchVector(&HemisphereSP, HemisphereS, 2, getDeviceName(), "HEMISPHERE", "Hemisphere", MOUNTINFO_TAB, IP_RO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    /* Home */
    IUFillSwitch(&HomeS[IEQ_FIND_HOME], "FindHome", "Find Home", ISS_OFF);
    IUFillSwitch(&HomeS[IEQ_SET_HOME], "SetCurrentAsHome", "Set current as Home", ISS_OFF);
    IUFillSwitch(&HomeS[IEQ_GOTO_HOME], "GoToHome", "Go to Home", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 3, getDeviceName(), "HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0,
                       IPS_IDLE);

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[RA_AXIS], "RA_GUIDE_RATE", "x Sidereal", "%.2f", 0.01, 0.9, 0.1, 0.5);
    IUFillNumber(&GuideRateN[DEC_AXIS], "DE_GUIDE_RATE", "x Sidereal", "%.2f", 0.1, 0.99, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    TrackState = SCOPE_IDLE;

    GI::initProperties(MOTION_TAB);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    SetParkDataType(PARK_AZ_ALT);

    addAuxControls();

    set_ieqpro_device(getDeviceName());

    // Only CEM40 and GEM45 have 115200 baud, rest are 9600
    if (strstr(getDeviceName(), "CEM40") || strstr(getDeviceName(), "GEM45"))
        serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    currentRA  = get_local_sidereal_time(LocationNP[LOCATION_LONGITUDE].getValue());
    currentDEC = LocationNP[LOCATION_LATITUDE].getValue() > 0 ? 90 : -90;
    return true;
}

bool IEQProLegacy::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineProperty(&HomeSP);

        defineProperty(&GuideRateNP);

        defineProperty(&FirmwareTP);
        defineProperty(&GPSStatusSP);
        defineProperty(&TimeSourceSP);
        defineProperty(&HemisphereSP);

        getStartupData();
    }
    else
    {
        deleteProperty(HomeSP.name);

        deleteProperty(GuideRateNP.name);

        deleteProperty(FirmwareTP.name);
        deleteProperty(GPSStatusSP.name);
        deleteProperty(TimeSourceSP.name);
        deleteProperty(HemisphereSP.name);
    }

    GI::updateProperties();

    return true;
}

void IEQProLegacy::getStartupData()
{
    LOG_DEBUG("Getting firmware data...");
    if (get_ieqpro_firmware(PortFD, &firmwareInfo))
    {
        IUSaveText(&FirmwareT[0], firmwareInfo.Model.c_str());
        IUSaveText(&FirmwareT[1], firmwareInfo.MainBoardFirmware.c_str());
        IUSaveText(&FirmwareT[2], firmwareInfo.ControllerFirmware.c_str());
        IUSaveText(&FirmwareT[3], firmwareInfo.RAFirmware.c_str());
        IUSaveText(&FirmwareT[4], firmwareInfo.DEFirmware.c_str());

        FirmwareTP.s = IPS_OK;
        IDSetText(&FirmwareTP, nullptr);
    }

    LOG_DEBUG("Getting guiding rate...");
    double raGuideRate = 0, deGuideRate = 0;
    if (get_ieqpro_guide_rate(PortFD, &raGuideRate, &deGuideRate))
    {
        GuideRateN[RA_AXIS].value = raGuideRate;
        GuideRateN[DEC_AXIS].value = deGuideRate;
        IDSetNumber(&GuideRateNP, nullptr);
    }

    double utc_offset;
    int yy, dd, mm, hh, minute, ss;
    if (get_ieqpro_utc_date_time(PortFD, &utc_offset, &yy, &mm, &dd, &hh, &minute, &ss))
    {
        char isoDateTime[32] = {0};
        char utcOffset[8] = {0};

        snprintf(isoDateTime, 32, "%04d-%02d-%02dT%02d:%02d:%02d", yy, mm, dd, hh, minute, ss);
        snprintf(utcOffset, 8, "%4.2f", utc_offset);

        TimeTP[UTC].setText(isoDateTime);
        TimeTP[OFFSET].setText(utcOffset);

        LOGF_INFO("Mount UTC offset is %s. UTC time is %s", utcOffset, isoDateTime);

        TimeTP.setState(IPS_OK);
        TimeTP.apply();
    }

    // Get Longitude and Latitude from mount
    double longitude = 0, latitude = 0;
    if (get_ieqpro_latitude(PortFD, &latitude) && get_ieqpro_longitude(PortFD, &longitude))
    {
        // Convert to INDI standard longitude (0 to 360 Eastward)
        if (longitude < 0)
            longitude += 360;

        LOGF_INFO("Mount Longitude %g Latitude %g", longitude, latitude);

        LocationNP[LOCATION_LATITUDE].setValue(latitude);
        LocationNP[LOCATION_LONGITUDE].setValue(longitude);
        LocationNP.setState(IPS_OK);

        LocationNP.apply(nullptr);

        saveConfig(true, "GEOGRAPHIC_COORD");
    }
    else if (IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude) == 0 &&
             IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude) == 0)
    {
        LocationNP[LOCATION_LATITUDE].setValue(latitude);
        LocationNP[LOCATION_LONGITUDE].setValue(longitude);
        LocationNP.setState(IPS_OK);

        LocationNP.apply();
    }

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

    if (isSimulation())
    {
        if (isParked())
            set_sim_system_status(ST_PARKED);
        else
            set_sim_system_status(ST_STOPPED);
    }
}

bool IEQProLegacy::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Check guider interface
    if (GI::processNumber(dev, name, values, names, n))
        return true;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Guiding Rate
        if (!strcmp(name, GuideRateNP.name))
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);

            if (set_ieqpro_guide_rate(PortFD, GuideRateN[RA_AXIS].value, GuideRateN[DEC_AXIS].value))
                GuideRateNP.s = IPS_OK;
            else
                GuideRateNP.s = IPS_ALERT;

            IDSetNumber(&GuideRateNP, nullptr);

            return true;
        }
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool IEQProLegacy::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(getDeviceName(), dev))
    {
        if (!strcmp(name, HomeSP.name))
        {
            IUUpdateSwitch(&HomeSP, states, names, n);

            IEQ_HOME_OPERATION operation = static_cast<IEQ_HOME_OPERATION>(IUFindOnSwitchIndex(&HomeSP));

            IUResetSwitch(&HomeSP);

            switch (operation)
            {
                case IEQ_FIND_HOME:
                    if (firmwareInfo.Model.find("CEM") == std::string::npos)
                    {
                        HomeSP.s = IPS_IDLE;
                        IDSetSwitch(&HomeSP, nullptr);
                        LOG_WARN("Home search is not supported in this model.");
                        return true;
                    }

                    if (find_ieqpro_home(PortFD) == false)
                    {
                        HomeSP.s = IPS_ALERT;
                        IDSetSwitch(&HomeSP, nullptr);
                        return false;
                    }

                    HomeSP.s = IPS_OK;
                    IDSetSwitch(&HomeSP, nullptr);
                    LOG_INFO("Searching for home position...");
                    return true;

                case IEQ_SET_HOME:
                    if (set_ieqpro_current_home(PortFD) == false)
                    {
                        HomeSP.s = IPS_ALERT;
                        IDSetSwitch(&HomeSP, nullptr);
                        return false;
                    }

                    HomeSP.s = IPS_OK;
                    IDSetSwitch(&HomeSP, nullptr);
                    LOG_INFO("Home position set to current coordinates.");
                    return true;

                case IEQ_GOTO_HOME:
                    if (goto_ieqpro_home(PortFD) == false)
                    {
                        HomeSP.s = IPS_ALERT;
                        IDSetSwitch(&HomeSP, nullptr);
                        return false;
                    }

                    HomeSP.s = IPS_OK;
                    IDSetSwitch(&HomeSP, nullptr);
                    LOG_INFO("Slewing to home position...");
                    return true;
            }

            return true;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool IEQProLegacy::ReadScopeStatus()
{
    bool rc = false;

    IEQInfo newInfo;

    if (isSimulation())
        mountSim();

    rc = get_ieqpro_status(PortFD, &newInfo);

    if (rc)
    {
        IUResetSwitch(&GPSStatusSP);
        GPSStatusS[newInfo.gpsStatus].s = ISS_ON;
        IDSetSwitch(&GPSStatusSP, nullptr);

        IUResetSwitch(&TimeSourceSP);
        TimeSourceS[newInfo.timeSource].s = ISS_ON;
        IDSetSwitch(&TimeSourceSP, nullptr);

        IUResetSwitch(&HemisphereSP);
        HemisphereS[newInfo.hemisphere].s = ISS_ON;
        IDSetSwitch(&HemisphereSP, nullptr);

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

        switch (newInfo.systemStatus)
        {
            case ST_STOPPED:
                TrackModeSP.setState(IPS_IDLE);
                TrackState    = SCOPE_IDLE;
                break;
            case ST_PARKED:
                TrackModeSP.setState(IPS_IDLE);
                TrackState    = SCOPE_PARKED;
                if (!isParked())
                    SetParked(true);
                break;
            case ST_HOME:
                TrackModeSP.setState(IPS_IDLE);
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
                // If slew to parking position is complete, issue park command now.
                if (TrackState == SCOPE_PARKING)
                    park_ieqpro(PortFD);
                else
                {
                    TrackModeSP.setState(IPS_BUSY);
                    TrackState    = SCOPE_TRACKING;
                    if (scopeInfo.systemStatus == ST_SLEWING)
                        LOG_INFO("Slew complete, tracking...");
                    else if (scopeInfo.systemStatus == ST_MERIDIAN_FLIPPING)
                        LOG_INFO("Meridian flip complete, tracking...");
                }
                break;
        }

            TrackModeSP.reset();
        TrackModeSP[newInfo.trackRate].setState(ISS_ON);
            TrackModeSP.apply();

        scopeInfo = newInfo;
    }

    rc = get_ieqpro_coords(PortFD, &currentRA, &currentDEC);

    if (rc)
        NewRaDec(currentRA, currentDEC);

    return rc;
}

bool IEQProLegacy::Goto(double r, double d)
{
    targetRA  = r;
    targetDEC = d;
    char RAStr[64] = {0}, DecStr[64] = {0};

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    if (set_ieqpro_ra(PortFD, r) == false || set_ieqpro_dec(PortFD, d) == false)
    {
        LOG_ERROR("Error setting RA/DEC.");
        return false;
    }

    if (slew_ieqpro(PortFD) == false)
    {
        LOG_ERROR("Failed to slew.");
        return false;
    }

    TrackState = SCOPE_SLEWING;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool IEQProLegacy::Sync(double ra, double dec)
{
    if (set_ieqpro_ra(PortFD, ra) == false || set_ieqpro_dec(PortFD, dec) == false)
    {
        LOG_ERROR("Error setting RA/DEC.");
        return false;
    }

    if (sync_ieqpro(PortFD) == false)
    {
        LOG_ERROR("Failed to sync.");
    }

    EqNP.setState(IPS_OK);

    currentRA  = ra;
    currentDEC = dec;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool IEQProLegacy::Abort()
{
    return abort_ieqpro(PortFD);
}

bool IEQProLegacy::Park()
{
#if 0
    targetRA  = GetAxis1Park();
    targetDEC = GetAxis2Park();

    if (set_ieqpro_ra(PortFD, targetRA) == false || set_ieqpro_dec(PortFD, targetDEC) == false)
    {
        LOG_ERROR("Error setting RA/DEC.");
        return false;
    }

    if (slew_ieqpro(PortFD) == false)
    {
        LOG_ERROR("Failed to slew tp parking position.");
        return false;
    }

    char RAStr[64] = {0}, DecStr[64] = {0};
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    TrackState = SCOPE_PARKING;
    LOGF_INFO("Telescope parking in progress to RA: %s DEC: %s", RAStr, DecStr);

    return true;
#endif

    double parkAz  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    INDI::IEquatorialCoordinates equatorialCoords {0, 0};
    INDI::IHorizontalCoordinates horizontalCoords {parkAz, parkAlt};
    INDI::HorizontalToEquatorial(&horizontalCoords, &m_Location, ln_get_julian_from_sys(), &equatorialCoords);

    if (Goto(equatorialCoords.rightascension, equatorialCoords.declination))
    {
        TrackState = SCOPE_PARKING;
        LOG_INFO("Parking is in progress...");

        return true;
    }
    else
        return false;
}

bool IEQProLegacy::UnPark()
{
    if (unpark_ieqpro(PortFD))
    {
        SetParked(false);
        TrackState = SCOPE_IDLE;
        return true;
    }
    else
        return false;
}

bool IEQProLegacy::Handshake()
{
    if (isSimulation())
    {
        set_sim_gps_status(GPS_DATA_OK);
        set_sim_system_status(ST_STOPPED);
        set_sim_track_rate(TR_SIDEREAL);
        set_sim_slew_rate(SR_3);
        set_sim_time_source(TS_GPS);
        set_sim_hemisphere(HEMI_NORTH);
    }

    if (check_ieqpro_connection(PortFD) == false)
        return false;

    return true;
}

bool IEQProLegacy::updateTime(ln_date *utc, double utc_offset)
{
    struct ln_zonedate ltm;

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600.0);

    // Set Local Time
    if (set_ieqpro_local_time(PortFD, ltm.hours, ltm.minutes, ltm.seconds) == false)
    {
        LOG_ERROR("Error setting local time.");
        return false;
    }

    // Send it as YY (i.e. 2015 --> 15)
    ltm.years -= 2000;

    // Set Local date
    if (set_ieqpro_local_date(PortFD, ltm.years, ltm.months, ltm.days) == false)
    {
        LOG_ERROR("Error setting local date.");
        return false;
    }

    // UTC Offset
    if (set_ieqpro_utc_offset(PortFD, utc_offset) == false)
    {
        LOG_ERROR("Error setting UTC Offset.");
        return false;
    }

    LOG_INFO("Time and date updated.");

    return true;
}

bool IEQProLegacy::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (longitude > 180)
        longitude -= 360;

    if (set_ieqpro_longitude(PortFD, longitude) == false)
    {
        LOG_ERROR("Failed to set longitude.");
        return false;
    }

    if (set_ieqpro_latitude(PortFD, latitude) == false)
    {
        LOG_ERROR("Failed to set latitude.");
        return false;
    }

    char l[32] = {0}, L[32] = {0};
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

void IEQProLegacy::debugTriggered(bool enable)
{
    set_ieqpro_debug(enable);
}

void IEQProLegacy::simulationTriggered(bool enable)
{
    set_ieqpro_simulation(enable);
}

bool IEQProLegacy::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    switch (command)
    {
        case MOTION_START:
            if (start_ieqpro_motion(PortFD, (dir == DIRECTION_NORTH ? IEQ_N : IEQ_S)) == false)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (dir == DIRECTION_NORTH) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (stop_ieqpro_motion(PortFD, (dir == DIRECTION_NORTH ? IEQ_N : IEQ_S)) == false)
            {
                LOG_ERROR("Error stopping N/S motion.");
                return false;
            }
            else
                LOGF_INFO("%s motion stopped.", (dir == DIRECTION_NORTH) ? "North" : "South");
            break;
    }

    return true;
}

bool IEQProLegacy::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    switch (command)
    {
        case MOTION_START:
            if (start_ieqpro_motion(PortFD, (dir == DIRECTION_WEST ? IEQ_W : IEQ_E)) == false)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (dir == DIRECTION_WEST) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (stop_ieqpro_motion(PortFD, (dir == DIRECTION_WEST ? IEQ_W : IEQ_E)) == false)
            {
                LOG_ERROR("Error stopping W/E motion.");
                return false;
            }
            else
                LOGF_INFO("%s motion stopped.", (dir == DIRECTION_WEST) ? "West" : "East");
            break;
    }

    return true;
}

IPState IEQProLegacy::GuideNorth(uint32_t ms)
{
    bool rc = start_ieqpro_guide(PortFD, IEQ_N, ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState IEQProLegacy::GuideSouth(uint32_t ms)
{
    bool rc = start_ieqpro_guide(PortFD, IEQ_S, ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState IEQProLegacy::GuideEast(uint32_t ms)
{
    bool rc = start_ieqpro_guide(PortFD, IEQ_E, ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState IEQProLegacy::GuideWest(uint32_t ms)
{
    bool rc = start_ieqpro_guide(PortFD, IEQ_W, ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

bool IEQProLegacy::SetSlewRate(int index)
{
    IEQ_SLEW_RATE rate = static_cast<IEQ_SLEW_RATE>(index);
    return set_ieqpro_slew_rate(PortFD, rate);
}

bool IEQProLegacy::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    return true;
}

void IEQProLegacy::mountSim()
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
                    set_sim_system_status(ST_TRACKING_PEC_OFF);
                else
                    set_sim_system_status(ST_PARKED);
            }

            break;

        default:
            break;
    }

    set_sim_ra(currentRA);
    set_sim_dec(currentDEC);
}

bool IEQProLegacy::SetCurrentPark()
{
    INDI::IEquatorialCoordinates equatorialCoords {currentRA, currentDEC};
    INDI::IHorizontalCoordinates horizontalCoords {0, 0};
    INDI::EquatorialToHorizontal(&equatorialCoords, &m_Location, ln_get_julian_from_sys(), &horizontalCoords);
    double parkAZ = horizontalCoords.azimuth;
    double parkAlt = horizontalCoords.altitude;

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr,
               AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool IEQProLegacy::SetDefaultPark()
{
    // By default azimuth 0
    SetAxis1Park(0);

    // Altitude = latitude of observer
    SetAxis2Park(LocationNP[LOCATION_LATITUDE].getValue());

    return true;
}

bool IEQProLegacy::SetTrackMode(uint8_t mode)
{
    IEQ_TRACK_RATE rate = static_cast<IEQ_TRACK_RATE>(mode);

    if (set_ieqpro_track_mode(PortFD, rate))
        return true;

    return false;
}

bool IEQProLegacy::SetTrackRate(double raRate, double deRate)
{
    static bool deRateWarning = true;

    // Convert to arcsecs/s to +/- 0.0100 accepted by
    double ieqRARate = raRate - TRACKRATE_SIDEREAL;
    if (deRate != 0 && deRateWarning)
    {
        // Only send warning once per session
        deRateWarning = false;
        LOG_WARN("Custom Declination tracking rate is not implemented yet.");
    }

    if (set_ieqpro_custom_ra_track_rate(PortFD, ieqRARate))
        return true;

    return false;
}

bool IEQProLegacy::SetTrackEnabled(bool enabled)
{
    if (enabled)
    {
        // If we are engaging tracking, let us first set tracking mode, and if we have custom mode, then tracking rate.
        // NOTE: Is this the correct order? or should tracking be switched on first before making these changes? Need to test.
        SetTrackMode(TrackModeSP.findOnSwitchIndex());
        if (TrackModeSP[TR_CUSTOM].getState() == ISS_ON)
            SetTrackRate(TrackRateNP[AXIS_RA].getValue(), TrackRateNP[AXIS_DE].getValue());
    }

    return set_ieqpro_track_enabled(PortFD, enabled);
}
