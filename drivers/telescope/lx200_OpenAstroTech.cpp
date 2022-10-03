/*
    OpenAstroTech
    Copyright (C) 2021 Anjo Krank

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

#include "lx200_OpenAstroTech.h"

#include "indicom.h"
#include "lx200driver.h"

#include <cmath>
#include <string.h>

#include <libnova/transform.h>
#include <termios.h>
#include <unistd.h>
#include <mutex>

#define RB_MAX_LEN    64
#define CMD_MAX_LEN 32
extern std::mutex lx200CommsLock;

LX200_OpenAstroTech::LX200_OpenAstroTech(void) : LX200GPS()
{
    setVersion(MAJOR_VERSION, MINOR_VERSION);
}

bool LX200_OpenAstroTech::Handshake()
{
    bool result = LX200GPS::Handshake();
    return result;
}

#define OAT_MEADE_COMMAND "OAT_MEADE_COMMAND"
#define OAT_DEC_LOWER_LIMIT "OAT_DEC_LOWER_LIMIT"
#define OAT_DEC_UPPER_LIMIT "OAT_DEC_UPPER_LIMIT"
#define OAT_GET_DEBUG_LEVEL "OAT_GET_DEBUG_LEVEL"
#define OAT_GET_ENABLED_DEBUG_LEVEL "OAT_GET_ENABLED_DEBUG_LEVEL"
#define OAT_SET_DEBUG_LEVEL "OAT_GET_DEBUG_LEVEL"

const char *OAT_TAB       = "Open Astro Tech";

bool LX200_OpenAstroTech::initProperties()
{
    LX200GPS::initProperties();
    IUFillText(&MeadeCommandT, OAT_MEADE_COMMAND, "Result / Command", "");
    IUFillTextVector(&MeadeCommandTP, &MeadeCommandT, 1, getDeviceName(), "Meade", "", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    FI::SetCapability(FOCUSER_CAN_ABORT | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_REVERSE | FOCUSER_HAS_VARIABLE_SPEED | FOCUSER_HAS_BACKLASH);
    
    FI::initProperties(FOCUS_TAB);
    initFocuserProperties(FOCUS_TAB);

    // Polar Align Alt
    IUFillNumber(&PolarAlignAltN, "OAT_POLAR_ALT", "Arcsecs", "%.f", -500.0, 500.0, 10.0, 0);
    IUFillNumberVector(&PolarAlignAltNP, &PolarAlignAltN, 1, m_defaultDevice->getDeviceName(), "POLAR_ALT",
                       "Polar Align Alt",
                       MOTION_TAB, IP_RW, 60, IPS_OK);
    // Polar Align Az
    IUFillNumber(&PolarAlignAzN, "OAT_POLAR_AZ", "Arcsecs", "%.f", -500.0, 500.0, 10.0, 0);
    IUFillNumberVector(&PolarAlignAzNP, &PolarAlignAzN, 1, m_defaultDevice->getDeviceName(), "POLAR_AZ",
                       "Polar Align Azimuth",
                       MOTION_TAB, IP_RW, 60, IPS_OK);

    // Home
    IUFillSwitch(&HomeS, "OAT_HOME", "Home", ISS_OFF);
    IUFillSwitchVector(&HomeSP, &HomeS, 1, m_defaultDevice->getDeviceName(), 
                       "OAT_HOME", "Home", MOTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // RA Home
    IUFillNumber(&RAHomeN, "RA_HOME", "Hours", "%d", 1.0, 7.0, 1.0, 2);
    IUFillNumberVector(&RAHomeNP, &RAHomeN, 1, m_defaultDevice->getDeviceName(), 
                       "OAT_RA_HOME", "RA Home", MOTION_TAB, IP_RW, 60, IPS_IDLE);

    // RA Home
    IUFillNumber(&RAHomeOffsetN, "OAT_RA_HOME_OFFSET", "Steps", "%d", -10000.0, 10000.0, 100.0, 0);
    IUFillNumberVector(&RAHomeOffsetNP, &RAHomeOffsetN, 1, m_defaultDevice->getDeviceName(), 
                       "OAT_RA_HOME", "RA Home Offset", MOTION_TAB, IP_RW, 60, IPS_IDLE);

    // Polar Align Alt
    IUFillNumber(&DecLimitsN[0], "OAT_DEC_LIMIT_LOWER", "Lower", "%.f", -20000.0, 0.0, 1000.0, 0);
    IUFillNumber(&DecLimitsN[1], "OAT_DEC_LIMIT_UPPER", "Upper", "%.f", 0.0, 50000.0, 1000.0, 0);
    IUFillNumberVector(&DecLimitsNP, DecLimitsN, 2, m_defaultDevice->getDeviceName(), "OAT_DEC_LIMITS",
                       "DEC Limits", MOTION_TAB, IP_RW, 60, IPS_OK);
    return true;
}

bool LX200_OpenAstroTech::updateProperties()
{
    LX200GPS::updateProperties();

    if (isConnected())
    {
        defineProperty(&MeadeCommandTP);
        defineProperty(&PolarAlignAltNP);
        defineProperty(&PolarAlignAzNP);
        //defineProperty(&HomeSP);
        //defineProperty(&RAHomeNP);
        //defineProperty(&RAHomeOffsetNP);
        //defineProperty(&DecLimitsNP);
    }
    else
    {
        deleteProperty(MeadeCommandTP.name);
        deleteProperty(PolarAlignAltNP.name);
        deleteProperty(PolarAlignAzNP.name);
        //deleteProperty(HomeSP.name);
        //deleteProperty(RAHomeNP.name);
        //deleteProperty(RAHomeOffsetNP.name);
        //deleteProperty(DecLimitsNP.name);
    }

    return true;
}

bool LX200_OpenAstroTech::ReadScopeStatus() {
    if (!isConnected())
        return false;

    if (isSimulation()) //if Simulation is selected
    {
        mountSim();
        return true;
    }

    if(OATUpdateProperties() != 0) 
    {

    }
    if (OATUpdateFocuser() != 0)  // Update Focuser Position
    {
        LOG_WARN("Communication error on Focuser Update, this update aborted, will try again...");
    }
    return LX200GPS::ReadScopeStatus();
}

int LX200_OpenAstroTech::OATUpdateProperties()
{
    // we don't need to poll if we're not moving
    if(!(
        PolarAlignAltNP.s == IPS_BUSY || PolarAlignAzNP.s == IPS_BUSY
        || RAHomeNP.s == IPS_BUSY
    )) {
        return 0;
    }

    tcflush(PortFD, TCIOFLUSH);
    flushIO(PortFD);
   
    char value[RB_MAX_LEN] = {0};
    int rc = executeMeadeCommand(":GX#", value);
    if (rc == 0 && strlen(value) > 10)
    {
        char *status = "Tracking";
        char *motors = strchr(value, ',') + 1;
        if(PolarAlignAzNP.s == IPS_BUSY && motors[3] =='-') {
            PolarAlignAzNP.s = IPS_IDLE;
            IDSetNumber(&PolarAlignAzNP, nullptr);
        }
        if(PolarAlignAltNP.s == IPS_BUSY && motors[4] =='-') {
            PolarAlignAltNP.s = IPS_IDLE;
            IDSetNumber(&PolarAlignAltNP, nullptr);
        }
        if(RAHomeNP.s == IPS_BUSY && status[0] == 'H') 
        {
            RAHomeNP.s = IPS_IDLE;
            IDSetNumber(&RAHomeNP, nullptr);
        }
    }
    updateProperties();
    return 0;
}

bool LX200_OpenAstroTech::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, MeadeCommandTP.name))
        {
            if(!isSimulation()) {
                // we're using the "Set" field for the command and the actual field for the result
                // we need to:
                // - get the value
                // - check if it's a command
                // - if so, execute it, then set the result to the control
                // the client side needs to
                // - push ":somecmd#"
                // - listen to change on MeadeCommand and log it
                char * cmd = texts[0];
                size_t len = strlen(cmd);
                DEBUGFDEVICE(getDeviceName(), DBG_SCOPE, "Meade Command <%s>", cmd);
                if(len > 2 && cmd[0] == ':' && cmd[len-1] == '#') {
                    IText *tp = IUFindText(&MeadeCommandTP, names[0]);
                    int err = executeMeadeCommand(texts[0], MeadeCommandResult);
                    DEBUGFDEVICE(getDeviceName(), DBG_SCOPE, "Meade Command Result %d <%s>", err, MeadeCommandResult);
                    if(err == 0) {
                        MeadeCommandTP.s = IPS_OK;
                        IUSaveText(tp, MeadeCommandResult);
                        IDSetText(&MeadeCommandTP, MeadeCommandResult);
                        return true;
                    } else {
                        MeadeCommandTP.s = IPS_ALERT;
                        IDSetText(&MeadeCommandTP, nullptr);
                        return true;
                    }
                }
            }
       }
    }

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool LX200_OpenAstroTech::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        char read_buffer[RB_MAX_LEN] = {0};
        if (!strcmp(name, PolarAlignAltN.name))
        {
            /*if (IUUpdateNumber(&PolarAlignAltNP, values, names, n) < 0)
                return false;
            */
            LOGF_WARN("Moving Polar Alt to %.3f", values[0]);
            snprintf(read_buffer, sizeof(read_buffer), ":MAL%.3f#", values[0]);
            executeMeadeCommandBlind(read_buffer);
            PolarActive = true;
            PolarAlignAltNP.s = IPS_BUSY;
            IDSetNumber(&PolarAlignAltNP, nullptr);
            return true;
        }
        if (!strcmp(name, PolarAlignAzN.name))
        {
            /*if (IUUpdateNumber(&PolarAlignAzNP, values, names, n) < 0)
                return false;
            */
            LOGF_WARN("Moving Polar Az to %.3f", values[0]);
            snprintf(read_buffer, sizeof(read_buffer), ":MAZ%.3f#", values[0]);
            executeMeadeCommandBlind(read_buffer);
            PolarActive = true;
            PolarAlignAzNP.s = IPS_BUSY;
            IDSetNumber(&PolarAlignAzNP, nullptr);
            return true;
        }
    }

    return LX200GPS::ISNewNumber(dev, name, values, names, n);
}

