/*
    Deep Sky Dad AF1 focuser

    Copyright (C) 2019 Pavle Gartner

    Based on Moonline driver.
    Copyright (C) 2013-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "deepskydad_af1.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<DeepSkyDadAF1> deepSkyDadAf1(new DeepSkyDadAF1());

DeepSkyDadAF1::DeepSkyDadAF1()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_SYNC | FOCUSER_CAN_REVERSE | FOCUSER_CAN_ABORT);
}

bool DeepSkyDadAF1::initProperties()
{
    INDI::Focuser::initProperties();

    // Step Mode
    IUFillSwitch(&StepModeS[EIGHT], "EIGHT", "Eight Step", ISS_OFF);
    IUFillSwitch(&StepModeS[QUARTER], "QUARTER", "Quarter Step", ISS_OFF);
    IUFillSwitch(&StepModeS[HALF], "HALF", "Half Step", ISS_OFF);
    IUFillSwitch(&StepModeS[FULL], "FULL", "Full Step", ISS_OFF);
    IUFillSwitchVector(&StepModeSP, StepModeS, 4, getDeviceName(), "Step Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 5000.;
    FocusRelPosN[0].value = 0.;
    FocusRelPosN[0].step = 10.;

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = 100000.;
    FocusAbsPosN[0].value = 50000.;
    FocusAbsPosN[0].step = 500.;

    // Max. movement
    IUFillNumber(&FocusMaxMoveN[0], "MAX_MOVE", "Steps", "%7.0f", 0, 9999999, 100, 0);
    IUFillNumberVector(&FocusMaxMoveNP, FocusMaxMoveN, 1, getDeviceName(), "FOCUS_MAX_MOVE", "Max. movement",
                       MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Settle buffer
    IUFillNumber(&SettleBufferN[0], "SETTLE_BUFFER", "Period (ms)", "%5.0f", 0, 99999, 100, 0);
    IUFillNumberVector(&SettleBufferNP, SettleBufferN, 1, getDeviceName(), "FOCUS_SETTLE_BUFFER", "Settle buffer",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Idle coils timeout (ms)
    IUFillNumber(&IdleCoilsTimeoutN[0], "IDLE_COILS_TIMEOUT", "Period (ms)", "%6.0f", 0, 999999, 1000, 60000);
    IUFillNumberVector(&IdleCoilsTimeoutNP, IdleCoilsTimeoutN, 1, getDeviceName(), "FOCUS_IDLE_COILS_TIMEOUT", "Idle - coils timeout",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Coils mode
    IUFillSwitch(&CoilsModeS[ALWAYS_ON], "ALWAYS_ON", "Always on", ISS_OFF);
    IUFillSwitch(&CoilsModeS[IDLE_OFF], "IDLE_OFF", "Idle - off", ISS_OFF);
    IUFillSwitch(&CoilsModeS[IDLE_COILS_TIMEOUT], "IDLE_COILS_TIMEOUT", "Idle - coils timeout (ms)", ISS_OFF);
    IUFillSwitchVector(&CoilsModeSP, CoilsModeS, 3, getDeviceName(), "Coils mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Current move
    IUFillSwitch(&CurrentMoveS[CURRENT_25], "CMV_25", "25%", ISS_OFF);
    IUFillSwitch(&CurrentMoveS[CURRENT_50], "CMV_50", "50%", ISS_OFF);
    IUFillSwitch(&CurrentMoveS[CURRENT_75], "CMV_75", "75%", ISS_OFF);
    IUFillSwitch(&CurrentMoveS[CURRENT_100], "CMV_100", "100%", ISS_OFF);
    IUFillSwitchVector(&CurrentMoveSP, CurrentMoveS, 4, getDeviceName(), "Current - move", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Current hold
    IUFillSwitch(&CurrentHoldS[CURRENT_25], "CHD_25", "25%", ISS_OFF);
    IUFillSwitch(&CurrentHoldS[CURRENT_50], "CHD_50", "50%", ISS_OFF);
    IUFillSwitch(&CurrentHoldS[CURRENT_75], "CHD_75", "75%", ISS_OFF);
    IUFillSwitch(&CurrentHoldS[CURRENT_100], "CHD_100", "100%", ISS_OFF);
    IUFillSwitchVector(&CurrentHoldSP, CurrentHoldS, 4, getDeviceName(), "Current - hold", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    setDefaultPollingPeriod(500);
    addDebugControl();

    return true;
}

bool DeepSkyDadAF1::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&FocusMaxMoveNP);
        defineProperty(&StepModeSP);
        defineProperty(&SettleBufferNP);
        defineProperty(&CoilsModeSP);
        defineProperty(&IdleCoilsTimeoutNP);
        defineProperty(&CurrentMoveSP);
        defineProperty(&CurrentHoldSP);

        GetFocusParams();

        LOG_INFO("deepSkyDadAf1 parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(FocusMaxMoveNP.name);
        deleteProperty(StepModeSP.name);
        deleteProperty(SettleBufferNP.name);
        deleteProperty(CoilsModeSP.name);
        deleteProperty(IdleCoilsTimeoutNP.name);
        deleteProperty(CurrentMoveSP.name);
        deleteProperty(CurrentHoldSP.name);
    }

    return true;
}

bool DeepSkyDadAF1::Handshake()
{
    if (Ack())
    {
        LOG_INFO("deepSkyDadAf1 is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO(
        "Error retrieving data from deepSkyDadAf1, please ensure deepSkyDadAf1 controller is powered and the port is correct.");
    return false;
}

const char * DeepSkyDadAF1::getDefaultName()
{
    return "Deep Sky Dad AF1";
}

bool DeepSkyDadAF1::Ack()
{
    sleep(2);

    char res[DSD_RES] = {0};
    if (!sendCommand("[GPOS]", res))
    {
        LOG_ERROR("ACK - getPosition failed");
        return false;
    }

    int32_t pos;
    int rc = sscanf(res, "(%d)", &pos);

    if (rc <= 0)
    {
        LOG_ERROR("ACK - getPosition failed");
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readStepMode()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GSTP]", res) == false)
        return false;

    if (strcmp(res, "(1)") == 0)
        StepModeS[FULL].s = ISS_ON;
    else if (strcmp(res, "(2)") == 0)
        StepModeS[HALF].s = ISS_ON;
    else if (strcmp(res, "(4)") == 0)
        StepModeS[QUARTER].s = ISS_ON;
    else if (strcmp(res, "(8)") == 0)
        StepModeS[EIGHT].s = ISS_ON;
    else
    {
        LOGF_ERROR("Unknown error: focuser step value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readPosition()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GPOS]", res) == false)
        return false;

    int32_t pos;
    int rc = sscanf(res, "(%d)", &pos);

    if (rc > 0)
        FocusAbsPosN[0].value = pos;
    else
    {
        LOGF_ERROR("Unknown error: focuser position value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readMaxMovement()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GMXM]", res) == false)
        return false;

    uint32_t steps = 0;
    int rc = sscanf(res, "(%d)", &steps);
    if (rc > 0)
    {
        FocusMaxMoveN[0].value = steps;
        FocusMaxMoveNP.s = IPS_OK;
    }
    else
    {
        LOGF_ERROR("Unknown error: maximum movement value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readMaxPosition()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GMXP]", res) == false)
        return false;

    uint32_t steps = 0;
    int rc = sscanf(res, "(%d)", &steps);
    if (rc > 0)
    {
        FocusMaxPosN[0].value = steps;
        FocusMaxPosNP.s = IPS_OK;
    }
    else
    {
        LOGF_ERROR("Unknown error: maximum position value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readSettleBuffer()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GBUF]", res) == false)
        return false;

    uint32_t settleBuffer = 0;
    int rc = sscanf(res, "(%d)", &settleBuffer);
    if (rc > 0)
    {
        SettleBufferN[0].value = settleBuffer;
        SettleBufferNP.s = settleBuffer > 0 ? IPS_OK : IPS_IDLE;
    }
    else
    {
        LOGF_ERROR("Unknown error: settle buffer value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readIdleCoilsTimeout()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GIDC]", res) == false)
        return false;

    uint32_t ms = 0;
    int rc = sscanf(res, "(%d)", &ms);
    if (rc > 0)
    {
        IdleCoilsTimeoutN[0].value = ms;
        IdleCoilsTimeoutNP.s = ms > 0 ? IPS_OK : IPS_IDLE;
    }
    else
    {
        LOGF_ERROR("Unknown error: idle coils timeout value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readCoilsMode()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GCLM]", res) == false)
        return false;

    if (strcmp(res, "(0)") == 0)
    {
        CoilsModeSP.s = IPS_IDLE;
        CoilsModeS[IDLE_OFF].s = ISS_ON;
    }
    else if (strcmp(res, "(1)") == 0)
    {
        CoilsModeSP.s = IPS_OK;
        CoilsModeS[ALWAYS_ON].s = ISS_ON;
    }
    else if (strcmp(res, "(2)") == 0)
    {
        CoilsModeSP.s = IPS_IDLE;
        CoilsModeS[IDLE_COILS_TIMEOUT].s = ISS_ON;
    }
    else
    {
        LOGF_ERROR("Unknown error: readCoilsMode value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readCurrentMove()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GCMV%]", res) == false)
        return false;

    if (strcmp(res, "(25%)") == 0)
    {
        CurrentMoveSP.s = IPS_OK;
        CurrentMoveS[CURRENT_25].s = ISS_ON;
    }
    else if (strcmp(res, "(50%)") == 0)
    {
        CurrentMoveSP.s = IPS_OK;
        CurrentMoveS[CURRENT_50].s = ISS_ON;
    }
    else if (strcmp(res, "(75%)") == 0)
    {
        CurrentMoveSP.s = IPS_OK;
        CurrentMoveS[CURRENT_75].s = ISS_ON;
    }
    else if (strcmp(res, "(100%)") == 0)
    {
        CurrentMoveSP.s = IPS_OK;
        CurrentMoveS[CURRENT_100].s = ISS_ON;
    }

    else
    {
        LOGF_ERROR("Unknown error: currentMove value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readCurrentHold()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GCHD%]", res) == false)
        return false;

    if (strcmp(res, "(25%)") == 0)
    {
        CurrentHoldSP.s = IPS_OK;
        CurrentHoldS[CURRENT_25].s = ISS_ON;
    }
    else if (strcmp(res, "(50%)") == 0)
    {
        CurrentHoldSP.s = IPS_OK;
        CurrentHoldS[CURRENT_50].s = ISS_ON;
    }
    else if (strcmp(res, "(75%)") == 0)
    {
        CurrentHoldSP.s = IPS_OK;
        CurrentHoldS[CURRENT_75].s = ISS_ON;
    }
    else if (strcmp(res, "(100%)") == 0)
    {
        CurrentHoldSP.s = IPS_OK;
        CurrentHoldS[CURRENT_100].s = ISS_ON;
    }

    else
    {
        LOGF_ERROR("Unknown error: currentMove value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::isMoving()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GMOV]", res) == false)
        return false;

    if (strcmp(res, "(1)") == 0)
        return true;
    else if (strcmp(res, "(0)") == 0)
        return false;

    LOGF_ERROR("Unknown error: isMoving value (%s)", res);
    return false;
}

bool DeepSkyDadAF1::SyncFocuser(uint32_t ticks)
{
    char cmd[DSD_RES] = {0};
    snprintf(cmd, DSD_RES, "[SPOS%06d]", ticks);
    return sendCommand(cmd);
}

bool DeepSkyDadAF1::ReverseFocuser(bool enabled)
{
    char cmd[DSD_RES] = {0};
    snprintf(cmd, DSD_RES, "[SREV%01d]", enabled ? 1 : 0);
    return sendCommand(cmd);
}

bool DeepSkyDadAF1::MoveFocuser(uint32_t position)
{
    char cmd[DSD_RES] = {0};
    char res[DSD_RES] = {0};
    snprintf(cmd, DSD_RES, "[STRG%06d]", position);
    // Set Position First
    if (sendCommand(cmd, res) == false)
        return false;

    if(strcmp(res, "!101)") == 0)
    {
        LOG_ERROR("MoveFocuserFailed - requested movement too big. You can increase the limit by changing the value of Max. movement.");
        return false;
    }

    // Now start motion toward position
    if (sendCommand("[SMOV]") == false)
        return false;

    return true;
}

bool DeepSkyDadAF1::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Focus Step Mode
        if (strcmp(StepModeSP.name, name) == 0)
        {
            int current_mode = IUFindOnSwitchIndex(&StepModeSP);

            IUUpdateSwitch(&StepModeSP, states, names, n);

            int target_mode = IUFindOnSwitchIndex(&StepModeSP);

            if (current_mode == target_mode)
            {
                StepModeSP.s = IPS_OK;
                IDSetSwitch(&StepModeSP, nullptr);
                return true;
            }

            char cmd[DSD_RES] = {0};

            if(target_mode == 0)
                target_mode = 1;
            else if(target_mode == 1)
                target_mode = 2;
            else if(target_mode == 2)
                target_mode = 4;
            else if(target_mode == 3)
                target_mode = 8;

            snprintf(cmd, DSD_RES, "[SSTP%d]", target_mode);
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                IUResetSwitch(&StepModeSP);
                StepModeS[current_mode].s = ISS_ON;
                StepModeSP.s              = IPS_ALERT;
                IDSetSwitch(&StepModeSP, nullptr);
                return false;
            }

            StepModeSP.s = IPS_OK;
            IDSetSwitch(&StepModeSP, nullptr);
            return true;
        }

        // Coils mode
        if (strcmp(CoilsModeSP.name, name) == 0)
        {
            int coilsModeCurrent = IUFindOnSwitchIndex(&CoilsModeSP);

            IUUpdateSwitch(&CoilsModeSP, states, names, n);

            int coilsModeTarget = IUFindOnSwitchIndex(&CoilsModeSP);

            if (coilsModeCurrent == coilsModeTarget)
            {
                IDSetSwitch(&CoilsModeSP, nullptr);
                return true;
            }

            if(coilsModeTarget == 0)
                coilsModeTarget = 1;
            else if(coilsModeTarget == 1)
                coilsModeTarget = 0;
            else if(coilsModeTarget == 2)
                coilsModeTarget = 2;

            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SCLM%d]", coilsModeTarget);

            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                IUResetSwitch(&CoilsModeSP);
                CoilsModeS[coilsModeCurrent].s = ISS_ON;
                CoilsModeSP.s              = IPS_ALERT;
                IDSetSwitch(&CoilsModeSP, nullptr);
                return false;
            }

            CoilsModeSP.s = coilsModeTarget == 1 ? IPS_OK : IPS_IDLE;
            IDSetSwitch(&CoilsModeSP, nullptr);
            return true;
        }

        // Current - move
        if (strcmp(CurrentMoveSP.name, name) == 0)
        {
            int current = IUFindOnSwitchIndex(&CurrentMoveSP);

            IUUpdateSwitch(&CurrentMoveSP, states, names, n);

            int targetCurrent = IUFindOnSwitchIndex(&CurrentMoveSP);

            if (current == targetCurrent)
            {
                IDSetSwitch(&CurrentMoveSP, nullptr);
                return true;
            }

            int targetCurrentValue = 75;
            switch(targetCurrent)
            {
                case 0:
                    targetCurrentValue = 25;
                    break;
                case 1:
                    targetCurrentValue = 50;
                    break;
                case 2:
                    targetCurrentValue = 75;
                    break;
                case 3:
                    targetCurrentValue = 100;
                    break;
            }

            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SCMV%d%%]", targetCurrentValue);

            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                IUResetSwitch(&CurrentMoveSP);
                CurrentMoveS[current].s = ISS_ON;
                CurrentMoveSP.s              = IPS_ALERT;
                IDSetSwitch(&CurrentMoveSP, nullptr);
                return false;
            }

            CurrentMoveSP.s = IPS_OK;
            IDSetSwitch(&CurrentMoveSP, nullptr);
            return true;
        }

        // Current - hold
        if (strcmp(CurrentHoldSP.name, name) == 0)
        {
            int current = IUFindOnSwitchIndex(&CurrentHoldSP);

            IUUpdateSwitch(&CurrentHoldSP, states, names, n);

            int targetCurrent = IUFindOnSwitchIndex(&CurrentHoldSP);

            if (current == targetCurrent)
            {
                IDSetSwitch(&CurrentHoldSP, nullptr);
                return true;
            }

            int targetCurrentValue = 75;
            switch(targetCurrent)
            {
                case 0:
                    targetCurrentValue = 25;
                    break;
                case 1:
                    targetCurrentValue = 50;
                    break;
                case 2:
                    targetCurrentValue = 75;
                    break;
                case 3:
                    targetCurrentValue = 100;
                    break;
            }

            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SCHD%d%%]", targetCurrentValue);

            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                IUResetSwitch(&CurrentHoldSP);
                CurrentHoldS[current].s = ISS_ON;
                CurrentHoldSP.s              = IPS_ALERT;
                IDSetSwitch(&CurrentHoldSP, nullptr);
                return false;
            }

            CurrentHoldSP.s = IPS_OK;
            IDSetSwitch(&CurrentHoldSP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool DeepSkyDadAF1::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Settle buffer Settings
        if (strcmp(name, SettleBufferNP.name) == 0)
        {
            IUUpdateNumber(&SettleBufferNP, values, names, n);
            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SBUF%06d]", static_cast<int>(SettleBufferN[0].value));
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                SettleBufferNP.s = IPS_ALERT;
                return false;
            }

            SettleBufferNP.s = IPS_OK;
            IDSetNumber(&SettleBufferNP, nullptr);
            return true;
        }

        // Idle coils timeout
        if (strcmp(name, IdleCoilsTimeoutNP.name) == 0)
        {
            IUUpdateNumber(&IdleCoilsTimeoutNP, values, names, n);
            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SIDC%06d]", static_cast<int>(IdleCoilsTimeoutN[0].value));
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                IdleCoilsTimeoutNP.s = IPS_ALERT;
                return false;
            }

            IdleCoilsTimeoutNP.s = IPS_OK;
            IDSetNumber(&IdleCoilsTimeoutNP, nullptr);
            return true;
        }

        // Max. position
        if (strcmp(name, FocusMaxPosNP.name) == 0)
        {
            IUUpdateNumber(&FocusMaxPosNP, values, names, n);
            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SMXP%d]", static_cast<int>(FocusMaxPosN[0].value));
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                FocusMaxPosNP.s = IPS_ALERT;
                return false;
            }

            FocusMaxPosNP.s = IPS_OK;
            IDSetNumber(&FocusMaxPosNP, nullptr);
            return true;
        }

        // Max. movement
        if (strcmp(name, FocusMaxMoveNP.name) == 0)
        {
            IUUpdateNumber(&FocusMaxMoveNP, values, names, n);
            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SMXM%d]", static_cast<int>(FocusMaxMoveN[0].value));
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                FocusMaxMoveNP.s = IPS_ALERT;
                return false;
            }

            FocusMaxMoveNP.s = IPS_OK;
            IDSetNumber(&FocusMaxMoveNP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void DeepSkyDadAF1::GetFocusParams()
{
    IUResetSwitch(&StepModeSP);
    IUResetSwitch(&CoilsModeSP);
    IUResetSwitch(&CurrentMoveSP);
    IUResetSwitch(&CurrentHoldSP);

    if (readPosition())
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (readStepMode())
        IDSetSwitch(&StepModeSP, nullptr);

    if (readSettleBuffer())
        IDSetNumber(&SettleBufferNP, nullptr);

    if (readMaxPosition())
        IDSetNumber(&FocusMaxPosNP, nullptr);

    if (readMaxMovement())
        IDSetNumber(&FocusMaxMoveNP, nullptr);

    if (readIdleCoilsTimeout())
        IDSetNumber(&IdleCoilsTimeoutNP, nullptr);

    if (readCoilsMode())
        IDSetSwitch(&CoilsModeSP, nullptr);

    if (readCurrentMove())
        IDSetSwitch(&CurrentMoveSP, nullptr);

    if (readCurrentHold())
        IDSetSwitch(&CurrentHoldSP, nullptr);
}

IPState DeepSkyDadAF1::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    INDI_UNUSED(speed);
    // either go all the way in or all the way out
    // then use timer to stop
    if (dir == FOCUS_INWARD)
        MoveFocuser(0);
    else
        MoveFocuser(FocusMaxPosN[0].value);

    IEAddTimer(duration, &DeepSkyDadAF1::timedMoveHelper, this);
    return IPS_BUSY;
}

void DeepSkyDadAF1::timedMoveHelper(void * context)
{
    static_cast<DeepSkyDadAF1*>(context)->timedMoveCallback();
}

void DeepSkyDadAF1::timedMoveCallback()
{
    AbortFocuser();
    FocusAbsPosNP.s = IPS_IDLE;
    FocusRelPosNP.s = IPS_IDLE;
    FocusTimerNP.s = IPS_IDLE;
    FocusTimerN[0].value = 0;
    IDSetNumber(&FocusAbsPosNP, nullptr);
    IDSetNumber(&FocusRelPosNP, nullptr);
    IDSetNumber(&FocusTimerNP, nullptr);
}


IPState DeepSkyDadAF1::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (!MoveFocuser(targetPos))
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState DeepSkyDadAF1::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosN[0].max), newPosition));
    if (!MoveFocuser(newPosition))
        return IPS_ALERT;

    // JM 2019-02-10: This is already set by the framework
    //FocusRelPosN[0].value = ticks;
    //FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void DeepSkyDadAF1::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = readPosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 5)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool DeepSkyDadAF1::AbortFocuser()
{
    return sendCommand("[STOP]");
}

bool DeepSkyDadAF1::saveConfigItems(FILE * fp)
{
    Focuser::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &StepModeSP);
    IUSaveConfigNumber(fp, &FocusMaxMoveNP);
    IUSaveConfigNumber(fp, &SettleBufferNP);
    IUSaveConfigSwitch(fp, &CoilsModeSP);
    IUSaveConfigNumber(fp, &IdleCoilsTimeoutNP);
    IUSaveConfigSwitch(fp, &CurrentMoveSP);
    IUSaveConfigSwitch(fp, &CurrentHoldSP);

    return true;
}

bool DeepSkyDadAF1::sendCommand(const char * cmd, char * res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if ((rc = tty_nread_section(PortFD, res, DSD_RES, DSD_DEL, DSD_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

bool DeepSkyDadAF1::sendCommandSet(const char * cmd)
{
    char res[DSD_RES] = {0};

    if (sendCommand(cmd, res) == false)
        return false;

    return strcmp(res, "(OK)") == 0;
}
