/*
    Astro-Physics INDI driver

    Copyright (C) 2014 Jasem Mutlaq, Mike Fulbright
    Copyright (C) 2020 indilib.org, by Markus Wildi
    Copyright (C) 2022 Hy Murveit

    Based on INDI Astrophysics Driver by Markus Wildi

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
/*
2020-08-07, ToDo --wildi
AP commands not yet implemented for revision >= G

Sets the centering rate for the N-S-E-W buttons to xxx Rcxxx#
Default command for an equatorial fork mount, which eliminates the meridian flip :FM#
Default command for A German equatorial mount that includes the meridian flip :EM#
Horizon check during slewing functions :ho# and :hq#
*/


/***********************************************************************
 * This file was copied an modified from lx200ap.cpp in Jan 2022.
 *
 * This is an update of the Wildi and Fulbright A-P drivers.
 * It is currently being tested.
 * You should not use this unless part of the test group.
***********************************************************************/

#include "lx200ap_v2.h"

#include "indicom.h"
#include "lx200driver.h"
#include "lx200apdriver.h"
#include "connectionplugins/connectiontcp.h"

#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <regex>

// PEC Recording values
enum APPECRecordingState
{
    AP_PEC_RECORD_OFF = 0,
    AP_PEC_RECORD_ON = 1
};

// maximum guide pulse request to send to controller
#define MAX_LX200AP_PULSE_LEN 999

// The workaround for long pulses doesn't work!
// #define DONT_SIMULATE_LONG_PULSES true
// This didn't work. The driver simply doesn't send pulse
// commands longer than 999ms since CP3 controllers don't support that.

LX200AstroPhysicsV2::LX200AstroPhysicsV2() : LX200Generic()
{
    setLX200Capability(LX200_HAS_PULSE_GUIDING);
    // The 5 means there are 5 slew rates.
    SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_PEC | TELESCOPE_CAN_CONTROL_TRACK
                           | TELESCOPE_HAS_TRACK_RATE, 5);

    majorVersion = 0;
    minorVersion = 0;
    setVersion(1, 1);
}

const char *LX200AstroPhysicsV2::getDefaultName()
{
    return "AstroPhysics V2";
}

bool LX200AstroPhysicsV2::Connect()
{
    Connection::Interface *activeConnection = getActiveConnection();
    if (!activeConnection->name().compare("CONNECTION_TCP"))
    {
        // When using a tcp connection, the GTOCP4 adds trailing LF to response.
        // this small hack will get rid of them as they are not expected in the driver. and generated
        // lot of communication errors.
        tty_clr_trailing_read_lf(1);
    }

    // If ApInitialize fails, probably have to turn some buttons red. Verify!
    return LX200Generic::Connect() && ApInitialize();
}

