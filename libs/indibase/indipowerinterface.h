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
            POWER_HAS_DC_OUT            = 1 << 0,  /*!< Has 12V DC outputs */
            POWER_HAS_PWM_OUT           = 1 << 1,  /*!< Has PWM outputs for dew heaters */
            POWER_HAS_VARIABLE_OUT      = 1 << 2,  /*!< Has variable voltage outputs */
            POWER_HAS_VOLTAGE_SENSOR    = 1 << 3,  /*!< Has voltage monitoring */
            POWER_HAS_OVERALL_CURRENT   = 1 << 4,  /*!< Has overall current monitoring */
            POWER_HAS_PER_PORT_CURRENT  = 1 << 5,  /*!< Has per-port current monitoring */
            POWER_HAS_LED_TOGGLE        = 1 << 6   /*!< Can toggle power LEDs */
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
         * @return True if power device has PWM outputs for dew heaters
         */
        bool HasPWMOutput()
        {
            return powerCapability & POWER_HAS_PWM_OUT;
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

    protected:
        explicit PowerInterface(DefaultDevice *defaultDevice);

        /**
         * \brief Initialize Power properties. It is recommended to call this function within
         * initProperties() of your primary device
         * \param groupName Group or tab name to be used to define Power properties.
         * \param nPowerPorts Number of 12V power ports
         * \param nPWMPorts Number of PWM/Dew heater ports
         * \param nVariablePorts Number of variable voltage ports
         */
        void initProperties(const char *groupName, size_t nPowerPorts = 0, size_t nPWMPorts = 0, size_t nVariablePorts = 0);

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
         * @brief SetPWMPort Set PWM/Dew port duty cycle
         * @param port Port number starting from 0
         * @param enabled True to turn on, false to turn off
         * @param dutyCycle Duty cycle value 0-100
         * @return True if successful, false otherwise
         */
        virtual bool SetPWMPort(size_t port, bool enabled, double dutyCycle);

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

        // Power Ports (12V DC)
        std::vector<INDI::PropertySwitch> PowerPortsSP;      // On/Off switches
        std::vector<INDI::PropertyNumber> PowerPortCurrentNP; // Current sensors (if per-port current monitoring available)
        std::vector<INDI::PropertyText> PowerPortLabelsTP;    // Custom labels

        // PWM/Dew Ports
        std::vector<INDI::PropertySwitch> PWMPortsSP;        // On/Off switches
        std::vector<INDI::PropertyNumber> PWMPortDutyCycleNP; // Duty cycle control
        std::vector<INDI::PropertyNumber> PWMPortCurrentNP;   // Current sensors (if per-port current monitoring available)
        std::vector<INDI::PropertyText> PWMPortLabelsTP;      // Custom labels

        // Variable Voltage Ports
        std::vector<INDI::PropertySwitch> VariablePortsSP;    // On/Off switches
        std::vector<INDI::PropertyNumber> VariablePortVoltsNP; // Voltage control
        std::vector<INDI::PropertyText> VariablePortLabelsTP;  // Custom labels

        // Over Voltage Protection
        INDI::PropertyNumber OverVoltageProtectionNP {1};

        // Power Off on Disconnect
        INDI::PropertySwitch PowerOffOnDisconnectSP {2};

        // LED Control
        INDI::PropertySwitch LEDControlSP {2};

        uint32_t powerCapability = 0;
        DefaultDevice *m_defaultDevice { nullptr };

        const char *POWER_TAB {"Power"};
        const char *PWM_TAB {"PWM"};
        const char *VARIABLE_TAB {"Variable"};
};

}
