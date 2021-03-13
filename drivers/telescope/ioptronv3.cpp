/*
    INDI IOptron v3 Driver for firmware version 20171001 or later.

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

    Updated for PEC V3
*/

#include "ioptronv3.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"
#include "indicom.h"

#include <libnova/transform.h>
#include <libnova/sidereal_time.h>

#include <memory>

#include <cmath>
#include <cstring>

using namespace IOPv3;

#define MOUNTINFO_TAB "Mount Info"
// #define PEC_TAB "PEC"  //Not Needed

// We declare an auto pointer to IOptronV3.
static std::unique_ptr<IOptronV3> scope(new IOptronV3());

void ISGetProperties(const char *dev)
{
    scope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    scope->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    scope->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    scope->ISNewNumber(dev, name, values, names, n);
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
IOptronV3::IOptronV3()
{
    setVersion(1, 3);

    driver.reset(new Driver(getDeviceName()));

    scopeInfo.gpsStatus    = GPS_OFF;
    scopeInfo.systemStatus = ST_STOPPED;
    scopeInfo.trackRate    = TR_SIDEREAL;
    /* v3.0 use default PEC Settings */
    scopeInfo.systemStatus = ST_TRACKING_PEC_OFF;
    // End Mod */
    scopeInfo.slewRate     = SR_1;
    scopeInfo.timeSource   = TS_RS232;
    scopeInfo.hemisphere   = HEMI_NORTH;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           /* v3.0 use default PEC Settings */
                           TELESCOPE_HAS_PEC  |
                           // End Mod */
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_TRACK_RATE | TELESCOPE_HAS_PIER_SIDE,
                           9);
}

const char *IOptronV3::getDefaultName()
{
    return "iOptronV3";
}

