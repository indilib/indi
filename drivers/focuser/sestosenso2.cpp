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

struct MotorRates
{
    // Rate values: 1-10
    uint32_t accRate = 0, runSpeed = 0, decRate = 0;
};

struct MotorCurrents
{
    // Current values: 1-10
    uint32_t accCurrent = 0, runCurrent = 0, decCurrent = 0;
    // Hold current: 1-5
    uint32_t holdCurrent = 0;
};

// Settings names for the default motor settings presets
const char *MOTOR_PRESET_NAMES[] = { "light", "medium", "slow" };

SestoSenso2::SestoSenso2()
{
    setVersion(0, 7);
    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);

    m_MotionProgressTimer.reset(new INDI::Timer);
    m_MotionProgressTimer->callOnTimeout(std::bind(&SestoSenso2::checkMotionProgressCallback, this));
    m_MotionProgressTimer->setSingleShot(true);

    m_HallSensorTimer.reset(new INDI::Timer);
    m_HallSensorTimer->callOnTimeout(std::bind(&SestoSenso2::checkHallSensorCallback, this));
    m_HallSensorTimer->setSingleShot(true);
}

bool SestoSenso2::initProperties()
{

    INDI::Focuser::initProperties();

    setConnectionParams();

    // Firmware information
    IUFillText(&FirmwareT[FIRMWARE_SN], "SERIALNUMBER", "Serial Number", "");
    IUFillText(&FirmwareT[FIRMWARE_VERSION], "VERSION", "Version", "");
    IUFillTextVector(&FirmwareTP, FirmwareT, 2, getDeviceName(), "FOCUS_FIRMWARE", "Firmware", CONNECTION_TAB, IP_RO, 0,
                     IPS_IDLE);

    // Voltage Information
    IUFillNumber(&VoltageInN[0], "VOLTAGEIN", "Volts", "%.2f", 0, 100, 0., 0.);
    IUFillNumberVector(&VoltageInNP, VoltageInN, 1, getDeviceName(), "VOLTAGE_IN", "Voltage in", ENVIRONMENT_TAB, IP_RO, 0,
                       IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[TEMPERATURE_MOTOR], "TEMPERATURE", "Motor (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumber(&TemperatureN[TEMPERATURE_EXTERNAL], "TEMPERATURE_ETX", "External (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 2, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", ENVIRONMENT_TAB,
                       IP_RO, 0, IPS_IDLE);

    // Current Speed
    IUFillNumber(&SpeedN[0], "SPEED", "steps/s", "%.f", 0, 7000., 1, 0);
    IUFillNumberVector(&SpeedNP, SpeedN, 1, getDeviceName(), "FOCUS_SPEED", "Motor Speed", MAIN_CONTROL_TAB, IP_RO, 0,
                       IPS_IDLE);

    // Focuser calibration
    IUFillText(&CalibrationMessageT[0], "CALIBRATION", "Calibration stage", "Press START to begin the Calibration.");
    IUFillTextVector(&CalibrationMessageTP, CalibrationMessageT, 1, getDeviceName(), "CALIBRATION_MESSAGE", "Calibration",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Calibration
    IUFillSwitch(&CalibrationS[CALIBRATION_START], "CALIBRATION_START", "Start", ISS_OFF);
    IUFillSwitch(&CalibrationS[CALIBRATION_NEXT], "CALIBRATION_NEXT", "Next", ISS_OFF);
    IUFillSwitchVector(&CalibrationSP, CalibrationS, 2, getDeviceName(), "FOCUS_CALIBRATION", "Calibration", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Speed Moves
    IUFillSwitch(&FastMoveS[FASTMOVE_IN], "FASTMOVE_IN", "Move In", ISS_OFF);
    IUFillSwitch(&FastMoveS[FASTMOVE_OUT], "FASTMOVE_OUT", "Move out", ISS_OFF);
    IUFillSwitch(&FastMoveS[FASTMOVE_STOP], "FASTMOVE_STOP", "Stop", ISS_OFF);
    IUFillSwitchVector(&FastMoveSP, FastMoveS, 3, getDeviceName(), "FAST_MOVE", "Calibration Move", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    // Hold state
    IUFillSwitch(&MotorHoldS[MOTOR_HOLD_ON], "HOLD_ON", "Hold On", ISS_OFF);
    IUFillSwitch(&MotorHoldS[MOTOR_HOLD_OFF], "HOLD_OFF", "Hold Off", ISS_OFF);
    IUFillSwitchVector(&MotorHoldSP, MotorHoldS, 2, getDeviceName(), "MOTOR_HOLD", "Motor Hold", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Override the default Max. Position to make it Read-Only
    IUFillNumberVector(&FocusMaxPosNP, FocusMaxPosN, 1, getDeviceName(), "FOCUS_MAX", "Max. Position", MAIN_CONTROL_TAB, IP_RO,
                       0, IPS_IDLE);

    // Home Position
    IUFillSwitch(&HomeS[0], "FOCUS_HOME_GO", "Go", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 1, getDeviceName(), "FOCUS_HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);

    // Motor rate
    IUFillNumber(&MotorRateN[MOTOR_RATE_ACC], "ACC", "Acceleration", "%.f", 1, 10, 1, 1);
    IUFillNumber(&MotorRateN[MOTOR_RATE_RUN], "RUN", "Run Speed", "%.f", 1, 10, 1, 2);
    IUFillNumber(&MotorRateN[MOTOR_RATE_DEC], "DEC", "Deceleration", "%.f", 1, 10, 1, 1);
    IUFillNumberVector(&MotorRateNP, MotorRateN, 3, getDeviceName(), "MOTOR_RATE", "Motor Rate", MOTOR_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Motor current
    IUFillNumber(&MotorCurrentN[MOTOR_CURR_ACC], "CURR_ACC", "Acceleration", "%.f", 1, 10, 1, 7);
    IUFillNumber(&MotorCurrentN[MOTOR_CURR_RUN], "CURR_RUN", "Run", "%.f", 1, 10, 1, 7);
    IUFillNumber(&MotorCurrentN[MOTOR_CURR_DEC], "CURR_DEC", "Deceleration", "%.f", 1, 10, 1, 7);
    IUFillNumber(&MotorCurrentN[MOTOR_CURR_HOLD], "CURR_HOLD", "Hold", "%.f", 0, 5, 1, 3);
    IUFillNumberVector(&MotorCurrentNP, MotorCurrentN, 4, getDeviceName(), "MOTOR_CURRENT", "Current", MOTOR_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Load motor preset
    IUFillSwitch(&MotorApplyPresetS[MOTOR_APPLY_LIGHT], "MOTOR_APPLY_LIGHT", "Light", ISS_OFF);
    IUFillSwitch(&MotorApplyPresetS[MOTOR_APPLY_MEDIUM], "MOTOR_APPLY_MEDIUM", "Medium", ISS_OFF);
    IUFillSwitch(&MotorApplyPresetS[MOTOR_APPLY_HEAVY], "MOTOR_APPLY_HEAVY", "Heavy", ISS_OFF);
    IUFillSwitchVector(&MotorApplyPresetSP, MotorApplyPresetS, 3, getDeviceName(), "MOTOR_APPLY_PRESET", "Apply Preset",
                       MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Load user preset
    IUFillSwitch(&MotorApplyUserPresetS[MOTOR_APPLY_USER1], "MOTOR_APPLY_USER1", "User 1", ISS_OFF);
    IUFillSwitch(&MotorApplyUserPresetS[MOTOR_APPLY_USER2], "MOTOR_APPLY_USER2", "User 2", ISS_OFF);
    IUFillSwitch(&MotorApplyUserPresetS[MOTOR_APPLY_USER3], "MOTOR_APPLY_USER3", "User 3", ISS_OFF);
    IUFillSwitchVector(&MotorApplyUserPresetSP, MotorApplyUserPresetS, 3, getDeviceName(), "MOTOR_APPLY_USER_PRESET",
                       "Apply Custom", MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Save user preset
    IUFillSwitch(&MotorSaveUserPresetS[MOTOR_SAVE_USER1], "MOTOR_SAVE_USER1", "User 1", ISS_OFF);
    IUFillSwitch(&MotorSaveUserPresetS[MOTOR_SAVE_USER2], "MOTOR_SAVE_USER2", "User 2", ISS_OFF);
    IUFillSwitch(&MotorSaveUserPresetS[MOTOR_SAVE_USER3], "MOTOR_SAVE_USER3", "User 3", ISS_OFF);
    IUFillSwitchVector(&MotorSaveUserPresetSP, MotorSaveUserPresetS, 3, getDeviceName(), "MOTOR_SAVE_USER_PRESET",
                       "Save Custom", MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

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

bool SestoSenso2::updateProperties()
{
    if (isConnected() && updateMaxLimit() == false)
        LOGF_WARN("Check you have the latest %s firmware. Focuser requires calibration.", getDeviceName());

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&SpeedNP);
        defineProperty(&CalibrationMessageTP);
        defineProperty(&CalibrationSP);
        defineProperty(&HomeSP);
        defineProperty(&MotorRateNP);
        defineProperty(&MotorCurrentNP);
        defineProperty(&MotorHoldSP);
        defineProperty(&MotorApplyPresetSP);
        defineProperty(&MotorApplyUserPresetSP);
        defineProperty(&MotorSaveUserPresetSP);

        defineProperty(&FirmwareTP);

        if (updateTemperature())
            defineProperty(&TemperatureNP);

        if (updateVoltageIn())
            defineProperty(&VoltageInNP);

        if (getStartupValues())
            LOG_INFO("Parameters updated, focuser ready for use.");
        else
            LOG_WARN("Failed to inquire parameters. Check logs.");
    }
    else
    {
        if (TemperatureNP.s == IPS_OK)
            deleteProperty(TemperatureNP.name);
        deleteProperty(FirmwareTP.name);
        deleteProperty(VoltageInNP.name);
        deleteProperty(CalibrationMessageTP.name);
        deleteProperty(CalibrationSP.name);
        deleteProperty(SpeedNP.name);
        deleteProperty(HomeSP.name);
        deleteProperty(MotorRateNP.name);
        deleteProperty(MotorCurrentNP.name);
        deleteProperty(MotorHoldSP.name);
        deleteProperty(MotorApplyPresetSP.name);
        deleteProperty(MotorApplyUserPresetSP.name);
        deleteProperty(MotorSaveUserPresetSP.name);
    }

    return true;
}

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

bool SestoSenso2::Disconnect()
{
    //    if (isSimulation() == false)
    //        command->goHome();

    return INDI::Focuser::Disconnect();
}

const char *SestoSenso2::getDefaultName()
{
    return "Sesto Senso 2";
}

bool SestoSenso2::updateTemperature()
{
    char res[SESTO_LEN] = {0};
    double temperature = 0;

    if (isSimulation())
        strncpy(res, "23.45", SESTO_LEN);
    else if (command->getMotorTemp(res) == false)
        return false;

    try
    {
        temperature = std::stod(res);
    }
    catch(...)
    {
        LOGF_WARN("Failed to process temperature response: %s (%d bytes)", res, strlen(res));
        return false;
    }

    if (temperature > 90)
        return false;

    TemperatureN[TEMPERATURE_MOTOR].value = temperature;
    TemperatureNP.s = IPS_OK;

    // External temperature - Optional
    if (command->getExternalTemp(res))
    {
        TemperatureN[TEMPERATURE_EXTERNAL].value = -273.15;
        try
        {
            temperature = std::stod(res);
        }
        catch(...)
        {
            LOGF_DEBUG("Failed to process external temperature response: %s (%d bytes)", res, strlen(res));
        }

        if (temperature < 90)
            TemperatureN[TEMPERATURE_EXTERNAL].value = temperature;
    }

    return true;
}


bool SestoSenso2::updateMaxLimit()
{
    char res[SESTO_LEN] = {0};

    if (isSimulation())
        return true;

    if (command->getMaxPosition(res) == false)
        return false;

    int maxLimit = 0;

    sscanf(res, "%d", &maxLimit);

    if (maxLimit > 0)
    {
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


    FocusMaxPosNP.s = IPS_ALERT;
    return false;
}

bool SestoSenso2::updatePosition()
{
    char res[SESTO_LEN] = {0};
    if (isSimulation())
        snprintf(res, SESTO_LEN, "%u", static_cast<uint32_t>(FocusAbsPosN[0].value));
    else if (command->getAbsolutePosition(res) == false)
        return false;

    try
    {
        FocusAbsPosN[0].value = std::stoi(res);
        FocusAbsPosNP.s = IPS_OK;
        return true;
    }
    catch(...)
    {
        LOGF_WARN("Failed to process position response: %s (%d bytes)", res, strlen(res));
        FocusAbsPosNP.s = IPS_ALERT;
        return false;
    }
}

bool SestoSenso2::updateVoltageIn()
{
    char res[SESTO_LEN] = {0};
    double voltageIn = 0;

    if (isSimulation())
        strncpy(res, "12.00", SESTO_LEN);
    else if (command->getVoltageIn(res) == false)
        return false;

    try
    {
        voltageIn = std::stod(res);
    }
    catch(...)
    {
        LOGF_WARN("Failed to process voltage response: %s (%d bytes)", res, strlen(res));
        return false;
    }

    if (voltageIn > 24)
        return false;

    VoltageInN[0].value = voltageIn;
    VoltageInNP.s = (voltageIn >= 11.0) ? IPS_OK : IPS_ALERT;

    return true;
}

bool SestoSenso2::fetchMotorSettings()
{
    // Fetch driver state and reflect in INDI
    MotorRates ms;
    MotorCurrents mc;
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
        if (!command->getMotorSettings(ms, mc, motorHoldActive))
        {
            MotorRateNP.s = IPS_IDLE;
            MotorCurrentNP.s = IPS_IDLE;
            MotorHoldSP.s = IPS_IDLE;
            return false;
        }
    }

    MotorRateN[MOTOR_RATE_ACC].value = ms.accRate;
    MotorRateN[MOTOR_RATE_RUN].value = ms.runSpeed;
    MotorRateN[MOTOR_RATE_DEC].value = ms.decRate;
    MotorRateNP.s = IPS_OK;
    IDSetNumber(&MotorRateNP, nullptr);

    MotorCurrentN[MOTOR_CURR_ACC].value = mc.accCurrent;
    MotorCurrentN[MOTOR_CURR_RUN].value = mc.runCurrent;
    MotorCurrentN[MOTOR_CURR_DEC].value = mc.decCurrent;
    MotorCurrentN[MOTOR_CURR_HOLD].value = mc.holdCurrent;
    MotorCurrentNP.s = IPS_OK;
    IDSetNumber(&MotorCurrentNP, nullptr);

    // Also update motor hold switch
    const char *activeSwitchID = motorHoldActive ? "HOLD_ON" : "HOLD_OFF";
    ISwitch *sp = IUFindSwitch(&MotorHoldSP, activeSwitchID);
    assert(sp != nullptr && "Motor hold switch not found");
    if (sp)
    {
        IUResetSwitch(&MotorHoldSP);
        sp->s = ISS_ON;
        MotorHoldSP.s = motorHoldActive ? IPS_OK : IPS_ALERT;
        IDSetSwitch(&MotorHoldSP, nullptr);
    }

    if (motorHoldActive && mc.holdCurrent == 0)
    {
        LOG_WARN("Motor hold current set to 0, motor hold setting will have no effect");
    }

    return true;
}

bool SestoSenso2::applyMotorRates()
{
    if (isSimulation())
        return true;

    // Send INDI state to driver
    MotorRates mr;
    mr.accRate = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_ACC].value);
    mr.runSpeed = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_RUN].value);
    mr.decRate = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_DEC].value);

    if (!command->setMotorRates(mr))
    {
        LOG_ERROR("Failed to apply motor rates");
        // TODO: Error state?
        return false;
    }

    LOGF_INFO("Motor rates applied: Acc: %u Run: %u Dec: %u", mr.accRate, mr.runSpeed, mr.decRate);
    return true;
}

bool SestoSenso2::applyMotorCurrents()
{
    if (isSimulation())
        return true;

    // Send INDI state to driver
    MotorCurrents mc;
    mc.accCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_ACC].value);
    mc.runCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_RUN].value);
    mc.decCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_DEC].value);
    mc.holdCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_HOLD].value);

    if (!command->setMotorCurrents(mc))
    {
        LOG_ERROR("Failed to apply motor currents");
        // TODO: Error state?
        return false;
    }

    LOGF_INFO("Motor currents applied: Acc: %u Run: %u Dec: %u Hold: %u", mc.accCurrent, mc.runCurrent, mc.decCurrent,
              mc.holdCurrent);
    return true;
}

