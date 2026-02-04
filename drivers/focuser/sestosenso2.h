/*
    SestoSenso 2 Focuser
    Copyright (C) 2020 Piotr Zyziuk

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

#include "indifocuser.h"
#include "inditimer.h"
#include "primalucacommandset.h"

class SestoSenso2 : public INDI::Focuser
{
    public:
        SestoSenso2();
        virtual ~SestoSenso2() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual bool Handshake() override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;
        virtual bool SetFocuserBacklash(int32_t steps) override;

        virtual bool saveConfigItems(FILE *fp) override;

    private:
        bool Ack();
        bool setMinLimit(uint32_t limit);
        bool setMaxLimit(uint32_t limit);
        bool updateMaxLimit();

        bool updateTemperature();
        bool updatePosition();
        bool updateVoltageIn();
        bool fetchMotorSettings();
        bool applyMotorRates();
        bool applyMotorCurrents();
        void setConnectionParams();
        bool initCommandSet();
        void checkMotionProgressCallback();
        void checkHallSensorCallback();

        bool getStartupValues();
        void hexDump(char * buf, const char * data, int size);
        bool isMotionComplete();

        FocusDirection backlashDirection { FOCUS_INWARD };
        FocusDirection oldbacklashDirection { FOCUS_INWARD };

        int32_t startPos { 0 };
        uint32_t backlashTicks { 0 };
        uint32_t targetPos { 0 };
        uint32_t lastPos { 0 };
        int32_t previousPos { 0 };
        double lastVoltageIn { 0 };
        double lastTemperature { 0 };
        uint16_t m_TemperatureCounter { 0 };

        INDI::PropertyNumber TemperatureNP {2};
        enum
        {
            TEMPERATURE_EXTERNAL,
            TEMPERATURE_MOTOR,
        };

        INDI::PropertyNumber SpeedNP {1};

        INDI::PropertyText FirmwareTP {2};
        enum
        {
            FIRMWARE_SN,
            FIRMWARE_VERSION,
        };

        INDI::PropertyNumber VoltageInNP {1};

        INDI::PropertySwitch CalibrationSP {2};
        enum
        {
            CALIBRATION_START,
            CALIBRATION_NEXT
        };

        INDI::PropertySwitch FastMoveSP {3};
        enum
        {
            FASTMOVE_IN,
            FASTMOVE_OUT,
            FASTMOVE_STOP
        };

        // New switch properties for semi-automatic calibration
        INDI::PropertySwitch MoveInOut100SP {2};
        enum
        {
            MOVE_IN_100,
            MOVE_OUT_100
        };

        INDI::PropertySwitch MoveInOut500SP {2};
        enum
        {
            MOVE_IN_500,
            MOVE_OUT_500
        };

        INDI::PropertySwitch MoveInOut1000SP {2};
        enum
        {
            MOVE_IN_1000,
            MOVE_OUT_1000
        };

        enum
        {
            CMD_OK = true,
            CMD_FALSE = false
        };

        INDI::PropertyNumber MotorRateNP {3};
        enum
        {
            MOTOR_RATE_ACC,
            MOTOR_RATE_RUN,
            MOTOR_RATE_DEC
        };

        INDI::PropertyNumber MotorCurrentNP {4};
        enum
        {
            MOTOR_CURR_ACC,
            MOTOR_CURR_RUN,
            MOTOR_CURR_DEC,
            MOTOR_CURR_HOLD
        };

        INDI::PropertySwitch MotorHoldSP {2};
        enum
        {
            MOTOR_HOLD_ON,
            MOTOR_HOLD_OFF
        };

        INDI::PropertySwitch MotorApplyPresetSP {3};
        enum
        {
            MOTOR_APPLY_LIGHT,
            MOTOR_APPLY_MEDIUM,
            MOTOR_APPLY_HEAVY,
        };

        INDI::PropertySwitch MotorApplyUserPresetSP {3};
        enum
        {
            MOTOR_APPLY_USER1,
            MOTOR_APPLY_USER2,
            MOTOR_APPLY_USER3
        };

        INDI::PropertySwitch MotorSaveUserPresetSP {3};
        enum
        {
            MOTOR_SAVE_USER1,
            MOTOR_SAVE_USER2,
            MOTOR_SAVE_USER3
        };

        INDI::PropertyText CalibrationMessageTP {1};
        typedef enum { Idle, GoToMiddle, GoMinimum, GoDupa, GoMaximum, Complete } CalibrationStage;
        CalibrationStage cStage { Idle };

        INDI::Timer m_MotionProgressTimer;
        INDI::Timer m_HallSensorTimer;

        enum SestoSensoModel
        {
            SESTOSENSO2,
            SESTOSENSO3_STANDARD,
            SESTOSENSO3_SC,
            SESTOSENSO3_LS,
        };
        SestoSensoModel m_SestoSensoModel { SESTOSENSO2 };

        std::unique_ptr<PrimalucaLabs::SestoSenso2> m_SestoSenso2; // This will be updated to a more generic type or a SestoSenso3 specific one later

        INDI::PropertyNumber RecoveryDelayNP {1}; // New property for recovery delay
        enum
        {
            RECOVERY_DELAY_VALUE,
        };

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        // CR is the stop char
        static const char SESTO_STOP_CHAR { 0xD };
        // Update temperature every 10x POLLMS. For 500ms, we would
        // update the temperature one every 5 seconds.
        static constexpr const uint8_t SESTO_TEMPERATURE_FREQ {10};
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t SESTO_TIMEOUT {5};
        // Maximum buffer for sending/receiving.
        static constexpr const int SESTO_LEN {1024};

};