bool IOptronV3::initProperties()
{
    INDI::Telescope::initProperties();

    // Slew Rates
    strncpy(SlewRateS[0].label, "1x", MAXINDILABEL);
    strncpy(SlewRateS[1].label, "2x", MAXINDILABEL);
    strncpy(SlewRateS[2].label, "8x", MAXINDILABEL);
    strncpy(SlewRateS[3].label, "16x", MAXINDILABEL);
    strncpy(SlewRateS[4].label, "64x", MAXINDILABEL);
    strncpy(SlewRateS[5].label, "128x", MAXINDILABEL);
    strncpy(SlewRateS[6].label, "256x", MAXINDILABEL);
    strncpy(SlewRateS[7].label, "512x", MAXINDILABEL);
    strncpy(SlewRateS[8].label, "MAX", MAXINDILABEL);
    IUResetSwitch(&SlewRateSP);
    // 64x is the default
    SlewRateS[4].s = ISS_ON;

    /* Firmware */
    FirmwareTP[FW_MODEL].fill("Model", "", nullptr);
    FirmwareTP[FW_BOARD].fill("Board", "", nullptr);
    FirmwareTP[FW_CONTROLLER].fill("Controller", "", nullptr);
    FirmwareTP[FW_RA].fill("RA", "", nullptr);
    FirmwareTP[FW_DEC].fill("DEC", "", nullptr);
    FirmwareTP.fill(getDeviceName(), "Firmware Info", "", MOUNTINFO_TAB, IP_RO, 0,
                     IPS_IDLE);

    /* Tracking Mode */
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_KING", "King");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    /* GPS Status */
    GPSStatusSP[GPS_OFF].fill("Off", "", ISS_ON);
    GPSStatusSP[GPS_ON].fill("On", "", ISS_OFF);
    GPSStatusSP[GPS_DATA_OK].fill("Data OK", "", ISS_OFF);
    GPSStatusSP.fill(getDeviceName(), "GPS_STATUS", "GPS", MOUNTINFO_TAB, IP_RO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    /* Time Source */
    TimeSourceSP[TS_RS232].fill("RS232", "", ISS_ON);
    TimeSourceSP[TS_CONTROLLER].fill("Controller", "", ISS_OFF);
    TimeSourceSP[TS_GPS].fill("GPS", "", ISS_OFF);
    TimeSourceSP.fill(getDeviceName(), "TIME_SOURCE", "Time Source", MOUNTINFO_TAB,
                       IP_RO, ISR_1OFMANY, 0, IPS_IDLE);

    /* Hemisphere */
    HemisphereSP[HEMI_SOUTH].fill("South", "", ISS_OFF);
    HemisphereSP[HEMI_NORTH].fill("North", "", ISS_ON);
    HemisphereSP.fill(getDeviceName(), "HEMISPHERE", "Hemisphere", MOUNTINFO_TAB, IP_RO,
                       ISR_1OFMANY, 0, IPS_IDLE);

    /* Home */
    HomeSP[IOP_FIND_HOME].fill("FindHome", "Find Home", ISS_OFF);
    HomeSP[IOP_SET_HOME].fill("SetCurrentAsHome", "Set current as Home", ISS_OFF);
    HomeSP[IOP_GOTO_HOME].fill("GoToHome", "Go to Home", ISS_OFF);
    HomeSP.fill(getDeviceName(), "HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0,
                       IPS_IDLE);

    /* v3.0 Create PEC Training switches */
    // PEC Training
    PECTrainingSP[0].fill("PEC_Recording", "Record", ISS_OFF);
    PECTrainingSP[1].fill("PEC_Status", "Status", ISS_OFF);
    PECTrainingSP.fill(getDeviceName(), "PEC_TRAINING", "Training", MOTION_TAB, IP_RW,
                       ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Create PEC Training Information */
    PECInfoTP[0].fill("PEC_INFO", "Status", "");
    PECInfoTP.fill(getDeviceName(), "PEC_INFOS", "Data", MOTION_TAB,
                     IP_RO, 60, IPS_IDLE);
    // End Mod */

    /* How fast do we guide compared to sidereal rate */
    GuideRateNP[0].fill("RA_GUIDE_RATE", "x Sidereal", "%g", 0.01, 0.9, 0.1, 0.5);
    GuideRateNP[1].fill("DE_GUIDE_RATE", "x Sidereal", "%g", 0.1, 0.99, 0.1, 0.5);
    GuideRateNP.fill(getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);


    /* Slew Mode. Normal vs Counter Weight up */
    SlewModeSP[IOP_CW_NORMAL].fill("Normal", "Normal", ISS_ON);
    SlewModeSP[IOP_CW_UP].fill("Counterweight UP", "Counterweight up", ISS_OFF);
    SlewModeSP.fill(getDeviceName(), "Slew Type", "Slew Type", MOTION_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    /* Daylight Savings */
    DaylightSP[0].fill("ON", "ON", ISS_OFF);
    DaylightSP[1].fill("OFF", "OFF", ISS_ON);
    DaylightSP.fill(getDeviceName(), "DaylightSaving", "Daylight Savings", SITE_TAB, IP_RW,
                       ISR_1OFMANY, 0,
                       IPS_IDLE);

    /* Counter Weight State */
    CWStateSP[IOP_CW_NORMAL].fill("Normal", "Normal", ISS_ON);
    CWStateSP[IOP_CW_UP].fill("Up", "Up", ISS_OFF);
    CWStateSP.fill(getDeviceName(), "CWState", "Counter weights", MOTION_TAB, IP_RO, ISR_1OFMANY,
                       0,
                       IPS_IDLE);

    // Baud rates.
    // 230400 for 120
    // 115000 for everything else
    if (strstr(getDeviceName(), "120"))
        serialConnection->setDefaultBaudRate(Connection::Serial::B_230400);
    else
        serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    // Default WiFi connection parametes
    tcpConnection->setDefaultHost("10.10.100.254");
    tcpConnection->setDefaultPort(8899);

    TrackState = SCOPE_IDLE;

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    SetParkDataType(PARK_AZ_ALT);

    addAuxControls();

    double longitude = 0, latitude = 90;
    // Get value from config file if it exists.
    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
    currentRA  = get_local_sidereal_time(longitude);
    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
    currentDEC = latitude > 0 ? 90 : -90;
    driver->setSimLongLat(longitude > 180 ? longitude - 360 : longitude, latitude);

    return true;
}

bool IOptronV3::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineProperty(HomeSP);

        /* v3.0 Create PEC switches */
        defineProperty(PECTrainingSP);
        defineProperty(PECInfoTP);
        // End Mod */

        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(GuideRateNP);

        defineProperty(FirmwareTP);
        defineProperty(GPSStatusSP);
        defineProperty(TimeSourceSP);
        defineProperty(HemisphereSP);
        defineProperty(SlewModeSP);
        defineProperty(DaylightSP);
        defineProperty(CWStateSP);

        getStartupData();
    }
    else
    {
        deleteProperty(HomeSP.getName());

        /* v3.0 Delete PEC switches */
        deleteProperty(PECTrainingSP.getName());
        deleteProperty(PECInfoTP.getName());
        // End Mod*/

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.getName());

        deleteProperty(FirmwareTP.getName());
        deleteProperty(GPSStatusSP.getName());
        deleteProperty(TimeSourceSP.getName());
        deleteProperty(HemisphereSP.getName());
        deleteProperty(SlewModeSP.getName());
        deleteProperty(DaylightSP.getName());
        deleteProperty(CWStateSP.getName());
    }

    return true;
}

