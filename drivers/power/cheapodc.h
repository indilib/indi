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
#include <indipowerinterface.h>

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
        - move to ActiveDevicesTP model for setting snoop devices
    V1.2
        - Add support for addtional Controller Pins supported in CheapoDC firmware 2.2.0.
        - Fix GET command processing to not throw a JSON error when an error response is received from device
    V2.0
        - Major refactor to move to INDI Power Interface
        - Support for new CheapoDC firmware 2.3.0 internal Humidity sensor capability
*/
#define CHEAPODC_VERSION_MAJOR 2
#define CHEAPODC_VERSION_MINOR 0

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
#define CDC_CMD_CDT "CDT"   // Location current date time
#define CDC_CMD_TMZ "TMZ"   // Location time zone offset in seconds
#define CDC_CMD_DST "DST"   // DST offset
#define CDC_CMD_OMIN "OMIN" // DC Min output
#define CDC_CMD_OMAX "OMAX" // DC Max output
#define CDC_CMD_ATPX "ATPX" // Local Temperature input by external app
#define CDC_CMD_CTP "CTP"   // Current Track Point Temperature
#define CDC_CMD_WUL "WUL"   // Get Weather Query Station name
#define CDC_CMD_LWUD "LWUD" // date of last weather update (in Weather Staion time zone)
#define CDC_CMD_LWUT "LWUT" // time of last weather update (in Weather Staion time zone)
#define CDC_CMD_WQEN "WQEN" // Weather Query Enabled (false=0, true=1)
// the following commands are stubs for the addtional controller output pins 2, 3, 4, 5. Inserting the
// pin number in the stub command string will form the correct CheapoDC command string.
#define CDC_CMD_CPM "CPM%d"   // Controller Pin Mode for pin x  (0=Disabled, 1=Controller, 2=PWM, 3=Boolean)
#define CDC_CMD_CPO "CPO%d"   // Controller Pin Output for pin x (0 to 100)
#define CDC_CMD_SDAP "SDAP"   // Internal Humidity sensor SDA Pin

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
#define CDC_MIN_ADDITIONAL_OUTPUT 2  // Addtional outputs start at 2
#define CDC_TOTAL_ADDITIONAL_OUTPUTS 4  // 4 addtional outputs - 2, 3, 4 and 5


// Snoop Device information
#define CDC_SNOOP_LOCATION_PROPERTY "GEOGRAPHIC_COORD"
#define CDC_SNOOP_LOCATION_LATITUDE "LAT"
#define CDC_SNOOP_LOCATION_LONGITUDE "LONG"
#define CDC_SNOOP_TIME_PROPERTY "TIME_UTC"
#define CDC_SNOOP_TIME_OFFSET "OFFSET"
#define CDC_SNOOP_FOCUSER_PROPERTY "FOCUS_TEMPERATURE"
#define CDC_SNOOP_FOCUSER_TEMPERATURE "TEMPERATURE"
#define CDC_SNOOP_WEATHER_PROPERTY "WEATHER_PARAMETERS"
#define CDC_SNOOP_WEATHER_TEMPERATURE "WEATHER_TEMPERATURE"
#define CDC_SNOOP_WEATHER_HUMIDITY "WEATHER_HUMIDITY"

/******************************************************************************/

