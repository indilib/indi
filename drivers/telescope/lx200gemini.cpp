/*
    Losmandy Gemini INDI driver

    Copyright (C) 2017 Jasem Mutlaq
    Copyright (C) 2018 Eric Vickery

    Difference from LX200 Generic:

    1. Added Side of Pier
    2. Reimplemented isSlewComplete to use :Gv# since it is more reliable
    3. Support networked connections.
    4. Side of pier
    5. Variable GOTO/SLEW/MOVE speeds.

    v1.4:

    + Added MOUNT_STATE_UPDATE_FREQ to reduce number of calls to updateMountState to reduce traffic
    + All TCIFLUSH --> TCIOFLUSH to make sure both pipes are flushed since we received logs with mismatched traffic.

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
#include "lx200gemini.h"

#include "indicom.h"
#include "lx200driver.h"
#include "connectionplugins/connectioninterface.h"
#include "connectionplugins/connectiontcp.h"

#include <cstring>
#include <termios.h>

#define LX200_TIMEOUT 5 /* FD timeout in seconds */

#define MANUAL_SLEWING_SPEED_ID        120
#define GOTO_SLEWING_SPEED_ID          140
#define MOVE_SPEED_ID                  145 // L5
#define GUIDING_SPEED_ID               150
#define GUIDING_SPEED_RA_ID            151 // L5
#define GUIDING_SPEED_DEC_ID           152 // L5
#define CENTERING_SPEED_ID             170
#define SERVO_POINTING_PRECISION_ID    401 // L6
#define PEC_MAX_STEPS_ID               27
#define PEC_COUNTER_ID                 501
#define PEC_STATUS_ID                  509
#define PEC_START_TRAINING_ID          530 // L5
#define PEC_ABORT_TRAINING_ID          535 // L5
#define PEC_REPLAY_ON_ID               531 // L5
#define PEC_REPLAY_OFF_ID              532 // L5
#define PEC_ENABLE_AT_BOOT_ID          508 // L5.2
#define PEC_GUIDING_SPEED_ID           502
#define SERVO_FIRMWARE                 400 // L6 <ra>;<dec> (L6)
#define FLIP_POINT_EAST_ID             227 // L6
#define FLIP_POINT_WEST_ID             228 // L6
#define FLIP_POINTS_ENABLED_ID         229 // L6
#define SET_SAFETY_LIMIT_TO_CURRENT_ID 220
#define EAST_SAFETY_LIMIT_ID           221
#define WEST_SAFETY_LIMIT_ID           222
#define WEST_GOTO_SAFETY_LIMIT_ID      223




LX200Gemini::LX200Gemini()
{
    setVersion(1, 6);

    setLX200Capability(LX200_HAS_SITES | LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_FLIP | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_PEC,
                           4);
}


const char *LX200Gemini::getDefaultName()
{
    return "Losmandy Gemini";
}

bool LX200Gemini::Connect()
{
    Connection::Interface *activeConnection = getActiveConnection();

    if (!activeConnection->name().compare("CONNECTION_TCP"))
    {
        tty_set_gemini_udp_format(1);
    }

    return LX200Generic::Connect();
}

void LX200Gemini::ISGetProperties(const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    // Read config from file
    int index = 0;
    if (IUGetConfigOnSwitch(&StartupModeSP, &index) == 0)
    {
        IUResetSwitch(&StartupModeSP);
        StartupModeSP.sp[index].s = ISS_ON;
        defineProperty(&StartupModeSP);
    }
}

