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

class CommandSet
{

    public:
        CommandSet(int Port, const char *deviceName)
        {
            PortFD = Port;
            this->deviceName = deviceName;
        }
        int PortFD;
        bool stop();
        bool getSerialNumber(char *res);
        bool getFirmwareVersion(char *res);
        bool abort();
        bool go(uint32_t targetTicks, char *res);
        bool goHome(char *res);
        bool fastMoveOut(char *res);
        bool fastMoveIn(char *res);
        bool getMaxPosition(char *res);
        bool getHallSensor(char *res);
        bool storeAsMaxPosition(char *res);
        bool goOutToFindMaxPos();
        bool storeAsMinPosition();
        bool initCalibration();
        bool getAbsolutePosition(char *res);
        bool getCurrentSpeed(char *res);
        bool applyMotorPreset(const char *name);
        bool applyMotorUserPreset(uint32_t index);
        bool saveMotorUserPreset(uint32_t index, struct MotorRates &mr, struct MotorCurrents &mc);
        bool getMotorTemp(char *res);
        bool getExternalTemp(char *res);
        bool getVoltageIn(char *res);
        bool getMotorSettings(struct MotorRates &ms, struct MotorCurrents &mc, bool &motorHoldActive);
        bool setMotorRates(struct MotorRates &ms);
        bool setMotorCurrents(struct MotorCurrents &mc);
        bool setMotorHold(bool hold);
        std::string deviceName;

        const char *getDeviceName() const
        {
            return deviceName.c_str();
        }

    private:

        // Send request and return full response
        bool send(const std::string &request, std::string &response) const;
        // Send command and parse response looking for value of property
        bool sendCmd(const std::string &cmd, std::string property = "", char *res = nullptr) const;
        bool sendCmd(const std::string &cmd, std::string property, std::string &res) const;
        bool getValueFromResponse(const std::string &response, const std::string &property, char *value) const;
        bool parseUIntFromResponse(const std::string &response, const std::string &property, uint32_t &result) const;

        // Maximum buffer for sending/receving.
        static constexpr const int SESTO_LEN {1024};
        enum
        {
            CMD_OK = true,
            CMD_FALSE = false
        };

};

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

        //        static void checkMotionProgressHelper(void *context);
        //        static void checkHallSensorHelper(void *context);

    protected:
        virtual bool Handshake() override;
        virtual bool Disconnect() override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;

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

        CommandSet *command {nullptr};

        bool getStartupValues();
        void hexDump(char * buf, const char * data, int size);
        bool isMotionComplete();

        uint32_t targetPos { 0 };
        uint32_t lastPos { 0 };
        double lastVoltageIn { 0 };
        double lastTemperature { 0 };
        uint16_t m_TemperatureCounter { 0 };

        INumberVectorProperty TemperatureNP;
        INumber TemperatureN[2];
        enum
        {
            TEMPERATURE_MOTOR,
            TEMPERATURE_EXTERNAL,
        };

        INumber SpeedN[1];
        INumberVectorProperty SpeedNP;

        ITextVectorProperty FirmwareTP;
        IText FirmwareT[2];
        enum
        {
            FIRMWARE_SN,
            FIRMWARE_VERSION,
        };

        INumber VoltageInN[1] {};
        INumberVectorProperty VoltageInNP;

        ISwitch CalibrationS[2];
        ISwitchVectorProperty CalibrationSP;
        enum
        {
            CALIBRATION_START,
            CALIBRATION_NEXT
        };

        ISwitch FastMoveS[3];
        ISwitchVectorProperty FastMoveSP;
        enum
        {
            FASTMOVE_IN,
            FASTMOVE_OUT,
            FASTMOVE_STOP
        };

        enum
        {
            CMD_OK = true,
            CMD_FALSE = false
        };

        INumberVectorProperty MotorRateNP;
        INumber MotorRateN[3];
        enum
        {
            MOTOR_RATE_ACC,
            MOTOR_RATE_RUN,
            MOTOR_RATE_DEC
        };

        INumberVectorProperty MotorCurrentNP;
        INumber MotorCurrentN[4];
        enum
        {
            MOTOR_CURR_ACC,
            MOTOR_CURR_RUN,
            MOTOR_CURR_DEC,
            MOTOR_CURR_HOLD
        };

        ISwitchVectorProperty MotorHoldSP;
        ISwitch MotorHoldS[2];
        enum
        {
            MOTOR_HOLD_ON,
            MOTOR_HOLD_OFF
        };

        ISwitchVectorProperty MotorApplyPresetSP;
        ISwitch MotorApplyPresetS[3];
        enum
        {
            MOTOR_APPLY_LIGHT,
            MOTOR_APPLY_MEDIUM,
            MOTOR_APPLY_HEAVY,
        };

        ISwitchVectorProperty MotorApplyUserPresetSP;
        ISwitch MotorApplyUserPresetS[3];
        enum
        {
            MOTOR_APPLY_USER1,
            MOTOR_APPLY_USER2,
            MOTOR_APPLY_USER3
        };

        ISwitchVectorProperty MotorSaveUserPresetSP;
        ISwitch MotorSaveUserPresetS[3];
        enum
        {
            MOTOR_SAVE_USER1,
            MOTOR_SAVE_USER2,
            MOTOR_SAVE_USER3
        };

        ISwitchVectorProperty HomeSP;
        ISwitch HomeS[1];

        IText CalibrationMessageT[1] {};
        ITextVectorProperty CalibrationMessageTP;

        typedef enum { Idle, GoToMiddle, GoMinimum, GoDupa, GoMaximum, Complete } CalibrationStage;
        CalibrationStage cStage { Idle };

        //        int m_MotionProgressTimerID {-1};
        //        int m_HallSensorTimerID {-1};
        std::unique_ptr<INDI::Timer> m_MotionProgressTimer;
        std::unique_ptr<INDI::Timer> m_HallSensorTimer;
        bool m_IsSestoSenso2 { true };
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
        // Maximum buffer for sending/receving.
        static constexpr const int SESTO_LEN {1024};

};