void IOptronV3::getStartupData()
{
    LOG_DEBUG("Getting firmware data...");
    if (driver->getFirmwareInfo(&firmwareInfo))
    {
        FirmwareTP[0].setText(firmwareInfo.Model);
        FirmwareTP[1].setText(firmwareInfo.MainBoardFirmware);
        FirmwareTP[2].setText(firmwareInfo.ControllerFirmware);
        FirmwareTP[3].setText(firmwareInfo.RAFirmware);
        FirmwareTP[4].setText(firmwareInfo.DEFirmware);

        FirmwareTP.setState(IPS_OK);
        FirmwareTP.apply();
    }

    LOG_DEBUG("Getting guiding rate...");
    double RARate = 0, DERate = 0;
    if (driver->getGuideRate(&RARate, &DERate))
    {
        GuideRateNP[RA_AXIS].setValue(RARate);
        GuideRateNP[DEC_AXIS].setValue(DERate);
        GuideRateNP.apply();
    }

    int utcOffsetMinutes = 0;
    bool dayLightSavings = false;
    double JD = 0;
    if (driver->getUTCDateTime(&JD, &utcOffsetMinutes, &dayLightSavings))
    {
        time_t utc_time;
        ln_get_timet_from_julian(JD, &utc_time);

        // UTC Time
        char ts[32] = {0};
        struct tm *utc;
        utc = gmtime(&utc_time);
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", utc);
        TimeTP[0].setText(ts);
        LOGF_INFO("Mount UTC: %s", ts);

        // UTC Offset
        char offset[8] = {0};
        snprintf(offset, 8, "%.2f", utcOffsetMinutes / 60.0);
        TimeTP[1].setText(offset);
        LOGF_INFO("Mount UTC Offset: %s", offset);

        TimeTP.setState(IPS_OK);
        TimeTP.apply();

        LOGF_INFO("Mount Daylight Savings: %s", dayLightSavings ? "ON" : "OFF");
        DaylightSP[0].setState(dayLightSavings ? ISS_ON : ISS_OFF);
        DaylightSP[1].setState(!dayLightSavings ? ISS_ON : ISS_OFF);
        DaylightSP.setState(IPS_OK);
        DaylightSP.apply();
    }

    // Get Longitude and Latitude from mount
    double longitude = 0, latitude = 0;
    if (driver->getStatus(&scopeInfo))
    {
        LocationNP[LOCATION_LATITUDE].setValue(scopeInfo.latitude);
        // Convert to INDI standard longitude (0 to 360 Eastward)
        LocationNP[LOCATION_LONGITUDE].setValue((scopeInfo.longitude < 0) ? scopeInfo.longitude + 360 : scopeInfo.longitude);
        LocationNP.setState(IPS_OK);

        LocationNP.apply();

        char l[32] = {0}, L[32] = {0};
        fs_sexa(l, LocationNP[LOCATION_LATITUDE].getValue(), 3, 3600);
        fs_sexa(L, LocationNP[LOCATION_LONGITUDE].getValue(), 4, 3600);

        LOGF_INFO("Mount Location: Lat %.32s - Long %.32s", l, L);

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

    double parkAZ = LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180;
    double parkAL = LocationNP[LOCATION_LATITUDE].getValue();
    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(parkAZ);
        SetAxis2ParkDefault(parkAL);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(parkAZ);
        SetAxis2Park(parkAL);
        SetAxis1ParkDefault(parkAZ);
        SetAxis2ParkDefault(parkAL);

        driver->setParkAz(parkAZ);
        driver->setParkAlt(parkAL);
    }

    /* v3.0 Read PEC State at startup */
    IOPInfo newInfo;
    if (driver->getStatus(&newInfo))
    {
        switch (newInfo.systemStatus)
        {
            case ST_STOPPED:
            case ST_PARKED:
            case ST_HOME:
            case ST_SLEWING:
            case ST_MERIDIAN_FLIPPING:
            case ST_GUIDING:

            case ST_TRACKING_PEC_OFF:
                setPECState(PEC_OFF);
                GetPECDataStatus(true);
                break;

            case ST_TRACKING_PEC_ON:
                setPECState(PEC_ON);
                GetPECDataStatus(true);
                break;
        }
        scopeInfo = newInfo;
    }
    // End Mod */

    if (isSimulation())
    {
        if (isParked())
            driver->setSimSytemStatus(ST_PARKED);
        else
            driver->setSimSytemStatus(ST_STOPPED);
    }
}