bool LX200_OpenAstroTech::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int index = 0;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, HomeS.name))
        {
            HomeSP.s = IPS_IDLE;
            IUResetSwitch(&HomeSP);
            int targetState = IUFindOnSwitchIndex(&HomeSP);
            return executeMeadeCommandBlind(":hF#");
        }
    }
    return LX200GPS::ISNewSwitch(dev, name, states, names, n);
}

const char *LX200_OpenAstroTech::getDefaultName(void)
{
    return const_cast<const char *>("LX200 OpenAstroTech");
}

int LX200_OpenAstroTech::executeMeadeCommand(const char *cmd, char *data)
{
    bool wait = true, getchar = false;
    int len = strlen(cmd);
    data[0] = 0;
    if(len > 2) {
        if(cmd[1] == 'F') { // Fx except for Fp
            if(!(cmd[2] == 'p' || cmd[2] == 'B')) {
                wait = false;
            } else if(cmd[2] == 'B') {
                getchar = true;
            }
        } else if(cmd[1] == 'M' && cmd[2] == 'A') { // MAL, MAZ
            wait = false;
        }
    }
    if(!wait) {
        return executeMeadeCommandBlind(cmd);
    }
    int err;
    if(getchar) {
        int val = getCommandChar(PortFD, cmd);
        sprintf(data, "%c", val);
    } else {
        err = getCommandString(PortFD, data, cmd);
    }
    if(err) {
        LOGF_WARN("Executed Meade Command error: %d %s -> '%s'", err, cmd, data);
    } else {
        LOGF_INFO("Executed Meade Command: %d %s -> '%s'", wait, cmd, data);
    }
    return err;
}

