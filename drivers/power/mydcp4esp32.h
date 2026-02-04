/*
    myDCP4ESP32
    Copyright (C) 2023 Stephen Hillier

    Based on MyFocuserPro2 Focuser
    Copyright (C) 2019 Alan Townshend

    As well as USB_Dewpoint
    Copyright (C) 2017-2023 Jarno Paananen

    And INDI Sky Quality Meter Driver
    Copyright(c) 2016 Jasem Mutlaq. All rights reserved.

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
#include <indipowerinterface.h>

#include <time.h>           // for nsleep() 
#include <errno.h>          // for nsleep() 

// Version 1.0 - First release.
// Version 2.0 - Refactor for INDI::Power Interface

#define CDRIVER_VERSION_MAJOR           2
#define CDRIVER_VERSION_MINOR           0

/***************************** myDCP4ESP32 Commands **************************/
//
// Commands and responses are string types
// Commands start with a ':' and are terminated with a '#', for example, ':01#'
// Commands are case sensitive
// Responses have a lead character, then values – more than one value is separated with a comma [,] and the string is
// terminated with a '#'

#define MDCP_CMD_LENGTH                     32        // Command buffer size
#define MDCP_CMD_TERMINATOR                 "#"
#define MDCP_GET_CONTROLLER_CODE_CMD        ":00#"    // Get controller code - Response: 0code# 
#define MDCP_GET_VERSION_CMD                ":01#"    // Get version number - Respnse: 1version#
#define MDCP_REBOOT_CMD                     ":02#"    // Reboot controller - Response: none
#define MDCP_GET_TRACKING_MODE_CMD          ":03#"    // Get tracking mode (1=Ambient, 2=Dewpoint, 3=Midpoint) - Response: 3trackingmode#
#define MDCP_SET_TRACKING_MODE_CMD          ":04%1u#" // Set tracking mode (1=Ambient, 2=Dewpoint, 3=Midpoint) - Response: none
#define MDCP_GET_TRACKING_OFFSET_CMD        ":05#"    // Get tracking mode offset value (-4 to +3) - Response: 5trackingmodeoffset# 
#define MDCP_ZERO_TRACKING_OFFSET_CMD       ":06#"    // Set tracking mode offset to 0 - Response: none
#define MDCP_DECREASE_TRACKING_OFFSET_CMD   ":07#"    // Decrease tracking mode offset - Response: none
#define MDCP_INCREASE_TRACKING_OFFSET_CMD   ":08#"    // Increase tracking mode offset - Response: none
#define MDCP_GET_NUMBER_PROBES_CMD          ":09#"    // Get the number of temperature probes attached - Response: 9number_of_probes#
#define MDCP_SAVE_CONTROLLER_SETTINGS_CMD   ":10#"    // Write variables immediately to SPIFFS - Response: none
#define MDCP_SET_DEFAULT_SETTINGS_CMD       ":11#"    // controller settings to default values - Response: none
#define MDCP_GET_AMBIENT_TEMPERATURE_CMD    ":12#"    // Get the Ambient temperature - Response: avalue# 
#define MDCP_GET_AMBIENT_OFFSET_CMD         ":13#"    // Get the Ambient offset - Response: bvalue$ 
#define MDCP_SET_AMBIENT_OFFSET_CMD         ":14%1.2f#"  // Set the Ambient offset, Num is a value between +3 and -4 - Response: none
#define MDCP_GET_HUMIDITY_CMD               ":15#"    // Get the Relative Humidity % - Resonse: cvalue#
#define MDCP_GET_DEWPOINT_CMD               ":16#"    // Get the Dew Point - Response: dvalue# 
#define MDCP_GET_TEMP_MODE_CMD              ":17#"    // Get Temp Mode, 0=Celsius, 1=Fahrenheit - Response: eTempMode
#define MDCP_SET_TEMP_MODE_C_CMD            ":181#"   // Display in Celsius - Response: none
#define MDCP_SET_TEMP_MODE_F_CMD            ":182#"   // Display in Fahrenheit - Response: none
#define MDCP_SET_TRACKING_OFFSET_CMD        ":19%d#"  // Set the tracking mode offset value [-4 to +3] - Response: none
#define MDCP_GET_MAC_ADDRESS_CMD            ":26#"    // Get MAC address - Response:  hString#
#define MDCP_GET_IP_ADDRESS_CMD             ":27#"    // Get IP address - Response: iString#
#define MDCP_GET_CHANNEL_TEMPS_CMD          ":28#"    // Get the temp in Celsius for Channel1-4 - Response: jch1temp,ch2temp,ch3temp,ch4temp# 
#define MDCP_SET_CH1_OFFSET_CMD             ":29%01.2f#"  // Set ch1offset value (expects a float) - Response: none
#define MDCP_SET_CH2_OFFSET_CMD             ":30%01.2f#"  // Set ch2offset value (expects a float) - Response: none
#define MDCP_SET_CH3_OFFSET_CMD             ":31%01.2f#"  // Set ch3offset value (expects a float) - Response: none
#define MDCP_SET_CH4_OFFSET_CMD             ":32%01.2f#"  // Set ch4offset value (expects a float) - Response: none
#define MDCP_ZERO_ALL_CH_OFFSET_CMD         ":33#"    // Clear ch1offset-ch4offset values to 0.0 - Response: none
#define MDCP_GET_ALL_CH_OFFSET_CMD          ":34#"    // Get the ch1offset=ch4offset values - Response: kch1offset,ch2offset,ch3offset# 
#define MDCP_SET_CH_100_CMD                 ":35%d#"  // Set boost 100% to channel x - Response: none
#define MDCP_GET_CH_OVERIDE_CMD             ":36%d#"  // Get Override mode for channel x - Response: return value is 0 or 1
#define MDCP_SET_CONTROLLER_MODE_CMD        ":37%d#"  // Set controller mode – controlledbyapp = 0 or 1 - Response: none
#define MDCP_GET_CONTROLLER_MODE_CMD        ":38#"    // Get controlledbyapp setting - Response: ux# 
#define MDCP_RESET_CH_100_CMD               ":39%d#"  // Turn off 100% boost to channel number x [1-4, 5=all] - Response: none
#define MDCP_GET_ALL_CH_POWER_CMD           ":40#"    // Get the power settings for ch1/ch2/ch3/ch4 - Response: lch1pwr,ch2pwr,ch3pwr,ch4pwr#
#define MDCP_SET_CH3_MODE_CMD               ":41%d#"  // Set the Channel 3 mode (0=disabled, 1=dewstrap1, 2=dewstrap2, 3=Manual, 4=use temp probe3) - Response: none
#define MDCP_GET_CH3_MODE_CMD               ":42#"    // Get the Channel 3 mode (0=disabled, 1=dewstrap1, 2=dewstrap2, 3=Manual, 4=use temp probe3) - Response: mmode#
#define MDCP_SET_CH3_MANUAL_POWER_CMD       ":43%d#"  // Set the Channel 3 power to a manual setting of Num - Response: none    

