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

#include <cmath>
#include <cstring>
#include <string>
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
#define WAKE_ON_LAN_MAC  "WAKE_ON_LAN_MAC"
#define WAKE_ON_LAN_SEND "WAKE_ON_LAN_SEND"
#define MOUNT_SHUTDOWN "MOUNT_SHUTDOWN"
#define MERIDIAN_LIMITS "MERIDIAN_LIMITS"
#define TRACKING_END "TRACKING_END"
#define ALLOWED_MERIDIAN_SIDE "ALLOWED_MERIDIAN_SIDE"
#define TARGET_PIER_SIDE "TARGET_PIER_SIDE"

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

    setVersion(1, 5); // don't forget to update drivers.xml
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


        IUFillText(&WoLMacT[0], "MAC", "MAC Address (XX:XX:XX:XX:XX:XX)", "");
        IUFillTextVector(&WoLMacTP, WoLMacT, 1, getDeviceName(), WAKE_ON_LAN_MAC, "Wake on LAN", CONNECTION_TAB, IP_RW, 60,
                         IPS_IDLE);
        char wolMacBuf[128] = {};
        if (IUGetConfigText(getDeviceName(), WoLMacTP.name, WoLMacT[0].name, wolMacBuf, sizeof(wolMacBuf)) == 0)
            IUSaveText(&WoLMacT[0], wolMacBuf);

        IUFillSwitch(&WoLSendS[0], "SEND", "Wake Mount", ISS_OFF);
        IUFillSwitchVector(&WoLSendSP, WoLSendS, 1, getDeviceName(), WAKE_ON_LAN_SEND, "Wake on LAN", CONNECTION_TAB, IP_RW,
                           ISR_ATMOST1, 60, IPS_IDLE);

        MountShutdownSP[0].fill("SHUTDOWN", "Shutdown Mount", ISS_OFF);
        MountShutdownSP.fill(getDeviceName(), MOUNT_SHUTDOWN, "Shutdown", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

        // Meridian. The limits and allowed side are mount settings,
        // the time to tracking end and target pier side are computed by the mount and read-only.
        // Ranges from the manual (firmware 3.1.4) 6.3.11 Flip Slew Tol. and 6.3.12 Flip Guide Tol.
        MeridianLimitsNP[MERIDIAN_LIMIT_SLEW].fill("SLEW_LIMIT_IN_DEG", "Slew limit (deg), Flip Slew Tolerance", "%.0f", 0, 30, 1,
                0);
        MeridianLimitsNP[MERIDIAN_LIMIT_TRACK].fill("TRACK_LIMIT_IN_DEG",
                "Tracking limit (deg), Flip Guide Tolerance, >= slew limit",
                "%.0f", 2, 30, 1, 2);
        MeridianLimitsNP.fill(getDeviceName(), MERIDIAN_LIMITS, "Meridian Limits", MOTION_TAB, IP_RW, 60, IPS_IDLE);

        TrackingEndNP[0].fill("TIME_TO_END_IN_S", "Time to tracking end (s)", "%.0f", 0, 999999, 0, 0);
        TrackingEndNP.fill(getDeviceName(), TRACKING_END, "Tracking End", MOTION_TAB, IP_RO, 60, IPS_IDLE);

        // Labels (TELESCOPE_PIER_SIDE style): the pier side the mount is allowed to use.
        // The manual names the choices by the objects allowed.
        // This results in "West" in 10Micron = "East (pointing west)" in INDI, and vice versa.
        AllowedMeridianSideSP[MERIDIAN_SIDE_BOTH].fill("BOTH", "Both", ISS_ON);
        AllowedMeridianSideSP[MERIDIAN_SIDE_WEST_ONLY].fill("WEST_ONLY", "East (pointing west)", ISS_OFF);
        AllowedMeridianSideSP[MERIDIAN_SIDE_EAST_ONLY].fill("EAST_ONLY", "West (pointing east)", ISS_OFF);
        AllowedMeridianSideSP.fill(getDeviceName(), ALLOWED_MERIDIAN_SIDE, "Meridian Side", MOTION_TAB, IP_RW,
                                   ISR_1OFMANY, 60, IPS_IDLE);

        TargetPierSideSP[TARGET_PIER_WEST].fill("PIER_WEST", "West (pointing east)", ISS_OFF);
        TargetPierSideSP[TARGET_PIER_EAST].fill("PIER_EAST", "East (pointing west)", ISS_OFF);
        TargetPierSideSP[TARGET_PIER_NONE].fill("NONE", "None (no target or unreachable)", ISS_ON);
        TargetPierSideSP.fill(getDeviceName(), TARGET_PIER_SIDE, "Target Pier Side", MOTION_TAB, IP_RO, ISR_1OFMANY, 60,
                              IPS_IDLE);
    }
    return result;
}

