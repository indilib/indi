/*
    Weather Interface
    Copyright (C) 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertylight.h"
#include "inditimer.h"

#include <stdint.h>
#include <string>
#include <vector>

// Alias
using WI = INDI::WeatherInterface;

namespace INDI
{

/**
 * \class WeatherInterface
 * \brief Provides interface to implement weather reporting and safety monitoring functionality.
 *
 * The WeatherInterface class provides a comprehensive framework for implementing weather monitoring
 * and observatory safety systems. It allows devices to report weather conditions and determine
 * if the environment is safe for observatory operations.
 *
 * \section weather_overview Overview
 *
 * The weather functionality can be implemented in two ways:
 * - As an independent device (e.g., weather station)
 * - As weather-related reports within another device (e.g., observatory control system)
 *
 * The interface provides:
 * - Parameter management for various weather measurements
 * - Configurable safe/warning/danger zones for each parameter
 * - Critical parameter monitoring that affects overall system safety
 * - Two evaluation models for parameters (standard and flipped)
 * - Automatic periodic updates of weather conditions
 * - Manual refresh capability
 * - Safety override for emergency situations
 *
 * \section weather_parameters Weather Parameters
 *
 * Weather parameters represent physical measurements such as:
 * - Temperature
 * - Humidity
 * - Wind speed
 * - Cloud cover
 * - Rain detection
 * - Sky quality
 * - Etc.
 *
 * Each parameter has configurable ranges that determine its state:
 * - OK range: Values considered safe for operation
 * - Warning range: Values approaching unsafe conditions
 * - Danger range: Values unsafe for operation
 *
 * \section weather_models Parameter Evaluation Models
 *
 * The interface supports two models for evaluating parameters:
 *
 * 1. Standard model (default):
 *    min--+         +-low-%    high-%-+        +--max
 *         |         |                 |        |
 *         v         v                 v        v
 *         [         (                 )        ]
 *    danger     warning       good         warning   danger
 *
 * 2. Flipped model:
 *    min--+         +-low-%    high-%-+        +--max
 *         |         |                 |        |
 *         v         v                 v        v
 *         [         (                 )        ]
 *    good       warning      danger        warning   good
 *
 * The flipped model is useful for parameters where extreme values indicate good conditions
 * (e.g., cloud sensors where very low/high readings indicate clear skies).
 *
 * \section weather_implementation Implementation
 *
 * When developing a driver:
 * - For a fully independent weather device, use INDI::Weather directly
 * - To add weather functionality to an existing driver, subclass INDI::WeatherInterface
 *
 * In your driver, call the necessary weather interface functions:
 *
 * <table>
 * <tr><th>Function</th><th>Where to call it from your driver</th></tr>
 * <tr><td>WI::initProperties</td><td>initProperties()</td></tr>
 * <tr><td>WI::updateProperties</td><td>updateProperties()</td></tr>
 * <tr><td>WI::processNumber</td><td>ISNewNumber(...) Check if the property name contains WEATHER_* and then call WI::processNumber(..) for such properties</td></tr>
 * <tr><td>WI::processSwitch</td><td>ISNewSwitch(...) Check if the property name contains WEATHER_* and then call WI::processSwitch(..) for such properties</td></tr>
 * </table>
 *
 * You must implement the updateWeather() method to update the raw values of your weather parameters.
 * This method should not change the state of any property, as that is handled by the WeatherInterface.
 *
 * \section weather_example Example Implementation
 *
 * A typical implementation would:
 * 1. Initialize the interface in your driver's initProperties()
 * 2. Add parameters with addParameter()
 * 3. Set critical parameters with setCriticalParameter()
 * 4. Implement updateWeather() to update parameter values
 * 5. Forward property updates to the interface's process methods
 *
 * The INDI Pegasus Ultimate Power Box driver is a good example of an actual implementation
 * of a weather interface within an auxiliary driver.
 *
 * \author Jasem Mutlaq
 */
class WeatherInterface
{
    public:

    protected:
        explicit WeatherInterface(DefaultDevice *defaultDevice);
        virtual ~WeatherInterface();

        /**
         * \brief Initialize focuser properties. It is recommended to call this function within
         * initProperties() of your primary device
         * \param statusGroup group for status properties
         * \param paramsGroup group for parameter properties
         */
        void initProperties(const char *statusGroup, const char *paramsGroup);

        /**
         * @brief updateProperties Define or Delete Rotator properties based on the connection status of the base device
         * @return True if successful, false otherwise.
         */
        bool updateProperties();

        /** \brief Process weather number properties */
        bool processNumber(const char *dev, const char *name, double values[], char *names[], int n);

        /** \brief Process weather switch properties */
        bool processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        /**
         * @brief checkWeatherUpdate Calls updateWeather and update critical parameters accordingly.
         */
        void checkWeatherUpdate();

        /**
         * @brief updateWeather Update weather conditions from device or service. The function should
         * not change the state of any property in the device as this is handled by Weather. It
         * should only update the raw values.
         * @return Return overall state. The state should be IPS_OK if data is valid. IPS_BUSY if
         * weather update is in progress. IPS_ALERT is there is an error. The clients will only accept
         * values with IPS_OK state.
         */
        virtual IPState updateWeather();