/**************************** myDCP4ESP32 Command Responses **************************/

#define MDCP_RESPONSE_LENGTH                64      // Response buffer size
#define MDCP_GET_CONTROLLER_CODE_RES        "0%s"   // Get controller code - Response: 0code# 
#define MDCP_GET_VERSION_RES                "1%d"   // Get version number - Respnse: 1version#
#define MDCP_REBOOT_RES                     ""      // Reboot controller - Response: none
#define MDCP_GET_TRACKING_MODE_RES          "3%d"   // Get tracking mode (1=Ambient, 2=Dewpoint, 3=Midpoint) - Response: 3trackingmode#
#define MDCP_SET_TRACKING_MODE_RES          ""      // Set tracking mode (1=Ambient, 2=Dewpoint, 3=Midpoint) - Response: none
#define MDCP_GET_TRACKING_OFFSET_RES        "5%d"   // Get tracking mode offset value (-4 to +3) - Response: 5trackingmodeoffset# 
#define MDCP_ZERO_TRACKING_OFFSET_RES       ""      // Set tracking mode offset to 0 - Response: none
#define MDCP_DECREASE_TRACKING_OFFSET_RES   ""      // Decrease tracking mode offset - Response: none
#define MDCP_INCREASE_TRACKING_OFFSET_RES   ""      // Increase tracking mode offset - Response: none
#define MDCP_GET_NUMBER_PROBES_RES          "9%d"   // Get the number of temperature probes attached - Response: 9number_of_probes#
#define MDCP_SAVE_CONTROLLER_SETTINGS_RES   ""      // Write variables immediately to SPIFFS - Response: none
#define MDCP_SET_DEFAULT_SETTINGS_RES       ""      // controller settings to default values - Response: none
#define MDCP_GET_AMBIENT_TEMPERATURE_RES    "a%f"   // Get the Ambient temperature - Response: avalue# 
#define MDCP_GET_AMBIENT_OFFSET_RES         "b%f"   // Get the Ambient offset - Response: bvalue# 
#define MDCP_SET_AMBIENT_OFFSET_RES         ""      // Set the Ambient offset, Num is a value between +3 and -4 - Response: none
#define MDCP_GET_HUMIDITY_RES               "c%f"   // Get the Relative Humidity % - Resonse: cvalue#
#define MDCP_GET_DEWPOINT_RES               "d%f"   // Get the Dew Point - Response: dvalue# 
#define MDCP_GET_TEMP_MODE_RES              "e%d"   // Get Temp Mode, 0=Celsius, 1=Fahrenheit - Response: eTempMode
#define MDCP_SET_TEMP_MODE_C_RES            ""      // Display in Celsius - Response: none
#define MDCP_SET_TEMP_MODE_F_RES            ""      // Display in Fahrenheit - Response: none
#define MDCP_SET_TRACKING_OFFSET_RES        ""      // Set the tracking mode offset value [-4 to +3] - Response: none
#define MDCP_GET_MAC_ADDRESS_RES            "h%s"   // Get MAC address - Response:  hString#
#define MDCP_GET_IP_ADDRESS_RES             "i%s"   // Get IP address - Response: iString#
#define MDCP_GET_CHANNEL_TEMPS_RES          "j%f,%f,%f,%f"    // Get the temp in Celsius for Channel1-4 - Response: jch1temp,ch2temp,ch3temp,ch4temp# 
#define MDCP_SET_CH1_OFFSET_RES             ""      // Set ch1offset value (expects a float) - Response: none
#define MDCP_SET_CH2_OFFSET_RES             ""      // Set ch2offset value (expects a float) - Response: none
#define MDCP_SET_CH3_OFFSET_RES             ""      // Set ch3offset value (expects a float) - Response: none
#define MDCP_SET_CH4_OFFSET_RES             ""      // Set ch4offset value (expects a float) - Response: none
#define MDCP_ZERO_ALL_CH_OFFSET_RES         ""      // Clear ch1offset-ch4offset values to 0.0 - Response: none
#define MDCP_GET_ALL_CH_OFFSET_RES          "k%f,%f,%f,%f"    // Get the ch1offset=ch4offset values - Response: kch1offset,ch2offset,ch3offset# 
#define MDCP_SET_CH_100_RES                 ""      // Set boost 100% to channel x - Response: none
#define MDCP_GET_CH_OVERIDE_RES             "t%d"   // Get Override mode for channel x - Response: return value is 0 or 1
#define MDCP_SET_CONTROLLER_MODE_RES        ""      // Set controller mode – controlledbyapp = 0 or 1 - Response: none
#define MDCP_GET_CONTROLLER_MODE_RES        "u%d"   // Get controlledbyapp setting - Response: ux# 
#define MDCP_RESET_CH_100_RES               ""      // Turn off 100% boost to channel number x [1-4, 5=all] - Response: none
#define MDCP_GET_ALL_CH_POWER_RES           "l%d,%d,%d,%d"    // Get the power settings for ch1/ch2/ch3/ch4 - Response: lch1pwr,ch2pwr,ch3pwr,ch4pwr#
#define MDCP_SET_CH3_MODE_RES               ""      // Set the Channel 3 mode (0=disabled, 1=dewstrap1, 2=dewstrap2, 3=Manual, 4=use temp probe3) - Response: none
#define MDCP_GET_CH3_MODE_RES               "m%d"   // Get the Channel 3 mode (0=disabled, 1=dewstrap1, 2=dewstrap2, 3=Manual, 4=use temp probe3) - Response: mmode#
#define MDCP_SET_CH3_MANUAL_POWER_RES       ""      // Set the Channel 3 power to a manual setting of Num - Response: none 