// stupid meade protocol... why not just always add a # after a result??
// now you have to know what each command will return
char LX200_OpenAstroTech::getCommandChar(int fd, const char * cmd)
{
    char read_buffer[RB_MAX_LEN] = {0};
    int nbytes_write = 0, nbytes_read = 0, error_type;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    //LOGF_INFO("getCommandChar 1: %s -> '%s'", cmd, read_buffer);
    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) == TTY_OK) {
        error_type = tty_read(fd, read_buffer, 1, 5, &nbytes_read);
        //LOGF_INFO("getCommandChar 2: %s -> '%s'", cmd, read_buffer);
        if (nbytes_read == 1)
            LOGF_INFO("getCommandChar 3: %s -> '%s'", cmd, read_buffer);
            return read_buffer[0];
    }

    LOGF_WARN("getCommandChar error: %d %s -> '%s'", error_type, cmd, read_buffer);
    return -1;
}

bool LX200_OpenAstroTech::executeMeadeCommandBlind(const char *cmd)
{
    int error_type;
    int nbytes_write = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    flushIO(PortFD);
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(PortFD, TCIFLUSH);

    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK) {
        LOGF_ERROR("CHECK CONNECTION: Error sending command %s", cmd);
        return 1;
    }
    LOGF_INFO("Executed Meade Command Immediate: %s", cmd);
    return 0;
}

