/*
    10micron INDI driver
    GM1000HPS GM2000QCI GM2000HPS GM3000HPS GM4000QCI GM4000HPS AZ2000
    Mount Command Protocol 2.14.11

    Copyright (C) 2017 Hans Lambermont

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

#include "lx200_10micron.h"
#include "indicom.h"
#include "lx200driver.h"

#include <cstring>
#include <strings.h>
#include <termios.h>
#include <math.h>
#include <libnova.h>

#define PRODUCT_TAB   "Product"
#define ALIGNMENT_TAB "Alignment"
#define SATELLITE_TAB "Satellite"
#define LX200_TIMEOUT 5 /* FD timeout in seconds */

LX200_10MICRON::LX200_10MICRON() : LX200Generic()
{
    setLX200Capability( LX200_HAS_TRACKING_FREQ | LX200_HAS_PULSE_GUIDING );

    SetTelescopeCapability( TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT |
        TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE |
        TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_TRACK_RATE );

    setVersion(1, 0);
}

// Called by INDI::DefaultDevice::ISGetProperties
// Note that getDriverName calls ::getDefaultName which returns LX200 Generic
const char *LX200_10MICRON::getDefaultName()
{
    return (const char *)"10micron";
}

// Called by either TCP Connect or Serial Port Connect
bool LX200_10MICRON::Handshake()
{
    fd = PortFD;

    if (isSimulation() == true)
    {
        LOG_INFO("Simulate Connect.");
        return true;
    }

    // Set Ultra Precision Mode #:U2# , replies like 15:58:19.49 instead of 15:21.2
    LOG_INFO("Setting Ultra Precision Mode.");
    if (setCommandInt(fd, 2, "#:U") < 0)
    {
        LOG_ERROR("Failed to set Ultra Precision Mode.");
        return false;
    }

    return true;
}

