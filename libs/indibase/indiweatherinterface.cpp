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

#include "indiweatherinterface.h"

#include "indilogger.h"

#include <cstring>

namespace INDI
{

#define getDeviceName m_defaultDevice->getDeviceName

WeatherInterface::WeatherInterface(DefaultDevice *defaultDevice) : m_defaultDevice(defaultDevice)
{
    m_UpdateTimer.callOnTimeout(std::bind(&WeatherInterface::checkWeatherUpdate, this));
    m_UpdateTimer.setSingleShot(true);
    m_UpdateTimer.setInterval(60000);
}

WeatherInterface::~WeatherInterface()
{
}

void WeatherInterface::initProperties(const char *statusGroup, const char *paramsGroup)
{
    m_ParametersGroup = paramsGroup;

    // Update Period
    // @INDI_STANDARD_PROPERTY@
    UpdatePeriodNP[0].fill("PERIOD", "Period (s)", "%.f", 0, 3600, 60, 60);
    UpdatePeriodNP.fill(getDeviceName(), "WEATHER_UPDATE", "Update", statusGroup,
                        IP_RW, 60, IPS_IDLE);

    // Refresh
    // @INDI_STANDARD_PROPERTY@
    RefreshSP[0].fill("REFRESH", "Refresh", ISS_OFF);
    RefreshSP.fill(getDeviceName(), "WEATHER_REFRESH", "Weather", statusGroup, IP_RW, ISR_ATMOST1, 0,
                   IPS_IDLE);

    // Override
    // @INDI_STANDARD_PROPERTY@
    OverrideSP[0].fill("OVERRIDE", "Override Status", ISS_OFF);
    OverrideSP.fill(getDeviceName(), "WEATHER_OVERRIDE", "Safety", statusGroup, IP_RW,
                    ISR_NOFMANY, 0, IPS_IDLE);

    // Parameters
    // @INDI_STANDARD_PROPERTY@
    ParametersNP.fill(getDeviceName(), "WEATHER_PARAMETERS", "Parameters", paramsGroup, IP_RO, 60, IPS_OK);

    // Weather Status
    // @INDI_STANDARD_PROPERTY@
    critialParametersLP.fill(getDeviceName(), "WEATHER_STATUS", "Status", statusGroup, IPS_IDLE);

    // Safety Status (standard property for safety monitoring)
    // @INDI_STANDARD_PROPERTY@
    SafetyStatusLP[0].fill("SAFETY", "Safety", IPS_IDLE);
    SafetyStatusLP.fill(getDeviceName(), "SAFETY_STATUS", "Safety", statusGroup, IPS_IDLE);
}

bool WeatherInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        m_defaultDevice->defineProperty(UpdatePeriodNP);
        m_defaultDevice->defineProperty(RefreshSP);
        m_defaultDevice->defineProperty(OverrideSP);

        if (critialParametersLP.count() > 0)
            m_defaultDevice->defineProperty(critialParametersLP);

        m_defaultDevice->defineProperty(SafetyStatusLP);

        if (ParametersNP.count() > 0)
            m_defaultDevice->defineProperty(ParametersNP);

        for (auto &oneProperty : ParametersRangeNP)
            m_defaultDevice->defineProperty(oneProperty);

        checkWeatherUpdate();
    }
    else
    {
        m_defaultDevice->deleteProperty(UpdatePeriodNP);
        m_defaultDevice->deleteProperty(RefreshSP);
        m_defaultDevice->deleteProperty(OverrideSP);

        if (critialParametersLP.count() > 0)
            m_defaultDevice->deleteProperty(critialParametersLP);

        m_defaultDevice->deleteProperty(SafetyStatusLP);

        if (ParametersNP.count() > 0)
            m_defaultDevice->deleteProperty(ParametersNP);

        if (ParametersRangeNP.size() > 0)
        {
            for (auto &oneProperty : ParametersRangeNP)
                m_defaultDevice->deleteProperty(oneProperty);
        }
    }

    return true;
}

