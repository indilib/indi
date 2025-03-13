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
    StepModeSP[EIGHT].fill("EIGHT", "Eight Step", ISS_OFF);
    StepModeSP[QUARTER].fill("QUARTER", "Quarter Step", ISS_OFF);
    StepModeSP[HALF].fill("HALF", "Half Step", ISS_OFF);
    StepModeSP[FULL].fill("FULL", "Full Step", ISS_OFF);
    StepModeSP.fill(getDeviceName(), "Step Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(5000.);
    FocusRelPosNP[0].setValue(0.);
    FocusRelPosNP[0].setStep(10.);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(100000.);
    FocusAbsPosNP[0].setValue(50000.);
    FocusAbsPosNP[0].setStep(500.);

    FocusMaxPosNP[0].setMin(0.);
    FocusMaxPosNP[0].setMax(9999999);
    FocusMaxPosNP[0].setValue(9999999);
    FocusMaxPosNP[0].setStep(500);

    // Max. movement
    FocusMaxMoveNP[0].fill("MAX_MOVE", "Steps", "%7.0f", 0, 9999999, 100, 0);
    FocusMaxMoveNP.fill(getDeviceName(), "FOCUS_MAX_MOVE", "Max. movement",
                       MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Settle buffer
    SettleBufferNP[0].fill("SETTLE_BUFFER", "Period (ms)", "%5.0f", 0, 99999, 100, 0);
    SettleBufferNP.fill(getDeviceName(), "FOCUS_SETTLE_BUFFER", "Settle buffer",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Idle coils timeout (ms)
    IUFillNumber(&IdleCoilsTimeoutN[0], "IDLE_COILS_TIMEOUT", "Period (ms)", "%6.0f", 0, 999999, 1000, 60000);
    IUFillNumberVector(&IdleCoilsTimeoutNP, IdleCoilsTimeoutN, 1, getDeviceName(), "FOCUS_IDLE_COILS_TIMEOUT",
                       "Idle - coils timeout",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Coils mode
    CoilsModeSP[ALWAYS_ON].fill("ALWAYS_ON", "Always on", ISS_OFF);
    CoilsModeSP[IDLE_OFF].fill("IDLE_OFF", "Idle - off", ISS_OFF);
    CoilsModeSP[IDLE_COILS_TIMEOUT].fill("IDLE_COILS_TIMEOUT", "Idle - coils timeout (ms)", ISS_OFF);
    CoilsModeSP.fill(getDeviceName(), "Coils mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Current move
    CurrentMoveSP[CURRENT_25].fill("CMV_25", "25%", ISS_OFF);
    CurrentMoveSP[CURRENT_50].fill("CMV_50", "50%", ISS_OFF);
    CurrentMoveSP[CURRENT_75].fill("CMV_75", "75%", ISS_OFF);
    CurrentMoveSP[CURRENT_100].fill("CMV_100", "100%", ISS_OFF);
    CurrentMoveSP.fill(getDeviceName(), "Current - move", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // Current hold
    CurrentHoldSP[CURRENT_25].fill("CHD_25", "25%", ISS_OFF);
    CurrentHoldSP[CURRENT_50].fill("CHD_50", "50%", ISS_OFF);
    CurrentHoldSP[CURRENT_75].fill("CHD_75", "75%", ISS_OFF);
    CurrentHoldSP[CURRENT_100].fill("CHD_100", "100%", ISS_OFF);
    CurrentHoldSP.fill(getDeviceName(), "Current - hold", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    setDefaultPollingPeriod(500);
    addDebugControl();

    return true;
}

bool DeepSkyDadAF1::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(FocusMaxMoveNP);
        defineProperty(StepModeSP);
        defineProperty(SettleBufferNP);
        defineProperty(CoilsModeSP);
        defineProperty(&IdleCoilsTimeoutNP);
        defineProperty(CurrentMoveSP);
        defineProperty(CurrentHoldSP);

        GetFocusParams();

        LOG_INFO("deepSkyDadAf1 parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(FocusMaxMoveNP);
        deleteProperty(StepModeSP);
        deleteProperty(SettleBufferNP);
        deleteProperty(CoilsModeSP);
        deleteProperty(IdleCoilsTimeoutNP.name);
        deleteProperty(CurrentMoveSP);
        deleteProperty(CurrentHoldSP);
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
        StepModeSP[FULL].setState(ISS_ON);
    else if (strcmp(res, "(2)") == 0)
        StepModeSP[HALF].setState(ISS_ON);
    else if (strcmp(res, "(4)") == 0)
        StepModeSP[QUARTER].setState(ISS_ON);
    else if (strcmp(res, "(8)") == 0)
        StepModeSP[EIGHT].setState(ISS_ON);
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
        FocusAbsPosNP[0].setValue(pos);
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
        FocusMaxMoveNP[0].setValue(steps);
        FocusMaxMoveNP.setState(IPS_OK);
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
        FocusMaxPosNP[0].setValue(steps);
        FocusMaxPosNP.setState(IPS_OK);
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
        SettleBufferNP[0].setValue(settleBuffer);
        SettleBufferNP.setState(settleBuffer > 0 ? IPS_OK : IPS_IDLE);
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
        CoilsModeSP.setState(IPS_IDLE);
        CoilsModeSP[IDLE_OFF].setState(ISS_ON);
    }
    else if (strcmp(res, "(1)") == 0)
    {
        CoilsModeSP.setState(IPS_OK);
        CoilsModeSP[ALWAYS_ON].setState(ISS_ON);
    }
    else if (strcmp(res, "(2)") == 0)
    {
        CoilsModeSP.setState(IPS_IDLE);
        CoilsModeSP[IDLE_COILS_TIMEOUT].setState(ISS_ON);
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
        CurrentMoveSP.setState(IPS_OK);
        CurrentMoveSP[CURRENT_25].setState(ISS_ON);
    }
    else if (strcmp(res, "(50%)") == 0)
    {
        CurrentMoveSP.setState(IPS_OK);
        CurrentMoveSP[CURRENT_50].setState(ISS_ON);
    }
    else if (strcmp(res, "(75%)") == 0)
    {
        CurrentMoveSP.setState(IPS_OK);
        CurrentMoveSP[CURRENT_75].setState(ISS_ON);
    }
    else if (strcmp(res, "(100%)") == 0)
    {
        CurrentMoveSP.setState(IPS_OK);
        CurrentMoveSP[CURRENT_100].setState(ISS_ON);
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
        CurrentHoldSP.setState(IPS_OK);
        CurrentHoldSP[CURRENT_25].setState(ISS_ON);
    }
    else if (strcmp(res, "(50%)") == 0)
    {
        CurrentHoldSP.setState(IPS_OK);
        CurrentHoldSP[CURRENT_50].setState(ISS_ON);
    }
    else if (strcmp(res, "(75%)") == 0)
    {
        CurrentHoldSP.setState(IPS_OK);
        CurrentHoldSP[CURRENT_75].setState(ISS_ON);
    }
    else if (strcmp(res, "(100%)") == 0)
    {
        CurrentHoldSP.setState(IPS_OK);
        CurrentHoldSP[CURRENT_100].setState(ISS_ON);
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
        if (StepModeSP.isNameMatch(name))
        {
            int current_mode = StepModeSP.findOnSwitchIndex();

            StepModeSP.update(states, names, n);

            int target_mode =StepModeSP.findOnSwitchIndex();

            if (current_mode == target_mode)
            {
                StepModeSP.setState(IPS_OK);
                StepModeSP.apply();
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
                StepModeSP.reset();
                StepModeSP[current_mode].setState(ISS_ON);
                StepModeSP.setState(IPS_ALERT);
                StepModeSP.apply();
                return false;
            }

            StepModeSP.setState(IPS_OK);
            StepModeSP.apply();
            return true;
        }

        // Coils mode
        if (CoilsModeSP.isNameMatch(name))
        {
            int coilsModeCurrent = CoilsModeSP.findOnSwitchIndex();

            CoilsModeSP.update(states, names, n);

            int coilsModeTarget = CoilsModeSP.findOnSwitchIndex();

            if (coilsModeCurrent == coilsModeTarget)
            {
                CoilsModeSP.apply();
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
                CoilsModeSP.reset();
                CoilsModeSP[coilsModeCurrent].setState(ISS_ON);
                CoilsModeSP.setState(IPS_ALERT);
                CoilsModeSP.apply();
                return false;
            }

            CoilsModeSP.setState(coilsModeTarget == 1 ? IPS_OK : IPS_IDLE);
            CoilsModeSP.apply();
            return true;
        }

        // Current - move
        if (CurrentMoveSP.isNameMatch(name))
        {
            int current = CurrentMoveSP.findOnSwitchIndex();

            CurrentMoveSP.update(states, names, n);

            int targetCurrent = CurrentMoveSP.findOnSwitchIndex();

            if (current == targetCurrent)
            {
                CurrentMoveSP.apply();
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
                CurrentMoveSP.reset();
                CurrentMoveSP[current].setState(ISS_ON);
                CurrentMoveSP.setState(IPS_ALERT);
                CurrentMoveSP.apply();
                return false;
            }

            CurrentMoveSP.setState(IPS_OK);
            CurrentMoveSP.apply();
            return true;
        }

        // Current - hold
        if (CurrentHoldSP.isNameMatch(name))
        {
            int current = CurrentHoldSP.findOnSwitchIndex();

            CurrentHoldSP.update(states, names, n);

            int targetCurrent = CurrentHoldSP.findOnSwitchIndex();

            if (current == targetCurrent)
            {
                CurrentHoldSP.apply();
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
                CurrentHoldSP.reset();
                CurrentHoldSP[current].setState(ISS_ON);
                CurrentHoldSP.setState(IPS_ALERT);
                CurrentHoldSP.apply();
                return false;
            }

            CurrentHoldSP.setState(IPS_OK);
            CurrentHoldSP.apply();
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
        if (SettleBufferNP.isNameMatch(name))
        {
            SettleBufferNP.update(values, names, n);
            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SBUF%06d]", static_cast<int>(SettleBufferNP[0].getValue()));
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                SettleBufferNP.setState(IPS_ALERT);
                return false;
            }

            SettleBufferNP.setState(IPS_OK);
            SettleBufferNP.apply();
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


        // Max. movement
        if (FocusMaxMoveNP.isNameMatch(name))
        {
            FocusMaxMoveNP.update(values, names, n);
            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SMXM%d]", static_cast<int>(FocusMaxMoveNP[0].getValue()));
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                FocusMaxMoveNP.setState(IPS_ALERT);
                return false;
            }

            FocusMaxMoveNP.setState(IPS_OK);
            FocusMaxMoveNP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void DeepSkyDadAF1::GetFocusParams()
{
    StepModeSP.reset();
    CoilsModeSP.reset();
    CurrentMoveSP.reset();
    CurrentHoldSP.reset();

    if (readPosition())
        FocusAbsPosNP.apply();

    if (readStepMode())
        StepModeSP.apply();

    if (readSettleBuffer())
        SettleBufferNP.apply();

    if (readMaxPosition())
        FocusMaxPosNP.apply();

    if (readMaxMovement())
        FocusMaxMoveNP.apply();

    if (readIdleCoilsTimeout())
        IDSetNumber(&IdleCoilsTimeoutNP, nullptr);

    if (readCoilsMode())
        CoilsModeSP.apply();

    if (readCurrentMove())
        CurrentMoveSP.apply();

    if (readCurrentHold())
        CurrentHoldSP.apply();
}

IPState DeepSkyDadAF1::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    INDI_UNUSED(speed);
    // either go all the way in or all the way out
    // then use timer to stop
    if (dir == FOCUS_INWARD)
        MoveFocuser(0);
    else
        MoveFocuser(FocusMaxPosNP[0].getValue());

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
    FocusAbsPosNP.setState(IPS_IDLE);
    FocusRelPosNP.setState(IPS_IDLE);
    FocusTimerNP.setState(IPS_IDLE);
    FocusTimerNP[0].setValue(0);
    FocusAbsPosNP.apply();
    FocusRelPosNP.apply();
    FocusTimerNP.apply();
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
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosNP[0].getMax()), newPosition));
    if (!MoveFocuser(newPosition))
        return IPS_ALERT;

    // JM 2019-02-10: This is already set by the framework
    //FocusRelPosNP[0].setValue(ticks);
    //FocusRelPosNP.setState(IPS_BUSY);

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
        if (std::abs(lastPos - FocusAbsPosNP[0].getValue()) > 5)
        {
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
        }
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
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

    StepModeSP.save(fp);
    FocusMaxMoveNP.save(fp);
    SettleBufferNP.save(fp);
    CoilsModeSP.save(fp);
    IUSaveConfigNumber(fp, &IdleCoilsTimeoutNP);
    CurrentMoveSP.save(fp);
    CurrentHoldSP.save(fp);

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

 bool DeepSkyDadAF1::SetFocuserMaxPosition(uint32_t ticks)
 {   
    char cmd[DSD_RES] = {0};
    snprintf(cmd, DSD_RES, "[SMXP%d]", static_cast<int>(ticks));
    return sendCommandSet(cmd);
 }