// Called by ISGetProperties to initialize basic properties that are required all the time
bool LX200_10MICRON::initProperties()
{
    const bool result = LX200Generic::initProperties();

    // TODO initialize properties additional to INDI::Telescope

    IUFillNumber(&RefractionModelTemperatureN[0], "TEMPERATURE", "Celsius", "%+6.1f", -999.9, 999.9, 0, 0.);
    IUFillNumberVector(&RefractionModelTemperatureNP, RefractionModelTemperatureN, 1, getDeviceName(),
        "REFRACTION_MODEL_TEMPERATURE", "Temperature", ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&RefractionModelPressureN[0], "PRESSURE", "hPa", "%6.1f", 0.0, 9999.9, 0, 0.);
    IUFillNumberVector(&RefractionModelPressureNP, RefractionModelPressureN, 1, getDeviceName(),
        "REFRACTION_MODEL_PRESSURE", "Pressure", ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&ModelCountN[0], "COUNT", "#", "%.0f", 0, 999, 0, 0);
    IUFillNumberVector(&ModelCountNP, ModelCountN, 1, getDeviceName(),
        "MODEL_COUNT", "Models", ALIGNMENT_TAB, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&AlignmentPointsN[0], "COUNT", "#", "%.0f", 0, 100, 0, 0);
    IUFillNumberVector(&AlignmentPointsNP, AlignmentPointsN, 1, getDeviceName(),
        "ALIGNMENT_POINTS", "Points", ALIGNMENT_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&AlignmentStateS[ALIGN_IDLE], "Idle", "Idle", ISS_ON);
    IUFillSwitch(&AlignmentStateS[ALIGN_START], "Start", "Start new model", ISS_OFF);
    IUFillSwitch(&AlignmentStateS[ALIGN_END], "End", "End new model", ISS_OFF);
    IUFillSwitch(&AlignmentStateS[ALIGN_DELETE_CURRENT], "Del", "Delete current model", ISS_OFF);
    IUFillSwitchVector(&AlignmentSP, AlignmentStateS, ALIGN_COUNT, getDeviceName(), "Alignment", "Alignment", ALIGNMENT_TAB,
        IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&MiniNewAlpRON[MALPRO_MRA], "MRA", "Mount RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&MiniNewAlpRON[MALPRO_MDEC], "MDEC", "Mount DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumber(&MiniNewAlpRON[MALPRO_MSIDE], "MSIDE", "Pier Side (0=E 1=W)", "%.0f", 0, 1, 0, 0);
    IUFillNumber(&MiniNewAlpRON[MALPRO_SIDTIME], "SIDTIME", "Sidereal Time (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumberVector(&MiniNewAlpRONP, MiniNewAlpRON, MALPRO_COUNT, getDeviceName(), "MINIMAL_NEW_ALIGNMENT_POINT_RO",
        "Actual", ALIGNMENT_TAB, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&MiniNewAlpN[MALP_PRA], "PRA", "Solved RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&MiniNewAlpN[MALP_PDEC], "PDEC", "Solved DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumberVector(&MiniNewAlpNP, MiniNewAlpN, MALP_COUNT, getDeviceName(), "MINIMAL_NEW_ALIGNMENT_POINT",
        "New Point", ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&NewAlpN[ALP_MRA], "MRA", "Mount RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&NewAlpN[ALP_MDEC], "MDEC", "Mount DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumber(&NewAlpN[ALP_MSIDE], "MSIDE", "Pier Side (0=E 1=W)", "%.0f", 0, 1, 0, 0);
    IUFillNumber(&NewAlpN[ALP_SIDTIME], "SIDTIME", "Sidereal Time (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&NewAlpN[ALP_PRA], "PRA", "Solved RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
    IUFillNumber(&NewAlpN[ALP_PDEC], "PDEC", "Solved DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    IUFillNumberVector(&NewAlpNP, NewAlpN, ALP_COUNT, getDeviceName(), "NEW_ALIGNMENT_POINT",
        "New Point", ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&NewAlignmentPointsN[0], "COUNT", "#", "%.0f", 0, 100, 1, 0);
    IUFillNumberVector(&NewAlignmentPointsNP, NewAlignmentPointsN, 1, getDeviceName(),
        "NEW_ALIGNMENT_POINTS", "New Points", ALIGNMENT_TAB, IP_RO, 60, IPS_IDLE);

    IUFillText(&NewModelNameT[0], "NAME", "Model Name", "newmodel");
    IUFillTextVector(&NewModelNameTP, NewModelNameT, 1, getDeviceName(), "NEW_MODEL_NAME", "New Name", ALIGNMENT_TAB,
                     IP_RW, 60, IPS_IDLE);

    IUFillText(&TLEtoUploadT[0], "TLE", "TLE", "");
    IUFillTextVector(&TLEtoUploadTP, TLEtoUploadT, 1, getDeviceName(), "TLE_TEXT", "TLE", SATELLITE_TAB,
                     IP_RW, 60, IPS_IDLE);

    IUFillNumber(&TLEfromDatabaseN[0], "NUMBER", "#", "%.0f", 1, 999, 1, 1);
    IUFillNumberVector(&TLEfromDatabaseNP, TLEfromDatabaseN, 1, getDeviceName(),
                       "TLE_NUMBER", "Database TLE ", SATELLITE_TAB, IP_RW, 60, IPS_IDLE);

    ln_get_date_from_sys(&today);
    IUFillNumber(&CalculateSatTrajectoryForTimeN[SAT_YYYY], "YEAR", "Year (yyyy)", "%.0f", 0, 9999, 0, today.years);
    IUFillNumber(&CalculateSatTrajectoryForTimeN[SAT_MM], "MONTH", "Month (mm)", "%.0f", 1, 12, 0, today.months);
    IUFillNumber(&CalculateSatTrajectoryForTimeN[SAT_DD], "DAY", "Day (dd)", "%.0f", 1, 31, 0, today.days);
    IUFillNumber(&CalculateSatTrajectoryForTimeN[SAT_HH24], "HOUR", "Hour 24 (hh)", "%.0f", 0, 24, 0, today.hours);
    IUFillNumber(&CalculateSatTrajectoryForTimeN[SAT_MM60], "MINUTE", "Minute", "%.0f", 0, 60, 0, today.minutes);
    IUFillNumber(&CalculateSatTrajectoryForTimeN[SAT_MM1440_NEXT], "COMING",
                 "In the following # minutes", "%.0f", 0, 1440, 0, 0);
    IUFillNumberVector(&CalculateSatTrajectoryForTimeNP, CalculateSatTrajectoryForTimeN,
                       SAT_COUNT, getDeviceName(), "TRAJECTORY_TIME",
                       "Sat pass", SATELLITE_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&TrackSatS[SAT_TRACK], "Track", "Track", ISS_OFF);
    IUFillSwitch(&TrackSatS[SAT_HALT], "Halt", "Halt", ISS_ON);
    IUFillSwitchVector(&TrackSatSP, TrackSatS, SAT_TRACK_COUNT, getDeviceName(), "SAT_TRACKING_STAT",
                       "Sat tracking", SATELLITE_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    return result;
}

// this should move to some generic library
int LX200_10MICRON::monthToNumber(const char *monthName)
{
    struct entry
    {
        const char *name;
        int id;
    };
    entry month_table[] = { { "Jan", 1 },  { "Feb", 2 },  { "Mar", 3 },  { "Apr", 4 }, { "May", 5 },
                            { "Jun", 6 },  { "Jul", 7 },  { "Aug", 8 },  { "Sep", 9 }, { "Oct", 10 },
                            { "Nov", 11 }, { "Dec", 12 }, { nullptr, 0 } };
    entry *p            = month_table;
    while (p->name != nullptr)
    {
        if (strcasecmp(p->name, monthName) == 0)
            return p->id;
        ++p;
    }
    return 0;
}

// Called by INDI::Telescope when connected state changes to add/remove properties
bool LX200_10MICRON::updateProperties()
{
    if (isConnected())
    {
        // getMountInfo defines ProductTP
        defineNumber(&RefractionModelTemperatureNP);
        defineNumber(&RefractionModelPressureNP);
        defineNumber(&ModelCountNP);
        defineNumber(&AlignmentPointsNP);
        defineSwitch(&AlignmentSP);
        defineNumber(&MiniNewAlpRONP);
        defineNumber(&MiniNewAlpNP);
        defineNumber(&NewAlpNP);
        defineNumber(&NewAlignmentPointsNP);
        defineText(&NewModelNameTP);
        defineText(&TLEtoUploadTP);
        defineNumber(&TLEfromDatabaseNP);
        defineNumber(&CalculateSatTrajectoryForTimeNP);
        defineSwitch(&TrackSatSP);
    }
    else
    {
        deleteProperty(ProductTP.name);
        deleteProperty(RefractionModelTemperatureNP.name);
        deleteProperty(RefractionModelPressureNP.name);
        deleteProperty(ModelCountNP.name);
        deleteProperty(AlignmentPointsNP.name);
        deleteProperty(AlignmentSP.name);
        deleteProperty(MiniNewAlpRONP.name);
        deleteProperty(MiniNewAlpNP.name);
        deleteProperty(NewAlpNP.name);
        deleteProperty(NewAlignmentPointsNP.name);
        deleteProperty(NewModelNameTP.name);
        deleteProperty(TLEtoUploadTP.name);
        deleteProperty(TLEfromDatabaseNP.name);
        deleteProperty(CalculateSatTrajectoryForTimeNP.name);
        deleteProperty(TrackSatSP.name);
    }
    bool result = LX200Generic::updateProperties();
    return result;
}

// INDI::Telescope calls ReadScopeStatus() every POLLMS to check the link to the telescope and update its state and position.
// The child class should call newRaDec() whenever a new value is read from the telescope.
bool LX200_10MICRON::ReadScopeStatus()
{
    if (!isConnected())
    {
        return false;
    }
    if (isSimulation())
    {
        mountSim();
        return true;
    }

    // Read scope status, based loosely on LX200_GENERIC::getCommandString
    char cmd[] = "#:Ginfo#";
    char data[80];
    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;
    // DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "CMD <%s>", cmd);
    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
    {
        return false;
    }
    error_type = tty_read_section(fd, data, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);
    if (error_type != TTY_OK)
    {
        return false;
    }
    term = strchr(data, '#');
    if (term)
    {
        *(term + 1) = '\0';
    }
    else
    {
        return false;
    }
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "CMD <%s> RES <%s>", cmd, data);

    // TODO: check if this needs changing when satellite tracking
    // Now parse the data. This format may consist of more parts some day
    nbytes_read = sscanf(data, "%g,%g,%c,%g,%g,%g,%d,%d#", &Ginfo.RA_JNOW, &Ginfo.DEC_JNOW, &Ginfo.SideOfPier,
        &Ginfo.AZ, &Ginfo.ALT, &Ginfo.Jdate, &Ginfo.Gstat, &Ginfo.SlewStatus);
    if (nbytes_read < 0)
    {
        return false;
    }

    if (Ginfo.Gstat != OldGstat)
    {
        if (OldGstat != GSTAT_UNSET)
        {
            LOGF_INFO("Gstat changed from %d to %d", OldGstat, Ginfo.Gstat);
        }
        else
        {
            LOGF_INFO("Gstat initialized at %d", Ginfo.Gstat);
        }
    }
    switch (Ginfo.Gstat)
    {
        case GSTAT_TRACKING:
            TrackState = SCOPE_TRACKING;
            break;
        case GSTAT_STOPPED:
            TrackState = SCOPE_IDLE;
            break;
        case GSTAT_PARKING:
            TrackState = SCOPE_PARKING;
            break;
        case GSTAT_UNPARKING:
            TrackState = SCOPE_TRACKING;
            break;
        case GSTAT_SLEWING_TO_HOME:
            TrackState = SCOPE_SLEWING;
            break;
        case GSTAT_PARKED:
            TrackState = SCOPE_PARKED;
            if (!isParked())
                SetParked(true);
            break;
        case GSTAT_SLEWING_OR_STOPPING:
            TrackState = SCOPE_SLEWING;
            break;
        case GSTAT_NOT_TRACKING_AND_NOT_MOVING:
            TrackState = SCOPE_IDLE;
            break;
        case GSTAT_MOTORS_TOO_COLD:
            TrackState = SCOPE_IDLE;
            break;
        case GSTAT_TRACKING_OUTSIDE_LIMITS:
            TrackState = SCOPE_TRACKING;
            break;
        case GSTAT_FOLLOWING_SATELLITE:
            TrackState = SCOPE_TRACKING;
            break;
        case GSTAT_NEED_USEROK:
            TrackState = SCOPE_IDLE;
            break;
        case GSTAT_UNKNOWN_STATUS:
            TrackState = SCOPE_IDLE;
            break;
        case GSTAT_ERROR:
            TrackState = SCOPE_IDLE;
            break;
        default:
            return false;
    }
    setPierSide((toupper(Ginfo.SideOfPier) == 'E') ? INDI::Telescope::PIER_EAST : INDI::Telescope::PIER_WEST);

    OldGstat = Ginfo.Gstat;
    NewRaDec(Ginfo.RA_JNOW, Ginfo.DEC_JNOW);

    // Update alignment Mini new alignment point Read-Only fields
    char LocalSiderealTimeS[80];
    getCommandString(fd, LocalSiderealTimeS, "#:GS#");
    f_scansexa(LocalSiderealTimeS, &Ginfo.SiderealTime);
    MiniNewAlpRON[MALPRO_MRA].value = Ginfo.RA_JNOW;
    MiniNewAlpRON[MALPRO_MDEC].value = Ginfo.DEC_JNOW;
    MiniNewAlpRON[MALPRO_MSIDE].value = (toupper(Ginfo.SideOfPier) == 'E') ? 0 : 1;
    MiniNewAlpRON[MALPRO_SIDTIME].value = Ginfo.SiderealTime;
    IDSetNumber(&MiniNewAlpRONP, nullptr);

    return true;
}

// Called by LX200Generic::updateProperties
void LX200_10MICRON::getBasicData()
{
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);

    // cannot call LX200Generic::getBasicData(); as getTimeFormat :Gc# (and getSiteName :GM#) are not implemented on 10Micron
    if (!isSimulation())
    {
        getMountInfo();

        getAlignment();
        checkLX200Format(fd);
        timeFormat = LX200_24;

        if (getTrackFreq(PortFD, &TrackFreqN[0].value) < 0)
        {
            LOG_WARN("Failed to get tracking frequency from device.");
        }
        else
        {
            LOGF_INFO("Tracking frequency is %.1f Hz", TrackFreqN[0].value);
            IDSetNumber(&TrackingFreqNP, nullptr);
        }

        char RefractionModelTemperature[80];
        getCommandString(PortFD, RefractionModelTemperature, "#:GRTMP#");
        float rmtemp;
        sscanf(RefractionModelTemperature, "%f#", &rmtemp);
        RefractionModelTemperatureN[0].value = (double) rmtemp;
        LOGF_INFO("RefractionModelTemperature is %0+6.1f degrees C", RefractionModelTemperatureN[0].value);
        IDSetNumber(&RefractionModelTemperatureNP, nullptr);

        char RefractionModelPressure[80];
        getCommandString(PortFD, RefractionModelPressure, "#:GRPRS#");
        float rmpres;
        sscanf(RefractionModelPressure, "%f#", &rmpres);
        RefractionModelPressureN[0].value = (double) rmpres;
        LOGF_INFO("RefractionModelPressure is %06.1f hPa", RefractionModelPressureN[0].value);
        IDSetNumber(&RefractionModelPressureNP, nullptr);

        int ModelCount;
        getCommandInt(PortFD, &ModelCount, "#:modelcnt#");
        ModelCountN[0].value = (double) ModelCount;
        LOGF_INFO("%d Alignment Models", (int) ModelCountN[0].value);
        IDSetNumber(&ModelCountNP, nullptr);

        int AlignmentPoints;
        getCommandInt(PortFD, &AlignmentPoints, "#:getalst#");
        AlignmentPointsN[0].value = (double) AlignmentPoints;
        LOGF_INFO("%d Alignment Stars in active model", (int) AlignmentPointsN[0].value);
        IDSetNumber(&AlignmentPointsNP, nullptr);
    }
    sendScopeLocation();
    sendScopeTime();
}

// Called by getBasicData
bool LX200_10MICRON::getMountInfo()
{
    char ProductName[80];
    getCommandString(PortFD, ProductName, "#:GVP#");
    char ControlBox[80];
    getCommandString(PortFD, ControlBox, "#:GVZ#");
    char FirmwareVersion[80];
    getCommandString(PortFD, FirmwareVersion, "#:GVN#");
    char FirmwareDate1[80];
    getCommandString(PortFD, FirmwareDate1, "#:GVD#");
    char FirmwareDate2[80];
    char mon[4];
    int dd, yyyy;
    sscanf(FirmwareDate1, "%s %02d %04d", mon, &dd, &yyyy);
    getCommandString(PortFD, FirmwareDate2, "#:GVT#");
    char FirmwareDate[80];
    snprintf(FirmwareDate, 80, "%04d-%02d-%02dT%s", yyyy, monthToNumber(mon), dd, FirmwareDate2);

    LOGF_INFO("Product:%s Control box:%s Firmware:%s of %s", ProductName, ControlBox, FirmwareVersion, FirmwareDate);

    IUFillText(&ProductT[PRODUCT_NAME], "NAME", "Product Name", ProductName);
    IUFillText(&ProductT[PRODUCT_CONTROL_BOX], "CONTROL_BOX", "Control Box", ControlBox);
    IUFillText(&ProductT[PRODUCT_FIRMWARE_VERSION], "FIRMWARE_VERSION", "Firmware Version", FirmwareVersion);
    IUFillText(&ProductT[PRODUCT_FIRMWARE_DATE], "FIRMWARE_DATE", "Firmware Date", FirmwareDate);
    IUFillTextVector(&ProductTP, ProductT, PRODUCT_COUNT, getDeviceName(), "PRODUCT_INFO", "Product", PRODUCT_TAB,
            IP_RO, 60, IPS_IDLE);

    defineText(&ProductTP);

    return true;
}

// this should move to some generic library
int LX200_10MICRON::setStandardProcedureWithoutRead(int fd, const char *data)
{
    int error_type;
    int nbytes_write = 0;

    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "CMD <%s>", data);
    if ((error_type = tty_write_string(fd, data, &nbytes_write)) != TTY_OK)
    {
        return error_type;
    }
    tcflush(fd, TCIFLUSH);
    return 0;
}
int LX200_10MICRON::setStandardProcedureAndExpect(int fd, const char *data, const char *expect)
{
    char bool_return[2];
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "CMD <%s>", data);

    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, data, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read(fd, bool_return, 1, LX200_TIMEOUT, &nbytes_read);

    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
        return error_type;

    if (bool_return[0] != expect[0])
    {
        DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "CMD <%s> failed.", data);
        return -1;
    }

    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "CMD <%s> successful.", data);

    return 0;
}

