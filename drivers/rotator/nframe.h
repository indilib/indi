/*
     nFrame Rotator by Gene N
    Based on:
    Seletek Rotator Driver

    Copyright (C) 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <indirotator.h>
#include <indifocuserinterface.h>

class nFrameRotator : public INDI::Rotator
{
    public:
        enum FocusDirection
        {
            FOCUS_INWARD,
            FOCUS_OUTWARD
        };

        nFrameRotator();
        virtual ~nFrameRotator() override = default;

        virtual bool Handshake() override;
        const char *getDefaultName() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual IPState MoveRotator(double angle) override;
        virtual bool AbortRotator() override;
        virtual bool SyncRotator(double angle) override;
//ESN        virtual bool SetRotatorBacklash(int32_t steps) override;
//ESN        virtual bool SetRotatorBacklashEnabled(bool enabled) override;
//ESN        virtual bool ReverseRotator(bool enabled) override;
        IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) ;
        IPState MoveAbsFocuser(uint32_t targetTicks) ;
        bool SetFocuserSpeed(uint8_t speed) ;
        bool SyncFocuser(uint32_t ticks) ;

        virtual bool saveConfigItems(FILE *fp) override;

        virtual void TimerHit() override;

// GeneN hack
        // Focuser Speed (if variable speeds are supported)
        INumberVectorProperty FocusSpeedNP;
        INumber FocusSpeedN[1];

        // Focuser Motion switch.
        // For absolute focusers, this controls the directoin of FocusRelPos when updated.
        // For DC speed based focusers, this moves the focuser continues in the CW/CCW directions until stopped.
        ISwitchVectorProperty FocusMotionSP;
        ISwitch FocusMotionS[2];

        // Timer for user with DC focusers to run focuser in specific direction for this duration
        INumberVectorProperty FocusTimerNP;
        INumber FocusTimerN[1];

        // Absolute Focuser Position in steps
        INumberVectorProperty FocusAbsPosNP;
        INumber FocusAbsPosN[1];

        // Relative Focuser position to be commanded
        INumberVectorProperty FocusRelPosNP;
        INumber FocusRelPosN[1];

        // Absolute Focuser position is 0 to this maximum limit. By Default, it is set to 200,000.
        INumberVectorProperty FocusMaxPosNP;
        INumber FocusMaxPosN[1];

        // Sync
        INumberVectorProperty FocusSyncNP;
        INumber FocusSyncN[1];


    private:

        ///////////////////////////////////////////////////////////////////////////////
        /// Set & Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool setParam(const std::string &param, uint32_t value);
        bool getParam(const std::string &param, uint32_t &value);
        bool gotoTarget(uint32_t position);
        // bool setSpeedRange(uint32_t min, uint32_t max);
        bool syncSettings();
        bool echo();
        double calculateAngle(uint32_t steps);

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, int32_t &res);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////
        // Peripheral Ports
        ISwitchVectorProperty PerPortSP;
        ISwitch PerPortS[3];
        enum { PORT_MAIN, PORT_EXP, PORT_THIRD };

        // Motor wiring
        ISwitchVectorProperty WiringSP;
        ISwitch WiringS[4];
        enum { WIRING_LUNATICO_NORMAL, WIRING_LUNATICO_REVERSED, WIRING_RFMOONLITE_NORMAL, WIRING_RFMOONLITE_REVERSED };

        // Halfstep
        ISwitchVectorProperty HalfStepSP;
        ISwitch HalfStepS[2];

        // Motor Mode
        ISwitchVectorProperty MotorTypeSP;
        ISwitch MotorTypeS[4];
        enum { MOTOR_UNIPOLAR, MOTOR_BIPOLAR, MOTOR_DC, MOTOR_STEPDIR };

        // Settings
        INumberVectorProperty SettingNP;
        INumber SettingN[1];
//        enum { PARAM_MIN_SPEED, PARAM_MAX_SPEED, PARAM_MIN_LIMIT, PARAM_MAX_LIMIT, PARAM_STEPS_DEGREE };
        enum { PARAM_STEPS_DEGREE };

        // Rotator Steps
        INumber RotatorAbsPosN[1];
        INumberVectorProperty RotatorAbsPosNP;

        // Firmware Version
        IText FirmwareVersionT[1] {};
        ITextVectorProperty FirmwareVersionTP;

        ///////////////////////////////////////////////////////////////////////
        /// Private Variables
        ///////////////////////////////////////////////////////////////////////
        bool m_IsMoving {false};
        uint32_t m_ZeroPosition {0};

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * SETTINGS_TAB = "Settings";
        // '#' is the stop char
        static const char DRIVER_STOP_CHAR { 0x23 };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {128};
        // Operatives
        static constexpr const uint8_t DRIVER_OPERATIVES {2};
        // Models
        static constexpr const uint8_t DRIVER_MODELS {4};

        ISwitch SteppingModeS[3];
        ISwitchVectorProperty SteppingModeSP;
        enum
        {
            STEPPING_WAVE,
            STEPPING_HALF,
            STEPPING_FULL,
        };

        ISwitch CoilStatusS[2];
        ISwitchVectorProperty CoilStatusSP;
        enum
        {
            COIL_ENERGIZED_OFF,
            COIL_ENERGIZED_ON,
        };

        INumber SteppingPhaseN[1];
        INumberVectorProperty SteppingPhaseNP;

        INumber MaxSpeedN[1];
        INumberVectorProperty MaxSpeedNP;

        bool readPosition();
        bool readCompensationInfo();
        bool readSpeedInfo();
        bool readSteppingInfo();
        bool readCoilStatus();
        bool setSteppingPhase(uint8_t phase);
        bool setCoilStatus(uint8_t status);
        bool setMaxSpeed(uint8_t maxSpeed);

       bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        bool getStartupValues();
        bool isMoving();

        int32_t m_TargetDiff { 0 };
	double requestedAngle = -1.0;
	bool wantAbort = false;


       static constexpr const char * STEPPING_TAB = "Stepping";
        // '#' is the stop char
        static const char NSTEP_STOP_CHAR { 0x23 };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t NSTEP_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t NSTEP_LEN {64};


};