bool SestoSenso2::isMotionComplete()
{
    char res[SESTO_LEN] = {0};

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

        snprintf(res, SESTO_LEN, "%d", nextPos);
    }
    else
    {
        if(command->getCurrentSpeed(res))
        {
            try
            {
                uint32_t newSpeed = std::stoi(res);
                SpeedN[0].value = newSpeed;
                SpeedNP.s = IPS_OK;
            }
            catch (...)
            {
                LOGF_WARN("Failed to get motor speed response: %s (%d bytes)", res, strlen(res));
            }

            if(!strcmp(res, "0"))
                return true;

            *res = {0};
            if(command->getAbsolutePosition(res))
            {
                try
                {
                    uint32_t newPos = std::stoi(res);
                    FocusAbsPosN[0].value = newPos;
                }
                catch (...)
                {
                    LOGF_WARN("Failed to process motion response: %s (%d bytes)", res, strlen(res));
                }
            }
        }

    }

    return false;
}

bool SestoSenso2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Calibrate focuser
        if (!strcmp(name, CalibrationSP.name))
        {
            int current_switch = 0;

            CalibrationSP.s = IPS_BUSY;
            //IDSetSwitch(&CalibrationSP, nullptr);
            IUUpdateSwitch(&CalibrationSP, states, names, n);

            current_switch = IUFindOnSwitchIndex(&CalibrationSP);
            CalibrationS[current_switch].s = ISS_ON;
            IDSetSwitch(&CalibrationSP, nullptr);

            if (current_switch == CALIBRATION_START)
            {
                if (cStage == Idle || cStage == Complete )
                {
                    // Start the calibration process
                    LOG_INFO("Start Calibration");
                    CalibrationSP.s = IPS_BUSY;
                    IDSetSwitch(&CalibrationSP, nullptr);

                    //
                    // Init
                    //
                    if (m_IsSestoSenso2 && command->initCalibration() == false)
                        return false;

                    IUSaveText(&CalibrationMessageT[0], "Set focus in MIN position and then press NEXT.");
                    IDSetText(&CalibrationMessageTP, nullptr);

                    // Motor hold disabled during calibration init, so fetch new hold state
                    fetchMotorSettings();

                    // Set next step
                    cStage = GoToMiddle;
                }
                else
                {
                    LOG_INFO("Already started calibration. Proceed to next step.");
                    IUSaveText(&CalibrationMessageT[0], "Already started. Proceed to NEXT.");
                    IDSetText(&CalibrationMessageTP, nullptr);
                }
            }
            else if (current_switch == CALIBRATION_NEXT)
            {
                if (cStage == GoToMiddle)
                {
                    defineProperty(&FastMoveSP);
                    if (m_IsSestoSenso2)
                    {
                        if (command->storeAsMinPosition() == false)
                            return false;

                        IUSaveText(&CalibrationMessageT[0], "Press MOVE OUT to move focuser out (CAUTION!)");
                        IDSetText(&CalibrationMessageTP, nullptr);
                        cStage = GoMinimum;
                    }
                    // For Esatto, start moving out immediately
                    else
                    {
                        cStage = GoMaximum;

                        ISState fs[1] = { ISS_ON };
                        const char *fn[1] =  { FastMoveS[FASTMOVE_OUT].name };
                        ISNewSwitch(getDeviceName(), FastMoveSP.name, fs, const_cast<char **>(fn), 1);
                    }
                }
                else if (cStage == GoMinimum)
                {
                    char res[SESTO_LEN] = {0};
                    if (m_IsSestoSenso2 && command->storeAsMaxPosition(res) == false)
                        return false;

                    IUSaveText(&CalibrationMessageT[0], "Press NEXT to finish.");
                    IDSetText(&CalibrationMessageTP, nullptr);
                    cStage = GoMaximum;
                }
                else if (cStage == GoMaximum)
                {
                    char res[SESTO_LEN] = {0};

                    if (command->getMaxPosition(res) == false)
                        return false;

                    int maxLimit = 0;
                    sscanf(res, "%d", &maxLimit);
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

                    IUSaveText(&CalibrationMessageT[0], "Calibration Completed.");
                    IDSetText(&CalibrationMessageTP, nullptr);

                    deleteProperty(FastMoveSP.name);
                    cStage = Complete;

                    LOG_INFO("Calibration completed");
                    CalibrationSP.s = IPS_OK;
                    IDSetSwitch(&CalibrationSP, nullptr);
                    CalibrationS[current_switch].s = ISS_OFF;
                    IDSetSwitch(&CalibrationSP, nullptr);

                    // Double check motor hold state after calibration
                    fetchMotorSettings();
                }
                else
                {
                    IUSaveText(&CalibrationMessageT[0], "Calibration not in progress.");
                    IDSetText(&CalibrationMessageTP, nullptr);
                }

            }
            return true;
        }
        // Fast motion
        else if (!strcmp(name, FastMoveSP.name))
        {
            IUUpdateSwitch(&FastMoveSP, states, names, n);
            int current_switch = IUFindOnSwitchIndex(&FastMoveSP);
            char res[SESTO_LEN] = {0};

            switch (current_switch)
            {
                case FASTMOVE_IN:
                    if (command->fastMoveIn(res) == false)
                    {
                        return false;
                    }
                    break;
                case FASTMOVE_OUT:
                    if (m_IsSestoSenso2)
                    {
                        if (command->goOutToFindMaxPos() == false)
                        {
                            return false;
                        }
                        IUSaveText(&CalibrationMessageT[0], "Press STOP focuser almost at MAX position.");

                        // GoOutToFindMaxPos should cause motor hold to be reactivated
                        fetchMotorSettings();
                    }
                    else
                    {
                        if (command->fastMoveOut(res))
                        {
                            IUSaveText(&CalibrationMessageT[0], "Focusing out to detect hall sensor.");

                            //                            if (m_MotionProgressTimerID > 0)
                            //                                IERmTimer(m_MotionProgressTimerID);
                            //                            m_MotionProgressTimerID = IEAddTimer(500, &SestoSenso2::checkMotionProgressHelper, this);
                            m_MotionProgressTimer->start(500);
                        }
                    }
                    IDSetText(&CalibrationMessageTP, nullptr);
                    break;
                case FASTMOVE_STOP:
                    if (command->stop() == false)
                    {
                        return false;
                    }
                    IUSaveText(&CalibrationMessageT[0], "Press NEXT to store max limit.");
                    IDSetText(&CalibrationMessageTP, nullptr);
                    break;
                default:
                    break;
            }

            FastMoveSP.s = IPS_BUSY;
            IDSetSwitch(&FastMoveSP, nullptr);
            return true;
        }
        // Homing
        else if (!strcmp(HomeSP.name, name))
        {
            char res[SESTO_LEN] = {0};
            if (command->goHome(res))
            {
                HomeS[0].s = ISS_ON;
                HomeSP.s = IPS_BUSY;

                //                if (m_MotionProgressTimerID > 0)
                //                    IERmTimer(m_MotionProgressTimerID);
                //                m_MotionProgressTimerID = IEAddTimer(100, &SestoSenso2::checkMotionProgressHelper, this);
                m_MotionProgressTimer->start(100);
            }
            else
            {
                HomeS[0].s = ISS_OFF;
                HomeSP.s = IPS_ALERT;
            }

            IDSetSwitch(&HomeSP, nullptr);
            return true;
        }
        else if (!strcmp(name, MotorHoldSP.name))
        {
            IUUpdateSwitch(&MotorHoldSP, states, names, n);
            ISwitch *sp = IUFindOnSwitch(&MotorHoldSP);
            assert(sp != nullptr);

            // NOTE: Default to HOLD_ON as a safety feature
            if (!strcmp(sp->name, "HOLD_OFF"))
            {
                command->setMotorHold(false);
                MotorHoldSP.s = IPS_ALERT;
                LOG_INFO("Motor hold OFF. You may now manually adjust the focuser. Remember to enable motor hold once done.");
            }
            else
            {
                command->setMotorHold(true);
                MotorHoldSP.s = IPS_OK;
                LOG_INFO("Motor hold ON. Do NOT attempt to manually adjust the focuser!");
                if (MotorCurrentN[MOTOR_CURR_HOLD].value < 2.0)
                {
                    LOGF_WARN("Motor hold current set to %.1f: This may be insufficent to hold focus", MotorCurrentN[MOTOR_CURR_HOLD].value);
                }
            }

            IDSetSwitch(&MotorHoldSP, nullptr);
            return true;
        }
        else if (!strcmp(name, MotorApplyPresetSP.name))
        {
            IUUpdateSwitch(&MotorApplyPresetSP, states, names, n);
            int index = IUFindOnSwitchIndex(&MotorApplyPresetSP);
            assert(index >= 0 && index < 3);

            const char* presetName = MOTOR_PRESET_NAMES[index];

            if (command->applyMotorPreset(presetName))
            {
                LOGF_INFO("Loaded motor preset: %s", presetName);
                MotorApplyPresetSP.s = IPS_IDLE;
            }
            else
            {
                LOGF_ERROR("Failed to load motor preset: %s", presetName);
                MotorApplyPresetSP.s = IPS_ALERT;
            }

            MotorApplyPresetS[index].s = ISS_OFF;
            IDSetSwitch(&MotorApplyPresetSP, nullptr);

            fetchMotorSettings();
            return true;
        }
        else if (!strcmp(name, MotorApplyUserPresetSP.name))
        {
            IUUpdateSwitch(&MotorApplyUserPresetSP, states, names, n);
            int index = IUFindOnSwitchIndex(&MotorApplyUserPresetSP);
            assert(index >= 0 && index < 3);
            uint32_t userIndex = index + 1;

            if (command->applyMotorUserPreset(userIndex))
            {
                LOGF_INFO("Loaded motor user preset: %u", userIndex);
                MotorApplyUserPresetSP.s = IPS_IDLE;
            }
            else
            {
                LOGF_ERROR("Failed to load motor user preset: %u", userIndex);
                MotorApplyUserPresetSP.s = IPS_ALERT;
            }

            MotorApplyUserPresetS[index].s = ISS_OFF;
            IDSetSwitch(&MotorApplyUserPresetSP, nullptr);

            fetchMotorSettings();
            return true;
        }
        else if (!strcmp(name, MotorSaveUserPresetSP.name))
        {
            IUUpdateSwitch(&MotorSaveUserPresetSP, states, names, n);
            int index = IUFindOnSwitchIndex(&MotorSaveUserPresetSP);
            assert(index >= 0 && index < 3);
            uint32_t userIndex = index + 1;

            MotorRates mr;
            mr.accRate = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_ACC].value);
            mr.runSpeed = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_RUN].value);
            mr.decRate = static_cast<uint32_t>(MotorRateN[MOTOR_RATE_DEC].value);

            MotorCurrents mc;
            mc.accCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_ACC].value);
            mc.runCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_RUN].value);
            mc.decCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_DEC].value);
            mc.holdCurrent = static_cast<uint32_t>(MotorCurrentN[MOTOR_CURR_HOLD].value);

            if (command->saveMotorUserPreset(userIndex, mr, mc))
            {
                LOGF_INFO("Saved motor user preset %u to firmware", userIndex);
                MotorSaveUserPresetSP.s = IPS_IDLE;
            }
            else
            {
                LOGF_ERROR("Failed to save motor user preset %u to firmware", userIndex);
                MotorSaveUserPresetSP.s = IPS_ALERT;
            }

            MotorSaveUserPresetS[index].s = ISS_OFF;
            IDSetSwitch(&MotorSaveUserPresetSP, nullptr);
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool SestoSenso2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev == nullptr || strcmp(dev, getDeviceName()) != 0)
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

    if (!strcmp(name, MotorRateNP.name))
    {
        IUUpdateNumber(&MotorRateNP, values, names, n);
        MotorRateNP.s = IPS_OK;
        applyMotorRates();
        IDSetNumber(&MotorRateNP, nullptr);
        return true;
    }
    else if (!strcmp(name, MotorCurrentNP.name))
    {
        IUUpdateNumber(&MotorCurrentNP, values, names, n);
        MotorCurrentNP.s = IPS_OK;
        applyMotorCurrents();
        IDSetNumber(&MotorCurrentNP, nullptr);
        return true;
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

IPState SestoSenso2::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (isSimulation() == false)
    {
        char res[SESTO_LEN] = {0};
        if (command->go(targetTicks, res) == false)
            return IPS_ALERT;
    }

    //    if (m_MotionProgressTimerID > 0)
    //        IERmTimer(m_MotionProgressTimerID);
    //    m_MotionProgressTimerID = IEAddTimer(10, &SestoSenso2::checkMotionProgressHelper, this);
    m_MotionProgressTimer->start(10);
    return IPS_BUSY;
}

