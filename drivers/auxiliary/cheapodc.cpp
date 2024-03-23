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

CheapoDC::CheapoDC()
{
    setVersion(CDRIVER_VERSION_MAJOR, CDRIVER_VERSION_MINOR);
}

bool CheapoDC::initProperties()
{
    DefaultDevice::initProperties();

    /* Output Power */
    OutputPowerNP[0].fill("OUTPUT", "Power (%)", "%3.0f", 0, 100, 1, 0.);
    OutputPowerNP.fill(getDeviceName(), "OUTPUT", "Output", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    /* Minimum Output Power */
    MinimumOutputNP[0].fill("MINIMUMOUTPUT", "Power (%)", "%3.0f", 0, 99, 1, prevMinOutput);
    MinimumOutputNP.fill(getDeviceName(), "MINIMUMOUTPUT", "Output Min", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /* Maximum Output Power */
    MaximumOutputNP[0].fill("MAXIMUMOUTPUT", "Power (%)", "%3.0f", 1, 100, 1, prevMaxOutput);
    MaximumOutputNP.fill(getDeviceName(), "MAXIMUMOUTPUT", "Output Max", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    XtrnTemperatureNP[0].fill("WEATHERQUERY", "Weather Query (\u2103)", "%3.2f", -50., 120., 0., 0.);
    XtrnTemperatureNP[1].fill("EXTERNALINPUT", "External Input (\u2103)", "%3.2f", -50., 120., 0., 0.);
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
    TrackPointOffsetNP.fill(getDeviceName(), "TRACKPOINTOFFSET", "Track Point Offset", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Tracking Range */
    TrackingRangeNP[0].fill("TRACKINGRANGE", "4.0 to 10.0 (\u2103)", "%2.1f", 4, 10, .5, 5.0);
    TrackingRangeNP.fill(getDeviceName(), "TRACKINGRANGE", "Tracking Range", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Dew Controller mode */
    ControllerModeSP[0].fill("AUTOMATIC", "Automatic", ISS_OFF);
    ControllerModeSP[1].fill("MANUAL", "Manual", ISS_ON);
    ControllerModeSP[2].fill("OFF", "Off", ISS_OFF);
    ControllerModeSP.fill(getDeviceName(), "CONTROLLER_MODE", "Controller Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /*  Temperature mode */
    TemperatureModeSP[0].fill("WEATHER_QUERY", "Weather Query", ISS_ON);
    TemperatureModeSP[1].fill("EXTERNAL_INPUT", "External Input", ISS_OFF);
    TemperatureModeSP.fill(getDeviceName(), "TEMPERATURE_MODE", "Temperature Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /*  Set Point mode */
    SetPointModeSP[0].fill("DEWPOINT", "Dew Point", ISS_ON);
    SetPointModeSP[1].fill("TEMPERATURE", "Temperature", ISS_OFF);
    SetPointModeSP[2].fill("MIDPOINT", "Midpoint", ISS_OFF);
    SetPointModeSP.fill(getDeviceName(), "SETPOINT_MODE", "Set Point Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /*  Update Output Every 1 to 20 minutes */
    UpdateOutputEveryNP[0].fill("UPDATE_PERIOD", "Period (min)", "%2.0f", 1, 20, 1, 1);
    UpdateOutputEveryNP.fill(getDeviceName(), "UPDATE_OUTPUT", "Update Output", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Query Weather Every 0 to 20 minutes */
    QueryWeatherEveryNP[0].fill("UPDATE_PERIOD", "Period (min)", "%2.0f", 0, 20, 1, 5);
    QueryWeatherEveryNP.fill(getDeviceName(), "QUERY_WEATHER", "Query Weather", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /* Firmware version */
    FWversionTP[0].fill("FIRMWARE", "Firmware Version", nullptr);
    FWversionTP.fill(getDeviceName(), "FW_VERSION", "Firmware", CONNECTION_TAB, IP_RO, 0, IPS_IDLE);

    /* Weather Query API Key*/
    WeatherQueryAPIKeyTP[0].fill("API_KEY", "Weather API Key", nullptr);
    WeatherQueryAPIKeyTP.fill(getDeviceName(), "WEATHER_API_KEY", "Weather API Key", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Latitude */
    LatitudeNP[0].fill("LAT", "Decimal Degrees", "%4.2f", -90.0, 90.0, 0., 0.);
    LatitudeNP.fill(getDeviceName(), "LATITUDE", "Latitude", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Longitude */
    LongitudeNP[0].fill("LONG", "Decimal Degrees", "%4.2f", -180.0, 180.0, 0., 0.);
    LongitudeNP.fill(getDeviceName(), "LONGITUDE", "Longitude", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    /*  Location Name */
    LocationNameTP[0].fill("NAME", "Location Name", nullptr);
    LocationNameTP.fill(getDeviceName(), "LOCATION_NAME", "Weather", OPTIONS_TAB, IP_RO, 0, IPS_IDLE);

    /*  Location Name */
    WeatherUpdatedTP[0].fill("LAST_UPDATED", "Last Updated", nullptr);
    WeatherUpdatedTP.fill(getDeviceName(), "WEATHER_UPDATED", "Weather", OPTIONS_TAB, IP_RO, 0, IPS_IDLE);

    IUGetConfigText(getDeviceName(), "LOCATION_SNOOP", "LOCATION_DEVICE", locationDevice, MAXINDIDEVICE);
    IUGetConfigText(getDeviceName(), "LOCATION_SNOOP", "LOCATION_PROPERTY", locationProperty, MAXINDINAME);
    IUGetConfigText(getDeviceName(), "LOCATION_SNOOP", "LOCATION_LAT_ATTRIBUTE", locationLatAttribute, MAXINDINAME);
    IUGetConfigText(getDeviceName(), "LOCATION_SNOOP", "LOCATION_LON_ATTRIBUTE", locationLongAttribute, MAXINDINAME);

    IUGetConfigText(getDeviceName(), "TEMPERATURE_SNOOP", "TEMPERATURE_DEVICE", temperatureDevice, MAXINDIDEVICE);
    IUGetConfigText(getDeviceName(), "TEMPERATURE_SNOOP", "TEMPERATURE_PROPERTY", temperatureProperty, MAXINDINAME);
    IUGetConfigText(getDeviceName(), "TEMPERATURE_SNOOP", "TEMPERATURE_ATTRIBUTE", temperatureAttribute, MAXINDINAME);

    if (IUGetConfigOnSwitchIndex(getDeviceName(), "ENABLE_SNOOP_LOCATION", &snoopLocationIndex) == 0)
        setSnoopLocation = (snoopLocationIndex == 0);
    if (IUGetConfigOnSwitchIndex(getDeviceName(), "ENABLE_SNOOP_TEMPERATURE", &snoopTemperatureIndex) == 0) 
        setSnoopTemperature = (snoopTemperatureIndex == 0);

    /* Enable/Disable Snoop pickup for Latitude/Longitude and Temperature */
    EnableSnoopLocationSP[0].fill("ENABLE", "Enable", ISS_OFF);
    EnableSnoopLocationSP[1].fill("DISABLE", "Disable", ISS_ON);
    EnableSnoopLocationSP.fill(getDeviceName(), "ENABLE_SNOOP_LOCATION", "Snoop Location", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    SnoopLocationDeviceTP[0].fill("LOCATION_DEVICE", "Device", locationDevice);
    SnoopLocationDeviceTP[1].fill("LOCATION_PROPERTY", "Property", locationProperty);
    SnoopLocationDeviceTP[2].fill("LOCATION_LAT_ATTRIBUTE", "LAT Attribute", locationLatAttribute);
    SnoopLocationDeviceTP[3].fill("LOCATION_LONG_ATTRIBUTE", "LONG Attribute", locationLongAttribute);
    SnoopLocationDeviceTP.fill(getDeviceName(), "LOCATION_SNOOP", "Location Device", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    EnableSnoopTemperatureSP[0].fill("ENABLE", "Enable", ISS_OFF);
    EnableSnoopTemperatureSP[1].fill("DISABLE", "Disable", ISS_ON);
    EnableSnoopTemperatureSP.fill(getDeviceName(), "ENABLE_SNOOP_TEMPERATURE", "Snoop Temp", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    SnoopTemperatureDeviceTP[0].fill("TEMPERATURE_DEVICE", "Device", temperatureDevice);
    SnoopTemperatureDeviceTP[1].fill("TEMPERATURE_Property", "Property", temperatureProperty);
    SnoopTemperatureDeviceTP[2].fill("TEMPERATURE_ATTRIBUTE", "Attribute", temperatureAttribute);
    SnoopTemperatureDeviceTP.fill(getDeviceName(), "TEMPERATURE_SNOOP", "Temperature Device", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Refresh
    RefreshSP[0].fill("REFRESH", "Refresh", ISS_OFF);
    RefreshSP.fill(getDeviceName(),  "CHEAPODC_REFRESH", "CheapoDC", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    setDriverInterface(AUX_INTERFACE);

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

    if (isConnected())
    {

        // Main Control Tab
        defineProperty(ControllerModeSP);
        defineProperty(OutputPowerNP);
        defineProperty(TemperatureModeSP);
        defineProperty(XtrnTemperatureNP);
        defineProperty(SetPointModeSP);
        defineProperty(HumidityNP);
        defineProperty(DewpointNP);
        defineProperty(SetPointTemperatureNP);
        defineProperty(RefreshSP);
        // Options Tab
        defineProperty(MinimumOutputNP);
        defineProperty(MaximumOutputNP);
        defineProperty(TrackPointOffsetNP);
        defineProperty(TrackingRangeNP);
        defineProperty(UpdateOutputEveryNP);
        defineProperty(QueryWeatherEveryNP);
        // Get weather source to determin if API Key needed
        getWeatherSource();
        if (usingOpenWeather)
        {
            defineProperty(WeatherQueryAPIKeyTP);
            defineProperty(LocationNameTP);
        }
        defineProperty(WeatherUpdatedTP);
        defineProperty(LatitudeNP);
        defineProperty(LongitudeNP);
        defineProperty(EnableSnoopLocationSP);
        defineProperty(SnoopLocationDeviceTP);
        defineProperty(EnableSnoopTemperatureSP);
        defineProperty(SnoopTemperatureDeviceTP);

        // Connection Tab
        defineProperty(FWversionTP);

        loadConfig(true);
        readSettings();
        LOG_INFO("CheapoDC parameters updated, device ready for use.");
        timerIndex = SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(OutputPowerNP);
        deleteProperty(MinimumOutputNP);
        deleteProperty(MaximumOutputNP);
        deleteProperty(ControllerModeSP);
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
        if (usingOpenWeather)
        {
            deleteProperty(WeatherQueryAPIKeyTP);
            deleteProperty(LocationNameTP);
        }
        deleteProperty(WeatherUpdatedTP);
        deleteProperty(LatitudeNP);
        deleteProperty(LongitudeNP);
        deleteProperty(FWversionTP);
        deleteProperty(EnableSnoopLocationSP);
        deleteProperty(SnoopLocationDeviceTP);
        deleteProperty(EnableSnoopTemperatureSP);
        deleteProperty(SnoopTemperatureDeviceTP);
    }

    return true;
}

void CheapoDC::redrawMainControl()
{
    // Main Control Tab delete properties
    deleteProperty(ControllerModeSP);
    deleteProperty(OutputPowerNP);
    deleteProperty(TemperatureModeSP);
    deleteProperty(XtrnTemperatureNP);
    deleteProperty(SetPointModeSP);
    deleteProperty(HumidityNP);
    deleteProperty(DewpointNP);
    deleteProperty(SetPointTemperatureNP);
    deleteProperty(RefreshSP);

    // Main Control Tab re-define properties to pick up changes and maintain order
    defineProperty(ControllerModeSP);
    defineProperty(OutputPowerNP);
    defineProperty(TemperatureModeSP);
    defineProperty(XtrnTemperatureNP);
    defineProperty(SetPointModeSP);
    defineProperty(HumidityNP);
    defineProperty(DewpointNP);
    defineProperty(SetPointTemperatureNP);
    defineProperty(RefreshSP);

    doMainControlRedraw = false;
}

void CheapoDC::redrawOptions()
{
    // Options Tab delete properties
    deleteProperty(MinimumOutputNP);
    deleteProperty(MaximumOutputNP);
    deleteProperty(TrackPointOffsetNP);
    deleteProperty(TrackingRangeNP);
    deleteProperty(UpdateOutputEveryNP);
    deleteProperty(QueryWeatherEveryNP);
    if (usingOpenWeather)
    {
        deleteProperty(WeatherQueryAPIKeyTP);
        deleteProperty(LocationNameTP);
    }
    deleteProperty(WeatherUpdatedTP);
    deleteProperty(LatitudeNP);
    deleteProperty(LongitudeNP);
    deleteProperty(FWversionTP);
    deleteProperty(EnableSnoopLocationSP);
    deleteProperty(SnoopLocationDeviceTP);
    deleteProperty(EnableSnoopTemperatureSP);
    deleteProperty(SnoopTemperatureDeviceTP);

    // Options Tab re-define properties to pick up changes and maintain order
    defineProperty(MinimumOutputNP);
    defineProperty(MaximumOutputNP);
    defineProperty(TrackPointOffsetNP);
    defineProperty(TrackingRangeNP);
    defineProperty(UpdateOutputEveryNP);
    defineProperty(QueryWeatherEveryNP);
    if (usingOpenWeather)
    {
        defineProperty(WeatherQueryAPIKeyTP);
        defineProperty(LocationNameTP);
    }
    defineProperty(WeatherUpdatedTP);
    defineProperty(LatitudeNP);
    defineProperty(LongitudeNP);
    defineProperty(EnableSnoopLocationSP);
    defineProperty(SnoopLocationDeviceTP);
    defineProperty(EnableSnoopTemperatureSP);
    defineProperty(SnoopTemperatureDeviceTP);

    doOptionsRedraw = false;
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

    LOGF_DEBUG("CMD <%s>", cmd);
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
            LOGF_ERROR("Error reading response for command <%s>: %s.", cmd, errstr);
            return false;
        }

        if (nbytes_read < 2)
        {
            LOGF_ERROR("Invalid response <%s> for command <%s>.", resp, cmd);
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
                LOGF_ERROR("Error parsing GET %s response for value: %s Error: %s", cmd, getResponse, e.what());
                return false;
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
            LOG_INFO("CheapoDC is online. Getting device parameters...");
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

    if (!this->sendGetCommand(CDC_CMD_FW, resp))
        return false;

    FWversionTP[0].setText(resp);
    FWversionTP.setState(IPS_OK);
    FWversionTP.apply();

    return true;
}

void CheapoDC::getWeatherSource()
{
    char resp[CDC_RESPONSE_LENGTH] = {};

    if (sendGetCommand(CDC_CMD_WS, resp))
        usingOpenWeather = (strcmp(resp, "OpenWeather") == 0);
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
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

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

bool CheapoDC::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    bool result = false;
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (ControllerModeSP.isNameMatch(name))
    {
        ControllerModeSP.update(states, names, n);
        ControllerModeSP.setState(IPS_BUSY);
        ControllerModeSP.apply();
        result = setControllerMode(ControllerModeSP.findOnSwitchIndex());
        readSettings();
        return result;
    }

    if (TemperatureModeSP.isNameMatch(name))
    {
        TemperatureModeSP.update(states, names, n);
        TemperatureModeSP.setState(IPS_BUSY);
        TemperatureModeSP.apply();
        result = setTemperatureMode(TemperatureModeSP.findOnSwitchIndex());
        readSettings();
        return result;
    }

    if (SetPointModeSP.isNameMatch(name))
    {
        SetPointModeSP.update(states, names, n);
        SetPointModeSP.setState(IPS_BUSY);
        SetPointModeSP.apply();
        result = setSetPointMode(SetPointModeSP.findOnSwitchIndex());
        readSettings();
        return result;
    }

    if (EnableSnoopLocationSP.isNameMatch(name))
    {
        EnableSnoopLocationSP.update(states, names, n);
        EnableSnoopLocationSP.setState(IPS_BUSY);
        snoopLocationIndex = EnableSnoopLocationSP.findOnSwitchIndex();
        EnableSnoopLocationSP.apply();
        setSnoopLocation = (snoopLocationIndex == 0);
        result = readSettings();

        return result;
    }

    if (EnableSnoopTemperatureSP.isNameMatch(name))
    {
        EnableSnoopTemperatureSP.update(states, names, n);
        EnableSnoopTemperatureSP.setState(IPS_BUSY);
        snoopTemperatureIndex = EnableSnoopTemperatureSP.findOnSwitchIndex();
        EnableSnoopTemperatureSP.apply();
        setSnoopTemperature = (snoopTemperatureIndex == 0);
        result = readSettings();

        return result;
    }

    if (RefreshSP.isNameMatch(name))
    {
        char buf[3] = {"NA"};

        RefreshSP.update(states, names, n);
        RefreshSP.setState(IPS_BUSY);
        RefreshSP.apply();
        result = sendSetCommand(CDC_CMD_QN, buf);
        return readSettings();
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

bool CheapoDC::setMinimumOutput(int value)
{
    if (value >= MaximumOutputNP[0].getValue())
        return false;
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
        return false;
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
    if ((value < -180.0) || (value > 180.0))
        return false;
    else
    {
        char valBuf[CDC_SET_VALUE_LENGTH] = {};

        sprintf(valBuf, CDC_FLOAT_VALUE, value);
        return sendSetCommand(CDC_CMD_LON, valBuf);
    }
}

bool CheapoDC::setExternalTemperature(float value)
{
    char valBuf[CDC_SET_VALUE_LENGTH] = {};

    sprintf(valBuf, CDC_FLOAT_VALUE, value);
    return sendSetCommand(CDC_CMD_ATPX, valBuf);
}

bool CheapoDC::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    bool result = false;

    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (TrackPointOffsetNP.isNameMatch(name))
    {
        TrackPointOffsetNP.update(values, names, n);
        TrackPointOffsetNP.setState(IPS_BUSY);
        TrackPointOffsetNP.apply();
        result = setTrackPointOffset(TrackPointOffsetNP[0].getValue());
        readSettings();
        return result;
    }

    if (TrackingRangeNP.isNameMatch(name))
    {
        TrackingRangeNP.update(values, names, n);
        TrackingRangeNP.setState(IPS_BUSY);
        TrackingRangeNP.apply();
        result = setTrackingRange(TrackingRangeNP[0].getValue());
        readSettings();
        return result;
    }

    if (OutputPowerNP.isNameMatch(name))
    {
        if (ControllerModeSP.findOnSwitchIndex() == MANUAL)
        {
            int minOutput = MinimumOutputNP[0].getValue();
            int maxOutput = MaximumOutputNP[0].getValue();

            OutputPowerNP.update(values, names, n);

            if ((minOutput <= OutputPowerNP[0].getValue()) && (maxOutput >= OutputPowerNP[0].getValue()))
            {
                OutputPowerNP.setState(IPS_BUSY);
                OutputPowerNP.apply();

                result = setOutput(OutputPowerNP[0].getValue());
            }
            else
            {
                LOGF_WARN("Output must be >= Minimum Output (%d) and <= MaximumOutput (%d).", minOutput, maxOutput);
                result = false;
            }
            readSettings();
            return result;
        }
        else
        {
            LOG_WARN("Controller Mode must be set to Manual to set Output Power.");
            readSettings();
            return false;
        }
    }

    if (MinimumOutputNP.isNameMatch(name))
    {
        MinimumOutputNP.update(values, names, n);
        MinimumOutputNP.setState(IPS_BUSY);
        MinimumOutputNP.apply();
        result = setMinimumOutput(MinimumOutputNP[0].getValue());
        doMainControlRedraw = true;
        doOptionsRedraw = true;
        readSettings();
        return result;
    }

    if (MaximumOutputNP.isNameMatch(name))
    {
        MaximumOutputNP.update(values, names, n);
        MaximumOutputNP.setState(IPS_BUSY);
        MaximumOutputNP.apply();
        result = setMaximumOutput(MaximumOutputNP[0].getValue());
        doMainControlRedraw = true;
        doOptionsRedraw = true;
        readSettings();
        return result;
    }

    if (SetPointTemperatureNP.isNameMatch(name))
    {
        SetPointTemperatureNP.update(values, names, n);
        SetPointTemperatureNP.setState(IPS_BUSY);
        SetPointTemperatureNP.apply();
        result = setSetPoint(SetPointTemperatureNP[0].getValue());
        readSettings();
        return result;
    }

    if (LatitudeNP.isNameMatch(name))
    {
        LatitudeNP.update(values, names, n);
        LatitudeNP.setState(IPS_BUSY);
        LatitudeNP.apply();
        result = setLatitude(LatitudeNP[0].getValue());
        readSettings();
        return result;
    }

    if (LongitudeNP.isNameMatch(name))
    {
        LongitudeNP.update(values, names, n);
        LongitudeNP.setState(IPS_BUSY);
        LongitudeNP.apply();
        result = setLongitude(LongitudeNP[0].getValue());
        readSettings();
        return result;
    }

    if (UpdateOutputEveryNP.isNameMatch(name))
    {
        UpdateOutputEveryNP.update(values, names, n);
        UpdateOutputEveryNP.setState(IPS_BUSY);
        UpdateOutputEveryNP.apply();
        result = setUpdateOutputEvery(UpdateOutputEveryNP[0].getValue());
        readSettings();
        return result;
    }

    if (QueryWeatherEveryNP.isNameMatch(name))
    {
        QueryWeatherEveryNP.update(values, names, n);
        QueryWeatherEveryNP.setState(IPS_BUSY);
        QueryWeatherEveryNP.apply();
        result = setWeatherQueryEvery(QueryWeatherEveryNP[0].getValue());
        readSettings();
        return result;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool CheapoDC::setSnoopLocationDevice(const char *device, const char *property, const char *latAttribute, const char *lonAttribute)
{
    bool newSnoop = false;
    if (strcmp(device, locationDevice) != 0)
    {
        indi_strlcpy(locationDevice, device, sizeof(locationDevice));
        newSnoop = true;
    }

    if (strcmp(property, locationProperty) != 0)
    {
        indi_strlcpy(locationProperty, property, sizeof(locationProperty));
        newSnoop = true;
    }

    if (strcmp(latAttribute, locationLatAttribute) != 0)
        indi_strlcpy(locationLatAttribute, latAttribute, sizeof(locationLatAttribute));

    if (strcmp(lonAttribute, locationLongAttribute) != 0)
        indi_strlcpy(locationLongAttribute, lonAttribute, sizeof(locationLongAttribute));

    if (newSnoop)
    {
        IDSnoopDevice(locationDevice, locationProperty);
        LOGF_INFO("Update location device, %s : %s", locationDevice, locationProperty);
    }

    return true;
}

bool CheapoDC::setSnoopTemperatureDevice(const char *device, const char *property, const char *attribute)
{
    bool newSnoop = false;
    if (strcmp(device, temperatureDevice) != 0)
    {
        indi_strlcpy(temperatureDevice, device, sizeof(temperatureDevice));
        newSnoop = true;
    }

    if (strcmp(property, temperatureProperty) != 0)
    {
        indi_strlcpy(temperatureProperty, property, sizeof(temperatureProperty));
        newSnoop = true;
    }

    if (strcmp(attribute, temperatureAttribute) != 0)
        indi_strlcpy(temperatureAttribute, attribute, sizeof(temperatureAttribute));

    if (newSnoop)
    {
        IDSnoopDevice(temperatureDevice, temperatureProperty);
        LOGF_INFO("Update temperature device, %s : %s", temperatureDevice, temperatureProperty);
    }

    return true;
}

bool CheapoDC::setWeatherQueryAPIKey(const char *key)
{

    char valBuf[CDC_SET_VALUE_LENGTH] = {};

    sprintf(valBuf, CDC_TEXT_VALUE, key);
    return sendSetCommand(CDC_CMD_WKEY, valBuf);
}

bool CheapoDC::setLocationName(const char *name)
{

    char valBuf[CDC_SET_VALUE_LENGTH] = {};

    sprintf(valBuf, CDC_TEXT_VALUE, name);
    return sendSetCommand(CDC_CMD_WUL, valBuf);
}

bool CheapoDC::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    bool result = false;

    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (usingOpenWeather && (WeatherQueryAPIKeyTP.isNameMatch(name)))
    {
        WeatherQueryAPIKeyTP.update(texts, names, n);
        WeatherQueryAPIKeyTP.setState(IPS_OK);
        WeatherQueryAPIKeyTP.apply();
        result = setWeatherQueryAPIKey(WeatherQueryAPIKeyTP[0].getText());
        return result;
    }

    if (SnoopTemperatureDeviceTP.isNameMatch(name))
    {
        SnoopTemperatureDeviceTP.update(texts, names, n);
        SnoopTemperatureDeviceTP.setState(IPS_OK);
        SnoopTemperatureDeviceTP.apply();
        result = setSnoopTemperatureDevice(SnoopTemperatureDeviceTP[0].getText(),
                                           SnoopTemperatureDeviceTP[1].getText(),
                                           SnoopTemperatureDeviceTP[2].getText());
        return result;
    }

    if (SnoopLocationDeviceTP.isNameMatch(name))
    {
        SnoopLocationDeviceTP.update(texts, names, n);
        SnoopLocationDeviceTP.setState(IPS_OK);
        SnoopLocationDeviceTP.apply();
        result = setSnoopLocationDevice(SnoopLocationDeviceTP[0].getText(),
                                        SnoopLocationDeviceTP[1].getText(),
                                        SnoopLocationDeviceTP[2].getText(),
                                        SnoopLocationDeviceTP[3].getText());
        return result;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool CheapoDC::ISSnoopDevice(XMLEle *root)
{
    XMLEle *ep = nullptr;
    const char *propName = findXMLAttValu(root, "name");
    const char *deviceName = findXMLAttValu(root, "device");
    bool result = false;

    if (!(cdcConnection & CONNECTION_TCP))
    {
        return true;
    }

    if ((!strcmp(propName, temperatureProperty)) && (!strcmp(deviceName, temperatureDevice)) && (!snoopTemperatureIndex))
    {
        bool tempAtributeFound = false;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *name = findXMLAttValu(ep, "name");

            if (!strcmp(name, temperatureAttribute))
            {
                float FocuserTemp = atof(pcdataXMLEle(ep));

                result = setExternalTemperature(FocuserTemp);
                tempAtributeFound = true;
            }
        }

        if (!tempAtributeFound)
            LOGF_WARN("TEMPERATURE attribute, %s, not found for %s:%s", temperatureAttribute, temperatureDevice, temperatureProperty);
    }

    if ((!strcmp(propName, locationProperty)) && (!strcmp(deviceName, locationDevice)) && (!snoopLocationIndex))
    {
        bool latAtributeFound = false;
        bool longAtributeFound = false;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char *name = findXMLAttValu(ep, "name");

            if (!strcmp(name, locationLongAttribute))
            {
                float Longitude = atof(pcdataXMLEle(ep));
                if (Longitude > 180)
                    Longitude -= 360;

                result = setLongitude(Longitude);
                longAtributeFound = true;
            }
            else if (!strcmp(name, locationLatAttribute))
            {
                float Latitude = atof(pcdataXMLEle(ep));

                result = setLatitude(Latitude);
                latAtributeFound = true;
            }
        }
        if (!longAtributeFound)
            LOGF_WARN("LONG attribute, %s, not found for %s:%s", locationLongAttribute, locationDevice, locationProperty);
        if (!latAtributeFound)
            LOGF_WARN("LAT attribute, %s, not found for %s:%s", locationLatAttribute, locationDevice, locationProperty);
    }

    return result;
}

bool CheapoDC::readSettings()
{
    char resp[CDC_RESPONSE_LENGTH] = {};
    char dateBuf[32] = {};
    char timeBuf[32] = {};
    int ok = -1;
    float temp_ambient, temp_external, humidity, dewpoint, setPoint, trackingRange;
    unsigned int output, minOutput, maxOutput, updatePeriod;
    float trackPointOffset, latitude, longitude;
    unsigned int controllerMode, temperatureMode, setPointMode, queryPeriod;

    EnableSnoopLocationSP.reset();
    EnableSnoopLocationSP[snoopLocationIndex].setState(ISS_ON);
    EnableSnoopLocationSP.setState(IPS_OK);
    EnableSnoopLocationSP.apply();
    if (prevSnoopLocationIndex != snoopLocationIndex)
    {
        if (snoopLocationIndex == 1)
        {
            LongitudeNP.setPermission(IP_RW);
            LatitudeNP.setPermission(IP_RW);
        }
        else
        {
            LongitudeNP.setPermission(IP_RO);
            LatitudeNP.setPermission(IP_RO);
        }
        prevSnoopLocationIndex = snoopLocationIndex;
        doOptionsRedraw = true;
    }

    EnableSnoopTemperatureSP.reset();
    EnableSnoopTemperatureSP[snoopTemperatureIndex].setState(ISS_ON);
    EnableSnoopTemperatureSP.setState(IPS_OK);
    EnableSnoopTemperatureSP.apply();

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
        LOGF_ERROR("Get Humidity: Response <%s> for Command <%s> not valid.", resp, CDC_CMD_HU);

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
        LOGF_ERROR("Get Dew point: Response <%s> for Command <%s> not valid.", resp, CDC_CMD_DP);
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
        LOGF_ERROR("Get Set Point: Response <%s> for Command <%s> not valid.", resp, CDC_CMD_SP);

    // Get Power output
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_DCO, resp))
        return false;

    ok = sscanf(resp, "%d", &output);

    if (ok == 1)
    {
        OutputPowerNP[0].setValue(output);
        OutputPowerNP.setState(IPS_OK);
        OutputPowerNP.apply();
    }
    else
        LOGF_ERROR("Get Power Output: Response <%s> for Command <%s> invalid.", resp, CDC_CMD_DCO);

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
            OutputPowerNP[0].setMin(minOutput);
            OutputPowerNP.apply();
            MaximumOutputNP[0].setMin(minOutput + 1);
            MaximumOutputNP.apply();
            doMainControlRedraw = true;
            doOptionsRedraw = true;
            prevMinOutput = minOutput;
        }
        MinimumOutputNP.setState(IPS_OK);
        MinimumOutputNP.apply();
    }
    else
        LOGF_ERROR("Get Minimum Output: Response <%s> for Command <%s> invalid.", resp, CDC_CMD_OMIN);

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
            OutputPowerNP[0].setMax(maxOutput);
            OutputPowerNP.apply();
            MinimumOutputNP[0].setMax(maxOutput - 1);
            MinimumOutputNP.apply();
            doMainControlRedraw = true;
            doOptionsRedraw = true;
            prevMaxOutput = maxOutput;
        }
        MaximumOutputNP.setState(IPS_OK);
        MaximumOutputNP.apply();
    }
    else
        LOGF_ERROR("Get Maximum Output: Response <%s> for Command <%s> invalid.", resp, CDC_CMD_OMAX);

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
        LOGF_ERROR("Get Track Point Offset: Response <%s> for Command <%s> invalid.", resp, CDC_CMD_TPO);

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
        LOGF_ERROR("Get Update Output Every: Response <%s> for Command <%s> invalid.", resp, CDC_CMD_TKR);

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
        LOGF_ERROR("Get Query Weather Every: Response <%s> for Command <%s> invalid.", resp, CDC_CMD_UOE);

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
        LOGF_ERROR("Get Query Weather Every: Response <%s> for Command <%s> invalid.", resp, CDC_CMD_WQE);

    // Get Latitude
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_LAT, resp))
        return false;

    ok = sscanf(resp, "%f", &latitude);

    if ((ok == 1) && (latitude >= -90.0) && (latitude <= 90.0))
    {
        LatitudeNP[0].setValue(latitude);
        LatitudeNP.setState(IPS_OK);
        LatitudeNP.apply();
    }
    else
        LOGF_ERROR("Get Latitude: Response <%s> for Command <%s> invalid.", resp, CDC_CMD_LAT);

    // Get Longitude
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_LON, resp))
        return false;

    ok = sscanf(resp, "%f", &longitude);

    if ((ok == 1) && (longitude >= -180.0) && (longitude <= 180.0))
    {
        LongitudeNP[0].setValue(longitude);
        LongitudeNP.setState(IPS_OK);
        LongitudeNP.apply();
    }
    else
        LOGF_ERROR("Get Longitude: Response <%s> for Command <%s> invalid.", resp, CDC_CMD_LON);

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
        LOGF_ERROR("Get Set Point Mode: Response <%s> for Command <%s> not valid.", resp, CDC_CMD_SPM);

    // Get API Key if using OpenWeather
    if (usingOpenWeather)
    {
        memset(resp, '\0', sizeof(resp));

        if (!sendGetCommand(CDC_CMD_WKEY, resp))
            return false;

        WeatherQueryAPIKeyTP[0].setText(resp);
        WeatherQueryAPIKeyTP.setState(IPS_OK);
        WeatherQueryAPIKeyTP.apply();

        // Get Query Station name also only if using OpenWeather
        memset(resp, '\0', sizeof(resp));

        if (!sendGetCommand(CDC_CMD_WUL, resp))
            return false;

        LocationNameTP[0].setText(resp);
        LocationNameTP.setState(IPS_OK);
        LocationNameTP.apply();
    }
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

    // Snoop Location Device
    SnoopLocationDeviceTP[0].setText(locationDevice);
    SnoopLocationDeviceTP[1].setText(locationProperty);
    SnoopLocationDeviceTP[2].setText(locationLatAttribute);
    SnoopLocationDeviceTP[3].setText(locationLongAttribute);
    SnoopLocationDeviceTP.setState(IPS_OK);
    SnoopLocationDeviceTP.apply();

    // Update for Snoop location device when driver loads
    if (setSnoopLocation)
    {
        IDSnoopDevice(locationDevice, locationProperty);
        setSnoopLocation = false;
    }

    // Snoop Temperature Device
    SnoopTemperatureDeviceTP[0].setText(temperatureDevice);
    SnoopTemperatureDeviceTP[1].setText(temperatureProperty);
    SnoopTemperatureDeviceTP[2].setText(temperatureAttribute);
    SnoopTemperatureDeviceTP.setState(IPS_OK);
    SnoopTemperatureDeviceTP.apply();

    // Update for Snoop temperature device when driver loads
    if (setSnoopTemperature)
    {
        IDSnoopDevice(temperatureDevice, temperatureProperty);
        setSnoopTemperature = false;
    }

    // Get Controller Mode
    memset(resp, '\0', sizeof(resp));

    if (!sendGetCommand(CDC_CMD_DCM, resp))
        return false;

    ok = sscanf(resp, "%d", &controllerMode);

    if ((ok == 1) && (controllerMode <= OFF))
    {
        ControllerModeSP.reset();
        ControllerModeSP[controllerMode].setState(ISS_ON);
        ControllerModeSP.setState(IPS_OK);
        ControllerModeSP.apply();

        if (controllerMode != previousControllerMode)
        {

            if (controllerMode == MANUAL)
            {
                OutputPowerNP.setPermission(IP_RW);
                OutputPowerNP.apply();
                doMainControlRedraw = true;
            }
            if (previousControllerMode == MANUAL)
            {
                OutputPowerNP.setPermission(IP_RO);
                OutputPowerNP.apply();
                doMainControlRedraw = true;
            }
            previousControllerMode = controllerMode;
        }
    }
    else
        LOGF_ERROR("Get Controller Mode: Response <%s> for Command <%s> not valid.", resp, CDC_CMD_DCM);

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
        LOGF_ERROR("Get Temperature Mode: Response <%s> for Command <%s> not valid.", resp, CDC_CMD_DCTM);

    // Refresh
    RefreshSP.reset();
    RefreshSP[0].setState(ISS_OFF);
    RefreshSP.setState(IPS_OK);
    RefreshSP.apply();

    // Check to see if any properties need to be redrawn
    if (doMainControlRedraw)
        redrawMainControl();

    if (doOptionsRedraw)
        redrawOptions();

    return true;
}

bool CheapoDC::saveConfigItems(FILE *fp)
{

    EnableSnoopLocationSP.save(fp);
    EnableSnoopTemperatureSP.save(fp);
    SnoopLocationDeviceTP.save(fp);
    SnoopTemperatureDeviceTP.save(fp);

    return INDI::DefaultDevice::saveConfigItems(fp);
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