int LX200_10MICRON::setStandardProcedureAndReturnResponse(int fd, const char *data, char *response, int max_response_length)
{
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "CMD <%s>", data);

    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, data, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read(fd, response, max_response_length, LX200_TIMEOUT, &nbytes_read);

    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
        return error_type;

    return 0;
}

bool LX200_10MICRON::Park()
{
    LOG_INFO("Parking.");
    if (setStandardProcedureWithoutRead(fd, "#:KA#") < 0)
    {
        return false;
    }
    return true;
}

bool LX200_10MICRON::UnPark()
{
    LOG_INFO("Unparking.");
    if (setStandardProcedureWithoutRead(fd, "#:PO#") < 0)
    {
        return false;
    }
    SetParked(false);
    return true;
}

bool LX200_10MICRON::SyncConfigBehaviour(bool cmcfg)
{
    LOG_INFO("SyncConfig.");
    if (setCommandInt(fd, cmcfg, "#:CMCFG") < 0)
    {
        return false;
    }
    return true;
}

bool LX200_10MICRON::setLocalDate(uint8_t days, uint8_t months, uint16_t years)
{
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char data[64];
    snprintf(data, sizeof(data), ":SC%04d-%02d-%02d#", years, months, days);
    return 0 == setStandardProcedureAndExpect(fd, data, "1");
}

