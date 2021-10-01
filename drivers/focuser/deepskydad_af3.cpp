/*
    Deep Sky Dad AF3 focuser

    Copyright (C) 2019 Pavle Gartner

    Based on Moonlite driver.
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

#include "deepskydad_af3.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<DeepSkyDadAF3> deepSkyDadAf3(new DeepSkyDadAF3());

DeepSkyDadAF3::DeepSkyDadAF3()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_SYNC | FOCUSER_CAN_REVERSE | FOCUSER_CAN_ABORT |
                      FOCUSER_HAS_BACKLASH);
}

bool DeepSkyDadAF3::initProperties()
{
    INDI::Focuser::initProperties();

    // Step Mode
    IUFillSwitch(&StepModeS[S256], "S256", "1/256 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[S128], "S128", "1/128 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[S64], "S64", "1/64 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[S32], "S32", "1/32 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[S16], "S16", "1/16 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[S8], "S8", "1/8 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[S4], "S4", "1/4 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[S2], "S2", "1/2 Step", ISS_OFF);
    IUFillSwitch(&StepModeS[S1], "S1", "Full Step", ISS_OFF);
    IUFillSwitchVector(&StepModeSP, StepModeS, 9, getDeviceName(), "Step Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Speed Mode
    IUFillSwitch(&SpeedModeS[VERY_SLOW], "VERY_SLOW", "Very slow", ISS_OFF);
    IUFillSwitch(&SpeedModeS[SLOW], "SLOW", "Slow", ISS_OFF);
    IUFillSwitch(&SpeedModeS[MEDIUM], "MEDIUM", "Medium", ISS_OFF);
    IUFillSwitch(&SpeedModeS[FAST], "FAST", "Fast", ISS_OFF);
    IUFillSwitch(&SpeedModeS[VERY_FAST], "VERY_FAST", "Very fast", ISS_OFF);
    IUFillSwitchVector(&SpeedModeSP, SpeedModeS, 5, getDeviceName(), "Speed Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 50000.;
    FocusRelPosN[0].value = 0.;
    FocusRelPosN[0].step = 10.;

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = 1000000.;
    FocusAbsPosN[0].value = 50000.;
    FocusAbsPosN[0].step = 5000.;

    FocusMaxPosN[0].min = 0.;
    FocusMaxPosN[0].max = 1000000.;
    FocusMaxPosN[0].value = 1000000.;
    FocusMaxPosN[0].step = 5000.;

    FocusSyncN[0].min = 0.;
    FocusSyncN[0].max = 1000000.;
    FocusSyncN[0].value = 50000.;
    FocusSyncN[0].step = 5000.;

    FocusBacklashN[0].min = -1000;
    FocusBacklashN[0].max = 1000;
    FocusBacklashN[0].step = 1;
    FocusBacklashN[0].value = 0;

    // Max. movement
    IUFillNumber(&FocusMaxMoveN[0], "MAX_MOVE", "Steps", "%7.0f", 0, 9999999, 100, 0);
    IUFillNumberVector(&FocusMaxMoveNP, FocusMaxMoveN, 1, getDeviceName(), "FOCUS_MAX_MOVE", "Max. movement",
                       MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Settle buffer
    IUFillNumber(&SettleBufferN[0], "SETTLE_BUFFER", "Period (ms)", "%5.0f", 0, 99999, 100, 0);
    IUFillNumberVector(&SettleBufferNP, SettleBufferN, 1, getDeviceName(), "FOCUS_SETTLE_BUFFER", "Settle buffer",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Motor move multiplier
    IUFillNumber(&MoveCurrentMultiplierN[0], "MOTOR_MOVE_MULTIPLIER", "%", "%3.0f", 1, 100, 1, 90);
    IUFillNumberVector(&MoveCurrentMultiplierNP, MoveCurrentMultiplierN, 1, getDeviceName(), "FOCUS_MMM",
                       "Move current multiplier",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Motor hold multiplier
    IUFillNumber(&HoldCurrentMultiplierN[0], "MOTOR_HOLD_MULTIPLIER", "%", "%3.0f", 1, 100, 1, 40);
    IUFillNumberVector(&HoldCurrentMultiplierNP, HoldCurrentMultiplierN, 1, getDeviceName(), "FOCUS_MHM",
                       "Hold current multiplier",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    setDefaultPollingPeriod(500);
    addDebugControl();

    return true;
}

bool DeepSkyDadAF3::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&FocusMaxMoveNP);
        defineProperty(&StepModeSP);
        defineProperty(&SpeedModeSP);
        defineProperty(&SettleBufferNP);
        defineProperty(&MoveCurrentMultiplierNP);
        defineProperty(&HoldCurrentMultiplierNP);
        defineProperty(&TemperatureNP);

        GetFocusParams();

        LOG_INFO("deepSkyDadAf3 parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(FocusMaxMoveNP.name);
        deleteProperty(StepModeSP.name);
        deleteProperty(SpeedModeSP.name);
        deleteProperty(SettleBufferNP.name);
        deleteProperty(MoveCurrentMultiplierNP.name);
        deleteProperty(HoldCurrentMultiplierNP.name);
        deleteProperty(TemperatureNP.name);
    }

    return true;
}

bool DeepSkyDadAF3::Handshake()
{
    if (Ack())
    {
        LOG_INFO("deepSkyDadAf3 is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO(
        "Error retrieving data from deepSkyDadAf3, please ensure deepSkyDadAf3 controller is powered and the port is correct.");
    return false;
}

const char * DeepSkyDadAF3::getDefaultName()
{
    return "Deep Sky Dad AF3";
}

bool DeepSkyDadAF3::Ack()
{
    sleep(2);

    char res[DSD_RES] = {0};
    if (!sendCommand("[GPOS]", res) && !sendCommand("[GPOS]", res)) //try twice
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

bool DeepSkyDadAF3::readStepMode()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GSTP]", res) == false)
        return false;

    if (strcmp(res, "(1)") == 0)
        StepModeS[S1].s = ISS_ON;
    else if (strcmp(res, "(2)") == 0)
        StepModeS[S2].s = ISS_ON;
    else if (strcmp(res, "(4)") == 0)
        StepModeS[S4].s = ISS_ON;
    else if (strcmp(res, "(8)") == 0)
        StepModeS[S8].s = ISS_ON;
    else if (strcmp(res, "(16)") == 0)
        StepModeS[S16].s = ISS_ON;
    else if (strcmp(res, "(32)") == 0)
        StepModeS[S32].s = ISS_ON;
    else if (strcmp(res, "(64)") == 0)
        StepModeS[S64].s = ISS_ON;
    else if (strcmp(res, "(128)") == 0)
        StepModeS[S128].s = ISS_ON;
    else if (strcmp(res, "(256)") == 0)
        StepModeS[S256].s = ISS_ON;
    else
    {
        LOGF_ERROR("Unknown error: focuser step value (%s)", res);
        return false;
    }

    StepModeSP.s = IPS_OK;
    return true;
}

bool DeepSkyDadAF3::readSpeedMode()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GSPD]", res) == false)
        return false;

    if (strcmp(res, "(1)") == 0)
        SpeedModeS[VERY_SLOW].s = ISS_ON;
    else if (strcmp(res, "(2)") == 0)
        SpeedModeS[SLOW].s = ISS_ON;
    else if (strcmp(res, "(3)") == 0)
        SpeedModeS[MEDIUM].s = ISS_ON;
    else if (strcmp(res, "(4)") == 0)
        SpeedModeS[FAST].s = ISS_ON;
    else if (strcmp(res, "(5)") == 0)
        SpeedModeS[VERY_FAST].s = ISS_ON;
    else
    {
        LOGF_ERROR("Unknown error: focuser speed value (%s)", res);
        return false;
    }

    SpeedModeSP.s = IPS_OK;
    return true;
}

bool DeepSkyDadAF3::readPosition()
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

bool DeepSkyDadAF3::readMaxMovement()
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

bool DeepSkyDadAF3::readMaxPosition()
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

bool DeepSkyDadAF3::readSettleBuffer()
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

bool DeepSkyDadAF3::readMoveCurrentMultiplier()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GMMM]", res) == false)
        return false;

    uint32_t mcm = 0;
    int rc = sscanf(res, "(%d)", &mcm);
    if (rc > 0)
    {
        MoveCurrentMultiplierN[0].value = mcm;
        MoveCurrentMultiplierNP.s = IPS_OK;
    }
    else
    {
        LOGF_ERROR("Unknown error: move current multiplier value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF3::readHoldCurrentMultiplier()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GMHM]", res) == false)
        return false;

    uint32_t hcm = 0;
    int rc = sscanf(res, "(%d)", &hcm);
    if (rc > 0)
    {
        HoldCurrentMultiplierN[0].value = hcm;
        HoldCurrentMultiplierNP.s = IPS_OK;
    }
    else
    {
        LOGF_ERROR("Unknown error: hold current multiplier value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF3::readTemperature()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GTMC]", res) == false)
        return false;

    double temp = 0;
    int rc = sscanf(res, "(%lf)", &temp);
    if (rc > 0)
    {
        TemperatureN[0].value = temp;
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser temperature value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF3::isMoving()
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

bool DeepSkyDadAF3::SyncFocuser(uint32_t ticks)
{
    char cmd[DSD_RES] = {0};
    snprintf(cmd, DSD_RES, "[SPOS%07d]", ticks);
    char res[DSD_RES] = {0};
    return sendCommand(cmd, res);
}

bool DeepSkyDadAF3::ReverseFocuser(bool enabled)
{
    char cmd[DSD_RES] = {0};
    snprintf(cmd, DSD_RES, "[SREV%01d]", enabled ? 1 : 0);
    char res[DSD_RES] = {0};
    return sendCommand(cmd, res);
}

bool DeepSkyDadAF3::MoveFocuser(uint32_t position)
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
    if (sendCommand("[SMOV]", res) == false)
        return false;

    return true;
}

bool DeepSkyDadAF3::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
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
            else if(target_mode == 4)
                target_mode = 16;
            else if(target_mode == 5)
                target_mode = 32;
            else if(target_mode == 6)
                target_mode = 64;
            else if(target_mode == 7)
                target_mode = 128;
            else if(target_mode == 8)
                target_mode = 256;

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

        // Focus Speed Mode
        if (strcmp(SpeedModeSP.name, name) == 0)
        {
            int current_mode = IUFindOnSwitchIndex(&SpeedModeSP);

            IUUpdateSwitch(&SpeedModeSP, states, names, n);

            int target_mode = IUFindOnSwitchIndex(&SpeedModeSP);

            if (current_mode == target_mode)
            {
                SpeedModeSP.s = IPS_OK;
                IDSetSwitch(&SpeedModeSP, nullptr);
                return true;
            }

            char cmd[DSD_RES] = {0};

            if(target_mode == 0)
                target_mode = 1;
            else if(target_mode == 1)
                target_mode = 2;
            else if(target_mode == 2)
                target_mode = 3;
            else if(target_mode == 3)
                target_mode = 4;
            else if(target_mode == 4)
                target_mode = 5;

            snprintf(cmd, DSD_RES, "[SSPD%d]", target_mode);
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                IUResetSwitch(&SpeedModeSP);
                SpeedModeS[current_mode].s = ISS_ON;
                SpeedModeSP.s              = IPS_ALERT;
                IDSetSwitch(&SpeedModeSP, nullptr);
                return false;
            }

            SpeedModeSP.s = IPS_OK;
            IDSetSwitch(&SpeedModeSP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool DeepSkyDadAF3::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
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

        // Move current multiplier
        if (strcmp(name, MoveCurrentMultiplierNP.name) == 0)
        {
            IUUpdateNumber(&MoveCurrentMultiplierNP, values, names, n);
            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SMMM%03d]", static_cast<int>(MoveCurrentMultiplierN[0].value));
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                MoveCurrentMultiplierNP.s = IPS_ALERT;
                return false;
            }

            MoveCurrentMultiplierNP.s = IPS_OK;
            IDSetNumber(&MoveCurrentMultiplierNP, nullptr);
            return true;
        }

        // Hold current multiplier
        if (strcmp(name, HoldCurrentMultiplierNP.name) == 0)
        {
            IUUpdateNumber(&HoldCurrentMultiplierNP, values, names, n);
            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SMHM%03d]", static_cast<int>(HoldCurrentMultiplierN[0].value));
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                HoldCurrentMultiplierNP.s = IPS_ALERT;
                return false;
            }

            HoldCurrentMultiplierNP.s = IPS_OK;
            IDSetNumber(&HoldCurrentMultiplierNP, nullptr);
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
        //        if (strcmp(name, FocusMaxMoveNP.name) == 0)
        //        {
        //            IUUpdateNumber(&FocusMaxMoveNP, values, names, n);
        //            char cmd[DSD_RES] = {0};
        //            snprintf(cmd, DSD_RES, "[SMXM%d]", static_cast<int>(FocusMaxMoveN[0].value));
        //            bool rc = sendCommandSet(cmd);
        //            if (!rc)
        //            {
        //                FocusMaxMoveNP.s = IPS_ALERT;
        //                return false;
        //            }

        //            FocusMaxMoveNP.s = IPS_OK;
        //            IDSetNumber(&FocusMaxMoveNP, nullptr);
        //            return true;
        //        }

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void DeepSkyDadAF3::GetFocusParams()
{
    IUResetSwitch(&StepModeSP);
    IUResetSwitch(&SpeedModeSP);

    if (readPosition())
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (readStepMode())
        IDSetSwitch(&StepModeSP, nullptr);

    if (readSpeedMode())
        IDSetSwitch(&SpeedModeSP, nullptr);

    if (readSettleBuffer())
        IDSetNumber(&SettleBufferNP, nullptr);

    if (readMoveCurrentMultiplier())
        IDSetNumber(&MoveCurrentMultiplierNP, nullptr);

    if (readHoldCurrentMultiplier())
        IDSetNumber(&HoldCurrentMultiplierNP, nullptr);

    if (readMaxPosition())
        IDSetNumber(&FocusMaxPosNP, nullptr);

    if (readMaxMovement())
        IDSetNumber(&FocusMaxMoveNP, nullptr);

    if (readTemperature())
        IDSetNumber(&TemperatureNP, nullptr);
}

IPState DeepSkyDadAF3::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    INDI_UNUSED(speed);
    // either go all the way in or all the way out
    // then use timer to stop
    if (dir == FOCUS_INWARD)
        MoveFocuser(0);
    else
        MoveFocuser(FocusMaxPosN[0].value);

    IEAddTimer(duration, &DeepSkyDadAF3::timedMoveHelper, this);
    return IPS_BUSY;
}

void DeepSkyDadAF3::timedMoveHelper(void * context)
{
    static_cast<DeepSkyDadAF3*>(context)->timedMoveCallback();
}

void DeepSkyDadAF3::timedMoveCallback()
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


IPState DeepSkyDadAF3::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    double bcValue = FocusBacklashN[0].value;
    int diff = targetTicks - FocusAbsPosN[0].value;
    if ((diff > 0 && bcValue < 0) || (diff < 0 && bcValue > 0))
    {
        backlashComp = bcValue;
        targetPos -= bcValue;
    }

    if (!MoveFocuser(targetPos))
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState DeepSkyDadAF3::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosN[0].max), newPosition));
    if (!MoveAbsFocuser(newPosition))
        return IPS_ALERT;

    // JM 2019-02-10: This is already set by the framework
    //FocusRelPosN[0].value = ticks;
    //FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void DeepSkyDadAF3::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = readPosition();
    if (rc)
    {
        if (std::abs(lastPos - FocusAbsPosN[0].value) > 5)
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

            if(moveAborted)
            {
                LOG_INFO("Move aborted.");
            }
            else if(backlashComp != 0)
            {
                LOGF_INFO("Performing backlash compensation of %i.", (int)backlashComp);
                targetPos += backlashComp;
                MoveFocuser(targetPos);
            }
            else
            {
                LOG_INFO("Focuser reached requested position.");
            }

            moveAborted = false;
            backlashComp = 0;
        }
    }

    rc = readTemperature();
    if (rc)
    {
        //more accurate update
        if (std::abs(lastTemperature - TemperatureN[0].value) >= 0.1)
        {
            IDSetNumber(&TemperatureNP, nullptr);
            lastTemperature = TemperatureN[0].value;
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool DeepSkyDadAF3::AbortFocuser()
{
    moveAborted = true;
    return sendCommand("[STOP]");
}

bool DeepSkyDadAF3::saveConfigItems(FILE * fp)
{
    Focuser::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &StepModeSP);
    IUSaveConfigSwitch(fp, &SpeedModeSP);
    IUSaveConfigNumber(fp, &FocusMaxMoveNP);
    IUSaveConfigNumber(fp, &SettleBufferNP);
    IUSaveConfigNumber(fp, &MoveCurrentMultiplierNP);
    IUSaveConfigNumber(fp, &HoldCurrentMultiplierNP);

    return true;
}

bool DeepSkyDadAF3::sendCommand(const char * cmd, char * res)
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

bool DeepSkyDadAF3::sendCommandSet(const char * cmd)
{
    char res[DSD_RES] = {0};

    if (sendCommand(cmd, res) == false)
        return false;

    return strcmp(res, "(OK)") == 0;
}

bool DeepSkyDadAF3::SetFocuserBacklash(int32_t steps)
{
    INDI_UNUSED(steps);
    return true;
}

bool DeepSkyDadAF3::SetFocuserMaxPosition(uint32_t ticks)
{
    char cmd[DSD_RES] = {0};

    snprintf(cmd, DSD_RES, "[SMXP%d]", ticks);

    if (sendCommandSet(cmd))
    {
        SyncPresets(ticks);
        return true;
    }

    return false;
}
