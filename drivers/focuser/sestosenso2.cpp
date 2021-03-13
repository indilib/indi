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

void ISGetProperties(const char *dev)
{
    sesto->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    sesto->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    sesto->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    sesto->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
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

void ISSnoopDevice(XMLEle *root)
{
    sesto->ISSnoopDevice(root);
}

SestoSenso2::SestoSenso2()
{
    setVersion(0, 7);
    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);

}

bool SestoSenso2::initProperties()
{

    INDI::Focuser::initProperties();

    setConnectionParams();

    // Firmware information
    FirmwareTP[FIRMWARE_SN].fill("SERIALNUMBER", "Serial Number", "");
    FirmwareTP[FIRMWARE_VERSION].fill("VERSION", "Version", "");
    FirmwareTP.fill(getDeviceName(), "FOCUS_FIRMWARE", "Firmware", CONNECTION_TAB, IP_RO, 0,
                     IPS_IDLE);

    // Voltage Information
    VoltageInNP[0].fill("VOLTAGEIN", "Volts", "%.2f", 0, 100, 0., 0.);
    VoltageInNP.fill(getDeviceName(), "VOLTAGE_IN", "Voltage in", ENVIRONMENT_TAB, IP_RO, 0,
                       IPS_IDLE);

    // Focuser temperature
    TemperatureNP[TEMPERATURE_MOTOR].fill("TEMPERATURE", "Motor (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP[TEMPERATURE_EXTERNAL].fill("TEMPERATURE_ETX", "External (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", ENVIRONMENT_TAB,
                       IP_RO, 0, IPS_IDLE);

    // Current Speed
    SpeedNP[0].fill("SPEED", "steps/s", "%.f", 0, 7000., 1, 0);
    SpeedNP.fill(getDeviceName(), "FOCUS_SPEED", "Motor Speed", MAIN_CONTROL_TAB, IP_RO, 0,
                       IPS_IDLE);

    // Focuser calibration
    CalibrationMessageTP[0].fill("CALIBRATION", "Calibration stage", "Press START to begin the Calibration.");
    CalibrationMessageTP.fill(getDeviceName(), "CALIBRATION_MESSAGE", "Calibration",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Calibration
    CalibrationSP[CALIBRATION_START].fill("CALIBRATION_START", "Start", ISS_OFF);
    CalibrationSP[CALIBRATION_NEXT].fill("CALIBRATION_NEXT", "Next", ISS_OFF);
    CalibrationSP.fill(getDeviceName(), "FOCUS_CALIBRATION", "Calibration", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Speed Moves
    FastMoveSP[FASTMOVE_IN].fill("FASTMOVE_IN", "Move In", ISS_OFF);
    FastMoveSP[FASTMOVE_OUT].fill("FASTMOVE_OUT", "Move out", ISS_OFF);
    FastMoveSP[FASTMOVE_STOP].fill("FASTMOVE_STOP", "Stop", ISS_OFF);
    FastMoveSP.fill(getDeviceName(), "FAST_MOVE", "Calibration Move", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    // Hold state
    MotorHoldSP[MOTOR_HOLD_ON].fill("HOLD_ON", "Hold On", ISS_OFF);
    MotorHoldSP[MOTOR_HOLD_OFF].fill("HOLD_OFF", "Hold Off", ISS_OFF);
    MotorHoldSP.fill(getDeviceName(), "MOTOR_HOLD", "Motor Hold", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Override the default Max. Position to make it Read-Only
    FocusMaxPosNP.fill(getDeviceName(), "FOCUS_MAX", "Max. Position", MAIN_CONTROL_TAB, IP_RO,
                       0, IPS_IDLE);

    // Home Position
    HomeSP[0].fill("FOCUS_HOME_GO", "Go", ISS_OFF);
    HomeSP.fill(getDeviceName(), "FOCUS_HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);

    // Motor rate
    MotorRateNP[MOTOR_RATE_ACC].fill("ACC", "Acceleration", "%.f", 1, 10, 1, 1);
    MotorRateNP[MOTOR_RATE_RUN].fill("RUN", "Run Speed", "%.f", 1, 10, 1, 2);
    MotorRateNP[MOTOR_RATE_DEC].fill("DEC", "Deceleration", "%.f", 1, 10, 1, 1);
    MotorRateNP.fill(getDeviceName(), "MOTOR_RATE", "Motor Rate", MOTOR_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Motor current
    MotorCurrentNP[MOTOR_CURR_ACC].fill("CURR_ACC", "Acceleration", "%.f", 1, 10, 1, 7);
    MotorCurrentNP[MOTOR_CURR_RUN].fill("CURR_RUN", "Run", "%.f", 1, 10, 1, 7);
    MotorCurrentNP[MOTOR_CURR_DEC].fill("CURR_DEC", "Deceleration", "%.f", 1, 10, 1, 7);
    MotorCurrentNP[MOTOR_CURR_HOLD].fill("CURR_HOLD", "Hold", "%.f", 0, 5, 1, 3);
    MotorCurrentNP.fill(getDeviceName(), "MOTOR_CURRENT", "Current", MOTOR_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Load motor preset
    MotorApplyPresetSP[MOTOR_APPLY_LIGHT].fill("MOTOR_APPLY_LIGHT", "Light", ISS_OFF);
    MotorApplyPresetSP[MOTOR_APPLY_MEDIUM].fill("MOTOR_APPLY_MEDIUM", "Medium", ISS_OFF);
    MotorApplyPresetSP[MOTOR_APPLY_HEAVY].fill("MOTOR_APPLY_HEAVY", "Heavy", ISS_OFF);
    MotorApplyPresetSP.fill(getDeviceName(), "MOTOR_APPLY_PRESET", "Apply Preset",
                       MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Load user preset
    MotorApplyUserPresetSP[MOTOR_APPLY_USER1].fill("MOTOR_APPLY_USER1", "User 1", ISS_OFF);
    MotorApplyUserPresetSP[MOTOR_APPLY_USER2].fill("MOTOR_APPLY_USER2", "User 2", ISS_OFF);
    MotorApplyUserPresetSP[MOTOR_APPLY_USER3].fill("MOTOR_APPLY_USER3", "User 3", ISS_OFF);
    MotorApplyUserPresetSP.fill(getDeviceName(), "MOTOR_APPLY_USER_PRESET",
                       "Apply Custom", MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Save user preset
    MotorSaveUserPresetSP[MOTOR_SAVE_USER1].fill("MOTOR_SAVE_USER1", "User 1", ISS_OFF);
    MotorSaveUserPresetSP[MOTOR_SAVE_USER2].fill("MOTOR_SAVE_USER2", "User 2", ISS_OFF);
    MotorSaveUserPresetSP[MOTOR_SAVE_USER3].fill("MOTOR_SAVE_USER3", "User 3", ISS_OFF);
    MotorSaveUserPresetSP.fill(getDeviceName(), "MOTOR_SAVE_USER_PRESET",
                       "Save Custom", MOTOR_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(50000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1000);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(200000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);

    FocusMaxPosNP[0].setValue(2097152);
    PresetNP[0].setMax(FocusMaxPosNP[0].value);
    PresetNP[1].setMax(FocusMaxPosNP[0].value);
    PresetNP[2].setMax(FocusMaxPosNP[0].value);

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
        defineProperty(SpeedNP);
        defineProperty(CalibrationMessageTP);
        defineProperty(CalibrationSP);
        defineProperty(HomeSP);
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
        if (TemperatureNP.getState() == IPS_OK)
            deleteProperty(TemperatureNP.getName());
        deleteProperty(FirmwareTP.getName());
        deleteProperty(VoltageInNP.getName());
        deleteProperty(CalibrationMessageTP.getName());
        deleteProperty(CalibrationSP.getName());
        deleteProperty(SpeedNP.getName());
        deleteProperty(HomeSP.getName());
        deleteProperty(MotorRateNP.getName());
        deleteProperty(MotorCurrentNP.getName());
        deleteProperty(MotorHoldSP.getName());
        deleteProperty(MotorApplyPresetSP.getName());
        deleteProperty(MotorApplyUserPresetSP.getName());
        deleteProperty(MotorSaveUserPresetSP.getName());
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

    TemperatureNP[TEMPERATURE_MOTOR].setValue(temperature);
    TemperatureNP.setState(IPS_OK);

    // External temperature - Optional
    if (command->getExternalTemp(res))
    {
        TemperatureNP[TEMPERATURE_EXTERNAL].setValue(-273.15);
        try
        {
            temperature = std::stod(res);
        }
        catch(...)
        {
            LOGF_DEBUG("Failed to process external temperature response: %s (%d bytes)", res, strlen(res));
        }

        if (temperature < 90)
            TemperatureNP[TEMPERATURE_EXTERNAL].setValue(temperature);
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
        FocusMaxPosNP[0].setMax(maxLimit);
        if (FocusMaxPosNP[0].value > maxLimit)
            FocusMaxPosNP[0].setValue(maxLimit);

        FocusAbsPosNP[0].setMin(0);
        FocusAbsPosNP[0].setMax(maxLimit);
        FocusAbsPosNP[0].setValue(0);
        FocusAbsPosNP[0].setStep((FocusAbsPosNP[0].max - FocusAbsPosNP[0].min) / 50.0);

        FocusRelPosNP[0].setMin(0.);
        FocusRelPosNP[0].setMax(FocusAbsPosNP[0].step * 10);
        FocusRelPosNP[0].setValue(0);
        FocusRelPosNP[0].setStep(FocusAbsPosNP[0].step);

        PresetNP[0].setMax(maxLimit);
        PresetNP[0].setStep((FocusAbsPosNP[0].max - FocusAbsPosNP[0].min) / 50.0);
        PresetNP[1].setMax(maxLimit);
        PresetNP[1].setStep((FocusAbsPosNP[0].max - FocusAbsPosNP[0].min) / 50.0);
        PresetNP[2].setMax(maxLimit);
        PresetNP[2].setStep((FocusAbsPosNP[0].max - FocusAbsPosNP[0].min) / 50.0);


        FocusMaxPosNP.setState(IPS_OK);
        return true;
    }


    FocusMaxPosNP.setState(IPS_ALERT);
    return false;
}

bool SestoSenso2::updatePosition()
{
    char res[SESTO_LEN] = {0};
    if (isSimulation())
        snprintf(res, SESTO_LEN, "%u", static_cast<uint32_t>(FocusAbsPosNP[0].value));
    else if (command->getAbsolutePosition(res) == false)
        return false;

    try
    {
        FocusAbsPosNP[0].setValue(std::stoi(res));
        FocusAbsPosNP.setState(IPS_OK);
        return true;
    }
    catch(...)
    {
        LOGF_WARN("Failed to process position response: %s (%d bytes)", res, strlen(res));
        FocusAbsPosNP.setState(IPS_ALERT);
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

    VoltageInNP[0].setValue(voltageIn);
    VoltageInNP.setState((voltageIn >= 11.0) ? IPS_OK : IPS_ALERT);

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
    MotorRateNP.apply();

    MotorCurrentNP[MOTOR_CURR_ACC].setValue(mc.accCurrent);
    MotorCurrentNP[MOTOR_CURR_RUN].setValue(mc.runCurrent);
    MotorCurrentNP[MOTOR_CURR_DEC].setValue(mc.decCurrent);
    MotorCurrentNP[MOTOR_CURR_HOLD].setValue(mc.holdCurrent);
    MotorCurrentNP.setState(IPS_OK);
    MotorCurrentNP.apply();

    // Also update motor hold switch
    const char *activeSwitchID = motorHoldActive ? "HOLD_ON" : "HOLD_OFF";
    ISwitch *sp = MotorHoldSP.findWidgetByName(activeSwitchID);
    assert(sp != nullptr && "Motor hold switch not found");
    if (sp)
    {
        MotorHoldSP.reset();
        sp->s = ISS_ON;
        MotorHoldSP.setState(motorHoldActive ? IPS_OK : IPS_ALERT);
        MotorHoldSP.apply();
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
    mr.accRate = static_cast<uint32_t>(MotorRateNP[MOTOR_RATE_ACC].value);
    mr.runSpeed = static_cast<uint32_t>(MotorRateNP[MOTOR_RATE_RUN].value);
    mr.decRate = static_cast<uint32_t>(MotorRateNP[MOTOR_RATE_DEC].value);

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
    mc.accCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_ACC].value);
    mc.runCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_RUN].value);
    mc.decCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_DEC].value);
    mc.holdCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_HOLD].value);

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
        int32_t nextPos = FocusAbsPosNP[0].getValue();
        int32_t targPos = static_cast<int32_t>(targetPos);

        if (targPos > nextPos)
            nextPos += 250;
        else if (targPos < nextPos)
            nextPos -= 250;

        if (abs(nextPos - targPos) < 250)
            nextPos = targetPos;
        else if (nextPos < 0)
            nextPos = 0;
        else if (nextPos > FocusAbsPosNP[0].max)
            nextPos = FocusAbsPosNP[0].getMax();

        snprintf(res, SESTO_LEN, "%d", nextPos);
    }
    else
    {
        if(command->getCurrentSpeed(res))
        {
            try
            {
                uint32_t newSpeed = std::stoi(res);
                SpeedNP[0].setValue(newSpeed);
                SpeedNP.setState(IPS_OK);
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
                    FocusAbsPosNP[0].setValue(newPos);
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
        if (CalibrationSP.isNameMatch(name))
        {
            int current_switch = 0;

            CalibrationSP.setState(IPS_BUSY);
            //CalibrationSP.apply();
            CalibrationSP.update(states, names, n);

            current_switch = CalibrationSP.findOnSwitchIndex();
            CalibrationSP[current_switch].setState(ISS_ON);
            CalibrationSP.apply();

            if (current_switch == CALIBRATION_START)
            {
                if (cStage == Idle || cStage == Complete )
                {
                    // Start the calibration process
                    LOG_INFO("Start Calibration");
                    CalibrationSP.setState(IPS_BUSY);
                    CalibrationSP.apply();

                    //
                    // Init
                    //
                    if (m_IsSestoSenso2 && command->initCalibration() == false)
                        return false;

                    CalibrationMessageTP[0].setText("Set focus in MIN position and then press NEXT.");
                    CalibrationMessageTP.apply();

                    // Motor hold disabled during calibration init, so fetch new hold state
                    fetchMotorSettings();

                    // Set next step
                    cStage = GoToMiddle;
                }
                else
                {
                    LOG_INFO("Already started calibration. Proceed to next step.");
                    CalibrationMessageTP[0].setText("Already started. Proceed to NEXT.");
                    CalibrationMessageTP.apply();
                }
            }
            else if (current_switch == CALIBRATION_NEXT)
            {
                if (cStage == GoToMiddle)
                {
                    defineProperty(FastMoveSP);
                    if (m_IsSestoSenso2)
                    {
                        if (command->storeAsMinPosition() == false)
                            return false;

                        CalibrationMessageTP[0].setText("Press MOVE OUT to move focuser out (CAUTION!)");
                        CalibrationMessageTP.apply();
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
                    char res[SESTO_LEN] = {0};
                    if (m_IsSestoSenso2 && command->storeAsMaxPosition(res) == false)
                        return false;

                    CalibrationMessageTP[0].setText("Press NEXT to finish.");
                    CalibrationMessageTP.apply();
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

                    FocusMaxPosNP[0].setMax(maxLimit);
                    FocusMaxPosNP[0].setValue(maxLimit);

                    FocusAbsPosNP[0].setMin(0);
                    FocusAbsPosNP[0].setMax(maxLimit);
                    FocusAbsPosNP[0].setValue(maxLimit);
                    FocusAbsPosNP[0].setStep((FocusAbsPosNP[0].max - FocusAbsPosNP[0].min) / 50.0);

                    FocusRelPosNP[0].setMin(0.);
                    FocusRelPosNP[0].setMax(FocusAbsPosNP[0].step * 10);
                    FocusRelPosNP[0].setValue(0);
                    FocusRelPosNP[0].setStep(FocusAbsPosNP[0].step);

                    PresetNP[0].setMax(maxLimit);
                    PresetNP[0].setStep((FocusAbsPosNP[0].max - FocusAbsPosNP[0].min) / 50.0);
                    PresetNP[1].setMax(maxLimit);
                    PresetNP[1].setStep((FocusAbsPosNP[0].max - FocusAbsPosNP[0].min) / 50.0);
                    PresetNP[2].setMax(maxLimit);
                    PresetNP[2].setStep((FocusAbsPosNP[0].max - FocusAbsPosNP[0].min) / 50.0);

                    FocusMaxPosNP.setState(IPS_OK);
                    FocusAbsPosNP.updateMinMax();
                    FocusRelPosNP.updateMinMax();
                    PresetNP.updateMinMax();
                    FocusMaxPosNP.updateMinMax();

                    CalibrationMessageTP[0].setText("Calibration Completed.");
                    CalibrationMessageTP.apply();

                    deleteProperty(FastMoveSP.getName());
                    cStage = Complete;

                    LOG_INFO("Calibration completed");
                    CalibrationSP.setState(IPS_OK);
                    CalibrationSP.apply();
                    CalibrationSP[current_switch].setState(ISS_OFF);
                    CalibrationSP.apply();

                    // Double check motor hold state after calibration
                    fetchMotorSettings();
                }
                else
                {
                    CalibrationMessageTP[0].setText("Calibration not in progress.");
                    CalibrationMessageTP.apply();
                }

            }
            return true;
        }
        // Fast motion
        else if (FastMoveSP.isNameMatch(name))
        {
            FastMoveSP.update(states, names, n);
            int current_switch = FastMoveSP.findOnSwitchIndex();
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
                        CalibrationMessageTP[0].setText("Press STOP focuser almost at MAX position.");

                        // GoOutToFindMaxPos should cause motor hold to be reactivated
                        fetchMotorSettings();
                    }
                    else
                    {
                        if (command->fastMoveOut(res))
                        {
                            CalibrationMessageTP[0].setText("Focusing out to detect hall sensor.");

                            if (m_MotionProgressTimerID > 0)
                                IERmTimer(m_MotionProgressTimerID);
                            m_MotionProgressTimerID = IEAddTimer(500, &SestoSenso2::checkMotionProgressHelper, this);
                        }
                    }
                    CalibrationMessageTP.apply();
                    break;
                case FASTMOVE_STOP:
                    if (command->stop() == false)
                    {
                        return false;
                    }
                    CalibrationMessageTP[0].setText("Press NEXT to store max limit.");
                    CalibrationMessageTP.apply();
                    break;
                default:
                    break;
            }

            FastMoveSP.setState(IPS_BUSY);
            FastMoveSP.apply();
            return true;
        }
        // Homing
        else if (HomeSP.isNameMatch(name))
        {
            char res[SESTO_LEN] = {0};
            if (command->goHome(res))
            {
                HomeSP[0].setState(ISS_ON);
                HomeSP.setState(IPS_BUSY);

                if (m_MotionProgressTimerID > 0)
                    IERmTimer(m_MotionProgressTimerID);
                m_MotionProgressTimerID = IEAddTimer(100, &SestoSenso2::checkMotionProgressHelper, this);
            }
            else
            {
                HomeSP[0].setState(ISS_OFF);
                HomeSP.setState(IPS_ALERT);
            }

            HomeSP.apply();
            return true;
        }
        else if (MotorHoldSP.isNameMatch(name))
        {
            MotorHoldSP.update(states, names, n);
            ISwitch *sp = MotorHoldSP.findOnSwitch();
            assert(sp != nullptr);

            // NOTE: Default to HOLD_ON as a safety feature
            if (!strcmp(sp->name, "HOLD_OFF"))
            {
                command->setMotorHold(false);
                MotorHoldSP.setState(IPS_ALERT);
                LOG_INFO("Motor hold OFF. You may now manually adjust the focuser. Remember to enable motor hold once done.");
            }
            else
            {
                command->setMotorHold(true);
                MotorHoldSP.setState(IPS_OK);
                LOG_INFO("Motor hold ON. Do NOT attempt to manually adjust the focuser!");
                if (MotorCurrentNP[MOTOR_CURR_HOLD].value < 2.0)
                {
                    LOGF_WARN("Motor hold current set to %.1f: This may be insufficent to hold focus", MotorCurrentNP[MOTOR_CURR_HOLD].getValue());
                }
            }

            MotorHoldSP.apply();
            return true;
        }
        else if (MotorApplyPresetSP.isNameMatch(name))
        {
            MotorApplyPresetSP.update(states, names, n);
            int index = MotorApplyPresetSP.findOnSwitchIndex();
            assert(index >= 0 && index < 3);

            const char* presetName = MOTOR_PRESET_NAMES[index];

            if (command->applyMotorPreset(presetName))
            {
                LOGF_INFO("Loaded motor preset: %s", presetName);
                MotorApplyPresetSP.setState(IPS_IDLE);
            }
            else
            {
                LOGF_ERROR("Failed to load motor preset: %s", presetName);
                MotorApplyPresetSP.setState(IPS_ALERT);
            }

            MotorApplyPresetSP[index].setState(ISS_OFF);
            MotorApplyPresetSP.apply();

            fetchMotorSettings();
            return true;
        }
        else if (MotorApplyUserPresetSP.isNameMatch(name))
        {
            MotorApplyUserPresetSP.update(states, names, n);
            int index = MotorApplyUserPresetSP.findOnSwitchIndex();
            assert(index >= 0 && index < 3);
            uint32_t userIndex = index + 1;

            if (command->applyMotorUserPreset(userIndex))
            {
                LOGF_INFO("Loaded motor user preset: %u", userIndex);
                MotorApplyUserPresetSP.setState(IPS_IDLE);
            }
            else
            {
                LOGF_ERROR("Failed to load motor user preset: %u", userIndex);
                MotorApplyUserPresetSP.setState(IPS_ALERT);
            }

            MotorApplyUserPresetSP[index].setState(ISS_OFF);
            MotorApplyUserPresetSP.apply();

            fetchMotorSettings();
            return true;
        }
        else if (MotorSaveUserPresetSP.isNameMatch(name))
        {
            MotorSaveUserPresetSP.update(states, names, n);
            int index = MotorSaveUserPresetSP.findOnSwitchIndex();
            assert(index >= 0 && index < 3);
            uint32_t userIndex = index + 1;

            MotorRates mr;
            mr.accRate = static_cast<uint32_t>(MotorRateNP[MOTOR_RATE_ACC].value);
            mr.runSpeed = static_cast<uint32_t>(MotorRateNP[MOTOR_RATE_RUN].value);
            mr.decRate = static_cast<uint32_t>(MotorRateNP[MOTOR_RATE_DEC].value);

            MotorCurrents mc;
            mc.accCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_ACC].value);
            mc.runCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_RUN].value);
            mc.decCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_DEC].value);
            mc.holdCurrent = static_cast<uint32_t>(MotorCurrentNP[MOTOR_CURR_HOLD].value);

            if (command->saveMotorUserPreset(userIndex, mr, mc))
            {
                LOGF_INFO("Saved motor user preset %u to firmware", userIndex);
                MotorSaveUserPresetSP.setState(IPS_IDLE);
            }
            else
            {
                LOGF_ERROR("Failed to save motor user preset %u to firmware", userIndex);
                MotorSaveUserPresetSP.setState(IPS_ALERT);
            }

            MotorSaveUserPresetSP[index].setState(ISS_OFF);
            MotorSaveUserPresetSP.apply();
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool SestoSenso2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev == nullptr || strcmp(dev, getDeviceName()) != 0)
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

    if (MotorRateNP.isNameMatch(name))
    {
        MotorRateNP.update(values, names, n);
        MotorRateNP.setState(IPS_OK);
        applyMotorRates();
        MotorRateNP.apply();
        return true;
    }
    else if (MotorCurrentNP.isNameMatch(name))
    {
        MotorCurrentNP.update(values, names, n);
        MotorCurrentNP.setState(IPS_OK);
        applyMotorCurrents();
        MotorCurrentNP.apply();
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

    if (m_MotionProgressTimerID > 0)
        IERmTimer(m_MotionProgressTimerID);
    m_MotionProgressTimerID = IEAddTimer(10, &SestoSenso2::checkMotionProgressHelper, this);
    return IPS_BUSY;
}

IPState SestoSenso2::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (FocusReverseSP.findOnSwitchIndex() == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosNP[0].getValue() + relativeTicks;

    bool rc = MoveAbsFocuser(newPosition);

    return (rc ? IPS_BUSY : IPS_ALERT);
}

bool SestoSenso2::AbortFocuser()
{
    if (m_MotionProgressTimerID > 0)
    {
        IERmTimer(m_MotionProgressTimerID);
        m_MotionProgressTimerID = -1;
    }

    if (isSimulation())
        return true;

    bool rc = command->abort();

    if (rc && HomeSP.getState() == IPS_BUSY)
    {
        HomeSP[0].setState(ISS_OFF);
        HomeSP.setState(IPS_IDLE);
        HomeSP.apply();
    }

    return rc;
}

void SestoSenso2::checkMotionProgressHelper(void *context)
{
    static_cast<SestoSenso2*>(context)->checkMotionProgressCallback();
}

void SestoSenso2::checkHallSensorHelper(void *context)
{
    static_cast<SestoSenso2*>(context)->checkHallSensorCallback();
}

//
// This timer function is initiated when a GT command has been issued
// A timer will call this function on a regular interval during the motion
// Modified the code to exit when motion is complete
//
void SestoSenso2::checkMotionProgressCallback()
{
    if (isMotionComplete())
    {
        FocusAbsPosNP.setState(IPS_OK);
        FocusRelPosNP.setState(IPS_OK);
        SpeedNP.setState(IPS_OK);
        SpeedNP[0].setValue(0);
        SpeedNP.apply();

        FocusRelPosNP.apply();
        FocusAbsPosNP.apply();


        lastPos = FocusAbsPosNP[0].getValue();

        if (HomeSP.getState() == IPS_BUSY)
        {
            LOG_INFO("Focuser at home position.");
            HomeSP[0].setState(ISS_OFF);
            HomeSP.setState(IPS_OK);
            HomeSP.apply();
        }
        else if (CalibrationSP.getState() == IPS_BUSY)
        {
            ISState states[2] = { ISS_OFF, ISS_ON };
            const char * names[2] = { CalibrationSP[CALIBRATION_START].getName(), CalibrationSP[CALIBRATION_NEXT].getName() };
            ISNewSwitch(getDeviceName(), CalibrationSP.getName(), states, const_cast<char **>(names), CalibrationSP.size());
        }
        else
            LOG_INFO("Focuser reached requested position.");
        return;
    }
    else
    {
        FocusAbsPosNP.apply();
    }

    SpeedNP.setState(IPS_BUSY);
    SpeedNP.apply();

    lastPos = FocusAbsPosNP[0].getValue();

    IERmTimer(m_MotionProgressTimerID);
    m_MotionProgressTimerID = IEAddTimer(500, &SestoSenso2::checkMotionProgressHelper, this);
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
                const char * names[2] = { CalibrationSP[CALIBRATION_START].getName(), CalibrationSP[CALIBRATION_NEXT].getName() };
                ISNewSwitch(getDeviceName(), CalibrationSP.getName(), states, const_cast<char **>(names), CalibrationSP.size());
                return;
            }
        }
    }

    m_HallSensorTimerID = IEAddTimer(1000, &SestoSenso2::checkHallSensorHelper, this);
}

