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

#pragma once

#include <defaultdevice.h>
#include "indipropertytext.h"

#include <time.h>  // for nsleep()
#include <errno.h> // for nsleep()

/* 
    Version Change Information
    V1.0
        - Initial release supports CheapoDC firmware 1.X features
    V1.1
        - release to add support for CheapoDC firmware 2.x features
        - bug fix for Longitude range checking
        - adds support for Weather Device snoop for local temperature/humidity instead of using CheapoDC weather query
        - move Latitude/Longitude settings to more common Site Management Tab with Location settings
        - Location Snoop enabled by default for Telescope Simulator 
*/
#define CHEAPODC_VERSION_MAJOR 1
#define CHEAPODC_VERSION_MINOR 1

// CheapoDC Commands used
#define CDC_CMD_ATPQ "ATPQ" // ambient temperature - float %3.2f
#define CDC_CMD_HU "HU"     // humidity - float %3.2f
#define CDC_CMD_DP "DP"     // Dew point - float %3.2f
#define CDC_CMD_SP "SP"     // set point - float %3.2f
#define CDC_CMD_TPO "TPO"   // Track Point offset - float %2.2f (-5.0 to 5.0)
#define CDC_CMD_TKR "TKR"   // Tracking range - float %2.2f (4.0 to 10.0)
#define CDC_CMD_DCO "DCO"   // Dew Controller Output- int (0 to 100)
#define CDC_CMD_WS "WS"     // Weather source - string
#define CDC_CMD_QN "QN"     // Query Weather Now (Set only command)
#define CDC_CMD_FW "FW"     // firmware version - string
#define CDC_CMD_DCM "DCM"   // dew controller mode
#define CDC_CMD_DCTM "DCTM" // dew controller temperature mode
#define CDC_CMD_SPM "SPM"   // dew controller set point mode
#define CDC_CMD_WQE "WQE"   // Weather query every
#define CDC_CMD_UOE "UOE"   // update outputs every
#define CDC_CMD_WAPI "WAPI" // Weather API URL
#define CDC_CMD_WKEY "WKEY" // Weather API Key
#define CDC_CMD_LAT "LAT"   // Location latitude
#define CDC_CMD_LON "LON"   // Location longitude
#define CDC_CMD_LNM "LNM"   // Location name
#define CDC_CMD_OMIN "OMIN" // DC Min output
#define CDC_CMD_OMAX "OMAX" // DC Max output
#define CDC_CMD_ATPX "ATPX" // Local Temperature input by external app
#define CDC_CMD_CTP "CTP"   // Current Track Point Temperature
#define CDC_CMD_WUL "WUL"   // Get Weather Query Station name
#define CDC_CMD_LWUD "LWUD" // date of last weather update (in Weather Staion time zone)
#define CDC_CMD_LWUT "LWUT" // time of last weather update (in Weather Staion time zone)
#define CDC_CMD_WQEN "WQEN" // Weather Query Enabled (false=0, true=1)

#define CDC_GET_COMMAND "{\"GET\":\"%s\"}"
#define CDC_SET_COMMAND "{\"SET\":{\"%s\":\"%s\"}}"
#define CDC_RESPONSE_LENGTH 512
#define CDC_COMMAND_LENGTH 512
#define CDC_SET_VALUE_LENGTH 256
#define CDC_INT_VALUE "%d"
#define CDC_FLOAT_VALUE "%3.2f"
#define CDC_TEXT_VALUE "%s"
#define CDC_DEFAULT_POLLING_PERIOD 30000 // in msec, 30 seconds is often enough for Dew Control
#define CDC_DEFAULT_HOST "cheapodc.local" // default host for connection tab
#define CDC_DEFAULT_PORT 58000 // default TCP port for connection tab

/******************************************************************************/

class CheapoDC : public INDI::DefaultDevice
{
public:
    CheapoDC();
    virtual ~CheapoDC() = default;

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISSnoopDevice(XMLEle *root) override;
    virtual void TimerHit() override;

    /**
     * @struct CDCConnection
     * @brief Holds the connection mode of the device.
     */
    typedef enum
    {
        CONNECTION_NONE = 1 << 0,
        CONNECTION_SERIAL = 1 << 1,
        CONNECTION_TCP = 1 << 2
    } CDCConnection;

private:
    enum controllerMode
    {
        AUTOMATIC = 0,
        MANUAL,
        OFF,
    };

    enum temperatureMode
    {
        WEATHER_QUERY = 0,
        EXTERNAL_INPUT
    };

    enum setPointMode
    {
        DEWPOINT = 0,
        TEMPERATURE,
        MIDPOINT
    };

    enum weatherSource
    {
        OPENMETEO = 0,
        OPENWEATHER,
        EXTERNALSOURCE
    };

    enum CheapoDCLocation
        {
            LOCATION_LATITUDE,
            LOCATION_LONGITUDE
        };

