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

#pragma once

#include "indibase.h"
#include "indipropertyswitch.h"
#include "indipropertynumber.h"
#include "indipropertytext.h"
#include <stdint.h>

using PI = INDI::PowerInterface;

namespace INDI
{

class PowerInterface
{
    public:
        /**
         * \struct PowerCapability
         * \brief Holds the capabilities of a Power device
         */
        enum
        {
            POWER_HAS_DC_OUT                    = 1 << 0,  /*!< Has 12V DC outputs */
            POWER_HAS_DEW_OUT                   = 1 << 1,  /*!< Has DEW outputs for dew heaters */
            POWER_HAS_VARIABLE_OUT              = 1 << 2,  /*!< Has variable voltage outputs */
            POWER_HAS_VOLTAGE_SENSOR            = 1 << 3,  /*!< Has voltage monitoring */
            POWER_HAS_OVERALL_CURRENT           = 1 << 4,  /*!< Has overall current monitoring */
            POWER_HAS_PER_PORT_CURRENT          = 1 << 5,  /*!< Has per-port current monitoring */
            POWER_HAS_LED_TOGGLE                = 1 << 6,  /*!< Can toggle power LEDs */
            POWER_HAS_AUTO_DEW                  = 1 << 7,  /*!< Has automatic dew control */
            POWER_HAS_POWER_CYCLE               = 1 << 8,  /*!< Can cycle power to all ports */
            POWER_HAS_USB_TOGGLE                = 1 << 9,  /*!< Can toggle power to specific USB ports */
            POWER_HAS_OVER_VOTALGE_PROTECTION   = 1 << 10, /*!< Do not toggle output DC ports if voltage exceeds a threshold */
            POWER_OFF_ON_DISCONNECT             = 1 << 11, /*!< Power off all DC, Dew, and Variable ports when driver is disconnected*/
        } PowerCapability;

        /**
         * @brief GetPowerCapability returns the capability of the Power device
         */
        uint32_t GetCapability() const
        {
            return powerCapability;
        }

        /**
         * @brief SetPowerCapability sets the Power capabilities. All capabilities must be initialized.
         * @param cap pointer to Power capability struct.
         */
        void SetCapability(uint32_t cap)
        {
            powerCapability = cap;
        }

        /**
         * @return True if power device has 12V DC outputs
         */
        bool HasDCOutput()
        {
            return powerCapability & POWER_HAS_DC_OUT;
        }

        /**
         * @return True if power device has DEW outputs for dew heaters
         */
        bool HasDewOutput()
        {
            return powerCapability & POWER_HAS_DEW_OUT;
        }

        /**
         * @return True if power device has variable voltage outputs
         */
        bool HasVariableOutput()
        {
            return powerCapability & POWER_HAS_VARIABLE_OUT;
        }

        /**
         * @return True if power device has voltage monitoring
         */
        bool HasVoltageSensor()
        {
            return powerCapability & POWER_HAS_VOLTAGE_SENSOR;
        }

        /**
         * @return True if power device has overall current monitoring
         */
        bool HasOverallCurrent()
        {
            return powerCapability & POWER_HAS_OVERALL_CURRENT;
        }

        /**
         * @return True if power device has per-port current monitoring
         */
        bool HasPerPortCurrent()
        {
            return powerCapability & POWER_HAS_PER_PORT_CURRENT;
        }

        /**
         * @return True if power device can toggle power LEDs
         */
        bool HasLEDToggle()
        {
            return powerCapability & POWER_HAS_LED_TOGGLE;
        }

        /**
         * @return True if power device has automatic dew control
         */
        bool HasAutoDew()
        {
            return powerCapability & POWER_HAS_AUTO_DEW;
        }

        /**
         * @return True if power device can toggle power to specific USB ports
         */
        bool HasUSBPort()
        {
            return powerCapability & POWER_HAS_USB_TOGGLE;
        }

        /**
         * @return True if DC ports can only be toggled if the input voltage is within a certain threshold.
         * @note This applies to *unregulated* power controller where the output DC voltage matches the input DC voltage
         */
        bool HasOverVoltageProtection()
        {
            return powerCapability & POWER_HAS_OVER_VOTALGE_PROTECTION;
        }

        /**
         * @return True if all power should be toggled off when driver is connected.
         * @note Default behavior is power output does not change on driver disconnect.
         */
        bool ShouldPowerOffOnDisconnect()
        {
            return powerCapability & POWER_OFF_ON_DISCONNECT;
        }

    protected:
        explicit PowerInterface(DefaultDevice *defaultDevice);

        /**
         * \brief Initialize Power properties. It is recommended to call this function within
         * initProperties() of your primary device
         * \param groupName Group or tab name to be used to define Power properties.
         * \param nPowerPorts Number of 12V power ports
         * \param nDewPorts Number of DEW/Dew heater ports
         * \param nVariablePorts Number of variable voltage ports
         * \param nAutoDewPorts Number of Auto Dew ports
         * \param nUSBPorts Number of USB ports
         */
        void initProperties(const char *groupName, size_t nPowerPorts = 0, size_t nDewPorts = 0, size_t nVariablePorts = 0,
                            size_t nAutoDewPorts = 0, size_t nUSBPorts = 0);