IPState SestoSenso2::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (IUFindOnSwitchIndex(&FocusReverseSP) == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosN[0].value + relativeTicks;

    bool rc = MoveAbsFocuser(newPosition);

    return (rc ? IPS_BUSY : IPS_ALERT);
}

bool SestoSenso2::AbortFocuser()
{
    //    if (m_MotionProgressTimerID > 0)
    //    {
    //        IERmTimer(m_MotionProgressTimerID);
    //        m_MotionProgressTimerID = -1;
    //    }

    m_MotionProgressTimer->stop();

    if (isSimulation())
        return true;

    bool rc = command->abort();

    if (rc && HomeSP.s == IPS_BUSY)
    {
        HomeS[0].s = ISS_OFF;
        HomeSP.s = IPS_IDLE;
        IDSetSwitch(&HomeSP, nullptr);
    }

    return rc;
}

//void SestoSenso2::checkMotionProgressHelper(void *context)
//{
//    static_cast<SestoSenso2*>(context)->checkMotionProgressCallback();
//}

//void SestoSenso2::checkHallSensorHelper(void *context)
//{
//    static_cast<SestoSenso2*>(context)->checkHallSensorCallback();
//}

//
// This timer function is initiated when a GT command has been issued
// A timer will call this function on a regular interval during the motion
// Modified the code to exit when motion is complete
//
void SestoSenso2::checkMotionProgressCallback()
{
    if (isMotionComplete())
    {
        FocusAbsPosNP.s = IPS_OK;
        FocusRelPosNP.s = IPS_OK;
        SpeedNP.s = IPS_OK;
        SpeedN[0].value = 0;
        IDSetNumber(&SpeedNP, nullptr);

        IDSetNumber(&FocusRelPosNP, nullptr);
        IDSetNumber(&FocusAbsPosNP, nullptr);


        lastPos = FocusAbsPosN[0].value;

        if (HomeSP.s == IPS_BUSY)
        {
            LOG_INFO("Focuser at home position.");
            HomeS[0].s = ISS_OFF;
            HomeSP.s = IPS_OK;
            IDSetSwitch(&HomeSP, nullptr);
        }
        else if (CalibrationSP.s == IPS_BUSY)
        {
            ISState states[2] = { ISS_OFF, ISS_ON };
            const char * names[2] = { CalibrationS[CALIBRATION_START].name, CalibrationS[CALIBRATION_NEXT].name };
            ISNewSwitch(getDeviceName(), CalibrationSP.name, states, const_cast<char **>(names), CalibrationSP.nsp);
        }
        else
            LOG_INFO("Focuser reached requested position.");
        return;
    }
    else
    {
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    SpeedNP.s = IPS_BUSY;
    IDSetNumber(&SpeedNP, nullptr);

    lastPos = FocusAbsPosN[0].value;

    //    IERmTimer(m_MotionProgressTimerID);
    //    m_MotionProgressTimerID = IEAddTimer(500, &SestoSenso2::checkMotionProgressHelper, this);
    m_MotionProgressTimer->start(500);
}

void SestoSenso2::checkHallSensorCallback()
{
    char res[SESTO_LEN] = {0};
    if (command->getHallSensor(res))
    {
        int detected = 0;
        if (sscanf(res, "%d", &detected) == 1)
        {
            if (detected == 1)
            {
                ISState states[2] = { ISS_OFF, ISS_ON };
                const char * names[2] = { CalibrationS[CALIBRATION_START].name, CalibrationS[CALIBRATION_NEXT].name };
                ISNewSwitch(getDeviceName(), CalibrationSP.name, states, const_cast<char **>(names), CalibrationSP.nsp);
                return;
            }
        }
    }

    //m_HallSensorTimerID = IEAddTimer(1000, &SestoSenso2::checkHallSensorHelper, this);
    m_HallSensorTimer->start(1000);
}

void SestoSenso2::TimerHit()
{
    if (!isConnected() || FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY || (m_IsSestoSenso2
            && CalibrationSP.s == IPS_BUSY))
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
            if (fabs(lastTemperature - TemperatureN[0].value) >= 0.1)
            {
                IDSetNumber(&TemperatureNP, nullptr);
                lastTemperature = TemperatureN[0].value;
            }
        }

        // Also use temparature poll rate for tracking input voltage
        rc = updateVoltageIn();
        if (rc)
        {
            if (fabs(lastVoltageIn - VoltageInN[0].value) >= 0.1)
            {
                IDSetNumber(&VoltageInNP, nullptr);
                lastVoltageIn = VoltageInN[0].value;

                if (VoltageInN[0].value < 11.0)
                {
                    LOG_WARN("Please check 12v DC power supply is connected.");
                }
            }
        }

        m_TemperatureCounter = 0;   // Reset the counter
    }

    SetTimer(getCurrentPollingPeriod());
}

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