int LX200_OpenAstroTech::flushIO(int fd)
{
    tcflush(fd, TCIOFLUSH);
    int error_type = 0;
    int nbytes_read;
    std::unique_lock<std::mutex> guard(lx200CommsLock);
    tcflush(fd, TCIOFLUSH);
    do {
        char discard_data[RB_MAX_LEN] = {0};
        error_type = tty_read_section_expanded(fd, discard_data, '#', 0, 1000, &nbytes_read);
        if (error_type >= 0) {
            LOGF_DEBUG("flushIO: Information in buffer: Bytes: %u, string: %s", nbytes_read, discard_data);
        }
        //LOGF_DEBUG("flushIO: error_type = %i", error_type);
    } while (error_type > 0);
    return 0;
}

IPState LX200_OpenAstroTech::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    INDI_UNUSED(speed);
    //  :FRsnnn#  Set focuser target position relative (in microns)
    //            Returns: Nothing
    LOGF_ERROR("MoveFocuser shouldn't be called: %d at %d for %d", dir, speed, duration);
    double output;
    char read_buffer[32];
    output = duration;
    if (dir != FocuserDirectionLast) {
        FocuserDirectionLast = dir;
        LOGF_INFO("Applying backlash %d to %d", FocuserBacklash, output);
        output += FocuserBacklash;
    }
    if (dir == FOCUS_INWARD) output = 0 - output;
    snprintf(read_buffer, sizeof(read_buffer), ":FM%f#", output);
    executeMeadeCommandBlind(read_buffer);
    return IPS_BUSY; // Normal case, should be set to normal by update.
}

IPState LX200_OpenAstroTech::MoveAbsFocuser (uint32_t targetTicks)
{
    if (FocusAbsPosN[0].max >= int(targetTicks) && FocusAbsPosN[0].min <= int(targetTicks))
    {
        char read_buffer[32];
        snprintf(read_buffer, sizeof(read_buffer), ":Fp#");
        if(!executeMeadeCommand(read_buffer, read_buffer)) {
            uint32_t currentTicks = atoi(read_buffer);
            if(FocuserDirectionLast == FOCUS_INWARD && targetTicks > currentTicks) {
                targetTicks += FocuserBacklash;
                FocuserDirectionLast = FOCUS_OUTWARD;
            } else if(FocuserDirectionLast == FOCUS_OUTWARD && targetTicks < currentTicks) {
                targetTicks -= FocuserBacklash;
                FocuserDirectionLast = FOCUS_INWARD;
            }
            uint32_t relTicks = targetTicks - currentTicks;

            char read_buffer[32];
            snprintf(read_buffer, sizeof(read_buffer), ":FM%d#", int(relTicks));
            executeMeadeCommandBlind(read_buffer);
            return IPS_BUSY; // Normal case, should be set to normal by update.
        }
    }
    else
    {
        LOG_INFO("Unable to move focuser, out of range");
        return IPS_ALERT;
    }
}

