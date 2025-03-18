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
    TemperatureNP[TEMPERATURE_MOTOR].fill("TEMPERATURE", "Motor (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP[TEMPERATURE_EXTERNAL].fill("TEMPERATURE_ETX", "External (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", ENVIRONMENT_TAB,
                       IP_RO, 0, IPS_IDLE);

    // Current Speed
    SpeedNP[0].fill("SPEED", "steps/s", "%.f", 0, 7000., 1, 0);
    SpeedNP.fill(getDeviceName(), "FOCUS_SPEED", "Motor Speed", MAIN_CONTROL_TAB, IP_RO, 0,
                 IPS_IDLE);

    // Backlash measurement
    BacklashMessageTP[0].fill("BACKLASH", "Backlash stage", "Press START to measure backlash.");
    BacklashMessageTP.fill(getDeviceName(), "BACKLASH_MESSAGE", "Backlash",
                           MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Backlash measurement stages
    BacklashMeasurementSP[BACKLASH_START].fill("BACKLASH_START", "Start", ISS_OFF);
    BacklashMeasurementSP[BACKLASH_NEXT].fill("BACKLASH_NEXT", "Next", ISS_OFF);
    BacklashMeasurementSP.fill(getDeviceName(), "FOCUS_BACKLASH", "Backlash",
                               MAIN_CONTROL_TAB,
                               IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Speed Moves
    FastMoveSP[FASTMOVE_IN].fill("FASTMOVE_IN", "Move In", ISS_OFF);
    FastMoveSP[FASTMOVE_OUT].fill("FASTMOVE_OUT", "Move out", ISS_OFF);
    FastMoveSP[FASTMOVE_STOP].fill("FASTMOVE_STOP", "Stop", ISS_OFF);
    FastMoveSP.fill(getDeviceName(), "FAST_MOVE", "Calibration Move", MAIN_CONTROL_TAB, IP_RW,
                    ISR_ATMOST1, 0, IPS_IDLE);

    // Override the default Max. Position to make it Read-Only
    FocusMaxPosNP.fill(getDeviceName(), "FOCUS_MAX", "Max. Position", MAIN_CONTROL_TAB, IP_RO,
                       0, IPS_IDLE);

    //////////////////////////////////////////////////////
    /// Arco Properties
    /////////////////////////////////////////////////////
    INDI::RotatorInterface::initProperties(ROTATOR_TAB);

    setDriverInterface(getDriverInterface() | ROTATOR_INTERFACE);

    // Rotator Ticks
    RotatorAbsPosNP[0].fill("ROTATOR_ABSOLUTE_POSITION", "Ticks", "%.f", 0., 100000., 1000., 0.);
    RotatorAbsPosNP.fill(getDeviceName(), "ABS_ROTATOR_POSITION", "Goto", ROTATOR_TAB, IP_RW,
                         0, IPS_IDLE );
    // Rotator Calibration
    RotatorCalibrationSP[ARCO_CALIBRATION_START].fill("ARCO_CALIBRATION_START", "Start", ISS_OFF);
    RotatorCalibrationSP.fill(getDeviceName(), "ARCO_CALIBRATION", "Calibrate", ROTATOR_TAB,
                              IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Read reverse rotator config
    ReverseRotatorSP.load();

    //////////////////////////////////////////////////////
    // Defaults
    /////////////////////////////////////////////////////

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
    PresetNP[0].setMax(FocusMaxPosNP[0].getValue());
    PresetNP[1].setMax(FocusMaxPosNP[0].getValue());
    PresetNP[2].setMax(FocusMaxPosNP[0].getValue());

    FocusBacklashNP[0].setMin(0);
    FocusBacklashNP[0].setMax(10000);
    FocusBacklashNP[0].setStep(1);
    FocusBacklashNP[0].setValue(0);

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

        defineProperty(SpeedNP);
        defineProperty(BacklashMessageTP);
        defineProperty(BacklashMeasurementSP);
        defineProperty(FirmwareTP);

        if (updateTemperature())
            defineProperty(TemperatureNP);

        if (updateVoltageIn())
            defineProperty(VoltageNP);

        // Rotator
        INDI::RotatorInterface::updateProperties();
        defineProperty(RotatorAbsPosNP);
        defineProperty(RotatorCalibrationSP);
        defineProperty(&RotCalibrationMessageTP);
    }
    else
    {
        //Focuser
        INDI::Focuser::updateProperties();

        if (TemperatureNP.getState() == IPS_OK)
            deleteProperty(TemperatureNP);

        deleteProperty(FirmwareTP.getName());
        deleteProperty(VoltageNP.getName());
        deleteProperty(BacklashMessageTP);
        deleteProperty(BacklashMeasurementSP);
        deleteProperty(SpeedNP);

        // Rotator
        INDI::RotatorInterface::updateProperties();
        deleteProperty(RotatorAbsPosNP);
        deleteProperty(RotatorCalibrationSP);
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
        TemperatureNP[TEMPERATURE_MOTOR].setValue(temperature);
        TemperatureNP.setState(IPS_OK);
    }
    else
        TemperatureNP.setState(IPS_ALERT);

    TemperatureNP[TEMPERATURE_EXTERNAL].setValue(-273.15);
    if (m_Esatto->getExternalTemp(temperature) && temperature > -127)
    {
        TemperatureNP[TEMPERATURE_EXTERNAL].setValue(temperature);
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
        FocusMaxPosNP[0].setMax(maxLimit);
        if (FocusMaxPosNP[0].getValue() > maxLimit)
            FocusMaxPosNP[0].setValue(maxLimit);

        FocusAbsPosNP[0].setMin(0);
        FocusAbsPosNP[0].setMax(maxLimit);
        FocusAbsPosNP[0].setValue(0);
        FocusAbsPosNP[0].setStep((FocusAbsPosNP[0].getMax() - FocusAbsPosNP[0].getMin()) / 50.0);

        FocusRelPosNP[0].setMin(0.);
        FocusRelPosNP[0].setMax(FocusAbsPosNP[0].getStep());
        FocusRelPosNP[0].setValue(0);
        FocusRelPosNP[0].setStep(FocusAbsPosNP[0].getStep());

        PresetNP[0].setMax(maxLimit);
        PresetNP[0].setStep((FocusAbsPosNP[0].getMax() - FocusAbsPosNP[0].getMin()) / 50.0);
        PresetNP[1].setMax(maxLimit);
        PresetNP[1].setStep((FocusAbsPosNP[0].getMax() - FocusAbsPosNP[0].getMin()) / 50.0);
        PresetNP[2].setMax(maxLimit);
        PresetNP[2].setStep((FocusAbsPosNP[0].getMax() - FocusAbsPosNP[0].getMin()) / 50.0);


        FocusMaxPosNP.setState(IPS_OK);
        return true;
    }

    FocusMaxPosNP.setState(IPS_ALERT);
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
        FocusAbsPosNP[0].setValue(steps);

    double arcoPosition;
    // Update Arco steps position
    if (m_Arco->getAbsolutePosition(PrimalucaLabs::UNIT_STEPS, arcoPosition))
    {
        //Update Rotator Position
        RotatorAbsPosNP[0].setValue(arcoPosition);
    }

    // Update Arco degrees position
    if (m_Arco->getAbsolutePosition(PrimalucaLabs::UNIT_DEGREES, arcoPosition))
    {
        //Update Rotator Position
        const bool isReversed = ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON;
        if (isReversed)
            GotoRotatorNP[0].setValue(range360(360 - arcoPosition));
        else
            GotoRotatorNP[0].setValue(range360(arcoPosition));

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
        if (BacklashMeasurementSP.isNameMatch(name))
        {
            BacklashMeasurementSP.setState(IPS_BUSY);
            BacklashMeasurementSP.update(states, names, n);

            auto current_switch = BacklashMeasurementSP.findOnSwitchIndex();
            BacklashMeasurementSP[current_switch].setState(ISS_ON);
            BacklashMeasurementSP.apply();

            if (current_switch == BACKLASH_START)
            {
                if (bStage == BacklashIdle || bStage == BacklashComplete )
                {
                    // Start the backlash measurement process
                    LOG_INFO("Start Backlash measurement.");
                    BacklashMeasurementSP.setState(IPS_BUSY);
                    BacklashMeasurementSP.apply();

                    BacklashMessageTP[0].setText("Drive the focuser in any direction until focus changes.");
                    BacklashMessageTP.apply();

                    // Set next step
                    bStage = BacklashMinimum;
                }
                else
                {
                    LOG_INFO("Already started backlash measure. Proceed to next step.");
                    BacklashMessageTP[0].setText("Already started. Proceed to NEXT.");
                    BacklashMessageTP.apply();
                }
            }
            else if (current_switch == BACKLASH_NEXT)
            {
                if (bStage == BacklashMinimum)
                {
                    FocusBacklashNP[0].setValue(static_cast<int32_t>(FocusAbsPosNP[0].getValue()));

                    BacklashMessageTP[0].setText( "Drive the focuser in the opposite direction, then press NEXT to finish.");
                    BacklashMessageTP.apply();
                    bStage = BacklashMaximum;
                }
                else if (bStage == BacklashMaximum)
                {
                    FocusBacklashNP[0].setValue(FocusBacklashNP[0].getValue() - FocusAbsPosNP[0].getValue());

                    // Set Esatto backlash
                    SetFocuserBacklash(FocusBacklashNP[0].getValue());
                    FocusBacklashNP.apply();

                    SetFocuserBacklashEnabled(true);

                    BacklashMessageTP[0].setText("Backlash Measure Completed.");
                    BacklashMessageTP.apply();

                    bStage = BacklashComplete;

                    LOG_INFO("Backlash measurement completed");
                    BacklashMeasurementSP.setState(IPS_OK);
                    BacklashMeasurementSP.apply();
                    BacklashMeasurementSP[current_switch].setState(ISS_OFF);
                    BacklashMeasurementSP.apply();
                }
                else
                {
                    BacklashMessageTP[0].setText("Backlash not in progress.");
                    BacklashMessageTP.apply();
                }

            }
            return true;
        }
        // Fast motion
        else if (FastMoveSP.isNameMatch(name))
        {
            FastMoveSP.update(states, names, n);
            auto current_switch = FastMoveSP.findOnSwitchIndex();

            switch (current_switch)
            {
                case FASTMOVE_IN:
                    if (!m_Esatto->fastMoveIn())
                        return false;
                    FastMoveSP.setState(IPS_BUSY);
                    break;
                case FASTMOVE_OUT:
                    if (!m_Esatto->fastMoveOut())
                        return false;
                    FastMoveSP.setState(IPS_BUSY);
                    break;
                case FASTMOVE_STOP:
                    if (!m_Esatto->stop())
                        return false;
                    FastMoveSP.setState(IPS_IDLE);
                    break;
                default:
                    break;
            }

            FastMoveSP.apply();
            return true;
        }
        // Rotator Calibration
        else if (RotatorCalibrationSP.isNameMatch(name))
        {
            if(m_Arco->calibrate())
            {
                LOG_INFO("Calibrating Arco. Please wait.");
                RotatorAbsPosNP.setState(IPS_BUSY);
                GotoRotatorNP.setState(IPS_BUSY);
                RotatorCalibrationSP.setState(IPS_BUSY);
                RotatorCalibrationSP.apply();
                GotoRotatorNP.apply();
                RotatorAbsPosNP.apply();
            }
            else
            {
                RotatorCalibrationSP.reset();
                RotatorCalibrationSP.setState(IPS_ALERT);
                RotatorCalibrationSP.apply();
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

    else if (RotatorAbsPosNP.isNameMatch(name))
    {
        if (m_Arco->moveAbsolutePoition(PrimalucaLabs::UNIT_STEPS, values[0]))
            RotatorAbsPosNP.setState(IPS_BUSY);
        else
            RotatorAbsPosNP.setState(IPS_ALERT);
        GotoRotatorNP.setState(RotatorAbsPosNP.getState());
        RotatorAbsPosNP.apply();
        GotoRotatorNP.apply();
        if (RotatorAbsPosNP.getState() == IPS_BUSY)
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
        RotatorAbsPosNP.setState(IPS_BUSY);
        RotatorAbsPosNP.apply();
        return IPS_BUSY;
    }
    return IPS_ALERT;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
IPState EsattoArco::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (FocusReverseSP.findOnSwitchIndex() == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosNP[0].getValue() + relativeTicks;

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

    auto currentFocusPosition = FocusAbsPosNP[0].getValue();
    auto currentRotatorPosition = RotatorAbsPosNP[0].getValue();
    if (updatePosition())
    {
        if (std::abs(currentFocusPosition - FocusAbsPosNP[0].getValue()) > 0)
        {
            // Focuser State Machine
            if (FocusAbsPosNP.getState() == IPS_BUSY && m_Esatto->isBusy() == false)
            {
                FocusAbsPosNP.setState(IPS_OK);
                FocusRelPosNP.setState(IPS_OK);
                FocusAbsPosNP.apply();
                FocusRelPosNP.apply();
            }
            else
                FocusAbsPosNP.apply();
        }

        // Rotator State Machine

        // Only check status if position changed.
        if (std::abs(currentRotatorPosition - RotatorAbsPosNP[0].getValue()) > 0)
        {
            // Rotator was busy and now stopped?
            if (GotoRotatorNP.getState() == IPS_BUSY && m_Arco->isBusy() == false)
            {
                // Check if we were calibrating
                if(RotatorCalibrationSP.getState() == IPS_BUSY)
                {
                    RotatorCalibrationSP.setState(IPS_IDLE);
                    RotatorCalibrationSP.apply();
                    LOG_INFO("Arco calibration complete.");
                    if(m_Arco->sync(PrimalucaLabs::UNIT_STEPS, 0))
                        LOG_INFO("Arco position synced to zero.");
                }
                GotoRotatorNP.setState(IPS_OK);
                RotatorAbsPosNP.setState(IPS_OK);
                GotoRotatorNP.apply();
                RotatorAbsPosNP.apply();
            }
            else
            {
                GotoRotatorNP.apply();
                RotatorAbsPosNP.apply();
            }
        }
    }

    if (m_TemperatureCounter++ == TEMPERATURE_FREQUENCY)
    {
        auto currentTemperature = TemperatureNP[0].getValue();
        if (updateTemperature())
        {
            if (std::abs(currentTemperature - TemperatureNP[0].getValue()) >= 0.1)
                TemperatureNP.apply();
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
        RotatorAbsPosNP[0].setMin(calMin);
        RotatorAbsPosNP[0].setMax(calMax);
        RotatorAbsPosNP[0].setStep(std::abs(calMax - calMin) / 50.0);
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
    const bool isReversed = ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON;
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
    if (rc && RotatorAbsPosNP.getState() != IPS_IDLE)
    {
        RotatorAbsPosNP.setState(IPS_IDLE);
        GotoRotatorNP.setState(IPS_IDLE);
        RotatorAbsPosNP.apply();
        GotoRotatorNP.apply();
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
    GotoRotatorNP[0].setValue(range360(360 - GotoRotatorNP[0].getValue()));
    return true;
}

/************************************************************************************************************
 *
*************************************************************************************************************/
bool EsattoArco::SyncRotator(double angle)
{
    const bool isReversed = ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON;
    auto newAngle = 0;
    if (isReversed)
        newAngle = ( angle > 180 ? 360 - angle : angle * -1);
    else
        newAngle = ( angle > 180 ? angle - 360 : angle);
    return m_Arco->sync(PrimalucaLabs::UNIT_DEGREES, newAngle);
}
