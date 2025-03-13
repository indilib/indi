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
    StepModeSP[S256].fill("S256", "1/256 Step", ISS_OFF);
    StepModeSP[S128].fill("S128", "1/128 Step", ISS_OFF);
    StepModeSP[S64].fill("S64", "1/64 Step", ISS_OFF);
    StepModeSP[S32].fill("S32", "1/32 Step", ISS_OFF);
    StepModeSP[S16].fill("S16", "1/16 Step", ISS_OFF);
    StepModeSP[S8].fill("S8", "1/8 Step", ISS_OFF);
    StepModeSP[S4].fill("S4", "1/4 Step", ISS_OFF);
    StepModeSP[S2].fill("S2", "1/2 Step", ISS_OFF);
    StepModeSP[S1].fill("S1", "Full Step", ISS_OFF);
    StepModeSP.fill(getDeviceName(), "Step Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Speed Mode
    SpeedModeSP[VERY_SLOW].fill("VERY_SLOW", "Very slow", ISS_OFF);
    SpeedModeSP[SLOW].fill("SLOW", "Slow", ISS_OFF);
    SpeedModeSP[MEDIUM].fill("MEDIUM", "Medium", ISS_OFF);
    SpeedModeSP[FAST].fill("FAST", "Fast", ISS_OFF);
    SpeedModeSP[VERY_FAST].fill("VERY_FAST", "Very fast", ISS_OFF);
    SpeedModeSP.fill(getDeviceName(), "Speed Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(50000.);
    FocusRelPosNP[0].setValue(0.);
    FocusRelPosNP[0].setStep(10.);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(1000000.);
    FocusAbsPosNP[0].setValue(50000.);
    FocusAbsPosNP[0].setStep(5000.);

    FocusMaxPosNP[0].setMin(0.);
    FocusMaxPosNP[0].setMax(1000000.);
    FocusMaxPosNP[0].setValue(1000000.);
    FocusMaxPosNP[0].setStep(5000.);

    FocusSyncNP[0].setMin(0.);
    FocusSyncNP[0].setMax(1000000.);
    FocusSyncNP[0].setValue(50000.);
    FocusSyncNP[0].setStep(5000.);

    FocusBacklashNP[0].setMin(-1000);
    FocusBacklashNP[0].setMax(1000);
    FocusBacklashNP[0].setStep(1);
    FocusBacklashNP[0].setValue(0);

    // Settle buffer
    SettleBufferNP[0].fill("SETTLE_BUFFER", "Period (ms)", "%5.0f", 0, 99999, 100, 0);
    SettleBufferNP.fill(getDeviceName(), "FOCUS_SETTLE_BUFFER", "Settle buffer",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Motor move multiplier
    MoveCurrentMultiplierNP[0].fill("MOTOR_MOVE_MULTIPLIER", "%", "%3.0f", 1, 100, 1, 90);
    MoveCurrentMultiplierNP.fill(getDeviceName(), "FOCUS_MMM",
                       "Move current multiplier",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Motor hold multiplier
    HoldCurrentMultiplierNP[0].fill("MOTOR_HOLD_MULTIPLIER", "%", "%3.0f", 1, 100, 1, 40);
    HoldCurrentMultiplierNP.fill(getDeviceName(), "FOCUS_MHM",
                       "Hold current multiplier",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
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
        defineProperty(StepModeSP);
        defineProperty(SpeedModeSP);
        defineProperty(SettleBufferNP);
        defineProperty(MoveCurrentMultiplierNP);
        defineProperty(HoldCurrentMultiplierNP);
        defineProperty(TemperatureNP);

        GetFocusParams();

        LOG_INFO("deepSkyDadAf3 parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(StepModeSP);
        deleteProperty(SpeedModeSP);
        deleteProperty(SettleBufferNP);
        deleteProperty(MoveCurrentMultiplierNP);
        deleteProperty(HoldCurrentMultiplierNP);
        deleteProperty(TemperatureNP);
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
        StepModeSP[S1].setState(ISS_ON);
    else if (strcmp(res, "(2)") == 0)
        StepModeSP[S2].setState(ISS_ON);
    else if (strcmp(res, "(4)") == 0)
        StepModeSP[S4].setState(ISS_ON);
    else if (strcmp(res, "(8)") == 0)
        StepModeSP[S8].setState(ISS_ON);
    else if (strcmp(res, "(16)") == 0)
        StepModeSP[S16].setState(ISS_ON);
    else if (strcmp(res, "(32)") == 0)
        StepModeSP[S32].setState(ISS_ON);
    else if (strcmp(res, "(64)") == 0)
        StepModeSP[S64].setState(ISS_ON);
    else if (strcmp(res, "(128)") == 0)
        StepModeSP[S128].setState(ISS_ON);
    else if (strcmp(res, "(256)") == 0)
        StepModeSP[S256].setState(ISS_ON);
    else
    {
        LOGF_ERROR("Unknown error: focuser step value (%s)", res);
        return false;
    }

    StepModeSP.setState(IPS_OK);
    return true;
}

bool DeepSkyDadAF3::readSpeedMode()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GSPD]", res) == false)
        return false;

    if (strcmp(res, "(1)") == 0)
        SpeedModeSP[VERY_SLOW].setState(ISS_ON);
    else if (strcmp(res, "(2)") == 0)
        SpeedModeSP[SLOW].setState(ISS_ON);
    else if (strcmp(res, "(3)") == 0)
        SpeedModeSP[MEDIUM].setState(ISS_ON);
    else if (strcmp(res, "(4)") == 0)
        SpeedModeSP[FAST].setState(ISS_ON);
    else if (strcmp(res, "(5)") == 0)
        SpeedModeSP[VERY_FAST].setState(ISS_ON);
    else
    {
        LOGF_ERROR("Unknown error: focuser speed value (%s)", res);
        return false;
    }

    SpeedModeSP.setState(IPS_OK);
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
        FocusAbsPosNP[0].setValue(pos);
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
        FocusMaxPosNP[0].setValue(steps);
        FocusMaxPosNP.setState(IPS_OK);
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

bool DeepSkyDadAF3::readSettleBuffer()
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

bool DeepSkyDadAF3::readMoveCurrentMultiplier()
{
    char res[DSD_RES] = {0};

    if (sendCommand("[GMMM]", res) == false)
        return false;

    uint32_t mcm = 0;
    int rc = sscanf(res, "(%d)", &mcm);
    if (rc > 0)
    {
        MoveCurrentMultiplierNP[0].setValue(mcm);
        MoveCurrentMultiplierNP.setState(IPS_OK);
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
        HoldCurrentMultiplierNP[0].setValue(hcm);
        HoldCurrentMultiplierNP.setState(IPS_OK);
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
        TemperatureNP[0].setValue(temp);
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
        if (StepModeSP.isNameMatch(name))
        {
            int current_mode = StepModeSP.findOnSwitchIndex();

            StepModeSP.update(states, names, n);

            int target_mode = StepModeSP.findOnSwitchIndex();

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

        // Focus Speed Mode
        if (SpeedModeSP.isNameMatch(name))
        {
            int current_mode = SpeedModeSP.findOnSwitchIndex();

            SpeedModeSP.update(states, names, n);

            int target_mode = SpeedModeSP.findOnSwitchIndex();

            if (current_mode == target_mode)
            {
                SpeedModeSP.setState(IPS_OK);
                SpeedModeSP.apply();
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
                SpeedModeSP.reset();
                SpeedModeSP[current_mode].setState(ISS_ON);
                SpeedModeSP.setState(IPS_ALERT);
                SpeedModeSP.apply();
                return false;
            }

            SpeedModeSP.setState(IPS_OK);
            SpeedModeSP.apply();
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

        // Move current multiplier
        if (MoveCurrentMultiplierNP.isNameMatch(name))
        {
            MoveCurrentMultiplierNP.update(values, names, n);
            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SMMM%03d]", static_cast<int>(MoveCurrentMultiplierNP[0].getValue()));
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                MoveCurrentMultiplierNP.setState(IPS_ALERT);
                return false;
            }

            MoveCurrentMultiplierNP.setState(IPS_OK);
            MoveCurrentMultiplierNP.apply();
            return true;
        }

        // Hold current multiplier
        if (HoldCurrentMultiplierNP.isNameMatch(name))
        {
            HoldCurrentMultiplierNP.update(values, names, n);
            char cmd[DSD_RES] = {0};
            snprintf(cmd, DSD_RES, "[SMHM%03d]", static_cast<int>(HoldCurrentMultiplierNP[0].getValue()));
            bool rc = sendCommandSet(cmd);
            if (!rc)
            {
                HoldCurrentMultiplierNP.setState(IPS_ALERT);
                return false;
            }

            HoldCurrentMultiplierNP.setState(IPS_OK);
            HoldCurrentMultiplierNP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void DeepSkyDadAF3::GetFocusParams()
{
    StepModeSP.reset();
    SpeedModeSP.reset();

    if (readPosition())
        FocusAbsPosNP.apply();

    if (readStepMode())
        StepModeSP.reset();

    if (readSpeedMode())
        SpeedModeSP.apply();

    if (readSettleBuffer())
        SettleBufferNP.apply();

    if (readMoveCurrentMultiplier())
        MoveCurrentMultiplierNP.apply();

    if (readHoldCurrentMultiplier())
        HoldCurrentMultiplierNP.apply();

    if (readMaxPosition())
        FocusMaxPosNP.apply();

    if (readMaxMovement())
        FocusMaxPosNP.apply();

    if (readTemperature())
        TemperatureNP.apply();
}

IPState DeepSkyDadAF3::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    INDI_UNUSED(speed);
    // either go all the way in or all the way out
    // then use timer to stop
    if (dir == FOCUS_INWARD)
        MoveFocuser(0);
    else
        MoveFocuser(FocusMaxPosNP[0].getValue());

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
    FocusAbsPosNP.setState(IPS_IDLE);
    FocusRelPosNP.setState(IPS_IDLE);
    FocusTimerNP.setState(IPS_IDLE);
    FocusTimerNP[0].setValue(0);
    FocusAbsPosNP.apply();
    FocusRelPosNP.apply();
    FocusTimerNP.apply();
}


IPState DeepSkyDadAF3::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    double bcValue = FocusBacklashNP[0].getValue();
    int diff = targetTicks - FocusAbsPosNP[0].getValue();
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
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosNP[0].getMax()), newPosition));
    if (!MoveAbsFocuser(newPosition))
        return IPS_ALERT;

    // JM 2019-02-10: This is already set by the framework
    //FocusRelPosNP[0].setValue(ticks);
    //FocusRelPosNP.setState(IPS_BUSY);

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
            if( backlashComp == 0 )
            {
                FocusAbsPosNP.setState(IPS_OK);
                FocusRelPosNP.setState(IPS_OK);
            }
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();

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
        if (std::abs(lastTemperature - TemperatureNP[0].getValue()) >= 0.1)
        {
            TemperatureNP.apply();
            lastTemperature = TemperatureNP[0].getValue();
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

    StepModeSP.save(fp);
    SpeedModeSP.save(fp);
    SettleBufferNP.save(fp);
    MoveCurrentMultiplierNP.save(fp);
    HoldCurrentMultiplierNP.save(fp);

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
