/*
    myDewControllerPro Driver
    Copyright (C) 2017-2023 Chemistorge

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
#include <indiweatherinterface.h>

/***************************** myDewControllerPro Commands **************************/

#define MDCP_CMD_LEN 8

//LCD Display Commands
#define MDCP_GET_LCD_STATE "5#"
#define MDCP_GET_LCD_STATE_RESPONSE "5%d$"
#define MDCP_LCD_DISABLE "{#"
#define MDCP_LCD_ENABLE "}#"
#define MDCP_GET_LCD_DISPLAY_TIME "H#"
#define MDCP_GET_LCD_DISPLAY_TIME_RESPONSE "H%d$"
#define MDCP_SET_LCD_DISPLAY_TIME "b%d#"
#define MDCP_GET_TEMP_DISPLAY "h#"
#define MDCP_GET_TEMP_DISPLAY_RESPONSE "h%d$"
#define MDCP_LCD_DISPLAY_CELSIUS "c#"
#define MDCP_LCD_DISPLAY_FAHRENHEIT "f#"


//Misc Commands
#define MDCP_GET_VERSION "v#"
#define MDCP_IDENTIFY_RESPONSE "v%u$" // Firmware version? Mine is "v342"
#define MDCP_SAVE_TO_EEPROM "w#"
#define MDCP_RESET_EEPROM_TO_DEFAULT "r#"
#define MDCP_GET_NUMBER_OF_PROBES "g#"

//Tracking Mode Commands

#define MDCP_SET_TRACKING_MODE "a%1u#"
#define MDCP_GET_TRACKING_MODE "T#"
#define MDCP_GET_TRACKING_MODE_RESPONSE "T%d$"
#define MDCP_GET_TRACKING_MODE_OFFSET "y#"
#define MDCP_GET_TRACKING_MODE_OFFSET_RESPONSE "y%d$"
#define MDCP_SET_TRACKING_MODE_OFFSET "3%d#"

//DHT Probe Commands

#define MDCP_GET_AMB_TEMP "A#"
#define MDCP_GET_AMB_TEMP_REPSONSE "A%f$"
#define MDCP_GET_AMB_TEMP_OFFSET "B#"
#define MDCP_GET_AMB_TEMP_OFFSET_RESPONSE "B%d$"
#define MDCP_SET_AMB_TEMP_OFFSET "e%1d#"
#define MDCP_GET_REL_HUMIDITY "R#"
#define MDCP_GET_REL_HUMIDITY_REPSONSE "R%f$"
#define MDCP_GET_DEW_POINT "D#"
#define MDCP_GET_DEW_POINT_RESPONSE "D%f$"


//Temperature Probe Commands

#define MDCP_GET_PROBE_TEMPS "C#"
#define MDCP_SET_TEMP_CH1_OFFSET "[%.1f#"
#define MDCP_SET_TEMP_CH2_OFFSET "]%.1f#"
#define MDCP_SET_TEMP_CH3_OFFSET "%%%.1f#"
#define MDCP_CLEAR_TEMP_OFFSETS "&#"
#define MDCP_GET_TEMP_OFFSETS "?#"
#define MDCP_GET_TEMP_OFFSETS_RESPONSE "?%f#%f#%f$"
#define MDCP_GET_TEMP_RESPONSE "C%f#%f#%f$"

//Board Cooling Fan Commands

#define MDCP_GET_FAN_SPEED "F#"
#define MDCP_SET_FAN_SPEED "s%d#"
#define MDCP_GET_BOARD_TEMP "K#"
#define MDCP_GET_BOARD_TEMP_RESPONSE "K%f$"
#define MDCP_GET_FAN_ON_TEMP "J#"
#define MDCP_GET_FAN_ON_TEMP_RESPONSE "J%d$"
#define MDCP_GET_FAN_OFF_TEMP "L#"
#define MDCP_GET_FAN_OFF_TEMP_RESPONSE "L%d$"
#define MDCP_SET_FAN_ON_TEMP "N%d#"
#define MDCP_SET_FAN_OFF_TEMP "M%d#"
#define MDCP_GET_FAN_MODE "O#"
#define MDCP_GET_FAN_MODE_RESPONSE "O%d$"
#define MDCP_SET_FAN_MODE "I%d#"

//Dew Strap Commands

