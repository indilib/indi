/*
    Primaluca Labs Essato-Arco Focuser+Rotator Driver

    Copyright (C) 2020 Piotr Zyziuk
    Copyright (C) 2020-2022 Jasem Mutlaq

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

    JM 2022.07.16: Major refactor to using json.h and update to Essato Arco
    Document protocol revision 3.3 (8th July 2022).
*/

#pragma once

#include "indifocuser.h"
#include "inditimer.h"
#include "indirotatorinterface.h"
#include "json.h"

using json = nlohmann::json;

namespace CommandSet
{
typedef struct MotorRates
{
    // Rate values: 1-10
    uint32_t accRate = 0, runSpeed = 0, decRate = 0;
} MotorRates;

typedef struct MotorCurrents
{
    // Current values: 1-10
    uint32_t accCurrent = 0, runCurrent = 0, decCurrent = 0;
    // Hold current: 1-5
    uint32_t holdCurrent = 0;
} MotorCurrents;

    namespace Essato
    {        
        bool setMotorCommandDone(const json &command);

        template <typename T = int32_t> bool getMotorParameter(const std::string &parameter, T &value);
        template <typename T = int32_t> bool setMotorCommand(const json &command, T *response = nullptr);


        bool stop();
        bool getSerialNumber(std::string &response);
        bool getFirmwareVersion(std::string &response);
        bool abort();
        bool go(uint32_t targetTicks);
        bool goHome(char *res);
        bool fastMoveOut();
        bool fastMoveIn();
        bool getMaxPosition(uint32_t &position);
        bool getHallSensor(char *res);
        bool storeAsMaxPosition();
        bool goOutToFindMaxPos();
        bool storeAsMinPosition();
        bool initCalibration();
        bool getAbsolutePosition(char *res);
        bool getCurrentSpeed(char *res);
        bool applyMotorPreset(const std::string &name);
        bool saveMotorUserPreset(uint32_t index, const MotorRates &rates, const MotorCurrents &currents);
        bool getMotorTemp(double &value);
        bool getExternalTemp(double &value);
        bool getVoltageIn(double &value);
        bool getMotorSettings(struct MotorRates &rates, struct MotorCurrents &currents, bool &motorHoldActive);
        bool setMotorRates(const MotorRates &rates);
        bool setMotorCurrents(const MotorCurrents &current);
        bool setMotorHold(bool hold);
    }

    namespace Arco
    {
        typedef enum
        {
            UNIT_STEPS,
            UNIT_DEGREES,
            UNIT_ARCSECS
        } Units;

        // Arco functions
        bool isDetected();
        bool getAbsolutePosition(Units unit, double &value);
        bool setAbsolutePoition(Units unit, double value);
        bool sync(Units unit, double angle);
        bool isBusy();
        bool stop();
        bool calibrate();
        bool isCalibrating();
    }

    std::string deviceName;
    int PortFD {-1};
    const char *getDeviceName() { return deviceName.c_str();}
    // Send command
    bool sendCommand(const std::string &command, json *response = nullptr);

        // Maximum buffer for sending/receving.
        static constexpr const int DRIVER_LEN {1024};
        static const char DRIVER_STOP_CHAR { 0xD };
        static const char DRIVER_TIMEOUT { 5 };
        enum
        {
            CMD_OK = true,
            CMD_FALSE = false
        };
};


class EssatoArco : public INDI::Focuser, public INDI::RotatorInterface
{
    public:
        EssatoArco();
        virtual ~EssatoArco() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual bool Handshake() override;
        virtual bool Disconnect() override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;
        virtual bool SetFocuserBacklash(int32_t steps) override;

        virtual bool saveConfigItems(FILE *fp) override;

        // Rotator
        virtual IPState MoveRotator(double angle) override;
        virtual bool SyncRotator(double angle) override;
        virtual bool AbortRotator() override;
        virtual bool ReverseRotator(bool enabled) override;

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

        INumberVectorProperty TemperatureNP;
        INumber TemperatureN[2];
        enum
        {
            TEMPERATURE_EXTERNAL,
            TEMPERATURE_MOTOR,
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

        ISwitch BacklashS[2];
        ISwitchVectorProperty BacklashSP;
        enum
        {
            BACKLASH_START,
            BACKLASH_NEXT
        };
        IText BacklashMessageT[1] {};
        ITextVectorProperty BacklashMessageTP;
        typedef enum { BacklashIdle, BacklashMinimum, BacklashMaximum, BacklashComplete } BacklashStage;
        BacklashStage bStage { BacklashIdle };

        //Rotator
        INumber RotatorAbsPosN[1];
        INumberVectorProperty RotatorAbsPosNP;
        enum
        {
            ROTATOR_ABSOLUTE_POSITION
        };

        double lastRotatorPosition { 0 };

        //ROTATOR CALIBRATION
        IText RotCalibrationMessageT[1] {};
        ITextVectorProperty RotCalibrationMessageTP;

        typedef enum { RotCalIdle, RotCalComplete } RotCalibrationStage;
        RotCalibrationStage rcStage { RotCalIdle };

        ISwitch RotCalibrationS[1];
        ISwitchVectorProperty RotCalibrationSP;
        enum
        {
            ARCO_CALIBRATION_START
        };

        INDI::Timer m_MotionProgressTimer;
        INDI::Timer m_HallSensorTimer;
};
