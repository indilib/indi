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

Esatto::Esatto()
{
    setVersion(1, 0);

    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_HAS_BACKLASH | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::initProperties()
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
    VoltageNP[VOLTAGE_12V].fill("VOLTAGE_12V", "12v", "%.2f", 0, 100, 0., 0.);
    VoltageNP[VOLTAGE_USB].fill("VOLTAGE_USB", "USB", "%.2f", 0, 100, 0., 0.);
    VoltageNP.fill(getDeviceName(), "VOLTAGE_IN", "Voltage in", ENVIRONMENT_TAB, IP_RO, 0, IPS_IDLE);

    // Focuser temperature
    TemperatureNP[TEMPERATURE_MOTOR].fill("TEMPERATURE", "Motor (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP[TEMPERATURE_EXTERNAL].fill("TEMPERATURE_ETX", "External (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", ENVIRONMENT_TAB, IP_RO, 0, IPS_IDLE);

    // Speed Moves
    FastMoveSP[FASTMOVE_IN].fill("FASTMOVE_IN", "Move In", ISS_OFF);
    FastMoveSP[FASTMOVE_OUT].fill("FASTMOVE_OUT", "Move out", ISS_OFF);
    FastMoveSP[FASTMOVE_STOP].fill("FASTMOVE_STOP", "Stop", ISS_OFF);
    FastMoveSP.fill(getDeviceName(), "FAST_MOVE", "Calibration Move", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Override the default Max. Position to make it Read-Only
    IUFillNumberVector(&FocusMaxPosNP, FocusMaxPosN, 1, getDeviceName(), "FOCUS_MAX", "Max. Position", MAIN_CONTROL_TAB, IP_RO,
                       0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 50000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1000;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 200000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 10000;

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
    }
    else
    {
        if (TemperatureNP.getState() == IPS_OK)
            deleteProperty(TemperatureNP);
        deleteProperty(FirmwareTP);
        deleteProperty(VoltageNP.getName());
    }

    return true;
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
    if (m_Esatto->getExternalTemp(temperature))
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
bool Esatto::updatePosition()
{
    uint32_t steps = 0;
    if (isSimulation())
        steps = static_cast<uint32_t>(FocusAbsPosN[0].value);
    else if (m_Esatto->getAbsolutePosition(steps) == false)
        return false;

    FocusAbsPosN[0].value = steps;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Esatto::updateVoltageIn()
{
    double voltage;
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
bool Esatto::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Fast motion
        if (FastMoveSP.isNameMatch(name))
        {
            FastMoveSP.update(states, names, n);
            auto current_switch = FastMoveSP.findOnSwitchIndex();

            switch (current_switch)
            {
                case FASTMOVE_IN:
                    m_Esatto->fastMoveIn();
                    break;
                case FASTMOVE_OUT:
                    m_Esatto->fastMoveOut();
                    break;
                case FASTMOVE_STOP:
                    m_Esatto->stop();
                    break;
                default:
                    break;
            }

            FastMoveSP.setState(IPS_BUSY);
            FastMoveSP.apply();
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState Esatto::MoveAbsFocuser(uint32_t targetTicks)
{
    if (m_Esatto->goAbsolutePosition(targetTicks) == false)
        return IPS_ALERT;

    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState Esatto::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
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
bool Esatto::AbortFocuser()
{
    if (isSimulation())
        return true;

    return m_Esatto->stop();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Esatto::TimerHit()
{
    //if (!isConnected() || FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    auto lastPos = FocusAbsPosN[0].value;
    bool rc = updatePosition();
    if (rc && (std::abs(lastPos - FocusAbsPosN[0].value) > 0))
    {
        if (FocusAbsPosNP.s == IPS_BUSY && m_Esatto->isBusy() == false)
        {
            // To prevent reporting a bit too old position as the final one
            updatePosition();

            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusRelPosNP, nullptr);
        }

        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    if (m_TemperatureCounter++ == TEMPERATURE_FREQUENCY)
    {
        rc = updateTemperature();

        // Only update temperature if there is a change of 0.1 or more
        if (rc && (std::abs(m_LastTemperature[TEMPERATURE_EXTERNAL] - TemperatureNP[TEMPERATURE_EXTERNAL].value) >= MEASUREMENT_THRESHOLD ||
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
bool Esatto::getStartupValues()
{
    auto rc1 = updatePosition();
    uint32_t steps {0};
    auto rc2 = m_Esatto->getBacklash(steps);
    if (rc2)
        FocusBacklashN[0].value = steps;

    auto rc3 = updateMaxLimit();

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
