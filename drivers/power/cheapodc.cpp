/*
    CheapoDC - Dew Controller
    https://github.com/hcomet/CheapoDC

    Copyright (C) 2024 Stephen Hillier

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

#include "cheapodc.h"
#include "connectionplugins/connectiontcp.h"
#include "connectionplugins/connectionserial.h"
#include "indicom.h"
#include <cstring>
#include <string>
#include <memory>

#include <termios.h>
#include <unistd.h>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

std::unique_ptr<CheapoDC> cheapodc(new CheapoDC());

CheapoDC::CheapoDC() : INDI::PowerInterface(this)
{
    setVersion(CHEAPODC_VERSION_MAJOR, CHEAPODC_VERSION_MINOR);
}

bool CheapoDC::initProperties()
{
    DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | POWER_INTERFACE);

    SetCapability(INDI::PowerInterface::POWER_HAS_DEW_OUT | INDI::PowerInterface::POWER_HAS_AUTO_DEW | INDI::PowerInterface::POWER_HAS_DC_OUT );

    /* Minimum Output Power */
    MinimumOutputNP[0].fill("MINIMUMOUTPUT", "Power (%)", "%3.0f", 0, 99, 1, prevMinOutput);
    MinimumOutputNP.fill(getDeviceName(), "MINIMUMOUTPUT", "Controller Min", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    /* Maximum Output Power */
    MaximumOutputNP[0].fill("MAXIMUMOUTPUT", "Power (%)", "%3.0f", 1, 100, 1, prevMaxOutput);
    MaximumOutputNP.fill(getDeviceName(), "MAXIMUMOUTPUT", "Controller Max", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    XtrnTemperatureNP[WEATHER_QUERY].fill("WEATHERQUERY", "Weather Source (\u2103)", "%3.2f", -50., 120., 0., 0.);
    XtrnTemperatureNP[EXTERNAL_INPUT].fill("EXTERNALINPUT", "Focuser Device (\u2103)", "%3.2f", -50., 120., 0., 0.);
    XtrnTemperatureNP.fill(getDeviceName(), "TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    HumidityNP[0].fill("HUMIDITY", "Relative (%)", "%3.0f", 0., 100., 0., 0.);
    HumidityNP.fill(getDeviceName(), "HUMIDITY", "Humidity", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Dew point */
    DewpointNP[0].fill("DEWPOINT", "(\u2103)", "%3.2f", -50., 120., 0., 0.);
    DewpointNP.fill(getDeviceName(), "DEWPOINT", "Dew point", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Set Point Temperature */
    SetPointTemperatureNP[0].fill("SETPOINT", "Set Point (\u2103)", "%3.2f", -50., 120., 0., 0.);
    SetPointTemperatureNP.fill(getDeviceName(), "SETPOINT", "Temperature", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    /* Track Point Offset */
    TrackPointOffsetNP[0].fill("TRACKPOINTOFFSET", "-5.0 to 5.0 (\u2103)", "%2.1f", -5., 5., 0.5, 0.);
    TrackPointOffsetNP.fill(getDeviceName(), "TRACKPOINTOFFSET", "Track Point Offset", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    /*  Tracking Range */
    TrackingRangeNP[0].fill("TRACKINGRANGE", "4.0 to 10.0 (\u2103)", "%2.1f", 4, 10, .5, 5.0);
    TrackingRangeNP.fill(getDeviceName(), "TRACKINGRANGE", "Tracking Range", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    /*  Temperature mode */
    TemperatureModeSP[WEATHER_QUERY].fill("WEATHER_QUERY", "Weather Source", ISS_ON);
    TemperatureModeSP[EXTERNAL_INPUT].fill("EXTERNAL_INPUT", "Focuser Device", ISS_OFF);
    TemperatureModeSP.fill(getDeviceName(), "TEMPERATURE_MODE", "Temperature Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /*  Set Point mode */
    SetPointModeSP[DEWPOINT].fill("DEWPOINT", "Dew Point", ISS_ON);
    SetPointModeSP[TEMPERATURE].fill("TEMPERATURE", "Temperature", ISS_OFF);
    SetPointModeSP[MIDPOINT].fill("MIDPOINT", "Midpoint", ISS_OFF);
    SetPointModeSP.fill(getDeviceName(), "SETPOINT_MODE", "Set Point Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
 
    /*  Update Output Every 1 to 20 minutes */
    UpdateOutputEveryNP[0].fill("UPDATE_PERIOD", "Period (min)", "%2.0f", 1, 20, 1, 1);
    UpdateOutputEveryNP.fill(getDeviceName(), "UPDATE_OUTPUT", "Update Output", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Query Weather Every 0 to 20 minutes */
    QueryWeatherEveryNP[0].fill("UPDATE_PERIOD", "Period (min)", "%2.0f", 0, 20, 1, 5);
    QueryWeatherEveryNP.fill(getDeviceName(), "QUERY_WEATHER", "Query Weather", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /* Firmware version */
    FWversionTP[0].fill("FIRMWARE", "Firmware Version", nullptr);
    FWversionTP.fill(getDeviceName(), "FW_VERSION", "Device", CONNECTION_TAB, IP_RO, 0, IPS_IDLE);

    /*  Set Weather Source */
    WeatherSourceSP[OPENMETEO].fill("OPENMETEO", "Open-Meteo", ISS_ON);
    WeatherSourceSP[OPENWEATHER].fill("OPENWEATHER", "OpenWeather", ISS_OFF);
    WeatherSourceSP[EXTERNALSOURCE].fill("EXTERNALSOURCE", "Weather Device", ISS_OFF);
    WeatherSourceSP[INTERNALSOURCE].fill("INTERNALSOURCE", "CheapoDC Sensor", ISS_OFF);
    WeatherSourceSP.fill(getDeviceName(), "WEATHER_SOURCE", "Weather Source", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* Weather Query API Key*/
    WeatherQueryAPIKeyTP[0].fill("API_KEY", "OpenWeather API Key", nullptr);
    WeatherQueryAPIKeyTP.fill(getDeviceName(), "WEATHER_API_KEY", "Weather API Key", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Weather Updated */
    WeatherUpdatedTP[0].fill("LAST_UPDATED", "Last Updated", nullptr);
    WeatherUpdatedTP.fill(getDeviceName(), "WEATHER_UPDATED", "Weather", OPTIONS_TAB, IP_RO, 0, IPS_IDLE);

    /* Location coordinates */
    LocationNP[LOCATION_LATITUDE].fill(CDC_SNOOP_LOCATION_LATITUDE, "Lat (dd:mm:ss)", "%010.6m", -90, 90, 0, 0.0);
    LocationNP[LOCATION_LONGITUDE].fill(CDC_SNOOP_LOCATION_LONGITUDE, "Lon (dd:mm:ss)", "%010.6m", 0, 360, 0, 0.0);
    LocationNP.fill(getDeviceName(), CDC_SNOOP_LOCATION_PROPERTY, "Location", SITE_TAB, IP_RW, 60, IPS_IDLE);

    /* Device Time */
    DeviceTimeTP[LOCAL_TIME].fill("CDC_DEVICE_TIME", "Local Time", nullptr);
    DeviceTimeTP[UTC_OFFSET].fill("UTC_OFFSET", "UTC Offset (hours)", nullptr);
    DeviceTimeTP.fill(getDeviceName(), "DEVICE_TIME", "Device Time", SITE_TAB, IP_RO, 0, IPS_IDLE);


    /* Active Devices */
    ActiveDeviceTP[ACTIVE_TELESCOPE].fill("ACTIVE_TELESCOPE", "Telescope", activeTelescopeDevice);
    ActiveDeviceTP[ACTIVE_FOCUSER].fill("ACTIVE_FOCUSER", "Focuser", activeFocuserDevice);
    ActiveDeviceTP[ACTIVE_WEATHER].fill("ACTIVE_WEATHER", "Weather", activeWeatherDevice);
    ActiveDeviceTP.fill(getDeviceName(), "ACTIVE_DEVICES", "Snoop Devices", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    ActiveDeviceTP.load();

    // Refresh
    RefreshSP[0].fill("REFRESH", "Refresh", ISS_OFF);
    RefreshSP.fill(getDeviceName(),  "CHEAPODC_REFRESH", "CheapoDC", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // setDriverInterface(AUX_INTERFACE); // Moved above

    addDebugControl();
    addConfigurationControl();
    setDefaultPollingPeriod(CDC_DEFAULT_POLLING_PERIOD);
    addPollPeriodControl();

    // No simulation control for now

    if (cdcConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->setDefaultHost(CDC_DEFAULT_HOST);
        tcpConnection->setDefaultPort(CDC_DEFAULT_PORT);
        tcpConnection->registerHandshake([&]()
                                         { return Handshake(); });

        registerConnection(tcpConnection);
    }

    return true;
}

bool CheapoDC::updateProperties()
{
    DefaultDevice::updateProperties();
    INDI::PowerInterface::updateProperties();

    if (isConnected())
    {
        // Enable Snoop Devices
        indi_strlcpy(activeTelescopeDevice, ActiveDeviceTP[ACTIVE_TELESCOPE].getText(), MAXINDINAME);
        if (strlen(activeTelescopeDevice) > 0)
        {
            IDSnoopDevice(activeTelescopeDevice, CDC_SNOOP_LOCATION_PROPERTY);
            if (!fwVOneDetected)
                IDSnoopDevice(activeTelescopeDevice, CDC_SNOOP_TIME_PROPERTY);
        }
        
        indi_strlcpy(activeFocuserDevice, ActiveDeviceTP[ACTIVE_FOCUSER].getText(), MAXINDINAME);
        if (strlen(activeFocuserDevice) > 0)
            IDSnoopDevice(activeFocuserDevice, CDC_SNOOP_FOCUSER_PROPERTY);
        
        // If fw V1 then blank out the activeWeatherDevice since it is not supported
        if (fwVOneDetected)
            ActiveDeviceTP[ACTIVE_WEATHER].setText("");

        indi_strlcpy(activeWeatherDevice, ActiveDeviceTP[ACTIVE_WEATHER].getText(), MAXINDINAME);
        if (strlen(activeWeatherDevice) > 0)
            IDSnoopDevice(activeWeatherDevice, CDC_SNOOP_WEATHER_PROPERTY);

        // Main Control Tab
        defineProperty(WeatherSourceSP);
        defineProperty(TemperatureModeSP);
        defineProperty(XtrnTemperatureNP);
        defineProperty(SetPointModeSP);
        defineProperty(HumidityNP);
        defineProperty(DewpointNP);
        defineProperty(SetPointTemperatureNP);
        defineProperty(MinimumOutputNP);
        defineProperty(MaximumOutputNP);
        defineProperty(TrackPointOffsetNP);
        defineProperty(TrackingRangeNP);
        defineProperty(RefreshSP);
        // Options Tab
        defineProperty(UpdateOutputEveryNP);
        defineProperty(QueryWeatherEveryNP);
        if (fwVOneDetected) // Set Weather Source selection to RO
        {
            WeatherSourceSP.setPermission(IP_RO);
        }
        defineProperty(WeatherQueryAPIKeyTP);
        defineProperty(WeatherUpdatedTP);
        defineProperty(ActiveDeviceTP);

        // Site Tab
        defineProperty(LocationNP);
        if (!fwVOneDetected)
            defineProperty(DeviceTimeTP);

        // Connection Tab
        defineProperty(FWversionTP);

        readSettings();
        if (fwVOneDetected)
        {
            LOG_WARN("Go to https://github.com/hcomet/CheapoDC/releases to download the latest firmware release");
            LOGF_WARN("CheapoDC firmware V%s detected. Please upgrade firmware to latest V2+.", FWversionTP[0].getText());
        }
        timerIndex = SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(WeatherSourceSP);
        deleteProperty(MinimumOutputNP);
        deleteProperty(MaximumOutputNP);
        deleteProperty(TemperatureModeSP);
        deleteProperty(SetPointModeSP);
        deleteProperty(XtrnTemperatureNP);
        deleteProperty(HumidityNP);
        deleteProperty(DewpointNP);
        deleteProperty(SetPointTemperatureNP);
        deleteProperty(RefreshSP);
        deleteProperty(TrackPointOffsetNP);
        deleteProperty(TrackingRangeNP);
        deleteProperty(UpdateOutputEveryNP);
        deleteProperty(QueryWeatherEveryNP);
        deleteProperty(WeatherQueryAPIKeyTP);
        deleteProperty(WeatherUpdatedTP);
        deleteProperty(ActiveDeviceTP);

        // Site Tab
        deleteProperty(LocationNP);
        if (!fwVOneDetected)
            deleteProperty(DeviceTimeTP);

        // Connection Tab
        deleteProperty(FWversionTP);
    }

    return true;
}

const char *CheapoDC::getDefaultName()
{
    return "CheapoDC";
}

// sleep for a number of milliseconds
int CheapoDC::msleep(long duration)
{
    struct timespec ts;
    int res;

    if (duration < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = duration / 1000;
    ts.tv_nsec = (duration % 1000) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

bool CheapoDC::sendCommand(const char *cmd, char *resp)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    LOGF_DEBUG("CMD: [%s]", cmd);
    tcflush(PortFD, TCIOFLUSH);
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error writing command %s: %s.", cmd, errstr);
        return false;
    }

    // Small delay to allow controller to process command
    msleep(CDC_SMALL_DELAY);

    if (resp)
    {
        if ((rc = tty_nread_section(PortFD, resp, CDC_RESPONSE_LENGTH, '\n', CDC_READ_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Error reading response for command [%s]: %s.", cmd, errstr);
            return false;
        }

        if (nbytes_read < 2)
        {
            LOGF_ERROR("Invalid response [%s] for command [%s].", resp, cmd);
            return false;
        }
    }

    return true;
}

bool CheapoDC::sendGetCommand(const char *cmd, char *resp)
{
    char getResponse[CDC_RESPONSE_LENGTH] = {};
    char getCommand[CDC_COMMAND_LENGTH] = {};

    snprintf(getCommand, CDC_COMMAND_LENGTH, CDC_GET_COMMAND, cmd);

    if (!this->sendCommand(getCommand, getResponse))
        return false;
    else
    {
        std::string getResponseST(getResponse, strlen(getResponse));
        try
        {
            json jsonResponse = json::parse(getResponseST);

            try
            {
                std::string jsonValue;
                jsonResponse[cmd].get_to(jsonValue);
                strcpy(resp, jsonValue.c_str());
            }
            catch (const std::exception &e)
            {
                try
                {
                    int errorCode;
                    jsonResponse["RESULT"].get_to(errorCode);
                    LOGF_DEBUG("GET command %s not supported.", cmd);
                    return false;
                }
                catch(const std::exception& e)
                {
                    LOGF_ERROR("Error parsing GET %s response for value: %s Error: %s", cmd, getResponse, e.what());
                    return false;
                }
                
            }
        }
        catch (json::exception &e)
        {
            // output exception information
            LOGF_ERROR("Error parsing GET %s response %s Error: %s", cmd, getResponseST.c_str(), e.what());
            return false;
        }
    }
    return true;
}

bool CheapoDC::sendSetCommand(const char *cmd, char *value)
{
    char setResponse[CDC_RESPONSE_LENGTH] = {};
    char setCommand[CDC_COMMAND_LENGTH] = {};
    int result = -1;

    snprintf(setCommand, CDC_COMMAND_LENGTH, CDC_SET_COMMAND, cmd, value);

    if (this->sendCommand(setCommand, setResponse))
    {
        std::string setResponseST(setResponse, strlen(setResponse));
        try
        {
            json jsonResponse = json::parse(setResponseST);

            try
            {
                jsonResponse["RESULT"].get_to(result);
            }
            catch (const std::exception &e)
            {
                LOGF_ERROR("Error parsing SET %s response for RESULT: %s Error: %s", cmd, setResponse, e.what());
                return false;
            }
        }
        catch (json::exception &e)
        {
            // output exception information
            LOGF_ERROR("Error parsing SET %s response %s Error: %s", cmd, setResponse, e.what());
            return false;
        }
    }
    return (result == 0);
}

bool CheapoDC::Handshake()
{
    if (getActiveConnection() == tcpConnection)
    {
        PortFD = tcpConnection->getPortFD();
    }
    else
    {
        return false;
        LOG_ERROR("CheapoDC is not connected.");
    }

    int tries = 2;
    do
    {
        if (Ack())
        {
            //LOG_INFO("CheapoDC is online. Getting device parameters...");
            return true;
        }
        LOG_ERROR("Error retrieving data from CheapoDC, retrying...");
    } while (--tries > 0);

    LOG_ERROR("Error retrieving data from CheapoDC, please ensure controller "
              "is powered and the port is correct.");

    return false;
}

bool CheapoDC::Ack()
{
    char resp[CDC_RESPONSE_LENGTH] = {};

    fwVOneDetected = false;
    additionalOutputsSupported = false;
    internalHumiditySensorSupported = false;

    memset(resp, '\0', sizeof(resp));

    if (!this->sendGetCommand(CDC_CMD_FW, resp))
        return false;

    FWversionTP[0].setText(resp);
    FWversionTP.setState(IPS_OK);
    FWversionTP.apply();
    //LOG_INFO("CheapoDC ACK.");
    //LOG_INFO(resp);

    if (!sscanf(resp, "%d.%d.%d", &fwMajorVersion, &fwMinorVersion, &fwPatchVersion))
    {
        LOGF_ERROR("Parsing Firmware Version: Response (%s) for Command (%s) not valid.", resp, CDC_CMD_FW);
        return false;
    }

    if (fwMajorVersion == 1)
    {
        fwVOneDetected = true;
    }
    else
    {
        // Addtional outputs supported in release 2.20 FW
        if ((fwMajorVersion > 2) || (fwMinorVersion >= 2)) // 2.2.0+
        {
            additionalOutputsSupported = true;
        } 

        // Internal Humidity Sensor supported in release 2.3.0 FW  
        if ((fwMajorVersion > 2) || (fwMinorVersion >= 3)) // 2.3.0+
        {
            internalHumiditySensorSupported = false;

            // Test to see if the internal sensor is configured
            memset(resp, '\0', sizeof(resp));
            if (this->sendGetCommand(CDC_CMD_SDAP, resp))
            {
                int internalSDAPin;

                if (sscanf(resp, "%d", &internalSDAPin) == 1)
                {
                    if (internalSDAPin >= 0)
                    {
                        internalHumiditySensorSupported = true;
                    }
                }
            }
        }
    }

    return checkOutputConfiguration();
}

// Check available outputs and configure INDI:PowerInterface properties
bool CheapoDC::checkOutputConfiguration()
{
    int dewOutputs = 0;
    int powerOutputs = 0;
    char resp[CDC_RESPONSE_LENGTH] = {};
    char command[CDC_COMMAND_LENGTH] = {}; 
    bool useDewLabels = false;
    bool usePowerLabels = false;

    if (additionalOutputsSupported)
    {
        // Calculate number of Dew and Power outputs
        for (int i = 0; i < CDC_TOTAL_ADDITIONAL_OUTPUTS; i++)
        {
            int mode;
            int pin = i + CDC_MIN_ADDITIONAL_OUTPUT;

            memset(resp, '\0', sizeof(resp));
            memset(command, '\0', sizeof(command));

            snprintf(command, CDC_COMMAND_LENGTH, CDC_CMD_CPM, pin);
            if (!this->sendGetCommand(command, resp))
                return false;

            if (sscanf(resp, "%d", &mode)) {
                
                switch (mode)
                {
                    case CONTROLLER_PIN_MODE_DISABLED:
                        // Do nothing since we don't show Disabled Outputs
                        break;
                    case CONTROLLER_PIN_MODE_CONTROLLER:
                        dewOutputs++;
                        break;
                    case CONTROLLER_PIN_MODE_PWM:
                        dewOutputs++;
                        break;
                    case CONTROLLER_PIN_MODE_BOOLEAN:
                        powerOutputs++;
                        break;
                    default:
                        LOGF_ERROR("Get Output Mode Pin %d: Response [%s] for Command [%s] not valid.", pin, resp, command);
                }

                lastControllerPinMode[i] = mode;

            }
            else
                LOGF_ERROR("Get Output Mode Pin %d: Response [%s] for Command [%s] not valid.", pin, resp, command);
            
        }
    }

    //LOGF_INFO("CheapoDC Additional Outputs: Dew Outputs: %d, Power Outputs: %d", dewOutputs, powerOutputs);

    PI::initProperties(DEW_TAB, powerOutputs, dewOutputs+1, 0, 1, 0);

    // Set up initial Min and Max output values for controller output
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_OMIN, resp))
        return false;

    if (!sscanf(resp, "%d", &prevMinOutput))
        return false;

    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_OMAX, resp))
        return false;

    if (!sscanf(resp, "%d", &prevMaxOutput))
        return false;

    DewChannelDutyCycleNP[0].setMin(prevMinOutput);
    DewChannelDutyCycleNP[0].setMax(prevMaxOutput);
    DewChannelDutyCycleNP.apply();
    MinimumOutputNP[0].setMax(prevMaxOutput - 1);
    MinimumOutputNP[0].setValue(prevMinOutput);
    MinimumOutputNP.apply();
    MaximumOutputNP[0].setMin(prevMinOutput + 1);
    MaximumOutputNP[0].setValue(prevMaxOutput);
    MaximumOutputNP.apply();

    useDewLabels = PI::DewChannelLabelsTP.load();
    
    // Overwrite default Dew Channel Labels but allow for custom labels
    if ((!useDewLabels) || (strcmp( PI::DewChannelLabelsTP[0].getText(), PI::DewChannelLabelsTP[0].getLabel()) == 0))
    {
        LOG_INFO("Setting default Dew Channel Labels.");
        PI::DewChannelLabelsTP[0].setText("Controller Output");
        PI::DewChannelsSP[0].setLabel("Controller Output");
        PI::DewChannelDutyCycleNP[0].setLabel("Controller Output");
        PI::AutoDewSP[0].setLabel("Controller Output");
    }

    if (powerOutputs > 0)
    {
        usePowerLabels = PI::PowerChannelLabelsTP.load();
    }

    if ((dewOutputs + powerOutputs) > 0)
    {
        dewOutputs = 1;
        powerOutputs = 0;
        for (int i = 0; i < CDC_TOTAL_ADDITIONAL_OUTPUTS; i++)
        {
            char labelC[MAXINDILABEL] = {};
            char dewLabel[MAXINDILABEL] = {};
            int pin = i + CDC_MIN_ADDITIONAL_OUTPUT;
            memset(labelC, '\0', sizeof(labelC));
            memset(dewLabel, '\0', sizeof(dewLabel));

            snprintf(labelC, MAXINDILABEL, channelLabels[lastControllerPinMode[i]], pin);
            snprintf(dewLabel, MAXINDILABEL, "%s (%%)", labelC);

            //LOGF_INFO("Dew count %d Power count %d", dewOutputs, powerOutputs);
            //LOGF_INFO("CheapoDC Additional Output Pin %d Mode %d Label: %s", pin, lastControllerPinMode[i], labelC);
            outputToChannel[i] = -1;

            switch (lastControllerPinMode[i])
            {
                case CONTROLLER_PIN_MODE_DISABLED:
                    // Do nothing since we don't show Disabled Outputs
                    break;
                case CONTROLLER_PIN_MODE_CONTROLLER:
                case CONTROLLER_PIN_MODE_PWM:
                    // Check to see if pin modes have been changed sin last time...
                    if (useDewLabels) 
                    {
                        int lastPin = -1;
                        char lastMode[MAXINDILABEL] = {};


                        if (sscanf(PI::DewChannelLabelsTP[dewOutputs].getText(), "Output %d (%s)", &lastPin, lastMode) == 2)
                        {
                            if (lastControllerPinMode[i] == CONTROLLER_PIN_MODE_CONTROLLER)
                            {
                                useDewLabels = (strcmp(lastMode, "Controller") == 0);
                            }
                            else
                            {
                                useDewLabels = (strcmp(lastMode, "PWM") == 0);
                            }
                            useDewLabels = (lastPin == pin) && useDewLabels;
                        }
                    }
                    if ((!useDewLabels) || (strcmp(PI::DewChannelLabelsTP[dewOutputs].getText(), PI::DewChannelLabelsTP[dewOutputs].getLabel()) == 0))
                    {
                        PI::DewChannelLabelsTP[dewOutputs].setText(labelC);
                        PI::DewChannelsSP[dewOutputs].setLabel(labelC);
                        PI::DewChannelDutyCycleNP[dewOutputs].setLabel(dewLabel);
                    }
                    dewChannelToOutput[dewOutputs] = pin;
                    outputToChannel[i] = dewOutputs;
                    //LOGF_INFO("Mapping Output to Dew Channel %d, Pin %d to Channel %d", i, pin, dewOutputs);
                    dewOutputs++;
                    break;
                case CONTROLLER_PIN_MODE_BOOLEAN:
                    // Check to see if pin modes have been changed since last time...
                    if (usePowerLabels) 
                    {
                        int lastPin = -1;

                        if (sscanf(PI::PowerChannelLabelsTP[powerOutputs].getText(), channelLabels[lastControllerPinMode[i]], &lastPin) == 1)
                        {
                            usePowerLabels = (lastPin == pin);
                        }
                    }
                    if ((!usePowerLabels) || (strcmp(PI::PowerChannelLabelsTP[powerOutputs].getText(), PI::PowerChannelLabelsTP[powerOutputs].getLabel()) == 0))
                    {
                        PI::PowerChannelLabelsTP[powerOutputs].setText(labelC);
                        PI::PowerChannelsSP[powerOutputs].setLabel(labelC);
                    }
                    powerChannelToOutput[powerOutputs] = pin;
                    outputToChannel[i] = powerOutputs;
                    //LOGF_INFO("Mapping Output tp Power Channel %d,Pin %d to Channel %d", i, pin, powerOutputs);
                    powerOutputs++;
                    break;
                default:
                    // Do nothing
                    break;
            } 
        }
    }

    PI::DewChannelLabelsTP.apply();
    if (powerOutputs > 0)
        PI::PowerChannelLabelsTP.apply();

    return true;
   
}

void CheapoDC::getWeatherSource()
{
    char resp[CDC_RESPONSE_LENGTH] = {};
    bool usingOpenWeather = false;

    if (sendGetCommand(CDC_CMD_WS, resp))
    {
        if (fwVOneDetected)
        {
            usingOpenWeather = (strcmp(resp, "OpenWeather") == 0);
            WeatherSourceSP.reset();
            if (usingOpenWeather)
                WeatherSourceSP[OPENWEATHER].setState(ISS_ON);
            else
                WeatherSourceSP[OPENMETEO].setState(ISS_ON);

            WeatherSourceSP.setState(IPS_OK);
            WeatherSourceSP.apply();
        }
        else
        {
            int newWeatherSource;
            int ok = 0;

            ok = sscanf(resp, "%d", &newWeatherSource);
            usingExternalWeatherSource = (newWeatherSource == EXTERNALSOURCE);

            if ((ok == 1) && (newWeatherSource <= INTERNALSOURCE))
            {
                WeatherSourceSP.reset();
                WeatherSourceSP[newWeatherSource].setState(ISS_ON);
                WeatherSourceSP.setState(IPS_OK);
                WeatherSourceSP.apply();
            }
            else
                LOGF_ERROR("Get Weather Source: Response [%s] for Command [%s] not valid.", resp, CDC_CMD_WS);
        }
    }
}

// Set Controller Mode (Automatic, Manual, Off)
bool CheapoDC::setControllerMode(int value)
{
    if ((value < AUTOMATIC) || (value > OFF))
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_INT_VALUE, value);
        return sendSetCommand(CDC_CMD_DCM, valBuf);
    }
}

// Set Temperature Mode (Weather Query, External Input)
bool CheapoDC::setTemperatureMode(int value)
{
    if ((value < WEATHER_QUERY) || (value > EXTERNAL_INPUT))
    {
        LOGF_ERROR("Invalid Temperature Mode value: %d.", value);
        
        return false;
    }
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        // Check Focuser Snoop if EXTERNAL_INPUT
        if ((value == EXTERNAL_INPUT) && (strlen(ActiveDeviceTP[ACTIVE_FOCUSER].getText()) == 0))
            LOG_INFO("Temperature Mode set to Focuser Device. Configure Snoop Device for Focuser to send temperatures from the Focuser Device.");
        
        sprintf(valBuf, CDC_INT_VALUE, value);
        return sendSetCommand(CDC_CMD_DCTM, valBuf);
    }
}

// Set SetPoint Mode (Dew Point, Temperature)
bool CheapoDC::setSetPointMode(int value)
{
    if ((value < DEWPOINT) || (value > MIDPOINT))
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_INT_VALUE, value);
        return sendSetCommand(CDC_CMD_SPM, valBuf);
    }
}

bool CheapoDC::setWeatherSource(int value)
{
    int maxValue = internalHumiditySensorSupported ? INTERNALSOURCE : EXTERNALSOURCE;

    if (fwVOneDetected)
    {
        LOGF_WARN("CheapoDC firmware V%s does not support Set Weather Source. Please upgrade firmware to latest V2+.", FWversionTP[0].getText());
        return false;
    }
    if ((value < OPENMETEO) || (value > maxValue))
    {
        if (value == INTERNALSOURCE)
        {
            LOG_WARN("Setting Weather Source to CheapoDC Sensor requires firmware v2.3.0+ with a configured sensor.");
        }
        else
        {
            LOGF_ERROR("Invalid Weather Source value: %d.", value);
        } 
        refreshSettings( true ); // Refresh to show actual state  
        return false;
    }
    else
    { 
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        if (value == EXTERNALSOURCE)
            LOG_INFO("Weather Source set to Weather Device. Configure Snoop Device for Weather to receive weather data.");

        sprintf(valBuf, CDC_INT_VALUE, value);
        return sendSetCommand(CDC_CMD_WS, valBuf);
    }
}

bool CheapoDC::SetPowerPort(size_t port, bool enabled)
{
    bool returnValue = false;
    int output = powerChannelToOutput[port];

    //LOGF_INFO("SetPowerPort: port=%zu, enabled=%s, Output=%d", port, enabled ? "true" : "false", output);

    if ((output >= CDC_MIN_ADDITIONAL_OUTPUT) && (output < (CDC_TOTAL_ADDITIONAL_OUTPUTS + CDC_MIN_ADDITIONAL_OUTPUT)))
    {
        if (enabled)
        {
            returnValue = setAdditionalOutput(output, 100);
        }
        else
        {
            returnValue = setAdditionalOutput(output, 0);
        }
    }
    else
    {
        LOGF_ERROR("SetPowerPort: Invalid Output %d to port mapping %zu.", output, port);
        returnValue = false;
    }
    if (returnValue)
    {
        PI::PowerChannelsSP.setState(IPS_BUSY);
        PI::PowerChannelsSP.apply();
        refreshSettings();
        return true;
    }
    return false;
}

bool CheapoDC::SetAutoDewEnabled(size_t port, bool enabled)
{
    if (port != 0) // CheapoDC has a single auto dew control
    {
        LOGF_WARN("SetAutoDewEnabled: Invalid port number %zu.", port);
        return false;
    }

    if (setControllerMode(enabled ? AUTOMATIC : OFF))
    {
        PI::AutoDewSP.setState(IPS_BUSY);
        PI::AutoDewSP.apply();
        refreshSettings();
        return true;
    }
    return false;
}

bool CheapoDC::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    bool returnValue = false;

    //LOGF_INFO("SetDewPort: port=%zu, enabled=%s, dutyCycle=%.2f", port, enabled ? "true" : "false", dutyCycle);
    if (port == 0) // Main dew output 
    {   
        if (AutoDewSP[0].getState() == ISS_ON)
        {
            LOG_WARN("Cannot set duty cycle for Controller while Auto Dew Control is enabled.");
            refreshSettings( true ); // Refresh to show actual state
            return false;
        }
        if (enabled)
        {
            if (previousControllerMode != MANUAL)
            {
                // Ignore the duty cycle and just set to Manual mode
                returnValue = setControllerMode(MANUAL);
            }
            else if ((dutyCycle < prevMinOutput) || (dutyCycle > prevMaxOutput))
            {
                LOGF_WARN("Controller Duty cycle %.2f is out of range (%.2f - %.2f).", 
                          dutyCycle, static_cast<double>(prevMinOutput), static_cast<double>(prevMaxOutput));
            }
            else
            {
                returnValue = setControllerMode(MANUAL) && setOutput(static_cast<int>(dutyCycle));
            }
        }
        else
        {
            returnValue = setControllerMode(OFF);
        }
    }
    else if (port >= 1 && port <= CDC_TOTAL_ADDITIONAL_OUTPUTS) // Additional 4 outputs (2 to 5) 4 Ports (1 to 4)
    {
        int output = dewChannelToOutput[port];
        // Additional outputs are 1-based in the interface, map to CDC_MIN_ADDITIONAL_OUTPUT + (port - 1)
        if (lastControllerPinMode[output - CDC_MIN_ADDITIONAL_OUTPUT] == CONTROLLER_PIN_MODE_PWM)
        {
            if (enabled)
            {
                returnValue = setAdditionalOutput(output, static_cast<int>(dutyCycle));
            }
            else
            {
                returnValue = setAdditionalOutput(output, 0);
            }
        }
        else
        {
            LOGF_WARN("Duty cycle for output %d is set by %s.", output, PI::AutoDewSP[0].getLabel());
        }
    }

    if (returnValue)
    {
        PI::DewChannelDutyCycleNP.setState(IPS_BUSY);
        PI::DewChannelsSP.setState(IPS_BUSY);
        PI::DewChannelDutyCycleNP.apply();
        PI::DewChannelsSP.apply();
    }

    refreshSettings( true ); // Delay since multiple Dew Port settings can be done in succession
    return returnValue;
}

bool CheapoDC::SetVariablePort(size_t port, bool enabled, double voltage)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    INDI_UNUSED(voltage);
    LOG_DEBUG("SetVariablePort not supported by CheapoDC.");
    return false;
}

bool CheapoDC::SetLEDEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    LOG_DEBUG("SetLEDEnabled not supported by CheapoDC.");
    return false;
}

bool CheapoDC::CyclePower()
{
    LOG_DEBUG("CyclePower not supported by CheapoDC.");
    return false;
}

bool CheapoDC::SetUSBPort(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    LOG_DEBUG("SetUSBPort not supported by CheapoDC.");
    return false;
}

bool CheapoDC::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    bool result = false;
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (INDI::PowerInterface::processSwitch(dev, name, states, names, n))
        return true;

    if (TemperatureModeSP.isNameMatch(name))
    {
        TemperatureModeSP.update(states, names, n);
        TemperatureModeSP.setState(IPS_BUSY);
        TemperatureModeSP.apply();
        result = setTemperatureMode(TemperatureModeSP.findOnSwitchIndex());
        return result && readSettings();
    }

    if (SetPointModeSP.isNameMatch(name))
    {
        SetPointModeSP.update(states, names, n);
        SetPointModeSP.setState(IPS_BUSY);
        SetPointModeSP.apply();
        result = setSetPointMode(SetPointModeSP.findOnSwitchIndex());
        return result && readSettings();
    }

    if (WeatherSourceSP.isNameMatch(name))
    {
        WeatherSourceSP.update(states, names, n);
        WeatherSourceSP.setState(IPS_BUSY);
        WeatherSourceSP.apply();
        result = setWeatherSource(WeatherSourceSP.findOnSwitchIndex());
        return result && readSettings();
    }

    if (RefreshSP.isNameMatch(name))
    {
        char buf[3] = {"NA"};

        RefreshSP.update(states, names, n);
        RefreshSP.setState(IPS_BUSY);
        RefreshSP.apply();
        result = sendSetCommand(CDC_CMD_QN, buf);
        return result && readSettings();
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool CheapoDC::setSetPoint(float value)
{
    char valBuf[CDC_SET_VALUE_LENGTH] = {};

    sprintf(valBuf, CDC_FLOAT_VALUE, value);
    return sendSetCommand(CDC_CMD_SP, valBuf);
}

bool CheapoDC::setTrackPointOffset(float value)
{
    if ((value < -5.0) || (value > 5.0))
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_FLOAT_VALUE, value);
        return sendSetCommand(CDC_CMD_TPO, valBuf);
    }
}

bool CheapoDC::setTrackingRange(float value)
{
    if ((value < 4.0) || (value > 10.0))
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_FLOAT_VALUE, value);
        return sendSetCommand(CDC_CMD_TKR, valBuf);
    }
}

bool CheapoDC::setOutput(int value)
{
    if ((value < MinimumOutputNP[0].getValue()) || (value > MaximumOutputNP[0].getValue()))
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_INT_VALUE, value);
        return sendSetCommand(CDC_CMD_DCO, valBuf);
    }
}