bool LX200_10MICRON::SetTLEtoFollow(const char *tle)
{
    LOGF_INFO("The function is called with TLE %s", tle);
    if (strlen(tle)>230)
    {
        LOG_WARN("TLE is too long");
    }

    std::string tle_str;
    std::string sep = "$0a";
    std::string search = "\n";
    tle_str = (std::string) tle;
    for( size_t pos = 0; ; pos += sep.length() ) {
        // Locate the substring to replace
        pos = tle_str.find( search, pos );
        if( pos == std::string::npos ) break;
        // Replace by erasing and inserting
        tle_str.erase( pos, search.length() );
        tle_str.insert( pos, sep );
    }
    char command[250];
    snprintf(command, sizeof(command), ":TLEL0%s#", tle_str.c_str());

    if ( !isSimulation() )
    {
        LOG_INFO(command);
        char response[2];
        if (0 != setStandardProcedureAndReturnResponse(fd, command, response, 2) )
        {
            LOG_ERROR("TLE set error");
            return 1;
        }
        if (response[0] == 'E')
        {
            LOG_ERROR("Invalid formatting of TLE, trying to split:");
            char *pch = strtok ((char*) tle,"\n");
            while (pch != NULL)
            {    
                LOGF_INFO("%s\n",pch);
                pch = strtok (NULL, "\n");
            }
            return 1;
        }
    }
    else
    {
        char *pch = strtok ((char*) tle,"\n");
        while (pch != NULL)
        { 
            LOGF_INFO("%s\n",pch);
            pch = strtok (NULL, "\n");
        }
    }
    return 0;
}
	