void LX200_10MICRON::ISGetProperties(const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    defineProperty(&WoLMacTP);
    defineProperty(&WoLSendSP);
}

bool LX200_10MICRON::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &UnattendedFlipSP);
    IUSaveConfigText(fp, &WoLMacTP);
    return true;
}

// Called by INDI::Telescope when connected state changes to add/remove properties
bool LX200_10MICRON::updateProperties()
{
    // Define MountShutdownSP before the parent call so it appears immediately on connect,
    // not after the slow getBasicData() mount queries inside LX200Generic::updateProperties().
    if (isConnected())
        defineProperty(MountShutdownSP);

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
        // Read the values before defining the properties: clients such as KStars fill their edit fields
        // (sliders) from the definition, so pressing Set without changes then writes the mount's own values.
        updateMeridianValues(true);
        defineProperty(MeridianLimitsNP);
        defineProperty(TrackingEndNP);
        defineProperty(AllowedMeridianSideSP);
        defineProperty(TargetPierSideSP);

        // read UnAttendedFlip setting from config and apply if available
        int readit = 0;
        for (int i = 0; readit == 0 && i < UnattendedFlipSP.nsp; i++)
        {
            readit = IUGetConfigSwitch(getDeviceName(), UnattendedFlipSP.name, UnattendedFlipS[i].name, &(UnattendedFlipS[i].s));
        }
        if (readit == 0)
        {
            if (UnattendedFlip >= 0 && UnattendedFlipS[UnattendedFlip].s == ISS_ON)
            {
                LOGF_INFO("Unattended Flip from config and mount are %s",
                          (UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s == ISS_ON) ? "enabled" : "disabled");
            }
            else
            {
                if (UnattendedFlip < 0)
                    LOGF_INFO("Could not read Unattended Flip from the mount, applying %s from config",
                              (UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s == ISS_ON) ? "enabled" : "disabled");
                else
                    LOGF_INFO("Read Unattended Flip %s from config while mount has %s, updating mount",
                              (UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s == ISS_ON) ? "enabled" : "disabled",
                              (UnattendedFlip == UNATTENDED_FLIP_ENABLED) ? "enabled" : "disabled");
                const int wanted = (UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s == ISS_ON) ? UNATTENDED_FLIP_ENABLED :
                                   UNATTENDED_FLIP_DISABLED;
                setUnattendedFlipSetting(wanted == UNATTENDED_FLIP_ENABLED);
                // Show what the mount uses now, clients still show the value read before the update
                confirmUnattendedFlipSetting(wanted);
            }
        }
        else
        {
            LOG_INFO("Did not find an Unattended Flip setting in the config file. Specify desired behaviour in Motion Control tab and save config in Options tab.");
        }
    }
    else
    {
        deleteProperty(MountShutdownSP);
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
        deleteProperty(MeridianLimitsNP);
        deleteProperty(TrackingEndNP);
        deleteProperty(AllowedMeridianSideSP);
        deleteProperty(TargetPierSideSP);
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

        updateUnattendedFlipSetting(READ_AT_CONNECT);
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
    // The mount can be unparked by another client, such as MountWizzard4 or the hand pad.
    // Clear our parked state only when Gstat clearly says the mount is not parked, and leave it
    // alone for conditions that say nothing about the position (motors too cold, user OK
    // needed, unknown, error). Done before the switch below, as SetParked(false) also resets TrackState.
    if (isParked())
    {
        switch (Ginfo.Gstat)
        {
            case GSTAT_TRACKING:
            case GSTAT_STOPPED:
            case GSTAT_PARKING:
            case GSTAT_UNPARKING:
            case GSTAT_SLEWING_TO_HOME:
            case GSTAT_SLEWING_OR_STOPPING:
            case GSTAT_NOT_TRACKING_AND_NOT_MOVING:
            case GSTAT_TRACKING_OUTSIDE_LIMITS:
            case GSTAT_FOLLOWING_SATELLITE:
                LOGF_INFO("Gstat %d means the mount is no longer parked.", Ginfo.Gstat);
                SetParked(false);
                break;
            default:
                break;
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

    updateMeridianValues(false);

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
    // The reply is a single digit without a '#' terminator, so read exactly one character instead of
    // waiting for a '#' that never comes (that cost a 5 second timeout on every connect).
    // When the setting cannot be read, UnattendedFlip is set to -1 (unknown) and false is returned.
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char guaf[2] = {0};
    if (0 != setStandardProcedureAndReturnResponse(fd, "#:Guaf#", guaf, 1) || (guaf[0] != '0' && guaf[0] != '1'))
    {
        UnattendedFlip = -1;
        return false;
    }
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

// Read the unattended flip setting back from the mount and show it. The mount resets it to disabled at
// every power up, so this also runs periodically, see updateMeridianValues. What is logged and sent to
// clients depends on the context: at connect always, periodically only on change, and after a write
// nothing, as the caller reports the result of the write.
void LX200_10MICRON::updateUnattendedFlipSetting(LX200_10MICRON_READ_CONTEXT context)
{
    const bool enabled = getUnattendedFlipSetting();
    if (UnattendedFlip < 0)
    {
        if (context == READ_AFTER_WRITE)
        {
            UnattendedFlipSP.s = IPS_ALERT;
        }
        else if (context == READ_AT_CONNECT || UnattendedFlipSP.s != IPS_ALERT)
        {
            LOG_WARN("Failed to read the Unattended Flip setting.");
            UnattendedFlipSP.s = IPS_ALERT;
            IDSetSwitch(&UnattendedFlipSP, nullptr);
        }
        return;
    }
    const ISState enabledState = enabled ? ISS_ON : ISS_OFF;
    const bool changed = UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s != enabledState || UnattendedFlipSP.s != IPS_OK;
    UnattendedFlipS[UNATTENDED_FLIP_DISABLED].s = enabled ? ISS_OFF : ISS_ON;
    UnattendedFlipS[UNATTENDED_FLIP_ENABLED].s = enabledState;
    UnattendedFlipSP.s = IPS_OK;
    if (context == READ_AT_CONNECT || (context == READ_PERIODIC && changed))
    {
        if (context == READ_AT_CONNECT)
            LOGF_INFO("Unattended Flip is %s.", enabled ? "enabled" : "disabled");
        else
            LOGF_INFO("Unattended Flip changed on the mount: %s.", enabled ? "enabled" : "disabled");
        IDSetSwitch(&UnattendedFlipSP, nullptr);
    }
}

// After writing the unattended flip setting (:SuafN# returns nothing), read it back to confirm what the
// mount actually uses, then report the result in one log line and one property update.
bool LX200_10MICRON::confirmUnattendedFlipSetting(int wanted)
{
    updateUnattendedFlipSetting(READ_AFTER_WRITE);
    bool result = false;
    if (UnattendedFlip < 0)
    {
        LOG_WARN("Unattended Flip was written, but could not be read back.");
    }
    else if (UnattendedFlip != wanted)
    {
        LOGF_ERROR("The mount did not take Unattended Flip %s.", (wanted == UNATTENDED_FLIP_ENABLED) ? "enabled" : "disabled");
        UnattendedFlipSP.s = IPS_ALERT;
    }
    else
    {
        LOGF_INFO("Unattended Flip set: %s.", (wanted == UNATTENDED_FLIP_ENABLED) ? "enabled" : "disabled");
        result = true;
    }
    IDSetSwitch(&UnattendedFlipSP, nullptr);
    return result;
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

// Called at connect with all=true, and by ReadScopeStatus with all=false.
// The time to tracking end and the target pier side change while the mount tracks, so they are read
// every 10 seconds. The limits, meridian side and unattended flip only change when someone changes them,
// here or elsewhere, or (unattended flip) when the mount powers up, so they are read every 60 seconds.
// Properties are only sent when they change.
void LX200_10MICRON::updateMeridianValues(bool all)
{
    if (isSimulation())
        return;

    const auto now = std::chrono::steady_clock::now();
    if (all || now - lastMeridianFastPoll >= std::chrono::seconds(10))
    {
        getTimeToTrackingEnd();
        getTargetPierSide();
        lastMeridianFastPoll = now;
    }
    if (all || now - lastMeridianSlowPoll >= std::chrono::seconds(60))
    {
        const LX200_10MICRON_READ_CONTEXT context = all ? READ_AT_CONNECT : READ_PERIODIC;
        getMeridianLimits(context);
        getAllowedMeridianSide(context);
        // At connect this is already done by getBasicData
        if (!all)
            updateUnattendedFlipSetting(READ_PERIODIC);
        lastMeridianSlowPoll = now;
    }
}

bool LX200_10MICRON::getMeridianLimits(LX200_10MICRON_READ_CONTEXT context)
{
    // #:Glms#
    // Returns the meridian limit for slews in degrees.
    // Returns: NN#
    // The meridian limit for slews in degrees.
    // Available from version 2.11.
    // #:Glmt#
    // Returns the meridian limit for tracking in degrees.
    // Returns: NN#
    // The meridian limit for tracking in degrees.
    // Available from version 2.11.
    // The manual (firmware 3.1.4) calls these Flip Slew Tol. and Flip Guide Tol.:
    // 6.3.11 Flip Slew Tol.
    // Normally, the mount will point an object going to the "correct" side of the meridian; i.e. the telescope
    // will be to the east of the mount when observing an object in the western sky and vice versa. This option
    // allows you to "anticipate" an object that has not yet crossed the meridian, if the object is nearer than the
    // specified value to the meridian. The user movements are blocked after they have gone through the meridian
    // the "wrong way" more than this value. The value can vary from 0 to 30 degrees. Since this feature allows
    // the telescope to go to the "wrong" side of the meridian, check that the value you enter here does not allow
    // collisions between the instrumentation and the mount or the mount's support.
    // 6.3.12 Flip Guide Tol.
    // This option allows you to track an object through the meridian for up to 30 degrees on the "wrong" side.
    // The value can vary from 2 to 30 degrees, and must be greater than the Flip Slew Tol. value. Since this
    // feature allows the telescope to go to the "wrong" side of the meridian, check that the value you enter here
    // does not allow collisions between the instrumentation and the mount or the mount's support.
    // What is logged and sent to clients depends on the context: at connect always, periodically only on
    // change, and after a write nothing, as the caller reports the result of the write.
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char slewLimitString[80] = {0}, trackLimitString[80] = {0};
    int slewLimit = 0, trackLimit = 0;
    if (getCommandString(PortFD, slewLimitString, "#:Glms#") != 0 || sscanf(slewLimitString, "%d", &slewLimit) != 1 ||
            getCommandString(PortFD, trackLimitString, "#:Glmt#") != 0 || sscanf(trackLimitString, "%d", &trackLimit) != 1)
    {
        if (context == READ_AFTER_WRITE)
        {
            MeridianLimitsNP.setState(IPS_ALERT);
        }
        else if (context == READ_AT_CONNECT || MeridianLimitsNP.getState() != IPS_ALERT)
        {
            LOG_WARN("Failed to read the meridian limits.");
            MeridianLimitsNP.setState(IPS_ALERT);
            MeridianLimitsNP.apply();
        }
        return false;
    }
    const bool changed = MeridianLimitsNP[MERIDIAN_LIMIT_SLEW].getValue() != slewLimit ||
                         MeridianLimitsNP[MERIDIAN_LIMIT_TRACK].getValue() != trackLimit || MeridianLimitsNP.getState() != IPS_OK;
    MeridianLimitsNP[MERIDIAN_LIMIT_SLEW].setValue(slewLimit);
    MeridianLimitsNP[MERIDIAN_LIMIT_TRACK].setValue(trackLimit);
    MeridianLimitsNP.setState(IPS_OK);
    if (context == READ_AT_CONNECT || (context == READ_PERIODIC && changed))
    {
        if (context == READ_AT_CONNECT)
            LOGF_INFO("Meridian limits are: slew %d, tracking %d degrees.", slewLimit, trackLimit);
        else
            LOGF_INFO("Meridian limits changed on the mount: slew %d, tracking %d degrees.", slewLimit, trackLimit);
        MeridianLimitsNP.apply();
    }
    return true;
}

bool LX200_10MICRON::setMeridianLimits(int slewLimit, int trackLimit)
{
    // #:SlmsNN#
    // Sets the meridian limit for slews in degrees. Setting a meridian limit for slews greater
    // than the meridian limit for tracking will increase the meridian limit for tracking to the same value.
    // Returns:
    // 0 invalid
    // 1 valid
    // Available from version 2.11.
    // Note: on AZ2000HPS and AZ4000HPS mounts the meridian limit cannot be set. This command returns always 0.
    // #:SlmtNN#
    // Sets the meridian limit for tracking in degrees. The minimum value is the same as the
    // meridian limit for slews.
    // Returns:
    // 0 invalid
    // 1 valid
    // Available from version 2.11.
    // Note: on AZ2000HPS and AZ4000HPS mounts the meridian limit cannot be set. This command returns always 0.
    // See getMeridianLimits for the manual's description (Flip Slew Tol. and Flip Guide Tol.). The manual says the
    // tracking limit must be greater than the slew limit, this protocol description says at least the same, so
    // the caller checks >= and the mount has the final say.
    // The slew limit is set first, so that any tracking limit at or above the new slew limit is valid.
    // Afterwards the limits are read back, so clients see what the mount actually uses. Returns false when
    // the mount refused a value; a failed read back leaves MeridianLimitsNP in Alert for the caller to report.
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char data[64];
    snprintf(data, sizeof(data), "#:Slms%02d#", slewLimit);
    bool result = (0 == setStandardProcedureAndExpectChar(fd, data, "1"));
    if (result)
    {
        snprintf(data, sizeof(data), "#:Slmt%02d#", trackLimit);
        result = (0 == setStandardProcedureAndExpectChar(fd, data, "1"));
    }
    getMeridianLimits(READ_AFTER_WRITE);
    return result;
}

bool LX200_10MICRON::getTimeToTrackingEnd()
{
    // #:Gmte#
    // Returns the estimated time to tracking end due to horizon / flip limits reached.
    // Returns: NNNN#
    // The estimated time to tracking end in minutes of time.
    // Available from version 2.11.
    // The mount reports whole minutes, the property is in seconds.
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char timeToEndString[80] = {0};
    int minutes = 0;
    if (getCommandString(PortFD, timeToEndString, "#:Gmte#") != 0 || sscanf(timeToEndString, "%d", &minutes) != 1)
    {
        if (TrackingEndNP.getState() != IPS_ALERT)
        {
            LOG_WARN("Failed to read the time to tracking end.");
            TrackingEndNP.setState(IPS_ALERT);
            TrackingEndNP.apply();
        }
        return false;
    }
    if (TrackingEndNP[0].getValue() != minutes * 60 || TrackingEndNP.getState() != IPS_OK)
    {
        TrackingEndNP[0].setValue(minutes * 60);
        TrackingEndNP.setState(IPS_OK);
        TrackingEndNP.apply();
    }
    return true;
}

bool LX200_10MICRON::getAllowedMeridianSide(LX200_10MICRON_READ_CONTEXT context)
{
    // #:GMF#
    // Get meridian side behaviour.
    // Returns: n#
    // A number with the following interpretation:
    // 1 both sides of the meridian allowed;
    // 2 only objects west of the meridian allowed (plus slew flip tolerance) – all
    //   slews are done so that after the slew the ":pS#" command would return "East#";
    // 3 only objects east of the meridian allowed (plus slew flip tolerance) – all slews
    //   are done so that after the slew the ":pS#" command would return "West#".
    // Available from version 2.7.7.
    // The manual (firmware 3.1.4) describes this as:
    // 6.3.16 Meridian side
    // This function controls the side of the meridian you can observe. As a security feature, you may restrict the
    // motion of the mount to only one side of the meridian. Select Both if you want to allow the mount to slew
    // to both sides of the meridian (east and west). This is the normal operation, and is required for alignment
    // and balance functions.
    // Select West or East if you want to restrict the mount so that the scope may point only at objects on one
    // side of the meridian (west or east, respectively).
    // [...]
    // When pointing an object with the command protocol (i.e. from a planetarium software), the mount will fail
    // the slew if the object is on the forbidden side of the meridian.
    // The INDI labels follow TELESCOPE_PIER_SIDE: West (objects west only) is shown as East (pointing west).
    // What is logged and sent to clients depends on the context: at connect always, periodically only on
    // change, and after a write nothing, as the caller reports the result of the write.
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char sideString[80] = {0};
    int side = 0;
    if (getCommandString(PortFD, sideString, "#:GMF#") != 0 || sscanf(sideString, "%d", &side) != 1 || side < 1
            || side > MERIDIAN_SIDE_COUNT)
    {
        if (context == READ_AFTER_WRITE)
        {
            AllowedMeridianSideSP.setState(IPS_ALERT);
        }
        else if (context == READ_AT_CONNECT || AllowedMeridianSideSP.getState() != IPS_ALERT)
        {
            LOG_WARN("Failed to read the meridian side.");
            AllowedMeridianSideSP.setState(IPS_ALERT);
            AllowedMeridianSideSP.apply();
        }
        return false;
    }
    const int index = side - 1;
    const bool changed = AllowedMeridianSideSP.findOnSwitchIndex() != index || AllowedMeridianSideSP.getState() != IPS_OK;
    AllowedMeridianSideSP.reset();
    AllowedMeridianSideSP[index].setState(ISS_ON);
    AllowedMeridianSideSP.setState(IPS_OK);
    if (context == READ_AT_CONNECT || (context == READ_PERIODIC && changed))
    {
        if (context == READ_AT_CONNECT)
            LOGF_INFO("Meridian side is %s.", AllowedMeridianSideSP[index].getLabel());
        else
            LOGF_INFO("Meridian side changed on the mount: %s.", AllowedMeridianSideSP[index].getLabel());
        AllowedMeridianSideSP.apply();
    }
    return true;
}

bool LX200_10MICRON::setAllowedMeridianSide(int side)
{
    // #:SMFn#
    // Set meridian side behaviour, where n is an ASCII digit (1..3) with the following
    // interpretation:
    // 1 both sides of the meridian allowed;
    // 2 only objects west of the meridian allowed (plus slew flip tolerance) – all slews
    //   are done so that after the slew the ":pS#" command would return "East#";
    // 3 only objects east of the meridian allowed (plus slew flip tolerance) – all slews
    //   are done so that after the slew the ":pS#" command would return "West#".
    // Returns:
    // 0 invalid
    // 1 valid
    // See getAllowedMeridianSide for the manual's description (6.3.16 Meridian side).
    // Afterwards the side is read back, so clients see what the mount actually uses. Returns false when the
    // mount refused; a failed read back leaves AllowedMeridianSideSP in Alert for the caller to report.
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char data[64];
    snprintf(data, sizeof(data), "#:SMF%d#", side + 1);
    const bool result = (0 == setStandardProcedureAndExpectChar(fd, data, "1"));
    getAllowedMeridianSide(READ_AFTER_WRITE);
    return result;
}

bool LX200_10MICRON::getTargetPierSide()
{
    // #:GTsid#
    // Get the destination side of the target object
    // Returns:
    // 0 no target defined, or the target object is located in a position where it is not
    //   possible to go
    // 2 the target is located in a position where the mount would slew the telescope to
    //   the west side so that after the slew the ":pS#" command would return "West#".
    // 3 the target is located in a position where the mount would slew the telescope to
    //   the east side so that after the slew the ":pS#" command would return "East#".
    // Available from version 2.9.9.
    // Notes
    // See the :MSfs command.
    // Up to version 2.15.31 included, return values 2 and 3 were reversed in the southern hemisphere.
    // The reply is a single digit without a '#' terminator, so read exactly one character.
    // The target is the last one set, normally by the last goto. This is the pier side a new slew to it
    // would end on, which can change while the target moves across the meridian.
    DEBUGFDEVICE(getDefaultName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char sideString[2] = {0};
    int index = -1;
    if (0 == setStandardProcedureAndReturnResponse(fd, "#:GTsid#", sideString, 1))
    {
        switch (sideString[0])
        {
            case '0':
                index = TARGET_PIER_NONE;
                break;
            case '2':
                index = TARGET_PIER_WEST;
                break;
            case '3':
                index = TARGET_PIER_EAST;
                break;
        }
    }
    if (index < 0)
    {
        if (TargetPierSideSP.getState() != IPS_ALERT)
        {
            LOG_WARN("Failed to read the target pier side.");
            TargetPierSideSP.setState(IPS_ALERT);
            TargetPierSideSP.apply();
        }
        return false;
    }
    if (TargetPierSideSP.findOnSwitchIndex() != index || TargetPierSideSP.getState() != IPS_OK)
    {
        TargetPierSideSP.reset();
        TargetPierSideSP[index].setState(ISS_ON);
        TargetPierSideSP.setState(IPS_OK);
        TargetPierSideSP.apply();
    }
    return true;
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
        if (MeridianLimitsNP.isNameMatch(name))
        {
            int slewLimit = static_cast<int>(MeridianLimitsNP[MERIDIAN_LIMIT_SLEW].getValue());
            int trackLimit = static_cast<int>(MeridianLimitsNP[MERIDIAN_LIMIT_TRACK].getValue());
            for (int i = 0; i < n; i++)
            {
                if (MeridianLimitsNP[MERIDIAN_LIMIT_SLEW].isNameMatch(names[i]))
                    slewLimit = static_cast<int>(std::lround(values[i]));
                else if (MeridianLimitsNP[MERIDIAN_LIMIT_TRACK].isNameMatch(names[i]))
                    trackLimit = static_cast<int>(std::lround(values[i]));
            }
            if (trackLimit < slewLimit)
            {
                LOGF_ERROR("Tracking limit (Flip Guide Tolerance) %d must be >= slew limit (Flip Slew Tolerance) %d, "
                           "nothing written to the mount.", trackLimit, slewLimit);
                MeridianLimitsNP.setState(IPS_ALERT);
                MeridianLimitsNP.apply();
                return false;
            }
            // One log line and one property update, showing what the mount reports after the write
            bool result = false;
            if (!setMeridianLimits(slewLimit, trackLimit))
            {
                LOGF_ERROR("The mount refused tracking limit %d with slew limit %d.", trackLimit, slewLimit);
                MeridianLimitsNP.setState(IPS_ALERT);
            }
            else if (MeridianLimitsNP.getState() == IPS_ALERT)
            {
                LOG_WARN("Meridian limits were written, but could not be read back.");
            }
            else
            {
                LOGF_INFO("Meridian limits set: slew %d, tracking %d degrees.",
                          static_cast<int>(MeridianLimitsNP[MERIDIAN_LIMIT_SLEW].getValue()),
                          static_cast<int>(MeridianLimitsNP[MERIDIAN_LIMIT_TRACK].getValue()));
                result = true;
            }
            MeridianLimitsNP.apply();
            return result;
        }
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
        if (AllowedMeridianSideSP.isNameMatch(name))
        {
            AllowedMeridianSideSP.update(states, names, n);
            const int index = AllowedMeridianSideSP.findOnSwitchIndex();
            if (index < 0)
            {
                AllowedMeridianSideSP.setState(IPS_ALERT);
                AllowedMeridianSideSP.apply();
                return false;
            }
            // One log line and one property update, showing what the mount reports after the write
            const char *label = AllowedMeridianSideSP[index].getLabel();
            bool result = false;
            if (!setAllowedMeridianSide(index))
            {
                LOGF_ERROR("The mount refused meridian side %s.", label);
                AllowedMeridianSideSP.setState(IPS_ALERT);
            }
            else if (AllowedMeridianSideSP.getState() == IPS_ALERT)
            {
                LOG_WARN("Meridian side was written, but could not be read back.");
            }
            else if (AllowedMeridianSideSP.findOnSwitchIndex() != index)
            {
                LOGF_ERROR("The mount did not take meridian side %s.", label);
                AllowedMeridianSideSP.setState(IPS_ALERT);
            }
            else
            {
                LOGF_INFO("Meridian side set: %s.", label);
                result = true;
            }
            AllowedMeridianSideSP.apply();
            return result;
        }
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
                    break;
                case UNATTENDED_FLIP_ENABLED:
                    if (false == setUnattendedFlipSetting(true))
                    {
                        LOG_ERROR("Setting unattended flip failed");
                        UnattendedFlipSP.s = IPS_ALERT;
                        IDSetSwitch(&UnattendedFlipSP, nullptr);
                        return false;
                    }
                    break;
                default:
                    UnattendedFlipSP.s = IPS_ALERT;
                    IDSetSwitch(&UnattendedFlipSP, "Unknown unattended flip setting %d", index);
                    return false;
            }
            // :SuafN# returns nothing, so read the setting back to confirm what the mount actually uses
            return confirmUnattendedFlipSetting(index);
        }
        if (strcmp(WoLSendSP.name, name) == 0)
        {
            IUResetSwitch(&WoLSendSP);
            if (sendWakeOnLanPacket())
            {
                WoLSendSP.s = IPS_OK;
                LOG_INFO("Wake on LAN packet sent successfully.");
            }
            else
            {
                WoLSendSP.s = IPS_ALERT;
                LOG_ERROR("Failed to send Wake on LAN packet.");
            }
            IDSetSwitch(&WoLSendSP, nullptr);
            return true;
        }
        if (MountShutdownSP.isNameMatch(name))
        {
            MountShutdownSP.reset();
            if (!isParked())
            {
                MountShutdownSP.setState(IPS_ALERT);
                LOG_WARN("Mount must be parked before shutdown.");
                MountShutdownSP.apply();
                return false;
            }
            // :shutdown# — returns '1' on success, '0' on failure; ~20 s until power-off
            if (0 != setStandardProcedureAndExpectChar(fd, ":shutdown#", "1"))
            {
                MountShutdownSP.setState(IPS_ALERT);
                LOG_ERROR("Mount shutdown command failed.");
                MountShutdownSP.apply();
                return false;
            }
            MountShutdownSP.setState(IPS_OK);
            LOG_INFO("Mount shutdown initiated. Disconnecting before mount powers off.");
            MountShutdownSP.apply();
            // Defer disconnect so ISNewSwitch returns cleanly before we tear down the property list
            m_ShutdownPending = true;
            SetTimer(100);
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
            if (0 == CalculateSatTrajectory(SatPassWindowTP[SAT_PASS_WINDOW_START].getText(),
                                            SatPassWindowTP[SAT_PASS_WINDOW_END].getText()))
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
        if (strcmp(name, WAKE_ON_LAN_MAC) == 0)
        {
            IUUpdateText(&WoLMacTP, texts, names, n);
            WoLMacTP.s = IPS_OK;
            IDSetText(&WoLMacTP, nullptr);
            LOGF_INFO("WoL MAC address set to %s", WoLMacT[0].text);
            return true;
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

void LX200_10MICRON::TimerHit()
{
    if (m_ShutdownPending)
    {
        m_ShutdownPending = false;
        // Standard INDI disconnect sequence — same as clicking Disconnect in the UI
        Disconnect();
        setConnected(false, IPS_IDLE);
        updateProperties();
        return; // do not reschedule polling
    }
    LX200Generic::TimerHit();
}

bool LX200_10MICRON::sendWakeOnLanPacket()
{
    const char *macStr = WoLMacT[0].text;
    if (!macStr || macStr[0] == '\0')
    {
        LOG_ERROR("WoL: MAC address not configured.");
        return false;
    }

    std::string cmd = "wakeonlan " + std::string(macStr);
    int ret = system(cmd.c_str());
    if (ret == 0)
    {
        LOGF_INFO("WoL: magic packet sent to %s", macStr);
        return true;
    }
    if (ret == 127)
    {
        LOG_ERROR("WoL: wakeonlan tool not found. Please install it on your system.");
        return false;
    }
    LOG_ERROR("WoL: wakeonlan failed to send the packet.");
    return false;
}
