/*
    SestoSenso 2 Focuser
    Copyright (C) 2020 Piotr Zyziuk
    Copyright (C) 2020 Jasem Mutlaq (Added Esatto support)

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

#include "sestosenso2.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <algorithm>

#include <assert.h>
#include <termios.h>
#include <unistd.h>
#include <connectionplugins/connectionserial.h>
#include <sys/ioctl.h>

static std::unique_ptr<SestoSenso2> sesto(new SestoSenso2());

static const char *MOTOR_TAB  = "Motor";
static const char *ENVIRONMENT_TAB  = "Environment";
// Settings names for the default motor settings presets
const char *MOTOR_PRESET_NAMES[] = { "light", "medium", "slow" };

SestoSenso2::SestoSenso2()
{
    setVersion(1, 0);

    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_HAS_BACKLASH | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);

    m_MotionProgressTimer.callOnTimeout(std::bind(&SestoSenso2::checkMotionProgressCallback, this));
    m_MotionProgressTimer.setSingleShot(true);

    //    m_HallSensorTimer.callOnTimeout(std::bind(&SestoSenso2::checkHallSensorCallback, this));
    //    m_HallSensorTimer.setSingleShot(true);
    //    m_HallSensorTimer.setInterval(1000);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::initProperties()
{

    INDI::Focuser::initProperties();

    FocusBacklashN[0].min = 0;
    FocusBacklashN[0].max = 10000;
    FocusBacklashN[0].step = 1;
    FocusBacklashN[0].value = 0;

    setConnectionParams();

    // Firmware information
    FirmwareTP[FIRMWARE_SN].fill("SERIALNUMBER", "Serial Number", "");
    FirmwareTP[FIRMWARE_VERSION].fill("VERSION", "Version", "");
    FirmwareTP.fill(getDeviceName(), "FOCUS_FIRMWARE", "Firmware", CONNECTION_TAB, IP_RO, 0,  IPS_IDLE);

    // Voltage Information
    VoltageInNP[0].fill("VOLTAGEIN", "Volts", "%.2f", 0, 100, 0., 0.);
    VoltageInNP.fill(getDeviceName(), "VOLTAGE_IN", "Voltage in", ENVIRONMENT_TAB, IP_RO, 0, IPS_IDLE);

    // Focuser temperature
    TemperatureNP[TEMPERATURE_MOTOR].fill("TEMPERATURE", "Motor (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP[TEMPERATURE_EXTERNAL].fill("TEMPERATURE_ETX", "External (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", ENVIRONMENT_TAB, IP_RO, 0, IPS_IDLE);

    // Current Speed
    SpeedNP[0].fill("SPEED", "steps/s", "%.f", 0, 7000., 1, 0);
    SpeedNP.fill(getDeviceName(), "FOCUS_SPEED", "Motor Speed", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Focuser calibration
    CalibrationMessageTP[0].fill("CALIBRATION", "Calibration stage", "Press START to begin the Calibration.");
    CalibrationMessageTP.fill(getDeviceName(), "CALIBRATION_MESSAGE", "Calibration", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Calibration
    CalibrationSP[CALIBRATION_START].fill("CALIBRATION_START", "Start", ISS_OFF);
    CalibrationSP[CALIBRATION_NEXT].fill("CALIBRATION_NEXT", "Next", ISS_OFF);
    CalibrationSP.fill(getDeviceName(), "FOCUS_CALIBRATION", "Calibration", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Speed Moves
    FastMoveSP[FASTMOVE_IN].fill("FASTMOVE_IN", "Move In", ISS_OFF);
    FastMoveSP[FASTMOVE_OUT].fill("FASTMOVE_OUT", "Move out", ISS_OFF);
    FastMoveSP[FASTMOVE_STOP].fill("FASTMOVE_STOP", "Stop", ISS_OFF);
    FastMoveSP.fill(getDeviceName(), "FAST_MOVE", "Calibration Move", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Hold state
    IUFillSwitch(&MotorHoldS[MOTOR_HOLD_ON], "HOLD_ON", "Hold On", ISS_OFF);
    IUFillSwitch(&MotorHoldS[MOTOR_HOLD_OFF], "HOLD_OFF", "Hold Off", ISS_OFF);
    IUFillSwitchVector(&MotorHoldSP, MotorHoldS, 2, getDeviceName(), "MOTOR_HOLD", "Motor Hold", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Override the default Max. Position to make it Read-Only
    IUFillNumberVector(&FocusMaxPosNP, FocusMaxPosN, 1, getDeviceName(), "FOCUS_MAX", "Max. Position", MAIN_CONTROL_TAB, IP_RO,
                       0, IPS_IDLE);

    // Motor rate
    MotorRateNP[MOTOR_RATE_ACC].fill("ACC", "Acceleration", "%.f", 1, 10, 1, 1);
    MotorRateNP[MOTOR_RATE_RUN].fill("RUN", "Run Speed", "%.f", 1, 10, 1, 2);
    MotorRateNP[MOTOR_RATE_DEC].fill("DEC", "Deceleration", "%.f", 1, 10, 1, 1);
    MotorRateNP.fill(getDeviceName(), "MOTOR_RATE", "Motor Rate", MOTOR_TAB, IP_RW, 0, IPS_IDLE);

    // Motor current
    MotorCurrentNP[MOTOR_CURR_ACC].fill("CURR_ACC", "Acceleration", "%.f", 1, 10, 1, 7);
    MotorCurrentNP[MOTOR_CURR_RUN].fill("CURR_RUN", "Run", "%.f", 1, 10, 1, 7);
    MotorCurrentNP[MOTOR_CURR_DEC].fill("CURR_DEC", "Deceleration", "%.f", 1, 10, 1, 7);
    MotorCurrentNP[MOTOR_CURR_HOLD].fill("CURR_HOLD", "Hold", "%.f", 0, 5, 1, 3);
    MotorCurrentNP.fill(getDeviceName(), "MOTOR_CURRENT", "Current", MOTOR_TAB, IP_RW, 0, IPS_IDLE);

    // Load motor preset
    MotorApplyPresetSP[MOTOR_APPLY_LIGHT].fill("MOTOR_APPLY_LIGHT", "Light", ISS_OFF);
    MotorApplyPresetSP[MOTOR_APPLY_MEDIUM].fill("MOTOR_APPLY_MEDIUM", "Medium", ISS_OFF);
    MotorApplyPresetSP[MOTOR_APPLY_HEAVY].fill("MOTOR_APPLY_HEAVY", "Heavy", ISS_OFF);
    MotorApplyPresetSP.fill(getDeviceName(), "MOTOR_APPLY_PRESET", "Apply Preset", MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Load user preset
    MotorApplyUserPresetSP[MOTOR_APPLY_USER1].fill("MOTOR_APPLY_USER1", "User 1", ISS_OFF);
    MotorApplyUserPresetSP[MOTOR_APPLY_USER2].fill("MOTOR_APPLY_USER2", "User 2", ISS_OFF);
    MotorApplyUserPresetSP[MOTOR_APPLY_USER3].fill("MOTOR_APPLY_USER3", "User 3", ISS_OFF);
    MotorApplyUserPresetSP.fill(getDeviceName(), "MOTOR_APPLY_USER_PRESET", "Apply Custom", MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Save user preset
    MotorSaveUserPresetSP[MOTOR_SAVE_USER1].fill("MOTOR_SAVE_USER1", "User 1", ISS_OFF);
    MotorSaveUserPresetSP[MOTOR_SAVE_USER2].fill("MOTOR_SAVE_USER2", "User 2", ISS_OFF);
    MotorSaveUserPresetSP[MOTOR_SAVE_USER3].fill("MOTOR_SAVE_USER3", "User 3", ISS_OFF);
    MotorSaveUserPresetSP.fill(getDeviceName(), "MOTOR_SAVE_USER_PRESET", "Save Custom", MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 50000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1000;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 200000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    FocusMaxPosN[0].value = 2097152;
    PresetN[0].max = FocusMaxPosN[0].value;
    PresetN[1].max = FocusMaxPosN[0].value;
    PresetN[2].max = FocusMaxPosN[0].value;

    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::updateProperties()
{
    if (isConnected() && updateMaxLimit() == false)
        LOGF_WARN("Check you have the latest %s firmware. Focuser requires calibration.", getDeviceName());

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(SpeedNP);
        defineProperty(CalibrationMessageTP);
        defineProperty(CalibrationSP);
        defineProperty(MotorRateNP);
        defineProperty(MotorCurrentNP);
        defineProperty(MotorHoldSP);
        defineProperty(MotorApplyPresetSP);
        defineProperty(MotorApplyUserPresetSP);
        defineProperty(MotorSaveUserPresetSP);

        defineProperty(FirmwareTP);

        if (updateTemperature())
            defineProperty(TemperatureNP);

        if (updateVoltageIn())
            defineProperty(VoltageInNP);

        if (getStartupValues())
            LOG_INFO("Parameters updated, focuser ready for use.");
        else
            LOG_WARN("Failed to inquire parameters. Check logs.");
    }
    else
    {
        if (TemperatureNP->getState() == IPS_OK)
            deleteProperty(TemperatureNP);
        deleteProperty(FirmwareTP);
        deleteProperty(VoltageInNP);
        deleteProperty(CalibrationMessageTP);
        deleteProperty(CalibrationSP);
        deleteProperty(SpeedNP);
        deleteProperty(MotorRateNP);
        deleteProperty(MotorCurrentNP);
        deleteProperty(MotorHoldSP);
        deleteProperty(MotorApplyPresetSP);
        deleteProperty(MotorApplyUserPresetSP);
        deleteProperty(MotorSaveUserPresetSP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::Handshake()
{
    if (Ack())
    {
        LOGF_INFO("%s is online. Getting focus parameters...", getDeviceName());
        return true;
    }

    LOG_INFO("Error retrieving data from device, please ensure focuser is powered and the port is correct.");
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::SetFocuserBacklash(int32_t steps)
{
    return m_SestoSenso2->setBacklash(steps);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *SestoSenso2::getDefaultName()
{
    return "Sesto Senso 2";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::updateTemperature()
{
    double temperature = 0;

    if (isSimulation())
        temperature = 23.5;
    else if ( m_SestoSenso2->getMotorTemp(temperature) == false)
        return false;

    if (temperature > 90)
        return false;

    TemperatureNP[TEMPERATURE_MOTOR].setValue(temperature);
    TemperatureNP.setState(IPS_OK);

    // External temperature - Optional
    if (m_SestoSenso2->getExternalTemp(temperature))
    {
        if (temperature < 90)
            TemperatureNP[TEMPERATURE_EXTERNAL].setValue(temperature);
        else
            TemperatureNP[TEMPERATURE_EXTERNAL].setValue(-273.15);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::updateMaxLimit()
{
    uint32_t maxLimit = 0;

    if (isSimulation())
        return true;

    if (m_SestoSenso2->getMaxPosition(maxLimit) == false)
        return false;

    FocusMaxPosN[0].max = maxLimit;
    if (FocusMaxPosN[0].value > maxLimit)
        FocusMaxPosN[0].value = maxLimit;

    FocusAbsPosN[0].min   = 0;
    FocusAbsPosN[0].max   = maxLimit;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;

    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = FocusAbsPosN[0].step * 10;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = FocusAbsPosN[0].step;

    PresetN[0].max = maxLimit;
    PresetN[0].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;
    PresetN[1].max = maxLimit;
    PresetN[1].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;
    PresetN[2].max = maxLimit;
    PresetN[2].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;


    FocusMaxPosNP.s = IPS_OK;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::updatePosition()
{
    uint32_t steps = 0;
    if (isSimulation())
        steps = static_cast<uint32_t>(FocusAbsPosN[0].value);
    else if (m_SestoSenso2->getAbsolutePosition(steps) == false)
        return false;

    FocusAbsPosN[0].value = steps;
    FocusAbsPosNP.s = IPS_OK;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::updateVoltageIn()
{
    double voltageIn = 0;

    if (isSimulation())
        voltageIn = 12.0;
    else if (m_SestoSenso2->getVoltage12v(voltageIn) == false)
        return false;

    if (voltageIn > 24)
        return false;

    VoltageInNP[0].setValue(voltageIn);
    VoltageInNP.setState((voltageIn >= 11.0) ? IPS_OK : IPS_ALERT);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::fetchMotorSettings()
{
    // Fetch driver state and reflect in INDI
    PrimalucaLabs::MotorRates ms;
    PrimalucaLabs::MotorCurrents mc;
    bool motorHoldActive = false;

    if (isSimulation())
    {
        ms.accRate = 1;
        ms.runSpeed = 2;
        ms.decRate = 1;
        mc.accCurrent = 3;
        mc.runCurrent = 4;
        mc.decCurrent = 3;
        mc.holdCurrent = 2;
    }
    else
    {
        if (!m_SestoSenso2->getMotorSettings(ms, mc, motorHoldActive))
        {
            MotorRateNP.setState(IPS_IDLE);
            MotorCurrentNP.setState(IPS_IDLE);
            MotorHoldSP.setState(IPS_IDLE);
            return false;
        }
    }

    MotorRateNP[MOTOR_RATE_ACC].setValue(ms.accRate);
    MotorRateNP[MOTOR_RATE_RUN].setValue(ms.runSpeed);
    MotorRateNP[MOTOR_RATE_DEC].setValue(ms.decRate);
    MotorRateNP.setState(IPS_OK);
    MotorRateNP->apply();

    MotorCurrentNP[MOTOR_CURR_ACC].setValue(mc.accCurrent);
    MotorCurrentNP[MOTOR_CURR_RUN].setValue(mc.runCurrent);
    MotorCurrentNP[MOTOR_CURR_DEC].setValue(mc.decCurrent);
    MotorCurrentNP[MOTOR_CURR_HOLD].setValue(mc.holdCurrent);
    MotorCurrentNP.setState(IPS_OK);
    MotorCurrentNP->apply();

    // Also update motor hold switch
    auto activeSwitchID = motorHoldActive ? "HOLD_ON" : "HOLD_OFF";
    auto sp = MotorHoldSP->findWidgetByName(activeSwitchID);
    assert(sp != nullptr && "Motor hold switch not found");
    if (sp)
    {
        MotorHoldSP.reset();
        sp->s = ISS_ON;
        MotorHoldSP.setState(motorHoldActive ? IPS_OK : IPS_ALERT);
        MotorHoldSP->apply();
    }

    if (motorHoldActive && mc.holdCurrent == 0)
    {
        LOG_WARN("Motor hold current set to 0, motor hold setting will have no effect");
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::applyMotorRates()
{
    if (isSimulation())
        return true;

    // Send INDI state to driver
    PrimalucaLabs::MotorRates mr;
    mr.accRate = static_cast<uint32_t>(MotorRateNP[MOTOR_RATE_ACC].getValue());
    mr.runSpeed = static_cast<uint32_t>(MotorRateNP[MOTOR_RATE_RUN].getValue());
    mr.decRate = static_cast<uint32_t>(MotorRateNP[MOTOR_RATE_DEC].getValue());

    if (!m_SestoSenso2->setMotorRates(mr))
    {
        LOG_ERROR("Failed to apply motor rates");
        // TODO: Error state?
        return false;
    }

    LOGF_INFO("Motor rates applied: Acc: %u Run: %u Dec: %u", mr.accRate, mr.runSpeed, mr.decRate);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::applyMotorCurrents()
{
    if (isSimulation())
        return true;

    // Send INDI state to driver
    PrimalucaLabs::MotorCurrents mc;
    mc.accCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_ACC].getValue());
    mc.runCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_RUN].getValue());
    mc.decCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_DEC].getValue());
    mc.holdCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_HOLD].getValue());

    if (!m_SestoSenso2->setMotorCurrents(mc))
    {
        LOG_ERROR("Failed to apply motor currents");
        return false;
    }

    LOGF_INFO("Motor currents applied: Acc: %u Run: %u Dec: %u Hold: %u", mc.accCurrent, mc.runCurrent, mc.decCurrent,
              mc.holdCurrent);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::isMotionComplete()
{
    if (isSimulation())
    {
        int32_t nextPos = FocusAbsPosN[0].value;
        int32_t targPos = static_cast<int32_t>(targetPos);

        if (targPos > nextPos)
            nextPos += 250;
        else if (targPos < nextPos)
            nextPos -= 250;

        if (abs(nextPos - targPos) < 250)
            nextPos = targetPos;
        else if (nextPos < 0)
            nextPos = 0;
        else if (nextPos > FocusAbsPosN[0].max)
            nextPos = FocusAbsPosN[0].max;

        FocusAbsPosN[0].value = nextPos;
    }
    else
    {
        uint32_t speed {0};
        if(m_SestoSenso2->getCurrentSpeed(speed))
        {
            SpeedNP[0].setValue(speed);
            SpeedNP->setState(IPS_OK);

            uint32_t position {0};
            if(m_SestoSenso2->getAbsolutePosition(position))
            {
                FocusAbsPosN[0].value = position;
            }
        }

    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Calibrate focuser
        if (CalibrationSP->isNameMatch(name))
        {
            CalibrationSP.update(states, names, n);
            auto current_switch = IUFindOnSwitchIndex(&CalibrationSP);
            CalibrationSP[current_switch].setState(ISS_ON);
            CalibrationSP->apply();

            if (current_switch == CALIBRATION_START)
            {
                if (cStage == Idle || cStage == Complete )
                {
                    // Start the calibration process
                    LOG_INFO("Start Calibration");
                    CalibrationSP.setState(IPS_BUSY);
                    CalibrationSP->apply();

                    //
                    // Init
                    //
                    if (m_IsSestoSenso2 && m_SestoSenso2->initCalibration() == false)
                        return false;

                    CalibrationMessageTP[0].setText("Set focus in MIN position and then press NEXT.");
                    CalibrationMessageTP->apply();

                    // Motor hold disabled during calibration init, so fetch new hold state
                    fetchMotorSettings();

                    // Set next step
                    cStage = GoToMiddle;
                }
                else
                {
                    LOG_INFO("Already started calibration. Proceed to next step.");
                    CalibrationMessageTP[0].setText("Already started. Proceed to NEXT.");
                    CalibrationMessageTP->apply();
                }
            }
            else if (current_switch == CALIBRATION_NEXT)
            {
                if (cStage == GoToMiddle)
                {
                    defineProperty(&FastMoveSP);
                    if (m_IsSestoSenso2)
                    {
                        if (m_SestoSenso2->storeAsMinPosition() == false)
                            return false;

                        CalibrationMessageTP[0].setText("Press MOVE OUT to move focuser out (CAUTION!)");
                        CalibrationMessageTP->apply();
                        cStage = GoMinimum;
                    }
                    // For Esatto, start moving out immediately
                    else
                    {
                        cStage = GoMaximum;

                        ISState fs[1] = { ISS_ON };
                        const char *fn[1] =  { FastMoveSP[FASTMOVE_OUT].getName() };
                        ISNewSwitch(getDeviceName(), FastMoveSP.getName(), fs, const_cast<char **>(fn), 1);
                    }
                }
                else if (cStage == GoMinimum)
                {
                    if (m_SestoSenso2->storeAsMaxPosition() == false)
                        return false;

                    CalibrationMessageTP[0].setText("Press NEXT to finish.");
                    CalibrationMessageTP->apply();
                    cStage = GoMaximum;
                }
                else if (cStage == GoMaximum)
                {
                    uint32_t maxLimit = 0;

                    if (m_SestoSenso2->getMaxPosition(maxLimit) == false)
                        return false;

                    LOGF_INFO("MAX setting is %d", maxLimit);

                    FocusMaxPosN[0].max = maxLimit;
                    FocusMaxPosN[0].value = maxLimit;

                    FocusAbsPosN[0].min   = 0;
                    FocusAbsPosN[0].max   = maxLimit;
                    FocusAbsPosN[0].value = maxLimit;
                    FocusAbsPosN[0].step  = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;

                    FocusRelPosN[0].min   = 0.;
                    FocusRelPosN[0].max   = FocusAbsPosN[0].step * 10;
                    FocusRelPosN[0].value = 0;
                    FocusRelPosN[0].step  = FocusAbsPosN[0].step;

                    PresetN[0].max = maxLimit;
                    PresetN[0].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;
                    PresetN[1].max = maxLimit;
                    PresetN[1].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;
                    PresetN[2].max = maxLimit;
                    PresetN[2].step = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;

                    FocusMaxPosNP.s = IPS_OK;
                    IUUpdateMinMax(&FocusAbsPosNP);
                    IUUpdateMinMax(&FocusRelPosNP);
                    IUUpdateMinMax(&PresetNP);
                    IUUpdateMinMax(&FocusMaxPosNP);

                    CalibrationMessageTP[0].setText("Calibration Completed.");
                    CalibrationMessageTP->apply();

                    deleteProperty(FastMoveSP);
                    cStage = Complete;

                    LOG_INFO("Calibration completed");
                    CalibrationSP->setState(IPS_OK);
                    CalibrationSP->apply();
                    CalibrationSP[current_switch].setState(ISS_OFF);
                    CalibrationSP.apply();

                    // Double check motor hold state after calibration
                    fetchMotorSettings();
                }
                else
                {
                    CalibrationMessageTP[0].setText("Calibration not in progress.");
                    CalibrationMessageTP->apply();
                }

            }
            return true;
        }
        // Fast motion
        else if (FastMoveSP->isNameMatch(name))
        {
            FastMoveSP.update(states, names, n);
            auto current_switch = IUFindOnSwitchIndex(&FastMoveSP);

            switch (current_switch)
            {
                case FASTMOVE_IN:
                    if (m_SestoSenso2->fastMoveIn() == false)
                        return false;
                    break;
                case FASTMOVE_OUT:
                    // NOT CORRECT FIX ME
                    // Only use when calibration active?
                    if (m_IsSestoSenso2)
                    {
                        if (m_SestoSenso2->goOutToFindMaxPos() == false)
                        {
                            return false;
                        }
                        CalibrationMessageTP[0].setText("Press STOP focuser almost at MAX position.");
                        fetchMotorSettings();
                    }
                    else
                    {
                        if (m_SestoSenso2->fastMoveOut())
                        {
                            CalibrationMessageTP[0].setText("Focusing out to detect hall sensor.");
                            m_MotionProgressTimer.start(500);
                        }
                    }
                    IDSetText(&CalibrationMessageTP, nullptr);
                    break;
                case FASTMOVE_STOP:
                    if (m_SestoSenso2->stop() == false)
                    {
                        return false;
                    }
                    CalibrationMessageTP[0].setText("Press NEXT to store max limit.");
                    CalibrationMessageTP->apply();
                    break;
                default:
                    break;
            }

            FastMoveSP.setState(IPS_BUSY);
            FastMoveSP->apply();
            return true;
        }
        // Homing
        else if (MotorHoldSP->isNameMatch(name))
        {
            MotorHoldSP.update(states, names, n);
            auto sp = IUFindOnSwitch(&MotorHoldSP);
            assert(sp != nullptr);

            // NOTE: Default to HOLD_ON as a safety feature
            if (!strcmp(sp->name, "HOLD_OFF"))
            {
                m_SestoSenso2->setMotorHold(false);
                MotorHoldSP.setState(IPS_ALERT);
                LOG_INFO("Motor hold OFF. You may now manually adjust the focuser. Remember to enable motor hold once done.");
            }
            else
            {
                m_SestoSenso2->setMotorHold(true);
                MotorHoldSP.setState(IPS_OK);
                LOG_INFO("Motor hold ON. Do NOT attempt to manually adjust the focuser!");
                if (MotorCurrentNP[MOTOR_CURR_HOLD].getValue() < 2.0)
                {
                    LOGF_WARN("Motor hold current set to %.1f: This may be insufficent to hold focus", MotorCurrentNP[MOTOR_CURR_HOLD].getValue());
                }
            }

            IDSetSwitch(&MotorHoldSP, nullptr);
            return true;
        }
        else if (MotorApplyPresetSP->isNameMatch(name))
        {
            MotorApplyPresetSP.update(states, names, n);
            auto index = IUFindOnSwitchIndex(&MotorApplyPresetSP);
            assert(index >= 0 && index < 3);

            const char* presetName = MOTOR_PRESET_NAMES[index];

            if (m_SestoSenso2->applyMotorPreset(presetName))
            {
                LOGF_INFO("Loaded motor preset: %s", presetName);
                MotorApplyPresetSP.setState(IPS_IDLE);
            }
            else
            {
                LOGF_ERROR("Failed to load motor preset: %s", presetName);
                MotorApplyPresetSP.setState(IPS_ALERT);
            }

            MotorApplyPresetSP[index].s = ISS_OFF;
            IDSetSwitch(&MotorApplyPresetSP, nullptr);

            fetchMotorSettings();
            return true;
        }
        //        else if (MotorApplyUserPresetSP->isNameMatch(name))
        //        {
        //            MotorApplyUserPresetSP.update(states, names, n);
        //            auto index = IUFindOnSwitchIndex(&MotorApplyUserPresetSP);
        //            assert(index >= 0 && index < 3);
        //            uint32_t userIndex = index + 1;

        //            if (m_SestoSenso2->applyMotorPreset(userIndex))
        //            {
        //                LOGF_INFO("Loaded motor user preset: %u", userIndex);
        //                MotorApplyUserPresetSP.setState(IPS_IDLE);
        //            }
        //            else
        //            {
        //                LOGF_ERROR("Failed to load motor user preset: %u", userIndex);
        //                MotorApplyUserPresetSP.s = IPS_ALERT;
        //            }

        //            MotorApplyUserPresetS[index].s = ISS_OFF;
        //            IDSetSwitch(&MotorApplyUserPresetSP, nullptr);

        //            fetchMotorSettings();
        //            return true;
        //        }
        //        else if (!strcmp(name, MotorSaveUserPresetSP.name))
        //        {
        //            IUUpdateSwitch(&MotorSaveUserPresetSP, states, names, n);
        //            int index = IUFindOnSwitchIndex(&MotorSaveUserPresetSP);
        //            assert(index >= 0 && index < 3);
        //            uint32_t userIndex = index + 1;

        //            MotorRates mr;
        //            mr.accRate = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_ACC].value);
        //            mr.runSpeed = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_RUN].value);
        //            mr.decRate = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_DEC].value);

        //            MotorCurrents mc;
        //            mc.accCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_ACC].value);
        //            mc.runCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_RUN].value);
        //            mc.decCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_DEC].value);
        //            mc.holdCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_HOLD].value);

        //            if (command->saveMotorUserPreset(userIndex, mr, mc))
        //            {
        //                LOGF_INFO("Saved motor user preset %u to firmware", userIndex);
        //                MotorSaveUserPresetSP.s = IPS_IDLE;
        //            }
        //            else
        //            {
        //                LOGF_ERROR("Failed to save motor user preset %u to firmware", userIndex);
        //                MotorSaveUserPresetSP.s = IPS_ALERT;
        //            }

        //            MotorSaveUserPresetS[index].s = ISS_OFF;
        //            IDSetSwitch(&MotorSaveUserPresetSP, nullptr);
        //            return true;
        //        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev == nullptr || strcmp(dev, getDeviceName()) != 0)
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

    if (MotorRateNP.isNameMatch(name))
    {
        IUUpdateNumber(&MotorRateNP, values, names, n);
        MotorRateNP.setState(IPS_OK);
        applyMotorRates();
        IDSetNumber(&MotorRateNP, nullptr);
        return true;
    }
    else if (MotorCurrentNP->isNameMatch(name))
    {
        IUUpdateNumber(&MotorCurrentNP, values, names, n);
        MotorCurrentNP.setState(IPS_OK);
        applyMotorCurrents();
        IDSetNumber(&MotorCurrentNP, nullptr);
        return true;
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState SestoSenso2::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (isSimulation() == false)
    {
        backlashDirection = targetTicks < lastPos ? FOCUS_INWARD : FOCUS_OUTWARD;
        if(backlashDirection == FOCUS_INWARD)
        {
            targetPos -=  backlashTicks;
        }
        else
        {
            targetPos +=  backlashTicks;
        }
        if (m_SestoSenso2->goAbsolutePosition(targetPos) == false)
            return IPS_ALERT;
    }

    m_MotionProgressTimer.start(10);
    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState SestoSenso2::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (IUFindOnSwitchIndex(&FocusReverseSP) == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosN[0].value + relativeTicks;

    bool rc = MoveAbsFocuser(newPosition);

    return (rc ? IPS_BUSY : IPS_ALERT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::AbortFocuser()
{
    //    if (m_MotionProgressTimerID > 0)
    //    {
    //        IERmTimer(m_MotionProgressTimerID);
    //        m_MotionProgressTimerID = -1;
    //    }

    m_MotionProgressTimer.stop();

    if (isSimulation())
        return true;

    return m_SestoSenso2->stop();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void SestoSenso2::checkMotionProgressCallback()
{
    if (isMotionComplete())
    {
        FocusAbsPosNP.s = IPS_OK;
        FocusRelPosNP.s = IPS_OK;
        SpeedNP.setState(IPS_OK);
        SpeedNP[0].value = 0;
        IDSetNumber(&SpeedNP, nullptr);

        IDSetNumber(&FocusRelPosNP, nullptr);
        IDSetNumber(&FocusAbsPosNP, nullptr);


        lastPos = FocusAbsPosN[0].value;

        if (CalibrationSP.getState() == IPS_BUSY)
        {
            ISState states[2] = { ISS_OFF, ISS_ON };
            const char * names[2] = { CalibrationSP[CALIBRATION_START].getName(), CalibrationSP[CALIBRATION_NEXT].getName() };
            ISNewSwitch(getDeviceName(), CalibrationSP.getName(), states, const_cast<char **>(names), CalibrationSP->count());
        }
        else
            LOG_INFO("Focuser reached requested position.");
        return;
    }
    else
    {
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    SpeedNP.setState(IPS_BUSY);
    IDSetNumber(&SpeedNP, nullptr);

    lastPos = FocusAbsPosN[0].value;

    m_MotionProgressTimer.start(500);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
//void SestoSenso2::checkHallSensorCallback()
//{
//    // FIXME
//    // Function not getting call from anywhere?
//    char res[SESTO_LEN] = {0};
//    if (command->getHallSensor(res))
//    {
//        int detected = 0;
//        if (sscanf(res, "%d", &detected) == 1)
//        {
//            if (detected == 1)
//            {
//                ISState states[2] = { ISS_OFF, ISS_ON };
//                const char * names[2] = { CalibrationS[CALIBRATION_START].name, CalibrationS[CALIBRATION_NEXT].name };
//                ISNewSwitch(getDeviceName(), CalibrationSP.name, states, const_cast<char **>(names), CalibrationSP.nsp);
//                return;
//            }
//        }
//    }

//    //m_HallSensorTimerID = IEAddTimer(1000, &SestoSenso2::checkHallSensorHelper, this);
//    m_HallSensorTimer.start();
//}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void SestoSenso2::TimerHit()
{
    if (!isConnected() ||
            FocusAbsPosNP.s == IPS_BUSY
            || FocusRelPosNP.s == IPS_BUSY ||
            (m_IsSestoSenso2 && CalibrationSP.getState() == IPS_BUSY))
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 0)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    if (m_TemperatureCounter++ == SESTO_TEMPERATURE_FREQ)
    {
        rc = updateTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureNP[0].value) >= 0.1)
            {
                IDSetNumber(&TemperatureNP, nullptr);
                lastTemperature = TemperatureNP[0].value;
            }
        }

        // Also use temparature poll rate for tracking input voltage
        rc = updateVoltageIn();
        if (rc)
        {
            if (fabs(lastVoltageIn - VoltageInNP[0].value) >= 0.1)
            {
                IDSetNumber(&VoltageInNP, nullptr);
                lastVoltageIn = VoltageInNP[0].value;

                if (VoltageInNP[0].value < 11.0)
                {
                    LOG_WARN("Please check 12v DC power supply is connected.");
                }
            }
        }

        m_TemperatureCounter = 0;   // Reset the counter
    }

    SetTimer(getCurrentPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::getStartupValues()
{
    bool rc = updatePosition();
    if (rc)
    {
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    rc &= fetchMotorSettings();

    return (rc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::ReverseFocuser(bool enable)
{
    INDI_UNUSED(enable);
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::Ack()
{
    std::string response;

    if (isSimulation())
        response = "1.0 Simulation";
    else
    {
        if(initCommandSet() == false)
        {
            LOG_ERROR("Failed setting attributes on serial port and init command sets");
            return false;
        }
        if(m_SestoSenso2->getSerialNumber(response))
        {
            LOGF_INFO("Serial number: %s", response.c_str());
        }
        else
        {
            return false;
        }
    }

    m_IsSestoSenso2 = !strstr(response.c_str(), "ESATTO");
    FirmwareTP[FIRMWARE_SN].setText(response.c_str());

    if (m_SestoSenso2->getFirmwareVersion(response))
    {
        LOGF_INFO("Firmware version: %s", response.c_str());
        IUSaveText(&FirmwareTP[FIRMWARE_VERSION], response.c_str());
    }
    else
    {
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void SestoSenso2::setConnectionParams()
{
    serialConnection->setDefaultBaudRate(serialConnection->B_115200);
    serialConnection->setWordSize(8);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::initCommandSet()
{
    m_SestoSenso2.reset(new PrimalucaLabs::SestoSenso2(getDeviceName(), PortFD));

    struct termios tty_setting;
    if (tcgetattr(PortFD, &tty_setting) == -1)
    {
        LOG_ERROR("setTTYFlags: failed getting tty attributes.");
        return false;
    }
    tty_setting.c_lflag |= ICANON;
    if (tcsetattr(PortFD, TCSANOW, &tty_setting))
    {
        LOG_ERROR("setTTYFlags: failed setting attributes on serial port.");
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SestoSenso2::saveConfigItems(FILE *fp)
{
    Focuser::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &MotorRateNP);
    IUSaveConfigNumber(fp, &MotorCurrentNP);
    return true;
}