bool SestoSenso2::ReverseFocuser(bool enable)
{
    INDI_UNUSED(enable);
    return false;
}


bool SestoSenso2::Ack()
{
    char res[SESTO_LEN] = {0};

    if (isSimulation())
        strncpy(res, "1.0 Simulation", SESTO_LEN);
    else
    {
        if(initCommandSet() == false)
        {
            LOG_ERROR("Failed setting attributes on serial port and init command sets");
            return false;
        }
        if(command->getSerialNumber(res))
        {
            LOGF_INFO("Serial number: %s", res);
        }
        else
        {
            return false;
        }
    }

    m_IsSestoSenso2 = !strstr(res, "ESATTO");
    IUSaveText(&FirmwareT[FIRMWARE_SN], res);

    if (command->getFirmwareVersion(res))
    {
        LOGF_INFO("Firmware version: %s", res);
        IUSaveText(&FirmwareT[FIRMWARE_VERSION], res);
    }
    else
    {
        return false;
    }

    return true;
}


void SestoSenso2::setConnectionParams()
{
    serialConnection->setDefaultBaudRate(serialConnection->B_115200);
    serialConnection->setWordSize(8);
}


bool SestoSenso2::initCommandSet()
{
    command = new CommandSet(PortFD, getDeviceName());

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

bool SestoSenso2::saveConfigItems(FILE *fp)
{
    Focuser::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &MotorRateNP);
    IUSaveConfigNumber(fp, &MotorCurrentNP);
    return true;
}