/******************************************************************************/

class MyDCP4ESP : public INDI::DefaultDevice, public INDI::PowerInterface
{
    public:
        MyDCP4ESP();
        virtual ~MyDCP4ESP() = default;

        // Power Interface Implementations
        virtual bool SetPowerPort(size_t port, bool enabled) override;
        virtual bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
        virtual bool SetVariablePort(size_t port, bool enabled, double voltage) override;
        virtual bool SetLEDEnabled(bool enabled) override;
        virtual bool SetAutoDewEnabled(size_t port, bool enabled) override;
        virtual bool CyclePower() override;
        virtual bool SetUSBPort(size_t port, bool enabled) override;

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual void TimerHit() override;

        /**
         * @struct MdcpConnection
         * @brief Holds the connection mode of the device.
         */
        typedef enum
        {
            CONNECTION_NONE   = 1 << 0,
            CONNECTION_SERIAL = 1 << 1,
            CONNECTION_TCP    = 1 << 2
        } MdcpConnection;

    private:
        int  timerIndex;
        int  myDCP4Firmware = 0;
        float channelActive[4] = {1};
        int  msleep( long duration);
        bool sendCommand(const char *cmd, char *response);
        bool Handshake();
        bool Ack();
        bool rebootController();
        bool readSettings();
        bool setChannelOffset(unsigned int channel, float value);
        bool setAmbientOffset(float value);
        bool setTrackingMode(unsigned int value);
        bool setTrackingOffset(int value);
        bool setCh3Mode(unsigned int value);
        bool setCh3Output(unsigned int value);
        bool setChannelBoost(unsigned int channel, unsigned int value);
        bool getActiveChannels();

