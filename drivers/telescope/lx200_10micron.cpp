/*
    10micron INDI driver
    GM1000HPS GM2000QCI GM2000HPS GM3000HPS GM4000QCI GM4000HPS AZ2000
    Mount Command Protocol 2.14.11

    Copyright (C) 2017-2025 Hans Lambermont

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

/** \file lx200_10micron.cpp
    \brief Implementation of the driver for the 10micron mounts.

    \example lx200_10micron.cpp
    The 10micron mount implementation contains an example for TLE-based satellite tracking.
*/

#include "lx200_10micron.h"
#include "indicom.h"
#include "lx200driver.h"

#include <cstring>
#include <strings.h>
#include <termios.h>
#include <math.h>
#include <libnova/libnova.h>

#define PRODUCT_TAB   "Product"
#define ALIGNMENT_TAB "Alignment"
#define LX200_TIMEOUT 5 /* FD timeout in seconds */

// INDI Number and Text names
#define REFRACTION_MODEL_TEMPERATURE "REFRACTION_MODEL_TEMPERATURE"
#define REFRACTION_MODEL_PRESSURE "REFRACTION_MODEL_PRESSURE"
#define MODEL_COUNT "MODEL_COUNT"
#define ALIGNMENT_POINTS "ALIGNMENT_POINTS"
#define ALIGNMENT_STATE "Alignment"
#define MINIMAL_NEW_ALIGNMENT_POINT_RO "MINIMAL_NEW_ALIGNMENT_POINT_RO"
#define MINIMAL_NEW_ALIGNMENT_POINT "MINIMAL_NEW_ALIGNMENT_POINT"
#define NEW_ALIGNMENT_POINT "NEW_ALIGNMENT_POINT"
#define NEW_ALIGNMENT_POINTS "NEW_ALIGNMENT_POINTS"
#define NEW_MODEL_NAME "NEW_MODEL_NAME"
#define PRODUCT_INFO "PRODUCT_INFO"
#define TLE_TEXT "TLE_TEXT"
#define TLE_NUMBER "TLE_NUMBER"
#define TRAJECTORY_TIME "TRAJECTORY_TIME"
#define SAT_TRACKING_STAT "SAT_TRACKING_STAT"
#define UNATTENDED_FLIP "UNATTENDED_FLIP"

LX200_10MICRON::LX200_10MICRON() : LX200Generic()
{
    setLX200Capability( LX200_HAS_TRACKING_FREQ | LX200_HAS_PULSE_GUIDING );

    SetTelescopeCapability(
        TELESCOPE_CAN_GOTO |
        TELESCOPE_CAN_SYNC |
        TELESCOPE_CAN_PARK |
        TELESCOPE_CAN_FLIP |
        TELESCOPE_CAN_ABORT |
        TELESCOPE_HAS_TIME |
        TELESCOPE_HAS_LOCATION |
        TELESCOPE_HAS_PIER_SIDE |
        TELESCOPE_HAS_TRACK_MODE |
        TELESCOPE_CAN_CONTROL_TRACK |
        TELESCOPE_HAS_TRACK_RATE |
        TELESCOPE_CAN_TRACK_SATELLITE,
        4
    );

    setVersion(1, 3); // don't forget to update drivers.xml
}

// Called by INDI::DefaultDevice::ISGetProperties
// Note that getDriverName calls ::getDefaultName which returns LX200 Generic
const char *LX200_10MICRON::getDefaultName()
{
    return "10micron";
}

// Called by INDI::Telescope::callHandshake, either TCP Connect or Serial Port Connect
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
    // #:U2#
    // Set ultra precision mode. In ultra precision mode, extra decimal digits are returned for
    // some commands, and there is no more difference between different emulation modes.
    // Returns: nothing
    // Available from version 2.10.
    if (setCommandInt(fd, 2, "#:U") < 0)
    {
        LOG_ERROR("Failed to set Ultra Precision Mode.");
        return false;
    }

    return true;
}