void SestoSenso2::TimerHit()
{
    if (!isConnected() || FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY || (m_IsSestoSenso2
            && CalibrationSP.getState() == IPS_BUSY))
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > 0)
        {
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
        }
    }

    if (m_TemperatureCounter++ == SESTO_TEMPERATURE_FREQ)
    {
        rc = updateTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureNP[0].getValue()) >= 0.1)
            {
                TemperatureNP.apply();
                lastTemperature = TemperatureNP[0].getValue();
            }
        }

        // Also use temparature poll rate for tracking input voltage
        rc = updateVoltageIn();
        if (rc)
        {
            if (fabs(lastVoltageIn - VoltageInNP[0].getValue()) >= 0.1)
            {
                VoltageInNP.apply();
                lastVoltageIn = VoltageInNP[0].getValue();

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

bool SestoSenso2::getStartupValues()
{
    bool rc = updatePosition();
    if (rc)
    {
        FocusAbsPosNP.apply();
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
    FirmwareTP[FIRMWARE_SN].setText(res);

    if (command->getFirmwareVersion(res))
    {
        LOGF_INFO("Firmware version: %s", res);
        FirmwareTP[FIRMWARE_VERSION].setText(res);
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

    MotorRateNP.save(fp);
    MotorCurrentNP.save(fp);
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
