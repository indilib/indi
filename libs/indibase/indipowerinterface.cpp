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
    PowerOffOnDisconnectSP[0].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    PowerOffOnDisconnectSP[1].fill("INDI_DISABLED", "Disabled", ISS_ON);

    // Initialize LED Control
    LEDControlSP[0].fill("INDI_ENABLED", "On", ISS_ON);
    LEDControlSP[1].fill("INDI_DISABLED", "Off", ISS_OFF);


    // Initialize Power Cycle All
    PowerCycleAllSP[0].fill("POWER_CYCLE_Toggle", "Toggle", ISS_OFF);
}

void PowerInterface::initProperties(const char *groupName, size_t nPowerPorts, size_t nDewPorts, size_t nVariablePorts,
                                    size_t nAutoDewPorts, size_t nUSBPorts)
{
    // Main Control - Overall Power Sensors
    PowerSensorsNP.fill(m_defaultDevice->getDeviceName(), "POWER_SENSORS", "Sensors", groupName, IP_RO, 60, IPS_IDLE);

    // Over Voltage Protection
    OverVoltageProtectionNP.fill(m_defaultDevice->getDeviceName(), "OVER_VOLTAGE_PROTECTION", "Over Voltage", groupName,
                                 IP_RW, 60, IPS_IDLE);

    // Power Off on Disconnect
    PowerOffOnDisconnectSP.fill(m_defaultDevice->getDeviceName(), "POWER_OFF_DISCONNECT", "Power Off", groupName,
                                IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // LED Control
    if (HasLEDToggle())
        LEDControlSP.fill(m_defaultDevice->getDeviceName(), "LED_CONTROL", "LEDs", groupName, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);


    // Power Cycle All
    if (powerCapability & POWER_HAS_POWER_CYCLE)
        PowerCycleAllSP.fill(m_defaultDevice->getDeviceName(), "POWER_CYCLE", "Cycle Power", groupName, IP_RW, ISR_ATMOST1, 60,
                             IPS_IDLE);


    if (nPowerPorts > 0)
    {
        // Power Channel Label
        PowerChannelLabelsTP.resize(nPowerPorts);
        for (size_t i = 0; i < nPowerPorts; i++)
        {
            char channelNum[8];
            snprintf(channelNum, sizeof(channelNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "POWER_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "Channel %d", static_cast<int>(i + 1));
            PowerChannelLabelsTP[i].fill(propName, propLabel, propLabel);
        }
        PowerChannelLabelsTP.fill(m_defaultDevice->getDeviceName(), "POWER_LABELS", "Labels", POWER_TAB,
                                  IP_RW, 60, IPS_IDLE);
        PowerChannelLabelsTP.load();

        // Initialize Power Channels (12V DC)
        PowerChannelsSP.resize(nPowerPorts);
        for (size_t i = 0; i < nPowerPorts; i++)
        {
            char channelNum[8];
            snprintf(channelNum, sizeof(channelNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "POWER_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "%s", PowerChannelLabelsTP[i].getText());
            PowerChannelsSP[i].fill(propName, propLabel, ISS_OFF);
        }
        PowerChannelsSP.fill(m_defaultDevice->getDeviceName(), "POWER_CHANNELS", "Toggle DC", POWER_TAB, IP_RW, ISR_NOFMANY,
                             60, IPS_IDLE);

        // Power Channel Current (only if per-channel current monitoring is available)

        PowerChannelCurrentNP.resize(nPowerPorts);
        for (size_t i = 0; i < nPowerPorts; i++)
        {
            char channelNum[8];
            snprintf(channelNum, sizeof(channelNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "POWER_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "%s (A)", PowerChannelLabelsTP[i].getText());
            PowerChannelCurrentNP[i].fill(propName, propLabel, "%.2f", 0, 999, 0, 0);
        }
        PowerChannelCurrentNP.fill(m_defaultDevice->getDeviceName(), "POWER_CURRENTS", "Currents", POWER_TAB, IP_RO, 60,
                                   IPS_IDLE);
    }
    else
    {
        PowerChannelsSP.resize(0);
        PowerChannelLabelsTP.resize(0);
        PowerChannelCurrentNP.resize(0);
    }

    if (nDewPorts > 0)
    {
        // DEW Channel Label
        DewChannelLabelsTP.resize(nDewPorts);
        for (size_t i = 0; i < nDewPorts; i++)
        {
            char channelNum[8];
            snprintf(channelNum, sizeof(channelNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "DEW_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "Channel %d", static_cast<int>(i + 1));
            DewChannelLabelsTP[i].fill(propName, propLabel, propLabel);
        }
        DewChannelLabelsTP.fill(m_defaultDevice->getDeviceName(), "DEW_LABELS", "Labels", DEW_TAB, IP_RW, 60,
                                IPS_IDLE);
        DewChannelLabelsTP.load();

        // Initialize DEW/Dew Channels
        DewChannelsSP.resize(nDewPorts);
        for (size_t i = 0; i < nDewPorts; i++)
        {
            char channelNum[8];
            snprintf(channelNum, sizeof(channelNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "DEW_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "%s", DewChannelLabelsTP[i].getText());
            DewChannelsSP[i].fill(propName, propLabel, ISS_OFF);
        }
        DewChannelsSP.fill(m_defaultDevice->getDeviceName(), "DEW_CHANNELS", "Toggle Dew", DEW_TAB, IP_RW, ISR_NOFMANY, 60,
                           IPS_IDLE);

        // DEW Channel Duty Cycle
        DewChannelDutyCycleNP.resize(nDewPorts);
        for (size_t i = 0; i < nDewPorts; i++)
        {
            char channelNum[8];
            snprintf(channelNum, sizeof(channelNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "DEW_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "%s (%%)", DewChannelLabelsTP[i].getText());
            DewChannelDutyCycleNP[i].fill(propName, propLabel, "%.0f", 0, 100, 10, 0);
        }
        DewChannelDutyCycleNP.fill(m_defaultDevice->getDeviceName(), "DEW_DUTY_CYCLES", "Duty Cycles", DEW_TAB, IP_RW, 60,
                                   IPS_IDLE);
    }
    else
    {
        DewChannelDutyCycleNP.resize(0);
        DewChannelsSP.resize(0);
        DewChannelLabelsTP.resize(0);
    }

    // Auto Dew Control
    if (nAutoDewPorts > 0)
    {
        // DEW Channel Current (only if per-channel current monitoring is available)
        DewChannelCurrentNP.resize(nDewPorts);
        for (size_t i = 0; i < nDewPorts; i++)
        {
            char channelNum[8];
            snprintf(channelNum, sizeof(channelNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "DEW_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "%s (A)", DewChannelLabelsTP[i].getText());
            DewChannelCurrentNP[i].fill(propName, propLabel, "%.2f", 0, 999, 0, 0);
        }
        DewChannelCurrentNP.fill(m_defaultDevice->getDeviceName(), "DEW_CURRENTS", "Currents", DEW_TAB, IP_RO,
                                 60, IPS_IDLE);

        AutoDewSP.resize(nAutoDewPorts);
        for (size_t i = 0; i < nAutoDewPorts; i++)
        {
            char portNum[8];
            snprintf(portNum, sizeof(portNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "DEW_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "%s", DewChannelLabelsTP[i].getText());
            AutoDewSP[i].fill(propName, propLabel, ISS_OFF);
        }
        if (nAutoDewPorts > 0)
        {
            AutoDewSP.fill(m_defaultDevice->getDeviceName(), "AUTO_DEW_CONTROL", "Auto Dew Control", DEW_TAB, IP_RW, ISR_NOFMANY, 60,
                           IPS_IDLE);
        }
    }
    else
    {
        AutoDewSP.resize(0);
        DewChannelCurrentNP.resize(0);
    }


    // Initialize USB Ports
    if (nUSBPorts > 0)
    {
        // USB Port Labels
        USBPortLabelsTP.resize(nUSBPorts);
        for (size_t i = 0; i < nUSBPorts; i++)
        {
            char portNum[8];
            snprintf(portNum, sizeof(portNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "USB_PORT_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "Port %d", static_cast<int>(i + 1));
            USBPortLabelsTP[i].fill(propName, propLabel, propLabel);
        }
        if (nUSBPorts > 0)
        {
            USBPortLabelsTP.fill(m_defaultDevice->getDeviceName(), "USB_LABELS", "Labels", USB_TAB, IP_RW, 60,
                                 IPS_IDLE);
            USBPortLabelsTP.load();
        }

        USBPortSP.resize(nUSBPorts);
        for (size_t i = 0; i < nUSBPorts; i++)
        {
            char portNum[8];
            snprintf(portNum, sizeof(portNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "USB_PORT_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "%s", USBPortLabelsTP[i].getText());
            USBPortSP[i].fill(propName, propLabel, ISS_OFF);
        }
        if (nUSBPorts > 0)
        {
            USBPortSP.fill(m_defaultDevice->getDeviceName(), "USB_PORTS", "Ports", USB_TAB, IP_RW, ISR_NOFMANY, 60,
                           IPS_IDLE);
        }
    }
    else
    {
        USBPortSP.resize(0);
        USBPortLabelsTP.resize(0);
    }

    // Initialize Variable Voltage Channels
    if (nVariablePorts > 0)
    {
        // Variable Channel Label
        VariableChannelLabelsTP.resize(nVariablePorts);
        for (size_t i = 0; i < nVariablePorts; i++)
        {
            char channelNum[8];
            snprintf(channelNum, sizeof(channelNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "VAR_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "Channel %d", static_cast<int>(i + 1));
            VariableChannelLabelsTP[i].fill(propName, propLabel, propLabel);
        }
        VariableChannelLabelsTP.fill(m_defaultDevice->getDeviceName(), "VARIABLE_LABELS", "Labels", VARIABLE_TAB,
                                     IP_RW, 60, IPS_IDLE);
        VariableChannelLabelsTP.load();

        VariableChannelsSP.resize(nVariablePorts);
        for (size_t i = 0; i < nVariablePorts; i++)
        {
            char channelNum[8];
            snprintf(channelNum, sizeof(channelNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "VAR_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "%s", VariableChannelLabelsTP[i].getText());
            VariableChannelsSP[i].fill(propName, propLabel, ISS_OFF);
        }
        VariableChannelsSP.fill(m_defaultDevice->getDeviceName(), "VARIABLE_CHANNELS", "Channels", VARIABLE_TAB, IP_RW,
                                ISR_NOFMANY, 60, IPS_IDLE);

        // Variable Channel Voltage
        VariableChannelVoltsNP.resize(nVariablePorts);
        for (size_t i = 0; i < nVariablePorts; i++)
        {
            char channelNum[8];
            snprintf(channelNum, sizeof(channelNum), "%d", static_cast<int>(i + 1));

            char propName[MAXINDINAME];
            char propLabel[MAXINDILABEL];
            snprintf(propName, MAXINDINAME, "VAR_CHANNEL_%d", static_cast<int>(i + 1));
            snprintf(propLabel, MAXINDILABEL, "%s (V)", VariableChannelLabelsTP[i].getText());
            VariableChannelVoltsNP[i].fill(propName, propLabel, "%.1f", 3, 12, 0.1, 5);
        }
        VariableChannelVoltsNP.fill(m_defaultDevice->getDeviceName(), "VARIABLE_VOLTAGES", "Voltages", VARIABLE_TAB, IP_RW,
                                    60, IPS_IDLE);
    }
    else
    {
        VariableChannelsSP.resize(0);
        VariableChannelVoltsNP.resize(0);
        VariableChannelLabelsTP.resize(0);
    }
}

bool PowerInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        // Define properties only if connected
        if (HasVoltageSensor() || HasOverallCurrent())
            m_defaultDevice->defineProperty(PowerSensorsNP);
        if (HasOverVoltageProtection())
            m_defaultDevice->defineProperty(OverVoltageProtectionNP);
        if (ShouldPowerOffOnDisconnect())
            m_defaultDevice->defineProperty(PowerOffOnDisconnectSP);
        if (HasLEDToggle())
            m_defaultDevice->defineProperty(LEDControlSP);
        if (HasAutoDew())
        {
            m_defaultDevice->defineProperty(AutoDewSP);
        }
        if (powerCapability & POWER_HAS_POWER_CYCLE)
            m_defaultDevice->defineProperty(PowerCycleAllSP);

        // Power Channels
        if (HasDCOutput())
        {
            m_defaultDevice->defineProperty(PowerChannelsSP);
            if (HasPerPortCurrent())
            {
                m_defaultDevice->defineProperty(PowerChannelCurrentNP);
            }
            m_defaultDevice->defineProperty(PowerChannelLabelsTP);
        }

        // DEW Channels
        if (HasDewOutput())
        {
            m_defaultDevice->defineProperty(DewChannelsSP);
            m_defaultDevice->defineProperty(DewChannelDutyCycleNP);
            if (HasPerPortCurrent())
            {
                m_defaultDevice->defineProperty(DewChannelCurrentNP);
            }
            m_defaultDevice->defineProperty(DewChannelLabelsTP);
        }

        // Variable Channels
        if (HasVariableOutput())
        {
            m_defaultDevice->defineProperty(VariableChannelsSP);
            m_defaultDevice->defineProperty(VariableChannelVoltsNP);
            m_defaultDevice->defineProperty(VariableChannelLabelsTP);
        }

        // USB Ports
        if (HasUSBPort())
        {
            m_defaultDevice->defineProperty(USBPortSP);
            m_defaultDevice->defineProperty(USBPortLabelsTP);
        }
    }
    else
    {
        // Delete properties when disconnected
        if (HasVoltageSensor() || HasOverallCurrent())
            m_defaultDevice->deleteProperty(PowerSensorsNP);
        if (HasOverVoltageProtection())
            m_defaultDevice->deleteProperty(OverVoltageProtectionNP);
        if (ShouldPowerOffOnDisconnect())
            m_defaultDevice->deleteProperty(PowerOffOnDisconnectSP);
        if (HasLEDToggle())
            m_defaultDevice->deleteProperty(LEDControlSP);
        if (HasAutoDew())
        {
            m_defaultDevice->deleteProperty(AutoDewSP);
        }
        if (powerCapability & POWER_HAS_POWER_CYCLE)
            m_defaultDevice->deleteProperty(PowerCycleAllSP);

        // Power Channels
        if (HasDCOutput())
        {
            m_defaultDevice->deleteProperty(PowerChannelsSP);
            if (HasPerPortCurrent())
            {
                m_defaultDevice->deleteProperty(PowerChannelCurrentNP);
            }
            m_defaultDevice->deleteProperty(PowerChannelLabelsTP);
        }

        // DEW Channels
        if (HasDewOutput())
        {
            m_defaultDevice->deleteProperty(DewChannelsSP);
            m_defaultDevice->deleteProperty(DewChannelDutyCycleNP);
            if (HasPerPortCurrent())
                m_defaultDevice->deleteProperty(DewChannelCurrentNP);
            m_defaultDevice->deleteProperty(DewChannelLabelsTP);
        }

        // Variable Channels
        if (HasVariableOutput())
        {
            m_defaultDevice->deleteProperty(VariableChannelsSP);
            m_defaultDevice->deleteProperty(VariableChannelVoltsNP);
            m_defaultDevice->deleteProperty(VariableChannelLabelsTP);
        }

        // USB Ports
        if (HasUSBPort())
        {
            m_defaultDevice->deleteProperty(USBPortSP);
            m_defaultDevice->deleteProperty(USBPortLabelsTP);
        }
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
            return m_defaultDevice->updateProperty(OverVoltageProtectionNP, values, names, n, []()
            {
                return true;
            }, true);
        }

        // DEW Channel Duty Cycles
        if (DewChannelDutyCycleNP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(DewChannelDutyCycleNP, values, names, n, [this, values]()
            {
                bool allSuccessful = true;
                for (size_t i = 0; i < DewChannelDutyCycleNP.size(); i++)
                {
                    // If We try to update a duty cycle while the channel is OFF, then we save as-is.
                    // Otherwise, we need to set the duty cycle on the device.
                    if (DewChannelsSP[i].getState() == ISS_ON && !SetDewPort(i, true, values[i]))
                    {
                        allSuccessful = false;
                    }
                }
                return allSuccessful;
            }, true);
        }

        // Variable Channel Voltage
        if (VariableChannelVoltsNP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(VariableChannelVoltsNP, values, names, n, [this, values]()
            {
                bool allSuccessful = true;
                for (size_t i = 0; i < VariableChannelVoltsNP.size(); i++)
                {
                    // If We try to update a voltage while the channel is OFF, then we save as-is.
                    // Otherwise, we need to set the voltage on the device.
                    if (VariableChannelsSP[i].getState() == ISS_ON && !SetVariablePort(i, true, values[i]))
                    {
                        allSuccessful = false;
                    }
                }
                return allSuccessful;
            }, true);
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
    for (size_t i = 0; i < PowerChannelsSP.size(); ++i)
    {
        if (!SetPowerPort(i, false)) // Turn off
            success = false;
    }
    // Small delay to ensure power off
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    for (size_t i = 0; i < PowerChannelsSP.size(); ++i)
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
            return m_defaultDevice->updateProperty(PowerOffOnDisconnectSP, states, names, n, []()
            {
                return true;
            }, true);
        }

        // LED Control
        if (HasLEDToggle() && LEDControlSP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(LEDControlSP, states, names, n, [this, states]()
            {
                return SetLEDEnabled(states[0] == ISS_ON);
            }, true);
        }

        // Auto Dew Control
        if (HasAutoDew() && AutoDewSP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(AutoDewSP, states, names, n, [this, states]()
            {
                bool allSuccessful = true;
                for (size_t i = 0; i < AutoDewSP.size(); i++)
                {
                    // Only change if state is different
                    if (AutoDewSP[i].getState() != states[i])
                    {
                        if (!SetAutoDewEnabled(i, states[i] == ISS_ON))
                        {
                            allSuccessful = false;
                        }
                    }
                }
                return allSuccessful;
            }, true);
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
            PowerCycleAllSP.reset();
            PowerCycleAllSP.apply();
            return true;
        }

        // Power Channels
        if (PowerChannelsSP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(PowerChannelsSP, states, names, n, [this, states]()
            {
                bool allSuccessful = true;
                for (size_t i = 0; i < PowerChannelsSP.size(); i++)
                {
                    // Only change if state is different
                    if (PowerChannelsSP[i].getState() != states[i])
                    {
                        if (!SetPowerPort(i, states[i] == ISS_ON))
                        {
                            allSuccessful = false;
                        }
                    }
                }
                return allSuccessful;
            }, true);
        }

        // DEW Channels
        if (DewChannelsSP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(DewChannelsSP, states, names, n, [this, states]()
            {
                bool allSuccessful = true;
                for (size_t i = 0; i < DewChannelsSP.size(); i++)
                {
                    // Only change if state is different
                    if (DewChannelsSP[i].getState() != states[i])
                    {
                        if (!SetDewPort(i, states[i] == ISS_ON, DewChannelDutyCycleNP[i].getValue()))
                        {
                            allSuccessful = false;
                        }
                    }
                }
                return allSuccessful;
            }, true);
        }

        // Variable Channels
        if (VariableChannelsSP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(VariableChannelsSP, states, names, n, [this, states]()
            {
                bool allSuccessful = true;
                for (size_t i = 0; i < VariableChannelsSP.size(); i++)
                {
                    if (VariableChannelsSP[i].getState() != states[i])
                    {
                        if (!SetVariablePort(i, states[i] == ISS_ON, VariableChannelVoltsNP[i].getValue()))
                        {
                            allSuccessful = false;
                        }
                    }
                }
                return allSuccessful;
            }, true);
        }

        // USB Ports
        if (HasUSBPort() && USBPortSP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(USBPortSP, states, names, n, [this, states]()
            {
                bool allSuccessful = true;
                for (size_t i = 0; i < USBPortSP.size(); i++)
                {
                    if (USBPortSP[i].getState() != states[i])
                    {
                        if (!SetUSBPort(i, states[i] == ISS_ON))
                        {
                            allSuccessful = false;
                        }
                    }
                }
                return allSuccessful;
            }, true);
        }
    }

    return false;
}

bool PowerInterface::processText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, m_defaultDevice->getDeviceName()) == 0)
    {
        // Power Channel Labels
        if (PowerChannelLabelsTP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(PowerChannelLabelsTP, texts, names, n, []()
            {
                return true;
            }, true);
        }

        // DEW Channel Labels
        if (DewChannelLabelsTP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(DewChannelLabelsTP, texts, names, n, []()
            {
                return true;
            }, true);
        }

        // Variable Channel Labels
        if (VariableChannelLabelsTP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(VariableChannelLabelsTP, texts, names, n, []()
            {
                return true;
            }, true);
        }

        // USB Port Labels
        if (HasUSBPort() && USBPortLabelsTP.isNameMatch(name))
        {
            return m_defaultDevice->updateProperty(USBPortLabelsTP, texts, names, n, []()
            {
                return true;
            }, true);
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

bool PowerInterface::SetDewPort(size_t port, bool enabled, double dutyCycle)
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

bool PowerInterface::SetUSBPort(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    return false;
}

bool PowerInterface::saveConfigItems(FILE *fp)
{
    if (HasOverVoltageProtection())
        OverVoltageProtectionNP.save(fp);
    if (ShouldPowerOffOnDisconnect())
        PowerOffOnDisconnectSP.save(fp);
    if (HasLEDToggle())
        LEDControlSP.save(fp);

    if (HasDCOutput())
    {
        PowerChannelsSP.save(fp);
        PowerChannelLabelsTP.save(fp);
    }

    if (HasAutoDew())
    {
        AutoDewSP.save(fp);
    }

    if (HasDewOutput())
    {
        DewChannelsSP.save(fp);
        DewChannelDutyCycleNP.save(fp);
        DewChannelLabelsTP.save(fp);
    }

    if (HasVariableOutput())
    {
        VariableChannelsSP.save(fp);
        VariableChannelVoltsNP.save(fp);
        VariableChannelLabelsTP.save(fp);
    }

    if (HasUSBPort())
    {
        USBPortSP.save(fp);
        USBPortLabelsTP.save(fp);
    }

    return true;
}

}