        Connection::Serial *serialConnection { nullptr };
        Connection::TCP *tcpConnection { nullptr };

        int PortFD { -1 };

        uint8_t mdcpConnection { CONNECTION_SERIAL | CONNECTION_TCP };

        // MyDCP4ESP Timeouts
        static const uint8_t MDCP_READ_TIMEOUT { 10 };   // seconds
        static const long    MDCP_SMALL_DELAY  { 50 };   // 50ms delay from send command to read response

        enum
        {
            CH3MODE_DISABLED,
            CH3MODE_CH1POWER,
            CH3MODE_CH2POWER,
            CH3MODE_MANUAL,
            CH3MODE_CH3TEMP
        };

        INDI::PropertySwitch TempProbeFoundSP{4};
        INDI::PropertyNumber TemperatureNP{4};
        INDI::PropertyNumber ChannelOffsetNP{4};
        INDI::PropertySwitch ChannelBoostSP{5};
        INDI::PropertyNumber AmbientTemperatureNP{1};
        INDI::PropertyNumber AmbientOffsetNP{1};
        INDI::PropertyNumber HumidityNP{1};
        INDI::PropertyNumber DewpointNP{1};
        INDI::PropertySwitch TrackingModeSP{3};
        INDI::PropertyNumber TrackingOffsetNP{1};
        INDI::PropertySwitch Ch3ModeSP{5};
        INDI::PropertyText   CheckCodeTP{1};
        INDI::PropertyNumber FWversionNP{1};

};