void WeatherInterface::checkWeatherUpdate()
{
    if (!m_defaultDevice->isConnected())
        return;

    IPState state = updateWeather();

    switch (state)
    {
        // Ok
        case IPS_OK:

            if (syncCriticalParameters())
            {
                // Override weather state if required
                if (OverrideSP[0].getState() == ISS_ON)
                {
                    critialParametersLP.setState(IPS_OK);

                    // Update SafetyStatusLP to match override status (only if different)
                    if (SafetyStatusLP.getState() != IPS_OK)
                    {
                        SafetyStatusLP.setState(IPS_OK);
                        SafetyStatusLP.apply();
                    }
                }

                critialParametersLP.apply();
            }

            ParametersNP.setState(state);
            ParametersNP.apply();

            // If update period is set, then set up the timer
            if (UpdatePeriodNP[0].getValue() > 0)
                m_UpdateTimer.start(UpdatePeriodNP[0].getValue() * 1000);

            return;

        // Alert
        // We retry every 5000 ms until we get OK
        case IPS_ALERT:
            ParametersNP.setState(state);
            ParametersNP.apply();
            break;

        // Weather update is in progress
        default:
            break;
    }

    m_UpdateTimer.start(5000);
}

bool WeatherInterface::processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    INDI_UNUSED(dev);

    // Refresh
    if (RefreshSP.isNameMatch(name))
    {
        RefreshSP[0].setState(ISS_OFF);
        RefreshSP.setState(IPS_OK);
        RefreshSP.apply();

        checkWeatherUpdate();
        return true;
    }

    // Override
    if (OverrideSP.isNameMatch(name))
    {
        OverrideSP.update(states, names, n);
        if (OverrideSP[0].getState() == ISS_ON)
        {
            LOG_WARN("Weather override is enabled. Observatory is not safe. Turn off override as soon as possible.");
            OverrideSP.setState(IPS_BUSY);
            critialParametersLP.setState(IPS_OK);
            critialParametersLP.apply();

            // Update SafetyStatusLP to match override status (only if different)
            if (SafetyStatusLP.getState() != IPS_OK)
            {
                SafetyStatusLP.setState(IPS_OK);
                SafetyStatusLP.apply();
            }
        }
        else
        {
            LOG_INFO("Weather override is disabled");
            OverrideSP.setState(IPS_IDLE);

            syncCriticalParameters();
            critialParametersLP.apply();
        }

        OverrideSP.apply();
        return true;
    }

    return false;
}

bool WeatherInterface::processNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INDI_UNUSED(dev);

    // Update period
    if (UpdatePeriodNP.isNameMatch(name))
    {
        UpdatePeriodNP.update(values, names, n);
        UpdatePeriodNP.setState(IPS_OK);
        UpdatePeriodNP.apply();

        if (UpdatePeriodNP[0].getValue() == 0)
            LOG_INFO("Periodic updates are disabled.");
        else
        {
            m_UpdateTimer.setInterval(UpdatePeriodNP[0].getValue() * 1000);
            m_UpdateTimer.start();
        }
        return true;
    }
    else
    {
        // Parameter ranges
        for (auto &oneRange : ParametersRangeNP)
        {
            if (oneRange.isNameMatch(name))
            {
                oneRange.update(values, names, n);

                if (syncCriticalParameters())
                    critialParametersLP.apply();

                oneRange.setState(IPS_OK);
                oneRange.apply();
                m_defaultDevice->saveConfig(oneRange);
                return true;
            }
        }
    }

    return false;
}

IPState WeatherInterface::updateWeather()
{
    LOG_ERROR("updateWeather() must be implemented in Weather device child class to update weather properties.");
    return IPS_ALERT;
}

/**
 * @brief Add a physical weather measurable parameter to the weather driver.
 *
 * The weather value has three zones:
 * - OK: Set minimum and maximum values for acceptable values.
 * - Warning: Set minimum and maximum values for values outside of Ok range and in the dangerous warning zone.
 * - Alert: Any value outside of Ok and Warning zone is marked as Alert.
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
 * @param flipWarning boolean indicating if range warning should be flipped to in-bounds, rather than out-of-bounds
 */
