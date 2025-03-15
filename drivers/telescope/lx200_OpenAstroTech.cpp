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

    SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_CAN_HOME_GO, 4);
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

    // Set focuser capabilities
    FI::SetCapability(FOCUSER_CAN_ABORT | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_REVERSE | FOCUSER_HAS_VARIABLE_SPEED |
                      FOCUSER_HAS_BACKLASH | FOCUSER_CAN_SYNC);

    // Initialize focuser properties
    FI::initProperties(FOCUS_TAB);

    // Set custom ranges for focuser properties
    FocusSpeedNP[0].setMinMax(0.0, 4.0);
    FocusSpeedNP[0].setStep(1.0);
    FocusSpeedNP[0].setValue(2.0);

    FocusTimerNP[0].setMinMax(0.0, 5000.0);
    FocusTimerNP[0].setStep(50.0);
    FocusTimerNP[0].setValue(1000.0);
    lastTimerValue = 1000.0;

    FocusAbsPosNP[0].setMinMax(0.0, 100000.0);
    FocusAbsPosNP[0].setStep(100.0);
    FocusAbsPosNP[0].setValue(0);

    FocusRelPosNP[0].setMinMax(0.0, 100000.0);
    FocusRelPosNP[0].setStep(100.0);
    FocusRelPosNP[0].setValue(0);

    FocusSyncNP[0].setMinMax(0.0, 100000.0);
    FocusSyncNP[0].setStep(1000.0);
    FocusSyncNP[0].setValue(0);

    FocusMaxPosNP[0].setMinMax(1000.0, 100000.0);
    FocusMaxPosNP[0].setStep(10000.0);
    FocusMaxPosNP[0].setValue(50000.0);

    FocusBacklashNP[0].setMinMax(0, 5000.0);
    FocusBacklashNP[0].setStep(100);
    FocusBacklashNP[0].setValue(0);

    // Polar Align Alt
    IUFillNumber(&PolarAlignAltN, "OAT_POLAR_ALT", "Arcmin", "%.f", -140.0, 140.0, 1.0, 0);
    IUFillNumberVector(&PolarAlignAltNP, &PolarAlignAltN, 1, getDeviceName(), "POLAR_ALT",
                       "Polar Align Alt",
                       MOTION_TAB, IP_RW, 60, IPS_OK);
    // Polar Align Az
    IUFillNumber(&PolarAlignAzN, "OAT_POLAR_AZ", "Arcmin", "%.f", -320.0, 320.0, 1.0, 0);
    IUFillNumberVector(&PolarAlignAzNP, &PolarAlignAzN, 1, getDeviceName(), "POLAR_AZ",
                       "Polar Align Azimuth",
                       MOTION_TAB, IP_RW, 60, IPS_OK);

    // Home
    // IUFillSwitch(&HomeS, "OAT_HOME", "Home", ISS_OFF);
    // IUFillSwitchVector(&HomeSP, &HomeS, 1, getDeviceName(),
    //                    "OAT_HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // RA Home
    IUFillNumber(&RAHomeN, "RA_HOME", "Hours", "%d", 1.0, 7.0, 1.0, 2);
    IUFillNumberVector(&RAHomeNP, &RAHomeN, 1, getDeviceName(),
                       "OAT_RA_HOME", "RA Home", MOTION_TAB, IP_RW, 60, IPS_IDLE);

    // RA Home
    IUFillNumber(&RAHomeOffsetN, "OAT_RA_HOME_OFFSET", "Steps", "%d", -10000.0, 10000.0, 100.0, 0);
    IUFillNumberVector(&RAHomeOffsetNP, &RAHomeOffsetN, 1, getDeviceName(),
                       "OAT_RA_HOME", "RA Home Offset", MOTION_TAB, IP_RW, 60, IPS_IDLE);

    // DEC Limits
    IUFillNumber(&DecLimitsN[0], "OAT_DEC_LIMIT_LOWER", "Lower", "%.f", 0.0, -50.0, 0.0, 0);
    IUFillNumber(&DecLimitsN[1], "OAT_DEC_LIMIT_UPPER", "Upper", "%.f", 0.0, 180.0, 120.0, 0);
    IUFillNumberVector(&DecLimitsNP, DecLimitsN, 2, getDeviceName(), "OAT_DEC_LIMITS",
                       "DEC Limits", MOTION_TAB, IP_RW, 60, IPS_OK);
    // SetParkDataType(PARK_RA_DEC);
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
        // defineProperty(&HeaterNP);
        //defineProperty(&HomeSP);
        defineProperty(&RAHomeNP);
        defineProperty(&RAHomeOffsetNP);
        defineProperty(&DecLimitsNP);
    }
    else
    {
        deleteProperty(MeadeCommandTP.name);
        deleteProperty(PolarAlignAltNP.name);
        deleteProperty(PolarAlignAzNP.name);
        // deleteProperty(HeaterNP.name);
        //deleteProperty(HomeSP.name);
        deleteProperty(RAHomeNP.name);
        deleteProperty(RAHomeOffsetNP.name);
        deleteProperty(DecLimitsNP.name);
    }

    return true;
}