#define MDCP_BOOST_CH1 "1#"
#define MDCP_BOOST_CH2 "2#"
#define MDCP_CANCEL_BOOST "n#"
#define MDCP_GET_CHANNEL_POWER "W#"
#define MDCP_GET_CHANNEL_POWER_RESPONSE "W%d#%d#%d$"
#define MDCP_GET_CH3_SETTINGS "E#"
#define MDCP_GET_CH3_SETTINGS_RESPONSE "E%d$"
#define MDCP_SET_CH3_SETTINGS "S%d#"
#define MDCP_SET_CH3_MANUAL_POWER "G%d#"

#define MDCP_RES_LEN 80


/******************************************************************************/

namespace Connection
{
class Serial;
};

class myDewControllerPro : public INDI::DefaultDevice, public INDI::PowerInterface, public INDI::WeatherInterface
{
    public:
        myDewControllerPro();
        virtual ~myDewControllerPro() = default;

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual void TimerHit() override;

        // INDI::PowerInterface virtual methods
        virtual bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
        virtual bool SetAutoDewEnabled(size_t port, bool enabled) override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        bool sendCommand(const char *cmd, char *response);
        bool Handshake();
        bool Ack();
        bool readMainValues();
        bool readLCDDisplayValues();
        bool readBoardFanValues();
        bool readOffsetValues();
        bool setOutputBoost(unsigned int channel);
        bool cancelOutputBoost();
        bool setTempCalibrations(float ch1, float ch2, float ch3, int ambient);
        bool setFanTempTrigger(int tempOn, int tempOff);
        bool zeroTempCalibrations();
        bool setInt(int mode, const char* mask, const char* errMessage);
        bool setChoice(int testInt, const char* positiveChoice, const char* negativeChoice, const char* errMessage);

        Connection::Serial *serialConnection{ nullptr };
        int PortFD{ -1 };


        INDI::PropertyNumber OutputsNP{3};
        enum
        {
            DEW_STRAP_ONE_POWER,
            DEW_STRAP_TWO_POWER,
            DEW_STRAP_THREE_POWER
        };

        INDI::PropertySwitch CH1CH2BoostSP{2};
        enum
        {
            CH1_BOOST_100,
            CH2_BOOST_100,
        };

        INDI::PropertySwitch CH3_ModeSP{ 5 };
        enum
        {
            DISABLED_STRAP,
            DEWSTRAP_ONE,
            DEWSTRAP_TWO,
            MANUAL_STRAP,
            TEMP_PROBE_THREE
        };

        INDI::PropertySwitch FanModeSP{ 2 };
        enum
        {
            BOARD_TEMP,
            MANUAL_FAN
        };

        INDI::PropertyNumber TemperaturesNP{ 5 };
        enum
        {
            PROBE_1,
            PROBE_2,
            PROBE_3,
            AMBIENT_PROBE,
            BOARD_PROBE
        };

        INDI::PropertyNumber TemperatureOffsetsNP{ 4 };
        enum
        {
            TEMP_PROBE_ONE_OFFSET,
            TEMP_PROBE_TWO_OFFSET,
            TEMP_PROBE_THREE_OFFSET,
            AMBIENT_TEMP_PROBE_OFFSET
        };

        INDI::PropertySwitch TrackingModeSP{ 3 };
        enum
        {
            AMBIENT,
            DEWPOINT,
            MIDPOINT
        };

        INDI::PropertyNumber FanTempTriggerNP{ 2 };
        enum
        {
            FANTEMPON,
            FANTEMPOFF
        };

        INDI::PropertySwitch LCDDisplayTempUnitsSP{ 2 };
        enum
        {
            CELCIUS,
            FAHRENHEIT
        };

        INDI::PropertySwitch EnableLCDDisplaySP{ 2 };
        enum
        {
            DISABLE_LCD,
            ENABLE_LCD
        };

        INDI::PropertySwitch EEPROMSP{ 2 };
        enum
        {
            RESET_EEPROM,
            SAVE_TO_EEPROM
        };

        INDI::PropertyNumber CH3_Manual_PowerNP{ 1 };
        INDI::PropertyNumber FanSpeedNP{ 1 };
        INDI::PropertySwitch ZeroTempOffsetsSP{ 1 };
        INDI::PropertyNumber TrackingModeOffsetNP{ 1 };
        INDI::PropertyNumber HumidityNP{ 1 };
        INDI::PropertyNumber DewpointNP{ 1 };
        INDI::PropertyNumber FWVersionNP{ 1 };
        INDI::PropertyNumber LCDPageRefreshNP{ 1 };

        static constexpr const char *ENVIRONMENT_TAB {"Environment"};

};