        /**
         * @brief saveConfigItems Save parameters ranges in the config file.
         * @param fp pointer to open config file
         * @return true of success, false otherwise.
         */
        virtual bool saveConfigItems(FILE *fp);

        /**
         * @brief Add a physical weather measurable parameter to the weather driver.
         *
         * The weather value has three zones:
         * <ol>
         * <li>OK: Set minimum and maximum values for acceptable values.</li>
         * <li>Warning: Set minimum and maximum values for values outside of Ok range and in the
         * dangerous warning zone.</li>
         * <li>Alert: Any value outside of Ok and Warning zone is marked as Alert.</li>
         * </ol>
         *
         * The warning zone is calculated as a percentage of the range between min and max values.
         * For example, if min=0, max=100, and percWarning=10, then:
         * - Warning zone near min: 0-10
         * - OK zone: 10-90
         * - Warning zone near max: 90-100
         *
         * @param name Name of parameter
         * @param label Label of parameter (in GUI)
         * @param numMinOk minimum Ok range value.
         * @param numMaxOk maximum Ok range value.
         * @param percWarning percentage for Warning.
         * @param flipWarning boolean indicating if range warning should be flipped to in-bounds, rather than out-of-bounds.
         *        When true, the evaluation model is flipped so that values outside min/max limits are considered GOOD,
         *        values in warning zones are WARNING, and values in the middle zone are DANGER. This is useful for
         *        parameters where extreme values indicate good conditions.
         */
        void addParameter(std::string name, std::string label, double numMinOk, double numMaxOk, double percWarning, bool flipWarning = false);

        /**
         * @brief setCriticalParameter Set parameter that is considered critical to the operation of the
         * observatory. The parameter state can affect the overall weather driver state which signals
         * the client to take appropriate action depending on the severity of the state.
         * @param name Name of critical parameter.
         * @return True if critical parameter was set, false if parameter is not found.
         */
        bool setCriticalParameter(std::string name);

        /**
         * @brief setParameterValue Update weather parameter value
         * @param name name of weather parameter
         * @param value new value of weather parameter;
         */
        void setParameterValue(std::string name, double value);

        /**
         * @brief Checks the given parameter against the defined bounds
         *
         * This method evaluates a parameter's value against its defined ranges and determines its state.
         * The state can be one of:
         * - IPS_IDLE: The parameter is not found or not configured
         * - IPS_OK: The parameter value is in the safe zone
         * - IPS_BUSY: The parameter value is in the warning zone
         * - IPS_ALERT: The parameter value is in the danger zone
         *
         * There are two models for parameter evaluation:
         *
         * 1. Standard model (flipRangeTest = false):
         *    min--+         +-low-%    high-%-+        +--max
         *         |         |                 |        |
         *         v         v                 v        v
         *         [         (                 )        ]
         *    danger     warning       good         warning   danger
         *
         *    - Values outside min/max limits = DANGER (IPS_ALERT)
         *    - Values in warning zones = WARNING (IPS_BUSY)
         *    - Values in the middle safe zone = GOOD (IPS_OK)
         *
         * 2. Flipped model (flipRangeTest = true):
         *    min--+         +-low-%    high-%-+        +--max
         *         |         |                 |        |
         *         v         v                 v        v
         *         [         (                 )        ]
         *    good       warning      danger        warning   good
         *
         *    - Values outside min/max limits = GOOD (IPS_OK)
         *    - Values in warning zones = WARNING (IPS_BUSY)
         *    - Values in the middle zone = DANGER (IPS_ALERT)
         *
         * The flipped model is useful for parameters where values below minimum or above maximum
         * are actually good conditions (e.g., certain atmospheric measurements where extreme values
         * indicate clear conditions).
         *
         * @param param Name of parameter to check
         * @return IPS_IDLE if parameter not found, otherwise the state based on parameter value
         */
        IPState checkParameterState(const std::string &param) const;

        /**
         * @brief Synchronize and update the state of all critical parameters
         *
         * This method checks the state of all critical parameters and updates their
         * individual states as well as the overall state of the weather system.
         * The overall state is determined by the worst individual state.
         *
         * @return True if any parameter state changed, false otherwise
         */
        bool syncCriticalParameters();

        // Parameters
        INDI::PropertyNumber ParametersNP {0};

        // Parameter Ranges
        std::vector<INDI::PropertyNumber> ParametersRangeNP;
        enum
        {
            MIN_OK,
            MAX_OK,
            PERCENT_WARNING,
            FLIP_RANGE_TEST,
        };

        // Weather status
        INDI::PropertyLight critialParametersLP {0};

        // Safety status (standard property for safety monitoring)
        INDI::PropertyLight SafetyStatusLP {1};

        // Update Period
        INDI::PropertyNumber UpdatePeriodNP {1};
        // Refresh data
        INDI::PropertySwitch RefreshSP {1};

        // Override
        INDI::PropertySwitch OverrideSP {1};


    private:
        void createParameterRange(std::string name, std::string label, double numMinOk, double numMaxOk, double percWarning, bool flipWarning);
        DefaultDevice *m_defaultDevice { nullptr };
        std::string m_ParametersGroup;
        INDI::Timer m_UpdateTimer;
};
}