bool LX200_OpenAstroTech::ReadScopeStatus()
{
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
            ))
    {
        return 0;
    }

    tcflush(PortFD, TCIOFLUSH);
    flushIO(PortFD);

    char value[RB_MAX_LEN] = {0};
    int rc = executeMeadeCommand(":GX#", value);
    if (rc == 0 && strlen(value) > 10)
    {
        const char *motors = strchr(value, ',') + 1;
        if(PolarAlignAzNP.s == IPS_BUSY && motors[3] == '-')
        {
            PolarAlignAzNP.s = IPS_OK;
            IDSetNumber(&PolarAlignAzNP, nullptr);
        }
        if(PolarAlignAltNP.s == IPS_BUSY && motors[4] == '-')
        {
            PolarAlignAltNP.s = IPS_OK;
            IDSetNumber(&PolarAlignAltNP, nullptr);
        }
        if(RAHomeNP.s == IPS_BUSY && value[0] == 'H')
        {
            RAHomeNP.s = IPS_IDLE;
            IDSetNumber(&RAHomeNP, nullptr);
        }
    }
    // updateProperties();
    return 0;
}

bool LX200_OpenAstroTech::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, MeadeCommandTP.name))
        {
            if(!isSimulation())
            {
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
                if(len > 2)
                {
                    IText *tp = IUFindText(&MeadeCommandTP, names[0]);
                    MeadeCommandResult[0] = 0;
                    int err = 0;
                    if(cmd[0] == ':' && cmd[len - 1] == '#')
                    {
                        err = executeMeadeCommand(cmd, MeadeCommandResult);
                    }
                    else if(cmd[0] == '@' && cmd[len - 1] == '#')
                    {
                        cmd[0] = ':';
                        err = executeMeadeCommandBlind(cmd);
                    }
                    else if(cmd[0] == '&' && cmd[len - 1] == '#')
                    {
                        cmd[0] = ':';
                        int val = getCommandChar(PortFD, cmd);
                        if(val != -1)
                        {
                            sprintf(MeadeCommandResult, "%c", val);
                        }
                    }
                    DEBUGFDEVICE(getDeviceName(), DBG_SCOPE, "Meade Command Result %d <%s>", err, MeadeCommandResult);
                    if(err == 0)
                    {
                        MeadeCommandTP.s = IPS_OK;
                    }
                    else
                    {
                        MeadeCommandTP.s = IPS_ALERT;
                    }
                    IUSaveText(tp, MeadeCommandResult);
                    IDSetText(&MeadeCommandTP, "%s", MeadeCommandResult);
                    return true;
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
        if (!strcmp(name, PolarAlignAltN.name) || !strcmp(name, PolarAlignAltNP.name))
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
        if (!strcmp(name, PolarAlignAzN.name) || !strcmp(name, PolarAlignAzNP.name))
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

// bool LX200_OpenAstroTech::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
// {
//     if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
//     {
//         if (!strcmp(name, HomeS.name))
//         {
//             HomeSP.s = IPS_OK;
//             IUResetSwitch(&HomeSP);
//             /*int targetState = */IUFindOnSwitchIndex(&HomeSP);
//             IDSetSwitch(&HomeSP, nullptr);
//             return executeMeadeCommandBlind(":hF#");
//         }
//     }
//     return LX200GPS::ISNewSwitch(dev, name, states, names, n);
// }

const char *LX200_OpenAstroTech::getDefaultName(void)
{
    return "LX200 OpenAstroTech";
}

int LX200_OpenAstroTech::executeMeadeCommand(const char *cmd, char *data)
{
    bool wait = true, getchar = false;
    int len = strlen(cmd);
    data[0] = 0;
    if(len > 2)
    {
        if(cmd[1] == 'F')   // Fx except for Fp
        {
            if(!(cmd[2] == 'p' || cmd[2] == 'B'))
            {
                wait = false;
            }
            else if(cmd[2] == 'B')
            {
                getchar = true;
            }
        }
        else if(cmd[1] == 'M' && cmd[2] == 'A') // MAL, MAZ
        {
            wait = false;
        }
        else if(cmd[1] == 'M' && cmd[2] == 'X') // MXxnnnnn#
        {
            getchar = true;
        }
        else if(cmd[1] == 'M' && (cmd[2] == 'g' || cmd[2] == 'G')) // Mgnxxxx#
        {
            wait = false;
        }
        else if(cmd[1] == 'S' && cmd[2] != 'C') // SCMM/DD/YY#
        {
            getchar = true;
        }
        else if(cmd[1] == 'X' && cmd[2] == 'S') // :XSRn.n# :XSDn.n# :XS...
        {
            wait = false;
        }
    }
    if(!wait)
    {
        return executeMeadeCommandBlind(cmd);
    }
    int err = 0;
    if(getchar)
    {
        int val = getCommandChar(PortFD, cmd);
        if(0 < val)
        {
            sprintf(data, "%c", val);
        }
        else if(0 == val)
        {
            sprintf(data, "%s", "null");
        }
        else
        {
            err = val;
        }
    }
    else
    {
        err = getCommandString(PortFD, data, cmd);
    }
    if(err)
    {
        LOGF_WARN("Executed Meade Command error: %d %s -> '%s'", err, cmd, data);
    }
    else
    {
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
    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) == TTY_OK)
    {
        error_type = tty_read(fd, read_buffer, 1, 5, &nbytes_read);
        //LOGF_INFO("getCommandChar 2: %s -> '%s'", cmd, read_buffer);
        if (nbytes_read == 1)
        {
            LOGF_INFO("getCommandChar 3: %s -> '%s'", cmd, read_buffer);
            return read_buffer[0];
        }
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

    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
    {
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
    do
    {
        char discard_data[RB_MAX_LEN] = {0};
        error_type = tty_read_section_expanded(fd, discard_data, '#', 0, 1000, &nbytes_read);
        if (error_type >= 0)
        {
            LOGF_DEBUG("flushIO: Information in buffer: Bytes: %u, string: %s", nbytes_read, discard_data);
        }
        //LOGF_DEBUG("flushIO: error_type = %i", error_type);
    }
    while (error_type > 0);
    return 0;
}

IPState LX200_OpenAstroTech::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    int reversed = (FocusReverseSP.findOnSwitchIndex() == INDI_ENABLED) ? -1 : 1;
    INDI_UNUSED(speed);
    //  :FRsnnn#  Set focuser target position relative (in microns)
    //            Returns: Nothing
    LOGF_ERROR("MoveFocuser shouldn't be called: %d at %d for %d", dir, speed, duration);
    double output;
    char read_buffer[32];
    output = duration;
    if (dir != FocuserDirectionLast)
    {
        FocuserDirectionLast = dir;
        LOGF_INFO("Applying backlash %d to %d", FocuserBacklash, output);
        output += FocuserBacklash;
    }
    if (dir == FOCUS_INWARD) output = 0 - output;
    snprintf(read_buffer, sizeof(read_buffer), ":FM%f#", output * reversed);
    executeMeadeCommandBlind(read_buffer);
    return IPS_BUSY; // Normal case, should be set to normal by update.
}

IPState LX200_OpenAstroTech::MoveAbsFocuser (uint32_t targetTicks)
{
    if (FocusAbsPosNP[0].getMax() >= int(targetTicks) && FocusAbsPosNP[0].getMin() <= int(targetTicks))
    {
        char read_buffer[32];
        snprintf(read_buffer, sizeof(read_buffer), ":Fp#");
        if(!executeMeadeCommand(read_buffer, read_buffer))
        {
            uint32_t currentTicks = atoi(read_buffer);
            if(FocuserDirectionLast == FOCUS_INWARD && targetTicks > currentTicks)
            {
                targetTicks += FocuserBacklash;
                FocuserDirectionLast = FOCUS_OUTWARD;
            }
            else if(FocuserDirectionLast == FOCUS_OUTWARD && targetTicks < currentTicks)
            {
                targetTicks -= FocuserBacklash;
                FocuserDirectionLast = FOCUS_INWARD;
            }
            uint32_t relTicks = targetTicks - currentTicks;

            char read_buffer[32];
            snprintf(read_buffer, sizeof(read_buffer), ":FM%d#", int(relTicks));
            executeMeadeCommandBlind(read_buffer);
            return IPS_BUSY; // Normal case, should be set to normal by update.
        }
        return IPS_ALERT;
    }
    else
    {
        LOG_INFO("Unable to move focuser, out of range");
        return IPS_ALERT;
    }
}

IPState LX200_OpenAstroTech::MoveRelFocuser (FocusDirection dir, uint32_t ticks)
{
    int reversed = (FocusReverseSP.findOnSwitchIndex() == INDI_ENABLED) ? -1 : 1;
    //  :FMsnnn#  Set focuser target position relative (in microns)
    //            Returns: Nothing
    char read_buffer[64];
    if (dir != FocuserDirectionLast)
    {
        FocuserDirectionLast = dir;
        LOGF_INFO("Applying backlash %d to %d", FocuserBacklash, ticks);
        ticks += FocuserBacklash;
    }
    int output = ticks;
    if (dir == FOCUS_INWARD) output = 0 - ticks;
    snprintf(read_buffer, sizeof(read_buffer), ":FM%d#", output * reversed);
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

bool LX200_OpenAstroTech::SyncFocuser(uint32_t ticks)
{
    // :FPnnn#
    //      Description:
    //        Set position
    //      Information:
    //        Sets the current position of the focus stepper motor
    //      Returns:
    //        "1"
    char cmd[CMD_MAX_LEN] = {0};
    snprintf(cmd, sizeof(cmd), ":FP%d#", int(ticks));
    int val = getCommandChar(PortFD, cmd);
    return val == '1';
}

// initFocuserProperties function removed as we're using FocuserInterface properties

int LX200_OpenAstroTech::OATUpdateFocuser()
{
    // we don't need to poll if we're not moving
    if(!(FocusRelPosNP.getState() == IPS_BUSY || FocusAbsPosNP.getState() == IPS_BUSY) && FocusAbsPosNP[0].getValue() != 0.0)
    {
        return 0;
    }

    tcflush(PortFD, TCIOFLUSH);
    flushIO(PortFD);
    // get position
    char value[RB_MAX_LEN] = {0};
    if (!getCommandString(PortFD, value, ":Fp#"))
    {
        FocusAbsPosNP[0].setValue(atof(value));
        FocusSyncNP[0].setValue(atof(value));
        FocusAbsPosNP.apply();
        FocusSyncNP.apply();
        LOGF_INFO("Current focuser: %f", FocusAbsPosNP[0].getValue());
    }
    // get is_moving
    char valueStatus = getCommandChar(PortFD, ":FB#");
    if (valueStatus == '0')
    {
        FocusRelPosNP.setState(IPS_OK);
        FocusRelPosNP.apply();
        FocusAbsPosNP.setState(IPS_OK);
        FocusAbsPosNP.apply();
    }
    else if (valueStatus == '1')
    {
        FocusRelPosNP.setState(IPS_BUSY);
        FocusRelPosNP.apply();
        FocusAbsPosNP.setState(IPS_BUSY);
        FocusAbsPosNP.apply();
    }
    else
    {
        LOGF_WARN("Communication :FB# error, check connection: %d", valueStatus);
        //INVALID REPLY
        FocusRelPosNP.setState(IPS_ALERT);
        FocusRelPosNP.apply();
        FocusAbsPosNP.setState(IPS_ALERT);
        FocusAbsPosNP.apply();
    }
    FI::updateProperties();
    LOGF_DEBUG("After update properties: FocusAbsPosN min: %f max: %f", FocusAbsPosNP[0].getMin(), FocusAbsPosNP[0].getMax());
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState LX200_OpenAstroTech::ExecuteHomeAction(TelescopeHomeAction action)
{
    switch (action)
    {
        case HOME_GO:
            return executeMeadeCommandBlind(":hF#") ? IPS_BUSY : IPS_ALERT;

        default:
            return IPS_ALERT;
    }
}
