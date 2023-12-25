/*
    Primaluca Labs Essato-Arco Focuser+Rotator Driver

    Copyright (C) 2020 Piotr Zyziuk
    Copyright (C) 2020-2022 Jasem Mutlaq

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

    JM 2022.07.16: Major refactor to using json.h and update to Essato Arco
    Document protocol revision 3.3 (8th July 2022).
*/

#include "esattoarco.h"

#include "indicom.h"

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

#include <cmath>
#include <cstring>
#include <memory>
#include <algorithm>

#include <assert.h>
#include <termios.h>
#include <unistd.h>
#include <connectionplugins/connectionserial.h>
#include <sys/ioctl.h>

static std::unique_ptr<EsattoArco> esattoarco(new EsattoArco());

static const char *ENVIRONMENT_TAB  = "Environment";
static const char *ROTATOR_TAB = "Rotator";

EsattoArco::EsattoArco() : RotatorInterface(this)
{
    setVersion(1, 0);

    // Focuser capabilities
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_HAS_BACKLASH | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
    // Rotator capabilities
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_SYNC | ROTATOR_CAN_REVERSE);
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::initProperties()
{
    INDI::Focuser::initProperties();

    setConnectionParams();

    // Firmware information
    FirmwareTP[ESATTO_FIRMWARE_SN].fill("ESATTO_FIRMWARE_SN", "Esatto SN", "");
    FirmwareTP[ESATTO_FIRMWARE_VERSION].fill("ESATTO_FIRMWARE_VERSION", "Esatto Firmware", "");
    FirmwareTP[ARCO_FIRMWARE_SN].fill("ARCO_FIRMWARE_SN", "Arco SN", "");
    FirmwareTP[ARCO_FIRMWARE_VERSION].fill("VERARCO_FIRMWARE_VERSIONSION", "Arco Firmware", "");
    FirmwareTP.fill(getDeviceName(), "FOCUS_FIRMWARE", "Firmware", CONNECTION_TAB, IP_RO, 60, IPS_IDLE);

    //////////////////////////////////////////////////////
    /// Esatto Properties
    /////////////////////////////////////////////////////

    // Voltage Information
    VoltageNP[VOLTAGE_12V].fill("VOLTAGE_12V", "12v", "%.2f", 0, 100, 0., 0.);
    VoltageNP[VOLTAGE_USB].fill("VOLTAGE_USB", "USB", "%.2f", 0, 100, 0., 0.);
    VoltageNP.fill(getDeviceName(), "VOLTAGE_IN", "Voltage in", ENVIRONMENT_TAB, IP_RO, 0, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[TEMPERATURE_MOTOR], "TEMPERATURE", "Motor (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumber(&TemperatureN[TEMPERATURE_EXTERNAL], "TEMPERATURE_ETX", "External (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 2, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", ENVIRONMENT_TAB,
                       IP_RO, 0, IPS_IDLE);

    // Current Speed
    IUFillNumber(&SpeedN[0], "SPEED", "steps/s", "%.f", 0, 7000., 1, 0);
    IUFillNumberVector(&SpeedNP, SpeedN, 1, getDeviceName(), "FOCUS_SPEED", "Motor Speed", MAIN_CONTROL_TAB, IP_RO, 0,
                       IPS_IDLE);

    // Backlash measurement
    IUFillText(&BacklashMessageT[0], "BACKLASH", "Backlash stage", "Press START to measure backlash.");
    IUFillTextVector(&BacklashMessageTP, BacklashMessageT, 1, getDeviceName(), "BACKLASH_MESSAGE", "Backlash",
                     MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Backlash measurement stages
    IUFillSwitch(&BacklashMeasurementS[BACKLASH_START], "BACKLASH_START", "Start", ISS_OFF);
    IUFillSwitch(&BacklashMeasurementS[BACKLASH_NEXT], "BACKLASH_NEXT", "Next", ISS_OFF);
    IUFillSwitchVector(&BacklashMeasurementSP, BacklashMeasurementS, 2, getDeviceName(), "FOCUS_BACKLASH", "Backlash",
                       MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Speed Moves
    IUFillSwitch(&FastMoveS[FASTMOVE_IN], "FASTMOVE_IN", "Move In", ISS_OFF);
    IUFillSwitch(&FastMoveS[FASTMOVE_OUT], "FASTMOVE_OUT", "Move out", ISS_OFF);
    IUFillSwitch(&FastMoveS[FASTMOVE_STOP], "FASTMOVE_STOP", "Stop", ISS_OFF);
    IUFillSwitchVector(&FastMoveSP, FastMoveS, 3, getDeviceName(), "FAST_MOVE", "Calibration Move", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    // Override the default Max. Position to make it Read-Only
    IUFillNumberVector(&FocusMaxPosNP, FocusMaxPosN, 1, getDeviceName(), "FOCUS_MAX", "Max. Position", MAIN_CONTROL_TAB, IP_RO,
                       0, IPS_IDLE);

    //////////////////////////////////////////////////////
    /// Arco Properties
    /////////////////////////////////////////////////////
    INDI::RotatorInterface::initProperties(ROTATOR_TAB);

    setDriverInterface(getDriverInterface() | ROTATOR_INTERFACE);

    // Rotator Ticks
    IUFillNumber(&RotatorAbsPosN[0], "ROTATOR_ABSOLUTE_POSITION", "Ticks", "%.f", 0., 100000., 1000., 0.);
    IUFillNumberVector(&RotatorAbsPosNP, RotatorAbsPosN, 1, getDeviceName(), "ABS_ROTATOR_POSITION", "Goto", ROTATOR_TAB, IP_RW,
                       0, IPS_IDLE );
    // Rotator Calibration
    IUFillSwitch(&RotCalibrationS[ARCO_CALIBRATION_START], "ARCO_CALIBRATION_START", "Start", ISS_OFF);
    IUFillSwitchVector(&RotatorCalibrationSP, RotCalibrationS, 1, getDeviceName(), "ARCO_CALIBRATION", "Calibrate", ROTATOR_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Read reverse rotator config
    int index = -1;
    IUGetConfigOnSwitchIndex(getDeviceName(), ReverseRotatorSP.name, &index);
    if (index >= 0)
    {
        IUResetSwitch(&ReverseRotatorSP);
        ReverseRotatorS[index].s = ISS_ON;
    }

    //////////////////////////////////////////////////////
    // Defaults
    /////////////////////////////////////////////////////

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

    FocusBacklashN[0].min = 0;
    FocusBacklashN[0].max = 10000;
    FocusBacklashN[0].step = 1;
    FocusBacklashN[0].value = 0;

    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::updateProperties()
{
    if (isConnected() && updateMaxLimit() == false)
        LOGF_WARN("Check you have the latest %s firmware. Focuser requires calibration.", getDeviceName());

    if (isConnected())
    {
        if (getStartupValues())
            LOGF_INFO("Parameters updated, %s ready for use.", getDeviceName());
        else
            LOG_WARN("Failed to inquire parameters. Check logs.");

        //Focuser
        INDI::Focuser::updateProperties();

        defineProperty(&SpeedNP);
        defineProperty(&BacklashMessageTP);
        defineProperty(&BacklashMeasurementSP);
        defineProperty(FirmwareTP);

        if (updateTemperature())
            defineProperty(&TemperatureNP);

        if (updateVoltageIn())
            defineProperty(VoltageNP);

        // Rotator
        INDI::RotatorInterface::updateProperties();
        defineProperty(&RotatorAbsPosNP);
        defineProperty(&RotatorCalibrationSP);
        defineProperty(&RotCalibrationMessageTP);
    }
    else
    {
        //Focuser
        INDI::Focuser::updateProperties();

        if (TemperatureNP.s == IPS_OK)
            deleteProperty(TemperatureNP.name);

        deleteProperty(FirmwareTP.getName());
        deleteProperty(VoltageNP.getName());
        deleteProperty(BacklashMessageTP.name);
        deleteProperty(BacklashMeasurementSP.name);
        deleteProperty(SpeedNP.name);

        // Rotator
        INDI::RotatorInterface::updateProperties();
        deleteProperty(RotatorAbsPosNP.name);
        deleteProperty(RotatorCalibrationSP.name);
        deleteProperty(RotCalibrationMessageTP.name);
    }

    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::Handshake()
{
    if (Ack())
    {
        LOGF_INFO("%s is online. Getting parameters...", getDeviceName());
        return true;
    }

    LOG_INFO("Error retrieving data from device, please ensure focuser is powered and the port is correct.");
    return false;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
const char *EsattoArco::getDefaultName()
{
    return "Esatto Arco";
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::updateTemperature()
{
    double temperature = 0;
    if (m_Esatto->getMotorTemp(temperature))
    {
        TemperatureN[TEMPERATURE_MOTOR].value = temperature;
        TemperatureNP.s = IPS_OK;
    }
    else
        TemperatureNP.s = IPS_ALERT;

    TemperatureN[TEMPERATURE_EXTERNAL].value = -273.15;
    if (m_Esatto->getExternalTemp(temperature) && temperature > -127)
    {
        TemperatureN[TEMPERATURE_EXTERNAL].value = temperature;
    }

    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::updateMaxLimit()
{
    uint32_t maxLimit = 0;

    if (m_Esatto->getMaxPosition(maxLimit) && maxLimit > 0)
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

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::SetFocuserBacklash(int32_t steps)
{
    return m_Esatto->setBacklash(steps);
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::updatePosition()
{
    uint32_t steps;
    // Update focuser position
    if (m_Esatto->getAbsolutePosition(steps))
        FocusAbsPosN[0].value = steps;

    double arcoPosition;
    // Update Arco steps position
    if (m_Arco->getAbsolutePosition(PrimalucaLabs::UNIT_STEPS, arcoPosition))
    {
        //Update Rotator Position
        RotatorAbsPosN[0].value = arcoPosition;
    }

    // Update Arco degrees position
    if (m_Arco->getAbsolutePosition(PrimalucaLabs::UNIT_DEGREES, arcoPosition))
    {
        //Update Rotator Position
        const bool isReversed = ReverseRotatorS[INDI_ENABLED].s == ISS_ON;
        if (isReversed)
            GotoRotatorN[0].value = range360(360 - arcoPosition);
        else
            GotoRotatorN[0].value = range360(arcoPosition);

    }

    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::updateVoltageIn()
{
    double voltage;
    if (m_Esatto->getVoltage12v(voltage))
        VoltageNP[VOLTAGE_12V].setValue(voltage);

    VoltageNP.setState((voltage >= 11.0) ? IPS_OK : IPS_ALERT);
    if (m_Esatto->getVoltageUSB(voltage))
        VoltageNP[VOLTAGE_USB].setValue(voltage);
    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::isMotionComplete()
{
    uint32_t speed;
    if (m_Esatto->getCurrentSpeed(speed))
    {
        return speed == 0;
    }
    return false;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Set backlash
        if (!strcmp(name, BacklashMeasurementSP.name))
        {
            BacklashMeasurementSP.s = IPS_BUSY;
            IUUpdateSwitch(&BacklashMeasurementSP, states, names, n);

            auto current_switch = IUFindOnSwitchIndex(&BacklashMeasurementSP);
            BacklashMeasurementS[current_switch].s = ISS_ON;
            IDSetSwitch(&BacklashMeasurementSP, nullptr);

            if (current_switch == BACKLASH_START)
            {
                if (bStage == BacklashIdle || bStage == BacklashComplete )
                {
                    // Start the backlash measurement process
                    LOG_INFO("Start Backlash measurement.");
                    BacklashMeasurementSP.s = IPS_BUSY;
                    IDSetSwitch(&BacklashMeasurementSP, nullptr);

                    IUSaveText(&BacklashMessageT[0], "Drive the focuser in any direction until focus changes.");
                    IDSetText(&BacklashMessageTP, nullptr);

                    // Set next step
                    bStage = BacklashMinimum;
                }
                else
                {
                    LOG_INFO("Already started backlash measure. Proceed to next step.");
                    IUSaveText(&BacklashMessageT[0], "Already started. Proceed to NEXT.");
                    IDSetText(&BacklashMessageTP, nullptr);
                }
            }
            else if (current_switch == BACKLASH_NEXT)
            {
                if (bStage == BacklashMinimum)
                {
                    FocusBacklashN[0].value = static_cast<int32_t>(FocusAbsPosN[0].value);

                    IUSaveText(&BacklashMessageT[0], "Drive the focuser in the opposite direction, then press NEXT to finish.");
                    IDSetText(&BacklashMessageTP, nullptr);
                    bStage = BacklashMaximum;
                }
                else if (bStage == BacklashMaximum)
                {
                    FocusBacklashN[0].value -= FocusAbsPosN[0].value;

                    // Set Esatto backlash
                    SetFocuserBacklash(FocusBacklashN[0].value);
                    IDSetNumber(&FocusBacklashNP, nullptr);

                    SetFocuserBacklashEnabled(true);

                    IUSaveText(&BacklashMessageT[0], "Backlash Measure Completed.");
                    IDSetText(&BacklashMessageTP, nullptr);

                    bStage = BacklashComplete;

                    LOG_INFO("Backlash measurement completed");
                    BacklashMeasurementSP.s = IPS_OK;
                    IDSetSwitch(&BacklashMeasurementSP, nullptr);
                    BacklashMeasurementS[current_switch].s = ISS_OFF;
                    IDSetSwitch(&BacklashMeasurementSP, nullptr);
                }
                else
                {
                    IUSaveText(&BacklashMessageT[0], "Backlash not in progress.");
                    IDSetText(&BacklashMessageTP, nullptr);
                }

            }
            return true;
        }
        // Fast motion
        else if (!strcmp(name, FastMoveSP.name))
        {
            IUUpdateSwitch(&FastMoveSP, states, names, n);
            auto current_switch = IUFindOnSwitchIndex(&FastMoveSP);

            switch (current_switch)
            {
                case FASTMOVE_IN:
                    if (!m_Esatto->fastMoveIn())
                        return false;
                    FastMoveSP.s = IPS_BUSY;
                    break;
                case FASTMOVE_OUT:
                    if (!m_Esatto->fastMoveOut())
                        return false;
                    FastMoveSP.s = IPS_BUSY;
                    break;
                case FASTMOVE_STOP:
                    if (!m_Esatto->stop())
                        return false;
                    FastMoveSP.s = IPS_IDLE;
                    break;
                default:
                    break;
            }

            IDSetSwitch(&FastMoveSP, nullptr);
            return true;
        }
        // Rotator Calibration
        else if (!strcmp(name, RotatorCalibrationSP.name))
        {
            if(m_Arco->calibrate())
            {
                LOG_INFO("Calibrating Arco. Please wait.");
                RotatorAbsPosNP.s = IPS_BUSY;
                GotoRotatorNP.s = IPS_BUSY;
                RotatorCalibrationSP.s = IPS_BUSY;
                IDSetSwitch(&RotatorCalibrationSP, nullptr);
                IDSetNumber(&GotoRotatorNP, nullptr);
                IDSetNumber(&RotatorAbsPosNP, nullptr);
            }
            else
            {
                IUResetSwitch(&RotatorCalibrationSP);
                RotatorCalibrationSP.s = IPS_ALERT;
                IDSetSwitch(&RotatorCalibrationSP, nullptr);
            }
            return true;
        }
        else if (strstr(name, "ROTATOR"))
        {
            if (INDI::RotatorInterface::processSwitch(dev, name, states, names, n))
                return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev == nullptr || strcmp(dev, getDeviceName()) != 0)
        return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

    else if (strcmp(name, RotatorAbsPosNP.name) == 0)
    {
        if (m_Arco->moveAbsolutePoition(PrimalucaLabs::UNIT_STEPS, values[0]))
            RotatorAbsPosNP.s = IPS_BUSY;
        else
            RotatorAbsPosNP.s = IPS_ALERT;
        GotoRotatorNP.s = RotatorAbsPosNP.s;
        IDSetNumber(&RotatorAbsPosNP, nullptr);
        IDSetNumber(&GotoRotatorNP, nullptr);
        if (RotatorAbsPosNP.s == IPS_BUSY)
            LOGF_INFO("Rotator moving to %.f steps...", values[0]);
        return true;
    }
    else if (strstr(name, "ROTATOR"))
    {
        if (INDI::RotatorInterface::processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************************************
 *
*************************************************************************************************************/
IPState EsattoArco::MoveAbsFocuser(uint32_t targetTicks)
{
    if (m_Esatto->goAbsolutePosition(targetTicks))
    {
        RotatorAbsPosNP.s = IPS_BUSY;
        IDSetNumber(&RotatorAbsPosNP, nullptr);
        return IPS_BUSY;
    }
    return IPS_ALERT;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
IPState EsattoArco::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (IUFindOnSwitchIndex(&FocusReverseSP) == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosN[0].value + relativeTicks;

    bool rc = MoveAbsFocuser(newPosition);

    return (rc ? IPS_BUSY : IPS_ALERT);
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::AbortFocuser()
{
    return m_Esatto->stop();
}

/************************************************************************************************************
 *
*************************************************************************************************************/
void EsattoArco::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    auto currentFocusPosition = FocusAbsPosN[0].value;
    auto currentRotatorPosition = RotatorAbsPosN[0].value;
    if (updatePosition())
    {
        if (std::abs(currentFocusPosition - FocusAbsPosN[0].value) > 0)
        {
            // Focuser State Machine
            if (FocusAbsPosNP.s == IPS_BUSY && m_Esatto->isBusy() == false)
            {
                FocusAbsPosNP.s = IPS_OK;
                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
            }
            else
                IDSetNumber(&FocusAbsPosNP, nullptr);
        }

        // Rotator State Machine

        // Only check status if position changed.
        if (std::abs(currentRotatorPosition - RotatorAbsPosN[0].value) > 0)
        {
            // Rotator was busy and now stopped?
            if (GotoRotatorNP.s == IPS_BUSY && m_Arco->isBusy() == false)
            {
                // Check if we were calibrating
                if(RotatorCalibrationSP.s == IPS_BUSY)
                {
                    RotatorCalibrationSP.s = IPS_IDLE;
                    IDSetSwitch(&RotatorCalibrationSP, nullptr);
                    LOG_INFO("Arco calibration complete.");
                    if(m_Arco->sync(PrimalucaLabs::UNIT_STEPS, 0))
                        LOG_INFO("Arco position synced to zero.");
                }
                GotoRotatorNP.s = IPS_OK;
                RotatorAbsPosNP.s = IPS_OK;
                IDSetNumber(&GotoRotatorNP, nullptr);
                IDSetNumber(&RotatorAbsPosNP, nullptr);
            }
            else
            {
                IDSetNumber(&GotoRotatorNP, nullptr);
                IDSetNumber(&RotatorAbsPosNP, nullptr);
            }
        }
    }

    if (m_TemperatureCounter++ == TEMPERATURE_FREQUENCY)
    {
        auto currentTemperature = TemperatureN[0].value;
        if (updateTemperature())
        {
            if (std::abs(currentTemperature - TemperatureN[0].value) >= 0.1)
                IDSetNumber(&TemperatureNP, nullptr);
        }

        auto current12V = VoltageNP[VOLTAGE_12V].getValue();
        auto currentUSB = VoltageNP[VOLTAGE_USB].getValue();
        if (updateVoltageIn())
        {
            if (std::abs(current12V - VoltageNP[VOLTAGE_12V].getValue()) >= 0.1 ||
                    std::abs(currentUSB - VoltageNP[VOLTAGE_USB].getValue()) >= 0.1)
            {
                VoltageNP.apply();
                if (VoltageNP[VOLTAGE_12V].getValue() < 11.0)
                    LOG_WARN("Please check 12v DC power supply is connected.");
            }
        }

        m_TemperatureCounter = 0;   // Reset the counter
    }

    SetTimer(getCurrentPollingPeriod());
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::getStartupValues()
{
    updatePosition();
    json info;
    if (m_Arco->getMotorInfo(info))
    {
        int calMax, calMin;
        try
        {
            info["get"]["MOT2"]["CAL_MAXPOS"].get_to(calMax);
            info["get"]["MOT2"]["CAL_MINPOS"].get_to(calMin);
        }
        catch (json::exception &e)
        {
            // output exception information
            LOGF_ERROR("Failed to parse info: %s Exception: %s id: %d", info.dump().c_str(),
                       e.what(), e.id);
            return false;
        }
        RotatorAbsPosN[0].min = calMin;
        RotatorAbsPosN[0].max = calMax;
        RotatorAbsPosN[0].step = std::abs(calMax - calMin) / 50.0;
    }
    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::Ack()
{
    std::string serial, firmware;

    if(initCommandSet() == false)
    {
        LOG_ERROR("Failed setting attributes on serial port and init command sets");
        return false;
    }

    if (m_Arco->setEnabled(true) && !m_Arco->isEnabled())
    {
        LOG_ERROR("Failed to enable ARCO rotator. Please check it is powered and connected.");
        return false;
    }

    bool rc1 = m_Esatto->getSerialNumber(serial);
    bool rc2 = m_Esatto->getFirmwareVersion(firmware);

    if (rc1 && rc2)
    {
        FirmwareTP[ESATTO_FIRMWARE_SN].setText(serial);
        FirmwareTP[ESATTO_FIRMWARE_VERSION].setText(firmware);
        LOGF_INFO("Esatto SN: %s Firmware version: %s", FirmwareTP[ESATTO_FIRMWARE_SN].getText(),
                  FirmwareTP[ESATTO_FIRMWARE_VERSION].getText());
    }
    else
        return false;

    rc1 = m_Arco->getSerialNumber(serial);
    rc2 = m_Arco->getFirmwareVersion(firmware);

    if (rc1 && rc2)
    {
        FirmwareTP[ARCO_FIRMWARE_SN].setText(serial);
        FirmwareTP[ARCO_FIRMWARE_VERSION].setText(firmware);
        LOGF_INFO("Arco SN: %s Firmware version: %s", FirmwareTP[ARCO_FIRMWARE_SN].getText(),
                  FirmwareTP[ARCO_FIRMWARE_VERSION].getText());
    }
    else
        return false;

    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
void EsattoArco::setConnectionParams()
{
    serialConnection->setDefaultBaudRate(serialConnection->B_115200);
    serialConnection->setWordSize(8);
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::initCommandSet()
{
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

    m_Esatto.reset(new PrimalucaLabs::Esatto(getDeviceName(), PortFD));
    m_Arco.reset(new PrimalucaLabs::Arco(getDeviceName(), PortFD));
    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::saveConfigItems(FILE *fp)
{
    Focuser::saveConfigItems(fp);
    RI::saveConfigItems(fp);
    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
IPState EsattoArco::MoveRotator(double angle)
{
    // Rotator move 0 to +180 degrees CCW
    // Rotator move 0 to -180 degrees CW
    // This is from looking at rotator from behind.
    const bool isReversed = ReverseRotatorS[INDI_ENABLED].s == ISS_ON;
    auto newAngle = 0;
    if (isReversed)
        newAngle = ( angle > 180 ? 360 - angle : angle * -1);
    else
        newAngle = ( angle > 180 ? angle - 360 : angle);

    if (m_Arco->moveAbsolutePoition(PrimalucaLabs::UNIT_DEGREES, newAngle))
        return IPS_BUSY;
    return IPS_ALERT;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::AbortRotator()
{
    auto rc = m_Arco->stop();
    if (rc && RotatorAbsPosNP.s != IPS_IDLE)
    {
        RotatorAbsPosNP.s = IPS_IDLE;
        GotoRotatorNP.s = IPS_IDLE;
        IDSetNumber(&RotatorAbsPosNP, nullptr);
        IDSetNumber(&GotoRotatorNP, nullptr);
    }

    return rc;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool  EsattoArco::ReverseRotator(bool enabled)
{
    // Do not use Primaluca native reverse since it has some bugs
    //return m_Arco->reverse(enabled);
    INDI_UNUSED(enabled);
    GotoRotatorN[0].value = range360(360 - GotoRotatorN[0].value);
    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::SyncRotator(double angle)
{
    const bool isReversed = ReverseRotatorS[INDI_ENABLED].s == ISS_ON;
    auto newAngle = 0;
    if (isReversed)
        newAngle = ( angle > 180 ? 360 - angle : angle * -1);
    else
        newAngle = ( angle > 180 ? angle - 360 : angle);
    return m_Arco->sync(PrimalucaLabs::UNIT_DEGREES, newAngle);
}