bool LX200Gemini::initProperties()
{
    LX200Generic::initProperties();

    // Show firmware
    IUFillText(&VersionT[FIRMWARE_DATE], "Build Date", "", "");
    IUFillText(&VersionT[FIRMWARE_TIME], "Build Time", "", "");
    IUFillText(&VersionT[FIRMWARE_LEVEL], "Software Level", "", "");
    IUFillText(&VersionT[FIRMWARE_NAME], "Product Name", "", "");
    IUFillTextVector(&VersionTP, VersionT, 5, getDeviceName(), "Firmware Info", "", CONNECTION_TAB, IP_RO, 0, IPS_IDLE);

    // Park Option
    IUFillSwitch(&ParkSettingsS[PARK_HOME], "HOME", "Home", ISS_ON);
    IUFillSwitch(&ParkSettingsS[PARK_STARTUP], "STARTUP", "Startup", ISS_OFF);
    IUFillSwitch(&ParkSettingsS[PARK_ZENITH], "ZENITH", "Zenith", ISS_OFF);
    IUFillSwitchVector(&ParkSettingsSP, ParkSettingsS, 3, getDeviceName(), "PARK_SETTINGS", "Park Settings",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&StartupModeS[COLD_START], "COLD_START", "Cold", ISS_ON);
    IUFillSwitch(&StartupModeS[PARK_STARTUP], "WARM_START", "Warm", ISS_OFF);
    IUFillSwitch(&StartupModeS[PARK_ZENITH], "WARM_RESTART", "Restart", ISS_OFF);
    IUFillSwitchVector(&StartupModeSP, StartupModeS, 3, getDeviceName(), "STARTUP_MODE", "Startup Mode",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillNumber(&ManualSlewingSpeedN[0], "MANUAL_SLEWING_SPEED", "Manual Slewing Speed", "%g", 20, 2000., 10., 800);
    IUFillNumberVector(&ManualSlewingSpeedNP, ManualSlewingSpeedN, 1, getDeviceName(), "MANUAL_SLEWING_SPEED",
                       "Slew Speed", MOTION_TAB, IP_RW,  0, IPS_IDLE);

    IUFillNumber(&GotoSlewingSpeedN[0], "GOTO_SLEWING_SPEED", "Goto Slewing Speed", "%g", 20, 2000., 10., 800);
    IUFillNumberVector(&GotoSlewingSpeedNP, GotoSlewingSpeedN, 1, getDeviceName(), "GOTO_SLEWING_SPEED",
                       "Goto Speed", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&MoveSpeedN[0], "MOVE_SPEED", "Move Speed", "%g", 20, 2000., 10., 10);
    IUFillNumberVector(&MoveSpeedNP, MoveSpeedN, 1, getDeviceName(), "MOVE_SLEWING_SPEED",
                       "Move Speed", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&GuidingSpeedBothN[GUIDING_BOTH], "GUIDING_SPEED", "Guide Speed RA/DEC", "%g", 0.2, 0.8, 0.1, 0.5);
    IUFillNumberVector(&GuidingSpeedBothNP, GuidingSpeedBothN, 1, getDeviceName(), "GUIDING_SLEWING_SPEED_BOTH",
                       "Guide Speed",
                       GUIDE_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&GuidingSpeedN[GUIDING_WE], "GUIDE_RATE_WE", "W/E Rate", "%g", 0.2, 0.8, 0.1, 0.5);
    IUFillNumber(&GuidingSpeedN[GUIDING_NS], "GUIDE_RATE_NS", "N/S Rate", "%g", 0.2, 0.8, 0.1, 0.5);
    IUFillNumberVector(&GuidingSpeedNP, GuidingSpeedN, 2, getDeviceName(), "GUIDE_RATE",
                       "Guide Speed", GUIDE_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&CenteringSpeedN[0], "CENTERING_SPEED", "Centering Speed", "%g", 20, 2000., 10., 10);
    IUFillNumberVector(&CenteringSpeedNP, CenteringSpeedN, 1, getDeviceName(), "CENTERING_SLEWING_SPEED",
                       "Center Speed", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    TrackModeSP[GEMINI_TRACK_SIDEREAL].fill("TRACK_SIDEREAL", "Sidereal", ISS_ON);
    TrackModeSP[GEMINI_TRACK_KING].fill("TRACK_CUSTOM", "King", ISS_OFF);
    TrackModeSP[GEMINI_TRACK_LUNAR].fill("TRACK_LUNAR", "Lunar", ISS_OFF);
    TrackModeSP[GEMINI_TRACK_SOLAR].fill("TRACK_SOLAR", "Solar", ISS_OFF);

    //PEC
    IUFillSwitch(&PECControlS[PEC_START_TRAINING], "PEC_START_TRAINING", "Start Training", ISS_OFF);
    IUFillSwitch(&PECControlS[PEC_ABORT_TRAINING], "PEC_ABORT_TRAINING", "Abort Training", ISS_OFF);
    IUFillSwitchVector(&PECControlSP, PECControlS, 2, getDeviceName(), "PEC_COMMANDS",
                       "PEC Cmds", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillText(&PECStateT[PEC_STATUS_ACTIVE], "PEC_STATUS_ACTIVE", "PEC active", "");
    IUFillText(&PECStateT[PEC_STATUS_FRESH_TRAINED], "PEC_STATUS_FRESH_TRAINED", "PEC freshly trained", "");
    IUFillText(&PECStateT[PEC_STATUS_TRAINING_IN_PROGRESS], "PEC_STATUS_TRAINING_IN_PROGRESS", "PEC training in progress", "");
    IUFillText(&PECStateT[PEC_STATUS_TRAINING_COMPLETED], "PEC_STATUS_TRAINING_COMPLETED", "PEC training just completed", "");
    IUFillText(&PECStateT[PEC_STATUS_WILL_TRAIN], "PEC_STATUS_WILL_TRAIN", "PEC will train soon", "");

    IUFillText(&PECStateT[PEC_STATUS_DATA_AVAILABLE], "PEC_STATUS_DATA_AVAILABLE", "PEC Data available", "");
    IUFillTextVector(&PECStateTP, PECStateT, 6, getDeviceName(), "PEC_STATE",
                     "PEC State", MOTION_TAB, IP_RO, 0, IPS_OK);

    IUFillText(&PECCounterT[0], "PEC_COUNTER", "Counter", "");
    IUFillTextVector(&PECCounterTP, PECCounterT, 1, getDeviceName(), "PEC_COUNTER",
                     "PEC Counter", MOTION_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&PECMaxStepsN[0], "PEC_MAX_STEPS", "PEC MaxSteps", "%f", 0, 4294967296, 1, 0);
    IUFillNumberVector(&PECMaxStepsNP, PECMaxStepsN, 1, getDeviceName(), "PEC_MAX_STEPS",
                       "PEC Steps", MOTION_TAB, IP_RO, 0, IPS_IDLE);

    IUFillSwitch(&ServoPrecisionS[SERVO_RA], "SERVO_RA", "4x RA Precision", ISS_OFF);
    IUFillSwitch(&ServoPrecisionS[SERVO_DEC], "SERVO_DEC", "4x DEC Precision", ISS_OFF);
    IUFillSwitchVector(&ServoPrecisionSP, ServoPrecisionS, 2, getDeviceName(), "SERVO",
                       "Servo", MOTION_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    IUFillNumber(&PECGuidingSpeedN[0], "PEC_GUIDING_SPEED", "PEC GuidingSpeed", "%f", 0.2, 0.8, 0.1, 0);
    IUFillNumberVector(&PECGuidingSpeedNP, PECGuidingSpeedN, 1, getDeviceName(), "PEC_GUIDING_SPEED",
                       "PEC Speed", MOTION_TAB, IP_RO, 0, IPS_IDLE);

    IUFillSwitch(&PECEnableAtBootS[0], "ENABLE_PEC_AT_BOOT", "Enable PEC at boot", ISS_OFF);
    IUFillSwitchVector(&PECEnableAtBootSP, PECEnableAtBootS, 1, getDeviceName(), "ENABLE_PEC_AT_BOOT",
                       "PEC Setting", MOTION_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // Flip points
    IUFillSwitch(&FlipControlS[FLIP_EAST_CONTROL], "FLIP_EAST_CONTROL", "East", ISS_OFF);
    IUFillSwitch(&FlipControlS[FLIP_WEST_CONTROL], "FLIP_WEST_CONTROL", "West", ISS_OFF);
    IUFillSwitchVector(&FlipControlSP, FlipControlS, 2, getDeviceName(), "FLIP_CONTROL",
                       "Flip Control", MOTION_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    IUFillNumber(&FlipPositionN[FLIP_EAST_VALUE], "FLIP_EAST_VALUE", "East (dd:mm)", "%060.4m", 0, 90, 0, 0.0);
    IUFillNumber(&FlipPositionN[FLIP_WEST_VALUE], "FLIP_WEST_VALUE", "West (dd:mm)", "%060.4m", 0, 90, 0, 0.0);
    IUFillNumberVector(&FlipPositionNP, FlipPositionN, 2, getDeviceName(), "FLIP_POSITION",
                       "Flip Position", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    // Refraction
    IUFillSwitch(&RefractionControlS[0], "REF_COORDS", "Refract Coords", ISS_OFF);
    IUFillSwitchVector(&RefractionControlSP, RefractionControlS, 1, getDeviceName(), "REFRACT",
                       "Refraction", MOTION_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

    // Safety
    IUFillSwitch(&SetSafetyLimitToCurrentS[0], "SET_SAFETY", "Set to Current", ISS_OFF);
    IUFillSwitchVector(&SetSafetyLimitToCurrentSP, SetSafetyLimitToCurrentS, 1, getDeviceName(), "SET_CURR_SAFETY",
                       "Safety Limit", MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&SafetyLimitsN[EAST_SAFETY], "EAST_SAFTEY", "East Safety(dd:mm)", "%060.4m", 0, 180, 0, 0.0);
    IUFillNumber(&SafetyLimitsN[WEST_SAFETY], "WEST_SAFTEY", "West Safety(dd:mm)", "%060.4m", 0, 180, 0, 0.0);
    IUFillNumber(&SafetyLimitsN[WEST_GOTO], "WEST_GOTO", "West Goto (dd:mm)", "%060.4m", 0, 180, 0, 0.0);
    IUFillNumberVector(&SafetyLimitsNP, SafetyLimitsN, 3, getDeviceName(), "SAFETY_LIMITS",
                       "Limits", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    tcpConnection->setConnectionType(Connection::TCP::TYPE_UDP);
    tcpConnection->setDefaultPort(11110);

    gemini_software_level_ = 0.0;

    return true;
}

bool LX200Gemini::getRefractionJNOW(int &data)
{
    if (isSimulation())
        return true;

    // Response
    char response[2] =  { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;


    tcflush(PortFD, TCIFLUSH);

    const char *cmd = ":p?#";
    data = 0;
    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    // Read response
    if ((rc = tty_read(PortFD, response, 1, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return false;
    }
    tcflush(PortFD, TCIFLUSH);
    response[1] = '\0';

    data = atoi(response);
    return true;
}

bool LX200Gemini::getRefraction(bool &on)
{
    if (isSimulation())
        return true;

    int data = 0;

    bool success = getRefractionJNOW(data);

    if(data & 2)
    {
        on = true;
        if((data != 2) && (data != 0))
        {
            LOGF_WARN("Mount Precess being reset to JNOW: %i", data);
        }
        return setRefraction(2);
    }
    else
    {
        on = false;
        if((data != 2) && (data != 0))
        {
            LOGF_WARN("Mount Precess being reset to JNOW: %i", data);
        }
        return setRefraction(0);
    }

    return success;
}

bool LX200Gemini::setRefraction(int data)
{
    if (isSimulation())
        return true;

    int rc = 0, nbytes_written = 0;

    tcflush(PortFD, TCIFLUSH);

    char cmd[5] = { 0 };
    snprintf(cmd, 5, ":p%i#", data);
    data = 0;
    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }
    tcflush(PortFD, TCIFLUSH);

    return true;
}

bool LX200Gemini::setRefraction(bool on)
{
    if (isSimulation())
        return true;

    int data;

    getRefractionJNOW(data);

    if(on)
    {
        if((data != 2) && (data != 0))
        {
            LOGF_WARN("Mount Precess being reset to JNOW %i ", data);
        }
        return setRefraction(2);
    }
    else
    {
        if((data != 2) && (data != 0))
        {
            LOGF_WARN("Mount Precess being reset to JNOW %i", data);
        }
        return setRefraction(0);
    }
}

void LX200Gemini::syncState()
{
    const int MAX_VALUE_LENGTH = 32;
    char value[MAX_VALUE_LENGTH] = {0};
    char value2[MAX_VALUE_LENGTH] = {0};
    char value3[MAX_VALUE_LENGTH] = {0};

    if (gemini_software_level_ >= 5.0)
    {
        if (getGeminiProperty(GUIDING_SPEED_RA_ID, value))
        {
            float guiding_value;
            sscanf(value, "%f", &guiding_value);
            GuidingSpeedN[GUIDING_WE].value = guiding_value;
            IDSetNumber(&GuidingSpeedNP, nullptr);
        }
        else
        {
            GuidingSpeedNP.s = IPS_ALERT;
            IDSetNumber(&GuidingSpeedNP, nullptr);
        }
        if (getGeminiProperty(GUIDING_SPEED_DEC_ID, value))
        {
            float guiding_value;
            sscanf(value, "%f", &guiding_value);
            GuidingSpeedN[GUIDING_NS].value = guiding_value;
            IDSetNumber(&GuidingSpeedNP, nullptr);
        }
        else
        {
            GuidingSpeedNP.s = IPS_ALERT;
            IDSetNumber(&GuidingSpeedNP, nullptr);
        }
    }
    if (gemini_software_level_ >= 5.2)
    {
        if (getGeminiProperty(PEC_ENABLE_AT_BOOT_ID, value))
        {
            uint32_t pec_at_boot_value;
            sscanf(value, "%u", &pec_at_boot_value);
            if(pec_at_boot_value)
            {
                PECEnableAtBootS[0].s = ISS_ON;
            }
            else
            {
                PECEnableAtBootS[0].s = ISS_OFF;
            }
            PECEnableAtBootSP.s = IPS_OK;
        }
        else
        {
            PECEnableAtBootSP.s = IPS_ALERT;
        }
        IDSetSwitch(&PECEnableAtBootSP, nullptr);
    }
    if(gemini_software_level_ >= 6.0)
    {
        if (getGeminiProperty(SERVO_POINTING_PRECISION_ID, value))
        {
            uint8_t servo_value;
            sscanf(value, "%c", &servo_value);
            if(servo_value & RA_PRECISION_ENABLED)
            {
                ServoPrecisionS[SERVO_RA].s = ISS_ON;
            }
            else
            {
                ServoPrecisionS[SERVO_RA].s = ISS_OFF;
            }
            if(servo_value & DEC_PRECISION_ENABLED)
            {
                ServoPrecisionS[SERVO_DEC].s = ISS_ON;
            }
            else
            {
                ServoPrecisionS[SERVO_DEC].s = ISS_OFF;
            }
            ServoPrecisionSP.s = IPS_OK;
            IDSetSwitch(&ServoPrecisionSP, nullptr);

        }
        else
        {
            ServoPrecisionSP.s = IPS_ALERT;
            IDSetSwitch(&ServoPrecisionSP, nullptr);
        }
        if(getGeminiProperty(FLIP_POINTS_ENABLED_ID, value))
        {
            char valueString[32] = {0};
            uint32_t flip_value = 0;
            sscanf(value, "%u", &flip_value);
            snprintf(valueString, 32, "%i", flip_value);

            if(flip_value)
            {
                if(flip_value & FLIP_EAST)
                {
                    FlipControlS[FLIP_EAST_CONTROL].s = ISS_ON;
                }
                else
                {
                    FlipControlS[FLIP_EAST_CONTROL].s = ISS_OFF;
                }
                if(flip_value & FLIP_WEST)
                {
                    FlipControlS[FLIP_WEST_CONTROL].s = ISS_ON;
                }
                else
                {
                    FlipControlS[FLIP_WEST_CONTROL].s = ISS_OFF;
                }
            }
            else
            {
                FlipControlS[FLIP_EAST_CONTROL].s = ISS_OFF;
                FlipControlS[FLIP_WEST_CONTROL].s = ISS_OFF;
            }
            FlipControlSP.s = IPS_OK;
            IDSetSwitch(&FlipControlSP, nullptr);
        }
        else
        {
            FlipControlSP.s = IPS_ALERT;
            IDSetSwitch(&FlipControlSP, nullptr);
        }
        if(getGeminiProperty(FLIP_POINT_EAST_ID, value) &&
                getGeminiProperty(FLIP_POINT_WEST_ID, value2))
        {
            double eastSexa;
            double westSexa;

            f_scansexa(value, &eastSexa);
            FlipPositionN[FLIP_EAST_VALUE].value = eastSexa;

            f_scansexa(value2, &westSexa);
            FlipPositionN[FLIP_WEST_VALUE].value = westSexa;

            FlipPositionNP.s = IPS_OK;
            IDSetNumber(&FlipPositionNP, nullptr);
        }
        else
        {
            FlipPositionNP.s = IPS_ALERT;
            IDSetNumber(&FlipPositionNP, nullptr);
        }
    }

    if (getGeminiProperty(PEC_MAX_STEPS_ID, value))
    {
        float max_value;
        sscanf(value, "%f", &max_value);
        PECMaxStepsN[0].value = max_value;
        IDSetNumber(&PECMaxStepsNP, nullptr);
    }
    else
    {
        PECMaxStepsNP.s = IPS_ALERT;
        IDSetNumber(&PECMaxStepsNP, nullptr);
    }
    if (getGeminiProperty(PEC_COUNTER_ID, value))
    {
        char valueString[32] = {0};
        uint32_t pec_counter = 0;
        sscanf(value, "%u", &pec_counter);
        snprintf(valueString, 32, "%i", pec_counter);

        IUSaveText(&PECCounterT[0], valueString);
        IDSetText(&PECCounterTP, nullptr);
    }
    else
    {
        PECCounterTP.s = IPS_ALERT;
        IDSetText(&PECCounterTP, nullptr);
    }
    if (getGeminiProperty(PEC_GUIDING_SPEED_ID, value))
    {
        float guiding_value;
        sscanf(value, "%f", &guiding_value);
        PECGuidingSpeedN[0].value = guiding_value;
        IDSetNumber(&PECGuidingSpeedNP, nullptr);
    }
    else
    {
        PECGuidingSpeedNP.s = IPS_ALERT;
        IDSetNumber(&PECGuidingSpeedNP, nullptr);
    }
    if (getGeminiProperty(GUIDING_SPEED_ID, value))
    {
        float guiding_value;
        sscanf(value, "%f", &guiding_value);
        GuidingSpeedBothN[GUIDING_BOTH].value = guiding_value;
        IDSetNumber(&GuidingSpeedBothNP, nullptr);
    }
    else
    {
        GuidingSpeedBothNP.s = IPS_ALERT;
        IDSetNumber(&GuidingSpeedBothNP, nullptr);
    }
    if (getGeminiProperty(PEC_STATUS_ID, value))
    {
        uint32_t pec_status = 0;
        sscanf(value, "%u", &pec_status);
        if(pec_status & 1)   // PEC_ACTIVE
        {
            IUSaveText(&PECStateT[PEC_STATUS_ACTIVE], "Yes");
            setPECState(PEC_ON);
        }
        else
        {
            IUSaveText(&PECStateT[PEC_STATUS_ACTIVE], "No");
            setPECState(PEC_OFF);
        }
        if(pec_status & 2)   // Freshly_Trained
        {
            IUSaveText(&PECStateT[PEC_STATUS_FRESH_TRAINED], "Yes");
        }
        else
        {
            IUSaveText(&PECStateT[PEC_STATUS_FRESH_TRAINED], "No");
        }
        if(pec_status & 4)   // Training_In_Progress
        {
            IUSaveText(&PECStateT[PEC_STATUS_TRAINING_IN_PROGRESS], "Yes");
        }
        else
        {
            IUSaveText(&PECStateT[PEC_STATUS_TRAINING_IN_PROGRESS], "No");
        }
        if(pec_status & 8)   // Training_just_completed
        {
            IUSaveText(&PECStateT[PEC_STATUS_TRAINING_COMPLETED], "Yes");
        }
        else
        {
            IUSaveText(&PECStateT[PEC_STATUS_TRAINING_COMPLETED], "No");
        }
        if(pec_status & 16)   // Training will start soon
        {
            IUSaveText(&PECStateT[PEC_STATUS_WILL_TRAIN], "Yes");
        }
        else
        {
            IUSaveText(&PECStateT[PEC_STATUS_WILL_TRAIN], "No");
        }
        if(pec_status & 32)   // PEC Data Available
        {
            IUSaveText(&PECStateT[PEC_STATUS_DATA_AVAILABLE], "Yes");
        }
        else
        {
            IUSaveText(&PECStateT[PEC_STATUS_DATA_AVAILABLE], "No");
        }
        IDSetText(&PECStateTP, nullptr);
    }
    else
    {
        PECStateTP.s = IPS_ALERT;
        IDSetText(&PECStateTP, nullptr);
    }

    if(getGeminiProperty(EAST_SAFETY_LIMIT_ID, value) &&
            getGeminiProperty(WEST_SAFETY_LIMIT_ID, value2) &&
            getGeminiProperty(WEST_GOTO_SAFETY_LIMIT_ID, value3))
    {
        double eastSafeSexa;
        double westSafeSexa;
        double westGotoSexa;

        f_scansexa(value, &eastSafeSexa);
        SafetyLimitsN[EAST_SAFETY].value = eastSafeSexa;

        f_scansexa(value2, &westSafeSexa);
        SafetyLimitsN[WEST_SAFETY].value = westSafeSexa;

        f_scansexa(value3, &westGotoSexa);
        SafetyLimitsN[WEST_GOTO].value = westGotoSexa;

        SafetyLimitsNP.s = IPS_OK;
        IDSetNumber(&SafetyLimitsNP, nullptr);
    }
    else
    {
        SafetyLimitsNP.s = IPS_ALERT;
        IDSetNumber(&SafetyLimitsNP, nullptr);
    }
}


bool LX200Gemini::updateProperties()
{
    const int MAX_VALUE_LENGTH = 32;
    LX200Generic::updateProperties();

    if (isConnected())
    {
        uint32_t speed = 0;
        char value[MAX_VALUE_LENGTH] = {0};
        char value2[MAX_VALUE_LENGTH] = {0};
        char value3[MAX_VALUE_LENGTH] = {0};
        if (!isSimulation())
        {
            VersionTP.tp[FIRMWARE_DATE].text = new char[64];
            getVersionDate(PortFD, VersionTP.tp[FIRMWARE_DATE].text);
            VersionTP.tp[FIRMWARE_TIME].text = new char[64];
            getVersionTime(PortFD, VersionTP.tp[FIRMWARE_TIME].text);
            VersionTP.tp[FIRMWARE_LEVEL].text = new char[64];
            getVersionNumber(PortFD, VersionTP.tp[FIRMWARE_LEVEL].text);
            VersionTP.tp[FIRMWARE_NAME].text = new char[128];
            getProductName(PortFD, VersionTP.tp[FIRMWARE_NAME].text);
            sscanf(VersionTP.tp[FIRMWARE_LEVEL].text, "%f", &gemini_software_level_);
            IDSetText(&VersionTP, nullptr);
        }
        defineProperty(&VersionTP);
        defineProperty(&ParkSettingsSP);

        if (gemini_software_level_ < 5.0)
        {
            deleteProperty(PECStateSP);
        }
        if (gemini_software_level_ >= 5.2)
        {
            if(getGeminiProperty(PEC_ENABLE_AT_BOOT_ID, value))
            {
                uint32_t pec_at_boot_value;
                sscanf(value, "%i", &pec_at_boot_value);
                if(pec_at_boot_value)
                {
                    PECEnableAtBootS[0].s = ISS_ON;
                }
                else
                {
                    PECEnableAtBootS[0].s = ISS_OFF;
                }
                PECEnableAtBootSP.s = IPS_OK;
                IDSetSwitch(&PECEnableAtBootSP, nullptr);
            }
            else
            {
                PECEnableAtBootSP.s = IPS_ALERT;
                IDSetSwitch(&PECEnableAtBootSP, nullptr);
            }
            defineProperty(&PECEnableAtBootSP);

        }
        if (getGeminiProperty(PEC_GUIDING_SPEED_ID, value))
        {
            float guiding_speed_value;
            sscanf(value, "%f", &guiding_speed_value);
            PECGuidingSpeedN[0].value = guiding_speed_value;
            PECGuidingSpeedNP.s = IPS_OK;
            IDSetNumber(&PECGuidingSpeedNP, nullptr);
        }
        else
        {
            PECGuidingSpeedNP.s = IPS_ALERT;
            IDSetNumber(&PECGuidingSpeedNP, nullptr);
        }
        defineProperty(&PECGuidingSpeedNP);
        if (gemini_software_level_ >= 5.0)
        {
            if(getGeminiProperty(PEC_COUNTER_ID, value))
            {
                char valueString[32] = {0};
                uint32_t pec_counter = 0;
                sscanf(value, "%u", &pec_counter);
                snprintf(valueString, 32, "%i", pec_counter);
                IUSaveText(&PECCounterT[0], valueString);
                PECControlSP.s = IPS_OK;
                IDSetSwitch(&PECControlSP, nullptr);
            }
            else
            {
                PECControlSP.s = IPS_ALERT;
                IDSetSwitch(&PECControlSP, nullptr);
            }
            defineProperty(&PECControlSP);
            defineProperty(&PECCounterTP);
        }
        if (getGeminiProperty(PEC_MAX_STEPS_ID, value))
        {
            float max_steps_value;
            sscanf(value, "%f", &max_steps_value);
            PECMaxStepsN[0].value = max_steps_value;
            PECMaxStepsNP.s = IPS_OK;
            IDSetNumber(&PECMaxStepsNP, nullptr);
        }
        else
        {
            PECMaxStepsNP.s = IPS_ALERT;
            IDSetNumber(&PECMaxStepsNP, nullptr);
        }
        defineProperty(&PECMaxStepsNP);
        if (getGeminiProperty(PEC_STATUS_ID, value))
        {
            uint32_t pec_status = 0;
            sscanf(value, "%u", &pec_status);
            if(pec_status & 1)   // PEC_ACTIVE
            {
                IUSaveText(&PECStateT[PEC_STATUS_ACTIVE], "Yes");
                setPECState(PEC_ON);
            }
            else
            {
                IUSaveText(&PECStateT[PEC_STATUS_ACTIVE], "No");
                setPECState(PEC_OFF);
            }

            if(pec_status & 2)   // Freshly_Trained
            {
                IUSaveText(&PECStateT[PEC_STATUS_FRESH_TRAINED], "Yes");
            }
            else
            {
                IUSaveText(&PECStateT[PEC_STATUS_FRESH_TRAINED], "No");
            }

            if(pec_status & 4)   // Training_In_Progress
            {
                IUSaveText(&PECStateT[PEC_STATUS_TRAINING_IN_PROGRESS], "Yes");
            }
            else
            {
                IUSaveText(&PECStateT[PEC_STATUS_TRAINING_IN_PROGRESS], "No");
            }
            if(pec_status & 6)   // Training_just_completed
            {
                IUSaveText(&PECStateT[PEC_STATUS_TRAINING_COMPLETED], "Yes");
            }
            else
            {
                IUSaveText(&PECStateT[PEC_STATUS_TRAINING_COMPLETED], "No");
            }
            if(pec_status & 16)   // Training will start soon
            {
                IUSaveText(&PECStateT[PEC_STATUS_WILL_TRAIN], "Yes");
            }
            else
            {
                IUSaveText(&PECStateT[PEC_STATUS_WILL_TRAIN], "No");
            }
            if(pec_status & 32)   // PEC Data Available
            {
                IUSaveText(&PECStateT[PEC_STATUS_DATA_AVAILABLE], "Yes");
            }
            else
            {
                IUSaveText(&PECStateT[PEC_STATUS_DATA_AVAILABLE], "Yes");
            }
            PECStateTP.s = IPS_OK;
            IDSetText(&PECStateTP, nullptr);
        }
        else
        {
            PECStateTP.s = IPS_ALERT;
            IDSetText(&PECStateTP, nullptr);
        }
        defineProperty(&PECStateTP);
        if (gemini_software_level_ >= 6.0)
        {
            if(getGeminiProperty(FLIP_POINTS_ENABLED_ID, value))
            {
                char valueString[32] = {0};
                uint32_t flip_value = 0;
                sscanf(value, "%u", &flip_value);
                snprintf(valueString, 32, "%i", flip_value);

                if(flip_value)
                {
                    if(flip_value & FLIP_EAST)
                    {
                        FlipControlS[FLIP_EAST_CONTROL].s = ISS_ON;
                    }
                    else
                    {
                        FlipControlS[FLIP_EAST_CONTROL].s = ISS_OFF;
                    }
                    if(flip_value & FLIP_WEST)
                    {
                        FlipControlS[FLIP_WEST_CONTROL].s = ISS_ON;
                    }
                    else
                    {
                        FlipControlS[FLIP_WEST_CONTROL].s = ISS_OFF;
                    }
                }
                else
                {
                    FlipControlS[FLIP_EAST_CONTROL].s = ISS_OFF;
                    FlipControlS[FLIP_WEST_CONTROL].s = ISS_OFF;
                }
                FlipControlSP.s = IPS_OK;
                IDSetSwitch(&FlipControlSP, nullptr);
            }
            else
            {
                FlipControlSP.s = IPS_ALERT;
                IDSetSwitch(&FlipControlSP, nullptr);
            }
            defineProperty(&FlipControlSP);
        }

        if (gemini_software_level_ >= 6.0)
        {
            if(getGeminiProperty(FLIP_POINT_EAST_ID, value) &&
                    getGeminiProperty(FLIP_POINT_WEST_ID, value2))
            {
                double eastSexa;
                double westSexa;

                f_scansexa(value, &eastSexa);
                FlipPositionN[FLIP_EAST_VALUE].value = eastSexa;

                f_scansexa(value2, &westSexa);
                FlipPositionN[FLIP_WEST_VALUE].value = westSexa;

                FlipPositionNP.s = IPS_OK;
                IDSetNumber(&FlipPositionNP, nullptr);
            }
            else
            {
                FlipPositionNP.s = IPS_ALERT;
                IDSetNumber(&FlipPositionNP, nullptr);
            }
            defineProperty(&FlipPositionNP);
        }

        if(getGeminiProperty(EAST_SAFETY_LIMIT_ID, value) &&
                getGeminiProperty(WEST_SAFETY_LIMIT_ID, value2) &&
                getGeminiProperty(WEST_GOTO_SAFETY_LIMIT_ID, value3))
        {
            double eastSafeSexa;
            double westSafeSexa;
            double westGotoSexa;

            f_scansexa(value, &eastSafeSexa);
            SafetyLimitsN[EAST_SAFETY].value = eastSafeSexa;

            f_scansexa(value2, &westSafeSexa);
            SafetyLimitsN[WEST_SAFETY].value = westSafeSexa;

            f_scansexa(value3, &westGotoSexa);
            SafetyLimitsN[WEST_GOTO].value = westGotoSexa;

            SafetyLimitsNP.s = IPS_OK;
            IDSetNumber(&SafetyLimitsNP, nullptr);
        }
        else
        {
            SafetyLimitsNP.s = IPS_ALERT;
            IDSetNumber(&SafetyLimitsNP, nullptr);
        }
        defineProperty(&SetSafetyLimitToCurrentSP);
        defineProperty(&SafetyLimitsNP);

        if (gemini_software_level_ >= 6.0)
        {
            if(getGeminiProperty(SERVO_POINTING_PRECISION_ID, value))
            {
                uint8_t servo_value;
                sscanf(value, "%c", &servo_value);
                if(servo_value &  RA_PRECISION_ENABLED)
                {
                    ServoPrecisionS[SERVO_RA].s = ISS_ON;
                }
                else
                {
                    ServoPrecisionS[SERVO_RA].s = ISS_OFF;
                }
                if(servo_value & DEC_PRECISION_ENABLED)
                {
                    ServoPrecisionS[SERVO_DEC].s = ISS_ON;
                }
                else
                {
                    ServoPrecisionS[SERVO_DEC].s = ISS_OFF;
                }
                ServoPrecisionSP.s = IPS_OK;
                IDSetSwitch(&ServoPrecisionSP, nullptr);
            }
            else
            {
                ServoPrecisionSP.s = IPS_ALERT;
                IDSetSwitch(&ServoPrecisionSP, nullptr);
            }
            defineProperty(&ServoPrecisionSP);

            bool refractionSetting = false;
            if(getRefraction(refractionSetting))
            {
                if(refractionSetting)
                {
                    RefractionControlS[0].s = ISS_ON;
                }
                else
                {
                    RefractionControlS[0].s = ISS_OFF;
                }
                RefractionControlSP.s = IPS_OK;
                IDSetSwitch(&RefractionControlSP, nullptr);
            }
            else
            {
                RefractionControlSP.s = IPS_ALERT;
                IDSetSwitch(&RefractionControlSP, nullptr);
            }
            defineProperty(&RefractionControlSP);

        }
        if (getGeminiProperty(MANUAL_SLEWING_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            ManualSlewingSpeedN[0].value = speed;
            ManualSlewingSpeedNP.s = IPS_OK;
            IDSetNumber(&ManualSlewingSpeedNP, nullptr);
        }
        else
        {
            ManualSlewingSpeedNP.s = IPS_ALERT;
            IDSetNumber(&ManualSlewingSpeedNP, nullptr);
        }
        defineProperty(&ManualSlewingSpeedNP);
        if (getGeminiProperty(GOTO_SLEWING_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            GotoSlewingSpeedN[0].value = speed;
            GotoSlewingSpeedNP.s = IPS_OK;
        }
        else
        {
            GotoSlewingSpeedNP.s = IPS_ALERT;
            IDSetNumber(&GotoSlewingSpeedNP, nullptr);
        }
        defineProperty(&GotoSlewingSpeedNP);
        if (gemini_software_level_ >= 5.0)
        {
            if(getGeminiProperty(MOVE_SPEED_ID, value))
            {
                sscanf(value, "%u", &speed);
                MoveSpeedN[0].value = speed;
                MoveSpeedNP.s = IPS_OK;
                IDSetNumber(&MoveSpeedNP, nullptr);
            }
            else
            {
                MoveSpeedNP.s = IPS_ALERT;
                IDSetNumber(&MoveSpeedNP, nullptr);
            }
        }
        defineProperty(&MoveSpeedNP);
        if (getGeminiProperty(GUIDING_SPEED_ID, value))
        {
            float guidingSpeed = 0.0;
            sscanf(value, "%f", &guidingSpeed);
            GuidingSpeedBothN[GUIDING_BOTH].value = guidingSpeed;
            GuidingSpeedBothNP.s = IPS_OK;
            IDSetNumber(&GuidingSpeedBothNP, nullptr);
        }
        else
        {
            GuidingSpeedBothNP.s = IPS_ALERT;
            IDSetNumber(&GuidingSpeedBothNP, nullptr);
        }
        defineProperty(&GuidingSpeedBothNP);
        if (gemini_software_level_ >= 5.0)
        {
            if(getGeminiProperty(GUIDING_SPEED_RA_ID, value))
            {
                float guidingSpeed = 0.0;
                sscanf(value, "%f", &guidingSpeed);
                GuidingSpeedN[GUIDING_WE].value = guidingSpeed;
                GuidingSpeedNP.s = IPS_OK;
                IDSetNumber(&GuidingSpeedNP, nullptr);
            }
            else
            {
                GuidingSpeedNP.s = IPS_ALERT;
                IDSetNumber(&GuidingSpeedNP, nullptr);
            }
        }
        if (gemini_software_level_ >= 5.0)
        {
            if(getGeminiProperty(GUIDING_SPEED_DEC_ID, value))
            {
                float guidingSpeed = 0.0;
                sscanf(value, "%f", &guidingSpeed);
                GuidingSpeedN[GUIDING_NS].value = guidingSpeed;
                GuidingSpeedNP.s = IPS_OK;
                IDSetNumber(&GuidingSpeedNP, nullptr);
            }
            else
            {
                GuidingSpeedNP.s = IPS_ALERT;
                IDSetNumber(&GuidingSpeedNP, nullptr);
            }
        }
        defineProperty(&GuidingSpeedNP);
        if (getGeminiProperty(CENTERING_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            CenteringSpeedN[0].value = speed;
            CenteringSpeedNP.s = IPS_OK;
            IDSetNumber(&CenteringSpeedNP, nullptr);
        }
        else
        {
            CenteringSpeedNP.s = IPS_ALERT;
            IDSetNumber(&CenteringSpeedNP, nullptr);
        }
        defineProperty(&CenteringSpeedNP);

        updateParkingState();
        updateMovementState();
    }
    else
    {
        deleteProperty(ParkSettingsSP.name);
        deleteProperty(ManualSlewingSpeedNP.name);
        deleteProperty(GotoSlewingSpeedNP.name);
        deleteProperty(MoveSpeedNP.name);
        deleteProperty(GuidingSpeedNP.name);
        deleteProperty(GuidingSpeedBothNP.name);
        deleteProperty(CenteringSpeedNP.name);
        deleteProperty(PECStateTP.name);
        deleteProperty(PECCounterTP.name);
        deleteProperty(PECMaxStepsNP.name);
        deleteProperty(PECGuidingSpeedNP.name);
        deleteProperty(ServoPrecisionSP.name);
        deleteProperty(PECEnableAtBootSP.name);
        deleteProperty(PECGuidingSpeedNP.name);
        deleteProperty(VersionTP.name);
        deleteProperty(FlipPositionNP.name);
        deleteProperty(FlipControlSP.name);
        deleteProperty(PECControlSP.name);
        deleteProperty(SetSafetyLimitToCurrentSP.name);
        deleteProperty(SafetyLimitsNP.name);
    }

    return true;
}
bool LX200Gemini::ISNewText(const char *dev, const char *name, char **texts, char **names, int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, PECStateTP.name))
        {
            IUUpdateText(&PECStateTP, texts, names, n);
            IDSetText(&PECStateTP, nullptr);
        }
        if (!strcmp(name, PECCounterTP.name))
        {
            IUUpdateText(&PECCounterTP, texts, names, n);
            IDSetText(&PECCounterTP, nullptr);
        }
    }

    return LX200Generic::ISNewText(dev, name, texts, names, n);
}

bool LX200Gemini::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, StartupModeSP.name))
        {
            IUUpdateSwitch(&StartupModeSP, states, names, n);
            StartupModeSP.s = IPS_OK;
            if (isConnected())
                LOG_INFO("Startup mode will take effect on future connections.");
            IDSetSwitch(&StartupModeSP, nullptr);
            return true;
        }

        if (!strcmp(name, ParkSettingsSP.name))
        {
            IUUpdateSwitch(&ParkSettingsSP, states, names, n);
            ParkSettingsSP.s = IPS_OK;
            IDSetSwitch(&ParkSettingsSP, nullptr);
            return true;
        }

        if (gemini_software_level_ >= 5.0 && PECStateSP.isNameMatch(name))
        {
            PECStateSP.update(states, names, n);
            for(int i = 0; i < n; ++i)
            {
                // if (!strcmp(names[i], PECStateS[PEC_ON].name))
                if (PECStateSP[PEC_ON].isNameMatch(name))
                {
                    if(PECStateSP[PEC_ON].getState() == ISS_ON)
                    {
                        LOG_INFO("PEC State.s ON.");
                        char valueString[16] = {0};
                        if(!setGeminiProperty(PEC_REPLAY_ON_ID, valueString))
                        {
                            PECStateSP.setState(IPS_ALERT);
                            PECStateSP.apply();
                            return false;
                        }
                        else
                        {
                            return true;
                        }

                    }
                }
                if (PECStateSP[PEC_OFF].isNameMatch(name))
                {
                    if(PECStateSP[PEC_OFF].getState() == ISS_ON)
                    {
                        LOG_INFO("PEC State.s ON.");
                        char valueString[16] = {0};
                        if(!setGeminiProperty(PEC_REPLAY_OFF_ID, valueString))
                        {
                            PECStateSP.setState(IPS_ALERT);
                            PECStateSP.apply();
                            return false;
                        }
                        return true;
                    }
                }
            }
            PECStateSP.setState(IPS_OK);
            PECStateSP.apply();
            return true;
        }

        if (gemini_software_level_ >= 6.0 && !strcmp(name, ServoPrecisionSP.name))
        {
            IUUpdateSwitch(&ServoPrecisionSP, states, names, n);
            ServoPrecisionSP.s = IPS_BUSY;

            uint8_t precisionEnabled = 0;
            for(int i = 0; i < n; ++i)
            {
                if (!strcmp(names[i], ServoPrecisionS[SERVO_RA].name))
                {
                    if(ServoPrecisionS[SERVO_RA].s == ISS_ON)
                    {
                        precisionEnabled |= 1;
                    }
                }

                if (!strcmp(names[i], ServoPrecisionS[SERVO_DEC].name))
                {
                    if(ServoPrecisionS[SERVO_DEC].s == ISS_ON)
                    {
                        precisionEnabled |= 2;
                    }
                }
            }
            char valueString[16] = {0};

            snprintf(valueString, 16, "%u", precisionEnabled);

            if(precisionEnabled & 1)
            {
                LOGF_INFO("ServoPrecision: RA ON  <%i>", (int)precisionEnabled);
            }
            else
            {
                LOGF_INFO("ServoPrecision: RA OFF  <%i>", (int)precisionEnabled);
            }

            if(precisionEnabled & 2)
            {
                LOGF_INFO("ServoPrecision: DEC ON  <%i>", (int)precisionEnabled);
            }
            else
            {
                LOGF_INFO("ServoPrecision: DEC OFF  <%i>", (int)precisionEnabled);
            }

            if(!setGeminiProperty(SERVO_POINTING_PRECISION_ID, valueString))
            {
                ServoPrecisionSP.s = IPS_ALERT;
                IDSetSwitch(&ServoPrecisionSP, nullptr);
                return false;
            }
            else
            {
                ServoPrecisionSP.s = IPS_OK;
                IDSetSwitch(&ServoPrecisionSP, nullptr);
                return true;
            }
        }

        if (gemini_software_level_ >= 6.0 && !strcmp(name, RefractionControlSP.name))
        {
            IUUpdateSwitch(&RefractionControlSP, states, names, n);
            for(int i = 0; i < n; ++i)
            {
                if (!strcmp(names[i], RefractionControlS[0].name))
                {
                    if(RefractionControlS[0].s == ISS_ON)
                    {
                        if(!setRefraction(true))
                        {
                            RefractionControlSP.s = IPS_ALERT;
                            IDSetSwitch(&RefractionControlSP, nullptr);
                            return false;
                        }
                        else
                        {
                            RefractionControlSP.s = IPS_OK;
                            IDSetSwitch(&RefractionControlSP, nullptr);
                            return true;
                        }
                    }
                    else if(RefractionControlS[0].s == ISS_OFF)
                    {
                        if(!setRefraction(false))
                        {
                            RefractionControlSP.s = IPS_ALERT;
                            IDSetSwitch(&RefractionControlSP, nullptr);
                            return false;
                        }
                        else
                        {
                            RefractionControlSP.s = IPS_OK;
                            IDSetSwitch(&RefractionControlSP, nullptr);
                            return true;
                        }
                    }
                }
            }
        }

        if (gemini_software_level_ >= 6.0 && !strcmp(name, FlipControlSP.name))
        {
            IUUpdateSwitch(&FlipControlSP, states, names, n);
            FlipControlSP.s = IPS_OK;
            IDSetSwitch(&FlipControlSP, nullptr);
            int32_t flipEnabled = 0;
            for(int i = 0; i < n; ++i)
            {
                if (!strcmp(names[i], FlipControlS[FLIP_EAST_CONTROL].name))
                {
                    if(FlipControlS[FLIP_EAST_CONTROL].s == ISS_ON)
                    {
                        flipEnabled |= 1;
                        LOGF_INFO("FlipControl: EAST ON  <%i>", flipEnabled);
                    }
                    else
                    {
                        flipEnabled &= 0xfffffffe;
                        LOGF_INFO("FlipControl: EAST OFF  <%i>", flipEnabled);
                    }
                }

                if (!strcmp(names[i], FlipControlS[FLIP_WEST_CONTROL].name))
                {
                    if(FlipControlS[FLIP_WEST_CONTROL].s == ISS_ON)
                    {
                        flipEnabled |= 2;
                        LOGF_INFO("FlipControl: WEST ON  <%i>", flipEnabled);
                    }
                    else
                    {
                        flipEnabled &= 0xfffffffd;
                        LOGF_INFO("FlipControl: WEST OFF  <%i>", flipEnabled);
                    }
                }
            }
            char valueString[16] = {0};
            snprintf(valueString, 16, "%i", flipEnabled);
            LOGF_INFO("FlipControl: <%s>", valueString);
            if(!setGeminiProperty(FLIP_POINTS_ENABLED_ID, valueString))
            {
                FlipControlSP.s = IPS_ALERT;
                IDSetSwitch(&FlipControlSP, nullptr);
                return false;
            }
            else
            {
                FlipControlSP.s = IPS_OK;
                IDSetSwitch(&FlipControlSP, nullptr);
                return true;
            }
        }
        if (!strcmp(name, SetSafetyLimitToCurrentSP.name))
        {
            char valueString[16] = {0};
            IUUpdateSwitch(&SetSafetyLimitToCurrentSP, states, names, n);
            IDSetSwitch(&SetSafetyLimitToCurrentSP, nullptr);
            SetSafetyLimitToCurrentS[0].s = ISS_OFF;
            if(!setGeminiProperty(SET_SAFETY_LIMIT_TO_CURRENT_ID, valueString))
            {
                SetSafetyLimitToCurrentSP.s = IPS_ALERT;
                IDSetSwitch(&SetSafetyLimitToCurrentSP, nullptr);
                return false;
            }
            else
            {
                SetSafetyLimitToCurrentSP.s = IPS_OK;
                IDSetSwitch(&SetSafetyLimitToCurrentSP, nullptr);
            }

        }

        if (gemini_software_level_ >= 5.0 && !strcmp(name, PECControlSP.name))
        {
            IUUpdateSwitch(&PECControlSP, states, names, n);
            for(int i = 0; i < n; ++i)
            {
                if (!strcmp(names[i], PECControlS[PEC_START_TRAINING].name))
                {
                    char valueString[16] = {0};
                    if(!setGeminiProperty(PEC_START_TRAINING_ID, valueString))
                    {
                        PECControlSP.s = IPS_ALERT;
                        IDSetSwitch(&PECControlSP, nullptr);
                        return false;
                    }
                }
                else if (!strcmp(names[i], PECControlS[PEC_ABORT_TRAINING].name))
                {
                    char valueString[16] = {0};
                    if(!setGeminiProperty(PEC_ABORT_TRAINING_ID, valueString))
                    {
                        PECControlSP.s = IPS_ALERT;
                        IDSetSwitch(&PECControlSP, nullptr);
                        return false;
                    }

                }

            }
            PECControlSP.s = IPS_OK;
            IDSetSwitch(&PECControlSP, nullptr);
            return true;
        }
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200Gemini::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        char valueString[16] = {0};
        snprintf(valueString, 16, "%2.0f", values[0]);

        if (!strcmp(name, ManualSlewingSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set manual slewing speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(MANUAL_SLEWING_SPEED_ID, valueString))
            {
                ManualSlewingSpeedNP.s = IPS_ALERT;
                IDSetNumber(&ManualSlewingSpeedNP, "Error setting manual slewing speed");
                return false;
            }

            ManualSlewingSpeedNP.s    = IPS_OK;
            ManualSlewingSpeedN[0].value = values[0];
            IDSetNumber(&ManualSlewingSpeedNP, "Manual slewing speed set to %f", values[0]);

            return true;
        }
        if (!strcmp(name, GotoSlewingSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set goto slewing speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(GOTO_SLEWING_SPEED_ID, valueString))
            {
                GotoSlewingSpeedNP.s = IPS_ALERT;
                IDSetNumber(&GotoSlewingSpeedNP, "Error setting goto slewing speed");
                return false;
            }

            GotoSlewingSpeedNP.s       = IPS_OK;
            GotoSlewingSpeedN[0].value = values[0];
            IDSetNumber(&GotoSlewingSpeedNP, "Goto slewing speed set to %f", values[0]);

            return true;
        }
        if (gemini_software_level_ >= 5.0 && !strcmp(name, MoveSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set move speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(MOVE_SPEED_ID, valueString))
            {
                MoveSpeedNP.s = IPS_ALERT;
                IDSetNumber(&MoveSpeedNP, "Error setting move speed");
                return false;
            }

            MoveSpeedNP.s       = IPS_OK;
            MoveSpeedN[0].value = values[0];
            IDSetNumber(&MoveSpeedNP, "Move speed set to %f", values[0]);

            return true;
        }
        if (gemini_software_level_ >= 5.0 && !strcmp(name, GuidingSpeedBothNP.name))
        {
            LOGF_DEBUG("Trying to set guiding speed of: %f", values[0]);

            for(int i = 0; i < n; ++i)
            {
                if (!strcmp(names[i], GuidingSpeedBothN[GUIDING_BOTH].name))
                {
                    // Special formatting for guiding speed
                    snprintf(valueString, 16, "%1.1f", values[0]);

                    if (!isSimulation() && !setGeminiProperty(GUIDING_SPEED_ID, valueString))
                    {
                        GuidingSpeedBothNP.s = IPS_ALERT;
                        IDSetNumber(&GuidingSpeedBothNP, "Error setting guiding speed");
                        return false;
                    }

                }
            }

            GuidingSpeedBothN[GUIDING_BOTH].value = values[0];
            GuidingSpeedBothNP.s                  = IPS_OK;
            IDSetNumber(&GuidingSpeedBothNP, "Guiding speed set to %f", values[0]);

            return true;
        }
        if (!strcmp(name, GuidingSpeedNP.name))
        {

            for(int i = 0; i < n; ++i)
            {
                if (!strcmp(names[i], GuidingSpeedN[GUIDING_WE].name))
                {
                    // Special formatting for guiding speed
                    snprintf(valueString, 16, "%1.1f", values[i]);
                    if (!isSimulation() && !setGeminiProperty(GUIDING_SPEED_RA_ID, valueString))
                    {
                        GuidingSpeedN[GUIDING_WE].value = values[i];
                        GuidingSpeedNP.s = IPS_ALERT;
                        IDSetNumber(&GuidingSpeedNP, "Error Setting Guiding WE");
                        return false;
                    }

                }
                if (!strcmp(names[i], GuidingSpeedN[GUIDING_NS].name))
                {
                    // Special formatting for guiding speed
                    snprintf(valueString, 16, "%1.1f", values[i]);
                    GuidingSpeedNP.s = IPS_ALERT;
                    if (!isSimulation() && !setGeminiProperty(GUIDING_SPEED_DEC_ID, valueString))
                    {
                        GuidingSpeedN[GUIDING_NS].value = values[i];
                        GuidingSpeedNP.s = IPS_ALERT;
                        IDSetNumber(&GuidingSpeedNP, "Error Setting Guiding WE");
                        return false;
                    }
                }
            }

            GuidingSpeedNP.s       = IPS_OK;
            IDSetNumber(&GuidingSpeedNP, "Guiding speed set to RA:%f DEC:%f",  GuidingSpeedN[GUIDING_WE].value, GuidingSpeedN[GUIDING_NS].value);

            return true;
        }
        if (!strcmp(name, SafetyLimitsNP.name))
        {
            double eastSafeD = 0;
            double westSafeD = 0;
            double westGotoD = 0;

            for(int i = 0; i < n; ++i)
            {
                if (!strcmp(names[i], SafetyLimitsN[EAST_SAFETY].name))
                {
                    eastSafeD = values[i];
                }
                if (!strcmp(names[i], SafetyLimitsN[WEST_SAFETY].name))
                {
                    westSafeD = values[i];
                }
                if (!strcmp(names[i], SafetyLimitsN[WEST_GOTO].name))
                {
                    westGotoD = values[i];
                }
            }
            char eastSafe[32] = {0};
            SafetyLimitsN[EAST_SAFETY].value = eastSafeD;
            fs_sexa(eastSafe, eastSafeD, 2, 60);

            char westSafe[32] = {0};
            SafetyLimitsN[WEST_SAFETY].value = westSafeD;
            fs_sexa(westSafe, westSafeD, 2, 60);

            char westGoto[32] = {0};
            SafetyLimitsN[WEST_GOTO].value = westGotoD;
            fs_sexa(westGoto, westGotoD, 2, 60);

            char *colon = strchr(eastSafe, ':');
            if(colon)
            {
                *colon = 'd';
            }
            colon = strchr(westSafe, ':');
            if(colon)
            {
                *colon = 'd';
            }
            colon = strchr(westGoto, ':');
            if(colon)
            {
                *colon = 'd';
            }

            if (!isSimulation() &&
                    (!setGeminiProperty(EAST_SAFETY_LIMIT_ID, eastSafe) ||
                     !setGeminiProperty(WEST_SAFETY_LIMIT_ID, westSafe) ||
                     !setGeminiProperty(WEST_GOTO_SAFETY_LIMIT_ID, westGoto)))
            {
                SafetyLimitsNP.s = IPS_ALERT;
                IDSetNumber(&SafetyLimitsNP, "Error Setting Limits");
                return false;
            }
            IDSetNumber(&SafetyLimitsNP, "Limits EastSafe:%s, WestSafe:%s, WestGoto:%s", eastSafe, westSafe, westGoto);
            SafetyLimitsNP.s = IPS_OK;
            return true;
        }
        if (gemini_software_level_ >= 6.0 && !strcmp(name, FlipPositionNP.name))
        {
            double eastD = 0;
            double westD = 0;

            for(int i = 0; i < n; ++i)
            {
                if (!strcmp(names[i], FlipPositionN[FLIP_EAST_VALUE].name))
                {
                    eastD = values[i];
                }
                if (!strcmp(names[i], FlipPositionN[FLIP_WEST_VALUE].name))
                {
                    westD = values[i];
                }
            }
            char east[32] = {0};
            FlipPositionN[FLIP_EAST_VALUE].value = eastD;
            fs_sexa(east, eastD, 2, 60);

            char west[32] = {0};
            FlipPositionN[FLIP_WEST_VALUE].value = westD;
            fs_sexa(west, westD, 2, 60);

            char *colon = strchr(east, ':');
            if(colon)
            {
                *colon = 'd';
            }
            colon = strchr(west, ':');
            if(colon)
            {
                *colon = 'd';
            }
            if (!isSimulation() &&
                    (!setGeminiProperty(FLIP_POINT_EAST_ID, east) ||
                     !setGeminiProperty(FLIP_POINT_WEST_ID, west)))
            {
                FlipPositionNP.s = IPS_ALERT;
                IDSetNumber(&FlipPositionNP, "Error Setting Flip Points");
                return false;
            }
            IDSetNumber(&FlipPositionNP, "FlipPoints East:%s, West:%s", east, west);
            FlipPositionNP.s = IPS_OK;
            return true;
        }
        if (!strcmp(name, CenteringSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set centering speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(CENTERING_SPEED_ID, valueString))
            {
                CenteringSpeedNP.s = IPS_ALERT;
                IDSetNumber(&CenteringSpeedNP, "Error setting centering speed");
                return false;
            }

            CenteringSpeedNP.s       = IPS_OK;
            CenteringSpeedN[0].value = values[0];
            IDSetNumber(&CenteringSpeedNP, "Centering speed set to %f", values[0]);

            return true;
        }
        if (!strcmp(name, PECMaxStepsNP.name))
        {
            PECMaxStepsNP.s       = IPS_OK;
            PECMaxStepsN[0].value = values[0];
            IDSetNumber(&PECMaxStepsNP, "Max steps set to %f", values[0]);
            return true;
        }
        if (gemini_software_level_ >= 5.0 && !strcmp(name, PECGuidingSpeedNP.name))
        {
            PECGuidingSpeedNP.s       = IPS_OK;
            PECGuidingSpeedN[0].value = values[0];
            IDSetNumber(&PECGuidingSpeedNP, "Guiding Speed set to %f", values[0]);
            return true;
        }
    }

    //  If we didn't process it, continue up the chain, let somebody else give it a shot
    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200Gemini::checkConnection()
{
    if (isSimulation())
        return true;

    // Response
    char response[8] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%#02X>", 0x06);

    tcflush(PortFD, TCIFLUSH);

    char ack[1] = { 0x06 };

    if ((rc = tty_write(PortFD, ack, 1, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    // Read response
    if ((rc = tty_read_section(PortFD, response, '#', GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return false;
    }

    //response[1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    // If waiting for selection of startup mode, let us select it
    if (response[0] == 'b')
    {
        LOG_DEBUG("Mount is waiting for selection of the startup mode.");
        char cmd[4]     = "bC#";
        int startupMode = IUFindOnSwitchIndex(&StartupModeSP);
        if (startupMode == WARM_START)
            strncpy(cmd, "bW#", 4);
        else if (startupMode == WARM_RESTART)
            strncpy(cmd, "bR#", 4);

        LOGF_DEBUG("CMD: <%s>", cmd);

        if ((rc = tty_write(PortFD, cmd, 4, &nbytes_written)) != TTY_OK)
        {
            char errmsg[256];
            tty_error_msg(rc, errmsg, 256);
            LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
            return false;
        }

        tcflush(PortFD, TCIFLUSH);

        // Send ack again and check response
        return checkConnection();
    }
    else if (response[0] == 'B')
    {
        LOG_DEBUG("Initial startup message is being displayed.");
    }
    else if (response[0] == 'S')
    {
        LOG_DEBUG("Cold start in progress.");
    }
    else if (response[0] == 'G')
    {
        updateParkingState();
        updateMovementState();
        LOG_DEBUG("Startup complete with equatorial mount selected.");
    }
    else if (response[0] == 'A')
    {
        LOG_DEBUG("Startup complete with Alt-Az mount selected.");
    }

    return true;
}

bool LX200Gemini::isSlewComplete()
{
    LX200Gemini::MovementState movementState = getMovementState();

    if (movementState == TRACKING || movementState == GUIDING || movementState == NO_MOVEMENT)
        return true;
    else
        return false;
}

bool LX200Gemini::ReadScopeStatus()
{
    LOGF_DEBUG("ReadScopeStatus: TrackState is <%d>", TrackState);

    if (!isConnected())
        return false;

    if (isSimulation())
        return LX200Generic::ReadScopeStatus();

    if (TrackState == SCOPE_SLEWING)
    {
        updateMovementState();

        EqNP.setState(IPS_BUSY);
        EqNP.apply();

        // Check if LX200 is done slewing
        if (isSlewComplete())
        {
            // Set slew mode to "Centering"
            SlewRateSP.reset();
            SlewRateSP[SLEW_CENTERING].setState(ISS_ON);
            SlewRateSP.apply();

            EqNP.setState(IPS_OK);
            EqNP.apply();

            if (TrackState == SCOPE_IDLE)
            {
                SetTrackEnabled(true);
                updateMovementState();
            }

            LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        updateParkingState();

        if (isSlewComplete())
        {
            LOG_DEBUG("Park is complete ...");
            SetParked(true);
            sleepMount();

            EqNP.setState(IPS_IDLE);
            EqNP.apply();

            return true;
        }
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        LOG_ERROR("Error reading RA/DEC.");
        return false;
    }

    NewRaDec(currentRA, currentDEC);

    syncSideOfPier();
    syncState();
    return true;
}

void LX200Gemini::syncSideOfPier()
{
    // Send ':Gm#'
    const char *cmd = ":Gm#";
    // Response
    char response[8] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return;
    }

    if ((rc = tty_read_section(PortFD, response, '#', GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return;
    }

    response[nbytes_read - 1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    //LOGF_DEBUG("RES: <%s>", response);

    // fix to pier side read from the mount using the hour angle as a guide
    // see https://www.indilib.org/forum/general/6785-side-of-pier-problem-bug.html?start=12#52492
    // for a description of the problem and the proposed fix
    //
    auto lst = get_local_sidereal_time(this->LocationNP[LOCATION_LONGITUDE].getValue());
    auto ha = rangeHA(lst - currentRA);
    auto pointingState = PIER_UNKNOWN;

    if (ha >= -5.0 && ha <= 5.0)
    {
        // mount pier side is used unchanged
        pointingState = response[0] == 'E' ? PIER_EAST : PIER_WEST;
    }
    else if (ha <= -7.0 || ha >= 7.0)
    {
        // mount pier side is reversed
        pointingState = response[0] == 'W' ? PIER_EAST : PIER_WEST;
    }
    else
    {
        // use hour angle because the pier side changes spontaneously near +-6h
        pointingState = ha > 0 ? PIER_EAST : PIER_WEST;
    }

    LOGF_DEBUG("RES: <%s>, lst %f, ha %f, pierSide %d", response, lst, ha, pointingState);

    setPierSide(pointingState);
}


bool LX200Gemini::Park()
{
    char cmd[6] = ":hP#";

    int parkSetting = IUFindOnSwitchIndex(&ParkSettingsSP);

    if (parkSetting == PARK_STARTUP)
        strncpy(cmd, ":hC#", 5);
    else if (parkSetting == PARK_ZENITH)
        strncpy(cmd, ":hZ#", 5);

    // Response
    int rc = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    ParkSP.setState(IPS_BUSY);
    TrackState = SCOPE_PARKING;

    updateParkingState();
    return true;
}

bool LX200Gemini::UnPark()
{
    wakeupMount();

    SetParked(false);
    SetTrackEnabled(true);

    updateParkingState();
    updateMovementState();
    return true;
}

bool LX200Gemini::sleepMount()
{
    const char *cmd = ":hN#";

    // Response
    int rc = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    LOG_INFO("Mount is sleeping...");
    return true;
}

bool LX200Gemini::wakeupMount()
{
    const char *cmd = ":hW#";

    // Response
    int rc = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    LOG_INFO("Mount is awake...");
    return true;
}

void LX200Gemini::setTrackState(INDI::Telescope::TelescopeStatus state)
{
    if (TrackState != state)
        TrackState = state;
}

void LX200Gemini::updateMovementState()
{
    LX200Gemini::MovementState movementState = getMovementState();

    switch (movementState)
    {
        case NO_MOVEMENT:
            if (priorParkingState == PARKED)
                setTrackState(SCOPE_PARKED);
            else
                setTrackState(SCOPE_IDLE);
            break;

        case TRACKING:
        case GUIDING:
            setTrackState(SCOPE_TRACKING);
            break;

        case CENTERING:
        case SLEWING:
            setTrackState(SCOPE_SLEWING);
            break;

        case STALLED:
            setTrackState(SCOPE_IDLE);
            break;
    }
}

void LX200Gemini::updateParkingState()
{
    LX200Gemini::ParkingState parkingState = getParkingState();

    if (parkingState != priorParkingState)
    {
        if (parkingState == PARKED)
            SetParked(true);
        else if (parkingState == NOT_PARKED)
            SetParked(false);
    }
    priorParkingState = parkingState;
}

LX200Gemini::MovementState LX200Gemini::getMovementState()
{
    const char *cmd = ":Gv#";
    char response[2] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return LX200Gemini::MovementState::NO_MOVEMENT;
    }

    if ((rc = tty_read(PortFD, response, 1, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return LX200Gemini::MovementState::NO_MOVEMENT;
    }

    response[1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    switch (response[0])
    {
        case 'N':
            return LX200Gemini::MovementState::NO_MOVEMENT;

        case 'T':
            return LX200Gemini::MovementState::TRACKING;

        case 'G':
            return LX200Gemini::MovementState::GUIDING;

        case 'C':
            return LX200Gemini::MovementState::CENTERING;

        case 'S':
            return LX200Gemini::MovementState::SLEWING;

        case '!':
            return LX200Gemini::MovementState::STALLED;

        default:
            return LX200Gemini::MovementState::NO_MOVEMENT;
    }
}

LX200Gemini::ParkingState LX200Gemini::getParkingState()
{
    const char *cmd = ":h?#";
    char response[2] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return LX200Gemini::ParkingState::NOT_PARKED;
    }

    if ((rc = tty_read(PortFD, response, 1, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return LX200Gemini::ParkingState::NOT_PARKED;
    }

    response[1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    switch (response[0])
    {
        case '0':
            return LX200Gemini::ParkingState::NOT_PARKED;

        case '1':
            return LX200Gemini::ParkingState::PARKED;

        case '2':
            return LX200Gemini::ParkingState::PARK_IN_PROGRESS;

        default:
            return LX200Gemini::ParkingState::NOT_PARKED;
    }
}

bool LX200Gemini::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &StartupModeSP);
    IUSaveConfigSwitch(fp, &ParkSettingsSP);

    return true;
}

bool LX200Gemini::getGeminiProperty(uint32_t propertyNumber, char* value)
{
    int rc = TTY_OK;
    int nbytes = 0;
    char prefix[16] = {0};
    char cmd[16] = {0};

    switch(propertyNumber)
    {
        case MOVE_SPEED_ID:
        case GUIDING_SPEED_RA_ID:
        case GUIDING_SPEED_DEC_ID:
        case PEC_START_TRAINING_ID:
        case PEC_ABORT_TRAINING_ID:
        case PEC_REPLAY_ON_ID:
        case PEC_REPLAY_OFF_ID:
            if(gemini_software_level_ < 5.0)
            {
                LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
                return false;
            }
            break;
        case PEC_ENABLE_AT_BOOT_ID:
            if(gemini_software_level_ < 5.2)
            {
                LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
                return false;
            }
            break;
        case FLIP_POINT_EAST_ID:
        case FLIP_POINT_WEST_ID:
        case FLIP_POINTS_ENABLED_ID:
        case SERVO_POINTING_PRECISION_ID:
        case SERVO_FIRMWARE:
            if(gemini_software_level_ < 6)
            {
                LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
                return false;
            }
            break;
        default:
            ;
    }

    snprintf(prefix, 16, "<%d:", propertyNumber);

    uint8_t checksum = calculateChecksum(prefix);

    snprintf(cmd, 16, "%s%c#", prefix, checksum);

    LOGF_DEBUG("CMD: <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    if ((rc = tty_read_section(PortFD, value, '#', GEMINI_TIMEOUT, &nbytes)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return false;
    }

    value[nbytes - 1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    LOGF_DEBUG("RES: <%s>", value);
    return true;
}

int LX200Gemini::SendPulseCmd(int8_t direction, uint32_t duration_msec)
{
    return ::SendPulseCmd(PortFD, direction, duration_msec, true, 1000);
}

bool LX200Gemini::Flip(double ra, double dec)
{
    return GotoInternal(ra, dec, true);
}

bool LX200Gemini::Goto(double ra, double dec)
{
    return GotoInternal(ra, dec, false);
}

int  LX200Gemini::Flip(int fd)
{
    DEBUGFDEVICE(getDeviceName(), DBG_SCOPE, "<%s>", __FUNCTION__);
    char FlipNum[17] = {0};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;
    const char *command = ":MM#";

    DEBUGFDEVICE(getDeviceName(), DBG_SCOPE, "CMD <%s>", command);

    // Gemini Serial Command
    // Returns
    // 0                 Flip is Possible
    // 1                 Object below horizon.#
    // 2                 No object selected.#
    // 3                 Manual Control.#
    // 4                 Position unreachable.#
    // 5                 Not aligned.#
    // 6                 Outside Limits.#
    // 7                 Rejected - Mount is parked!#

    if ((error_type = tty_write_string(fd, command, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read(fd, FlipNum, 1, LX200_TIMEOUT, &nbytes_read);

    if (nbytes_read < 1)
    {
        DEBUGFDEVICE(getDeviceName(), DBG_SCOPE, "RES ERROR <%d>", error_type);
        return error_type;
    }

    /* We don't need to read the string message, just return corresponding error code */
    tcflush(fd, TCIFLUSH);

    DEBUGFDEVICE(getDeviceName(), DBG_SCOPE, "RES <%c>", FlipNum[0]);

    error_type = FlipNum[0] - '0';
    if ((error_type >= 0) && (error_type <= 9))
    {
        return error_type;
    }
    else
    {
        return -1;
    }
}

bool LX200Gemini::GotoInternal(double ra, double dec, bool flip)
{
    const struct timespec timeout = {0, 100000000L};

    targetRA  = ra;
    targetDEC = dec;
    char RAStr[64] = {0}, DecStr[64] = {0};
    int fracbase = 0;

    switch (getLX200EquatorialFormat())
    {
        case LX200_EQ_LONGER_FORMAT:
            fracbase = 360000;
            break;
        case LX200_EQ_LONG_FORMAT:
        case LX200_EQ_SHORT_FORMAT:
        default:
            fracbase = 3600;
            break;
    }

    fs_sexa(RAStr, targetRA, 2, fracbase);
    fs_sexa(DecStr, targetDEC, 2, fracbase);

    // If moving, let's stop it first.
    if (EqNP.getState() == IPS_BUSY)
    {
        if (!isSimulation() && abortSlew(PortFD) < 0)
        {
            AbortSP.setState(IPS_ALERT);
            LOG_ERROR("Abort slew failed.");
            AbortSP.apply();
            return false;
        }

        AbortSP.setState(IPS_OK);
        EqNP.setState(IPS_IDLE);
        LOG_ERROR("Slew aborted.");
        AbortSP.apply();
        EqNP.apply();

        if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
        {
            MovementNSSP.setState(IPS_IDLE);
            MovementWESP.setState(IPS_IDLE);
            EqNP.setState(IPS_IDLE);
            MovementNSSP.reset();
            MovementWESP.reset();
            MovementNSSP.apply();
            MovementWESP.apply();
        }

        // sleep for 100 mseconds
        nanosleep(&timeout, nullptr);
    }

    if (!isSimulation())
    {
        if (setObjectRA(PortFD, targetRA) < 0 || (setObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Error setting RA/DEC.");
            EqNP.apply();
            return false;
        }

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = flip ? Flip(PortFD) : Slew(PortFD)))
        {
            LOGF_ERROR("Error %s to JNow RA %s - DEC %s", flip ? "Flipping" : "Slewing", RAStr, DecStr);
            slewError(err);
            return false;
        }
    }

    TrackState = SCOPE_SLEWING;
    //EqNP.s     = IPS_BUSY;

    LOGF_INFO("%s to RA: %s - DEC: %s", flip ? "Flipping" : "Slewing", RAStr, DecStr);

    return true;
}

bool LX200Gemini::setGeminiProperty(uint32_t propertyNumber, char* value)
{
    int rc = TTY_OK;
    int nbytes_written = 0;
    char prefix[16] = {0};
    char cmd[16] = {0};
    switch(propertyNumber)
    {
        case MOVE_SPEED_ID:
        case GUIDING_SPEED_RA_ID:
        case GUIDING_SPEED_DEC_ID:
        case PEC_START_TRAINING_ID:
        case PEC_ABORT_TRAINING_ID:
        case PEC_REPLAY_ON_ID:
        case PEC_REPLAY_OFF_ID:
            if(gemini_software_level_ < 5.0)
            {
                LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
                return false;
            }
            break;
        case PEC_ENABLE_AT_BOOT_ID:
            if(gemini_software_level_ < 5.2)
            {
                LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
                return false;
            }
            break;
        case SERVO_POINTING_PRECISION_ID:
        case SERVO_FIRMWARE:
            if(gemini_software_level_ < 6)
            {
                LOGF_ERROR("Error Gemini Firmware Level %f does not support command %i ", gemini_software_level_, propertyNumber);
                return false;
            }
            break;
        default:
            ;
    }

    snprintf(prefix, 16, ">%d:%s", propertyNumber, value);

    uint8_t checksum = calculateChecksum(prefix);

    snprintf(cmd, 16, "%s%c#", prefix, checksum);

    LOGF_DEBUG("CMD: <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIFLUSH);

    return true;
}

bool LX200Gemini::SetTrackMode(uint8_t mode)
{
    char empty[] = "";
    return setGeminiProperty(131 + mode, empty);
}

bool LX200Gemini::SetTrackEnabled(bool enabled)
{
    return SetTrackMode(enabled ? TrackModeSP.findOnSwitchIndex() : GEMINI_TRACK_TERRESTRIAL);
}

uint8_t LX200Gemini::calculateChecksum(char *cmd)
{
    uint8_t result = cmd[0];

    for (size_t i = 1; i < strlen(cmd); i++)
        result = result ^ cmd[i];

    result = result % 128;
    result += 64;

    return result;
}