void WeatherInterface::addParameter(std::string name, std::string label, double numMinOk, double numMaxOk,
                                    double percWarning, bool flipWarning)
{
    LOGF_DEBUG("Parameter %s is added. Ok (%.2f,%.2f,%.2f,%s) ", name.c_str(), numMinOk, numMaxOk, percWarning,
               (flipWarning ? "true" : "false"));

    INDI::WidgetNumber oneParameter;
    oneParameter.fill(name.c_str(), label.c_str(), "%.2f", numMinOk, numMaxOk, 0, 0);
    ParametersNP.push(std::move(oneParameter));

    if (numMinOk != numMaxOk)
        createParameterRange(name, label, numMinOk, numMaxOk, percWarning, flipWarning);
}

/**
 * @brief Update weather parameter value
 * @param name name of weather parameter
 * @param value new value of weather parameter
 */
void WeatherInterface::setParameterValue(std::string name, double value)
{
    auto oneParameter = ParametersNP.findWidgetByName(name.c_str());
    if (oneParameter)
        oneParameter->setValue(value);
}

/**
 * @brief Set parameter that is considered critical to the operation of the observatory.
 *
 * The parameter state can affect the overall weather driver state which signals
 * the client to take appropriate action depending on the severity of the state.
 *
 * @param name Name of critical parameter.
 * @return True if critical parameter was set, false if parameter is not found.
 */
bool WeatherInterface::setCriticalParameter(std::string name)
{
    auto oneParameter = ParametersNP.findWidgetByName(name.c_str());
    if (!oneParameter)
    {
        LOGF_WARN("Unable to find parameter %s in list of existing parameters!", name.c_str());
        return false;
    }

    INDI::WidgetLight oneLight;
    oneLight.fill(name.c_str(), oneParameter->getLabel(), IPS_IDLE);
    critialParametersLP.push(std::move(oneLight));
    return true;
}

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
 * @param name Name of parameter to check
 * @return IPS_IDLE if parameter not found, otherwise the state based on parameter value
 */
IPState WeatherInterface::checkParameterState(const std::string &name) const
{
    auto oneRange = std::find_if(ParametersRangeNP.begin(), ParametersRangeNP.end(), [name](auto & oneElement)
    {
        return oneElement.isNameMatch(name);
    });

    auto oneParameter = ParametersNP.findWidgetByName(name.c_str());
    if (!oneParameter)
        return IPS_IDLE;

    // If parameter is found but not in range, then it is a critical parameter
    if (oneRange == ParametersRangeNP.end())
    {
        if (oneParameter->getMin() == 0 && oneParameter->getMax() == 0 && oneParameter->getValue() != 0)
            return IPS_ALERT;
        else
            return IPS_IDLE;
    }

    auto minLimit = (*oneRange)[MIN_OK].getValue();
    auto maxLimit = (*oneRange)[MAX_OK].getValue();
    auto percentageWarning = (*oneRange)[PERCENT_WARNING].getValue();
    auto rangeWarn = (maxLimit - minLimit) * (percentageWarning / 100);
    auto flipRangeTest = (*oneRange)[FLIP_RANGE_TEST].getValue();

    auto value = oneParameter->getValue();

    if (flipRangeTest)
    {
        // flipped original range test, namely: outside limits = OK; outside percentage bounds = warning; inside percentage bounds = danger
        if (value < minLimit || value > maxLimit)
            return IPS_OK;
        else if (((value < (minLimit + rangeWarn)) && minLimit != 0) || ((value > (maxLimit - rangeWarn)) && maxLimit != 0))
            return IPS_BUSY;
        else
            return IPS_ALERT;
    }
    else
    {
        // originally there was only this range test, namely: outside limits = danger; outside percentage bounds = warning; inside percentage bounds = OK
        if (value < minLimit || value > maxLimit)
            return IPS_ALERT;
        else if (((value < (minLimit + rangeWarn)) && minLimit != 0) || ((value > (maxLimit - rangeWarn)) && maxLimit != 0))
            return IPS_BUSY;
        else
            return IPS_OK;
    }

    return IPS_IDLE;
}

/**
 * @brief Synchronize and update the state of all critical parameters
 *
 * This method checks the state of all critical parameters and updates their
 * individual states as well as the overall state of the weather system.
 * The overall state is determined by the worst individual state.
 *
 * @return True if any parameter state changed, false otherwise
 */
