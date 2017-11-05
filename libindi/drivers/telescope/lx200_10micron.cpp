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
#include <termios.h>
#include <math.h>

#define PRODUCT_TAB   "Product"
#define ALIGNMENT_TAB "Alignment"
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
        DEBUG(INDI::Logger::DBG_SESSION, "Simulate Connect.");
        return true;
    }

    // Set Ultra Precision Mode #:U2# , replies like 15:58:19.49 instead of 15:21.2
    DEBUG(INDI::Logger::DBG_SESSION, "Setting Ultra Precision Mode.");
    if (setCommandInt(fd, 2, "#:U") < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to set Ultra Precision Mode.");
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
    }
    bool result = LX200Generic::updateProperties();
    return result;
}

// INDI::Telescope calls ReadScopeStatus() every updatePeriodMS to check the link to the telescope and update its state and position.
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
            DEBUGF(INDI::Logger::DBG_SESSION, "Gstat changed from %d to %d", OldGstat, Ginfo.Gstat);
        }
        else
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Gstat initialized at %d", Ginfo.Gstat);
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
    setPierSide(toupper(Ginfo.SideOfPier) ? INDI::Telescope::PIER_EAST : INDI::Telescope::PIER_WEST);

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
            DEBUG(INDI::Logger::DBG_WARNING, "Failed to get tracking frequency from device.");
        }
        else
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Tracking frequency is %.1f Hz", TrackFreqN[0].value);
            IDSetNumber(&TrackingFreqNP, nullptr);
        }

        char RefractionModelTemperature[80];
        getCommandString(PortFD, RefractionModelTemperature, "#:GRTMP#");
        float rmtemp;
        sscanf(RefractionModelTemperature, "%f#", &rmtemp);
        RefractionModelTemperatureN[0].value = (double) rmtemp;
        DEBUGF(INDI::Logger::DBG_SESSION, "RefractionModelTemperature is %0+6.1f degrees C", RefractionModelTemperatureN[0].value);
        IDSetNumber(&RefractionModelTemperatureNP, nullptr);

        char RefractionModelPressure[80];
        getCommandString(PortFD, RefractionModelPressure, "#:GRPRS#");
        float rmpres;
        sscanf(RefractionModelPressure, "%f#", &rmpres);
        RefractionModelPressureN[0].value = (double) rmpres;
        DEBUGF(INDI::Logger::DBG_SESSION, "RefractionModelPressure is %06.1f hPa", RefractionModelPressureN[0].value);
        IDSetNumber(&RefractionModelPressureNP, nullptr);

        int ModelCount;
        getCommandInt(PortFD, &ModelCount, "#:modelcnt#");
        ModelCountN[0].value = (double) ModelCount;
        DEBUGF(INDI::Logger::DBG_SESSION, "%d Alignment Models", (int) ModelCountN[0].value);
        IDSetNumber(&ModelCountNP, nullptr);

        int AlignmentPoints;
        getCommandInt(PortFD, &AlignmentPoints, "#:getalst#");
        AlignmentPointsN[0].value = (double) AlignmentPoints;
        DEBUGF(INDI::Logger::DBG_SESSION, "%d Alignment Stars in active model", (int) AlignmentPointsN[0].value);
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

    DEBUGF(INDI::Logger::DBG_SESSION, "Product:%s Control box:%s Firmware:%s of %s", ProductName, ControlBox, FirmwareVersion, FirmwareDate);

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
    DEBUG(INDI::Logger::DBG_SESSION, "Parking.");
    if (setStandardProcedureWithoutRead(fd, "#:KA#") < 0)
    {
        return false;
    }
    return true;
}

bool LX200_10MICRON::UnPark()
{
    DEBUG(INDI::Logger::DBG_SESSION, "Unparking.");
    if (setStandardProcedureWithoutRead(fd, "#:PO#") < 0)
    {
        return false;
    }
    SetParked(false);
    return true;
}