bool LX200AstroPhysicsV2::initProperties()
{
    LX200Generic::initProperties();

    timeFormat = LX200_24;

    IUFillNumber(&HourangleCoordsN[0], "HA", "HA H:M:S", "%10.6m", -24., 24., 0., 0.);
    IUFillNumber(&HourangleCoordsN[1], "DEC", "Dec D:M:S", "%10.6m", -90.0, 90.0, 0., 0.);
    IUFillNumberVector(&HourangleCoordsNP, HourangleCoordsN, 2, getDeviceName(), "HOURANGLE_COORD", "Hourangle Coords",
                       MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&HorizontalCoordsN[0], "AZ", "Az D:M:S", "%10.6m", 0., 360., 0., 0.);
    IUFillNumber(&HorizontalCoordsN[1], "ALT", "Alt D:M:S", "%10.6m", -90., 90., 0., 0.);
    IUFillNumberVector(&HorizontalCoordsNP, HorizontalCoordsN, 2, getDeviceName(), "HORIZONTAL_COORD",
                       "Horizontal Coords", MAIN_CONTROL_TAB, IP_RW, 120, IPS_IDLE);

    // Max rate is 999.99999X for the GTOCP4.
    // Using :RR998.9999#  just to be safe. 15.041067*998.99999 = 15026.02578
    TrackRateN[AXIS_RA].min = -15026.0258;
    TrackRateN[AXIS_RA].max = 15026.0258;
    TrackRateN[AXIS_DE].min = -998.9999;
    TrackRateN[AXIS_DE].max = 998.9999;

    // Rates populated in a different routine since they can change after connect:
    initRateLabels();

    // Home button for clutch aware mounts with encoders.
    IUFillSwitch(&HomeAndReSyncS[0], "GO", "Home and ReSync", ISS_OFF);
    IUFillSwitchVector(&HomeAndReSyncSP, HomeAndReSyncS, 1, getDeviceName(), "TELESCOPE_HOME",
                       "HomeAndReSync", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Manual-set-mount-to-parked button for recovering from issues.
    IUFillSwitch(&ManualSetParkedS[0], "MANUAL SET PARKED", "Manual Set Parked", ISS_OFF);
    IUFillSwitchVector(&ManualSetParkedSP, ManualSetParkedS, 1, getDeviceName(), "MANUAL_SET_PARKED",
                       "ManualSetParked", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillSwitch(&SwapS[0], "NS", "North/South", ISS_OFF);
    IUFillSwitch(&SwapS[1], "EW", "East/West", ISS_OFF);
    IUFillSwitchVector(&SwapSP, SwapS, 2, getDeviceName(), "SWAP", "Swap buttons", MOTION_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // guide speed
    IUFillSwitch(&APGuideSpeedS[0], "0.25", "0.25x", ISS_OFF);
    IUFillSwitch(&APGuideSpeedS[1], "0.5", "0.50x", ISS_OFF);
    IUFillSwitch(&APGuideSpeedS[2], "1.0", "1.0x", ISS_ON);
    IUFillSwitchVector(&APGuideSpeedSP, APGuideSpeedS, 3, getDeviceName(), "Guide Rate", "", GUIDE_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // Unpark from?
    // Order should be the same as the ParkPosition enum.
    IUFillSwitch(&UnparkFromS[0], "Last", "Last Parked--recommended!", ISS_ON);
    IUFillSwitch(&UnparkFromS[1], "Park1", "Park1", ISS_OFF);
    IUFillSwitch(&UnparkFromS[2], "Park2", "Park2", ISS_OFF);
    IUFillSwitch(&UnparkFromS[3], "Park3", "Park3", ISS_OFF);
    IUFillSwitch(&UnparkFromS[4], "Park4", "Park4", ISS_OFF);
    IUFillSwitchVector(&UnparkFromSP, UnparkFromS, 5, getDeviceName(), "UNPARK_FROM", "Unpark From?", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // park presets
    // Order should be the same as the ParkPosition enum.
    IUFillSwitch(&ParkToS[0], "Custom", "Custom--not implemented", ISS_OFF);
    IUFillSwitch(&ParkToS[1], "Park1", "Park1", ISS_OFF);
    IUFillSwitch(&ParkToS[2], "Park2", "Park2", ISS_OFF);
    IUFillSwitch(&ParkToS[3], "Park3", "Park3", ISS_ON);
    IUFillSwitch(&ParkToS[4], "Park4", "Park4", ISS_OFF);
    IUFillSwitch(&ParkToS[5], "Current", "Current Position--not implemented", ISS_OFF);
    IUFillSwitchVector(&ParkToSP, ParkToS, 6, getDeviceName(), "PARK_TO", "Park To?", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillText(&VersionT[0], "Version", "Version", "");
    IUFillTextVector(&VersionTP, VersionT, 1, getDeviceName(), "Firmware", "Firmware", SITE_TAB, IP_RO, 0, IPS_IDLE);

    // UTC offset
    IUFillNumber(&APUTCOffsetN[0], "APUTC_OFFSET", "AP UTC offset", "%8.5f", 0.0, 24.0, 0.0, 0.);
    IUFillNumberVector(&APUTCOffsetNP, APUTCOffsetN, 1, getDeviceName(), "APUTC_OFFSET", "AP UTC offset", SITE_TAB,
                       IP_RW, 60, IPS_OK);
    // sidereal time, ToDO move define where it belongs to
    IUFillNumber(&APSiderealTimeN[0], "AP_SIDEREAL_TIME", "AP sidereal time", "%10.6m", 0.0, 24.0, 0.0, 0.0);
    IUFillNumberVector(&APSiderealTimeNP, APSiderealTimeN, 1, getDeviceName(), "AP_SIDEREAL_TIME", "ap sidereal time", SITE_TAB,
                       IP_RO, 60, IPS_OK);

    // Worm position
    IUFillNumber(&APWormPositionN[0], "APWormPosition", "AP Worm Position", "%3.0f", 0, 1000, 1, 0);
    IUFillNumberVector(&APWormPositionNP, APWormPositionN, 1, getDeviceName(), "APWormPosition", "AP Worm Position",
                       MOTION_TAB, IP_RO, 0, IPS_IDLE);

    // PEC State
    IUFillText(&APPECStateT[0], "APPECState", "AP PEC State", "");
    IUFillTextVector(&APPECStateTP, APPECStateT, 1, getDeviceName(), "APPECState", "AP PEC State",
                     MOTION_TAB, IP_RO, 0, IPS_IDLE);

    IUFillText(&APMountStatusT[0], "APMountStatus", "AP Mount Status", "");
    IUFillTextVector(&APMountStatusTP, APMountStatusT, 1, getDeviceName(), "APMountStatus", "AP Mount Status",
                     MOTION_TAB, IP_RO, 0, IPS_IDLE);

    // PEC Record button.
    IUFillSwitch(&APPECRecordS[AP_PEC_RECORD_OFF], "APPECRecordOFF", "Off", ISS_ON);
    IUFillSwitch(&APPECRecordS[AP_PEC_RECORD_ON], "APPECRecordON", "Record", ISS_OFF);
    IUFillSwitchVector(&APPECRecordSP, APPECRecordS, 2, getDeviceName(), "APPECRecord", "Record PEC", MOTION_TAB,
                       IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    tcpConnection->setConnectionType(Connection::TCP::TYPE_UDP);

    // Without below, it will not write the ParkData.xml file.
    // However, ParkData.xml is not used.
    // SetParkDataType(PARK_AZ_ALT);

    return true;
}

void LX200AstroPhysicsV2::initRateLabels()
{
    if (rateTable == AP_RATE_TABLE_DEFAULT) // Legacy, pre P02-01
    {
        // Motion speed of axis when pressing NSWE buttons
        IUFillSwitch(&SlewRateS[0], "1", "Guide", ISS_OFF);
        IUFillSwitch(&SlewRateS[1], "12", "12x", ISS_OFF);
        IUFillSwitch(&SlewRateS[2], "64", "64x", ISS_ON);
        IUFillSwitch(&SlewRateS[3], "600", "600x", ISS_OFF);
        IUFillSwitch(&SlewRateS[4], "1200", "1200x", ISS_OFF);
        IUFillSwitchVector(&SlewRateSP, SlewRateS, 5, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW,
                           ISR_1OFMANY, 0, IPS_IDLE);

        // Slew speed when performing regular GOTO
        IUFillSwitch(&APSlewSpeedS[0], "600", "600x", ISS_ON);
        IUFillSwitch(&APSlewSpeedS[1], "900", "900x", ISS_OFF);
        IUFillSwitch(&APSlewSpeedS[2], "1200", "1200x", ISS_OFF);
        IUFillSwitchVector(&APSlewSpeedSP, APSlewSpeedS, 3, getDeviceName(), "GOTO Rate", "", MOTION_TAB, IP_RW, ISR_1OFMANY,
                           0, IPS_IDLE);
    }
    else
    {
        // This is the rate table straight out of the CPx source.  First two numbers
        // are the highest two center/button rates, and the next three numbers are the
        // three 'goto' rates.  There are 4 sets of rates for 4 different types of mounts.
        std::string standard_rates[4][5] =
        {
            { "600", "1200", "600", "900",  "1200" },
            { "500", "900",  "400", "650",  "900"  },
            { "400", "600",  "300", "450",  "600"  },
            { "600", "1200", "600", "1000", "1800" }
        };

        // The 8 means there are 8 slew/center rates.
        SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_PEC | TELESCOPE_CAN_CONTROL_TRACK
                               | TELESCOPE_HAS_TRACK_RATE, 8);
        int i = rateTable;
        IUFillSwitch(&SlewRateS[0], "0.25", "0.25x", ISS_OFF);
        IUFillSwitch(&SlewRateS[1], "0.5",  "0.5x",  ISS_OFF);
        IUFillSwitch(&SlewRateS[2], "1.0",  "1.0x",  ISS_OFF);
        IUFillSwitch(&SlewRateS[3], "12",   "12x",   ISS_OFF);
        IUFillSwitch(&SlewRateS[4], "64",   "64x",   ISS_ON);
        IUFillSwitch(&SlewRateS[5], "200",  "200x",  ISS_OFF);
        IUFillSwitch(&SlewRateS[6], standard_rates[i][0].c_str(), standard_rates[i][0].append("x").c_str(), ISS_OFF);
        IUFillSwitch(&SlewRateS[7], standard_rates[i][1].c_str(), standard_rates[i][1].append("x").c_str(), ISS_OFF);

        // Slew speed when performing regular GOTO
        IUFillSwitch(&APSlewSpeedS[0], standard_rates[i][2].c_str(), standard_rates[i][2].append("x").c_str(), ISS_ON);
        IUFillSwitch(&APSlewSpeedS[1], standard_rates[i][3].c_str(), standard_rates[i][3].append("x").c_str(), ISS_OFF);
        IUFillSwitch(&APSlewSpeedS[2], standard_rates[i][4].c_str(), standard_rates[i][4].append("x").c_str(), ISS_OFF);
    }
}

void LX200AstroPhysicsV2::ISGetProperties(const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    defineProperty(&ManualSetParkedSP);
    defineProperty(&UnparkFromSP);
    defineProperty(&ParkToSP);

    if (isConnected())
    {
        if (homeAndReSyncEnabled)
            defineProperty(&HomeAndReSyncSP);
        defineProperty(&VersionTP);
        defineProperty(&APSlewSpeedSP);
        defineProperty(&SwapSP);
        defineProperty(&APGuideSpeedSP);
        defineProperty(&APWormPositionNP);
        defineProperty(&APPECStateTP);
        defineProperty(&APPECRecordSP);
        defineProperty(&APMountStatusTP);
    }
}

bool LX200AstroPhysicsV2::updateProperties()
{
    LX200Generic::updateProperties();

    defineProperty(&ManualSetParkedSP);
    defineProperty(&UnparkFromSP);
    defineProperty(&ParkToSP);

    if (isConnected())
    {
        if (homeAndReSyncEnabled)
            defineProperty(&HomeAndReSyncSP);
        /* Motion group */
        defineProperty(&APSlewSpeedSP);
        defineProperty(&SwapSP);
        defineProperty(&APGuideSpeedSP);
        defineProperty(&APSiderealTimeNP);
        defineProperty(&HourangleCoordsNP);
        defineProperty(&APUTCOffsetNP);
        defineProperty(&APWormPositionNP);
        defineProperty(&APPECStateTP);
        defineProperty(&APPECRecordSP);
        defineProperty(&APMountStatusTP);
    }
    else
    {
        deleteProperty(HomeAndReSyncSP.name);
        deleteProperty(VersionTP.name);
        deleteProperty(APSlewSpeedSP.name);
        deleteProperty(SwapSP.name);
        deleteProperty(APGuideSpeedSP.name);
        deleteProperty(APUTCOffsetNP.name);
        deleteProperty(APSiderealTimeNP.name);
        deleteProperty(HourangleCoordsNP.name);
        deleteProperty(APWormPositionNP.name);
        deleteProperty(APPECStateTP.name);
        deleteProperty(APPECRecordSP.name);
        deleteProperty(APMountStatusTP.name);
    }

    return true;
}

bool LX200AstroPhysicsV2::getWormPosition()
{
    int position;
    if (isSimulation())
        position = 0;
    else
    {
        int res = getAPWormPosition(PortFD, &position);
        if (res != TTY_OK)
        {
            APWormPositionNP.np[0].value = 0;
            APWormPositionNP.s           = IPS_ALERT;
            IDSetNumber(&APWormPositionNP, nullptr);
            return false;
        }
    }
    APWormPositionNP.np[0].value = position;
    APWormPositionNP.s           = IPS_OK;
    IDSetNumber(&APWormPositionNP, nullptr);
    return true;
}

void LX200AstroPhysicsV2::processMountStatus(const char *statusString)
{
    const char *mountStatusString = apMountStatus(statusString);
    IUSaveText(&APMountStatusT[0], mountStatusString);
    IDSetText(&APMountStatusTP, nullptr);
    APMountStatusTP.s = IPS_OK;
}

bool LX200AstroPhysicsV2::getPECState(const char *statusString)
{
    int pecState;
    if (isSimulation())
        pecState = AP_PEC_OFF;
    else
    {
        if (strlen(statusString) < 10)
            return false;
        const char pecChar = statusString[9];
        switch (pecChar)
        {
            case 'O':
                pecState = AP_PEC_OFF;
                break;
            case 'P':
                pecState = AP_PEC_ON;
                break;
            case 'R':
                pecState = AP_PEC_RECORD;
                break;
            case 'E':
                pecState = AP_PEC_ENCODER;
                break;
            default:
                IUSaveText(&APPECStateT[0], "");
                IDSetText(&APPECStateTP, nullptr);
                APPECStateTP.s = IPS_ALERT;
                return false;
        }
    }
    // Set the text status display based on the info from the mount.
    // Also set the PEC buttons: playback on/off & recording on/off.
    switch (pecState)
    {
        case AP_PEC_OFF:
            IUSaveText(&APPECStateT[0], "Off");

            APPECRecordS[AP_PEC_RECORD_OFF].s = ISS_ON;
            APPECRecordS[AP_PEC_RECORD_ON].s = ISS_OFF;
            APPECRecordSP.s           = IPS_OK;
            IDSetSwitch(&APPECRecordSP, nullptr);

            setPECState(PEC_OFF);

            break;
        case AP_PEC_ON:
            IUSaveText(&APPECStateT[0], "On");

            APPECRecordS[AP_PEC_RECORD_OFF].s = ISS_ON;
            APPECRecordS[AP_PEC_RECORD_ON].s = ISS_OFF;
            APPECRecordSP.s           = IPS_OK;
            IDSetSwitch(&APPECRecordSP, nullptr);

            setPECState(PEC_ON);

            break;
        case AP_PEC_RECORD:
            IUSaveText(&APPECStateT[0], "Recording");

            APPECRecordS[AP_PEC_RECORD_OFF].s = ISS_OFF;
            APPECRecordS[AP_PEC_RECORD_ON].s = ISS_ON;
            APPECRecordSP.s           = IPS_OK;
            IDSetSwitch(&APPECRecordSP, nullptr);

            setPECState(PEC_OFF);

            break;
        case AP_PEC_ENCODER:
            IUSaveText(&APPECStateT[0], "Encoder");

            APPECRecordS[AP_PEC_RECORD_OFF].s = ISS_ON;
            APPECRecordS[AP_PEC_RECORD_ON].s = ISS_OFF;
            APPECRecordSP.s           = IPS_OK;
            IDSetSwitch(&APPECRecordSP, nullptr);

            setPECState(PEC_OFF);

            break;
    }
    IDSetText(&APPECStateTP, nullptr);
    APPECStateTP.s = IPS_OK;

    return true;
}

// The version string should be formatted as VCP4-$MAJOR-$MINOR.
// Could be VCP5 as well. For instance: VCP4-P02-12
void LX200AstroPhysicsV2::setMajorMinorVersions(char *version)
{
    majorVersion = 0;
    minorVersion = 0;

    const std::string v = version;
    std::regex rgx(".*-(\\w+)-(\\w+)");
    std::smatch match;

    if (std::regex_search(v.begin(), v.end(), match, rgx))
    {
        std::string major = match.str(1);
        std::string minor = match.str(2);

        std::string majorStripped = std::regex_replace(major, std::regex(R"([\D])"), "");
        std::string minorStripped = std::regex_replace(minor, std::regex(R"([\D])"), "");

        if (majorStripped.size() > 0)
            majorVersion = stoi(majorStripped);
        if (minorStripped.size() > 0)
            minorVersion = stoi(minorStripped);
    }
}

bool LX200AstroPhysicsV2::getFirmwareVersion()
{
    bool success;
    char rev[8];
    char versionString[128];
    majorVersion = 0;
    minorVersion = 0;

    success = false;

    if (isSimulation())
        strncpy(versionString, "VCP4-P01-01", 128);
    else
        getAPVersionNumber(PortFD, versionString);

    VersionTP.s = IPS_OK;
    IUSaveText(&VersionT[0], versionString);
    IDSetText(&VersionTP, nullptr);

    if (strstr(versionString, "VCP4"))
    {
        firmwareVersion = MCV_V;
        servoType = GTOCP4;
        strcpy(rev, "V");
        success = true;
        setMajorMinorVersions(versionString);
    }
    else if (strstr(versionString, "VCP5"))

    {
        firmwareVersion = MCV_V;
        servoType = GTOCP5;
        strcpy(rev, "V");
        setMajorMinorVersions(versionString);
        success = true;
    }
    else if (strlen(versionString) == 1 || strlen(versionString) == 2)
    {
        // Check earlier versions
        // FIXME could probably use better range checking in case we get a letter like 'Z' that doesn't map to anything!
        int typeIndex = VersionT[0].text[0] - 'D';
        if (typeIndex >= 0)
        {
            firmwareVersion = static_cast<ControllerVersion>(typeIndex);
            LOGF_DEBUG("Firmware version index: %d", typeIndex);
            if (firmwareVersion < MCV_G)
                servoType = GTOCP2;
            else
                servoType = GTOCP3;

            strncpy(rev, versionString, 8);

            success = true;
        }
        else
        {
            LOGF_WARN("unknown AP controller version %s", VersionT[0].text);
        }
    }

    if (success)
    {
        LOGF_INFO("Servo Box Controller: GTOCP%d.", servoType);
        LOGF_INFO("Firmware Version: '%s'", versionString);
        if (majorVersion && minorVersion)
            LOGF_INFO("Firmware Major Version: %d Minor Version %d", majorVersion, minorVersion);
    }

    return success;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200AstroPhysicsV2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(getDeviceName(), dev))
        return false;

    if (!strcmp(name, APUTCOffsetNP.name))
    {
        if (IUUpdateNumber(&APUTCOffsetNP, values, names, n) < 0)
            return false;

        float mdelay;
        int err;

        mdelay = APUTCOffsetN[0].value;

        if (!isSimulation() && (err = setAPUTCOffset(PortFD, mdelay) < 0))
        {
            LOGF_ERROR("Error setting UTC offset (%d).", err);
            return false;
        }

        APUTCOffsetNP.s  = IPS_OK;
        IDSetNumber(&APUTCOffsetNP, nullptr);

        return true;
    }
    if (!strcmp(name, HourangleCoordsNP.name))
    {

        if (IUUpdateNumber(&HourangleCoordsNP, values, names, n) < 0)
            return false;

        double lng = LocationN[LOCATION_LONGITUDE].value;
        double lst = get_local_sidereal_time(lng);
        double ra = lst - HourangleCoordsN[0].value;
        double dec = HourangleCoordsN[1].value;
        bool success = false;
        if ((ISS_ON == IUFindSwitch(&CoordSP, "TRACK")->s) || (ISS_ON == IUFindSwitch(&CoordSP, "SLEW")->s))
        {
            success = Goto(ra, dec);
        }
        else
        {
            success = APSync(ra, dec, true);
        }
        if (success)
        {
            HourangleCoordsNP.s = IPS_OK;
        }
        else
        {
            HourangleCoordsNP.s = IPS_ALERT;
        }
        IDSetNumber(&HourangleCoordsNP,  nullptr);
        return true;
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200AstroPhysicsV2::ApInitialize()
{
    if ((firmwareVersion == MCV_UNKNOWN) || (firmwareVersion < MCV_T))
    {
        // Can't use this driver
        LOG_ERROR("This driver requires at least version T firmware");
        return false;
    }

    rateTable = AP_RATE_TABLE_DEFAULT;
    char status_string[256];
    if (getApStatusString(PortFD, status_string) == TTY_OK)
    {
        rateTable = apRateTable(status_string);
        if ((rateTable >= 0) && (rateTable <= 3))
            LOGF_INFO("Using Rate Table: %d", rateTable);
        else
            rateTable = AP_RATE_TABLE_DEFAULT;
    }
    initRateLabels();

    homeAndReSyncEnabled = apCanHome(PortFD);

    // Set location up every time we connect.
    double longitude = -1000, latitude = -1000;
    // Get value from config file if it exists.
    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
    if (longitude != -1000 && latitude != -1000)
        updateAPLocation(latitude, longitude, 0);

    bool isAPParked = false;
    if (!IsMountParked(&isAPParked))
        return false;
    if (isAPParked)
    {
        if (!loadConfig(true, UnparkFromSP.name))
            LOG_DEBUG("could not load config data for UnparkFromSP.name");
        if (!loadConfig(true, ParkToSP.name))
            LOG_DEBUG("could not load config data for ParkTo.name");
        if(UnparkFromS[PARK_LAST].s == ISS_ON)
            LOG_INFO("Driver's config 'Unpark From ?' is set to Last Parked");
        // forcing mount being parked from INDI's perspective
        LOG_INFO("ApInitialize, parked.");
        SetParked(true);
    }
    else
    {
        LOG_INFO("ApInitialize, not parked.");
        SetParked(false);
    }

    if (isSimulation())
    {
        SlewRateSP.s = IPS_OK;
        IDSetSwitch(&SlewRateSP, nullptr);

        APSlewSpeedSP.s = IPS_OK;
        IDSetSwitch(&APSlewSpeedSP, nullptr);

        IUSaveText(&VersionT[0], "1.0");
        VersionTP.s = IPS_OK;
        IDSetText(&VersionTP, nullptr);

        return true;

    }
    // Make sure that the mount is setup according to the properties
    int switch_nr = IUFindOnSwitchIndex(&TrackModeSP);

    int err = 0;
    if ( (err = selectAPTrackingMode(PortFD, switch_nr)) < 0)
    {
        LOGF_ERROR("ApInitialize: Error setting tracking mode (%d).", err);
        return false;
    }

    // On most mounts SlewRateS defines the MoveTo AND Slew (GOTO) speeds
    // lx200ap is different - some of the MoveTo speeds are not VALID
    // Slew speeds so we have to keep two lists.
    //
    // SlewRateS is used as the MoveTo speed
    switch_nr = IUFindOnSwitchIndex(&SlewRateSP);
    if ((err = selectAPV2CenterRate(PortFD, switch_nr, rateTable)) < 0)
    {
        LOGF_ERROR("ApInitialize: Error setting move rate (%d).", err);
        return false;
    }

    SlewRateSP.s = IPS_OK;
    IDSetSwitch(&SlewRateSP, nullptr);

    // APSlewSpeedsS defines the Slew (GOTO) speeds valid on the AP mounts
    switch_nr = IUFindOnSwitchIndex(&APSlewSpeedSP);
    if ( (err = selectAPSlewRate(PortFD, switch_nr)) < 0)
    {
        LOGF_ERROR("ApInitialize: Error setting slew to rate (%d).", err);
        return false;
    }
    APSlewSpeedSP.s = IPS_OK;
    IDSetSwitch(&APSlewSpeedSP, nullptr);

    getLX200RA(PortFD, &currentRA);
    getLX200DEC(PortFD, &currentDEC);

    // make a IDSet in order the dome controller is aware of the initial values
    targetRA  = currentRA;
    targetDEC = currentDEC;

    NewRaDec(currentRA, currentDEC);

    char versionString[64];
    getAPVersionNumber(PortFD, versionString);
    VersionTP.s = IPS_OK;
    IUSaveText(&VersionT[0], versionString);
    IDSetText(&VersionTP, nullptr);

    return true;
}

bool LX200AstroPhysicsV2::ISNewSwitch(const char *dev, const char *name, ISState * states, char *names[], int n)
{
    int err = 0;
    if (strcmp(getDeviceName(), dev))
        return false;

    // =======================================
    // Swap Buttons
    // =======================================
    if (!strcmp(name, SwapSP.name))
    {
        int currentSwap;

        IUResetSwitch(&SwapSP);
        IUUpdateSwitch(&SwapSP, states, names, n);
        currentSwap = IUFindOnSwitchIndex(&SwapSP);

        if ((!isSimulation() && (err = swapAPButtons(PortFD, currentSwap)) < 0))
        {
            LOGF_ERROR("Error swapping buttons (%d).", err);
            return false;
        }

        SwapS[0].s = ISS_OFF;
        SwapS[1].s = ISS_OFF;
        SwapSP.s   = IPS_OK;
        IDSetSwitch(&SwapSP, nullptr);
        return true;
    }

    // ===========================================================
    // GOTO ("slew") Speed.
    // ===========================================================
    if (!strcmp(name, APSlewSpeedSP.name))
    {
        IUUpdateSwitch(&APSlewSpeedSP, states, names, n);
        int slewRate = IUFindOnSwitchIndex(&APSlewSpeedSP);

        if (!isSimulation() && (err = selectAPSlewRate(PortFD, slewRate) < 0))
        {
            LOGF_ERROR("Error setting move to rate (%d).", err);
            return false;
        }

        APSlewSpeedSP.s = IPS_OK;
        IDSetSwitch(&APSlewSpeedSP, nullptr);
        return true;
    }

    // ===========================================================
    // Guide Speed.
    // ===========================================================
    if (!strcmp(name, APGuideSpeedSP.name))
    {
        IUUpdateSwitch(&APGuideSpeedSP, states, names, n);
        int guideRate = IUFindOnSwitchIndex(&APGuideSpeedSP);

        if (!isSimulation() && (err = selectAPGuideRate(PortFD, guideRate) < 0))
        {
            LOGF_ERROR("Error setting guiding to rate (%d).", err);
            return false;
        }

        APGuideSpeedSP.s = IPS_OK;
        IDSetSwitch(&APGuideSpeedSP, nullptr);
        return true;
    }

    // =======================================
    // Choose the PEC playback mode
    // =======================================
    if (!strcmp(name, PECStateSP.name))
    {
        IUResetSwitch(&PECStateSP);
        IUUpdateSwitch(&PECStateSP, states, names, n);
        IUFindOnSwitchIndex(&PECStateSP);

        int pecstate = IUFindOnSwitchIndex(&PECStateSP);

        if (!isSimulation() && (err = selectAPPECState(PortFD, pecstate) < 0))
        {
            LOGF_ERROR("Error setting PEC state (%d).", err);
            return false;
        }

        PECStateSP.s = IPS_OK;
        IDSetSwitch(&PECStateSP, nullptr);

        return true;
    }

    if (strcmp(name, APPECRecordSP.name) == 0)
    {
        IUResetSwitch(&APPECRecordSP);
        IUUpdateSwitch(&APPECRecordSP, states, names, n);
        IUFindOnSwitchIndex(&APPECRecordSP);

        int recordState = IUFindOnSwitchIndex(&APPECRecordSP);

        // Can't turn recording off.
        if (recordState == AP_PEC_RECORD_ON)
        {
            err = selectAPPECState(PortFD, AP_PEC_RECORD);
            if (!isSimulation() && (err != 0))
            {
                LOGF_ERROR("Error setting PEC state RECORD (%d).", err);
                return false;
            }
            LOG_INFO("Recording PEC");
            APPECRecordSP.s = IPS_OK;
            IDSetSwitch(&PECStateSP, nullptr);
        }
        return true;
    }

    // ===========================================================
    // Unpark from positions
    // ===========================================================
    if (!strcmp(name, UnparkFromSP.name))
    {
        IUUpdateSwitch(&UnparkFromSP, states, names, n);
        ParkPosition unparkPos = static_cast<ParkPosition>(IUFindOnSwitchIndex(&UnparkFromSP));

        UnparkFromSP.s = IPS_OK;
        if( unparkPos != PARK_LAST)
        {
            double unparkAlt, unparkAz;
            if (!calcParkPosition(unparkPos, &unparkAlt, &unparkAz))
            {
                LOG_WARN("Error calculating unpark position!");
                UnparkFromSP.s = IPS_ALERT;
            }
            else
            {
                // 2020-06-01, wildi, UnPark() relies on it
                saveConfig(true);
            }
        }
        IDSetSwitch(&UnparkFromSP, nullptr);
        return true;
    }

    // ===========================================================
    // Switch Park(ed), Unpark(ed)
    // ===========================================================

    if (!strcmp(name, ParkSP.name))
    {
    }

    // ===========================================================
    // Park To positions
    // ===========================================================
    if (!strcmp(name, ParkToSP.name))
    {
        IUUpdateSwitch(&ParkToSP, states, names, n);
        ParkPosition parkPos = static_cast<ParkPosition>(IUFindOnSwitchIndex(&ParkToSP));
        if (parkPos != PARK_CUSTOM && parkPos != PARK_CURRENT)
        {
            double parkAz, parkAlt;
            if (calcParkPosition(parkPos, &parkAlt, &parkAz))
            {
                LOGF_INFO("Set predefined park position %d to az=%f alt=%f", parkPos, parkAz, parkAlt);
            }
            else
            {
                LOGF_ERROR("Unable to set predefined park position %d!!", parkPos);
            }
        }
        else
        {
            LOG_WARN("ISNewSwitch: park custom/current not supported");
            IUResetSwitch(&ParkToSP);
            ParkToSP.s = IPS_ALERT;
            IDSetSwitch(&ParkToSP, nullptr);
            return false;
        }
        IUResetSwitch(&ParkToSP);
        ParkToS[(int)parkPos].s = ISS_ON;
        ParkToSP.s = IPS_OK;
        IDSetSwitch(&ParkToSP, nullptr);
        return true;
    }

    if (strcmp(name, ManualSetParkedSP.name) == 0)
    {
        // Force the mount to be parked where it is and disconnect.
        IUResetSwitch(&ManualSetParkedSP);
        bool alreadyConnected = isConnected();
        if (!alreadyConnected)
        {
            LX200Generic::Disconnect();
            Connection::Interface *activeConnection = getActiveConnection();
            if (!activeConnection->name().compare("CONNECTION_TCP"))
            {
                // When using a tcp connection, the GTOCP4 adds trailing LF to response.
                // this small hack will get rid of them as they are not expected in the driver. and generated
                // lot of communication errors.
                tty_clr_trailing_read_lf(1);
            }
            if (!LX200Generic::Connect())
            {
                LOG_ERROR("Connect failed for Manual Park");
                return true;
            }
        }

        if (parkInternal())
            saveConfig(true);
        else
            LOG_ERROR("ParkInternal failed for Manual Park");

        if (!alreadyConnected)
        {
            if (!Disconnect())
                LOG_ERROR("Disconnect failed for Manual Park");
        }
        IDSetSwitch(&ManualSetParkedSP, nullptr);
        return true;
    }

    // ===========================================================
    // Home and ReSync mount
    // ===========================================================
    if (strcmp(name, HomeAndReSyncSP.name) == 0)
    {
        IUResetSwitch(&HomeAndReSyncSP);
        apHomeAndSync(PortFD);
        IDSetSwitch(&HomeAndReSyncSP, nullptr);
        return true;
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

char* trackStateString(INDI::Telescope::TelescopeStatus state)
{
    switch(state)
    {
        case INDI::Telescope::SCOPE_IDLE:
            static char idleStr[] = "Idle";
            return idleStr;
        case INDI::Telescope::SCOPE_SLEWING:
            static char slewStr[] = "Slewing";
            return slewStr;
        case INDI::Telescope::SCOPE_TRACKING:
            static char trackStr[] = "Tracking";
            return trackStr;
        case INDI::Telescope::SCOPE_PARKING:
            static char parkingStr[] = "Parking";
            return parkingStr;
        case INDI::Telescope::SCOPE_PARKED:
            static char parkStr[] = "Parked";
            return parkStr;
    }
    static char emptyStr[] = "???";
    return emptyStr;
}

bool LX200AstroPhysicsV2::ReadScopeStatus()
{
    if (!isAPReady())
    {
        LOGF_DEBUG("APStatus: Not ready--Checked %s Initialized %s Time updated %s Location Updated %s",
                   apInitializationChecked ? "Y" : "N", apIsInitialized ? "Y" : "N",
                   apTimeInitialized ? "Y" : "N", apLocationInitialized ? "Y" : "N");

        // hope this return doesn't delay the time & location. If it does return true?
        return false;
    }
    double lng = LocationN[LOCATION_LONGITUDE].value;
    double lst = get_local_sidereal_time(lng);
    double val = lst;
    if ((!isSimulation()) && (getSDTime(PortFD, &val) < 0))
    {
        LOG_ERROR("Reading sidereal time failed");
        return false;
    }
    std::string sTimeStr;
    if (val >= 0 && val <= 24.0)
        sTimeStr = std::to_string(val);
    else
    {
        val = 0;
        sTimeStr = "????";
    }

    APSiderealTimeNP.np[0].value = lst;
    APSiderealTimeNP.s           = IPS_IDLE;
    IDSetNumber(&APSiderealTimeNP, nullptr);

    if (isSimulation())
    {
        mountSim();
        return true;
    }
    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.setState(IPS_ALERT);
        LOG_ERROR("Error reading RA/DEC.");
        EqNP.apply();
        return false;
    }

    char apStatusString[256];
    if (getApStatusString(PortFD, apStatusString) != TTY_OK)
    {
        LOG_ERROR("Reading AP status failed");
        return false;
    }

    getWormPosition();
    getPECState(apStatusString);
    processMountStatus(apStatusString);

    const bool apParked = apStatusParked(apStatusString);
    if (!apParked)
    {
        double ha = get_local_hour_angle(lst, currentRA);

        // No need to spam log until we have some actual changes.
        if (std::fabs(HourangleCoordsN[0].value - ha) > 0.00001 ||
                std::fabs(HourangleCoordsN[1].value - currentDEC) > 0.00001)
        {
            // in case of simulation, the coordinates are set on parking
            HourangleCoordsN[0].value = ha;
            HourangleCoordsN[1].value = currentDEC;
            HourangleCoordsNP.s = IPS_OK;
            IDSetNumber(&HourangleCoordsNP, nullptr );
        }
    }
    LOGF_DEBUG("APStatus: %s %s stime: %s  RA/DEC: %.3f %.3f",
               trackStateString(TrackState), apParked ? "Parked" : "Unparked", sTimeStr.c_str(), currentRA, currentDEC);

    if (TrackState == SCOPE_SLEWING)
    {
        const double dx = fabs(lastRA - currentRA);
        const double dy = fabs(lastDE - currentDEC);

        LOGF_DEBUG("Slewing... currentRA: %.3f dx: %g currentDE: %.3f dy: %g", currentRA, dx, currentDEC, dy);

        // Note, RA won't hit 0 if it's not tracking, because the RA changes when still.
        // Dec might, though.
        // 0 might work now that I "fixed" slewing...perhaps not when tracking is off.
        if (dx < 1e-3 && dy < 1e-3)
        {
            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }

        // Keep try of last values to determine if the mount settled.
        lastRA = currentRA;
        lastDE = currentDEC;
    }
    else if (TrackState == SCOPE_PARKING)
    {
        bool slewcomplete = false;
        double PARKTHRES = 0.1; // max difference from parked position to consider mount PARKED

        if (!apStatusSlewing(apStatusString))
            slewcomplete = true;

        // old way
        if (getLX200Az(PortFD, &currentAz) < 0 || getLX200Alt(PortFD, &currentAlt) < 0)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Error reading Az/Alt.");
            EqNP.apply();
            return false;
        }

        const double dx = fabs(lastAZ - currentAz);
        const double dy = fabs(lastAL - currentAlt);
        LOGF_DEBUG("Parking... currentAz: %g dx: %g currentAlt: %g dy: %g", currentAz, dx, currentAlt, dy);

        // if for some reason we check slew status BEFORE park motion starts make sure we dont consider park
        // action complete too early by checking how far from park position we are!

        if (slewcomplete && (dx > PARKTHRES || dy > PARKTHRES))
        {
            LOG_WARN("Parking... slew status indicates mount stopped by dx/dy too far from mount - continuing!");
            slewcomplete = false;
        }

        // Not sure why it hedged previously. Require slewcomplete for now. Verify!
        // if (slewcomplete || (dx <= PARKTHRES && dy <= PARKTHRES))
        if (slewcomplete)
        {
            LOG_DEBUG("Parking slew is complete. Asking astrophysics mount to park...");
            if (!parkInternal())
            {
                return false;
            }
            saveConfig(true);
        }

        lastAZ = currentAz;
        lastAL = currentAlt;
    }

    NewRaDec(currentRA, currentDEC);

    syncSideOfPier();

    return true;
}

bool LX200AstroPhysicsV2::parkInternal()
{
    if (APParkMount(PortFD) < 0)
    {
        LOG_ERROR("Parking Failed.");
        return false;
    }

    SetTrackEnabled(false);
    SetParked(true);
    return true;
}

bool LX200AstroPhysicsV2::IsMountParked(bool * isAPParked)
{
    if (isSimulation())
    {
        // 2030-05-30, if Unparked is selected, this condition is not met
        *isAPParked = (ParkS[0].s == ISS_ON);
        return true;
    }

    char parkStatus;
    char slewStatus;
    if (check_lx200ap_status(PortFD, &parkStatus, &slewStatus) == 0)
    {
        LOGF_DEBUG("parkStatus: %c", parkStatus);

        *isAPParked = (parkStatus == 'P');
        return true;
    }
    return false;
}

bool LX200AstroPhysicsV2::Goto(double r, double d)
{
    if (!isAPReady())
        return false;

    const struct timespec timeout = {0, 100000000L};

    targetRA  = r;
    targetDEC = d;

    char RAStr[64], DecStr[64];
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // If moving, let's stop it first.
    if (EqNP.getState() == IPS_BUSY)
    {
        if (!isSimulation() && abortSlew(PortFD) < 0)
        {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
        }

        AbortSP.s = IPS_OK;
        EqNP.setState(IPS_IDLE);
        IDSetSwitch(&AbortSP, "Slew aborted.");
        EqNP.apply();

        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
            MovementNSSP.s = IPS_IDLE;
            MovementWESP.s = IPS_IDLE;
            EqNP.setState(IPS_IDLE);
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
        }

        // sleep for 100 mseconds
        nanosleep(&timeout, nullptr);
    }

    if (!isSimulation())
    {
        if (setAPObjectRA(PortFD, targetRA) < 0 || (setAPObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Error setting RA/DEC.");
            EqNP.apply();
            return false;
        }

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = Slew(PortFD)))
        {
            EqNP.setState(IPS_ALERT);
            LOGF_ERROR("Error Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
            EqNP.apply();
            slewError(err);
            return false;
        }
        lastRA = targetRA;
        lastDE = targetDEC;
    }

    TrackState = SCOPE_SLEWING;
    //EqNP.s     = IPS_BUSY;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

// AP mounts handle guide commands differently enough from the "generic" LX200 we need to override some
// functions related to the GuiderInterface

IPState LX200AstroPhysicsV2::GuideNorth(uint32_t ms)
{
    if (!isAPReady())
        return IPS_ALERT;

    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (ms > MAX_LX200AP_PULSE_LEN)
    {
        LOGF_DEBUG("GuideNorth truncating %dms pulse to %dms", ms, MAX_LX200AP_PULSE_LEN);
        ms = MAX_LX200AP_PULSE_LEN;
    }
    if (usePulseCommand)
    {
        APSendPulseCmd(PortFD, LX200_NORTH, ms);
        GuideNSTID      = IEAddTimer(static_cast<int>(ms), pulseGuideTimeoutHelperNS, this);
    }

    return IPS_BUSY;
}

IPState LX200AstroPhysicsV2::GuideSouth(uint32_t ms)
{
    if (!isAPReady())
        return IPS_ALERT;

    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (ms > MAX_LX200AP_PULSE_LEN)
    {
        LOGF_DEBUG("GuideSouth truncating %dms pulse to %dms", ms, MAX_LX200AP_PULSE_LEN);
        ms = MAX_LX200AP_PULSE_LEN;
    }
    if (usePulseCommand)
    {
        APSendPulseCmd(PortFD, LX200_SOUTH, ms);
        GuideNSTID      = IEAddTimer(static_cast<int>(ms), pulseGuideTimeoutHelperNS, this);
    }

    return IPS_BUSY;
}

IPState LX200AstroPhysicsV2::GuideEast(uint32_t ms)
{
    if (!isAPReady())
        return IPS_ALERT;

    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if (ms > MAX_LX200AP_PULSE_LEN)
    {
        LOGF_DEBUG("GuideEast truncating %dms pulse to %dms", ms, MAX_LX200AP_PULSE_LEN);
        ms = MAX_LX200AP_PULSE_LEN;
    }
    if (usePulseCommand)
    {
        APSendPulseCmd(PortFD, LX200_EAST, ms);
        GuideWETID      = IEAddTimer(static_cast<int>(ms), pulseGuideTimeoutHelperWE, this);
    }

    return IPS_BUSY;
}

IPState LX200AstroPhysicsV2::GuideWest(uint32_t ms)
{
    if (!isAPReady())
        return IPS_ALERT;

    // If we're using pulse command, then MovementXXX should NOT be active at all.
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot pulse guide while manually in motion. Stop first.");
        return IPS_ALERT;
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if (ms > MAX_LX200AP_PULSE_LEN)
    {
        LOGF_DEBUG("GuideWest truncating %dms pulse to %dms", ms, MAX_LX200AP_PULSE_LEN);
        ms = MAX_LX200AP_PULSE_LEN;
    }
    if (usePulseCommand)
    {
        APSendPulseCmd(PortFD, LX200_WEST, ms);
        GuideWETID      = IEAddTimer(static_cast<int>(ms), pulseGuideTimeoutHelperWE, this);
    }

    return IPS_BUSY;
}

void LX200AstroPhysicsV2::pulseGuideTimeoutHelperNS(void * p)
{
    static_cast<LX200AstroPhysicsV2 *>(p)->AstroPhysicsGuideTimeoutNS(false);
}

void LX200AstroPhysicsV2::pulseGuideTimeoutHelperWE(void * p)
{
    static_cast<LX200AstroPhysicsV2 *>(p)->AstroPhysicsGuideTimeoutWE(false);
}

void LX200AstroPhysicsV2::simulGuideTimeoutHelperNS(void * p)
{
    static_cast<LX200AstroPhysicsV2 *>(p)->AstroPhysicsGuideTimeoutNS(true);
}

void LX200AstroPhysicsV2::simulGuideTimeoutHelperWE(void * p)
{
    static_cast<LX200AstroPhysicsV2 *>(p)->AstroPhysicsGuideTimeoutWE(true);
}

void LX200AstroPhysicsV2::AstroPhysicsGuideTimeoutWE(bool simul)
{
    LOGF_DEBUG("AstroPhysicsGuideTimeoutWE() pulse guide simul = %d", simul);

    if (simul == true)
    {
        ISState states[] = { ISS_OFF, ISS_OFF };
        const char *names[] = { MovementWES[DIRECTION_WEST].name, MovementWES[DIRECTION_EAST].name};
        ISNewSwitch(MovementWESP.device, MovementWESP.name, states, const_cast<char **>(names), 2);
    }

    GuideWENP[DIRECTION_WEST].setValue(0);
    GuideWENP[DIRECTION_EAST].setValue(0);
    GuideWENP.setState(IPS_IDLE);
    GuideWETID = 0;
    GuideWENP.apply();
}

void LX200AstroPhysicsV2::AstroPhysicsGuideTimeoutNS(bool simul)
{
    LOGF_DEBUG("AstroPhysicsGuideTimeoutNS() pulse guide simul = %d", simul);

    if (simul == true)
    {
        ISState states[] = { ISS_OFF, ISS_OFF };
        const char *names[] = { MovementNSS[DIRECTION_NORTH].name, MovementNSS[DIRECTION_SOUTH].name};
        ISNewSwitch(MovementNSSP.device, MovementNSSP.name, states, const_cast<char **>(names), 2);
    }

    GuideNSNP[0].setValue(0);
    GuideNSNP[1].setValue(0);
    GuideNSNP.setState(IPS_IDLE);
    GuideNSTID            = 0;
    GuideNSNP.apply();
}

int LX200AstroPhysicsV2::SendPulseCmd(int8_t direction, uint32_t duration_msec)
{
    return APSendPulseCmd(PortFD, direction, duration_msec);
}

bool LX200AstroPhysicsV2::Handshake()
{
    if (isSimulation())
    {
        LOG_INFO("Simulated Astrophysics is online. Retrieving basic data...");
        getFirmwareVersion();
        return true;
    }

    int err = 0;

    if ((err = setAPClearBuffer(PortFD)) < 0)
    {
        LOGF_ERROR("Error clearing the buffer (%d): %s", err, strerror(err));
        return false;
    }
    if (!getActiveConnection()->name().compare("CONNECTION_TCP"))
    {
        LOG_INFO("Setting generic udp format (1)");
        tty_set_generic_udp_format(1);
    }
    if (setAPBackLashCompensation(PortFD, 0, 0, 0) < 0)
    {
        // It seems we need to send it twice before it works!
        if ((err = setAPBackLashCompensation(PortFD, 0, 0, 0)) < 0)
        {
            LOGF_ERROR("Error setting backlash compensation (%d): %s.", err, strerror(err));
        }
    }

    // get firmware version
    bool rc = false;

    rc = getFirmwareVersion();

    if (!rc || (firmwareVersion == MCV_UNKNOWN) || (firmwareVersion < MCV_T))
    {
        // Can't use this driver at < version T. No way to test.
        LOG_ERROR("Firmware detection failed or is unknown. This driver requires at least version T firmware");
        return false;
    }

    // Do not track until mount is umparked
    if ((err = selectAPTrackingMode(PortFD, AP_TRACKING_OFF)) < 0)
    {
        LOGF_ERROR("Handshake: Error setting tracking mode to zero (%d).", err);
        return false;
    }
    else
    {
        LOG_INFO("Stopped tracking");
    }

    // Check to see if the mount is initialized during handshake.
    // If it isn't, we'll later need to set things up, and make sure
    // that location and time were sent to it.
    apIsInitialized = false;
    apInitializationChecked = false;
    apLocationInitialized = false;
    apTimeInitialized = false;

    // Let it fail twice before failing
    if ((isAPInitialized(PortFD, &apIsInitialized) != TTY_OK) ||
            (isAPInitialized(PortFD, &apIsInitialized) != TTY_OK))
    {
        return false;
    }
    apInitializationChecked = true;

    // Detect and set format. It should be LONG.
    return (checkLX200EquatorialFormat(PortFD) == 0);
}

bool LX200AstroPhysicsV2::isAPReady()
{
    if (!apInitializationChecked)
        return false;

    // AP has passed the initialization check.
    if (apIsInitialized)
        return true;

    // Below is implementing the LastParked Scheme.
    // I don't require that PARK_LAST is the unparkFrom scheme.
    // If the mount is uninitialized, then trust the mount's PARK_LAST data.
    if (apLocationInitialized && apTimeInitialized)
    {
        bool commWorked = true;
        char statusString[256];
        if (getApStatusString(PortFD, statusString) != TTY_OK)
        {
            // Try again
            commWorked = getApStatusString(PortFD, statusString) == TTY_OK;
        }
        if (commWorked)
        {
            bool isAPParked = apStatusParked(statusString);

            // A-P came up uninitialized, but we can now fix.
            if (APUnParkMount(PortFD) != TTY_OK)
                // Try again if we had a comm failure.
                commWorked = APUnParkMount(PortFD) == TTY_OK;

            if (commWorked)
            {
                // The mount should now be "calibrated" and have a correct RA/DEC,
                // based on its LastParked position.
                bool mountOk = false;
                if (isAPInitialized(PortFD, &mountOk) != TTY_OK)
                {
                    // try one more time
                    commWorked = isAPInitialized(PortFD, &mountOk) == TTY_OK;
                }
                if (commWorked && mountOk)
                {
                    apIsInitialized = true;

                    // Put it back into the state we found it.
                    if (isAPParked)
                        parkInternal();
                    else
                        SetParked(false);
                    return true;
                }
            }
        }
        // If we arrive here, we tried but were unable to initialized the mount.
        // If unparkFrom is set to one of the park positions, and we're parked,
        // then don't fail, as we will recover on the unpark.
        ISState lastParkState;
        IUGetConfigSwitch(getDeviceName(), "UNPARK_FROM", "Last", &lastParkState);
        if (commWorked && apStatusParked(statusString) && (lastParkState != ISS_ON))
        {
            apIsInitialized = true;
            return true;
        }
        else
        {
            LOG_ERROR("Could not initialize mount.");
            Disconnect();  // Not sure about this....
            return false;
        }
    }
    // Not initialized, but not ready to give up either.
    return false;
}

bool LX200AstroPhysicsV2::Disconnect()
{
    apIsInitialized = false;
    apLocationInitialized = false;
    apTimeInitialized = false;
    apInitializationChecked = false;

    return LX200Generic::Disconnect();
}

bool LX200AstroPhysicsV2::APSync(double ra, double dec, bool recalibrate)
{
    char syncString[256] = ""; // simulation needs UTF-8

    if (!isSimulation())
    {
        if (setAPObjectRA(PortFD, ra) < 0 || setAPObjectDEC(PortFD, dec) < 0)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Error setting RA/DEC. Unable to Sync.");
            EqNP.apply();
            return false;
        }
        bool syncOK = true;

        if (recalibrate)
        {
            if (APSyncCMR(PortFD, syncString) < 0)
                syncOK = false;
        }
        else
        {
            if (APSyncCM(PortFD, syncString) < 0)
                syncOK = false;
        }

        if (!syncOK)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Synchronization failed");
            EqNP.apply();
            return false;
        }

    }

    currentRA  = ra;
    currentDEC = dec;
    LOGF_DEBUG("%s Synchronization successful %s", (recalibrate ? "CMR" : "CM"), syncString);

    EqNP.setState(IPS_OK);

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200AstroPhysicsV2::Sync(double ra, double dec)
{
    // The default sync is a "CMR" / "Recalibrate" sync.
    return APSync(ra, dec, true);
}

bool LX200AstroPhysicsV2::updateTime(ln_date * utc, double utc_offset)
{
    // 2020-06-02, wildi, ToDo, time obtained from KStars differs up to a couple
    // of 5 seconds from system time.
    struct ln_zonedate ltm;

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600.0);
    JD = ln_get_julian_day(utc);
    LOGF_DEBUG("New JD is %f, local time: %d, %d, %d, utc offset: %f", JD, ltm.hours, ltm.minutes, (int)ltm.seconds,
               utc_offset);

    // Set Local Time
    if (isSimulation() == false && setLocalTime(PortFD, ltm.hours, ltm.minutes, (int)ltm.seconds) < 0)
    {
        LOG_ERROR("Error setting local time.");
        return false;
    }
    LOGF_DEBUG("Set Local Time %02d:%02d:%02d is successful.", ltm.hours, ltm.minutes,
               (int)ltm.seconds);

    if (isSimulation() == false && setCalenderDate(PortFD, ltm.days, ltm.months, ltm.years) < 0)
    {
        LOG_ERROR("Error setting local date.");
        return false;
    }
    LOGF_DEBUG("Set Local Date %02d/%02d/%02d is successful.", ltm.days, ltm.months, ltm.years);

    // 2020-05-30, wildi, after a very long journey
    // AP:  TZ (0,12): West, East (-12.,-0), (>12,24)
    // Peru, Lima:
    //(TX=':Gg#'), RX='+77*01:42#
    //(TX=':SG05:00:00#'), RX='1'
    // Linux/Windows TZ values: West: -12,0, East 0,12
    // AP GTOCPX accepts a converted float including 24.
    double ap_utc_offset = - utc_offset;
    if (!isSimulation() && setAPUTCOffset(PortFD, ap_utc_offset) < 0)
    {
        LOG_ERROR("Error setting UTC Offset.");
        return false;
    }
    APUTCOffsetN[0].value = ap_utc_offset ;
    APUTCOffsetNP.s  = IPS_OK;
    IDSetNumber(&APUTCOffsetNP, nullptr);

    LOGF_DEBUG("Set UTC Offset %g as AP UTC Offset %g is successful.", utc_offset, ap_utc_offset);
    apTimeInitialized = true;
    return true;
}

bool LX200AstroPhysicsV2::updateAPLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    LOG_DEBUG("LX200AstroPhysicsV2::updateLocation entry");

    if ((latitude == 0.) && (longitude == 0.))
    {
        LOG_DEBUG("updateLocation: latitude, longitude both zero");
        return false;
    }

    // Why is it 360-longitude? Verify!
    double apLongitude = 360 - longitude ;
    while (apLongitude < 0)
        apLongitude += 360;
    while (apLongitude > 360)
        apLongitude -= 360;

    LOGF_DEBUG("Setting site longitude coordinates, %f %f", longitude, apLongitude);

    if (!isSimulation() && setAPSiteLongitude(PortFD, apLongitude) < 0)
    {
        LOG_ERROR("Error setting site longitude coordinates");
        return false;
    }

    if (!isSimulation() && setAPSiteLatitude(PortFD, latitude) < 0)
    {
        LOG_ERROR("Error setting site latitude coordinates");
        return false;
    }

    char l[32], L[32];
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    LOGF_DEBUG("Site location updated to Lat %.32s - Long %.32s, deg: %f, %f", l, L, latitude, longitude);
    apLocationInitialized = true;
    return true;
}

bool LX200AstroPhysicsV2::updateLocation(double latitude, double longitude, double elevation)
{
    if ((latitude == 0.) && (longitude == 0.))
    {
        LOG_DEBUG("updateLocation: latitude, longitude both zero");
        return false;
    }
    if (!isConnected())
        return true;
    return updateAPLocation(latitude, longitude, elevation);
}

void LX200AstroPhysicsV2::debugTriggered(bool enable)
{
    LX200Generic::debugTriggered(enable);

    // we use routines from legacy AP driver routines and newer experimental driver routines
    set_lx200ap_name(getDeviceName(), DBG_SCOPE);

}

// For most mounts the SetSlewRate() method sets both the MoveTo and Slew (GOTO) speeds.
// For AP mounts these two speeds are handled separately - so SetSlewRate() actually sets the MoveTo speed for AP mounts - confusing!
// ApSetSlew
bool LX200AstroPhysicsV2::SetSlewRate(int index)
{
    if (!isSimulation() && selectAPV2CenterRate(PortFD, index, rateTable) < 0)
    {
        LOG_ERROR("Error setting slew mode.");
        return false;
    }

    return true;
}

bool LX200AstroPhysicsV2::Park()
{
    ParkPosition parkPos = static_cast<ParkPosition>(IUFindOnSwitchIndex(&ParkToSP));
    double parkAz {90}, parkAlt {0};
    if (parkPos == PARK_CURRENT)
    {
        LOG_DEBUG("PARK_CURRENT not implemented");
    }
    else if (calcParkPosition(parkPos, &parkAlt, &parkAz))
    {
        LOGF_DEBUG("Set park position %d to az=%f alt=%f", parkPos, parkAz, parkAlt);
    }
    else
    {
        LOGF_ERROR("Unable to set park position %d!!", parkPos);
    }

    char AzStr[16] = {0}, AltStr[16] = {0};
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_INFO("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    INDI::IEquatorialCoordinates equatorialCoords {0, 0};
    INDI::IHorizontalCoordinates horizontalCoords {parkAz, parkAlt};
    INDI::HorizontalToEquatorial(&horizontalCoords, &m_Location, ln_get_julian_from_sys(), &equatorialCoords);
    double lst = get_local_sidereal_time(m_Location.longitude);
    double ha = get_local_hour_angle(lst, equatorialCoords.rightascension);

    HourangleCoordsNP.s = IPS_OK;
    HourangleCoordsN[0].value = ha;
    HourangleCoordsN[1].value = equatorialCoords.declination;
    IDSetNumber(&HourangleCoordsNP, nullptr);

    if (isSimulation())
    {
        Goto(equatorialCoords.rightascension, equatorialCoords.declination);
    }
    else
    {
        if (setAPObjectAZ(PortFD, parkAz) < 0 || setAPObjectAlt(PortFD, parkAlt) < 0)
        {
            LOG_ERROR("Error setting Az/Alt.");
            return false;
        }

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = Slew(PortFD)))
        {
            LOGF_ERROR("Error Slewing to Az %s - Alt %s", AzStr, AltStr);
            slewError(err);
            return false;
        }
        lastAZ = parkAz;
        lastAL = parkAlt;
    }

    EqNP.setState(IPS_BUSY);
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking is in progress...");

    return true;
}

bool LX200AstroPhysicsV2::calcParkPosition(ParkPosition pos, double * parkAlt, double * parkAz)
{
    switch (pos)
    {
        // last unparked and park custom share enum 0
        case PARK_CUSTOM:
            LOG_ERROR("Called calcParkPosition with PARK_CUSTOM or PARK_LAST!");
            return false;
            break;

        case PARK_CURRENT:
            LOG_ERROR("Called calcParkPosition with PARK_CURRENT!");
            return false;
            break;

        // Park 1
        // Northern Hemisphere should be pointing at ALT=0 AZ=0 with scope on WEST side of pier
        // Southern Hemisphere should be pointing at ALT=0 AZ=180 with scope on WEST side of pier
        case PARK_PARK1:
            LOG_INFO("Computing PARK1 position...");
            *parkAlt = 0;
            *parkAz = LocationN[LOCATION_LATITUDE].value > 0 ? 359.1 : 180.1;
            break;

        // Park 2
        // Northern Hemisphere should be pointing at ALT=0 AZ=90 with scope pointing EAST
        // Southern Hemisphere should be pointing at ALT=0 AZ=90 with scope pointing EAST
        case PARK_PARK2:
            LOG_INFO("Computing PARK2 position...");
            *parkAlt = 0;
            *parkAz = 90;
            break;

        // Park 3
        // Northern Hemisphere should be pointing at ALT=LAT AZ=0 with scope pointing NORTH with CW down
        // Southern Hemisphere should be pointing at ALT=LAT AZ=180 with scope pointing SOUTH with CW down
        // wildi: the hour angle is undefined if AZ = 0,180 and ALT=LAT is chosen, adding .1 to Az sets PARK3
        //        as close as possible to to HA = -6 hours (CW down), valid for both hemispheres.
        case PARK_PARK3:
            *parkAlt = fabs(LocationN[LOCATION_LATITUDE].value);
            *parkAz = LocationN[LOCATION_LATITUDE].value > 0 ? 0.1 : 179.9;
            LOG_INFO("Computing PARK3 position");
            break;

        // Park 4
        // Northern Hemisphere should be pointing at ALT=0 AZ=180 with scope on EAST side of pier
        // Southern Hemisphere should be pointing at ALT=0 AZ=0 with scope on EAST side of pier
        case PARK_PARK4:
            LOG_INFO("Computing PARK4 position...");
            *parkAlt = 0;
            *parkAz = LocationN[LOCATION_LATITUDE].value > 0 ? 180.1 : 359.1;
            break;

        default:
            LOG_ERROR("Unknown park position!");
            return false;
            break;
    }

    LOGF_DEBUG("calcParkPosition: parkPos=%d parkAlt=%f parkAz=%f", pos, *parkAlt, *parkAz);

    return true;

}

bool LX200AstroPhysicsV2::UnPark()
{
    bool unpark_from_last_config = (PARK_LAST == IUFindOnSwitchIndex(&UnparkFromSP));
    double unparkAlt = 0, unparkAz = 0;

    if (!unpark_from_last_config)
    {
        ParkPosition unparkfromPos = static_cast<ParkPosition>(IUFindOnSwitchIndex(&UnparkFromSP));
        LOGF_DEBUG("UnPark: park position = %d from current driver", unparkfromPos);
        if (!calcParkPosition(unparkfromPos, &unparkAlt, &unparkAz))
        {
            LOG_ERROR("UnPark: Error calculating unpark position!");
            UnparkFromSP.s = IPS_ALERT;
            IDSetSwitch(&UnparkFromSP, nullptr);
            return false;
        }
        LOGF_DEBUG("UnPark: parkPos=%d parkAlt=%f parkAz=%f", unparkfromPos, unparkAlt, unparkAz);
    }

    bool isAPParked = true;
    if(!isSimulation())
    {
        if (!IsMountParked(&isAPParked))
        {
            LOG_WARN("UnPark:could not determine AP park status");
            UnparkFromSP.s = IPS_ALERT;
            IDSetSwitch(&UnparkFromSP, nullptr);
            return false;
        }

        if(!isAPParked)
        {
            LOG_WARN("UnPark: AP mount status: unparked, park first");
            UnparkFromSP.s = IPS_ALERT;
            IDSetSwitch(&UnparkFromSP, nullptr);
            return false;
        }

        if (APUnParkMount(PortFD) < 0)
        {
            IUResetSwitch(&ParkSP);
            ParkS[0].s = ISS_ON;
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, nullptr);
            LOG_ERROR("UnParking AP mount failed.");
            return false;
        }

        SetParked(false);
        // Stop :Q#
        if ( abortSlew(PortFD) < 0)
        {
            IUResetSwitch(&ParkSP);
            ParkS[0].s = ISS_ON;
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, nullptr);
            LOG_WARN("Abort motion Failed");
            return false;
        }
        SetTrackEnabled(true);
        TrackState = SCOPE_IDLE;
    }
    else
    {
        SetParked(false);
        SetTrackEnabled(false);
        TrackState = SCOPE_IDLE;
    }

    if (!unpark_from_last_config)
    {
        INDI::IEquatorialCoordinates equatorialCoords {0, 0};
        INDI::IHorizontalCoordinates horizontalCoords {unparkAz, unparkAlt};
        INDI::HorizontalToEquatorial(&horizontalCoords, &m_Location, ln_get_julian_from_sys(), &equatorialCoords);

        char AzStr[16], AltStr[16];
        fs_sexa(AzStr, unparkAz, 2, 3600);
        fs_sexa(AltStr, unparkAlt, 2, 3600);
        char RaStr[16], DecStr[16];
        fs_sexa(RaStr, equatorialCoords.rightascension, 2, 3600);
        fs_sexa(DecStr, equatorialCoords.declination, 2, 3600);

        double lst = get_local_sidereal_time(m_Location.longitude);
        double ha = get_local_hour_angle(lst, equatorialCoords.rightascension);
        char HaStr[16];
        fs_sexa(HaStr, ha, 2, 3600);
        LOGF_INFO("UnPark: Current parking position Az (%s) Alt (%s), HA (%s) RA (%s) Dec (%s)", AzStr, AltStr, HaStr,
                  RaStr, DecStr);

        HourangleCoordsNP.s = IPS_OK;
        HourangleCoordsN[0].value = ha;
        HourangleCoordsN[1].value = equatorialCoords.declination;
        IDSetNumber(&HourangleCoordsNP, nullptr);


        // If we are not using PARK_LAST, then we're unparking from a pre-defined position, and this is the
        // only time we should use the full :CM "Fully Calibrate" sync command.
        bool success = APSync(equatorialCoords.rightascension, equatorialCoords.declination, false);
        if(!success)
        {
            LOG_WARN("Could not sync mount");
            return false;
        }
        else
        {
            LOGF_INFO("UnPark: Sync'd (:CM) to RA/DEC: %f %f", equatorialCoords.rightascension, equatorialCoords.declination);
        }
    }

    if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
    {
        MovementNSSP.s = IPS_IDLE;
        MovementWESP.s = IPS_IDLE;
        EqNP.setState(IPS_IDLE);
        IUResetSwitch(&MovementNSSP);
        IUResetSwitch(&MovementWESP);
        IDSetSwitch(&MovementNSSP, nullptr);
        IDSetSwitch(&MovementWESP, nullptr);
    }

    UnparkFromSP.s = IPS_OK;
    IDSetSwitch(&UnparkFromSP, nullptr);
    // SlewRateS is used as the MoveTo speed
    int err;
    int switch_nr = IUFindOnSwitchIndex(&SlewRateSP);
    if (!isSimulation() && (err = selectAPV2CenterRate(PortFD, switch_nr, rateTable)) < 0)
    {
        LOGF_ERROR("Error setting center (MoveTo) rate (%d).", err);
        return false;
    }

    SlewRateSP.s = IPS_OK;
    IDSetSwitch(&SlewRateSP, nullptr);

    // APSlewSpeedsS defines the Slew (GOTO) speeds valid on the AP mounts
    switch_nr = IUFindOnSwitchIndex(&APSlewSpeedSP);
    if (!isSimulation() && (err = selectAPSlewRate(PortFD, switch_nr)) < 0)
    {
        LOGF_ERROR("Error setting slew to rate (%d).", err);
        return false;
    }

    APSlewSpeedSP.s = IPS_OK;
    IDSetSwitch(&APSlewSpeedSP, nullptr);

    LOG_DEBUG("UnPark: Mount unparked successfully");

    return true;
}

bool LX200AstroPhysicsV2::SetCurrentPark()
{
    return true;
}

bool LX200AstroPhysicsV2::SetDefaultPark()
{
    return true;
}

void LX200AstroPhysicsV2::syncSideOfPier()
{
    const char *cmd = "#:pS#";
    // Response
    char response[16] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return;
    }

    // Read Side
    if ((rc = tty_read_section(PortFD, response, '#', 3, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return;
    }

    response[nbytes_read - 1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    if (!strcmp(response, "East"))
        setPierSide(INDI::Telescope::PIER_EAST);
    else if (!strcmp(response, "West"))
        setPierSide(INDI::Telescope::PIER_WEST);
    else
        LOGF_ERROR("Invalid pier side response from device-> %s", response);
}

bool LX200AstroPhysicsV2::saveConfigItems(FILE * fp)
{
    LX200Generic::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &APSlewSpeedSP);
    IUSaveConfigSwitch(fp, &APGuideSpeedSP);
    IUSaveConfigSwitch(fp, &ParkToSP);
    IUSaveConfigSwitch(fp, &UnparkFromSP);
    IUSaveConfigSwitch(fp, &TrackStateSP);

    return true;
}

bool LX200AstroPhysicsV2::SetTrackMode(uint8_t mode)
{
    int err = 0;

    LOGF_DEBUG("LX200AstroPhysicsV2::SetTrackMode(%d)", mode);

    if (mode == TRACK_CUSTOM)
    {
        if (!isSimulation() && (err = selectAPTrackingMode(PortFD, AP_TRACKING_SIDEREAL)) < 0)
        {
            LOGF_ERROR("Error setting tracking mode (%d).", err);
            return false;
        }

        return SetTrackRate(TrackRateN[AXIS_RA].value, TrackRateN[AXIS_DE].value);
    }

    if (!isSimulation() && (err = selectAPTrackingMode(PortFD, mode)) < 0)
    {
        LOGF_ERROR("Error setting tracking mode (%d).", err);
        return false;
    }

    return true;
}

bool LX200AstroPhysicsV2::SetTrackEnabled(bool enabled)
{
    bool rc;

    LOGF_DEBUG("LX200AstroPhysicsV2::SetTrackEnabled(%d)", enabled);

    rc = SetTrackMode(enabled ? IUFindOnSwitchIndex(&TrackModeSP) : AP_TRACKING_OFF);

    LOGF_DEBUG("LX200AstroPhysicsV2::SetTrackMode() returned %d", rc);

    return rc;
}

bool LX200AstroPhysicsV2::SetTrackRate(double raRate, double deRate)
{
    // Convert to arcsecs/s to AP sidereal multiplier
    /*
    :RR0.0000#      =       normal sidereal tracking in RA - similar to  :RT2#
    :RR+1.0000#     =       1 + normal sidereal     =       2X sidereal
    :RR+9.0000#     =       9 + normal sidereal     =       10X sidereal
    :RR-1.0000#     =       normal sidereal - 1     =       0 or Stop - similar to  :RT9#
    :RR-11.0000#    =       normal sidereal - 11    =       -10X sidereal (East at 10X)

    :RD0.0000#      =       normal zero rate for Dec.
    :RD5.0000#      =       5 + normal zero rate    =       5X sidereal clockwise from above - equivalent to South
    :RD-5.0000#     =       normal zero rate - 5    =       5X sidereal counter-clockwise from above - equivalent to North
    */

    double APRARate = (raRate - TRACKRATE_SIDEREAL) / TRACKRATE_SIDEREAL;
    double APDERate = deRate / TRACKRATE_SIDEREAL;

    if (!isSimulation())
    {
        if (setAPRATrackRate(PortFD, APRARate) < 0 || setAPDETrackRate(PortFD, APDERate) < 0)
            return false;
    }

    return true;
}

bool LX200AstroPhysicsV2::getUTFOffset(double * offset)
{
    if (isSimulation())
    {
        *offset = 3;
        return true;
    }

    return (getAPUTCOffset(PortFD, offset) == 0);
}

bool LX200AstroPhysicsV2::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    // If we are not guiding and we need to restore slew rate, then let's restore it.
    if (command == MOTION_START && GuideNSTID == 0 && rememberSlewRate >= 0)
    {
        ISState states[] = { ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF };
        states[rememberSlewRate] = ISS_ON;
        const char *names[] = { SlewRateS[0].name, SlewRateS[1].name,
                                SlewRateS[2].name, SlewRateS[3].name
                              };
        ISNewSwitch(SlewRateSP.device, SlewRateSP.name, states, const_cast<char **>(names), 4);
        rememberSlewRate = -1;
    }

    bool rc = LX200Generic::MoveNS(dir, command);
    return rc;
}

bool LX200AstroPhysicsV2::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    // If we are not guiding and we need to restore slew rate, then let's restore it.
    if (command == MOTION_START && GuideWETID == 0 && rememberSlewRate >= 0)
    {
        ISState states[] = { ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF };
        states[rememberSlewRate] = ISS_ON;
        const char *names[] = { SlewRateS[0].name, SlewRateS[1].name,
                                SlewRateS[2].name, SlewRateS[3].name
                              };
        ISNewSwitch(SlewRateSP.device, SlewRateSP.name, states, const_cast<char **>(names), 4);
        rememberSlewRate = -1;
    }

    bool rc = LX200Generic::MoveWE(dir, command);
    return rc;
}

bool LX200AstroPhysicsV2::GuideNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (!isAPReady())
        return false;

    // restore guide rate
    selectAPGuideRate(PortFD, IUFindOnSwitchIndex(&APGuideSpeedSP));

    bool rc = LX200Generic::MoveNS(dir, command);
    return rc;
}

bool LX200AstroPhysicsV2::GuideWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (!isAPReady())
        return false;

    // restore guide rate
    selectAPGuideRate(PortFD, IUFindOnSwitchIndex(&APGuideSpeedSP));

    bool rc = LX200Generic::MoveWE(dir, command);
    return rc;
}