bool CheapoDC::setAdditionalOutput(int pin, int value)
{
    if ((value < 0) || (value > 100))
        return false;
    else
    {
        char command[CDC_COMMAND_LENGTH] = {};
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_INT_VALUE, value);
        snprintf(command, CDC_COMMAND_LENGTH, CDC_CMD_CPO, pin);
        return sendSetCommand(command, valBuf);
    }
}

bool CheapoDC::setMinimumOutput(int value)
{
    if (value >= MaximumOutputNP[0].getValue())
    {
        LOGF_WARN("Minimum Output %d must be less than Maximum Output %d.", value, MaximumOutputNP[0].getValue());
        return false;
    }
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_INT_VALUE, value);
        return sendSetCommand(CDC_CMD_OMIN, valBuf);
    }
}

bool CheapoDC::setMaximumOutput(int value)
{
    if (value <= MinimumOutputNP[0].getValue())
    {
        LOGF_WARN("Maximum Output %d must be greater than Minimum Output %d.", value, MinimumOutputNP[0].getValue());
        return false;
    }
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_INT_VALUE, value);
        return sendSetCommand(CDC_CMD_OMAX, valBuf);
    }
}

bool CheapoDC::setUpdateOutputEvery(int value)
{
    if ((value < 0) || (value > 20))
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_INT_VALUE, value);
        return sendSetCommand(CDC_CMD_UOE, valBuf);
    }
}

