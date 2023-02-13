/*
    USB_Dewpoint
    Copyright (C) 2017-2023 Jarno Paananen

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

/***************************** myDewControllerPro Commands **************************/

#define MDCP_CMD_LEN 8



//Misc Commands
#define MDCP_GET_VERSION "v#"
#define MDCP_SAVE_TO_EEPROM "w#"
#define MDCP_RESET_EEPROM_TO_DEFAULT "r#"
#define MDCP_GET_NUMBER_OF_PROBES "g#"

//Tracking Mode Commands

#define MDCP_SET_TRACKING_MODE "a%1u#"
#define MDCP_GET_TRACKING_MODE "T#"
#define MDCP_GET_TRACKING_MODE_RESPONSE "T%d$"
#define MDCP_GET_TRACKING_MODE_OFFSET "y#"
#define MDCP_GET_TRACKING_MODE_OFFSET_RESPONSE "y%d$"
#define MDCP_SET_TRACKING_MODE_ZERO_OFFSET "z#"
#define MDCP_TRACKING_MODE_DECREASE_OFFSET "<#"
#define MDCP_TRACKING_MODE_INCREASE_OFFSET ">#"

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
#define MDCP_GET_TEMP_DISPLAY_MODE "h#"
#define MDCP_SET_TEMP_DISPLAY_C "c#"
#define MDCP_SET_TEMP_DISPLAY_F "f#"
#define MDCP_GET_TEMP_RESPONSE "C%f#%f#%f$"

//Board Cooling Fan Commands

#define MDCP_GET_FAN_SPEED "F#"
#define MDCP_SET_FAN_SPEED "s%d#"
#define MDCP_GET_BOARD_TEMP "K#"
#define MDCP_GET_BOARD_TEMP_RESPONSE "K%f$"
#define MDCP_GET_FAN_ON_TEMP "J#"
#define MDCP_GET_FAN_OFF_TEMP "L#"
#define MDCP_SET_FAN_ON_TEMP "I%d#"
#define MDCP_SET_FAN_OFF_TEMP "M%d#"

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



/**************************** myDewControllerPro Constants **************************/


#define MDCP_RES_LEN 80 // With some margin, maximum feasible seems to be around 70





#define MDCP_IDENTIFY_RESPONSE "v%u" // Firmware version? Mine is "v334"

/******************************************************************************/

namespace Connection
{
class Serial;
};

class myDewControllerPro : public INDI::DefaultDevice
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

    private:
        bool sendCommand(const char *cmd, char *response);

        bool Handshake();
        bool Ack();
        bool Resync();

        bool reset();
        bool readSettings();

        bool setOutputBoost(unsigned int channel);
        bool cancelOutputBoost();
        bool channelThreeModeSet(unsigned int mode);
        bool setCHThreeManualPower(unsigned int power);
        bool trackingModeSet(unsigned int mode);
        bool changeTrackingOffsets(int button);
        bool setTempCalibrations(float ch1, float ch2, float ch3, int ambient);
        bool zeroTempCalibrations();




        Connection::Serial *serialConnection{ nullptr };
        int PortFD{ -1 };

        INumber OutputsN[3];
        INumberVectorProperty OutputsNP;

        enum {
            DEW_STRAP_ONE_POWER,
            DEW_STRAP_TWO_POWER,
            DEW_STRAP_THREE_POWER
        };

        ISwitch CH1CH2BoostS[2];
        ISwitchVectorProperty CH1CH2BoostSP;

        enum {
            CH1_BOOST_100,
            CH2_BOOST_100,
        };

        ISwitch CH3_ModeS[5];
        ISwitchVectorProperty CH3_ModeSP;

        enum {
            DISABLED,
            DEWSTRAP_ONE,
            DEWSTRAP_TWO,
            MANUAL,
            TEMP_PROBE_THREE
        };

        INumber CH3_Manual_PowerN[1];
        INumberVectorProperty CH3_Manual_PowerNP;

        INumber FanSpeedN[1];
        INumberVectorProperty FanSpeedNP;

        ISwitch FanModeS[1];
        ISwitchVectorProperty FanModeSP;

        INumber TemperaturesN[5];
        INumberVectorProperty TemperaturesNP;

        enum {
            PROBE_1,
            PROBE_2,
            PROBE_3,
            AMBIENT_PROBE,
            BOARD_PROBE
        };

        INumber TemperatureOffsetsN[4];
        INumberVectorProperty TemperatureOffsetsNP;

        enum {
            TEMP_PROBE_ONE_OFFSET,
            TEMP_PROBE_TWO_OFFSET,
            TEMP_PROBE_THREE_OFFSET,
            AMBIENT_TEMP_PROBE_OFFSET
        };

        ISwitch ZeroTempOffsetsS[1];
        ISwitchVectorProperty ZeroTempOffsetsSP;

        ISwitch TrackingModeS[3];
        ISwitchVectorProperty TrackingModeSP;

        enum {
            AMBIENT,
            DEWPOINT,
            MIDPOINT
        };

        INumber TrackingModeOffsetN[1];
        INumberVectorProperty TrackingModeOffsetNP;



        INumber HumidityN[1];
        INumberVectorProperty HumidityNP;

        INumber DewpointN[1];
        INumberVectorProperty DewpointNP;

        ISwitch EEPROMSaveS[2];
        ISwitchVectorProperty EEPROMSaveSP;

        enum {
            RESET_EEPROM,
            SAVE_TO_EEPROM
        }

        INumber FWVersionN[1];
        INumberVectorProperty FWVersionNP;

        INumber FanTempTriggerN[2];
        INumberVectorProperty FanTempTriggerNP;

        enum {
            FANTEMPON,
            FANTEMPOFF
        }

















};