IPState LX200_OpenAstroTech::MoveRelFocuser (FocusDirection dir, uint32_t ticks)
{
    //  :FMsnnn#  Set focuser target position relative (in microns)
    //            Returns: Nothing
    char read_buffer[32];
    if (dir != FocuserDirectionLast) {
        FocuserDirectionLast = dir;
        LOGF_INFO("Applying backlash %d to %d", FocuserBacklash, ticks);
        ticks += FocuserBacklash;
    }
    int output = ticks;
    if (dir == FOCUS_INWARD) output = 0 - ticks;
    snprintf(read_buffer, sizeof(read_buffer), ":FM%d#", output);
    executeMeadeCommandBlind(read_buffer);
    return IPS_BUSY; // Normal case, should be set to normal by update.
}

bool LX200_OpenAstroTech::SetFocuserBacklash(int32_t steps)
{

    LOGF_INFO("Set backlash %d", steps);
    FocuserBacklash = steps;
    return true;
}

bool LX200_OpenAstroTech::AbortFocuser ()
{
    //  :FQ#   Stop the focuser
    //         Returns: Nothing
    char cmd[CMD_MAX_LEN] = {0};
    strncpy(cmd, ":FQ#", sizeof(cmd));
    executeMeadeCommandBlind(cmd);
    return IPS_OK;
}


