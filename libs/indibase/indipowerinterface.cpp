/*
    Power Interface
    Copyright (C) 2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indipowerinterface.h"
#include "defaultdevice.h"
#include "basedevice.h"

#include <cstring>
#include <chrono>
#include <thread>

namespace INDI
{

PowerInterface::PowerInterface(DefaultDevice *defaultDevice) : m_defaultDevice(defaultDevice)
{
    // Initialize Power Sensors
    PowerSensorsNP[SENSOR_VOLTAGE].fill("SENSOR_VOLTAGE", "Voltage (V)", "%.2f", 0, 999, 0, 0);
    PowerSensorsNP[SENSOR_CURRENT].fill("SENSOR_CURRENT", "Current (A)", "%.2f", 0, 999, 0, 0);
    PowerSensorsNP[SENSOR_POWER].fill("SENSOR_POWER", "Power (W)", "%.2f", 0, 999, 0, 0);

    // Initialize Over Voltage Protection
    OverVoltageProtectionNP[0].fill("OVERVOLTAGE", "Max Voltage", "%.1f", 0, 999, 0, 13.8);

    // Initialize Power Off on Disconnect
    PowerOffOnDisconnectSP[0].fill("ENABLED", "Enabled", ISS_OFF);
    PowerOffOnDisconnectSP[1].fill("DISABLED", "Disabled", ISS_ON);

    // Initialize LED Control
    LEDControlSP[0].fill("ENABLED", "On", ISS_ON);
    LEDControlSP[1].fill("DISABLED", "Off", ISS_OFF);


    // Initialize Power Cycle All
    PowerCycleAllSP[0].fill("POWER_CYCLE_ON", "All On", ISS_OFF);
    PowerCycleAllSP[1].fill("POWER_CYCLE_OFF", "All Off", ISS_OFF);
}

void PowerInterface::initProperties(const char *groupName, size_t nPowerPorts, size_t nPWMPorts, size_t nVariablePorts,
                                    size_t nAutoDewPorts)
{
    // Main Control - Overall Power Sensors
    PowerSensorsNP.fill(m_defaultDevice->getDeviceName(), "POWER_SENSORS", "Sensors", groupName, IP_RO, 60, IPS_IDLE);

    // Over Voltage Protection
    OverVoltageProtectionNP.fill(m_defaultDevice->getDeviceName(), "OVERVOLTAGE_PROTECTION", "Over Voltage", groupName,
                                 IP_RW, 60, IPS_IDLE);

    // Power Off on Disconnect
    PowerOffOnDisconnectSP.fill(m_defaultDevice->getDeviceName(), "POWER_OFF_DISCONNECT", "Power Off", groupName,
                                IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // LED Control
    if (HasLEDToggle())
        LEDControlSP.fill(m_defaultDevice->getDeviceName(), "LED_CONTROL", "LEDs", groupName, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Auto Dew Control
    for (size_t i = 0; i < nAutoDewPorts; i++)
    {
        char portNum[8];
        snprintf(portNum, sizeof(portNum), "%d", static_cast<int>(i + 1));

        AutoDewSP.emplace_back(2);
        char propName[MAXINDINAME];
        char propLabel[MAXINDILABEL];
        snprintf(propName, MAXINDINAME, "AUTO_DEW_PORT_%d", static_cast<int>(i + 1));
        snprintf(propLabel, MAXINDILABEL, "Auto Dew %d", static_cast<int>(i + 1));
        AutoDewSP[i][0].fill("ENABLED", "Enabled", ISS_OFF);
        AutoDewSP[i][1].fill("DISABLED", "Disabled", ISS_ON);
        AutoDewSP[i].fill(m_defaultDevice->getDeviceName(), propName, propLabel, groupName, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    }

    // Power Cycle All
    if (powerCapability & POWER_HAS_POWER_CYCLE)
        PowerCycleAllSP.fill(m_defaultDevice->getDeviceName(), "POWER_CYCLE", "Cycle Power", groupName, IP_RW, ISR_ATMOST1, 60,
                             IPS_IDLE);

    // Initialize Power Ports (12V DC)
    for (size_t i = 0; i < nPowerPorts; i++)
    {
        char portNum[8];
        snprintf(portNum, sizeof(portNum), "%d", static_cast<int>(i + 1));

        // Power Port Switch
        PowerPortsSP.emplace_back(2);
        char propName[MAXINDINAME];
        char propLabel[MAXINDILABEL];
        snprintf(propName, MAXINDINAME, "POWER_PORT_%d", static_cast<int>(i + 1));
        snprintf(propLabel, MAXINDILABEL, "Port %d", static_cast<int>(i + 1));
        PowerPortsSP[i][0].fill("ON", "On", ISS_OFF);
        PowerPortsSP[i][1].fill("OFF", "Off", ISS_ON);
        PowerPortsSP[i].fill(m_defaultDevice->getDeviceName(), propName, propLabel, POWER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

        // Power Port Current (only if per-port current monitoring is available)
        if (HasPerPortCurrent())
        {
            PowerPortCurrentNP.emplace_back(1);
            PowerPortCurrentNP[i][0].fill("CURRENT", "Current (A)", "%.2f", 0, 999, 0, 0);
            snprintf(propName, MAXINDINAME, "POWER_CURRENT_%d", static_cast<int>(i + 1));
            PowerPortCurrentNP[i].fill(m_defaultDevice->getDeviceName(), propName, propLabel, POWER_TAB, IP_RO, 60, IPS_IDLE);
        }

        // Power Port Label
        PowerPortLabelsTP.emplace_back(1);
        PowerPortLabelsTP[i][0].fill("LABEL", "Label", ("Port " + std::string(portNum)).c_str());
        snprintf(propName, MAXINDINAME, "POWER_LABEL_%d", static_cast<int>(i + 1));
        PowerPortLabelsTP[i].fill(m_defaultDevice->getDeviceName(), propName, "Label", POWER_TAB, IP_RW, 60, IPS_IDLE);
    }

    // Initialize PWM/Dew Ports
    for (size_t i = 0; i < nPWMPorts; i++)
    {
        char portNum[8];
        snprintf(portNum, sizeof(portNum), "%d", static_cast<int>(i + 1));

        // PWM Port Switch
        PWMPortsSP.emplace_back(2);
        char propName[MAXINDINAME];
        char propLabel[MAXINDILABEL];
        snprintf(propName, MAXINDINAME, "PWM_PORT_%d", static_cast<int>(i + 1));
        snprintf(propLabel, MAXINDILABEL, "Port %d", static_cast<int>(i + 1));
        PWMPortsSP[i][0].fill("ON", "On", ISS_OFF);
        PWMPortsSP[i][1].fill("OFF", "Off", ISS_ON);
        PWMPortsSP[i].fill(m_defaultDevice->getDeviceName(), propName, propLabel, PWM_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

        // PWM Port Duty Cycle
        PWMPortDutyCycleNP.emplace_back(1);
        PWMPortDutyCycleNP[i][0].fill("DUTY_CYCLE", "Duty Cycle %", "%.0f", 0, 100, 10, 0);
        snprintf(propName, MAXINDINAME, "PWM_DUTY_%d", static_cast<int>(i + 1));
        PWMPortDutyCycleNP[i].fill(m_defaultDevice->getDeviceName(), propName, propLabel, PWM_TAB, IP_RW, 60, IPS_IDLE);

        // PWM Port Current (only if per-port current monitoring is available)
        if (HasPerPortCurrent())
        {
            PWMPortCurrentNP.emplace_back(1);
            PWMPortCurrentNP[i][0].fill("CURRENT", "Current (A)", "%.2f", 0, 999, 0, 0);
            snprintf(propName, MAXINDINAME, "PWM_CURRENT_%d", static_cast<int>(i + 1));
            PWMPortCurrentNP[i].fill(m_defaultDevice->getDeviceName(), propName, propLabel, PWM_TAB, IP_RO, 60, IPS_IDLE);
        }

        // PWM Port Label
        PWMPortLabelsTP.emplace_back(1);
        PWMPortLabelsTP[i][0].fill("LABEL", "Label", ("Dew " + std::string(portNum)).c_str());
        snprintf(propName, MAXINDINAME, "PWM_LABEL_%d", static_cast<int>(i + 1));
        PWMPortLabelsTP[i].fill(m_defaultDevice->getDeviceName(), propName, "Label", PWM_TAB, IP_RW, 60, IPS_IDLE);
    }

    // Initialize Variable Voltage Ports
    for (size_t i = 0; i < nVariablePorts; i++)
    {
        char portNum[8];
        snprintf(portNum, sizeof(portNum), "%d", static_cast<int>(i + 1));

        // Variable Port Switch
        VariablePortsSP.emplace_back(2);
        char propName[MAXINDINAME];
        char propLabel[MAXINDILABEL];
        snprintf(propName, MAXINDINAME, "VAR_PORT_%d", static_cast<int>(i + 1));
        snprintf(propLabel, MAXINDILABEL, "Port %d", static_cast<int>(i + 1));
        VariablePortsSP[i][0].fill("ON", "On", ISS_OFF);
        VariablePortsSP[i][1].fill("OFF", "Off", ISS_ON);
        VariablePortsSP[i].fill(m_defaultDevice->getDeviceName(), propName, propLabel, VARIABLE_TAB, IP_RW, ISR_1OFMANY, 60,
                                IPS_IDLE);

        // Variable Port Voltage
        VariablePortVoltsNP.emplace_back(1);
        VariablePortVoltsNP[i][0].fill("VOLTAGE", "Voltage (V)", "%.1f", 3, 12, 0.1, 5);
        snprintf(propName, MAXINDINAME, "VAR_VOLTS_%d", static_cast<int>(i + 1));
        VariablePortVoltsNP[i].fill(m_defaultDevice->getDeviceName(), propName, propLabel, VARIABLE_TAB, IP_RW, 60, IPS_IDLE);

        // Variable Port Label
        VariablePortLabelsTP.emplace_back(1);
        VariablePortLabelsTP[i][0].fill("LABEL", "Label", ("Variable " + std::string(portNum)).c_str());
        snprintf(propName, MAXINDINAME, "VAR_LABEL_%d", static_cast<int>(i + 1));
        VariablePortLabelsTP[i].fill(m_defaultDevice->getDeviceName(), propName, "Label", VARIABLE_TAB, IP_RW, 60, IPS_IDLE);
    }
}

bool PowerInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        // Define properties only if connected
        if (HasVoltageSensor() || HasOverallCurrent())
            m_defaultDevice->defineProperty(PowerSensorsNP);
        m_defaultDevice->defineProperty(OverVoltageProtectionNP);
        m_defaultDevice->defineProperty(PowerOffOnDisconnectSP);
        if (HasLEDToggle())
            m_defaultDevice->defineProperty(LEDControlSP);
        if (HasAutoDew())
        {
            for (auto &autoDew : AutoDewSP)
                m_defaultDevice->defineProperty(autoDew);
        }
        if (powerCapability & POWER_HAS_POWER_CYCLE)
            m_defaultDevice->defineProperty(PowerCycleAllSP);

        // Power Ports
        for (auto &port : PowerPortsSP)
            m_defaultDevice->defineProperty(port);
        if (HasPerPortCurrent())
        {
            for (auto &current : PowerPortCurrentNP)
                m_defaultDevice->defineProperty(current);
        }
        for (auto &label : PowerPortLabelsTP)
            m_defaultDevice->defineProperty(label);

        // PWM Ports
        for (auto &port : PWMPortsSP)
            m_defaultDevice->defineProperty(port);
        for (auto &dutyCycle : PWMPortDutyCycleNP)
            m_defaultDevice->defineProperty(dutyCycle);
        if (HasPerPortCurrent())
        {
            for (auto &current : PWMPortCurrentNP)
                m_defaultDevice->defineProperty(current);
        }
        for (auto &label : PWMPortLabelsTP)
            m_defaultDevice->defineProperty(label);

        // Variable Ports
        for (auto &port : VariablePortsSP)
            m_defaultDevice->defineProperty(port);
        for (auto &voltage : VariablePortVoltsNP)
            m_defaultDevice->defineProperty(voltage);
        for (auto &label : VariablePortLabelsTP)
            m_defaultDevice->defineProperty(label);
    }
    else
    {
        // Delete properties when disconnected
        if (HasVoltageSensor() || HasOverallCurrent())
            m_defaultDevice->deleteProperty(PowerSensorsNP);
        m_defaultDevice->deleteProperty(OverVoltageProtectionNP);
        m_defaultDevice->deleteProperty(PowerOffOnDisconnectSP);
        if (HasLEDToggle())
            m_defaultDevice->deleteProperty(LEDControlSP);
        if (HasAutoDew())
        {
            for (auto &autoDew : AutoDewSP)
                m_defaultDevice->deleteProperty(autoDew);
            AutoDewSP.clear();
        }
        if (powerCapability & POWER_HAS_POWER_CYCLE)
            m_defaultDevice->deleteProperty(PowerCycleAllSP);

        // Power Ports
        for (auto &port : PowerPortsSP)
            m_defaultDevice->deleteProperty(port);
        if (HasPerPortCurrent())
        {
            for (auto &current : PowerPortCurrentNP)
                m_defaultDevice->deleteProperty(current);
        }
        for (auto &label : PowerPortLabelsTP)
            m_defaultDevice->deleteProperty(label);

        // PWM Ports
        for (auto &port : PWMPortsSP)
            m_defaultDevice->deleteProperty(port);
        for (auto &dutyCycle : PWMPortDutyCycleNP)
            m_defaultDevice->deleteProperty(dutyCycle);
        if (HasPerPortCurrent())
        {
            for (auto &current : PWMPortCurrentNP)
                m_defaultDevice->deleteProperty(current);
        }
        for (auto &label : PWMPortLabelsTP)
            m_defaultDevice->deleteProperty(label);

        // Variable Ports
        for (auto &port : VariablePortsSP)
            m_defaultDevice->deleteProperty(port);
        for (auto &voltage : VariablePortVoltsNP)
            m_defaultDevice->deleteProperty(voltage);
        for (auto &label : VariablePortLabelsTP)
            m_defaultDevice->deleteProperty(label);

        // Clear vectors
        PowerPortsSP.clear();
        PowerPortCurrentNP.clear();
        PowerPortLabelsTP.clear();

        PWMPortsSP.clear();
        PWMPortDutyCycleNP.clear();
        PWMPortCurrentNP.clear();
        PWMPortLabelsTP.clear();

        VariablePortsSP.clear();
        VariablePortVoltsNP.clear();
        VariablePortLabelsTP.clear();
    }

    return true;
}

bool PowerInterface::processNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, m_defaultDevice->getDeviceName()) == 0)
    {
        // Over Voltage Protection
        if (OverVoltageProtectionNP.isNameMatch(name))
        {
            OverVoltageProtectionNP.update(values, names, n);
            OverVoltageProtectionNP.setState(IPS_OK);
            OverVoltageProtectionNP.apply();
            return true;
        }

        // PWM Duty Cycles
        for (size_t i = 0; i < PWMPortDutyCycleNP.size(); i++)
        {
            if (PWMPortDutyCycleNP[i].isNameMatch(name))
            {
                PWMPortDutyCycleNP[i].update(values, names, n);
                if (SetPWMPort(i, PWMPortsSP[i][0].getState() == ISS_ON, values[0]))
                {
                    PWMPortDutyCycleNP[i].setState(IPS_OK);
                    PWMPortDutyCycleNP[i].apply();
                    return true;
                }
                else
                {
                    PWMPortDutyCycleNP[i].setState(IPS_ALERT);
                    PWMPortDutyCycleNP[i].apply();
                    return false;
                }
            }
        }

        // Variable Voltage
        for (size_t i = 0; i < VariablePortVoltsNP.size(); i++)
        {
            if (VariablePortVoltsNP[i].isNameMatch(name))
            {
                VariablePortVoltsNP[i].update(values, names, n);
                if (SetVariablePort(i, VariablePortsSP[i][0].getState() == ISS_ON, values[0]))
                {
                    VariablePortVoltsNP[i].setState(IPS_OK);
                    VariablePortVoltsNP[i].apply();
                    return true;
                }
                else
                {
                    VariablePortVoltsNP[i].setState(IPS_ALERT);
                    VariablePortVoltsNP[i].apply();
                    return false;
                }
            }
        }
    }

    return false;
}

bool PowerInterface::SetAutoDewEnabled(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    return false;
}

bool PowerInterface::CyclePower()
{
    // Default implementation: cycle all power ports
    bool success = true;
    for (size_t i = 0; i < PowerPortsSP.size(); ++i)
    {
        if (!SetPowerPort(i, false)) // Turn off
            success = false;
    }
    // Small delay to ensure power off
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for (size_t i = 0; i < PowerPortsSP.size(); ++i)
    {
        if (!SetPowerPort(i, true)) // Turn on
            success = false;
    }
    return success;
}

bool PowerInterface::processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, m_defaultDevice->getDeviceName()) == 0)
    {
        // Power Off on Disconnect
        if (PowerOffOnDisconnectSP.isNameMatch(name))
        {
            PowerOffOnDisconnectSP.update(states, names, n);
            PowerOffOnDisconnectSP.setState(IPS_OK);
            PowerOffOnDisconnectSP.apply();
            return true;
        }

        // LED Control
        if (HasLEDToggle() && LEDControlSP.isNameMatch(name))
        {
            // Find current ON index before updating
            auto prevOnIndex = LEDControlSP.findOnSwitchIndex();

            // Update the switch states
            LEDControlSP.update(states, names, n);

            // Try to enable/disable LEDs
            if (SetLEDEnabled(LEDControlSP[0].getState() == ISS_ON))
            {
                LEDControlSP.setState(IPS_OK);
                LEDControlSP.apply();
                return true;
            }
            else
            {
                // Reset and restore previous state on failure
                LEDControlSP.reset();
                LEDControlSP[prevOnIndex].setState(ISS_ON);
                LEDControlSP.setState(IPS_ALERT);
                LEDControlSP.apply();
                return false;
            }
        }

        // Auto Dew Control
        for (size_t i = 0; i < AutoDewSP.size(); i++)
        {
            if (AutoDewSP[i].isNameMatch(name))
            {
                auto prevOnIndex = AutoDewSP[i].findOnSwitchIndex();
                AutoDewSP[i].update(states, names, n);
                if (SetAutoDewEnabled(i, AutoDewSP[i][0].getState() == ISS_ON))
                {
                    AutoDewSP[i].setState(IPS_OK);
                    AutoDewSP[i].apply();
                    return true;
                }
                else
                {
                    AutoDewSP[i].reset();
                    AutoDewSP[i][prevOnIndex].setState(ISS_ON);
                    AutoDewSP[i].setState(IPS_ALERT);
                    AutoDewSP[i].apply();
                    return false;
                }
            }
        }

        // Power Cycle All
        if ((powerCapability & POWER_HAS_POWER_CYCLE) && PowerCycleAllSP.isNameMatch(name))
        {
            PowerCycleAllSP.update(states, names, n);
            if (CyclePower())
            {
                PowerCycleAllSP.setState(IPS_OK);
            }
            else
            {
                PowerCycleAllSP.setState(IPS_ALERT);
            }
            PowerCycleAllSP.reset(); // Reset to OFF after action
            PowerCycleAllSP.apply();
            return true;
        }

        // Power Ports
        for (size_t i = 0; i < PowerPortsSP.size(); i++)
        {
            if (PowerPortsSP[i].isNameMatch(name))
            {
                PowerPortsSP[i].update(states, names, n);
                if (SetPowerPort(i, PowerPortsSP[i][0].getState() == ISS_ON))
                {
                    PowerPortsSP[i].setState(IPS_OK);
                    PowerPortsSP[i].apply();
                    return true;
                }
                else
                {
                    PowerPortsSP[i].setState(IPS_ALERT);
                    PowerPortsSP[i].apply();
                    return false;
                }
            }
        }

        // PWM Ports
        for (size_t i = 0; i < PWMPortsSP.size(); i++)
        {
            if (PWMPortsSP[i].isNameMatch(name))
            {
                PWMPortsSP[i].update(states, names, n);
                if (SetPWMPort(i, PWMPortsSP[i][0].getState() == ISS_ON, PWMPortDutyCycleNP[i][0].getValue()))
                {
                    PWMPortsSP[i].setState(IPS_OK);
                    PWMPortsSP[i].apply();
                    return true;
                }
                else
                {
                    PWMPortsSP[i].setState(IPS_ALERT);
                    PWMPortsSP[i].apply();
                    return false;
                }
            }
        }

        // Variable Ports
        for (size_t i = 0; i < VariablePortsSP.size(); i++)
        {
            if (VariablePortsSP[i].isNameMatch(name))
            {
                VariablePortsSP[i].update(states, names, n);
                if (SetVariablePort(i, VariablePortsSP[i][0].getState() == ISS_ON, VariablePortVoltsNP[i][0].getValue()))
                {
                    VariablePortsSP[i].setState(IPS_OK);
                    VariablePortsSP[i].apply();
                    return true;
                }
                else
                {
                    VariablePortsSP[i].setState(IPS_ALERT);
                    VariablePortsSP[i].apply();
                    return false;
                }
            }
        }
    }

    return false;
}

bool PowerInterface::processText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, m_defaultDevice->getDeviceName()) == 0)
    {
        // Power Port Labels
        for (size_t i = 0; i < PowerPortLabelsTP.size(); i++)
        {
            if (PowerPortLabelsTP[i].isNameMatch(name))
            {
                PowerPortLabelsTP[i].update(texts, names, n);
                PowerPortLabelsTP[i].setState(IPS_OK);
                PowerPortLabelsTP[i].apply();
                return true;
            }
        }

        // PWM Port Labels
        for (size_t i = 0; i < PWMPortLabelsTP.size(); i++)
        {
            if (PWMPortLabelsTP[i].isNameMatch(name))
            {
                PWMPortLabelsTP[i].update(texts, names, n);
                PWMPortLabelsTP[i].setState(IPS_OK);
                PWMPortLabelsTP[i].apply();
                return true;
            }
        }

        // Variable Port Labels
        for (size_t i = 0; i < VariablePortLabelsTP.size(); i++)
        {
            if (VariablePortLabelsTP[i].isNameMatch(name))
            {
                VariablePortLabelsTP[i].update(texts, names, n);
                VariablePortLabelsTP[i].setState(IPS_OK);
                VariablePortLabelsTP[i].apply();
                return true;
            }
        }
    }

    return false;
}

bool PowerInterface::SetPowerPort(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    return false;
}

bool PowerInterface::SetPWMPort(size_t port, bool enabled, double dutyCycle)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    INDI_UNUSED(dutyCycle);
    return false;
}

bool PowerInterface::SetVariablePort(size_t port, bool enabled, double voltage)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    INDI_UNUSED(voltage);
    return false;
}

bool PowerInterface::SetLEDEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    return false;
}

bool PowerInterface::saveConfigItems(FILE *fp)
{
    OverVoltageProtectionNP.save(fp);
    PowerOffOnDisconnectSP.save(fp);
    if (HasLEDToggle())
        LEDControlSP.save(fp);
    if (HasAutoDew())
    {
        for (auto &autoDew : AutoDewSP)
            autoDew.save(fp);
    }
    if (powerCapability & POWER_HAS_POWER_CYCLE)
        PowerCycleAllSP.save(fp);

    for (auto &label : PowerPortLabelsTP)
        label.save(fp);
    for (auto &label : PWMPortLabelsTP)
        label.save(fp);
    for (auto &label : VariablePortLabelsTP)
        label.save(fp);

    for (auto &dutyCycle : PWMPortDutyCycleNP)
        dutyCycle.save(fp);
    for (auto &voltage : VariablePortVoltsNP)
        voltage.save(fp);

    return true;
}

}
