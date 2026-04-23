/*
    SestoSenso 3 Focuser
    Copyright (C) 2026 Jasem Mutlaq

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

class SestoSenso3 : public INDI::Focuser
{
    public:
        SestoSenso3();
        virtual ~SestoSenso3() override = default;

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
        bool getStartupValues();
        bool isMotionComplete();

        // Submodel variants
        enum SestoSenso3Model
        {
            SESTOSENSO3_STANDARD,
            SESTOSENSO3_SC,
            SESTOSENSO3_LS,
        };
        SestoSenso3Model m_SubModel { SESTOSENSO3_STANDARD };

        FocusDirection backlashDirection { FOCUS_INWARD };
        uint32_t backlashTicks { 0 };
        uint32_t targetPos { 0 };
        uint32_t lastPos { 0 };
        double lastVoltageIn { 0 };
        double lastTemperature { 0 };
        uint16_t m_TemperatureCounter { 0 };

        std::unique_ptr<PrimalucaLabs::SestoSenso3> m_SestoSenso3;

        // Properties
        INDI::PropertyText FirmwareTP {2};
        enum
        {
            FIRMWARE_SN,
            FIRMWARE_VERSION,
        };

        INDI::PropertyNumber VoltageInNP {1};

        INDI::PropertyNumber TemperatureNP {2};
        enum
        {
            TEMPERATURE_EXTERNAL,
            TEMPERATURE_MOTOR,
        };

        INDI::PropertyText CalibrationMessageTP {1};

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

        // Semi-automatic calibration move switches (STANDARD and LS only)
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

        INDI::PropertySwitch MotorHoldSP {2};
        enum
        {
            MOTOR_HOLD_ON,
            MOTOR_HOLD_OFF
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

        INDI::PropertySwitch MotorApplyPresetSP {3};
        enum
        {
            MOTOR_APPLY_LIGHT,
            MOTOR_APPLY_MEDIUM,
            MOTOR_APPLY_HEAVY,
        };

        INDI::PropertyNumber RecoveryDelayNP {1};
        enum
        {
            RECOVERY_DELAY_VALUE,
        };

        typedef enum { Idle, GoToMiddle, GoMinimum, GoMaximum, Complete } CalibrationStage;
        CalibrationStage cStage { Idle };

        INDI::Timer m_MotionProgressTimer;

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static const char SESTO_STOP_CHAR { 0xD };
        static constexpr const uint8_t SESTO_TEMPERATURE_FREQ {10};
        static constexpr const uint8_t SESTO_TIMEOUT {5};
        static constexpr const int SESTO_LEN {1024};
};
