/*
    ZWO Mount INDI driver

    Copyright (C) 2015-2027 Jasem Mutlaq

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

#include "lx200generic.h"
#include "indipropertyswitch.h"
#include "indipropertynumber.h"
#include "indipropertytext.h"

#include <chrono>

class LX200AM5 : public LX200Generic
{
    public:
        LX200AM5();
        virtual ~LX200AM5() override = default;

        virtual bool updateProperties() override;
        virtual bool initProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual const char *getDefaultName() override;

        // Communication
        virtual bool checkConnection() override;

        // Motion & Goto
        virtual bool ReadScopeStatus() override;
        virtual bool SetSlewRate(int index) override;

        // Tracking
        virtual bool SetTrackEnabled(bool enabled) override;

        // Time & Location
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool setUTCOffset(double offset) override;
        virtual bool setLocalDate(uint8_t days, uint8_t months, uint16_t years) override;

        // Parking
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

        // Home
        virtual IPState ExecuteHomeAction(TelescopeHomeAction action) override;

        // Config
        virtual bool saveConfigItems(FILE *fp) override;

    private:

        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// Communication
        ///////////////////////////////////////////////////////////////////////////////////////////////
        /**
                 * @brief sendCommand Send a string command to device.
                 * @param cmd Command to be sent. Can be either NULL TERMINATED or just byte buffer.
                 * @param res If not nullptr, the function will wait for a response from the device. If nullptr, it returns true immediately
                 * after the command is successfully sent.
                 * @param cmd_len if -1, it is assumed that the @a cmd is a null-terminated string. Otherwise, it would write @a cmd_len bytes from
                 * the @a cmd buffer.
                 * @param res_len if -1 and if @a res is not nullptr, the function will read until it detects the default delimiter DRIVER_STOP_CHAR
                 *  up to DRIVER_LEN length. Otherwise, the function will read @a res_len from the device and store it in @a res.
                 * @return True if successful, false otherwise.
        */
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        /**
         * @brief sendStatusCommand Send a set command answered with a single status character, or with
         * 'e' followed by an error code terminated by '#'.
         * @param cmd Null-terminated command.
         * @param res Buffer of at least DRIVER_LEN bytes. Receives the status character, or "e<code>" on error.
         * @return True if a reply was received, false on communication failure.
         */
        bool sendStatusCommand(const char * cmd, char * res);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        //////////////////////////////////////////////////////////////////////////////////
        /// Properties
        //////////////////////////////////////////////////////////////////////////////////
        // Go Home
        //INDI::PropertySwitch HomeSP {1};
        // Mount Type
        INDI::PropertySwitch MountTypeSP {2};
        enum
        {
            Azimuth,
            Equatorial
        };
        // Guide Rate
        INDI::PropertyNumber GuideRateNP {1};
        // Buzzer control
        INDI::PropertySwitch BuzzerSP {3};
        enum
        {
            Off,
            Low,
            High
        };

        // Heavy duty control
        INDI::PropertySwitch HeavyDutyModeSP {2};

        // Meridian Flip Control
        INDI::PropertySwitch MeridianFlipSP {2};

        // Post Meridian Track Control
        INDI::PropertySwitch PostMeridianTrackSP {2};
        enum
        {
            TRACK,
            STOP
        };

        // Meridian Limit
        INDI::PropertyNumber MeridianLimitNP {1};

        // Altitude Limits
        INDI::PropertySwitch AltitudeLimitSP {3};
        enum
        {
            ALT_LIMIT_ENABLE,
            ALT_LIMIT_DISABLE,
            ALT_LIMIT_GET
        };
        INDI::PropertyNumber AltitudeLimitUpperNP {1};
        INDI::PropertyNumber AltitudeLimitLowerNP {1};

        // Multi-Star Alignment
        INDI::PropertySwitch MultiStarAlignSP {1};
        enum
        {
            CLEAR_ALIGNMENT_DATA
        };

        // Variable Slew Speed
        INDI::PropertyNumber VariableSlewRateNP {1};

        // Mount model and firmware version
        INDI::PropertyText FirmwareTP {2};
        enum
        {
            FIRMWARE_MODEL,
            FIRMWARE_VERSION
        };

        // Whether a custom park position was stored in the mount (:Sp01#). The mount cannot be queried
        // for it, so it is kept in the config only and never defined. Without one, park goes home.
        INDI::PropertySwitch CustomParkSP {1};


        //////////////////////////////////////////////////////////////////////////////////
        /// AM5 Specific
        //////////////////////////////////////////////////////////////////////////////////
        void setup();

        // Homing
        bool goHome();
        bool park();
        bool setHome();

        // Firmware
        void getFirmwareInfo();
        bool hasMountPark() const
        {
            return m_FirmwareVersion >= FIRMWARE_MOUNT_PARK;
        }
        bool hasCustomParkPosition() const
        {
            return m_FirmwareVersion >= FIRMWARE_CUSTOM_PARK_POSITION;
        }

        // Mount park position (firmware dependent)
        bool parkToMountPosition();
        bool getMountParkState(char &state);
        void checkMountParkState();
        void updateMountPark(bool slewComplete, bool isHome);
        void failMountPark(const char *reason);

        // Altitude Limits
        bool setAltitudeLimitEnabled(bool enable);
        bool getAltitudeLimitStatus();
        bool setAltitudeLimitUpper(double limit);
        bool getAltitudeLimitUpper();
        bool setAltitudeLimitLower(double limit);
        bool getAltitudeLimitLower();

        // Multi-Star Alignment
        bool clearMultiStarAlignmentData();

        // Variable Slew Speed
        bool setVariableSlewRate(double rate);

        // Guide Rate
        bool setGuideRate(double value);
        bool getGuideRate();

        // Buzzer
        bool getBuzzer();
        bool setBuzzer(int value);

        // Heavy Duty Mode
        bool getHeavyDutyMode();
        bool setHeavyDutyMode(bool enable);

        // Mount type
        bool setMountType(int type);
        bool getMountType();

        // Track Mode
        bool getTrackMode();
        bool isTracking();

        // Meridian Flip
        bool setMeridianFlipSettings(bool enabled, bool track, double limit);
        bool getMeridianFlipSettings();


        //////////////////////////////////////////////////////////////////////////////////
        /// Static Constants
        //////////////////////////////////////////////////////////////////////////////////
        // # is the stop char
        static const char DRIVER_STOP_CHAR { 0x23 };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receiving.
        static constexpr const uint8_t DRIVER_LEN {64};
        // Slew Modes
        static constexpr const uint8_t SLEW_MODES {10};
        static constexpr const char * MERIDIAN_FLIP_TAB {"Meridian Flip"};
        static constexpr const char * ALTITUDE_LIMIT_TAB {"Altitude Limits"};
        static constexpr const char * ALIGNMENT_TAB {"Alignment"};

        // Firmware versions encoded as (major << 16) | (minor << 8) | patch
        // :hP# parks, :Gps# reports the park state and :Spu# unparks.
        static constexpr uint32_t FIRMWARE_MOUNT_PARK {0x010109};
        // :Sp01# stores the current position as the park position.
        static constexpr uint32_t FIRMWARE_CUSTOM_PARK_POSITION {0x010300};
        // :GU# reports H once a home slew is over.
        static constexpr uint32_t FIRMWARE_HOME_FLAG {0x010801};
        // Give up a park stage that does not start moving within this time.
        static constexpr std::chrono::seconds PARK_START_TIMEOUT {5};

        uint32_t m_FirmwareVersion {0};

        // A park to the mount park position goes home first (:hC#), then parks (:hP#).
        enum class ParkStage
        {
            None,
            Homing,
            Parking
        };
        ParkStage m_ParkStage {ParkStage::None};
        bool m_ParkMoved {false};
        std::chrono::steady_clock::time_point m_ParkDeadline;
};