bool CheapoDC::setWeatherQueryEvery(int value)
{
    if ((value < 0) || (value > 20))
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_INT_VALUE, value);
        return sendSetCommand(CDC_CMD_WQE, valBuf);
    }
}

bool CheapoDC::setWeatherQueryEnabled(bool enabled)
{
    char disabledBuf[2] = "0";
    char enabledBuf[2] = "1";

    if (enabled)
        return sendSetCommand(CDC_CMD_WQEN, enabledBuf);
    else
        return sendSetCommand(CDC_CMD_WQEN, disabledBuf);
}

bool CheapoDC::setLatitude(float value)
{
    if ((value < -90.0) || (value > 90.0))
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_FLOAT_VALUE, value);
        return sendSetCommand(CDC_CMD_LAT, valBuf);
    }
}

bool CheapoDC::setLongitude(float value)
{
    if (value > 180)
        value = value - 360;
    if ((value < -180.0) || (value > 180.0))
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_FLOAT_VALUE, value);
        return sendSetCommand(CDC_CMD_LON, valBuf);
    }
}

bool CheapoDC::setUTCOffset(int offset)
{
    char valBuf[CDC_SET_VALUE_LENGTH] = {};
    char zero[] = "0";

    if (fwVOneDetected)
        return false;

    sprintf(valBuf, CDC_INT_VALUE, offset*3600);
    return sendSetCommand(CDC_CMD_TMZ, valBuf) && sendSetCommand(CDC_CMD_DST, zero);
}