    bool fwVOneDetected = false;
    int timerIndex;
    unsigned int previousControllerMode = MANUAL;
    unsigned int prevMinOutput = 0;
    unsigned int prevMaxOutput = 100;
    int snoopLocationIndex = INDI_ENABLED; // Default is Enabled
    int snoopTemperatureIndex = INDI_DISABLED; // Default is Disabled
    int previousSnoopTemperatureIndex = snoopTemperatureIndex; 
    unsigned int previousTemperatureMode = WEATHER_QUERY;
    int snoopWeatherIndex = INDI_DISABLED; // Default is Disabled
    int previousSnoopWeatherIndex = snoopWeatherIndex;
    char locationDevice[MAXINDIDEVICE] = {"Telescope Simulator"};
    char locationProperty[MAXINDINAME] = {"GEOGRAPHIC_COORD"};
    char locationLatAttribute[MAXINDINAME] = {"LAT"};
    char locationLongAttribute[MAXINDINAME] = {"LONG"};
    char temperatureDevice[MAXINDIDEVICE] = {"Focuser Simulator"};
    char temperatureProperty[MAXINDINAME] = {"FOCUS_TEMPERATURE"};
    char temperatureAttribute[MAXINDINAME] = {"TEMPERATURE"};
    char weatherDevice[MAXINDIDEVICE] = {"Weather Simulator"};
    char weatherProperty[MAXINDINAME] = {"WEATHER_PARAMETERS"};
    char weatherTempAttribute[MAXINDINAME] = {"WEATHER_TEMPERATURE"};
    char weatherHumidityAttribute[MAXINDINAME] = {"WEATHER_HUMIDITY"};
    bool setSnoopLocation = true; //defualt is enabled
    bool setSnoopTemperature = false;
    bool setSnoopWeather = false;
    bool usingOpenWeather = false;
    bool previouslyUsingOpenWeather = usingOpenWeather;
    bool usingExternalWeatherSource = false;
    bool previuoslyUsingExternalWeatherSource = usingExternalWeatherSource;
    bool doMainControlRedraw = false;
    bool doOptionsRedraw = false;

    int msleep(long duration);
    bool sendCommand(const char *cmd, char *response);
    bool sendGetCommand(const char *cmd, char *resp);
    bool sendSetCommand(const char *cmd, char *value);
    bool Handshake();
    bool Ack();
    bool readSettings();
    void redrawMainControl();
    void redrawOptions();
    void getWeatherSource();
    bool setWeatherSource(int value);
    bool setControllerMode(int value);
    bool setTemperatureMode(int value);
    bool setSetPointMode(int value);
    bool setSetPoint(float value);
    bool setTrackPointOffset(float value);
    bool setTrackingRange(float value);
    bool setOutput(int value);
    bool setMinimumOutput(int value);
    bool setMaximumOutput(int value);
    bool setUpdateOutputEvery(int value);
    bool setWeatherQueryEvery(int value);
    bool setWeatherQueryAPIKey(const char *key);
    bool setWeatherQueryEnabled(bool enabled);
    bool setLatitude(float value);
    bool setLongitude(float value);
    bool setLocation(float latitude, float longitude);
    bool setLocationName(const char *name);
    bool setExternalTemperature(float value);
    bool setWeatherTemperature(float value);
    bool setWeatherHumidity(float value);
    bool setSnoopLocationDevice(const char *device, const char *property, const char *latAttribute, const char *lonAttribute);
    bool setSnoopTemperatureDevice(const char *device, const char *property, const char *attribute);
    bool setSnoopWeatherDevice(const char *device, const char *property, const char *temperatureAttribute, const char *humidityAttribute);
    

    // Connection::Serial *serialConnection { nullptr };
    Connection::TCP *tcpConnection{nullptr};

    int PortFD{-1};

    uint8_t cdcConnection{CONNECTION_TCP};

    // CheapoDC Timeouts
    static const uint8_t CDC_READ_TIMEOUT{10}; // 10 seconds
    static const long CDC_SMALL_DELAY{50};     // 50ms delay from send command to read response

    INDI::PropertyNumber OutputPowerNP{1};
    INDI::PropertyNumber MinimumOutputNP{1};
    INDI::PropertyNumber MaximumOutputNP{1};
    INDI::PropertySwitch ControllerModeSP{3};
    INDI::PropertySwitch TemperatureModeSP{2};
    INDI::PropertySwitch SetPointModeSP{3};
    INDI::PropertyNumber XtrnTemperatureNP{2};
    INDI::PropertyNumber HumidityNP{1};
    INDI::PropertyNumber DewpointNP{1};
    INDI::PropertyNumber SetPointTemperatureNP{1};
    INDI::PropertyNumber TrackPointOffsetNP{1};
    INDI::PropertyNumber TrackingRangeNP{1};
    INDI::PropertyNumber UpdateOutputEveryNP{1};
    INDI::PropertyNumber QueryWeatherEveryNP{1};
    INDI::PropertySwitch WeatherSourceSP{3};
    INDI::PropertyText WeatherQueryAPIKeyTP{1};
    INDI::PropertyText LocationNameTP{1};
    INDI::PropertyText WeatherUpdatedTP{1};
    //INDI::PropertyNumber LongitudeNP{1};
    //INDI::PropertyNumber LatitudeNP{1};
    INDI::PropertyNumber LocationNP{2};
    INDI::PropertyText FWversionTP{1};
    INDI::PropertySwitch EnableSnoopLocationSP{2};
    INDI::PropertySwitch EnableSnoopTemperatureSP{2};
    INDI::PropertySwitch EnableSnoopWeatherSP{2};
    INDI::PropertyText SnoopLocationDeviceTP{4};
    INDI::PropertyText SnoopTemperatureDeviceTP{3};
    INDI::PropertyText SnoopWeatherDeviceTP{4};
    INDI::PropertySwitch RefreshSP{1};
};
