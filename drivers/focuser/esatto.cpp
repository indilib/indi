/*
    Esatto Focuser
    Copyright (C) 2022 Jasem Mutlaq

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

#include "esatto.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <algorithm>

#include <connectionplugins/connectionserial.h>

static std::unique_ptr<Esatto> sesto(new Esatto());

static const char *ENVIRONMENT_TAB  = "Environment";

// The way out of the lock after an incomplete calibration. The web interface also calibrates away from the sensor.
static const char *CALIBRATE_HINT = "Calibrate it from the ESATTO web interface, or here with the focuser still fully "
                                    "in at its sensor. Restarting the driver lifts the lock, but does not correct the "
                                    "position.";

Esatto::Esatto()
{
    setVersion(1, 1);

    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_HAS_BACKLASH | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::initProperties()
{

    INDI::Focuser::initProperties();

    FocusBacklashNP[0].setMin(0);
    FocusBacklashNP[0].setMax(10000);
    FocusBacklashNP[0].setStep(1);
    FocusBacklashNP[0].setValue(0);

    setConnectionParams();

    // Firmware information
    FirmwareTP[FIRMWARE_SN].fill("SERIALNUMBER", "Serial Number", "");
    FirmwareTP[FIRMWARE_VERSION].fill("VERSION", "Version", "");
    FirmwareTP.fill(getDeviceName(), "FOCUS_FIRMWARE", "Firmware", CONNECTION_TAB, IP_RO, 0,  IPS_IDLE);

    // Voltage Information
    VoltageNP[VOLTAGE_12V].fill("VOLTAGE_12V", "12v", "%.2f", 0, 100, 0., 0.);
    VoltageNP[VOLTAGE_USB].fill("VOLTAGE_USB", "USB", "%.2f", 0, 100, 0., 0.);
    VoltageNP.fill(getDeviceName(), "VOLTAGE_IN", "Voltage in", ENVIRONMENT_TAB, IP_RO, 0, IPS_IDLE);

    // Focuser temperature
    TemperatureNP[TEMPERATURE_MOTOR].fill("TEMPERATURE", "Motor (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP[TEMPERATURE_EXTERNAL].fill("TEMPERATURE_ETX", "External (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", ENVIRONMENT_TAB, IP_RO, 0, IPS_IDLE);

    CalibrationSP[0].fill("CALIBRATION_START", "Start", ISS_OFF);
    CalibrationSP.fill(getDeviceName(), "FOCUS_CALIBRATION", "Calibrate", OPTIONS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Override the default Max. Position to make it Read-Only
    FocusMaxPosNP.setPermission(IP_RO);

    // Relative and absolute movement
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(50000.);
    FocusRelPosNP[0].setValue(0);
    FocusRelPosNP[0].setStep(1000);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(200000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(10000);

    FocusMaxPosNP[0].setValue(2097152);
    PresetNP[0].setMax(FocusMaxPosNP[0].getValue());
    PresetNP[1].setMax(FocusMaxPosNP[0].getValue());
    PresetNP[2].setMax(FocusMaxPosNP[0].getValue());

    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::updateProperties()
{
    if (isConnected())
        getStartupValues();

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(FirmwareTP);

        if (updateVoltageIn())
            defineProperty(VoltageNP);

        if (updateTemperature())
            defineProperty(TemperatureNP);

        defineProperty(CalibrationSP);
    }
    else
    {
        if (TemperatureNP.getState() == IPS_OK)
            deleteProperty(TemperatureNP);
        deleteProperty(FirmwareTP);
        deleteProperty(VoltageNP.getName());
        deleteProperty(CalibrationSP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0 && CalibrationSP.isNameMatch(name))
    {
        CalibrationSP.reset();

        if (!isConnected())
        {
            LOG_ERROR("Connect the focuser before calibrating it.");
            CalibrationSP.setState(IPS_ALERT);
            CalibrationSP.apply();
            return true;
        }

        // Starting it again while it runs leaves the zero position off
        if (CalibrationSP.getState() == IPS_BUSY)
        {
            LOG_ERROR("The focuser is calibrating. Wait until it is done.");
            CalibrationSP.apply();
            return true;
        }

        // The web interface only calibrates with the focuser fully in, where its sensor is active
        bool atSensor = false;
        if (FocusAbsPosNP.getState() == IPS_BUSY)
        {
            LOG_ERROR("Wait until the focuser stopped, then calibrate.");
            CalibrationSP.setState(IPS_ALERT);
        }
        else if (m_Esatto->isCalibrating())
        {
            LOG_ERROR("The focuser reports a running calibration. Wait until it is done.");
            CalibrationSP.setState(IPS_ALERT);
        }
        else if (!m_Esatto->isHallSensorDetected(atSensor) || !atSensor)
        {
            // After an incomplete calibration the focuser cannot be moved to its sensor from here
            if (isZeroPositionValid())
                LOG_ERROR("Move the focuser fully in until its sensor is active, then calibrate.");
            else
                LOGF_ERROR("The focuser is not at its sensor, and moves are locked after an incomplete calibration. %s",
                           CALIBRATE_HINT);
            CalibrationSP.setState(IPS_ALERT);
        }
        // The web interface asks whether the focuser is in or out when its sensor is active and sends another
        // request for a focuser fully out. Only the one for a focuser fully in has been seen on a device.
        else if (FocusMaxPosNP[0].getValue() > 0 && FocusAbsPosNP[0].getValue() > FocusMaxPosNP[0].getValue() / 2)
        {
            if (isZeroPositionValid())
                LOG_ERROR("The focuser is not fully in. Move it fully in until its sensor is active, then calibrate.");
            else
                LOGF_ERROR("The focuser is not fully in, and moves are locked after an incomplete calibration. %s",
                           CALIBRATE_HINT);
            CalibrationSP.setState(IPS_ALERT);
        }
        else
        {
            m_CalibrationCountKnown = m_Esatto->getCalibrationCount(m_CalibrationCount);
            if (m_Esatto->calibrate())
            {
                LOG_INFO("Calibrating the focuser. Please wait.");
                m_MoveStartPosition = FocusAbsPosNP[0].getValue();
                m_MoveTimer.start();
                CalibrationSP.setState(IPS_BUSY);
                FocusAbsPosNP.setState(IPS_BUSY);
                FocusAbsPosNP.apply();
            }
            else
                CalibrationSP.setState(IPS_ALERT);
        }

        CalibrationSP.apply();
        return true;
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::Handshake()
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
const char *Esatto::getDefaultName()
{
    return "Esatto";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::updateTemperature()
{
    double temperature = 0;

    if (isSimulation())
        temperature = 23.5;
    else if ( m_Esatto->getMotorTemp(temperature) == false)
        return false;

    if (temperature > 90)
        return false;

    TemperatureNP[TEMPERATURE_MOTOR].setValue(temperature);
    TemperatureNP.setState(IPS_OK);

    // External temperature - Optional
    if (m_Esatto->getExternalTemp(temperature) && temperature > -127 && temperature < 90)
        TemperatureNP[TEMPERATURE_EXTERNAL].setValue(temperature);
    else
        TemperatureNP[TEMPERATURE_EXTERNAL].setValue(-273.15);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::updatePosition()
{
    uint32_t steps = 0;
    if (isSimulation())
        steps = static_cast<uint32_t>(FocusAbsPosNP[0].getValue());
    else if (m_Esatto->getAbsolutePosition(steps) == false)
        return false;

    FocusAbsPosNP[0].setValue(steps);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::updateVoltageIn()
{
    double voltage = 0;
    if (m_Esatto->getVoltage12v(voltage))
        VoltageNP[VOLTAGE_12V].setValue(voltage);

    VoltageNP.setState((voltage >= 11.0) ? IPS_OK : IPS_ALERT);
    if (m_Esatto->getVoltageUSB(voltage))
        VoltageNP[VOLTAGE_USB].setValue(voltage);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState Esatto::MoveAbsFocuser(uint32_t targetTicks)
{
    // A move during the calibration leaves the zero position off
    if (CalibrationSP.getState() == IPS_BUSY)
    {
        LOG_ERROR("The focuser is calibrating. Wait until it is done.");
        return IPS_ALERT;
    }

    // Also one started elsewhere, or one that still runs in the firmware after an abort
    if (m_Esatto->isCalibrating())
    {
        LOG_ERROR("The focuser reports a running calibration. Wait until it is done.");
        return IPS_ALERT;
    }

    // A move after an aborted calibration ran 60 s against the inner end of an ESATTO 2", its position counting up
    if (!isZeroPositionValid())
    {
        LOGF_ERROR("The position is not valid after an incomplete calibration. %s", CALIBRATE_HINT);
        return IPS_ALERT;
    }

    // Nothing to send for a move to where the focuser stands. The last poll is not where it ends while it moves, or
    // brakes after an abort: only skip for a motor at rest whose position, read again, is the target.
    uint32_t position = 0;
    if (targetTicks == FocusAbsPosNP[0].getValue() && FocusAbsPosNP.getState() != IPS_BUSY && !m_Esatto->isBusy()
            && m_Esatto->getAbsolutePosition(position) && position == targetTicks)
        return IPS_OK;

    if (m_Esatto->goAbsolutePosition(targetTicks) == false)
        return IPS_ALERT;

    m_MoveStartPosition = FocusAbsPosNP[0].getValue();
    m_MoveTimer.start();
    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState Esatto::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (FocusReverseSP.findOnSwitchIndex() == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosNP[0].getValue() + relativeTicks;

    return MoveAbsFocuser(newPosition);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::AbortFocuser()
{
    if (isSimulation())
        return true;

    // MOT_STOP stops the motor, but the firmware keeps the calibration running for about a minute and does not
    // count the travel of the aborted calibration: the zero position is off afterwards.
    if (CalibrationSP.getState() == IPS_BUSY)
    {
        m_ZeroInvalid = true;
        CalibrationSP.setState(IPS_ALERT);
        CalibrationSP.apply();
        LOGF_WARN("Focuser calibration aborted. Its zero position is no longer valid, moves are locked. %s",
                  CALIBRATE_HINT);
    }

    return m_Esatto->stop();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Esatto::TimerHit()
{
    //if (!isConnected() || FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    auto lastPos = FocusAbsPosNP[0].getValue();
    bool rc = updatePosition();
    if (rc)
    {
        const bool moved = std::abs(lastPos - FocusAbsPosNP[0].getValue()) > 0;
        const bool finished = (FocusAbsPosNP.getState() == IPS_BUSY || CalibrationSP.getState() == IPS_BUSY)
                              && isMoveFinished();

        if (finished)
        {
            // To prevent reporting a bit too old position as the final one
            updatePosition();

            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusRelPosNP.apply();

            if (CalibrationSP.getState() == IPS_BUSY)
            {
                if (isCalibrationCompleted())
                {
                    m_ZeroInvalid = false;
                    CalibrationSP.setState(IPS_OK);
                    LOG_INFO("Focuser calibration complete.");
                }
                else
                {
                    m_ZeroInvalid = true;
                    CalibrationSP.setState(IPS_ALERT);
                    LOGF_ERROR("The focuser calibration did not complete. Its zero position may be off, moves are "
                               "locked. %s", CALIBRATE_HINT);
                }
                CalibrationSP.apply();
            }
        }

        if (moved || finished)
            FocusAbsPosNP.apply();
    }

    if (m_TemperatureCounter++ == TEMPERATURE_FREQUENCY)
    {
        rc = updateTemperature();

        // Only update temperature if there is a change of 0.1 or more
        if (rc && (std::abs(m_LastTemperature[TEMPERATURE_EXTERNAL] - TemperatureNP[TEMPERATURE_EXTERNAL].value) >=
                   MEASUREMENT_THRESHOLD ||
                   std::abs(m_LastTemperature[TEMPERATURE_MOTOR] - TemperatureNP[TEMPERATURE_MOTOR].value) >= MEASUREMENT_THRESHOLD ))
        {
            m_LastTemperature[TEMPERATURE_EXTERNAL] = TemperatureNP[TEMPERATURE_EXTERNAL].value;
            m_LastTemperature[TEMPERATURE_MOTOR] = TemperatureNP[TEMPERATURE_MOTOR].value;
            TemperatureNP.apply();
        }

        if (updateVoltageIn())
        {
            if (std::abs(m_LastVoltage[VOLTAGE_12V] - VoltageNP[VOLTAGE_12V].getValue()) >= MEASUREMENT_THRESHOLD ||
                    std::abs(m_LastVoltage[VOLTAGE_USB] - VoltageNP[VOLTAGE_USB].getValue()) >= MEASUREMENT_THRESHOLD)
            {
                m_LastVoltage[VOLTAGE_12V] = VoltageNP[VOLTAGE_12V].getValue();
                m_LastVoltage[VOLTAGE_USB] = VoltageNP[VOLTAGE_USB].getValue();
                VoltageNP.apply();
                if (VoltageNP[VOLTAGE_12V].getValue() < 11.0)
                    LOG_WARN("Please check 12v DC power supply is connected.");
            }
        }
        m_TemperatureCounter = 0;   // Reset the counter
    }

    SetTimer(getCurrentPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::isZeroPositionValid()
{
    if (!m_ZeroInvalid)
        return true;

    // A calibration completed since, also one started from the web interface or the hand pad
    uint32_t count = 0;
    if (m_CalibrationCountKnown && m_Esatto->getCalibrationCount(count) && count > m_CalibrationCount)
    {
        m_ZeroInvalid = false;
        LOG_INFO("The focuser was calibrated, moves are possible again.");
    }
    return !m_ZeroInvalid;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::isCalibrationCompleted()
{
    // CAL_STATUS also returns to "stop" when the firmware gives up, e.g. 120 s without reaching the sensor
    uint32_t count = 0;
    return !m_CalibrationCountKnown || (m_Esatto->getCalibrationCount(count) && count > m_CalibrationCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::isMoveFinished()
{
    if (m_Esatto->isBusy())
        return false;

    // The position stays put during the calibration, CAL_STATUS tells when it is done
    if (CalibrationSP.getState() == IPS_BUSY)
        return m_MoveTimer.elapsed() >= MOVE_START_GRACE_MS && !m_Esatto->isCalibrating();

    return FocusAbsPosNP[0].getValue() != m_MoveStartPosition || m_MoveTimer.elapsed() >= MOVE_START_GRACE_MS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::getStartupValues()
{
    // updateMaxLimit() resets the position, so it has to come first
    auto rc3 = updateMaxLimit();
    auto rc1 = updatePosition();
    uint32_t steps {0};
    auto rc2 = m_Esatto->getBacklash(steps);
    if (rc2)
        FocusBacklashNP[0].setValue(steps);

    return rc1 && rc2 && rc3;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::ReverseFocuser(bool enable)
{
    INDI_UNUSED(enable);
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::Ack()
{
    std::string response;
    m_Esatto.reset(new PrimalucaLabs::Esatto(getDeviceName(), PortFD));

    if(m_Esatto->getSerialNumber(response))
        LOGF_INFO("Serial number: %s", response.c_str());
    else
        return false;

    FirmwareTP[FIRMWARE_SN].setText(response.c_str());

    if (m_Esatto->getFirmwareVersion(response))
    {
        LOGF_INFO("Firmware version: %s", response.c_str());
        IUSaveText(&FirmwareTP[FIRMWARE_VERSION], response.c_str());
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Esatto::setConnectionParams()
{
    serialConnection->setDefaultBaudRate(serialConnection->B_115200);
    serialConnection->setWordSize(8);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::SetFocuserBacklash(int32_t steps)
{
    return m_Esatto->setBacklash(steps);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::updateMaxLimit()
{
    uint32_t maxLimit = 0;

    if (m_Esatto->getMaxPosition(maxLimit) == false)
        return false;

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