bool LX200_10MICRON::SetTLEfromDatabase(int tleN)
{
    char command[12];
    snprintf(command, sizeof(command), ":TLEDL%d#", tleN);

    LOG_INFO("Setting TLE from Database");
    if ( !isSimulation() )
    {
        LOG_INFO(command);
        char response[210];
        if (0 != setStandardProcedureAndReturnResponse(fd, command, response, 210) )
        {
            LOG_ERROR("TLE set error");
            return 1;
        }
        if (response[0] == 'E')
        {
            LOG_ERROR("TLE number not in mount");
            return 1;
        }
    }
    return 0;
}

bool LX200_10MICRON::CalculateTrajectory(int year, int month, int day, int hour, int minute, int nextpass, ln_date date_pass)
{
    LOGF_INFO("Calculate trajectory is called with date: %d-%d-%d %d:%d pass %d",
                year, month, day, hour, minute, nextpass);
    date_pass.years = year;
    date_pass.months = month;
    date_pass.days = day;
    date_pass.hours = hour;
    date_pass.minutes = minute;
    date_pass.seconds = 0.0;
    JD = ln_get_julian_day(&date_pass);

    char command[28];
    snprintf(command, sizeof(command), ":TLEP%7.8f,%01d#", JD, nextpass);
    LOGF_INFO("Julian day being %7.5f", JD);
    if ( !isSimulation() )
        {
            LOG_INFO(command);
            char response[36];
            if (0 != setStandardProcedureAndReturnResponse(fd, command, response, 36) )
            {
                LOG_ERROR("TLE calculate error");
                return 1;
            }
            if (response[0] == 'E')
            {
                LOG_ERROR("TLE not loaded or invalid command");
                return 1;
            }
            if (response[0] == 'N')
            {
                LOG_ERROR("No passes loaded");
                return 1;
            }
        }
    return 0;
}