bool CheapoDC::setLocation(float latitude, float longitude)
{
    bool result = true;
    char buf[3] = {"NA"};
    // CheapoDC longitude expected -180 to +180
    if (longitude > 180)
        longitude -= 360;

    if (!fwVOneDetected)
        result = result && setWeatherQueryEnabled( false);

    result = result && setLatitude(latitude);
    result = result && setLongitude(longitude);
    
    if (!fwVOneDetected)
    {
        result = result && setWeatherQueryEnabled(true);
        result = result && sendSetCommand(CDC_CMD_QN, buf);
    }
    return result;
}

bool CheapoDC::setExternalTemperature(float value)
{
    char valBuf[CDC_SET_VALUE_LENGTH] = {};

    sprintf(valBuf, CDC_FLOAT_VALUE, value);
    return sendSetCommand(CDC_CMD_ATPX, valBuf);
}

bool CheapoDC::setWeatherTemperature(float value)
{
    char valBuf[CDC_SET_VALUE_LENGTH] = {};
    if (usingExternalWeatherSource)
    {
        sprintf(valBuf, CDC_FLOAT_VALUE, value);
        return sendSetCommand(CDC_CMD_ATPQ, valBuf);
    }
    return false;
}

bool CheapoDC::setWeatherHumidity(float value)
{
    char valBuf[CDC_SET_VALUE_LENGTH] = {};
    if (usingExternalWeatherSource)
    {
        sprintf(valBuf, CDC_FLOAT_VALUE, value);
        return sendSetCommand(CDC_CMD_HU, valBuf);
    }
    return false;
}

