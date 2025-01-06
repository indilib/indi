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

/* Constructor */
IOptronV3::IOptronV3(): GI(this)
{
    setVersion(1, 7);

    driver.reset(new Driver(getDeviceName()));

    scopeInfo.gpsStatus    = GPS_OFF;
    scopeInfo.trackRate    = TR_SIDEREAL;
    scopeInfo.systemStatus = ST_TRACKING_PEC_OFF;
    scopeInfo.slewRate     = SR_MAX;
    scopeInfo.timeSource   = TS_RS232;
    scopeInfo.hemisphere   = HEMI_NORTH;

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_PEC  |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TRACK_RATE |
                           TELESCOPE_HAS_PIER_SIDE |
                           TELESCOPE_CAN_HOME_FIND |
                           TELESCOPE_CAN_HOME_SET |
                           TELESCOPE_CAN_HOME_GO,
                           9
                          );
}

const char *IOptronV3::getDefaultName()
{
    return "iOptronV3";
}

bool IOptronV3::initProperties()
{
    INDI::Telescope::initProperties();

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

    // Max is the default
    SlewRateSP[8].setState(ISS_ON);

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
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_KING", "King");
    AddTrackMode("TRACK_CUSTOM", "Custom");

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

    /* v3.0 Create PEC Training switches */
    // PEC Training
    IUFillSwitch(&PECTrainingS[0], "PEC_Recording", "Record", ISS_OFF);
    IUFillSwitch(&PECTrainingS[1], "PEC_Status", "Status", ISS_OFF);
    IUFillSwitchVector(&PECTrainingSP, PECTrainingS, 2, getDeviceName(), "PEC_TRAINING", "Training", MOTION_TAB, IP_RW,
                       ISR_ATMOST1, 0,
                       IPS_IDLE);

    // Create PEC Training Information */
    IUFillText(&PECInfoT[0], "PEC_INFO", "Status", "");
    IUFillTextVector(&PECInfoTP, PECInfoT, 1, getDeviceName(), "PEC_INFOS", "Data", MOTION_TAB,
                     IP_RO, 60, IPS_IDLE);
    // End Mod */

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[0], "RA_GUIDE_RATE", "x Sidereal", "%g", 0.01, 0.9, 0.1, 0.5);
    IUFillNumber(&GuideRateN[1], "DE_GUIDE_RATE", "x Sidereal", "%g", 0.1, 0.99, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);


    /* Slew Mode. Normal vs Counter Weight up */
    IUFillSwitch(&SlewModeS[IOP_CW_NORMAL], "Normal", "Normal", ISS_ON);
    IUFillSwitch(&SlewModeS[IOP_CW_UP], "Counterweight UP", "Counterweight up", ISS_OFF);
    IUFillSwitchVector(&SlewModeSP, SlewModeS, 2, getDeviceName(), "Slew Type", "Slew Type", MOTION_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    /* Daylight Savings */
    IUFillSwitch(&DaylightS[0], "ON", "ON", ISS_OFF);
    IUFillSwitch(&DaylightS[1], "OFF", "OFF", ISS_ON);
    IUFillSwitchVector(&DaylightSP, DaylightS, 2, getDeviceName(), "DaylightSaving", "Daylight Savings", SITE_TAB, IP_RW,
                       ISR_1OFMANY, 0,
                       IPS_IDLE);

    /* Counter Weight State */
    IUFillSwitch(&CWStateS[IOP_CW_NORMAL], "Normal", "Normal", ISS_ON);
    IUFillSwitch(&CWStateS[IOP_CW_UP], "Up", "Up", ISS_OFF);
    IUFillSwitchVector(&CWStateSP, CWStateS, 2, getDeviceName(), "CWState", "Counter weights", MOTION_TAB, IP_RO, ISR_1OFMANY,
                       0, IPS_IDLE);

    /* Meridian Behavior */
    MeridianActionSP[IOP_MB_STOP].fill("IOP_MB_STOP", "Stop", ISS_ON);
    MeridianActionSP[IOP_MB_FLIP].fill("IOP_MB_FLIP", "Flip", ISS_OFF);
    MeridianActionSP.fill(getDeviceName(), "MERIDIAN_ACTION", "Action", MB_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    MeridianActionSP.load();

    /* Meridian Limit */
    MeridianLimitNP[0].fill("VALUE", "Degrees", "%.f", 0, 10, 1, 0);
    MeridianLimitNP.fill(getDeviceName(), "MERIDIAN_LIMIT", "Limit", MB_TAB, IP_RW, 60, IPS_IDLE);
    MeridianLimitNP.load();

    if (strstr(getDeviceName(), "iMate"))
        serialConnection->setDefaultPort("/dev/ttyS7");

    // Baud rates.
    // 230400 for 120
    // 115000 for everything else
    if (strstr(getDeviceName(), "120"))
        serialConnection->setDefaultBaudRate(Connection::Serial::B_230400);
    else
        serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    // Default WiFi connection parameters
    tcpConnection->setDefaultHost("10.10.100.254");
    tcpConnection->setDefaultPort(8899);

    TrackState = SCOPE_IDLE;

    GI::initProperties(MOTION_TAB);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    SetParkDataType(PARK_AZ_ALT);

    addAuxControls();

    currentRA  = get_local_sidereal_time(LocationNP[LOCATION_LONGITUDE].getValue());
    currentDEC = LocationNP[LOCATION_LATITUDE].getValue() > 0 ? 90 : -90;
    driver->setSimLongLat(LocationNP[LOCATION_LONGITUDE].getValue() > 180 ? LocationNP[LOCATION_LONGITUDE].getValue() - 360 :
                          LocationNP[LOCATION_LONGITUDE].getValue(), LocationNP[LOCATION_LATITUDE].getValue());

    return true;
}

bool IOptronV3::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        /* v3.0 Create PEC switches */
        defineProperty(&PECTrainingSP);
        defineProperty(&PECInfoTP);
        // End Mod */

        defineProperty(&GuideRateNP);

        defineProperty(&FirmwareTP);
        defineProperty(&GPSStatusSP);
        defineProperty(&TimeSourceSP);
        defineProperty(&HemisphereSP);
        defineProperty(&SlewModeSP);
        defineProperty(&DaylightSP);
        defineProperty(&CWStateSP);

        defineProperty(MeridianActionSP);
        defineProperty(MeridianLimitNP);

        getStartupData();
    }
    else
    {
        /* v3.0 Delete PEC switches */
        deleteProperty(PECTrainingSP.name);
        deleteProperty(PECInfoTP.name);
        // End Mod*/

        deleteProperty(GuideRateNP.name);

        deleteProperty(FirmwareTP.name);
        deleteProperty(GPSStatusSP.name);
        deleteProperty(TimeSourceSP.name);
        deleteProperty(HemisphereSP.name);
        deleteProperty(SlewModeSP.name);
        deleteProperty(DaylightSP.name);
        deleteProperty(CWStateSP.name);

        deleteProperty(MeridianActionSP);
        deleteProperty(MeridianLimitNP);
    }

    GI::updateProperties();

    return true;
}