bool LX200_10MICRON::TrackSat()
{
    LOG_INFO("Tracking satellite");
    char command[7];
    snprintf(command, sizeof(command), ":TLES#");
    if ( !isSimulation() )
    {
        LOG_INFO(command);
        char response[2];
        if (0 != setStandardProcedureAndReturnResponse(fd, command, response, 2) )
        {
            LOG_ERROR("TLE track error");
            return 1;
        }
        if (response[0] == 'E')
        {
            LOG_ERROR("TLE transit not calculated");
            return 2;
        }
        if (response[0] == 'F')
        {
            LOG_ERROR("Slew failed");
            return 3;
        }
        if (response[0] == 'V')
        {
            LOG_INFO("Slewing to start of transit");
            return 0;
        }
        if (response[0] == 'S')
        {
            LOG_INFO("Slewing to transiting satellite");
            return 0;
        }
        if (response[0] == 'Q')
        {
            LOG_ERROR("Transit is already over");
            return 4;
        }
    }
  return 0;
}

int LX200_10MICRON::SetRefractionModelTemperature(double temperature)
{
    char data[16];
    snprintf(data, 16, "#:SRTMP%0+6.1f#", temperature);
    return setStandardProcedure(fd, data);
}

int LX200_10MICRON::SetRefractionModelPressure(double pressure)
{
    char data[16];
    snprintf(data, 16, "#:SRPRS%06.1f#", pressure);
    return setStandardProcedure(fd, data);
}

int LX200_10MICRON::AddSyncPoint(double MRa, double MDec, double MSide, double PRa, double PDec, double SidTime)
{
    char MRa_str[32], MDec_str[32];
    fs_sexa(MRa_str, MRa, 0, 36000);
    fs_sexa(MDec_str, MDec, 0, 3600);

    char MSide_char;
    ((int)MSide == 0) ? MSide_char = 'E' : MSide_char = 'W';

    char PRa_str[32], PDec_str[32];
    fs_sexa(PRa_str, PRa, 0, 36000);
    fs_sexa(PDec_str, PDec, 0, 3600);

    char SidTime_str[32];
    fs_sexa(SidTime_str, SidTime, 0, 36000);

    char command[80];
    snprintf(command, 80, "#:newalpt%s,%s,%c,%s,%s,%s#", MRa_str, MDec_str, MSide_char, PRa_str, PDec_str, SidTime_str);
    LOGF_INFO("AddSyncPoint %s", command);

    char response[6];
    if (0 != setStandardProcedureAndReturnResponse(fd, command, response, 5) || response[0] == 'E')
    {
        LOG_ERROR("AddSyncPoint error");
        return 1;
    }
    response[4] = 0;
    int points;
    int nbytes_read = sscanf(response, "%3d#", &points);
    if (nbytes_read < 0)
    {
        LOGF_ERROR("AddSyncPoint response error %d", nbytes_read);
        return 1;
    }
    LOGF_INFO("AddSyncPoint responded [%4s], there are now %d new alignment points", response, points);
    NewAlignmentPointsN[0].value = points;
    IDSetNumber(&NewAlignmentPointsNP, nullptr);

    return 0;
}

int LX200_10MICRON::AddSyncPointHere(double PRa, double PDec)
{
    double MSide = (toupper(Ginfo.SideOfPier) == 'E') ? 0 : 1;
    return AddSyncPoint(Ginfo.RA_JNOW, Ginfo.DEC_JNOW, MSide, PRa, PDec, Ginfo.SiderealTime);
}