bool CheapoDC::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    bool result = false;

    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (INDI::PowerInterface::processNumber(dev, name, values, names, n))
        return true;

    if (TrackPointOffsetNP.isNameMatch(name))
    {
        TrackPointOffsetNP.update(values, names, n);
        TrackPointOffsetNP.setState(IPS_BUSY);
        TrackPointOffsetNP.apply();
        result = setTrackPointOffset(TrackPointOffsetNP[0].getValue());
        return result && readSettings();
    }

    if (TrackingRangeNP.isNameMatch(name))
    {
        TrackingRangeNP.update(values, names, n);
        TrackingRangeNP.setState(IPS_BUSY);
        TrackingRangeNP.apply();
        result = setTrackingRange(TrackingRangeNP[0].getValue());
        return result && readSettings();
    }

    if (MinimumOutputNP.isNameMatch(name))
    {
        MinimumOutputNP.update(values, names, n);
        MinimumOutputNP.setState(IPS_BUSY);
        MinimumOutputNP.apply();
        result = setMinimumOutput(MinimumOutputNP[0].getValue());
        return result && readSettings();
    }

    if (MaximumOutputNP.isNameMatch(name))
    {
        MaximumOutputNP.update(values, names, n);
        MaximumOutputNP.setState(IPS_BUSY);
        MaximumOutputNP.apply();
        result = setMaximumOutput(MaximumOutputNP[0].getValue());
        return result && readSettings();
    }

    if (SetPointTemperatureNP.isNameMatch(name))
    {
        SetPointTemperatureNP.update(values, names, n);
        SetPointTemperatureNP.setState(IPS_BUSY);
        SetPointTemperatureNP.apply();
        result = setSetPoint(SetPointTemperatureNP[0].getValue());
        return result && readSettings();
    }

    if (UpdateOutputEveryNP.isNameMatch(name))
    {
        UpdateOutputEveryNP.update(values, names, n);
        UpdateOutputEveryNP.setState(IPS_BUSY);
        UpdateOutputEveryNP.apply();
        result = setUpdateOutputEvery(UpdateOutputEveryNP[0].getValue());
        return result && readSettings();
    }

    if (QueryWeatherEveryNP.isNameMatch(name))
    {
        QueryWeatherEveryNP.update(values, names, n);
        QueryWeatherEveryNP.setState(IPS_BUSY);
        QueryWeatherEveryNP.apply();
        result = setWeatherQueryEvery(QueryWeatherEveryNP[0].getValue());
        return result && readSettings();
    }

    if (LocationNP.isNameMatch(name))
    {
        LocationNP.update(values, names, n);
        LocationNP.setState(IPS_BUSY);
        LocationNP.apply();
        result = setLocation(LocationNP[LOCATION_LATITUDE].getValue(), LocationNP[LOCATION_LONGITUDE].getValue());
        return result && readSettings();
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

void CheapoDC::setActiveDevice(const char *telescopeDevice, const char *focuserDevice, const char *weatherDevice)
{
    if (strcmp(telescopeDevice, activeTelescopeDevice) != 0)
    {
        indi_strlcpy(activeTelescopeDevice, telescopeDevice, MAXINDINAME);
        LOGF_DEBUG("Set snoop for %s device", activeTelescopeDevice);
        if (strlen(activeTelescopeDevice) > 0)
        {
            IDSnoopDevice(activeTelescopeDevice, CDC_SNOOP_LOCATION_PROPERTY);
            if (!fwVOneDetected)
                IDSnoopDevice(activeTelescopeDevice, CDC_SNOOP_TIME_PROPERTY);
        }
    }

    if (strcmp(focuserDevice, activeFocuserDevice) != 0)
    {
        indi_strlcpy(activeFocuserDevice, focuserDevice, MAXINDINAME);
        LOGF_DEBUG("Set snoop for %s device", activeFocuserDevice);
        if (strlen(activeFocuserDevice) > 0)
            IDSnoopDevice(activeFocuserDevice, CDC_SNOOP_FOCUSER_PROPERTY);
    }

    if (strcmp(weatherDevice, activeWeatherDevice) != 0)
    {
        // Not supported for firmware 1 devices
        if (fwVOneDetected && (strlen(weatherDevice) > 0))
        {
            indi_strlcpy(activeWeatherDevice, "", MAXINDINAME);
            LOG_WARN("Snoop Weather Device not supported by CheapoDC firmware V1. Upgrade to V2+ for support.");
        } else
        {
            indi_strlcpy(activeWeatherDevice, weatherDevice, MAXINDINAME);
            LOGF_DEBUG("Set snoop for %s device", activeWeatherDevice);
            if (strlen(activeWeatherDevice) > 0)
                IDSnoopDevice(activeWeatherDevice, CDC_SNOOP_WEATHER_PROPERTY);
        }
    }
}

bool CheapoDC::setWeatherQueryAPIKey(const char *key)
{

    char valBuf[CDC_SET_VALUE_LENGTH] = {};

    sprintf(valBuf, CDC_TEXT_VALUE, key);
    return sendSetCommand(CDC_CMD_WKEY, valBuf);
}

bool CheapoDC::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    bool result = false;

    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (INDI::PowerInterface::processText(dev, name, texts, names, n))
        return true;

    if (WeatherQueryAPIKeyTP.isNameMatch(name))
    {
        WeatherQueryAPIKeyTP.update(texts, names, n);
        WeatherQueryAPIKeyTP.setState(IPS_BUSY);
        WeatherQueryAPIKeyTP.apply();
        result = setWeatherQueryAPIKey(WeatherQueryAPIKeyTP[0].getText());
        return readSettings() && result;
    }

    if (ActiveDeviceTP.isNameMatch(name))
    {
        ActiveDeviceTP.update(texts, names, n);
        ActiveDeviceTP.setState(IPS_BUSY);
        ActiveDeviceTP.apply();
        setActiveDevice(ActiveDeviceTP[ACTIVE_TELESCOPE].getText(), 
                        ActiveDeviceTP[ACTIVE_FOCUSER].getText(), 
                        ActiveDeviceTP[ACTIVE_WEATHER].getText());
        return readSettings();
    }


    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool CheapoDC::ISSnoopDevice(XMLEle *root)
{
    XMLEle *ep = nullptr;
    const char *propName = findXMLAttValu(root, "name");
    const char *deviceName = findXMLAttValu(root, "device");
    bool result = false;

    LOGF_DEBUG("ISSNoopDevice %s, %s", deviceName, propName);

    if (!(cdcConnection & CONNECTION_TCP))
    {
        return true;
    }
    
    if ((!strcmp(propName, CDC_SNOOP_FOCUSER_PROPERTY)) && 
        (!strcmp(deviceName, activeFocuserDevice)))
    {
        bool tempAtributeFound = false;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *name = findXMLAttValu(ep, "name");

            if (!strcmp(name, CDC_SNOOP_FOCUSER_TEMPERATURE))
            {
                float externalTemp = atof(pcdataXMLEle(ep));

                if (externalTemp != XtrnTemperatureNP[EXTERNAL_INPUT].getValue())
                    result = setExternalTemperature(externalTemp) || result;
                LOGF_DEBUG("External Temp set to: %.2f", externalTemp);
                tempAtributeFound = true;
            }
        }

        if (!tempAtributeFound)
            LOGF_WARN("Focuser TEMPERATURE attribute, %s, not found for %s:%s", CDC_SNOOP_FOCUSER_TEMPERATURE, ActiveDeviceTP[ACTIVE_FOCUSER].getText(), CDC_SNOOP_FOCUSER_PROPERTY);
    }

    if ((!fwVOneDetected) && (!strcmp(propName, CDC_SNOOP_TIME_PROPERTY)) && (!strcmp(deviceName, activeTelescopeDevice)))
    {
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *name = findXMLAttValu(ep, "name");

            if (!strcmp(name, CDC_SNOOP_TIME_OFFSET))
            {
                int offset = atoi(pcdataXMLEle(ep));

                result=setUTCOffset(offset);
            }
        }
    }


    if ((!strcmp(propName, CDC_SNOOP_LOCATION_PROPERTY)) && (!strcmp(deviceName, activeTelescopeDevice)))
    {
        bool latAtributeFound = false;
        bool longAtributeFound = false;
        bool updateLocation = false;
        float latitude = LocationNP[LOCATION_LATITUDE].getValue();
        float longitude = LocationNP[LOCATION_LONGITUDE].getValue();

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *name = findXMLAttValu(ep, "name");

            if (!strcmp(name, CDC_SNOOP_LOCATION_LONGITUDE))
            {
                longitude = atof(pcdataXMLEle(ep));
                updateLocation = (longitude != LocationNP[LOCATION_LONGITUDE].getValue()) || updateLocation;
                longAtributeFound = true;
            } else if (!strcmp(name, CDC_SNOOP_LOCATION_LATITUDE))
            {
                latitude = atof(pcdataXMLEle(ep));
                latAtributeFound = true;
                updateLocation = (latitude != LocationNP[LOCATION_LATITUDE].getValue()) || updateLocation;
            }
        }
        if (updateLocation)
        {
            result= setLocation(latitude, longitude) || result;
        }
        if (!longAtributeFound)
            LOGF_WARN("LONG attribute, %s, not found for %s:%s", CDC_SNOOP_LOCATION_LONGITUDE, activeTelescopeDevice, CDC_SNOOP_LOCATION_PROPERTY);
        if (!latAtributeFound)
            LOGF_WARN("LAT attribute, %s, not found for %s:%s", CDC_SNOOP_LOCATION_LATITUDE, activeTelescopeDevice, CDC_SNOOP_LOCATION_PROPERTY);
    }

    if (usingExternalWeatherSource && (!strcmp(propName, CDC_SNOOP_WEATHER_PROPERTY)) && 
        (!strcmp(deviceName, ActiveDeviceTP[ACTIVE_WEATHER].getText())))
    {
        bool temperatureAttributeFound = false;
        bool humidityAttributeFound = false;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *name = findXMLAttValu(ep, "name");

            if (!strcmp(name, CDC_SNOOP_WEATHER_TEMPERATURE))
            {
                float temperature = atof(pcdataXMLEle(ep));

                if (temperature != XtrnTemperatureNP[WEATHER_QUERY].getValue())
                    result = setWeatherTemperature(temperature) || result;
                temperatureAttributeFound = true;
            }
            else if (!strcmp(name, CDC_SNOOP_WEATHER_HUMIDITY))
            {
                float humidity = atof(pcdataXMLEle(ep));

                if (humidity != HumidityNP[0].getValue())
                    result = setWeatherHumidity(humidity) || result;
                humidityAttributeFound = true;
            }
        }
        if (!temperatureAttributeFound)
            LOGF_WARN("TEMPERATURE attribute, %s, not found for %s:%s", CDC_SNOOP_WEATHER_TEMPERATURE, activeWeatherDevice, CDC_SNOOP_WEATHER_PROPERTY);
        if (!humidityAttributeFound)
            LOGF_WARN("HUMIDITY attribute, %s, not found for %s:%s", CDC_SNOOP_WEATHER_HUMIDITY, activeWeatherDevice, CDC_SNOOP_WEATHER_PROPERTY);
    }

    if (result)
        result = readSettings() || result;
    return result;
}