bool WeatherInterface::syncCriticalParameters()
{
    if (critialParametersLP.count() == 0)
        return false;

    std::vector<IPState> preStates(critialParametersLP.count());
    for (size_t i = 0; i < critialParametersLP.count(); i++)
        preStates[i] = critialParametersLP[i].getState();

    critialParametersLP.setState(IPS_IDLE);

    for (auto &oneCriticalParam : critialParametersLP)
    {
        auto oneParameter = ParametersNP.findWidgetByName(oneCriticalParam.getName());
        if (!oneParameter)
            continue;

        auto state = checkParameterState(oneCriticalParam.getName());

        switch (state)
        {
            case IPS_BUSY:
                oneCriticalParam.setState(IPS_BUSY);
                LOGF_WARN("Warning: Parameter %s value (%.2f) is in the warning zone!", oneParameter->getLabel(), oneParameter->getValue());
                break;

            case IPS_ALERT:
                oneCriticalParam.setState(IPS_ALERT);
                LOGF_WARN("Caution: Parameter %s value (%.2f) is in the danger zone!", oneParameter->getLabel(), oneParameter->getValue());
                break;

            case IPS_IDLE:
            case IPS_OK:
                oneCriticalParam.setState(IPS_OK);
        }

        // The overall state is the worst individual state.
        if (oneCriticalParam.getState() > critialParametersLP.getState())
            critialParametersLP.setState(oneCriticalParam.getState());
    }

    // Update the SafetyStatusLP to mirror the overall critical parameters state (only if different)
    if (SafetyStatusLP.getState() != critialParametersLP.getState())
    {
        SafetyStatusLP.setState(critialParametersLP.getState());
        SafetyStatusLP.apply();
    }

    // if Any state changed, return true.
    for (size_t i = 0; i < critialParametersLP.count(); i++)
    {
        if (preStates[i] != critialParametersLP[i].getState())
            return true;
    }

    return false;
}

/**
 * @brief Create a parameter range with min/max values and warning percentage
 *
 * This method creates a property for configuring the acceptable ranges and warning
 * thresholds for a weather parameter. It includes:
 * - Minimum and maximum values for the "OK" range
 * - Percentage for warning zone calculation
 * - Flip warning flag to invert the alert logic
 *
 * @param name Name of parameter
 * @param label Label of parameter (in GUI)
 * @param numMinOk Minimum OK range value
 * @param numMaxOk Maximum OK range value
 * @param percWarning Percentage for warning zone calculation
 * @param flipWarning Boolean indicating if range warning should be flipped
 */
void WeatherInterface::createParameterRange(std::string name, std::string label, double numMinOk, double numMaxOk,
        double percWarning, bool flipWarning)
{
    INDI::WidgetNumber minWidget, maxWidget, warnWidget, typeWidget;
    minWidget.fill("MIN_OK", "OK range min", "%.2f", -1e6, 1e6, 0, numMinOk);
    maxWidget.fill("MAX_OK", "OK range max", "%.2f", -1e6, 1e6, 0, numMaxOk);
    warnWidget.fill("PERC_WARN", "% for Warning", "%.f", 0, 100, 5, percWarning);
    typeWidget.fill("ALERT_TYPE", "Flip alerting to in-bounds", "%.f", 0, 1, 1, (flipWarning ? 1 : 0));

    INDI::PropertyNumber oneRange {0};
    oneRange.push(std::move(minWidget));
    oneRange.push(std::move(maxWidget));
    oneRange.push(std::move(warnWidget));
    oneRange.push(std::move(typeWidget));
    oneRange.fill(getDeviceName(), name.c_str(), label.c_str(), m_ParametersGroup.c_str(), IP_RW, 60, IPS_IDLE);

    ParametersRangeNP.push_back(std::move(oneRange));
}

/**
 * @brief Save parameters ranges in the config file
 *
 * This method saves the update period and all parameter ranges to the configuration file.
 *
 * @param fp Pointer to open config file
 * @return True if successful, false otherwise
 */
bool WeatherInterface::saveConfigItems(FILE *fp)
{
    UpdatePeriodNP.save(fp);
    for (auto &oneRange : ParametersRangeNP)
        oneRange.save(fp);
    return true;
}


}