class CheapoDC : public INDI::DefaultDevice, public INDI::PowerInterface
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

    protected:
        virtual bool SetPowerPort(size_t port, bool enabled) override;
        virtual bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
        virtual bool SetVariablePort(size_t port, bool enabled, double voltage) override;
        virtual bool SetLEDEnabled(bool enabled) override;
        virtual bool SetAutoDewEnabled(size_t port, bool enabled) override;
        virtual bool CyclePower() override;
        virtual bool SetUSBPort(size_t port, bool enabled) override;

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
            EXTERNALSOURCE,
            INTERNALSOURCE  // Supported in CheapoDC firmware 2.3.0 and later
        };

        enum CheapoDCLocation
        {
            LOCATION_LATITUDE = 0,
            LOCATION_LONGITUDE
        };

        enum deviceTime
        {
            LOCAL_TIME = 0,
            UTC_OFFSET
        };

        enum activeDevice
        {
            ACTIVE_TELESCOPE = 0,
            ACTIVE_FOCUSER,
            ACTIVE_WEATHER
        };

        enum controllerPinModes
        {
            CONTROLLER_PIN_MODE_DISABLED = 0,
            CONTROLLER_PIN_MODE_CONTROLLER,
            CONTROLLER_PIN_MODE_PWM,
            CONTROLLER_PIN_MODE_BOOLEAN,
            MAX_PIN_MODES
        };

        const char* pinModeText[MAX_PIN_MODES] = {"Disabled", "Controller", "PWM", "Boolean"};
        const char* channelLabels[MAX_PIN_MODES] = {"Output %d (Disabled)", "Output %d (Controller)", 
                                    "Output %d (PWM)", "Output %d (On/Off)"};
        int powerChannelToOutput[CDC_TOTAL_ADDITIONAL_OUTPUTS+CDC_MIN_ADDITIONAL_OUTPUT] = { -1, -1, -1, -1, -1, -1  };
        int dewChannelToOutput[CDC_TOTAL_ADDITIONAL_OUTPUTS+CDC_MIN_ADDITIONAL_OUTPUT] = { -1, -1, -1, -1, -1, -1  };
        int outputToChannel[CDC_TOTAL_ADDITIONAL_OUTPUTS] = { -1, -1, -1, -1 };
        int lastControllerPinMode[CDC_TOTAL_ADDITIONAL_OUTPUTS] = { CONTROLLER_PIN_MODE_DISABLED, CONTROLLER_PIN_MODE_DISABLED,
                                            CONTROLLER_PIN_MODE_DISABLED, CONTROLLER_PIN_MODE_DISABLED };

        bool fwVOneDetected = false;
        bool internalHumiditySensorSupported = false;
        bool additionalOutputsSupported = false;
        int fwMajorVersion = 0;
        int fwMinorVersion = 0;
        int fwPatchVersion = 0;
        int timerIndex = -1;
        unsigned int previousControllerMode = MANUAL;
        unsigned int prevMinOutput = 0;
        unsigned int prevMaxOutput = 100;
        unsigned int previousTemperatureMode = WEATHER_QUERY;
        char activeTelescopeDevice[MAXINDINAME] = {"Telescope Simulator"};
        char activeFocuserDevice[MAXINDINAME] = {"Focuser Simulator"};
        char activeWeatherDevice[MAXINDINAME] = {"Weather Simulator"};
        bool usingExternalWeatherSource = false;
        bool previuoslyUsingExternalWeatherSource = usingExternalWeatherSource;

        int msleep(long duration);
        bool sendCommand(const char *cmd, char *response);
        bool sendGetCommand(const char *cmd, char *resp);
        bool sendSetCommand(const char *cmd, char *value);
        bool Handshake();
        bool Ack();
        bool readSettings();
        void refreshSettings(bool delayRefresh = false);
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
        bool setUTCOffset(int offset);
        bool setLatitude(float value);
        bool setLongitude(float value);
        bool setLocation(float latitude, float longitude);
        bool setExternalTemperature(float value);
        bool setWeatherTemperature(float value);
        bool setWeatherHumidity(float value);
        bool setSnoopLocationDevice(const char *device, const char *property, const char *latAttribute, const char *lonAttribute);
        bool setSnoopTemperatureDevice(const char *device, const char *property, const char *attribute);
        bool setSnoopWeatherDevice(const char *device, const char *property, const char *temperatureAttribute,
                                   const char *humidityAttribute);
        void setActiveDevice(const char *telescopeDevice, const char *focuserDevice, const char *weatherDevice);
        bool setAdditionalOutput( int outputPin, int output );
        bool checkOutputConfiguration();

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
        INDI::PropertySwitch WeatherSourceSP{4};
        INDI::PropertyText WeatherQueryAPIKeyTP{1};
        INDI::PropertyText WeatherUpdatedTP{1};
        INDI::PropertyNumber LocationNP{2};
        INDI::PropertyText FWversionTP{1};
        INDI::PropertySwitch RefreshSP{1};
        INDI::PropertyText DeviceTimeTP{2};
        INDI::PropertyText ActiveDeviceTP{3};
};
