/*
    NexDome Beaver Controller

    Copyright (C) 2021 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <memory>
#include <indidome.h>
#include <indipropertytext.h>
#include <indipropertyswitch.h>
#include <indipropertynumber.h>

class Beaver : public INDI::Dome
{
    public:
        Beaver();
        virtual ~Beaver() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        bool Handshake() override;
        virtual void TimerHit() override;

        // Rotator
        virtual IPState MoveAbs(double az) override;
        virtual IPState MoveRel(double azDiff) override;
        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

        // Shutter
        virtual IPState ControlShutter(ShutterOperation operation) override;

        // Abort
        virtual bool Abort() override;

        // Config
        virtual bool saveConfigItems(FILE * fp) override;

        // Parking
        virtual IPState Park() override;
        virtual IPState UnPark() override;

        // Beaver status
        enum
        {
            DOME_STATUS_ROTATOR_MOVING = 0x0001,
            DOME_STATUS_SHUTTER_MOVING = 0x0002,
            DOME_STATUS_ROTATOR_ERROR = 0x0004,
            DOME_STATUS_SHUTTER_ERROR = 0x0008,
            DOME_STATUS_SHUTTER_COMM = 0x0010,
            DOME_STATUS_UNSAFE_CW = 0x0020,
            DOME_STATUS_UNSAFE_RG = 0x0040,
            DOME_STATUS_SHUTTER_OPENED = 0x0080,
            DOME_STATUS_SHUTTER_CLOSED = 0x0100,
            DOME_STATUS_SHUTTER_OPENING = 0x0200,
            DOME_STATUS_SHUTTER_CLOSING = 0x0400,
            DOME_STATUS_ROTATOR_HOME = 0x0800,
            DOME_STATUS_ROTATOR_PARKED = 0x1000
        };

    private:

        ///////////////////////////////////////////////////////////////////////////////
        /// Set & Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool echo();

        ///////////////////////////////////////////////////////////////////////////////
        /// Rotator Motion Control
        ///////////////////////////////////////////////////////////////////////////////
        bool rotatorGotoAz(double az);
        bool rotatorGetAz();
        bool rotatorSyncAZ(double az);
        bool rotatorSetHome(double az);
        bool rotatorSetPark(double az);
        bool rotatorGotoPark();
        bool rotatorGotoHome();
        bool rotatorMeasureHome();
        bool rotatorFindHome();
        bool rotatorIsHome();
        bool rotatorIsParked();
        bool rotatorUnPark();
        bool rotatorSetPark();
        bool abortAll();

        bool rotatorGetSettings();
        bool rotatorSetSettings(double maxSpeed, double minSpeed, double acceleration, double timeout);

        ///////////////////////////////////////////////////////////////////////////////
        /// Shutter Motion Control
        ///////////////////////////////////////////////////////////////////////////////
        //bool shutterSetSettings(double maxSpeed, double minSpeed, double acceleration, double timeout, double voltage);
        bool shutterSetSettings(double maxSpeed, double minSpeed, double acceleration, double voltage);

        bool shutterGetSettings();
        bool shutterFindHome();
        bool shutterAbort();
        bool shutterOnLine();

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, double &res);
        bool sendRawCommand(const char * cmd, char *resString);
        bool getDomeStatus(uint16_t &domeStatus);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////
        // Beaver Firmware Version
        INDI::PropertyText VersionTP {1};
        // Home offset from north
        INDI::PropertyNumber HomePositionNP {1};
        // Home set options
        INDI::PropertySwitch HomeOptionsSP {2};
        enum
        {
            HOMECURRENT,
            HOMEDEFAULT
        };

        // Goto Home
        INDI::PropertySwitch GotoHomeSP {1};
        // Shutter voltage
        INDI::PropertyNumber ShutterVoltsNP {1};
        // Rotator Status
        INDI::PropertyText RotatorStatusTP {1};
        // Shutter Status
        INDI::PropertyText ShutterStatusTP {1};
        // Rotator Calibration
        INDI::PropertySwitch RotatorCalibrationSP {2};
        enum
        {
            ROTATOR_HOME_FIND,
            ROTATOR_HOME_MEASURE
        };
        // Shutter Calibration
        INDI::PropertySwitch ShutterCalibrationSP {1};
        enum
        {
            SHUTTER_HOME_FIND
        };
        // Shutter Configuration
        INDI::PropertyNumber ShutterSettingsNP {4};
        enum
        {
            SHUTTER_MAX_SPEED,
            SHUTTER_MIN_SPEED,
            SHUTTER_ACCELERATION,
            SHUTTER_SAFE_VOLTAGE
        };
        INDI::PropertyNumber ShutterSettingsTimeoutNP {1};

        // Rotator Configuration
        INDI::PropertyNumber RotatorSettingsNP {4};
        enum
        {
            ROTATOR_MAX_SPEED,
            ROTATOR_MIN_SPEED,
            ROTATOR_ACCELERATION,
            ROTATOR_TIMEOUT
        };

        ///////////////////////////////////////////////////////////////////////
        /// Private Variables
        ///////////////////////////////////////////////////////////////////////
        double m_TargetRotatorAz {-1};

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * ROTATOR_TAB = "Rotator";
        static constexpr const char * SHUTTER_TAB = "Shutter";
        // '#' is the stop char
        static const char DRIVER_STOP_CHAR { 0x23 };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receiving.
        static constexpr const uint8_t DRIVER_LEN {128};
        int domeDir = 1;
        double lastAzDiff = 1;
};