bool CommandSet::send(const std::string &request, std::string &response) const
{
    tcflush(CommandSet::PortFD, TCIOFLUSH);
    if (write(CommandSet::PortFD, request.c_str(), request.length()) == 0)
    {
        LOGF_ERROR("Failed to send to device: %s", request.c_str());
        return false;
    }

    // NOTE: Every request should result in a response from the device
    char read_buf[SESTO_LEN] = {0};
    if (read(CommandSet::PortFD, &read_buf, sizeof(read_buf)) == 0)
    {
        LOGF_ERROR("No response from device for request: %s", request.c_str());
        return false;
    }

    LOGF_DEBUG("Received response: %s", read_buf);

    response = read_buf;
    return true;
}

bool CommandSet::sendCmd(const std::string &cmd, std::string property, char *res) const
{
    LOGF_DEBUG("Sending command: %s with property: %s", cmd.c_str(), property.c_str());
    std::string response;
    if (!send(cmd, response))
        return false;

    if (property.empty() || res == nullptr)
        return true;

    if (getValueFromResponse(response, property, res) == false)
    {
        LOGF_ERROR("Communication error: cmd %s property %s response: %s", cmd.c_str(), property.c_str(), res);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

bool CommandSet::sendCmd(const std::string &cmd, std::string property, std::string &res) const
{
    char response_buff[SESTO_LEN] = {0};
    bool success = sendCmd(cmd, property, response_buff);
    res = response_buff;
    return success;
}

inline void remove_chars_inplace(std::string &str, char ch)
{
    str.erase(std::remove(str.begin(), str.end(), ch), str.end());
}

bool CommandSet::getValueFromResponse(const std::string &response, const std::string &property, char *value) const
{
    // FIXME: This parsing code will only return the first named property,
    // if JSON layout changes, this may break things in unexpected ways.

    // Find property
    std::size_t property_pos = response.find(property);
    if (property_pos == std::string::npos)
    {
        LOGF_ERROR("Failed to find property: %s", property.c_str());
        return false;
    }

    // Skip past key name
    std::string sub = response.substr(property_pos + property.length());

    // Find end of current JSON element: , or }
    std::size_t found = sub.find(",");
    if(found != std::string::npos)
    {
        sub = sub.substr(0, found);
    }
    else
    {
        found = sub.find("}");
        sub = sub.substr(0, found);
    }

    // Strip JSON related formatting
    remove_chars_inplace(sub, '\"');
    remove_chars_inplace(sub, ',');
    remove_chars_inplace(sub, ':');
    strcpy(value, sub.c_str());

    return true;
}

bool CommandSet::parseUIntFromResponse(const std::string &response, const std::string &property, uint32_t &result) const
{
    char valueBuff[SESTO_LEN] = { 0 };
    if (!getValueFromResponse(response, property, valueBuff))
        return false;

    if (sscanf(valueBuff, "%u", &result) != 1)
    {
        LOGF_ERROR("Failed to parse integer property %s with value: %s", property.c_str(), valueBuff);
        return false;
    }

    return true;
}

bool CommandSet::getSerialNumber(char *res)
{
    return sendCmd("{\"req\":{\"get\":{\"SN\":\"\"}}}", "SN", res);
}

bool CommandSet::getFirmwareVersion(char *res)
{
    return sendCmd("{\"req\":{\"get\":{\"SWVERS\":\"\"}}}", "SWAPP", res);
}

bool CommandSet::abort()
{
    return sendCmd("{\"req\":{\"cmd\":{\"MOT1\" :{\"MOT_ABORT\":\"\"}}}}");
}

bool CommandSet::go(uint32_t targetTicks, char *res)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), "{\"req\":{\"cmd\":{\"MOT1\" :{\"GOTO\":%u}}}}", targetTicks);
    return sendCmd(cmd, "GOTO", res);
}