bool LX200_10MICRON::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "REFRACTION_MODEL_TEMPERATURE") == 0)
        {
            IUUpdateNumber(&RefractionModelTemperatureNP, values, names, n);
            if (0 != SetRefractionModelTemperature(RefractionModelTemperatureN[0].value))
            {
                LOG_ERROR("SetRefractionModelTemperature error");
                RefractionModelTemperatureNP.s = IPS_ALERT;
                IDSetNumber(&RefractionModelTemperatureNP, nullptr);
                return false;
            }
            RefractionModelTemperatureNP.s = IPS_OK;
            IDSetNumber(&RefractionModelTemperatureNP, nullptr);
            LOGF_INFO("RefractionModelTemperature set to %0+6.1f degrees C", RefractionModelTemperatureN[0].value);
            return true;
        }
        if (strcmp(name, "REFRACTION_MODEL_PRESSURE") == 0)
        {
            IUUpdateNumber(&RefractionModelPressureNP, values, names, n);
            if (0 != SetRefractionModelPressure(RefractionModelPressureN[0].value))
            {
                LOG_ERROR("SetRefractionModelPressure error");
                RefractionModelPressureNP.s = IPS_ALERT;
                IDSetNumber(&RefractionModelPressureNP, nullptr);
                return false;
            }
            RefractionModelPressureNP.s = IPS_OK;
            IDSetNumber(&RefractionModelPressureNP, nullptr);
            LOGF_INFO("RefractionModelPressure set to %06.1f hPa", RefractionModelPressureN[0].value);
            return true;
        }
        if (strcmp(name, "MODEL_COUNT") == 0)
        {
            IUUpdateNumber(&ModelCountNP, values, names, n);
            ModelCountNP.s = IPS_OK;
            IDSetNumber(&ModelCountNP, nullptr);
            LOGF_INFO("ModelCount %d", ModelCountN[0].value);
            return true;
        }
        if (strcmp(name, "MINIMAL_NEW_ALIGNMENT_POINT_RO") == 0)
        {
            IUUpdateNumber(&MiniNewAlpNP, values, names, n);
            MiniNewAlpRONP.s = IPS_OK;
            IDSetNumber(&MiniNewAlpRONP, nullptr);
            return true;
        }
        if (strcmp(name, "MINIMAL_NEW_ALIGNMENT_POINT") == 0)
        {
            if (AlignmentState != ALIGN_START)
            {
                LOG_ERROR("Cannot add alignment points yet, need to start a new alignment first");
                return false;
            }

            IUUpdateNumber(&MiniNewAlpNP, values, names, n);
            if (0 != AddSyncPointHere(MiniNewAlpN[MALP_PRA].value, MiniNewAlpN[MALP_PDEC].value))
            {
                LOG_ERROR("AddSyncPointHere error");
                MiniNewAlpNP.s = IPS_ALERT;
                IDSetNumber(&MiniNewAlpNP, nullptr);
                return false;
            }
            MiniNewAlpNP.s = IPS_OK;
            IDSetNumber(&MiniNewAlpNP, nullptr);
            return true;
        }
        if (strcmp(name, "NEW_ALIGNMENT_POINT") == 0)
        {
            if (AlignmentState != ALIGN_START)
            {
                LOG_ERROR("Cannot add alignment points yet, need to start a new alignment first");
                return false;
            }

            IUUpdateNumber(&NewAlpNP, values, names, n);
            if (0 != AddSyncPoint(NewAlpN[ALP_MRA].value, NewAlpN[ALP_MDEC].value, NewAlpN[ALP_MSIDE].value,
                    NewAlpN[ALP_PRA].value, NewAlpN[ALP_PDEC].value, NewAlpN[ALP_SIDTIME].value))
            {
                LOG_ERROR("AddSyncPoint error");
                NewAlpNP.s = IPS_ALERT;
                IDSetNumber(&NewAlpNP, nullptr);
                return false;
            }
            NewAlpNP.s = IPS_OK;
            IDSetNumber(&NewAlpNP, nullptr);
            return true;
        }
        if (strcmp(name, "NEW_ALIGNMENT_POINTS") == 0)
        {
            IUUpdateNumber(&NewAlignmentPointsNP, values, names, n);
            NewAlignmentPointsNP.s = IPS_OK;
            IDSetNumber(&NewAlignmentPointsNP, nullptr);
            LOGF_INFO("New unnamed Model now has %d alignment points", NewAlignmentPointsN[0].value);
            return true;
        }
        if (strcmp(name, "TRAJECTORY_TIME") == 0)
          {
            IUUpdateNumber(&CalculateSatTrajectoryForTimeNP, values, names, n);
            if (0 != CalculateTrajectory(CalculateSatTrajectoryForTimeN[SAT_YYYY].value,
                                         CalculateSatTrajectoryForTimeN[SAT_MM].value,
                                         CalculateSatTrajectoryForTimeN[SAT_DD].value,
                                         CalculateSatTrajectoryForTimeN[SAT_HH24].value,
                                         CalculateSatTrajectoryForTimeN[SAT_MM60].value,
                                         CalculateSatTrajectoryForTimeN[SAT_MM1440_NEXT].value,
                                         date_pass)
                )
              {
                CalculateSatTrajectoryForTimeNP.s = IPS_ALERT;
                IDSetNumber(&CalculateSatTrajectoryForTimeNP, nullptr);
                return false;
              }
            CalculateSatTrajectoryForTimeNP.s = IPS_OK;
            IDSetNumber(&CalculateSatTrajectoryForTimeNP, nullptr);
            return true;
        }
        if (strcmp(name, "TLE_NUMBER") == 0)
        {
            LOG_INFO("I am trying to set from Database");
            IUUpdateNumber(&TLEfromDatabaseNP, values, names, n);
            if ( 0 != SetTLEfromDatabase(TLEfromDatabaseN[0].value) )
            {
                TLEfromDatabaseNP.s = IPS_ALERT;
                IDSetNumber(&TLEfromDatabaseNP, nullptr);
                return false;
            }
            TLEfromDatabaseNP.s = IPS_OK;
            TLEtoUploadTP.s = IPS_IDLE;
            IDSetText(&TLEtoUploadTP, nullptr);
            IDSetNumber(&TLEfromDatabaseNP, nullptr);
            LOGF_INFO("Selected TLE nr %.0f from database", TLEfromDatabaseN[0].value);
            return true;
        }
    }

    // Let INDI::LX200Generic handle any other number properties
    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200_10MICRON::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(AlignmentSP.name, name) == 0)
        {
            IUUpdateSwitch(&AlignmentSP, states, names, n);
            int index    = IUFindOnSwitchIndex(&AlignmentSP);

            switch (index)
            {
                case ALIGN_IDLE:
                    AlignmentState = ALIGN_IDLE;
                    LOG_INFO("Alignment state is IDLE");
                    break;

                case ALIGN_START:
                    if (0 != setStandardProcedureAndExpect(fd, "#:newalig#", "V"))
                    {
                        LOG_ERROR("New alignment start error");
                        AlignmentSP.s = IPS_ALERT;
                        IDSetSwitch(&AlignmentSP, nullptr);
                        return false;
                    }
                    LOG_INFO("New Alignment started");
                    AlignmentState = ALIGN_START;
                    break;

                case ALIGN_END:
                    if (0 != setStandardProcedureAndExpect(fd, "#:endalig#", "V"))
                    {
                        LOG_ERROR("New alignment end error");
                        AlignmentSP.s = IPS_ALERT;
                        IDSetSwitch(&AlignmentSP, nullptr);
                        return false;
                    }
                    LOG_INFO("New Alignment ended");
                    AlignmentState = ALIGN_END;
                    break;

                case ALIGN_DELETE_CURRENT:
                    if (0 != setStandardProcedureAndExpect(fd, "#:delalig#", "#"))
                    {
                        LOG_ERROR("Delete current alignment error");
                        AlignmentSP.s = IPS_ALERT;
                        IDSetSwitch(&AlignmentSP, nullptr);
                        return false;
                    }
                    LOG_INFO("Current Alignment deleted");
                    AlignmentState = ALIGN_DELETE_CURRENT;
                    break;

                default:
                    AlignmentSP.s = IPS_ALERT;
                    IDSetSwitch(&AlignmentSP, "Unknown alignment index %d", index);
                    AlignmentState = ALIGN_IDLE;
                    return false;
            }

            AlignmentSP.s = IPS_OK;
            IDSetSwitch(&AlignmentSP, nullptr);
            return true;
        }
        if (strcmp(TrackSatSP.name, name)==0)
          {
            IUUpdateSwitch(&TrackSatSP, states, names, n);
            int index    = IUFindOnSwitchIndex(&TrackSatSP);

            switch (index)
              {
              case SAT_TRACK:
                if ( 0!=TrackSat() )
                {
                    TrackSatSP.s = IPS_ALERT;
                    IDSetSwitch(&TrackSatSP, nullptr);
                    LOG_ERROR("Tracking failed");
                    return false;
                }
                TrackSatSP.s = IPS_OK;
                IDSetSwitch(&TrackSatSP, nullptr);
                LOG_INFO("Tracking satellite");
                return true;
              case SAT_HALT:
                if ( !Abort() )
                {
                    TrackSatSP.s = IPS_ALERT;
                    IDSetSwitch(&TrackSatSP, nullptr);
                    LOG_ERROR("Halt failed");
                    return false;
                }
                TrackSatSP.s = IPS_OK;
                IDSetSwitch(&TrackSatSP, nullptr);
                LOG_INFO("Halt tracking");
                return true;
              default:
                TrackSatSP.s = IPS_ALERT;
                IDSetSwitch(&TrackSatSP, "Unknown tracking modus %d", index);
                return false;
              }
          }
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200_10MICRON::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "NEW_MODEL_NAME") == 0)
        {
            IUUpdateText(&NewModelNameTP, texts, names, n);
            NewModelNameTP.s = IPS_OK;
            IDSetText(&NewModelNameTP, nullptr);
            LOGF_INFO("Model saved with name %s", NewModelNameT[0].text);
            return true;
        }
        if (strcmp(name, "TLE_TEXT") == 0)
        {
          IUUpdateText(&TLEtoUploadTP, texts, names, n);
          if (0 == SetTLEtoFollow(TLEtoUploadT[0].text))
            {
              TLEtoUploadTP.s = IPS_OK;
              TLEfromDatabaseNP.s = IPS_IDLE;
              IDSetText(&TLEtoUploadTP, nullptr);
              IDSetNumber(&TLEfromDatabaseNP, nullptr);
              LOGF_INFO("Selected TLE %s", TLEtoUploadT[0].text);
              return true;
            }
          else
            {
              TLEtoUploadTP.s = IPS_ALERT;
              TLEfromDatabaseNP.s = IPS_IDLE;
              IDSetText(&TLEtoUploadTP, nullptr);
              IDSetNumber(&TLEfromDatabaseNP, nullptr);
              LOG_ERROR("TLE was not correctly uploaded");
              return false;
            }
        }
    }
    return LX200Generic::ISNewText(dev, name, texts, names, n);
}
