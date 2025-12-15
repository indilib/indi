/*******************************************************************************
 Rigel Systems Dome INDI Driver

 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

 Based on Protocol extracted from https://github.com/rpineau/RigelDome

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

class RigelDome : public INDI::Dome
{
    public:
        typedef enum { S_Open, S_Closed, S_Opening, S_Closing, S_Error, S_UnKnown, S_NotFitted } RigelShutterState;
        typedef enum { M_Idle, M_MovingToTarget, M_MovingToVelocity, M_MovingAtSideral,
                       M_MovingCCW, M_MovingCW,  M_Calibrating, M_Homing
                     } RigelMotorState;

        RigelDome();
        virtual ~RigelDome() override = default;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n) override;
        virtual const char *getDefaultName() override;

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

        RigelShutterState m_rawShutterState {S_UnKnown};
        RigelMotorState m_rawMotorState {M_Idle};
        double targetAz { 0 };

        // Pulsar Dome Drive workaround for stuck motor detection
        double previousAngle { -1 };
        int stuckAngleCounter { 0 };
        static constexpr const int STUCK_THRESHOLD = 3;  // 3 consecutive cycles
        static constexpr const double ANGLE_TOLERANCE = 1.0;  // 1 degree

        ///////////////////////////////////////////////////////////////////////////////
        /// Shutter Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool setShutterConnected(bool enabled);
        bool isShutterConnected();
        bool readShutterStatus();
        ShutterState parseShutterState(int state);

        ///////////////////////////////////////////////////////////////////////////////
        /// Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool readFirmware();
        bool readModel();
        bool readPosition();
        bool readHomePosition();
        bool getStartupValues();
        bool readState();
        bool readBatteryLevels();
        bool readStepsPerRevolution();

        ///////////////////////////////////////////////////////////////////////////////
        /// Parking, Homing, and Calibration
        ///////////////////////////////////////////////////////////////////////////////
        bool setParkAz(double az);
        bool home();
        bool calibrate();
        bool setHome(double az);

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////
        INDI::PropertySwitch OperationSP {2};
        enum
        {
            OPERATION_FIND_HOME,
            OPERATION_CALIBRATE,
        };

        // Info
        INDI::PropertyText InfoTP {4};
        enum
        {
            INFO_FIRMWARE,
            INFO_MODEL,
            INFO_TICKS,
            INFO_BATTERY,
        };

        // Home angle
        INDI::PropertyNumber HomePositionNP {1};

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * INFO_TAB = "Info";
        // 0xD is the stop char
        static const char DRIVER_STOP_CHAR { 0x0D };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receiving.
        static constexpr const uint8_t DRIVER_LEN {64};
};