bool CommandSet::stop()
{
    return sendCmd("{\"req\":{\"cmd\":{\"MOT1\" :{\"MOT_STOP\":\"\"}}}}");
}

bool CommandSet::goHome(char *res)
{
    return sendCmd("{\"req\":{\"cmd\":{\"MOT1\" :{\"GOHOME\":\"\"}}}}", "GOHOME", res);
}

bool CommandSet::fastMoveOut(char *res)
{
    return sendCmd("{\"req\":{\"cmd\":{\"MOT1\" :{\"F_OUTW\":\"\"}}}}", "F_OUTW", res);
}

bool CommandSet::fastMoveIn(char *res)
{
    return sendCmd("{\"req\":{\"cmd\":{\"MOT1\" :{\"F_INW\":\"\"}}}}", "F_INW", res);
}

bool CommandSet::getMaxPosition(char *res)
{
    return sendCmd("{\"req\":{\"get\":{\"MOT1\":\"\"}}}", "CAL_MAXPOS", res);
}

bool CommandSet::getHallSensor(char *res)
{
    return sendCmd("{\"req\":{\"get\":{\"MOT1\":\"\"}}}", "HSENDET", res);
}

bool CommandSet::storeAsMaxPosition(char *res)
{
    return sendCmd("{\"req\":{\"cmd\": {\"MOT1\": {\"CAL_FOCUSER\": \"StoreAsMaxPos\"}}}}", res);
}