void IOptronV3::getStartupData()
{
    LOG_DEBUG("Getting firmware data...");
    if (driver->getFirmwareInfo(&firmwareInfo))
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
    double RARate = 0, DERate = 0;
    if (driver->getGuideRate(&RARate, &DERate))
    {
        GuideRateN[RA_AXIS].value = RARate;
        GuideRateN[DEC_AXIS].value = DERate;
        IDSetNumber(&GuideRateNP, nullptr);
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
        TimeTP[UTC].setText(ts);
        LOGF_INFO("Mount UTC: %s", ts);

        // UTC Offset
        char offset[8] = {0};
        // 2021-05-12 JM: Account for daylight savings
        if (dayLightSavings)
            utcOffsetMinutes += 60;

        snprintf(offset, 8, "%.2f", utcOffsetMinutes / 60.0);
        TimeTP[OFFSET].setText(offset);
        LOGF_INFO("Mount UTC Offset: %s", offset);

        TimeTP.setState(IPS_OK);
        TimeTP.apply();

        LOGF_INFO("Mount Daylight Savings: %s", dayLightSavings ? "ON" : "OFF");
        DaylightS[0].s = dayLightSavings ? ISS_ON : ISS_OFF;
        DaylightS[1].s = !dayLightSavings ? ISS_ON : ISS_OFF;
        DaylightSP.s = IPS_OK;
        IDSetSwitch(&DaylightSP, nullptr);
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

    IOP_MB_STATE action;
    uint8_t degrees = 0;
    if (driver->getMeridianBehavior(action, degrees))
    {
        MeridianActionSP.reset();
        MeridianActionSP[action].setState(ISS_ON);
        MeridianActionSP.setState(IPS_OK);
        MeridianLimitNP[0].setValue(degrees);

        LOGF_INFO("Reading mount meridian behavior: When mount reaches %.f degrees past meridian, it will %s.",
                  MeridianLimitNP[0].getValue(), MeridianActionSP[IOP_MB_STOP].getState() == ISS_ON ? "stop" : "flip");
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
    // Check guider interface
    if (GI::processNumber(dev, name, values, names, n))
        return true;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Guiding Rate
        if (!strcmp(name, GuideRateNP.name))
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);

            if (driver->setGuideRate(GuideRateN[RA_AXIS].value, GuideRateN[DEC_AXIS].value))
                GuideRateNP.s = IPS_OK;
            else
                GuideRateNP.s = IPS_ALERT;

            IDSetNumber(&GuideRateNP, nullptr);

            return true;
        }

        /****************************************
         Meridian Flip Limit
        *****************************************/
        if (MeridianLimitNP.isNameMatch(name))
        {
            auto lastLimit = MeridianLimitNP[0].getValue();
            MeridianLimitNP.update(values, names, n);
            // Only update driver if there is an actual change
            if (lastLimit != MeridianLimitNP[0].getValue())
            {
                MeridianLimitNP.setState(driver->setMeridianBehavior(static_cast<IOP_MB_STATE>(MeridianActionSP.findOnSwitchIndex()),
                                         MeridianLimitNP[0].getValue()) ? IPS_OK : IPS_ALERT);
                if (MeridianLimitNP.getState() == IPS_OK)
                {
                    LOGF_INFO("Setting mount meridian behavior: When mount reaches %.f degrees past meridian, it will %s.",
                              MeridianLimitNP[0].getValue(), MeridianActionSP[IOP_MB_STOP].getState() == ISS_ON ? "stop" : "flip");
                }
            }
            else
                MeridianLimitNP.setState(IPS_OK);

            MeridianLimitNP.apply();
            saveConfig(MeridianLimitNP);
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
         * Slew Mode Operations
        *******************************************************/
        if (!strcmp(name, SlewModeSP.name))
        {
            IUUpdateSwitch(&SlewModeSP, states, names, n);
            SlewModeSP.s = IPS_OK;
            IDSetSwitch(&SlewModeSP, nullptr);
            return true;
        }

        /*******************************************************
         * Daylight Savings Operations
        *******************************************************/
        if (!strcmp(name, DaylightSP.name))
        {
            IUUpdateSwitch(&DaylightSP, states, names, n);

            if (driver->setDaylightSaving(DaylightS[0].s == ISS_ON))
                DaylightSP.s = IPS_OK;
            else
                DaylightSP.s = IPS_ALERT;

            IDSetSwitch(&DaylightSP, nullptr);
            return true;
        }

        /*******************************************************
         * Meridian Action Operations
        *******************************************************/
        if (MeridianActionSP.isNameMatch(name))
        {
            auto lastAction = MeridianActionSP.findOnSwitchIndex();
            MeridianActionSP.update(states, names, n);

            if (lastAction != MeridianActionSP.findOnSwitchIndex())
            {
                MeridianActionSP.setState(driver->setMeridianBehavior(static_cast<IOP_MB_STATE>(MeridianActionSP.findOnSwitchIndex()),
                                          MeridianLimitNP[0].getValue()) ? IPS_OK : IPS_ALERT);
                if (MeridianActionSP.getState() == IPS_OK)
                {
                    LOGF_INFO("Setting mount meridian behavior: When mount reaches %.f degrees past meridian, it will %s.",
                              MeridianLimitNP[0].getValue(), MeridianActionSP[IOP_MB_STOP].getState() == ISS_ON ? "stop" : "flip");
                }
            }
            else
                MeridianActionSP.setState(IPS_OK);
            MeridianActionSP.apply();
            saveConfig(MeridianActionSP);
            return true;
        }

        /* v3.0 PEC add controls and calls to the driver */
        if (PECStateSP.isNameMatch(name))
        {
            PECStateSP.update(states, names, n);

            if(PECStateSP[PEC_OFF].getState() == ISS_ON)
            {
                // PEC OFF
                if(isTraining)
                {
                    // Training check
                    LOGF_WARN("Mount PEC busy recording, %d s", PECTime);
                }
                else
                {
                    driver->setPECEnabled(false);
                    PECStateSP.setState(IPS_OK);
                    LOG_INFO("Disabling PEC Chip");
                }
            }
            else
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

        /* v3.0 PEC add Training Controls to the Driver */
        if (!strcmp(name, PECTrainingSP.name))
        {
            IUUpdateSwitch(&PECTrainingSP, states, names, n);
            if(isTraining)
            {
                // Check if already training
                if(IUFindOnSwitchIndex(&PECTrainingSP) == 1)
                {
                    // Train Check Status
                    LOGF_WARN("Mount PEC busy recording, %d s", PECTime);
                }

                if(IUFindOnSwitchIndex(&PECTrainingSP) == 0)
                {
                    // Train Cancel
                    driver->setPETEnabled(false);
                    isTraining = false;
                    PECTrainingSP.s = IPS_ALERT;
                    LOG_WARN("PEC Training cancelled by user, chip disabled");
                }
            }
            else
            {
                if(IUFindOnSwitchIndex(&PECTrainingSP) == 0)
                {
                    if(TrackState == SCOPE_TRACKING)
                    {
                        // Train if tracking /guiding
                        driver->setPETEnabled(true);
                        isTraining = true;
                        PECTime = 0;
                        PECTrainingSP.s = IPS_BUSY;
                        LOG_INFO("PEC recording started...");
                    }
                    else
                    {
                        LOG_WARN("PEC Training only possible while guiding");
                        PECTrainingSP.s = IPS_IDLE;
                    }
                }
                if(IUFindOnSwitchIndex(&PECTrainingSP) == 1)
                {
                    // Train Status
                    GetPECDataStatus(true);
                }
            }
            IDSetSwitch(&PECTrainingSP, nullptr);
            return true;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool IOptronV3::ReadScopeStatus()
{
    bool rc = false;

    IOPInfo newInfo;

    if (isSimulation())
        mountSim();

    // Do not query mount if parked already.
    if (TrackState == SCOPE_PARKED)
        return true;

    rc = driver->getStatus(&newInfo);

    if (rc)
    {
        if (IUFindOnSwitchIndex(&GPSStatusSP) != newInfo.gpsStatus)
        {
            IUResetSwitch(&GPSStatusSP);
            GPSStatusS[newInfo.gpsStatus].s = ISS_ON;
            IDSetSwitch(&GPSStatusSP, nullptr);
        }

        if (IUFindOnSwitchIndex(&TimeSourceSP) != newInfo.timeSource)
        {
            IUResetSwitch(&TimeSourceSP);
            TimeSourceS[newInfo.timeSource].s = ISS_ON;
            IDSetSwitch(&TimeSourceSP, nullptr);
        }

        if (IUFindOnSwitchIndex(&HemisphereSP) != newInfo.hemisphere)
        {
            IUResetSwitch(&HemisphereSP);
            HemisphereS[newInfo.hemisphere].s = ISS_ON;
            IDSetSwitch(&HemisphereSP, nullptr);
        }

        if (SlewRateSP.findOnSwitchIndex() != newInfo.slewRate - 1)
        {
            SlewRateSP.reset();
            SlewRateSP[newInfo.slewRate - 1].setState(ISS_ON);
            SlewRateSP.apply();
        }

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
                if (HomeSP.getState() == IPS_BUSY)
                {
                    HomeSP.reset();
                    HomeSP.setState(IPS_OK);
                    HomeSP.apply();
                }
                break;
            case ST_HOME:
                TrackModeSP.setState(IPS_IDLE);
                TrackState    = SCOPE_IDLE;
                if (HomeSP.getState() == IPS_BUSY)
                {
                    HomeSP.reset();
                    HomeSP.setState(IPS_OK);
                    HomeSP.apply();
                }
                break;
            case ST_SLEWING:
            case ST_MERIDIAN_FLIPPING:
                if (TrackState != SCOPE_SLEWING && TrackState != SCOPE_PARKING)
                    TrackState = SCOPE_SLEWING;
                break;
            case ST_TRACKING_PEC_OFF:
            case ST_TRACKING_PEC_ON:
            case ST_GUIDING:
                if (newInfo.systemStatus == ST_TRACKING_PEC_OFF || newInfo.systemStatus == ST_TRACKING_PEC_ON)
                    setPECState(newInfo.systemStatus == ST_TRACKING_PEC_ON ? PEC_ON : PEC_OFF);
                TrackModeSP.setState(IPS_BUSY);
                TrackState    = SCOPE_TRACKING;
                if (scopeInfo.systemStatus == ST_SLEWING)
                    LOG_INFO("Slew complete, tracking...");
                else if (scopeInfo.systemStatus == ST_MERIDIAN_FLIPPING)
                    LOG_INFO("Meridian flip complete, tracking...");
                break;
        }

        if (TrackModeSP.findOnSwitchIndex() != newInfo.trackRate)
        {
            TrackModeSP.reset();
            TrackModeSP[newInfo.trackRate].setState(ISS_ON);
            TrackModeSP.apply();
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
                LOGF_INFO("%d second worm cycle recorded", PECTime);
                PECTrainingSP.s = IPS_OK;
                isTraining = false;
            }
            else
            {
                PECTime = PECTime + 1 * getCurrentPollingPeriod() / 1000;
                char PECText[MAXINDILABEL] = {0};
                snprintf(PECText, MAXINDILABEL, "Recording: %d s", PECTime);
                IUSaveText(&PECInfoT[0], PECText);
            }
        }
        else
        {
            driver->setPETEnabled(false);
            PECTrainingSP.s = IPS_ALERT;
            LOGF_ERROR("Tracking error, recording cancelled %d s", PECTime);
            IUSaveText(&PECInfoT[0], "Cancelled");
        }
        IDSetText(&PECInfoTP, nullptr);
        IDSetSwitch(&PECTrainingSP, nullptr);
    }
    // End Mod */

    IOP_PIER_STATE pierState = IOP_PIER_UNKNOWN;
    IOP_CW_STATE cwState = IOP_CW_NORMAL;

    double previousRA = currentRA, previousDE = currentDEC;
    rc = driver->getCoords(&currentRA, &currentDEC, &pierState, &cwState);
    if (rc)
    {
        // Add Extra Logging info
        if (isDebug())
        {
            char RAStr[64] = {0}, DecStr[64] = {0}, AzStr[64] = {0}, AltStr[64] = {0};
            fs_sexa(RAStr, currentRA, 2, 3600);
            fs_sexa(DecStr, currentDEC, 2, 3600);
            INDI::IEquatorialCoordinates equatorialCoords {currentRA, currentDEC};
            INDI::IHorizontalCoordinates horizontalCoords {0, 0};
            INDI::EquatorialToHorizontal(&equatorialCoords, &m_Location, ln_get_julian_from_sys(), &horizontalCoords);
            fs_sexa(AzStr, horizontalCoords.azimuth, 2, 3600);
            fs_sexa(AltStr, horizontalCoords.altitude, 2, 3600);
            std::string pierSide = "Uknkown";
            if (pierState == IOP_PIER_EAST)
                pierSide = "East";
            else if (pierState == IOP_PIER_WEST)
                pierSide = "West";
            DEBUGF(DBG_SCOPE, "RA: %s DE: %s AZ: %s AL: %s PierSide: %s CWState %d", RAStr, DecStr, AzStr, AltStr, pierSide.c_str(), cwState);
        }

        // 2021.11.30 JM: This is a hack to circumvent a bug in iOptorn firmware
        // the "system status" bit is set to SLEWING even when parking is done (2), it never
        // changes to (6) which indicates it has parked. So we use a counter to check if there
        // is no longer any motion.
        if (TrackState == SCOPE_PARKING)
        {
            if (std::abs(previousRA - currentRA) < 0.01 && std::abs(previousDE - currentDEC) < 0.01)
            {
                m_ParkingCounter++;
                if (m_ParkingCounter >= MAX_PARK_COUNTER)
                {
                    m_ParkingCounter = 0;
                    SetTrackEnabled(false);
                    SetParked(true);
                }
            }
        }
        if (pierState == IOP_PIER_UNKNOWN)
            setPierSide(PIER_UNKNOWN);
        else
            setPierSide(pierState == IOP_PIER_EAST ? PIER_EAST : PIER_WEST);

        if (IUFindOnSwitchIndex(&CWStateSP) != cwState)
        {
            IUResetSwitch(&CWStateSP);
            CWStateS[cwState].s = ISS_ON;
            IDSetSwitch(&CWStateSP, nullptr);
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
    if (IUFindOnSwitchIndex(&SlewModeSP) == IOP_CW_NORMAL)
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
        m_ParkingCounter = 0;
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
    // No communications while parked.
    if (TrackState == SCOPE_PARKED)
        return true;

    bool rc1 = driver->setUTCDateTime(ln_get_julian_day(utc));

    bool rc2 = driver->setUTCOffset(utc_offset * 60);

    return (rc1 && rc2);
}

bool IOptronV3::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    // No communications while parked.
    if (TrackState == SCOPE_PARKED)
        return true;

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
    IOP_SLEW_RATE rate = static_cast<IOP_SLEW_RATE>(index);
    return driver->setSlewRate(rate);
}

bool IOptronV3::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &SlewModeSP);
    IUSaveConfigSwitch(fp, &DaylightSP);

    MeridianLimitNP.save(fp);
    MeridianActionSP.save(fp);

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
    double currentSlewRate = Driver::IOP_SLEW_RATES[SlewRateSP.findOnSwitchIndex()] * TRACKRATE_SIDEREAL / 3600.0;
    da  = currentSlewRate * dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act accordingly */
    switch (TrackState)
    {
        case SCOPE_IDLE:
            currentRA += (TrackRateNP[AXIS_RA].getValue() / 3600.0 * dt) / 15.0;
            currentRA = range24(currentRA);
            break;

        case SCOPE_TRACKING:
            if (TrackModeSP[TR_CUSTOM].getState() == ISS_ON)
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
    INDI::IEquatorialCoordinates equatorialCoords {currentRA, currentDEC};
    INDI::IHorizontalCoordinates horizontalCoords {0, 0};
    INDI::EquatorialToHorizontal(&equatorialCoords, &m_Location, ln_get_julian_from_sys(), &horizontalCoords);
    double parkAZ = horizontalCoords.azimuth;
    // Wrap to 0
    if (parkAZ >= 360)
        parkAZ = 0;
    double parkAlt = horizontalCoords.altitude;
    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr, AltStr);
    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);
    driver->setParkAz(parkAZ);
    driver->setParkAlt(parkAlt);
    return true;
}