bool LX200_10MICRON::SyncConfigBehaviour(bool cmcfg)
{
    DEBUG(INDI::Logger::DBG_SESSION, "SyncConfig.");
    if (setCommandInt(fd, cmcfg, "#:CMCFG") < 0)
    {
        return false;
    }
    return true;
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
    DEBUGF(INDI::Logger::DBG_SESSION, "AddSyncPoint %s", command);

    char response[6];
    if (0 != setStandardProcedureAndReturnResponse(fd, command, response, 5) || response[0] == 'E')
    {
        DEBUG(INDI::Logger::DBG_ERROR, "AddSyncPoint error");
        return 1;
    }
    response[4] = 0;
    int points;
    int nbytes_read = sscanf(response, "%3d#", &points);
    if (nbytes_read < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "AddSyncPoint response error %d", nbytes_read);
        return 1;
    }
    DEBUGF(INDI::Logger::DBG_SESSION, "AddSyncPoint responded [%4s], there are now %d new alignment points", response, points);
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
                DEBUG(INDI::Logger::DBG_ERROR, "SetRefractionModelTemperature error");
                RefractionModelTemperatureNP.s = IPS_ALERT;
                IDSetNumber(&RefractionModelTemperatureNP, nullptr);
                return false;
            }
            RefractionModelTemperatureNP.s = IPS_OK;
            IDSetNumber(&RefractionModelTemperatureNP, nullptr);
            DEBUGF(INDI::Logger::DBG_SESSION, "RefractionModelTemperature set to %0+6.1f degrees C", RefractionModelTemperatureN[0].value);
            return true;
        }
        if (strcmp(name, "REFRACTION_MODEL_PRESSURE") == 0)
        {
            IUUpdateNumber(&RefractionModelPressureNP, values, names, n);
            if (0 != SetRefractionModelPressure(RefractionModelPressureN[0].value))
            {
                DEBUG(INDI::Logger::DBG_ERROR, "SetRefractionModelPressure error");
                RefractionModelPressureNP.s = IPS_ALERT;
                IDSetNumber(&RefractionModelPressureNP, nullptr);
                return false;
            }
            RefractionModelPressureNP.s = IPS_OK;
            IDSetNumber(&RefractionModelPressureNP, nullptr);
            DEBUGF(INDI::Logger::DBG_SESSION, "RefractionModelPressure set to %06.1f hPa", RefractionModelPressureN[0].value);
            return true;
        }
        if (strcmp(name, "MODEL_COUNT") == 0)
        {
            IUUpdateNumber(&ModelCountNP, values, names, n);
            ModelCountNP.s = IPS_OK;
            IDSetNumber(&ModelCountNP, nullptr);
            DEBUGF(INDI::Logger::DBG_SESSION, "ModelCount %d", ModelCountN[0].value);
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
                DEBUG(INDI::Logger::DBG_ERROR, "Cannot add alignment points yet, need to start a new alignment first");
                return false;
            }

            IUUpdateNumber(&MiniNewAlpNP, values, names, n);
            if (0 != AddSyncPointHere(MiniNewAlpN[MALP_PRA].value, MiniNewAlpN[MALP_PDEC].value))
            {
                DEBUG(INDI::Logger::DBG_ERROR, "AddSyncPointHere error");
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
                DEBUG(INDI::Logger::DBG_ERROR, "Cannot add alignment points yet, need to start a new alignment first");
                return false;
            }

            IUUpdateNumber(&NewAlpNP, values, names, n);
            if (0 != AddSyncPoint(NewAlpN[ALP_MRA].value, NewAlpN[ALP_MDEC].value, NewAlpN[ALP_MSIDE].value,
                    NewAlpN[ALP_PRA].value, NewAlpN[ALP_PDEC].value, NewAlpN[ALP_SIDTIME].value))
            {
                DEBUG(INDI::Logger::DBG_ERROR, "AddSyncPoint error");
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
            DEBUGF(INDI::Logger::DBG_SESSION, "New unnamed Model now has %d alignment points", NewAlignmentPointsN[0].value);
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
                    DEBUG(INDI::Logger::DBG_SESSION, "Alignment state is IDLE");
                    break;

                case ALIGN_START:
                    if (0 != setStandardProcedureAndExpect(fd, "#:newalig#", "V"))
                    {
                        DEBUG(INDI::Logger::DBG_ERROR, "New alignment start error");
                        AlignmentSP.s = IPS_ALERT;
                        IDSetSwitch(&AlignmentSP, nullptr);
                        return false;
                    }
                    DEBUG(INDI::Logger::DBG_SESSION, "New Alignment started");
                    AlignmentState = ALIGN_START;
                    break;

                case ALIGN_END:
                    if (0 != setStandardProcedureAndExpect(fd, "#:endalig#", "V"))
                    {
                        DEBUG(INDI::Logger::DBG_ERROR, "New alignment end error");
                        AlignmentSP.s = IPS_ALERT;
                        IDSetSwitch(&AlignmentSP, nullptr);
                        return false;
                    }
                    DEBUG(INDI::Logger::DBG_SESSION, "New Alignment ended");
                    AlignmentState = ALIGN_END;
                    break;

                case ALIGN_DELETE_CURRENT:
                    if (0 != setStandardProcedureAndExpect(fd, "#:delalig#", "#"))
                    {
                        DEBUG(INDI::Logger::DBG_ERROR, "Delete current alignment error");
                        AlignmentSP.s = IPS_ALERT;
                        IDSetSwitch(&AlignmentSP, nullptr);
                        return false;
                    }
                    DEBUG(INDI::Logger::DBG_SESSION, "Current Alignment deleted");
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
            DEBUGF(INDI::Logger::DBG_SESSION, "Model saved with name %s", NewModelNameT[0].text);
            return true;
        }
    }
    return LX200Generic::ISNewText(dev, name, texts, names, n);
}