// Called only once by DefaultDevice::ISGetProperties
// Initialize basic properties that are required all the time
bool LX200_10MICRON::initProperties()
{
    const bool result = LX200Generic::initProperties();
    if (result)
    {
        // TODO initialize properties additional to INDI::Telescope
        IUFillSwitch(&UnattendedFlipS[UNATTENDED_FLIP_DISABLED], "Disabled", "Disabled", ISS_ON);
        IUFillSwitch(&UnattendedFlipS[UNATTENDED_FLIP_ENABLED], "Enabled", "Enabled", ISS_OFF);
        IUFillSwitchVector(&UnattendedFlipSP, UnattendedFlipS, UNATTENDED_FLIP_COUNT, getDeviceName(),
                           UNATTENDED_FLIP, "Unattended Flip", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

        IUFillNumber(&RefractionModelTemperatureN[0], "TEMPERATURE", "Celsius", "%+6.1f", -999.9, 999.9, 0, 0.);
        IUFillNumberVector(&RefractionModelTemperatureNP, RefractionModelTemperatureN, 1, getDeviceName(),
                           REFRACTION_MODEL_TEMPERATURE, "Temperature", ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

        IUFillNumber(&RefractionModelPressureN[0], "PRESSURE", "hPa", "%6.1f", 0.0, 9999.9, 0, 0.);
        IUFillNumberVector(&RefractionModelPressureNP, RefractionModelPressureN, 1, getDeviceName(),
                           REFRACTION_MODEL_PRESSURE, "Pressure", ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

        IUFillNumber(&ModelCountN[0], "COUNT", "#", "%.0f", 0, 999, 0, 0);
        IUFillNumberVector(&ModelCountNP, ModelCountN, 1, getDeviceName(),
                           MODEL_COUNT, "Models", ALIGNMENT_TAB, IP_RO, 60, IPS_IDLE);

        IUFillNumber(&AlignmentPointsN[0], "COUNT", "#", "%.0f", 0, 100, 0, 0);
        IUFillNumberVector(&AlignmentPointsNP, AlignmentPointsN, 1, getDeviceName(),
                           ALIGNMENT_POINTS, "Points", ALIGNMENT_TAB, IP_RO, 60, IPS_IDLE);

        IUFillSwitch(&AlignmentStateS[ALIGN_IDLE], "Idle", "Idle", ISS_ON);
        IUFillSwitch(&AlignmentStateS[ALIGN_START], "Start", "Start new model", ISS_OFF);
        IUFillSwitch(&AlignmentStateS[ALIGN_END], "End", "End new model", ISS_OFF);
        IUFillSwitch(&AlignmentStateS[ALIGN_DELETE_CURRENT], "Del", "Delete current model", ISS_OFF);
        IUFillSwitchVector(&AlignmentStateSP, AlignmentStateS, ALIGN_COUNT, getDeviceName(),
                           ALIGNMENT_STATE, "Alignment", ALIGNMENT_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

        IUFillNumber(&MiniNewAlpRON[MALPRO_MRA], "MRA", "Mount RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
        IUFillNumber(&MiniNewAlpRON[MALPRO_MDEC], "MDEC", "Mount DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
        IUFillNumber(&MiniNewAlpRON[MALPRO_MSIDE], "MSIDE", "Pier Side (0=E 1=W)", "%.0f", 0, 1, 0, 0);
        IUFillNumber(&MiniNewAlpRON[MALPRO_SIDTIME], "SIDTIME", "Sidereal Time (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
        IUFillNumberVector(&MiniNewAlpRONP, MiniNewAlpRON, MALPRO_COUNT, getDeviceName(),
                           MINIMAL_NEW_ALIGNMENT_POINT_RO, "Actual", ALIGNMENT_TAB, IP_RO, 60, IPS_IDLE);

        IUFillNumber(&MiniNewAlpN[MALP_PRA], "PRA", "Solved RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
        IUFillNumber(&MiniNewAlpN[MALP_PDEC], "PDEC", "Solved DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
        IUFillNumberVector(&MiniNewAlpNP, MiniNewAlpN, MALP_COUNT, getDeviceName(),
                           MINIMAL_NEW_ALIGNMENT_POINT, "New Point", ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

        IUFillNumber(&NewAlpN[ALP_MRA], "MRA", "Mount RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
        IUFillNumber(&NewAlpN[ALP_MDEC], "MDEC", "Mount DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
        IUFillNumber(&NewAlpN[ALP_MSIDE], "MSIDE", "Pier Side (0=E 1=W)", "%.0f", 0, 1, 0, 0);
        IUFillNumber(&NewAlpN[ALP_SIDTIME], "SIDTIME", "Sidereal Time (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
        IUFillNumber(&NewAlpN[ALP_PRA], "PRA", "Solved RA (hh:mm:ss)", "%010.6m", 0, 24, 0, 0);
        IUFillNumber(&NewAlpN[ALP_PDEC], "PDEC", "Solved DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
        IUFillNumberVector(&NewAlpNP, NewAlpN, ALP_COUNT, getDeviceName(),
                           NEW_ALIGNMENT_POINT, "New Point", ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

        IUFillNumber(&NewAlignmentPointsN[0], "COUNT", "#", "%.0f", 0, 100, 1, 0);
        IUFillNumberVector(&NewAlignmentPointsNP, NewAlignmentPointsN, 1, getDeviceName(),
                           NEW_ALIGNMENT_POINTS, "New Points", ALIGNMENT_TAB, IP_RO, 60, IPS_IDLE);

        IUFillText(&NewModelNameT[0], "NAME", "Model Name", "newmodel");
        IUFillTextVector(&NewModelNameTP, NewModelNameT, 1, getDeviceName(),
                         NEW_MODEL_NAME, "New Name", ALIGNMENT_TAB, IP_RW, 60, IPS_IDLE);

        IUFillNumber(&TLEfromDatabaseN[0], "NUMBER", "#", "%.0f", 1, 999, 1, 1);
        IUFillNumberVector(&TLEfromDatabaseNP, TLEfromDatabaseN, 1, getDeviceName(),
                           "TLE_NUMBER", "Database TLE ", SATELLITE_TAB, IP_RW, 60, IPS_IDLE);
    }
    return result;
}

bool LX200_10MICRON::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &UnattendedFlipSP);
    return true;
}

// Called by INDI::Telescope when connected state changes to add/remove properties
bool LX200_10MICRON::updateProperties()
{
    bool result = LX200Generic::updateProperties();

    if (isConnected())
    {
        defineProperty(&UnattendedFlipSP);
        // getMountInfo defines ProductTP
        defineProperty(&RefractionModelTemperatureNP);
        defineProperty(&RefractionModelPressureNP);
        defineProperty(&ModelCountNP);
        defineProperty(&AlignmentPointsNP);
        defineProperty(&AlignmentStateSP);
        defineProperty(&MiniNewAlpRONP);
        defineProperty(&MiniNewAlpNP);
        defineProperty(&NewAlpNP);
        defineProperty(&NewAlignmentPointsNP);
        defineProperty(&NewModelNameTP);
        defineProperty(&TLEfromDatabaseNP);

        // read UnAttendedFlip setting from config and apply if available
        int readit = 0;
        for (int i = 0; readit == 0 && i < UnattendedFlipSP.nsp; i++)
        {
            readit = IUGetConfigSwitch(getDeviceName(), UnattendedFlipSP.name, UnattendedFlipS[i].name, &(UnattendedFlipS[i].s));
        }
        if (readit == 0)
        {
            if (UnattendedFlipS[UnattendedFlip].s == ISS_ON)
            {
                LOGF_INFO("Unattended Flip from config and mount are %s",
                          (UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s == ISS_ON) ? "enabled" : "disabled");
            }
            else
            {
                LOGF_INFO("Read Unattended Flip %s from config while mount has %s, updating mount",
                          (UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s == ISS_ON) ? "enabled" : "disabled",
                          (UnattendedFlip == UNATTENDED_FLIP_ENABLED) ? "enabled" : "disabled");
                setUnattendedFlipSetting(UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s == ISS_ON);
            }
        }
        else
        {
            LOG_INFO("Did not find an Unattended Flip setting in the config file. Specify desired behaviour in Motion Control tab and save config in Options tab.");
        }
    }
    else
    {
        deleteProperty(UnattendedFlipSP.name);
        deleteProperty(ProductTP.name);
        deleteProperty(RefractionModelTemperatureNP.name);
        deleteProperty(RefractionModelPressureNP.name);
        deleteProperty(ModelCountNP.name);
        deleteProperty(AlignmentPointsNP.name);
        deleteProperty(AlignmentStateSP.name);
        deleteProperty(MiniNewAlpRONP.name);
        deleteProperty(MiniNewAlpNP.name);
        deleteProperty(NewAlpNP.name);
        deleteProperty(NewAlignmentPointsNP.name);
        deleteProperty(NewModelNameTP.name);
        deleteProperty(TLEfromDatabaseNP.name);
    }

    return result;
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
        checkLX200EquatorialFormat(fd);
        timeFormat = LX200_24;

        if (getTrackFreq(PortFD, &TrackFreqN[0].value) < 0)
        {
            LOG_WARN("Failed to get tracking frequency from device.");
        }
        else
        {
            LOGF_INFO("Tracking frequency is %.1f Hz", TrackFreqN[0].value);
            IDSetNumber(&TrackFreqNP, nullptr);
        }

        char RefractionModelTemperature[80];
        getCommandString(PortFD, RefractionModelTemperature, "#:GRTMP#");
        float rmtemp;
        sscanf(RefractionModelTemperature, "%f#", &rmtemp);
        RefractionModelTemperatureN[0].value = rmtemp;
        LOGF_INFO("RefractionModelTemperature is %0+6.1f degrees C", RefractionModelTemperatureN[0].value);
        IDSetNumber(&RefractionModelTemperatureNP, nullptr);

        char RefractionModelPressure[80];
        getCommandString(PortFD, RefractionModelPressure, "#:GRPRS#");
        float rmpres;
        sscanf(RefractionModelPressure, "%f#", &rmpres);
        RefractionModelPressureN[0].value = rmpres;
        LOGF_INFO("RefractionModelPressure is %06.1f hPa", RefractionModelPressureN[0].value);
        IDSetNumber(&RefractionModelPressureNP, nullptr);

        int ModelCount;
        getCommandInt(PortFD, &ModelCount, "#:modelcnt#");
        ModelCountN[0].value = (double) ModelCount;
        LOGF_INFO("%d Alignment Models", static_cast<int>(ModelCountN[0].value));
        IDSetNumber(&ModelCountNP, nullptr);

        int AlignmentPoints;
        getCommandInt(PortFD, &AlignmentPoints, "#:getalst#");
        AlignmentPointsN[0].value = AlignmentPoints;
        LOGF_INFO("%d Alignment Stars in active model", static_cast<int>(AlignmentPointsN[0].value));
        IDSetNumber(&AlignmentPointsNP, nullptr);

        if (false == getUnattendedFlipSetting())
        {
            UnattendedFlipS[UNATTENDED_FLIP_DISABLED].s = ISS_ON;
            UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s = ISS_OFF;
            LOG_INFO("Unattended Flip is disabled.");
        }
        else
        {
            UnattendedFlipS[UNATTENDED_FLIP_DISABLED].s = ISS_OFF;
            UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s = ISS_ON;
            LOG_INFO("Unattended Flip is enabled.");
        }
        UnattendedFlipSP.s = IPS_OK;
        IDSetSwitch(&UnattendedFlipSP, nullptr);
    }

    if (sendLocationOnStartup)
    {
        LOG_INFO("sendLocationOnStartup is enabled, call sendScopeLocation.");
        sendScopeLocation();
    }
    else
    {
        LOG_INFO("sendLocationOnStartup is disabled, do not call sendScopeLocation.");
    }
    if (sendTimeOnStartup)
    {
        LOG_INFO("sendTimeOnStartup is enabled, call sendScopeTime.");
        sendScopeTime();
    }
    else
    {
        LOG_INFO("sendTimeOnStartup is disabled, do not call sendScopeTime.");
    }
}

// Called by our getBasicData
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
    IUFillTextVector(&ProductTP, ProductT, PRODUCT_COUNT, getDeviceName(),
                     PRODUCT_INFO, "Product", PRODUCT_TAB, IP_RO, 60, IPS_IDLE);

    defineProperty(&ProductTP);

    return true;
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
    // #:Ginfo#
    // Get multiple information. Returns a string where multiple data are encoded, separated
    // by commas ',', and terminated by '#'. Data are recognized by their position in the string.
    // The data are the following:
    // Position Datum
    // 1        The telescope right ascension in hours and decimals (from 000.00000 to 23.99999),
    //          true equinox and equator of date of observation (i.e. Jnow).
    // 2        The telescope declination in degrees and decimals (from –90.0000 to +90.0000),
    //          true equinox and equator of date of observation (i.e. Jnow).
    // 3        A flag indicating the side of the pier on which the telescope is currently positioned
    //          ("E" or "W").
    // 4        The telescope azimuth in degrees and decimals (from 000.0000 to 359.9999).
    // 5        The telescope altitude in degrees and decimals (from -90.0000 to +90.0000).
    // 6        The julian date (JJJJJJJ.JJJJJJJJ), UTC, with leap second flag (see command
    //          :GJD2# for the description of this datum).
    // 7        A number encoding the status of the mount as in the :Gstat command.
    // 8        A number returning the slew status (0 if :D# would return no slew, 1 otherwise).
    // The string is terminated by '#'. Other parameters may be added in future at the end of
    // the string: do not assume that the number of parameters will stay the same.
    // Available from version 2.14.9 (previous versions may have this command but it was
    // experimental and possibly with a different format).
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

bool LX200_10MICRON::Park()
{
    // #:KA#
    // Slew to park position
    // Returns: nothing
    LOG_INFO("Parking.");
    if (setStandardProcedureWithoutRead(fd, "#:KA#") < 0)
    {
        ParkSP.setState(IPS_ALERT);
        LOG_ERROR("Park command failed.");
        ParkSP.apply();
        return false;
    }

    ParkSP.setState(IPS_BUSY);
    TrackState = SCOPE_PARKING;
    ParkSP.apply();
    // postpone SetParked(true) for ReadScopeStatus so that we know it is actually correct
    return true;
}

bool LX200_10MICRON::UnPark()
{
    // #:PO#
    // Unpark
    // Returns:nothing
    LOG_INFO("Unparking.");
    if (setStandardProcedureWithoutRead(fd, "#:PO#") < 0)
    {
        ParkSP.setState(IPS_ALERT);
        LOG_ERROR("Unpark command failed.");
        ParkSP.apply();
        return false;
    }

    ParkSP.setState(IPS_OK);
    TrackState = SCOPE_IDLE;
    SetParked(false);
    ParkSP.apply();
    return true;
}

bool LX200_10MICRON::SetTrackEnabled(bool enabled)
{
    // :AL#
    // Stops tracking.
    // Returns: nothing
    // :AP#
    // Starts tracking.
    // Returns: nothing
    if (enabled)
    {
        LOG_INFO("Start tracking.");
        if (setStandardProcedureWithoutRead(fd, "#:AP#") < 0)
        {
            LOG_ERROR("Start tracking command failed");
            return false;
        }
    }
    else
    {
        LOG_INFO("Stop tracking.");
        if (setStandardProcedureWithoutRead(fd, "#:AL#") < 0)
        {
            LOG_ERROR("Stop tracking command failed");
            return false;
        }
    }
    return true;
}

bool LX200_10MICRON::getUnattendedFlipSetting()
{
    // #:Guaf#
    // Returns the unattended flip setting.
    // Returns:
    // 0 disabled
    // 1 enabled
    // Available from version 2.11.
    // Note: unattended flip didn't work properly in firmware versions up to 2.13.8 included.
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char guaf[80];
    getCommandString(PortFD, guaf, "#:Guaf#");
    if ('1' == guaf[0])
    {
        UnattendedFlip = UNATTENDED_FLIP_ENABLED;
    }
    else
    {
        UnattendedFlip = UNATTENDED_FLIP_DISABLED;
    }
    return '1' == guaf[0];
}

bool LX200_10MICRON::setUnattendedFlipSetting(bool setting)
{
    // #:SuafN#
    // Enables or disables the unattended flip. Use N=1 to enable, N=0 to disable. This is set always to 0 after power up.
    // Returns: nothing
    // Available from version 2.11.
    // unattended flip didn't work properly in firmware versions up to 2.13.8 included.
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char data[64];
    snprintf(data, sizeof(data), "#:Suaf%d#", (setting == false) ? 0 : 1);
    if (0 == setStandardProcedureWithoutRead(fd, data))
    {
        if (setting == false)
        {
            UnattendedFlip = UNATTENDED_FLIP_DISABLED;
        }
        else
        {
            UnattendedFlip = UNATTENDED_FLIP_ENABLED;
        }
        return true;
    }
    return false;
}

bool LX200_10MICRON::Flip(double ra, double dec)
{
    INDI_UNUSED(ra);
    INDI_UNUSED(dec);
    return flip();
}

bool LX200_10MICRON::flip()
{
    // #:FLIP#
    // This command acts in different ways on the AZ2000 and german equatorial (GM1000 – GM4000) mounts.
    // On an AZ2000 mount: When observing an object near the lowest culmination, requests to make a 360° turn of the azimuth axis and point the object again.
    // On a german equatorial mount: When observing an object near the meridian, requests to make a 180° turn of the RA axis and move the declination axis in order to
    // point the object with the telescope on the other side of the mount.
    // Returns:
    // 1 if successful
    // 0 if the movement cannot be done
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char data[64];
    snprintf(data, sizeof(data), "#:FLIP#");
    return 0 == setStandardProcedureAndExpectChar(fd, data, "1");
}

bool LX200_10MICRON::SyncConfigBehaviour(bool cmcfg)
{
    // #:CMCFGn#
    // Configures the behaviour of the :CM# and :CMR# commands depending on the value
    // of n. If n=0, :the commands :CM# and :CMR# work in the default mode, i.e. they
    // synchronize the position to the mount with the coordinates of the currently selected
    // target by correcting the axis offset values. If n=1, the commands :CM# and :CMR#
    // work by using the synchronization position as an additional alignment star for refining
    // the alignment model.
    // Returns:
    // the string "0#" if the value 0 has been passed
    // the string "1#" if the value 1 has been passed
    // Available from version 2.8.15.
    LOG_INFO("SyncConfig.");
    if (setCommandInt(fd, cmcfg, "#:CMCFG") < 0)
    {
        return false;
    }
    return true;
}

bool LX200_10MICRON::setLocalDate(uint8_t days, uint8_t months, uint16_t years)
{
    // #:SCYYYY-MM-DD#
    // Set date to YYYY-MM-DD (year, month, day). The date is expressed in local time.
    // Returns:
    // 0 if the date is invalid
    // The character "1" without additional strings in ultra-precision mode (regardless of
    // emulation).
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char data[64];
    snprintf(data, sizeof(data), ":SC%04d-%02d-%02d#", years, months, days);
    return 0 == setStandardProcedureAndExpectChar(fd, data, "1");
}

bool LX200_10MICRON::SetTLEtoFollow(const char *tle)
{
    // #:TLEL0<two line element>#
    // Loads satellite orbital elements in two-line format directly from the command protocol.
    // <two line element> is a string containing the two line elements. Each lines can be
    // terminated by escaped newline (ASCII code 10), carriage return (ASCII code 13) or a
    // combination of both. The first line may contain the satellite name. The entire string is
    // escaped with the mechanism described in the "escaped strings" section below.
    // The TLE format is described here:
    // https://www.celestrak.com/NORAD/documentation/tle-fmt.asp
    // For example, loading the NOAA 14 element set of that page can be accomplished with:
    // :TLEL0NOAA·14·················$0a
    // 1·23455U·94089A···97320.90946019··.00000140··00000-0··10191-3·0··2621$0a
    // 2·23455··99.0090·272.6745·0008546·223.1686·136.8816·14.11711747148495#
    // Returns:
    // E# invalid format
    // V# valid format
    // Available from version 2.13.20.
    LOGF_INFO("The function is called with TLE %s", tle);
    if (strlen(tle) > 230)
    {
        LOG_WARN("TLE is too long");
    }

    std::string tle_str;
    std::string sep = "$0a";
    std::string search = "\n";
    tle_str = (std::string) tle;
    for( size_t pos = 0; ; pos += sep.length() )
    {
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
            char *pch = strtok ((char*) tle, "\n");
            while (pch != nullptr)
            {
                LOGF_INFO("%s\n", pch);
                pch = strtok (nullptr, "\n");
            }
            return 1;
        }
    }
    else
    {
        char *pch = strtok ((char*) tle, "\n");
        while (pch != nullptr)
        {
            LOGF_INFO("%s\n", pch);
            pch = strtok (nullptr, "\n");
        }
    }
    return 0;
}

bool LX200_10MICRON::SetTLEfromDatabase(int tleN)
{
    // #:TLEDLn#
    // Loads orbital elements for a satellite from the TLE database in the mount. n is the index of the
    // orbital elements in the database, starting from 1 to the number returned by the TLEDN command.
    // Returns:
    // E# the mount database doesn't contain a TLE with the given index.
    // <two line elements># an escaped string containing the TLE data from the mount
    // database which has been loaded. Lines are terminated by ASCII newline (ASCII code 10).
    // Available from version 2.13.20.
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

bool LX200_10MICRON::CalculateSatTrajectory(std::string start_pass_isodatetime, std::string end_pass_isodatetime)
{
    // #:TLEPJD,min#
    // Precalculates the first transit of the satellite with the currently loaded orbital elements,
    // starting from Julian Date JD and for a period of min minutes, where min is from 1 to 1440.
    // Two-line elements have to be loaded with the :TLEL command.
    // Returns:
    // E# no TLE loaded or invalid command
    // N# no passes in the given amount of time
    // JDstart,JDend,flags# data for the first pass in the given interval. JDstart and JDend
    // mark the beginning and the end of the given transit. Flags is a string which can be
    // empty or contain the letter F – meaning that mount will flip during the transit.
    // Available from version 2.13.20.
    struct ln_date start_pass;
    if (extractISOTime(start_pass_isodatetime.c_str(), &start_pass) == -1)
    {
        LOGF_ERROR("Date/Time is invalid: %s.", start_pass_isodatetime.c_str());
        return 1;
    }

    struct ln_date end_pass;
    if (extractISOTime(end_pass_isodatetime.c_str(), &end_pass) == -1)
    {
        LOGF_ERROR("Date/Time is invalid: %s.", end_pass_isodatetime.c_str());
        return 1;
    }

    double JD_start;
    double JD_end;
    JD_start = ln_get_julian_day(&start_pass);
    JD_end = ln_get_julian_day(&end_pass);
    int nextPassInMinutes = static_cast<int>(ceil((JD_end - JD_start) * 24 * 60));
    int nextPassinMinutesUpTo1440 = std::min(nextPassInMinutes,  1440);
    int nextPassinMinutesBetween1and1440 = std::max(nextPassinMinutesUpTo1440, 1);

    char command[28];
    snprintf(command, sizeof(command), ":TLEP%7.8f,%01d#", JD_start, nextPassinMinutesBetween1and1440);
    LOGF_INFO("Julian day %7.8f", JD_start);
    LOGF_INFO("For the next %01d minutes", nextPassinMinutesBetween1and1440);
    LOGF_INFO("Command: %s", command);
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
    // #:TLES#
    // Slews to the start of the satellite transit that has been precalculated with the :TLEP command.
    // Returns:
    // E# no transit has been precalculated
    // F# slew failed due to mount parked or other status blocking slews
    // V# slewing to the start of the transit, the mount will automatically start tracking the satellite.
    // S# the transit has already started, slewing to catch the satellite
    // Q# the transit has already ended, no slew occurs
    // Available from version 2.13.20.
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
    // #:SRTMPsTTT.T#
    // Sets the temperature used in the refraction model to sTTT.T degrees Celsius (°C).
    // Returns:
    // 0 invalid
    // 1 valid
    // Available from version 2.3.0.
    char data[16];
    snprintf(data, 16, "#:SRTMP%0+6.1f#", temperature);
    return setStandardProcedure(fd, data);
}

int LX200_10MICRON::SetRefractionModelPressure(double pressure)
{
    // #:SRPRSPPPP.P#
    // Sets the atmospheric pressure used in the refraction model to PPPP.P hPa. Note
    // that this is the pressure at the location of the telescope, and not the pressure at sea level.
    // Returns:
    // 0 invalid
    // 1 valid
    // Available from version 2.3.0.
    char data[16];
    snprintf(data, 16, "#:SRPRS%06.1f#", pressure);
    return setStandardProcedure(fd, data);
}

int LX200_10MICRON::AddSyncPoint(double MRa, double MDec, double MSide, double PRa, double PDec, double SidTime)
{
    // #:newalptMRA,MDEC,MSIDE,PRA,PDEC,SIDTIME#
    // Add a new point to the alignment specification. The parameters are:
    // MRA – the mount-reported right ascension, expressed as HH:MM:SS.S
    // MDEC – the mount-reported declination, expressed as sDD:MM:SS
    // MSIDE – the mount-reported pier side (the letter 'E' or 'W', as reported by the :pS# command)
    // PRA – the plate-solved right ascension (i.e. the right ascension the telescope was
    //       effectively pointing to), expressed as HH:MM:SS.S
    // PDEC – the plate-solved declination (i.e. the declination the telescope was effectively
    //        pointing to), expressed as sDD:MM:SS
    // SIDTIME – the local sidereal time at the time of the measurement of the point,
    //           expressed as HH:MM:SS.S
    // Returns:
    // the string "nnn#" if the point is valid, where nnn is the current number of points in the
    // alignment specification (including this one)
    // the string "E#" if the point is not valid
    // See also the paragraph Entering an alignment model.
    // Available from version 2.8.15.
    char MRa_str[32], MDec_str[32];
    fs_sexa(MRa_str, MRa, 0, 36000);
    fs_sexa(MDec_str, MDec, 0, 3600);

    char MSide_char;
    (static_cast<int>(MSide) == 0) ? MSide_char = 'E' : MSide_char = 'W';

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
        if (strcmp(name, REFRACTION_MODEL_TEMPERATURE) == 0)
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
        if (strcmp(name, REFRACTION_MODEL_PRESSURE) == 0)
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
        if (strcmp(name, MODEL_COUNT) == 0)
        {
            IUUpdateNumber(&ModelCountNP, values, names, n);
            ModelCountNP.s = IPS_OK;
            IDSetNumber(&ModelCountNP, nullptr);
            LOGF_INFO("ModelCount %d", ModelCountN[0].value);
            return true;
        }
        if (strcmp(name, MINIMAL_NEW_ALIGNMENT_POINT_RO) == 0)
        {
            IUUpdateNumber(&MiniNewAlpNP, values, names, n);
            MiniNewAlpRONP.s = IPS_OK;
            IDSetNumber(&MiniNewAlpRONP, nullptr);
            return true;
        }
        if (strcmp(name, MINIMAL_NEW_ALIGNMENT_POINT) == 0)
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
        if (strcmp(name, NEW_ALIGNMENT_POINT) == 0)
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
        if (strcmp(name, NEW_ALIGNMENT_POINTS) == 0)
        {
            IUUpdateNumber(&NewAlignmentPointsNP, values, names, n);
            NewAlignmentPointsNP.s = IPS_OK;
            IDSetNumber(&NewAlignmentPointsNP, nullptr);
            LOGF_INFO("New unnamed Model now has %d alignment points", NewAlignmentPointsN[0].value);
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
            TLEtoTrackTP.setState(IPS_IDLE);
            TLEtoTrackTP.apply();
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
        if (strcmp(AlignmentStateSP.name, name) == 0)
        {
            IUUpdateSwitch(&AlignmentStateSP, states, names, n);
            int index    = IUFindOnSwitchIndex(&AlignmentStateSP);

            switch (index)
            {
                case ALIGN_IDLE:
                    AlignmentState = ALIGN_IDLE;
                    LOG_INFO("Alignment state is IDLE");
                    break;

                case ALIGN_START:
                    // #:newalig#
                    // Start creating a new alignment specification, that will be entered with the :newalpt command.
                    // Returns:
                    // the string "V#" (this is always successful).
                    // Available from version 2.8.15.
                    if (0 != setStandardProcedureAndExpectChar(fd, "#:newalig#", "V"))
                    {
                        LOG_ERROR("New alignment start error");
                        AlignmentStateSP.s = IPS_ALERT;
                        IDSetSwitch(&AlignmentStateSP, nullptr);
                        return false;
                    }
                    LOG_INFO("New Alignment started");
                    AlignmentState = ALIGN_START;
                    break;

                case ALIGN_END:
                    // #:endalig#
                    // Completes the alignment specification and computes a new alignment from the given
                    // alignment points.
                    // Returns:
                    // the string "V#" if the alignment has been computed successfully
                    // the string "E#" if the alignment couldn't be computed successfully with the current
                    // alignment specification. In this case the previous alignment is retained.
                    // Available from version 2.8.15.
                    if (0 != setStandardProcedureAndExpectChar(fd, "#:endalig#", "V"))
                    {
                        LOG_ERROR("New alignment end error");
                        AlignmentStateSP.s = IPS_ALERT;
                        IDSetSwitch(&AlignmentStateSP, nullptr);
                        return false;
                    }
                    LOG_INFO("New Alignment ended");
                    AlignmentState = ALIGN_END;
                    break;

                case ALIGN_DELETE_CURRENT:
                    // #:delalig#
                    // Deletes the current alignment model and stars.
                    // Returns: an empty string terminated by '#'.
                    // Available from version 2.8.15.
                    if (0 != setStandardProcedureAndExpectChar(fd, "#:delalig#", "#"))
                    {
                        LOG_ERROR("Delete current alignment error");
                        AlignmentStateSP.s = IPS_ALERT;
                        IDSetSwitch(&AlignmentStateSP, nullptr);
                        return false;
                    }
                    LOG_INFO("Current Alignment deleted");
                    AlignmentState = ALIGN_DELETE_CURRENT;
                    break;

                default:
                    AlignmentStateSP.s = IPS_ALERT;
                    IDSetSwitch(&AlignmentStateSP, "Unknown alignment index %d", index);
                    AlignmentState = ALIGN_IDLE;
                    return false;
            }

            AlignmentStateSP.s = IPS_OK;
            IDSetSwitch(&AlignmentStateSP, nullptr);
            return true;
        }
        if (TrackSatSP.isNameMatch(name))
        {

            TrackSatSP.update(states, names, n);
            int index    = TrackSatSP.findOnSwitchIndex();

            switch (index)
            {
                case SAT_TRACK:
                    if ( 0 != TrackSat() )
                    {
                        TrackSatSP.setState(IPS_ALERT);
                        TrackSatSP.apply();
                        LOG_ERROR("Tracking failed");
                        return false;
                    }
                    TrackSatSP.setState(IPS_OK);
                    TrackSatSP.apply();
                    LOG_INFO("Tracking satellite");
                    return true;
                case SAT_HALT:
                    if ( !Abort() )
                    {
                        TrackSatSP.setState(IPS_ALERT);
                        TrackSatSP.apply();
                        LOG_ERROR("Halt failed");
                        return false;
                    }
                    TrackSatSP.setState(IPS_OK);
                    TrackSatSP.apply();
                    LOG_INFO("Halt tracking");
                    return true;
                default:
                    TrackSatSP.setState(IPS_ALERT);
                    LOGF_ERROR("Unknown tracking modus %d", index);
                    return false;
            }
        }

        if (strcmp(UnattendedFlipSP.name, name) == 0)
        {
            IUUpdateSwitch(&UnattendedFlipSP, states, names, n);
            int index    = IUFindOnSwitchIndex(&UnattendedFlipSP);
            switch (index)
            {
                case UNATTENDED_FLIP_DISABLED:
                    if (false == setUnattendedFlipSetting(false))
                    {
                        LOG_ERROR("Setting unattended flip failed");
                        UnattendedFlipSP.s = IPS_ALERT;
                        IDSetSwitch(&UnattendedFlipSP, nullptr);
                        return false;
                    }
                    LOG_INFO("Unattended flip disabled");
                    break;
                case UNATTENDED_FLIP_ENABLED:
                    if (false == setUnattendedFlipSetting(true))
                    {
                        LOG_ERROR("Setting unattended flip failed");
                        UnattendedFlipSP.s = IPS_ALERT;
                        IDSetSwitch(&UnattendedFlipSP, nullptr);
                        return false;
                    }
                    LOG_INFO("Unattended flip enabled");
                    break;
                default:
                    UnattendedFlipSP.s = IPS_ALERT;
                    IDSetSwitch(&UnattendedFlipSP, "Unknown unattended flip setting %d", index);
                    return false;
            }
            UnattendedFlipSP.s = IPS_OK;
            IDSetSwitch(&UnattendedFlipSP, nullptr);
            return true;
        }
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200_10MICRON::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, NEW_MODEL_NAME) == 0)
        {
            IUUpdateText(&NewModelNameTP, texts, names, n);
            NewModelNameTP.s = IPS_OK;
            IDSetText(&NewModelNameTP, nullptr);
            LOGF_INFO("Model saved with name %s", NewModelNameT[0].text);
            return true;
        }
        if (strcmp(name, "SAT_TLE_TEXT") == 0)
        {

            TLEtoTrackTP.update(texts, names, n);
            if (0 == SetTLEtoFollow(TLEtoTrackTP[0].getText()))
            {
                TLEtoTrackTP.setState(IPS_OK);
                TLEfromDatabaseNP.s = IPS_IDLE;
                TLEtoTrackTP.apply();
                IDSetNumber(&TLEfromDatabaseNP, nullptr);
                LOGF_INFO("Selected TLE %s", TLEtoTrackTP[0].getText());
                return true;
            }
            else
            {
                TLEtoTrackTP.setState(IPS_ALERT);
                TLEfromDatabaseNP.s = IPS_IDLE;
                TLEtoTrackTP.apply();
                IDSetNumber(&TLEfromDatabaseNP, nullptr);
                LOG_ERROR("TLE was not correctly uploaded");
                return false;
            }
        }
        if (strcmp(name, "SAT_PASS_WINDOW") == 0)
        {
            SatPassWindowTP.update(texts, names, n);
            if (0 == CalculateSatTrajectory(SatPassWindowTP[SAT_PASS_WINDOW_START].getText(), SatPassWindowTP[SAT_PASS_WINDOW_END].getText()))
            {
                SatPassWindowTP.setState(IPS_OK);
                SatPassWindowTP.apply();
                LOG_INFO("Trajectory set");
                return true;
            }
            else
            {
                SatPassWindowTP.setState(IPS_ALERT);
                SatPassWindowTP.apply();
                LOG_ERROR("Trajectory could not be calculated");
                return false;
            }
        }
    }
    return LX200Generic::ISNewText(dev, name, texts, names, n);
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
        { "Nov", 11 }, { "Dec", 12 }, { nullptr, 0 }
    };
    entry *p            = month_table;
    while (p->name != nullptr)
    {
        if (strcasecmp(p->name, monthName) == 0)
            return p->id;
        ++p;
    }
    return 0;
}

int LX200_10MICRON::setStandardProcedureWithoutRead(int fd, const char *data)
{
    int error_type;
    int nbytes_write = 0;

    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "CMD <%s>", data);
    tcflush(fd, TCIFLUSH);
    if ((error_type = tty_write_string(fd, data, &nbytes_write)) != TTY_OK)
    {
        LOGF_ERROR("CMD <%s> write ERROR %d", data, error_type);
        return error_type;
    }
    tcflush(fd, TCIFLUSH);
    return 0;
}

int LX200_10MICRON::setStandardProcedureAndExpectChar(int fd, const char *data, const char *expect)
{
    char bool_return[2];
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "CMD <%s>", data);
    tcflush(fd, TCIFLUSH);
    if ((error_type = tty_write_string(fd, data, &nbytes_write)) != TTY_OK)
    {
        LOGF_ERROR("CMD <%s> write ERROR %d", data, error_type);
        return error_type;
    }
    error_type = tty_read(fd, bool_return, 1, LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
    {
        LOGF_ERROR("CMD <%s> read ERROR %d", data, error_type);
        return error_type;
    }

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
    {
        LOGF_ERROR("CMD <%s> write ERROR %d", data, error_type);
        return error_type;
    }
    error_type = tty_read(fd, response, max_response_length, LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
    {
        LOGF_ERROR("CMD <%s> read ERROR %d", data, error_type);
        return error_type;
    }

    return 0;
}