bool IOptronV3::SetDefaultPark()
{
    // By default azimuth 0
    SetAxis1Park(0);
    // Altitude = latitude of observer
    SetAxis2Park(LocationNP[LOCATION_LATITUDE].getValue());
    driver->setParkAz(0);
    driver->setParkAlt(LocationNP[LOCATION_LATITUDE].getValue());
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
        SetTrackMode(TrackModeSP.findOnSwitchIndex());
        if (TrackModeSP[TR_CUSTOM].getState() == ISS_ON)
            SetTrackRate(TrackRateNP[AXIS_RA].getValue(), TrackRateNP[AXIS_DE].getValue());
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
            IUSaveText(&PECInfoT[0], "Recorded");
            IDSetText(&PECInfoTP, nullptr);
            LOG_INFO("Mount PEC Chip Ready and Trained");
        }
        return true;
    }
    else
    {
        if (enabled)
        {
            IUSaveText(&PECInfoT[0], "None");
            IDSetText(&PECInfoTP, nullptr);
            LOG_INFO("Mount PEC Chip Needs Training");
        }
    }
    return false;
}

IPState IOptronV3::ExecuteHomeAction(TelescopeHomeAction action)
{
    switch (action)
    {
        case HOME_FIND:
            if (firmwareInfo.Model.find("CEM") == std::string::npos &&
                    firmwareInfo.Model.find("GEM45") == std::string::npos &&
                    firmwareInfo.Model.find("HAE") == std::string::npos &&
                    firmwareInfo.Model.find("HAZ") == std::string::npos &&
                    firmwareInfo.Model.find("HEM") == std::string::npos)
            {
                LOG_WARN("Home search is not supported in this model.");
                return IPS_ALERT;
            }

            if (driver->findHome() == false)
            {
                return IPS_ALERT;
            }

            LOG_INFO("Searching for home position...");
            return IPS_BUSY;

        case HOME_SET:
            if (driver->setCurrentHome() == false)
            {
                return IPS_ALERT;
            }

            LOG_INFO("Home position set to current coordinates.");
            return IPS_OK;

        case HOME_GO:
            if (driver->gotoHome() == false)
            {
                return IPS_ALERT;
            }

            LOG_INFO("Slewing to home position...");
            return IPS_BUSY;

        default:
            return IPS_ALERT;
    }

    return IPS_ALERT;
}