bool CheapoDC::readSettings()
{
    char resp[CDC_RESPONSE_LENGTH] = {};
    char dateBuf[32] = {};
    char timeBuf[32] = {};
    int ok = -1;
    int tmzOffset;
    float temp_ambient, temp_external, humidity, dewpoint, setPoint, trackingRange;
    unsigned int output, minOutput, maxOutput, updatePeriod;
    float trackPointOffset, latitude, longitude;
    unsigned int controllerMode, temperatureMode, setPointMode, queryPeriod;
    bool powerChannelsExist = false;
    
    // Get Temperatures first
    if (!sendGetCommand(CDC_CMD_ATPQ, resp))
        return false;

    ok = sscanf(resp, "%f", &temp_ambient);

    memset(resp, '\0', sizeof(resp));
    if (!sendGetCommand(CDC_CMD_ATPX, resp))
        return false;

    ok = ok * sscanf(resp, "%f", &temp_external);

    if (ok == 1)
    {
        XtrnTemperatureNP[0].setValue(temp_ambient);
        XtrnTemperatureNP[1].setValue(temp_external);
        XtrnTemperatureNP.setState(IPS_OK);
        XtrnTemperatureNP.apply();
    }
    else
        LOG_ERROR("GET temperature values failure");

    // Get Humidity
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_HU, resp))
        return false;

    ok = sscanf(resp, "%f", &humidity);

    if (ok == 1)
    {
        HumidityNP[0].setValue(humidity);
        HumidityNP.setState(IPS_OK);
        HumidityNP.apply();
    }
    else
        LOGF_ERROR("Get Humidity: Response [%s] for Command [%s] not valid.", resp, CDC_CMD_HU);

    // Get Dew Point
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_DP, resp))
        return false;

    ok = sscanf(resp, "%f", &dewpoint);

    if (ok == 1)
    {
        DewpointNP[0].setValue(dewpoint);
        DewpointNP.setState(IPS_OK);
        DewpointNP.apply();
    }
    else
        LOGF_ERROR("Get Dew point: Response [%s] for Command [%s] not valid.", resp, CDC_CMD_DP);
    // Get Set Point
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_SP, resp))
        return false;

    ok = sscanf(resp, "%f", &setPoint);

    if (ok == 1)
    {
        SetPointTemperatureNP[0].setValue(setPoint);
        SetPointTemperatureNP.setState(IPS_OK);
        SetPointTemperatureNP.apply();
    }
    else
        LOGF_ERROR("Get Set Point: Response [%s] for Command [%s] not valid.", resp, CDC_CMD_SP);

    // Get Controller Mode to set Dew Channel and Auto Dew states
    memset(resp, '\0', sizeof(resp));
    if (!sendGetCommand(CDC_CMD_DCM, resp))
        return false;

    ok = sscanf(resp, "%d", &controllerMode);

    if (ok != 1)
        LOGF_ERROR("Get Controller Mode: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_DCM);

    previousControllerMode = controllerMode;
    if (controllerMode == OFF)
    {
        DewChannelsSP[0].setState(ISS_OFF);
        AutoDewSP[0].setState(ISS_OFF);
        DewChannelDutyCycleNP[0].setValue(0);
    }
    else
    {
        DewChannelsSP[0].setState(controllerMode == MANUAL ? ISS_ON : ISS_OFF);
        AutoDewSP[0].setState(controllerMode == AUTOMATIC ? ISS_ON : ISS_OFF);

        memset(resp, '\0', sizeof(resp));

        if (!sendGetCommand(CDC_CMD_DCO, resp))
            return false;

        if (sscanf(resp, "%d", &output))
        {
            DewChannelDutyCycleNP[0].setValue(output);
        }
        else
            LOGF_ERROR("Get Power Output: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_DCO);
    }

    // Addtional Outputs added in release 2.20 FW
    if (additionalOutputsSupported)
    {  
        for (int p = 0; p < CDC_TOTAL_ADDITIONAL_OUTPUTS; p++)
        {
            //LOGF_INFO("Output to Channel %d with Channel %d", p, outputToChannel[p]);
            if (outputToChannel[p] >= 0)
            {
                char command[CDC_COMMAND_LENGTH] = {};
                int pin = p + CDC_MIN_ADDITIONAL_OUTPUT;

                memset(resp, '\0', sizeof(resp));
        
                snprintf(command, CDC_COMMAND_LENGTH, CDC_CMD_CPO, pin);
                if (!this->sendGetCommand(command, resp))
                    return false;
                
                if (sscanf(resp, "%d", &output))
                {
                    switch (lastControllerPinMode[p])
                    {
                    case CONTROLLER_PIN_MODE_CONTROLLER:
                    //LOGF_INFO("Controller PIN Mode Controller for output %d, Duty cycle: %d", pin, output);
                        DewChannelDutyCycleNP[outputToChannel[p]].setValue(output);
                        DewChannelsSP[outputToChannel[p]].setState(ISS_OFF);
                        break;
                    case CONTROLLER_PIN_MODE_PWM:
                    //LOGF_INFO("Controller PIN Mode PWM for output %d, Duty cycle: %d", pin, output);
                        DewChannelDutyCycleNP[outputToChannel[p]].setValue(output);
                        if (output > 0)
                            DewChannelsSP[outputToChannel[p]].setState(ISS_ON);
                        //DewChannelsSP[outputToChannel[p]].setState(output > 0 ? ISS_ON : ISS_OFF );
                        break;
                    case CONTROLLER_PIN_MODE_BOOLEAN:
                    //LOGF_INFO("Controller PIN Mode BOOLEAN for output %d, State: %d", pin, output);
                        PowerChannelsSP[outputToChannel[p]].setState(output > 0 ? ISS_ON : ISS_OFF );
                        powerChannelsExist = true;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }

    DewChannelsSP.setState(IPS_OK);
    DewChannelsSP.apply();
    DewChannelDutyCycleNP.setState(IPS_OK);
    DewChannelDutyCycleNP.apply();
    AutoDewSP.setState(IPS_OK);
    AutoDewSP.apply();
    if (powerChannelsExist)
    {
        PowerChannelsSP.setState(IPS_OK);
        PowerChannelsSP.apply();
    }

    // Get Minimum output
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_OMIN, resp))
        return false;

    ok = sscanf(resp, "%d", &minOutput);

    if (ok == 1)
    {
        if (minOutput != prevMinOutput)
        {
            MinimumOutputNP[0].setValue(minOutput);
            DewChannelDutyCycleNP[0].setMin(minOutput);
            DewChannelDutyCycleNP.apply();
            MaximumOutputNP[0].setMin(minOutput + 1);
            MaximumOutputNP.apply();
            prevMinOutput = minOutput;
        }
        MinimumOutputNP.setState(IPS_OK);
        MinimumOutputNP.apply();
    }
    else
        LOGF_ERROR("Get Minimum Output: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_OMIN);

    // Get Maximum output
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_OMAX, resp))
        return false;

    ok = sscanf(resp, "%d", &maxOutput);

    if (ok == 1)
    {

        if (maxOutput != prevMaxOutput)
        {
            MaximumOutputNP[0].setValue(maxOutput);
            DewChannelDutyCycleNP[0].setMax(maxOutput);
            DewChannelDutyCycleNP.apply();
            MinimumOutputNP[0].setMax(maxOutput - 1);
            MinimumOutputNP.apply();
            prevMaxOutput = maxOutput;
        }
        MaximumOutputNP.setState(IPS_OK);
        MaximumOutputNP.apply();
    }
    else
        LOGF_ERROR("Get Maximum Output: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_OMAX);

    // Get Track Point Offset
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_TPO, resp))
        return false;

    ok = sscanf(resp, "%f", &trackPointOffset);

    if (ok == 1)
    {
        TrackPointOffsetNP[0].setValue(trackPointOffset);
        TrackPointOffsetNP.setState(IPS_OK);
        TrackPointOffsetNP.apply();
    }
    else
        LOGF_ERROR("Get Track Point Offset: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_TPO);

    // Get Tracking Range
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_TKR, resp))
        return false;

    ok = sscanf(resp, "%f", &trackingRange);

    if (ok == 1)
    {
        TrackingRangeNP[0].setValue(trackingRange);
        TrackingRangeNP.setState(IPS_OK);
        TrackingRangeNP.apply();
    }
    else
        LOGF_ERROR("Get Update Output Every: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_TKR);

    // Get Output Update Period
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_UOE, resp))
        return false;

    ok = sscanf(resp, "%d", &updatePeriod);

    if (ok == 1)
    {
        UpdateOutputEveryNP[0].setValue(updatePeriod);
        UpdateOutputEveryNP.setState(IPS_OK);
        UpdateOutputEveryNP.apply();
    }
    else
        LOGF_ERROR("Get Query Weather Every: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_UOE);

    // Get Query Weather Period
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_WQE, resp))
        return false;

    ok = sscanf(resp, "%d", &queryPeriod);

    if (ok == 1)
    {
        QueryWeatherEveryNP[0].setValue(queryPeriod);
        QueryWeatherEveryNP.setState(IPS_OK);
        QueryWeatherEveryNP.apply();
    }
    else
        LOGF_ERROR("Get Query Weather Every: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_WQE);

    // Get Device Date-Time and UTC Offset (Not supported in FW V1)
    if (!fwVOneDetected)
    {
        memset(resp, '\0', sizeof(resp));

        if (!sendGetCommand(CDC_CMD_CDT, resp))
            return false;

        DeviceTimeTP[LOCAL_TIME].setText(resp);

        memset(resp, '\0', sizeof(resp));

        if (!sendGetCommand(CDC_CMD_TMZ, resp))
            return false;
        
        ok = sscanf(resp, "%d", &tmzOffset);

        if (ok == 1)
        {
            memset(resp, '\0', sizeof(resp));
            sprintf(resp, CDC_INT_VALUE, tmzOffset/3600);
            DeviceTimeTP[UTC_OFFSET].setText(resp);
            DeviceTimeTP.setState(IPS_OK);
            DeviceTimeTP.apply();
        }
        else
        {
            LOGF_ERROR("Get UTC Offset: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_TMZ);
            DeviceTimeTP.setState(IPS_ALERT);
        }
    }

    // Get Location - Latitude
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_LAT, resp))
        return false;

    ok = sscanf(resp, "%f", &latitude);

    if ((ok == 1) && (latitude >= -90.0) && (latitude <= 90.0))
    {
        LocationNP[LOCATION_LATITUDE].setValue(latitude);
        LocationNP.setState(IPS_OK);
        LocationNP.apply();
    }
    else
        LOGF_ERROR("Get Latitude: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_LAT);

    // Get Location - Longitude
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_LON, resp))
        return false;

    ok = sscanf(resp, "%f", &longitude);

    if ((ok == 1) && (longitude >= -180.0) && (longitude <= 180.0))
    {
        if (longitude < 0)
            longitude = longitude + 360;
        LocationNP[LOCATION_LONGITUDE].setValue(longitude);
        LocationNP.setState(IPS_OK);
        LocationNP.apply();
    }
    else
        LOGF_ERROR("Get Longitude: Response [%s] for Command [%s] invalid.", resp, CDC_CMD_LON);

    // Get Set Point Mode
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_SPM, resp))
        return false;

    ok = sscanf(resp, "%d", &setPointMode);

    if ((ok == 1) && (setPointMode <= MIDPOINT))
    {
        SetPointModeSP.reset();
        SetPointModeSP[setPointMode].setState(ISS_ON);
        SetPointModeSP.setState(IPS_OK);
        SetPointModeSP.apply();
    }
    else
        LOGF_ERROR("Get Set Point Mode: Response [%s] for Command [%s] not valid.", resp, CDC_CMD_SPM);

    // Get weather source
    getWeatherSource();

    // Get OpenWeather API Key
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_WKEY, resp))
        return false;

    WeatherQueryAPIKeyTP[0].setText(resp);
    WeatherQueryAPIKeyTP.setState(IPS_OK);
    WeatherQueryAPIKeyTP.apply();

    // Get Last Weather Update date time
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_LWUD, dateBuf))
        return false;

    if (!sendGetCommand(CDC_CMD_LWUT, timeBuf))
        return false;

    sprintf(resp, "%s %s", dateBuf, timeBuf);

    WeatherUpdatedTP[0].setText(resp);
    WeatherUpdatedTP.setState(IPS_OK);
    WeatherUpdatedTP.apply();

    // Active Devices
    ActiveDeviceTP[ACTIVE_TELESCOPE].setText(activeTelescopeDevice);
    ActiveDeviceTP[ACTIVE_FOCUSER].setText(activeFocuserDevice);
    ActiveDeviceTP[ACTIVE_WEATHER].setText(activeWeatherDevice);
    ActiveDeviceTP.setState(IPS_OK);
    ActiveDeviceTP.apply();

    // Get Temperature Mode
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_DCTM, resp))
        return false;

    ok = sscanf(resp, "%d", &temperatureMode);

    if ((ok == 1) && (temperatureMode <= TEMPERATURE))
    {
        TemperatureModeSP.reset();
        TemperatureModeSP[temperatureMode].setState(ISS_ON);
        TemperatureModeSP.setState(IPS_OK);
        TemperatureModeSP.apply();
    }
    else
        LOGF_ERROR("Get Temperature Mode: Response [%s] for Command [%s] not valid.", resp, CDC_CMD_DCTM);

    // Refresh
    RefreshSP.reset();
    RefreshSP[0].setState(ISS_OFF);
    RefreshSP.setState(IPS_OK);
    RefreshSP.apply();

    // Check for Snoop and settings alignment
    // For Temperature Device
    if ((previousTemperatureMode != temperatureMode) && 
        (temperatureMode == EXTERNAL_INPUT) &&
        (strlen(ActiveDeviceTP[ACTIVE_FOCUSER].getText()) == 0))
    {
        LOG_INFO("Temperature Mode set to External Input. Set Snoop Device for Focuser to send temperature from the Focuser Device.");
    }
    previousTemperatureMode = temperatureMode;

    // For Weather Device
    if (!fwVOneDetected)
    {
        if ((usingExternalWeatherSource) && 
            (!previuoslyUsingExternalWeatherSource) && 
            (strlen(ActiveDeviceTP[ACTIVE_WEATHER].getText()) == 0))
        {
            LOG_INFO("Weather Source set to Weather Device. Configure Snoop Device for Weather to send temperature/humidity from the Weather Device.");
        }
        previuoslyUsingExternalWeatherSource = usingExternalWeatherSource;

    }

    return true;
}