bool IOptronV3::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Guiding Rate
        if (GuideRateNP.isNameMatch(name))
        {
            GuideRateNP.update(values, names, n);

            if (driver->setGuideRate(GuideRateNP[RA_AXIS].value, GuideRateNP[DEC_AXIS].getValue()))
                GuideRateNP.setState(IPS_OK);
            else
                GuideRateNP.setState(IPS_ALERT);

            GuideRateNP.apply();

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

bool IOptronV3::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!strcmp(getDeviceName(), dev))
    {
        /*******************************************************
         * Home Operations
        *******************************************************/
        if (HomeSP.isNameMatch(name))
        {
            HomeSP.update(states, names, n);

            IOP_HOME_OPERATION operation = (IOP_HOME_OPERATION)HomeSP.findOnSwitchIndex();

            HomeSP.reset();

            switch (operation)
            {
                case IOP_FIND_HOME:
                    if (firmwareInfo.Model.find("CEM") == std::string::npos &&
                            firmwareInfo.Model.find("GEM45") == std::string::npos)
                    {
                        HomeSP.setState(IPS_IDLE);
                        HomeSP.apply();
                        LOG_WARN("Home search is not supported in this model.");
                        return true;
                    }

                    if (driver->findHome() == false)
                    {
                        HomeSP.setState(IPS_ALERT);
                        HomeSP.apply();
                        return false;
                    }

                    HomeSP.setState(IPS_OK);
                    HomeSP.apply();
                    LOG_INFO("Searching for home position...");
                    return true;

                case IOP_SET_HOME:
                    if (driver->setCurrentHome() == false)
                    {
                        HomeSP.setState(IPS_ALERT);
                        HomeSP.apply();
                        return false;
                    }

                    HomeSP.setState(IPS_OK);
                    HomeSP.apply();
                    LOG_INFO("Home position set to current coordinates.");
                    return true;

                case IOP_GOTO_HOME:
                    if (driver->gotoHome() == false)
                    {
                        HomeSP.setState(IPS_ALERT);
                        HomeSP.apply();
                        return false;
                    }

                    HomeSP.setState(IPS_OK);
                    HomeSP.apply();
                    LOG_INFO("Slewing to home position...");
                    return true;
            }

            return true;
        }

        /*******************************************************
         * Slew Mode Operations
        *******************************************************/
        if (SlewModeSP.isNameMatch(name))
        {
            SlewModeSP.update(states, names, n);
            SlewModeSP.setState(IPS_OK);
            SlewModeSP.apply();
            return true;
        }

        /*******************************************************
         * Daylight Savings Operations
        *******************************************************/
        if (DaylightSP.isNameMatch(name))
        {
            DaylightSP.update(states, names, n);

            if (driver->setDaylightSaving(DaylightSP[0].getState() == ISS_ON))
                DaylightSP.setState(IPS_OK);
            else
                DaylightSP.setState(IPS_ALERT);

            DaylightSP.apply();
            return true;
        }
    }

    /* v3.0 PEC add controls and calls to the driver */
    if (PECStateSP.isNameMatch(name))
    {
        PECStateSP.update(states, names, n);

        if(PECStateSP.findOnSwitchIndex() == 0)
        {
            // PEC OFF
            if(isTraining)
            {
                // Training check
                sprintf(PECText, "Mount PEC busy recording, %d s", PECTime);
                LOG_WARN(PECText);
            }
            else
            {
                driver->setPECEnabled(false);
                PECStateSP.setState(IPS_OK);
                LOG_INFO("Disabling PEC Chip");
            }
        }

        if(PECStateSP.findOnSwitchIndex() == 1)
        {
            // PEC ON
            if (GetPECDataStatus(true))
            {
                // Data Check
                driver->setPECEnabled(true);
                PECStateSP.setState(IPS_BUSY);
                LOG_INFO("Enabling PEC Chip");
            }
        }
        PECStateSP.apply();
        return true;
    }
    // End Mod */

    /* v3.0 PEC add Training Controls to the Driver */
    if (PECTrainingSP.isNameMatch(name))
    {
        PECTrainingSP.update(states, names, n);
        if(isTraining)
        {
            // Check if already training
            if(PECTrainingSP.findOnSwitchIndex() == 1)
            {
                // Train Check Status
                sprintf(PECText, "Mount PEC busy recording, %d s", PECTime);
                LOG_WARN(PECText);
            }

            if(PECTrainingSP.findOnSwitchIndex() == 0)
            {
                // Train Cancel
                driver->setPETEnabled(false);
                isTraining = false;
                PECTrainingSP.setState(IPS_ALERT);
                LOG_WARN("PEC Training cancelled by user, chip disabled");
            }
        }
        else
        {
            if(PECTrainingSP.findOnSwitchIndex() == 0)
            {
                if(TrackState == SCOPE_TRACKING)
                {
                    // Train if tracking /guiding
                    driver->setPETEnabled(true);
                    isTraining = true;
                    PECTime = 0;
                    PECTrainingSP.setState(IPS_BUSY);
                    LOG_INFO("PEC recording started...");
                }
                else
                {
                    LOG_WARN("PEC Training only possible while guiding");
                    PECTrainingSP.setState(IPS_IDLE);
                }
            }
            if(PECTrainingSP.findOnSwitchIndex() == 1)
            {
                // Train Status
                GetPECDataStatus(true);
            }
        }
        PECTrainingSP.apply();
        return true;
    }
    // End Mod */

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool IOptronV3::ReadScopeStatus()
{
    bool rc = false;

    IOPInfo newInfo;

    if (isSimulation())
        mountSim();

    rc = driver->getStatus(&newInfo);

    if (rc)
    {
        if (GPSStatusSP.findOnSwitchIndex() != newInfo.gpsStatus)
        {
            GPSStatusSP.reset();
            GPSStatusSP[newInfo.gpsStatus].setState(ISS_ON);
            GPSStatusSP.apply();
        }

        if (TimeSourceSP.findOnSwitchIndex() != newInfo.timeSource)
        {
            TimeSourceSP.reset();
            TimeSourceSP[newInfo.timeSource].setState(ISS_ON);
            TimeSourceSP.apply();
        }

        if (HemisphereSP.findOnSwitchIndex() != newInfo.hemisphere)
        {
            HemisphereSP.reset();
            HemisphereSP[newInfo.hemisphere].setState(ISS_ON);
            HemisphereSP.apply();
        }

        if (IUFindOnSwitchIndex(&SlewRateSP) != newInfo.slewRate - 1)
        {
            IUResetSwitch(&SlewRateSP);
            SlewRateS[newInfo.slewRate - 1].s = ISS_ON;
            IDSetSwitch(&SlewRateSP, nullptr);
        }

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
                TrackModeSP.s = IPS_IDLE;
                TrackState    = SCOPE_IDLE;
                break;
            case ST_PARKED:
                TrackModeSP.s = IPS_IDLE;
                TrackState    = SCOPE_PARKED;
                if (!isParked())
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
            /* v3.0 PEC update status */
            case ST_TRACKING_PEC_OFF:
                setPECState(PEC_OFF);
                break;
            case ST_TRACKING_PEC_ON:
                setPECState(PEC_ON);
                break;
            // End Mod */
            case ST_GUIDING:
                TrackModeSP.s = IPS_BUSY;
                TrackState    = SCOPE_TRACKING;
                if (scopeInfo.systemStatus == ST_SLEWING)
                    LOG_INFO("Slew complete, tracking...");
                else if (scopeInfo.systemStatus == ST_MERIDIAN_FLIPPING)
                    LOG_INFO("Meridian flip complete, tracking...");
                break;
        }

        if (IUFindOnSwitchIndex(&TrackModeSP) != newInfo.trackRate)
        {
            IUResetSwitch(&TrackModeSP);
            TrackModeS[newInfo.trackRate].s = ISS_ON;
            IDSetSwitch(&TrackModeSP, nullptr);
        }

        scopeInfo = newInfo;
    }

    /* v3.0 Monitor PEC Training */
    if (isTraining)
    {
        if (TrackState == SCOPE_TRACKING)
        {
            if(GetPECDataStatus(false))
            {
                sprintf(PECText, "%d second worm cycle recorded", PECTime);
                LOG_INFO(PECText);
                PECTrainingSP.setState(IPS_OK);
                isTraining = false;
            }
            else
            {
                PECTime = PECTime + 1 * getCurrentPollingPeriod() / 1000;
                sprintf(PECText, "Recording: %d s", PECTime);
                PECInfoTP[0].setText(PECText);
            }
        }
        else
        {
            driver->setPETEnabled(false);
            PECTrainingSP.setState(IPS_ALERT);
            sprintf(PECText, "Tracking error, recording cancelled %d s", PECTime);
            LOG_ERROR(PECText);
            PECInfoTP[0].setText("Cancelled");
        }
        PECInfoTP.apply();
        PECTrainingSP.apply();
    }
    // End Mod */

    IOP_PIER_STATE pierState = IOP_PIER_UNKNOWN;
    IOP_CW_STATE cwState = IOP_CW_NORMAL;

    rc = driver->getCoords(&currentRA, &currentDEC, &pierState, &cwState);
    if (rc)
    {
        if (pierState == IOP_PIER_UNKNOWN)
            setPierSide(PIER_UNKNOWN);
        else
            setPierSide(pierState == IOP_PIER_EAST ? PIER_EAST : PIER_WEST);

        if (CWStateSP.findOnSwitchIndex() != cwState)
        {
            CWStateSP.reset();
            CWStateSP[cwState].setState(ISS_ON);
            CWStateSP.apply();
        }

        NewRaDec(currentRA, currentDEC);
    }

    return rc;
}