bool CommandSet::goOutToFindMaxPos()
{
    return sendCmd("{\"req\":{\"cmd\": {\"MOT1\": {\"CAL_FOCUSER\": \"GoOutToFindMaxPos\"}}}}");
}

bool CommandSet::storeAsMinPosition()
{
    return sendCmd("{\"req\":{\"cmd\": {\"MOT1\": {\"CAL_FOCUSER\": \"StoreAsMinPos\"}}}}");
}

bool CommandSet::initCalibration()
{
    return sendCmd("{\"req\":{\"cmd\": {\"MOT1\": {\"CAL_FOCUSER\": \"Init\"}}}}");
}

bool CommandSet::getAbsolutePosition(char *res)
{
    return sendCmd("{\"req\":{\"get\":{\"MOT1\":\"\"}}}", "ABS_POS", res);
}

bool CommandSet::getCurrentSpeed(char *res)
{
    return sendCmd("{\"req\":{\"get\":{\"MOT1\":\"\"}}}", "SPEED", res);
}

bool CommandSet::applyMotorPreset(const char *name)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), "{\"req\":{\"cmd\":{\"RUNPRESET\":\"%s\"}}}", name);

    std::string result;
    if (!sendCmd(cmd, "RUNPRESET", result))
        return false;

    if (result == "done")
        return true;

    LOGF_ERROR("Req RUNPRESET %s returned: %s", name, result.c_str());
    return false;
}