void LX200_OpenAstroTech::initFocuserProperties(const char * groupName)
{
    IUFillNumber(&FocusSpeedN[0], "FOCUS_SPEED_VALUE", "Focus Speed", "%3.0f", 0.0, 4.0, 1.0, 2.0);
    IUFillNumberVector(&FocusSpeedNP, FocusSpeedN, 1, m_defaultDevice->getDeviceName(), "FOCUS_SPEED", "Speed", groupName,
                       IP_RW, 60, IPS_OK);

    IUFillNumber(&FocusTimerN[0], "FOCUS_TIMER_VALUE", "Focus Timer (ms)", "%4.0f", 0.0, 5000.0, 50.0, 1000.0);
    IUFillNumberVector(&FocusTimerNP, FocusTimerN, 1, m_defaultDevice->getDeviceName(), "FOCUS_TIMER", "Timer", groupName,
                       IP_RW, 60, IPS_OK);
    lastTimerValue = 1000.0;

    // Absolute Position
    IUFillNumber(&FocusAbsPosN[0], "FOCUS_ABSOLUTE_POSITION", "Steps", "%.f", 0.0, 100000.0, 100.0, 0);
    IUFillNumberVector(&FocusAbsPosNP, FocusAbsPosN, 1, m_defaultDevice->getDeviceName(), "ABS_FOCUS_POSITION",
                       "Absolute Position",
                       groupName, IP_RW, 60, IPS_OK);

    // Relative Position
    IUFillNumber(&FocusRelPosN[0], "FOCUS_RELATIVE_POSITION", "Steps", "%.f", 0.0, 100000.0, 100.0, 0);
    IUFillNumberVector(&FocusRelPosNP, FocusRelPosN, 1, m_defaultDevice->getDeviceName(), "REL_FOCUS_POSITION",
                       "Relative Position",
                       groupName, IP_RW, 60, IPS_OK);

    // Sync
    IUFillNumber(&FocusSyncN[0], "FOCUS_SYNC_VALUE", "Steps", "%.f", 0.0, 100000.0, 1000.0, 0);
    IUFillNumberVector(&FocusSyncNP, FocusSyncN, 1, m_defaultDevice->getDeviceName(), "FOCUS_SYNC", "Sync",
                       groupName, IP_RW, 60, IPS_OK);

    // Maximum Position
    IUFillNumber(&FocusMaxPosN[0], "FOCUS_MAX_VALUE", "Steps", "%.f", 1000.0, 100000.0, 10000.0, 50000.0);
    IUFillNumberVector(&FocusMaxPosNP, FocusMaxPosN, 1, m_defaultDevice->getDeviceName(), "FOCUS_MAX", "Max. Position",
                       groupName, IP_RW, 60, IPS_OK);

    // Abort
    IUFillSwitch(&FocusAbortS[0], "ABORT", "Abort", ISS_OFF);
    IUFillSwitchVector(&FocusAbortSP, FocusAbortS, 1, m_defaultDevice->getDeviceName(), "FOCUS_ABORT_MOTION", "Abort Motion",
                       groupName, IP_RW,
                       ISR_ATMOST1, 60, IPS_IDLE);

    // Revese
    IUFillSwitch(&FocusReverseS[DefaultDevice::INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&FocusReverseS[DefaultDevice::INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&FocusReverseSP, FocusReverseS, 2, m_defaultDevice->getDeviceName(), "FOCUS_REVERSE_MOTION",
                       "Reverse Motion", groupName, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    // Backlash Compensation
    IUFillSwitch(&FocusBacklashS[DefaultDevice::INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&FocusBacklashS[DefaultDevice::INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&FocusBacklashSP, FocusBacklashS, 2, m_defaultDevice->getDeviceName(), "FOCUS_BACKLASH_TOGGLE",
                       "Backlash", groupName, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    // Backlash Compensation Value
    IUFillNumber(&FocusBacklashN[0], "FOCUS_BACKLASH_VALUE", "Steps", "%.f", 0, 1000.0, 100, 0);
    IUFillNumberVector(&FocusBacklashNP, FocusBacklashN, 1, m_defaultDevice->getDeviceName(), "FOCUS_BACKLASH_STEPS",
                       "Backlash",
                       groupName, IP_RW, 60, IPS_OK);
}

int LX200_OpenAstroTech::OATUpdateFocuser()
{
    // we don't need to poll if we're not moving
    if(!(FocusRelPosNP.s == IPS_BUSY || FocusAbsPosNP.s == IPS_BUSY)) {
        return 0;
    }

    tcflush(PortFD, TCIOFLUSH);
    flushIO(PortFD);
   
    // get position
    char value[RB_MAX_LEN] = {0};
    if (!getCommandString(PortFD, value, ":Fp#")) {
        FocusAbsPosN[0].value =  atof(value);
        IDSetNumber(&FocusAbsPosNP, nullptr);
        LOGF_INFO("Current focuser: %d, %f from %s", FocusAbsPosN[0].value, FocusAbsPosN[0].value, value);
    }
    // get is_moving
    char valueStatus = getCommandChar(PortFD, ":FB#");
    if (valueStatus == '0')
    {
        FocusRelPosNP.s = IPS_OK;
        IDSetNumber(&FocusRelPosNP, nullptr);
        FocusAbsPosNP.s = IPS_OK;
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }
    else if (valueStatus == '1')
    {
        FocusRelPosNP.s = IPS_BUSY;
        IDSetNumber(&FocusRelPosNP, nullptr);
        FocusAbsPosNP.s = IPS_BUSY;
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }
    else
    {
        LOGF_WARN("Communication :FB# error, check connection: %d", valueStatus);
        //INVALID REPLY
        FocusRelPosNP.s = IPS_ALERT;
        IDSetNumber(&FocusRelPosNP, nullptr);
        FocusAbsPosNP.s = IPS_ALERT;
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }
    FI::updateProperties();
    LOGF_DEBUG("After update properties: FocusAbsPosN min: %f max: %f", FocusAbsPosN[0].min, FocusAbsPosN[0].max);
    return 0;
}
