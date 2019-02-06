/*
    Deep Sky Dad AF1 focuser
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

void ISGetProperties(const char * dev)
{
    deepSkyDadAf1->ISGetProperties(dev);
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    deepSkyDadAf1->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    deepSkyDadAf1->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    deepSkyDadAf1->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[],
               char * names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle * root)
{
    deepSkyDadAf1->ISSnoopDevice(root);
}

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
    IUFillSwitch(&StepModeS[FULL], "FULL", "Full Step", ISS_ON);
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

    // Settle buffer
    IUFillNumber(&SettleBufferN[0], "SETTLE_BUFFER", "Settle buffer", "%5.0f", 0, 99999, 100, 0);
    IUFillNumberVector(&SettleBufferNP, SettleBufferN, 1, getDeviceName(), "FOCUS_SETTLE_BUFFER", "Settle buffer",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Always on
    IUFillSwitch(&AlwaysOnS[ALWAYS_ON_NO], "NO", "No", ISS_OFF);
    IUFillSwitch(&AlwaysOnS[ALWAYS_ON_YES], "YES", "Yes", ISS_ON);
    IUFillSwitchVector(&AlwaysOnSP, AlwaysOnS, 2, getDeviceName(), "Always on", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Current move
    IUFillSwitch(&CurrentMoveS[CURRENT_25], "CMV_25", "25%", ISS_OFF);
    IUFillSwitch(&CurrentMoveS[CURRENT_50], "CMV_50", "50%", ISS_OFF);
    IUFillSwitch(&CurrentMoveS[CURRENT_75], "CMV_75", "75%", ISS_ON);
    IUFillSwitch(&CurrentMoveS[CURRENT_100], "CMV_100", "100%", ISS_OFF);
    IUFillSwitchVector(&CurrentMoveSP, CurrentMoveS, 4, getDeviceName(), "Current - move", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Current move
    IUFillSwitch(&CurrentAoS[CURRENT_25], "CAO_25", "25%", ISS_OFF);
    IUFillSwitch(&CurrentAoS[CURRENT_50], "CAO_50", "50%", ISS_OFF);
    IUFillSwitch(&CurrentAoS[CURRENT_75], "CAO_75", "75%", ISS_ON);
    IUFillSwitch(&CurrentAoS[CURRENT_100], "CAO_100", "100%", ISS_OFF);
    IUFillSwitchVector(&CurrentAoSP, CurrentAoS, 4, getDeviceName(), "Current - always on", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    setDefaultPollingPeriod(500);
    addDebugControl();

    return true;
}

bool DeepSkyDadAF1::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineSwitch(&StepModeSP);
        defineNumber(&SettleBufferNP);
        defineSwitch(&AlwaysOnSP);
        defineSwitch(&CurrentMoveSP);
        defineSwitch(&CurrentAoSP);

        GetFocusParams();

        LOG_INFO("deepSkyDadAf1 paramaters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(StepModeSP.name);
        deleteProperty(SettleBufferNP.name);
        deleteProperty(AlwaysOnSP.name);
        deleteProperty(CurrentMoveSP.name);
        deleteProperty(CurrentAoSP.name);
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
        "Error retreiving data from deepSkyDadAf1, please ensure deepSkyDadAf1 controller is powered and the port is correct.");
    return false;
}

const char * DeepSkyDadAF1::getDefaultName()
{
    return "Deep Sky Dad AF1";
}

bool DeepSkyDadAF1::Ack()
{
    sleep(2);

    if (!sendCommandSet("[SMXP100000]")) {
        LOG_ERROR("ACK - write setMaxPosition failed");
        return false;
    }

    if (!sendCommandSet("[SMXM5000]")) {
        LOG_ERROR("ACK - write setMaxMovement failed");
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readStepMode()
{
    char res[DSD_RES]= {0};

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
    char res[DSD_RES]= {0};

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


bool DeepSkyDadAF1::readSettleBuffer()
{
    char res[DSD_RES]= {0};

    if (sendCommand("[GBUF]", res) == false)
        return false;

    uint32_t settleBuffer = 0;
    int rc = sscanf(res, "(%d)", &settleBuffer);
    if (rc > 0) {
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

bool DeepSkyDadAF1::readAlwaysOn()
{
    char res[DSD_RES]= {0};

    if (sendCommand("[GAON]", res) == false)
        return false;

    if (strcmp(res, "(0)") == 0) {
        int stepMode = IUFindOnSwitchIndex(&StepModeSP);
        if(stepMode != 0) {
            StepModeS[0].s = ISS_ON;
            StepModeS[1].s = ISS_OFF;
            StepModeS[2].s = ISS_OFF;
            StepModeS[3].s = ISS_OFF;
            sendCommandSet("[SSTP1]");
            IDSetSwitch(&StepModeSP, nullptr);
            LOG_WARN("Always on is set to NO. Switching to FULL step mode.");
        }

        AlwaysOnSP.s = IPS_IDLE;
        AlwaysOnS[ALWAYS_ON_NO].s = ISS_ON;
    }
    else if (strcmp(res, "(1)") == 0) {
        AlwaysOnSP.s = IPS_OK;
        AlwaysOnS[ALWAYS_ON_YES].s = ISS_ON;
    }

    else
    {
        LOGF_ERROR("Unknown error: isAlwaysOn value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readCurrentMove()
{
    char res[DSD_RES]= {0};

    if (sendCommand("[GCMV]", res) == false)
        return false;

    if (strcmp(res, "(180)") == 0) {
        CurrentMoveSP.s = IPS_IDLE;
        CurrentMoveS[CURRENT_25].s = ISS_ON;
    }
    else if (strcmp(res, "(170)") == 0) {
        CurrentMoveSP.s = IPS_IDLE;
        CurrentMoveS[CURRENT_50].s = ISS_ON;
    }
    else if (strcmp(res, "(160)") == 0) {
        CurrentMoveSP.s = IPS_IDLE;
        CurrentMoveS[CURRENT_75].s = ISS_ON;
    }
    else if (strcmp(res, "(150)") == 0) {
        CurrentMoveSP.s = IPS_IDLE;
        CurrentMoveS[CURRENT_100].s = ISS_ON;
    }

    else
    {
        LOGF_ERROR("Unknown error: currentMove value (%s)", res);
        return false;
    }

    return true;
}

bool DeepSkyDadAF1::readCurrentAo()
{
    char res[DSD_RES]= {0};

    if (sendCommand("[GCAO]", res) == false)
        return false;

    if (strcmp(res, "(190)") == 0) {
        CurrentAoSP.s = IPS_IDLE;
        CurrentAoS[CURRENT_25].s = ISS_ON;
    }
    else if (strcmp(res, "(180)") == 0) {
        CurrentAoSP.s = IPS_IDLE;
        CurrentAoS[CURRENT_50].s = ISS_ON;
    }
    else if (strcmp(res, "(170)") == 0) {
        CurrentAoSP.s = IPS_IDLE;
        CurrentAoS[CURRENT_75].s = ISS_ON;
    }
    else if (strcmp(res, "(160)") == 0) {
        CurrentAoSP.s = IPS_IDLE;
        CurrentAoS[CURRENT_100].s = ISS_ON;
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
    char res[DSD_RES]= {0};

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
    char cmd[DSD_RES]= {0};
    snprintf(cmd, DSD_RES, "[SPOS%06d]", ticks);
    return sendCommand(cmd);
}

bool DeepSkyDadAF1::ReverseFocuser(bool enabled)
{
    char cmd[DSD_RES]= {0};
    snprintf(cmd, DSD_RES, "[SREV%01d]", enabled ? 1 : 0);
    return sendCommand(cmd);
}

bool DeepSkyDadAF1::MoveFocuser(uint32_t position)
{
    char cmd[DSD_RES]= {0};
    char res[DSD_RES]= {0};
    snprintf(cmd, DSD_RES, "[STRG%06d]", position);
    // Set Position First
    if (sendCommand(cmd, res) == false)
        return false;

    if(strcmp(res, "!101)") == 0) {
        LOG_ERROR("MoveFocuserFailed - invalid target position (maximum relative movement is limited to 5000 steps)");
        return false;
    }

    // Now start motion toward position
    if (sendCommand("[SMOV]") == false)
        return false;

    return true;
}

bool DeepSkyDadAF1::setStepMode(FocusStepMode mode)
{
    char cmd[DSD_RES]= {0};

    int32_t intMode = 1;
    if(mode == FULL)
        intMode = 1;
    else if(mode == HALF)
        intMode = 2;
    else if(mode == QUARTER)
        intMode = 4;
    else if(mode == EIGHT)
        intMode = 8;

    snprintf(cmd, DSD_RES, "[SSTP%d]", intMode);
    return sendCommandSet(cmd);
}

bool DeepSkyDadAF1::setSettleBuffer(uint32_t settleBuffer)
{
    char cmd[DSD_RES]= {0};
    snprintf(cmd, DSD_RES, "[SBUF%06d]", settleBuffer);
    return sendCommandSet(cmd);
}

bool DeepSkyDadAF1::setAlwaysOnSwitch(char * names[], int n, ISState * states) {
    int current_mode = IUFindOnSwitchIndex(&AlwaysOnSP);

    IUUpdateSwitch(&AlwaysOnSP, states, names, n);

    int target_mode = IUFindOnSwitchIndex(&AlwaysOnSP);

    if (current_mode == target_mode)
    {
        IDSetSwitch(&AlwaysOnSP, nullptr);
        return true;
    }

    char cmd[DSD_RES]= {0};
    snprintf(cmd, DSD_RES, "[SAON%d]", target_mode);

    bool rc = sendCommandSet(cmd);
    if (!rc)
    {
        IUResetSwitch(&AlwaysOnSP);
        AlwaysOnS[current_mode].s = ISS_ON;
        AlwaysOnSP.s              = IPS_ALERT;
        IDSetSwitch(&AlwaysOnSP, nullptr);
        return false;
    } else if (target_mode == 1) {
        AlwaysOnSP.s = IPS_OK; //OK (green) if on
    } else {
        AlwaysOnSP.s = IPS_IDLE; //IDLE (gray) if off
    }

    if(target_mode == ALWAYS_ON_NO){
        int stepMode = IUFindOnSwitchIndex(&StepModeSP);
        if(stepMode != 0) {
            StepModeS[0].s = ISS_ON;
            StepModeS[1].s = ISS_OFF;
            StepModeS[2].s = ISS_OFF;
            StepModeS[3].s = ISS_OFF;
            sendCommandSet("[SSTP1]");
            IDSetSwitch(&StepModeSP, nullptr);
            LOG_WARN("Always on is set to NO. Switching to FULL step mode.");
        }
    }

    IDSetSwitch(&AlwaysOnSP, nullptr);
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

            FocusStepMode mode = FULL;
            if(target_mode == 0)
                mode = FULL;
            else if(target_mode == 1)
                mode = HALF;
            else if(target_mode == 2)
                mode = QUARTER;
            else if(target_mode == 3)
                mode = EIGHT;

            bool rc = setStepMode(mode);
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

            if(mode != FULL) {
                AlwaysOnS[1].s = ISS_ON;
                AlwaysOnS[0].s = ISS_OFF;
                IDSetSwitch(&AlwaysOnSP, nullptr);
                sendCommandSet("[SAO1]");
                LOG_WARN("Microstepping turned on. Switching Always on to YES.");
            }

            return true;
        }

        // Always on
        if (strcmp(AlwaysOnSP.name, name) == 0)
        {
            return setAlwaysOnSwitch(names, n, states);
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

            int targetCurrentValue = 180;
            switch(targetCurrent) {
                case 0:
                    targetCurrentValue = 190;
                    break;
                case 1:
                    targetCurrentValue = 180;
                    break;
                case 2:
                    targetCurrentValue = 170;
                    break;
                case 3:
                    targetCurrentValue = 160;
                    break;
            }

            char cmd[DSD_RES]= {0};
            snprintf(cmd, DSD_RES, "[SCMV%03d]", targetCurrentValue);

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

        // Current - always on
        if (strcmp(CurrentAoSP.name, name) == 0)
        {
            int current = IUFindOnSwitchIndex(&CurrentAoSP);

            IUUpdateSwitch(&CurrentAoSP, states, names, n);

            int targetCurrent = IUFindOnSwitchIndex(&CurrentAoSP);

            if (current == targetCurrent)
            {
                IDSetSwitch(&CurrentAoSP, nullptr);
                return true;
            }

            int targetCurrentValue = 160;
            switch(targetCurrent) {
                case 0:
                    targetCurrentValue = 180;
                    break;
                case 1:
                    targetCurrentValue = 170;
                    break;
                case 2:
                    targetCurrentValue = 160;
                    break;
                case 3:
                    targetCurrentValue = 150;
                    break;
            }

            char cmd[DSD_RES]= {0};
            snprintf(cmd, DSD_RES, "[SCAO%03d]", targetCurrentValue);

            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                IUResetSwitch(&CurrentAoSP);
                CurrentAoS[current].s = ISS_ON;
                CurrentAoSP.s              = IPS_ALERT;
                IDSetSwitch(&CurrentAoSP, nullptr);
                return false;
            }

            CurrentAoSP.s = IPS_OK;
            IDSetSwitch(&CurrentAoSP, nullptr);
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
            setSettleBuffer(SettleBufferN[0].value);
            SettleBufferNP.s = IPS_OK;
            IDSetNumber(&SettleBufferNP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void DeepSkyDadAF1::GetFocusParams()
{
    if (readPosition())
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (readStepMode())
        IDSetSwitch(&StepModeSP, nullptr);

    if (readSettleBuffer())
        IDSetNumber(&SettleBufferNP, nullptr);

    if (readAlwaysOn())
        IDSetSwitch(&AlwaysOnSP, nullptr);

    if (readCurrentMove())
        IDSetSwitch(&CurrentMoveSP, nullptr);

    if (readCurrentAo())
        IDSetSwitch(&CurrentAoSP, nullptr);
}

IPState DeepSkyDadAF1::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
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

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void DeepSkyDadAF1::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(POLLMS);
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

    rc = readSettleBuffer();
    if (rc)
    {
        if (fabs(lastSettleBuffer - SettleBufferN[0].value) >= 1)
        {
            IDSetNumber(&SettleBufferNP, nullptr);
            lastSettleBuffer = SettleBufferN[0].value;
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

    SetTimer(POLLMS);
}

bool DeepSkyDadAF1::AbortFocuser()
{
    return sendCommand("[STOP]");
}

bool DeepSkyDadAF1::saveConfigItems(FILE * fp)
{
    Focuser::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &StepModeSP);
    IUSaveConfigNumber(fp, &SettleBufferNP);
    IUSaveConfigSwitch(fp, &AlwaysOnSP);
    IUSaveConfigSwitch(fp, &CurrentMoveSP);
    IUSaveConfigSwitch(fp, &CurrentAoSP);

    return true;
}

bool DeepSkyDadAF1::sendCommand(const char * cmd, char * res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF]= {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if ((rc = tty_nread_section(PortFD, res, DSD_RES, DSD_DEL, DSD_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF]= {0};
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
    char res[DSD_RES]= {0};

    if (sendCommand(cmd, res) == false)
        return false;

    return strcmp(res, "(OK)") == 0;
}