        /**
         * @brief updateProperties Define or Delete Power properties based on the connection status of the base device
         * @return True if successful, false otherwise.
         */
        bool updateProperties();

        /** \brief Process Power number properties */
        bool processNumber(const char *dev, const char *name, double values[], char *names[], int n);

        /** \brief Process Power switch properties */
        bool processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        /** \brief Process Power text properties */
        bool processText(const char *dev, const char *name, char *texts[], char *names[], int n);

        /**
         * @brief SetPowerPort Set power port on/off
         * @param port Port number starting from 0
         * @param enabled True to turn on, false to turn off
         * @return True if successful, false otherwise
         */
        virtual bool SetPowerPort(size_t port, bool enabled);

        /**
         * @brief SetDewPort Set DEW/Dew port duty cycle
         * @param port Port number starting from 0
         * @param enabled True to turn on, false to turn off
         * @param dutyCycle Duty cycle value 0-100
         * @return True if successful, false otherwise
         */
        virtual bool SetDewPort(size_t port, bool enabled, double dutyCycle);

        /**
         * @brief SetVariablePort Set variable voltage port
         * @param port Port number starting from 0
         * @param enabled True to turn on, false to turn off
         * @param voltage Target voltage
         * @return True if successful, false otherwise
         */
        virtual bool SetVariablePort(size_t port, bool enabled, double voltage);

        /**
         * @brief SetLEDEnabled Enable/disable power LEDs
         * @param enabled True to enable LEDs, false to disable
         * @return True if successful, false otherwise
         */
        virtual bool SetLEDEnabled(bool enabled);

        /**
         * @brief SetAutoDewEnabled Enable/disable automatic dew control
         * @param enabled True to enable, false to disable
         * @return True if successful, false otherwise
         */
        virtual bool SetAutoDewEnabled(size_t port, bool enabled);

        /**
         * @brief CyclePower Cycle power to all ports
         * @return True if successful, false otherwise
         */
        virtual bool CyclePower();

        /**
         * @brief SetUSBPort Set USB port on/off
         * @param port Port number starting from 0
         * @param enabled True to turn on, false to turn off
         * @return True if successful, false otherwise
         */
        virtual bool SetUSBPort(size_t port, bool enabled);

        /**
         * @brief saveConfigItems Save power properties in config file
         * @param fp Pointer to config file
         * @return Always return true
         */
        bool saveConfigItems(FILE *fp);

        // Power Sensors Indices
        enum
        {
            SENSOR_VOLTAGE,     /*!< Input voltage */
            SENSOR_CURRENT,     /*!< Total current draw */
            SENSOR_POWER,       /*!< Total power consumption */
            N_POWER_SENSORS     /*!< Number of power sensors */
        };

        // Main Control - Overall Power Sensors
        INDI::PropertyNumber PowerSensorsNP {N_POWER_SENSORS};  // Voltage, Total Current, Total Power

        // Power Channels (12V DC)
        INDI::PropertySwitch PowerChannelsSP {0};                 // On/Off switches
        INDI::PropertyNumber PowerChannelCurrentNP {0}; // Current sensors (if per-channel current monitoring available)
        INDI::PropertyText PowerChannelLabelsTP {0};    // Custom labels

        // DEW/Dew Channels
        INDI::PropertySwitch DewChannelsSP {0};        // On/Off switches
        INDI::PropertyNumber DewChannelDutyCycleNP {0}; // Duty cycle control
        INDI::PropertyNumber DewChannelCurrentNP {0};   // Current sensors (if per-channel current monitoring available)
        INDI::PropertyText DewChannelLabelsTP {0};      // Custom labels

        // Variable Voltage Channels
        INDI::PropertySwitch VariableChannelsSP {0};    // On/Off switches
        INDI::PropertyNumber VariableChannelVoltsNP {0}; // Voltage control
        INDI::PropertyText VariableChannelLabelsTP {0};  // Custom labels

        // Over Voltage Protection
        INDI::PropertyNumber OverVoltageProtectionNP {1};

        // Power Off on Disconnect
        INDI::PropertySwitch PowerOffOnDisconnectSP {2};

        // LED Control
        INDI::PropertySwitch LEDControlSP {2};

        // Auto Dew Control
        INDI::PropertySwitch AutoDewSP {0};

        // USB Ports
        INDI::PropertySwitch USBPortSP {0};
        INDI::PropertyText USBPortLabelsTP {0};

        // Power Cycle All
        INDI::PropertySwitch PowerCycleAllSP {1};

        uint32_t powerCapability = 0;
        DefaultDevice *m_defaultDevice { nullptr };

        const char *POWER_TAB {"Power"};
        const char *DEW_TAB {"Dew"};
        const char *USB_TAB {"USB"};
        const char *VARIABLE_TAB {"Variable"};
};

}