bool CheapoDC::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    INDI::PowerInterface::saveConfigItems(fp);
    ActiveDeviceTP.save(fp);
   /* MinimumOutputNP.save(fp);
    MaximumOutputNP.save(fp);
    TrackPointOffsetNP.save(fp);
    TrackingRangeNP.save(fp);
    UpdateOutputEveryNP.save(fp);
    QueryWeatherEveryNP.save(fp);
    WeatherSourceSP.save(fp);
    WeatherQueryAPIKeyTP.save(fp);
    LocationNP.save(fp);
    SetPointModeSP.save(fp);
    TemperatureModeSP.save(fp);
    SetPointTemperatureNP.save(fp); */
    return true;
}

void CheapoDC::refreshSettings(bool delayRefresh )
{
    // Stop current timer
    if (timerIndex != -1)
    {
        
        RemoveTimer(timerIndex);
    }
    if (!delayRefresh)
    {
        // Read settings immediately
        readSettings();
        // Restart timer
        timerIndex = SetTimer(getCurrentPollingPeriod()); //
    }
    else
    {
        // Wait then refresh
        timerIndex = SetTimer(500);
    }
}

void CheapoDC::TimerHit()
{
    if (!isConnected())
    {
        return;
    }

    // Get temperatures etc.
    readSettings();
    timerIndex = SetTimer(getCurrentPollingPeriod());
}
