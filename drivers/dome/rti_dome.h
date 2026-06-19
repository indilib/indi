/*******************************************************************************
 Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

 RTI Dome INDI Driver

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indidome.h"

class RTIDome : public INDI::Dome
{
    public:
        RTIDome();
        virtual ~RTIDome() override = default;

        virtual const char *getDefaultName() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool Handshake() override;
        virtual void TimerHit() override;

        virtual IPState MoveRel(double azDiff) override;
        virtual IPState MoveAbs(double az) override;
        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
        virtual IPState ControlShutter(ShutterOperation operation) override;
        virtual bool Abort() override;
        virtual bool Sync(double az) override;

        // Parking
        virtual IPState Park() override;
        virtual IPState UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

    private:

        double m_TargetAz {0};
        // True when the shutter controller has responded to a ping (H#/o# check)
        bool   m_bShutterPresent {false};
        // Counter used to schedule periodic shutter-presence re-checks
        int    m_nShutterPollCounter {0};

        ///////////////////////////////////////////////////////////////////////////////
        /// Setup Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool setupInitialParameters();

        ///////////////////////////////////////////////////////////////////////////////
        /// Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool getFirmwareVersion();
        bool getShutterFirmwareVersion();
        bool getDomeAzimuth(double &az);
        bool getSlewingStatus(int &status);
        bool getHomedStatus(int &status);
        bool queryShutterState(int &status);
        bool getStepsPerRotation(uint32_t &steps);
        bool getStepRate(uint32_t &rate);
        bool getAcceleration(uint32_t &accel);
        bool getHomePosition(double &az);
        bool getParkPosition(double &az);
        bool getMotorReversed(bool &reversed);
        bool getRainAction(int &action);
        bool getRainStatus(bool &raining);
        bool getShutterVoltage(double &voltage, double &cutoff);
        /// Send H# to wake up the shutter XBee radio
        bool sendShutterHello();
        /// Query o# to check whether shutter controller has responded to pings
        bool getShutterPresent(bool &present);

        ///////////////////////////////////////////////////////////////////////////////
        /// Set / Action Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool gotoAzimuth(double az);
        bool syncAzimuth(double az);
        bool stopDome();
        bool findHome();
        bool setHomePosition(double az);
        bool setParkPosition(double az);
        bool setStepsPerRotation(uint32_t steps);
        bool setStepRate(uint32_t rate);
        bool setAcceleration(uint32_t accel);
        bool setMotorReversed(bool reversed);
        bool setRainAction(int action);
        bool openShutter();
        bool closeShutter();

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////

        /**
         * @brief sendCommand Send a string command to device.
         * @param cmd Command string WITHOUT the trailing '#'. The function appends '#' before sending.
         * @param res If not nullptr, the function will wait for a response from the device. If nullptr,
         *            it returns true immediately after the command is successfully sent.
         * @return True if successful, false otherwise.
         */
        bool sendCommand(const char *cmd, char *res = nullptr);
        void hexDump(char *buf, const char *data, int size);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////

        // Firmware versions (rotation + shutter)
        INDI::PropertyText FirmwareTP {2};
        enum
        {
            FIRMWARE_ROTATION,
            FIRMWARE_SHUTTER
        };

        // Dome and shutter text status
        INDI::PropertyText StatusTP {2};
        enum
        {
            STATUS_DOME,
            STATUS_SHUTTER
        };

        // Motor settings: steps/rotation, speed, acceleration
        INDI::PropertyNumber SettingsNP {3};
        enum
        {
            SETTINGS_STEPS_PER_ROTATION,
            SETTINGS_STEP_RATE,
            SETTINGS_ACCELERATION
        };

        // Home position in degrees
        INDI::PropertyNumber HomePositionNP {1};

        // Find Home button
        INDI::PropertySwitch HomeSP {1};
        enum
        {
            HOME_FIND
        };

        // Rain sensor action
        INDI::PropertySwitch RainActionSP {3};
        enum
        {
            RAIN_DO_NOTHING,
            RAIN_GOTO_HOME,
            RAIN_GOTO_PARK
        };

        // Motor direction
        INDI::PropertySwitch ReversedSP {2};
        enum
        {
            REVERSED_NORMAL,
            REVERSED_REVERSED
        };

        // Shutter voltage info
        INDI::PropertyNumber ShutterVoltsNP {2};
        enum
        {
            SHUTTER_VOLTAGE_CURRENT,
            SHUTTER_VOLTAGE_CUTOFF
        };

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char *SETTINGS_TAB = "Settings";
        static constexpr const char *INFO_TAB      = "Info";
        // '#' is the stop character for all RTI dome commands/responses
        static const char DRIVER_STOP_CHAR { '#' };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receiving.
        static constexpr const uint8_t DRIVER_LEN {64};
        // Dome AZ threshold in degrees
        static constexpr const double DOME_AZ_THRESHOLD {0.01};
};