bool CommandSet::applyMotorUserPreset(uint32_t index)
{
    // WORKAROUND: Due to a bug in the Sesto Senso 2 FW, the RUNPRESET
    // command fails when applied to user presets. Therefore here we
    // fetch the motor preset and then apply it ourselves.
    char request[SESTO_LEN] = {0};
    snprintf(request, sizeof(request), "{\"req\":{\"get\":{\"RUNPRESET_%u\":\"\"}}}}", index);

    std::string response;
    if (!send(request, response))
        return false;   // send() call handles failure logging

    MotorRates mr;
    MotorCurrents mc;
    if (parseUIntFromResponse(response, "M1ACC", mr.accRate)
            && parseUIntFromResponse(response, "M1SPD", mr.runSpeed)
            && parseUIntFromResponse(response, "M1DEC", mr.decRate)
            && parseUIntFromResponse(response, "M1CACC", mc.accCurrent)
            && parseUIntFromResponse(response, "M1CSPD", mc.runCurrent)
            && parseUIntFromResponse(response, "M1CDEC", mc.decCurrent)
            && parseUIntFromResponse(response, "M1HOLD", mc.holdCurrent))
    {
        return setMotorRates(mr) && setMotorCurrents(mc);
    }

    // parseUIntFromResponse() should log failure
    return false;

    /* TODO: Replace above code with this once RUNPRESET is verified as fixed:
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), "{\"req\":{\"cmd\":{\"RUNPRESET\":%u}}}", index);

    std::string result;
    if (!sendCmd(cmd, "RUNPRESET", result))
        return false;

    if (result == "done")
        return true;

    LOGF_ERROR("Req RUNPRESET %u returned: %s cmd:\n%s", index, result.c_str(), cmd);
    return false;
    */
}

constexpr char MOTOR_SAVE_PRESET_CMD[] =
    "{\"req\":{\"set\":{\"RUNPRESET_%u\":{"
    "\"RP_NAME\":\"User%u\","
    "\"M1ACC\":%u,\"M1DEC\":%u,\"M1SPD\":%u,"
    "\"M1CACC\":%u,\"M1CDEC\":%u,\"M1CSPD\":%u,\"M1HOLD\":%u"
    "}}}}";

bool CommandSet::saveMotorUserPreset(uint32_t index, MotorRates &mr, MotorCurrents &mc)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), MOTOR_SAVE_PRESET_CMD, index,
             index,
             mr.accRate, mr.decRate, mr.runSpeed,
             mc.accCurrent, mc.decCurrent, mc.runCurrent, mc.holdCurrent);

    std::string result;
    if (!sendCmd(cmd, "M1ACC", result))
        return false;

    // TODO: Check each parameter's result
    if (result == "done")
        return true;

    LOGF_ERROR("Set RUNPRESET %u returned: %s", index, result.c_str());
    return false;
}

bool CommandSet::getMotorTemp(char *res)
{
    return sendCmd("{\"req\":{\"get\":{\"MOT1\":\"\"}}}", "NTC_T", res);
}

bool CommandSet::getExternalTemp(char *res)
{
    return sendCmd("{\"req\":{\"get\":{\"EXT_T\":\"\"}}}", "EXT_T", res);
}

bool CommandSet::getVoltageIn(char *res)
{
    return sendCmd("{\"req\":{\"get\":{\"VIN_12V\":\"\"}}}", "VIN_12V", res);
}

bool CommandSet::getMotorSettings(struct MotorRates &mr, struct MotorCurrents &mc, bool &motorHoldActive)
{
    std::string response;
    if (!send("{\"req\":{\"get\":{\"MOT1\":\"\"}}}", response))
        return false;   // send() call handles failure logging

    uint32_t holdStatus = 0;
    if (parseUIntFromResponse(response, "FnRUN_ACC", mr.accRate)
            && parseUIntFromResponse(response, "FnRUN_SPD", mr.runSpeed)
            && parseUIntFromResponse(response, "FnRUN_DEC", mr.decRate)
            && parseUIntFromResponse(response, "FnRUN_CURR_ACC", mc.accCurrent)
            && parseUIntFromResponse(response, "FnRUN_CURR_SPD", mc.runCurrent)
            && parseUIntFromResponse(response, "FnRUN_CURR_DEC", mc.decCurrent)
            && parseUIntFromResponse(response, "FnRUN_CURR_HOLD", mc.holdCurrent)
            && parseUIntFromResponse(response, "HOLDCURR_STATUS", holdStatus))
    {
        motorHoldActive = holdStatus != 0;
        return true;
    }

    // parseUIntFromResponse() should log failure
    return false;
}

constexpr char MOTOR_RATES_CMD[] =
    "{\"req\":{\"set\":{\"MOT1\":{"
    "\"FnRUN_ACC\":%u,"
    "\"FnRUN_SPD\":%u,"
    "\"FnRUN_DEC\":%u"
    "}}}}";

bool CommandSet::setMotorRates(struct MotorRates &mr)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), MOTOR_RATES_CMD, mr.accRate, mr.runSpeed, mr.decRate);

    std::string response;
    return send(cmd, response); // TODO: Check response!
}

constexpr char MOTOR_CURRENTS_CMD[] =
    "{\"req\":{\"set\":{\"MOT1\":{"
    "\"FnRUN_CURR_ACC\":%u,"
    "\"FnRUN_CURR_SPD\":%u,"
    "\"FnRUN_CURR_DEC\":%u,"
    "\"FnRUN_CURR_HOLD\":%u"
    "}}}}";

bool CommandSet::setMotorCurrents(struct MotorCurrents &mc)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), MOTOR_CURRENTS_CMD, mc.accCurrent, mc.runCurrent, mc.decCurrent, mc.holdCurrent);

    std::string response;
    return send(cmd, response); // TODO: Check response!
}

bool CommandSet::setMotorHold(bool hold)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd, sizeof(cmd), "{\"req\":{\"set\":{\"MOT1\":{\"HOLDCURR_STATUS\":%u}}}}", hold ? 1 : 0);

    std::string response;
    return send(cmd, response); // TODO: Check response!
}
