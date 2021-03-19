/*******************************************************************************
 Copyright(c) 2020 Jasem Mutlaq. All rights reserved.

 Astrometric Solutions DomePro2 INDI Driver

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

#include "indibase/indidome.h"
#include <map>

class DomePro2 : public INDI::Dome
{
    public:
        DomePro2();
        virtual ~DomePro2() override = default;

        virtual const char *getDefaultName() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;


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

        ShutterOperation m_TargetShutter { SHUTTER_OPEN };
        double targetAz {0};

        ///////////////////////////////////////////////////////////////////////////////
        /// Setup Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool setupInitialParameters();
        uint8_t processShutterStatus();


        ///////////////////////////////////////////////////////////////////////////////
        /// Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool getFirmwareVersion();
        bool getHardwareConfig();
        bool getDomeStatus();
        bool getShutterStatus();
        bool getDomeAzCPR();
        bool getDomeAzCoast();
        bool getDomeAzPos();
        bool getDomeHomeAz();
        bool getDomeParkAz();
        bool getDomeAzStallCount();


        ///////////////////////////////////////////////////////////////////////////////
        /// Set Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool setDomeAzCPR(uint32_t cpr);
        bool gotoDomeAz(uint32_t az);
        bool setDomeAzCoast(uint32_t coast);
        bool killDomeAzMovement();
        bool killDomeShutterMovement();
        bool setDomeHomeAz(uint32_t az);
        bool setDomeParkAz(uint32_t az);
        bool setDomeAzStallCount(uint32_t steps);
        bool calibrateDomeAz(double az);
        bool gotoDomePark();
        bool openDomeShutters();
        bool closeDomeShutters();
        bool gotoHomeDomeAz();
        bool discoverHomeDomeAz();
        bool setDomeLeftOn();
        bool setDomeRightOn();


        ///////////////////////////////////////////////////////////////////////////////
        /// Parking, Homing, and Calibration
        ///////////////////////////////////////////////////////////////////////////////


        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////

        /**
                 * @brief sendCommand Send a string command to device.
                 * @param cmd Command to be sent. Can be either NULL TERMINATED or just byte buffer.
                 * @param res If not nullptr, the function will wait for a response from the device. If nullptr, it returns true immediately
                 * after the command is successfully sent.
                 * @param cmd_len if -1, it is assumed that the @a cmd is a null-terminated string. Otherwise, it would write @a cmd_len bytes from
                 * the @a cmd buffer.
                 * @param res_len if -1 and if @a res is not nullptr, the function will read until it detects the default delimeter DRIVER_STOP_CHAR
                 *  up to DRIVER_LEN length. Otherwise, the function will read @a res_len from the device and store it in @a res.
                 * @return True if successful, false otherwise.
        */
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////
        ITextVectorProperty VersionTP;
        IText VersionT[2] {};
        enum
        {
            VERSION_FIRMWARE,
            VERSION_HARDWARE
        };

        ISwitchVectorProperty HomeSP;
        ISwitch HomeS[2] {};
        enum
        {
            HOME_DISCOVER,
            HOME_GOTO
        };

        ITextVectorProperty StatusTP;
        IText StatusT[2] {};
        enum
        {
            STATUS_DOME,
            STATUS_SHUTTER
        };

        INumberVectorProperty SettingsNP;
        INumber SettingsN[5];
        enum
        {
            SETTINGS_AZ_CPR,
            SETTINGS_AZ_COAST,
            SETTINGS_AZ_HOME,
            SETTINGS_AZ_PARK,
            SETTINGS_AZ_STALL_COUNT
        };

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * INFO_TAB = "Info";
        static constexpr const char * SETTINGS_TAB = "Settings";
        // 0x3B is the stop char
        static const char DRIVER_STOP_CHAR { 0x3B };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {64};
        // Dome AZ Threshold
        static constexpr const double DOME_AZ_THRESHOLD {0.01};
        // Dome hardware types
        const std::map<uint8_t, std::string> DomeHardware =
        {
            {0x0D, "DomePro2-d for classic domes"},
            {0x0E, "DomePro2-c for clamcshell domes"},
            {0x0F, "DomePro2-r for roll-off roof"}
        };
        // Shutter statuses
        const std::map<uint8_t, std::string> ShutterStatus =
        {
            {0x00, "Opened"},
            {0x01, "Closed"},
            {0x02, "Opening"},
            {0x03, "Closing"},
            {0x04, "ShutterError"},
            {0x05, "shutter module is not communicating to the azimuth module"},
            {0x06, "shutter 1 opposite direction timeout error on open occurred"},
            {0x07, "shutter 1 opposite direction timeout error on close occurred"},
            {0x08, "shutter 2 opposite direction timeout error on open occurred"},
            {0x09, "shutter 2 opposite direction timeout error on close occurred"},
            {0x0A, "shutter 1 completion timeout error on open occurred"},
            {0x0B, "shutter 1 completion timeout error on close occurred"},
            {0x0C, "shutter 2 completion timeout error on open occurred"},
            {0x0D, "shutter 2 completion timeout error on close occurred"},
            {0x0E, "shutter 1 limit fault on open occurred"},
            {0x0F, "shutter 1 limit fault on close occurred"},
            {0x10, "shutter 2 limit fault on open occurred"},
            {0x11, "shutter 2 limit fault on close occurred"},
            {0x12, "Shutter disabled (Shutter Enable input is not asserted)"},
            {0x13, "Intermediate"},
            {0x14, "GoTo"},
            {0x15, "shutter 1 OCP trip on open occurred"},
            {0x16, "shutter 1 OCP trip on close occurred"},
            {0x17, "shutter 2 OCP trip on open occurred"},
            {0x18, "shutter 2 OCP trip on close occurred"}
        };
        // Dome statuses
        const std::map<std::string, std::string> DomeStatus =
        {
            {"Fixed", "Idle"},
            {"Left", "Moving Left"},
            {"Right", "Moving Right"},
            {"Goto", "GoTo"},
            {"Homing", "Homing"},
            {"Parking", "Parking"},
            {"Gauging", "Gauging"},
            {"Timeout", "Azimuth movement timeout"},
            {"Stall", "Azimuth encoder registering insufficent counts… motor stalled"},
            {"OCP", "Over Current Protection was tripped"}
        };



};