bool IOptronV3::Goto(double ra, double de)
{
    targetRA  = ra;
    targetDEC = de;
    char RAStr[64] = {0}, DecStr[64] = {0};

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    if (driver->setRA(ra) == false || driver->setDE(de) == false)
    {
        LOG_ERROR("Error setting RA/DEC.");
        return false;
    }

    bool rc = false;
    if (SlewModeSP.findOnSwitchIndex() == IOP_CW_NORMAL)
        rc = driver->slewNormal();
    else
        rc = driver->slewCWUp();

    if (rc == false)
    {
        LOG_ERROR("Failed to slew.");
        return false;
    }

    TrackState = SCOPE_SLEWING;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool IOptronV3::Sync(double ra, double de)
{
    if (driver->setRA(ra) == false || driver->setDE(de) == false)
    {
        LOG_ERROR("Error setting RA/DEC.");
        return false;
    }

    if (driver->sync() == false)
    {
        LOG_ERROR("Failed to sync.");
    }

    EqNP.setState(IPS_OK);

    currentRA  = ra;
    currentDEC = de;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool IOptronV3::Abort()
{
    return driver->abort();
}

bool IOptronV3::Park()
{
    //    if (firmwareInfo.Model.find("CEM") == std::string::npos &&
    //            firmwareInfo.Model.find("iEQ45 Pro") == std::string::npos &&
    //            firmwareInfo.Model.find("iEQ35") == std::string::npos)
    //    {
    //        LOG_ERROR("Parking is not supported in this mount model.");
    //        return false;
    //    }

    if (driver->park())
    {
        TrackState = SCOPE_PARKING;
        LOG_INFO("Parking is in progress...");

        return true;
    }
    else
        return false;
}

bool IOptronV3::UnPark()
{
    //    if (firmwareInfo.Model.find("CEM") == std::string::npos &&
    //            firmwareInfo.Model.find("iEQ45 Pro") == std::string::npos &&
    //            firmwareInfo.Model.find("iEQ35") == std::string::npos)
    //    {
    //        LOG_ERROR("Unparking is not supported in this mount model.");
    //        return false;
    //    }

    if (driver->unpark())
    {
        SetParked(false);
        TrackState = SCOPE_IDLE;
        return true;
    }
    else
        return false;
}

bool IOptronV3::Handshake()
{
    driver->setSimulation(isSimulation());

    if (driver->checkConnection(PortFD) == false)
        return false;

    return true;
}

bool IOptronV3::updateTime(ln_date *utc, double utc_offset)
{
    bool rc1 = driver->setUTCDateTime(ln_get_julian_day(utc));

    bool rc2 = driver->setUTCOffset(utc_offset * 60);

    return (rc1 && rc2);
}

bool IOptronV3::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (longitude > 180)
        longitude -= 360;

    if (driver->setLongitude(longitude) == false)
    {
        LOG_ERROR("Failed to set longitude.");
        return false;
    }

    if (driver->setLatitude(latitude) == false)
    {
        LOG_ERROR("Failed to set longitude.");
        return false;
    }

    char l[32] = {0}, L[32] = {0};
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

void IOptronV3::debugTriggered(bool enable)
{
    driver->setDebug(enable);
}

void IOptronV3::simulationTriggered(bool enable)
{
    driver->setSimulation(enable);
}

bool IOptronV3::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    switch (command)
    {
        case MOTION_START:
            if (driver->startMotion(dir == DIRECTION_NORTH ? IOP_N : IOP_S) == false)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (dir == DIRECTION_NORTH) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (driver->stopMotion(dir == DIRECTION_NORTH ? IOP_N : IOP_S) == false)
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

bool IOptronV3::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    switch (command)
    {
        case MOTION_START:
            if (driver->startMotion(dir == DIRECTION_WEST ? IOP_W : IOP_E) == false)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (dir == DIRECTION_WEST) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (driver->stopMotion(dir == DIRECTION_WEST ? IOP_W : IOP_E) == false)
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

IPState IOptronV3::GuideNorth(uint32_t ms)
{
    bool rc = driver->startGuide(IOP_N, (uint32_t)ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState IOptronV3::GuideSouth(uint32_t ms)
{
    bool rc = driver->startGuide(IOP_S, (uint32_t)ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState IOptronV3::GuideEast(uint32_t ms)
{
    bool rc = driver->startGuide(IOP_E, (uint32_t)ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

IPState IOptronV3::GuideWest(uint32_t ms)
{
    bool rc = driver->startGuide(IOP_W, (uint32_t)ms);
    return (rc ? IPS_OK : IPS_ALERT);
}

bool IOptronV3::SetSlewRate(int index)
{
    IOP_SLEW_RATE rate = (IOP_SLEW_RATE) (index + 1);
    return driver->setSlewRate(rate);
}

bool IOptronV3::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    SlewModeSP.save(fp);
    DaylightSP.save(fp);

    return true;
}

void IOptronV3::mountSim()
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
    double currentSlewRate = Driver::IOP_SLEW_RATES[IUFindOnSwitchIndex(&SlewRateSP)] * TRACKRATE_SIDEREAL / 3600.0;
    da  = currentSlewRate * dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
    switch (TrackState)
    {
        case SCOPE_IDLE:
            currentRA += (TrackRateNP[AXIS_RA].value / 3600.0 * dt) / 15.0;
            currentRA = range24(currentRA);
            break;

        case SCOPE_TRACKING:
            if (TrackModeS[TR_CUSTOM].s == ISS_ON)
            {
                currentRA  += ( ((TRACKRATE_SIDEREAL / 3600.0) - (TrackRateNP[AXIS_RA].value / 3600.0)) * dt) / 15.0;
                currentDEC += ( (TrackRateNP[AXIS_DE].value / 3600.0) * dt);
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
                    driver->setSimSytemStatus(ST_TRACKING_PEC_OFF);
                else
                    driver->setSimSytemStatus(ST_PARKED);
            }

            break;

        default:
            break;
    }

    driver->setSimRA(currentRA);
    driver->setSimDE(currentDEC);
}

bool IOptronV3::SetCurrentPark()
{
    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270

    ln_lnlat_posn observer;
    observer.lat = LocationNP[LOCATION_LATITUDE].getValue();
    observer.lng = LocationNP[LOCATION_LONGITUDE].getValue();
    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;
    equatorialPos.ra  = currentRA * 15;
    equatorialPos.dec = currentDEC;
    get_hrz_from_equ(&equatorialPos, &observer, ln_get_julian_from_sys(), &horizontalPos);

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, horizontalPos.az, 2, 3600);
    fs_sexa(AltStr, horizontalPos.alt, 2, 3600);

    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr,
               AltStr);

    SetAxis1Park(horizontalPos.az);
    SetAxis2Park(horizontalPos.alt);

    driver->setParkAz(horizontalPos.az);
    driver->setParkAlt(horizontalPos.alt);

    return true;
}

bool IOptronV3::SetDefaultPark()
{
    // By defualt azimuth 0
    SetAxis1Park(0);
    // Altitude = latitude of observer
    SetAxis2Park(LocationNP[LOCATION_LATITUDE].value);
    driver->setParkAz(0);
    driver->setParkAlt(LocationNP[LOCATION_LATITUDE].value);
    return true;
}

bool IOptronV3::SetTrackMode(uint8_t mode)
{
    IOP_TRACK_RATE rate = static_cast<IOP_TRACK_RATE>(mode);

    if (driver->setTrackMode(rate))
        return true;

    return false;
}

bool IOptronV3::SetTrackRate(double raRate, double deRate)
{
    INDI_UNUSED(deRate);

    // Convert to arcsecs/s to rate
    double ieqRARate = raRate / TRACKRATE_SIDEREAL;

    if (ieqRARate < 0.1 || ieqRARate > 1.9)
    {
        LOG_ERROR("Rate is outside permitted limits of 0.1 to 1.9 sidereal rate (1.504 to 28.578 arcsecs/s)");
        return false;
    }


    if (driver->setCustomRATrackRate(ieqRARate))
        return true;

    return false;
}

bool IOptronV3::SetTrackEnabled(bool enabled)
{
    if (enabled)
    {
        // If we are engaging tracking, let us first set tracking mode, and if we have custom mode, then tracking rate.
        // NOTE: Is this the correct order? or should tracking be switched on first before making these changes? Need to test.
        SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP));
        if (TrackModeS[TR_CUSTOM].s == ISS_ON)
            SetTrackRate(TrackRateNP[AXIS_RA].value, TrackRateNP[AXIS_DE].getValue());
    }

    return driver->setTrackEnabled(enabled);
}

/* v3.0 PEC add data status to the Driver */
bool IOptronV3::GetPECDataStatus(bool enabled)
{
    if(driver->getPETEnabled(true))
    {
        if (enabled)
        {
            PECInfoTP[0].setText("Recorded");
            PECInfoTP.apply();
            LOG_INFO("Mount PEC Chip Ready and Trained");
        }
        return true;
    }
    else
    {
        if (enabled)
        {
            PECInfoTP[0].setText("None");
            PECInfoTP.apply();
            LOG_INFO("Mount PEC Chip Needs Training");
        }
    }
    return false;
}
