/*******************************************************************************
 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2021 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Windows ASCOM driver version 5.1.30

 and

 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 Baader Planetarium Dome INDI Driver

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

#include "scopedome_dome.h"

/**
 * ScopeDome USB Card 2.1
 */
class ScopeDomeUSB21 final: public ScopeDomeCard
{
    public:
        /** Default constructor. */
        ScopeDomeUSB21(ScopeDome *driver, int fd) : parent(driver), PortFD(fd) {}

        /** Destructor. */
        virtual ~ScopeDomeUSB21() = default;

        ScopeDomeUSB21(const ScopeDomeUSB21 &) = delete;
        ScopeDomeUSB21 &operator=(const ScopeDomeUSB21 &) = delete;

        virtual bool detect() override;
        virtual void setPortFD(int fd) override
        {
            PortFD = fd;
        };

        // State polling
        virtual int updateState() override;
        virtual uint32_t getStatus() override;
        virtual int getRotationCounter() override;
        virtual int getRotationCounterExt() override;

        // Information
        virtual void getFirmwareVersions(double &main, double &rotary) override;
        virtual uint32_t getStepsPerRevolution() override;
        virtual bool isCalibrationNeeded() override;

        // Commands
        virtual void abort() override;
        virtual void calibrate() override;
        virtual void findHome() override;
        virtual void controlShutter(ShutterOperation operation) override;

        virtual void resetCounter() override;

        // negative means CCW, positive CW steps
        virtual void move(int steps) override;

        // Input/Output management
        virtual size_t getNumberOfSensors() override;
        virtual SensorInfo getSensorInfo(size_t index) override;
        virtual double getSensorValue(size_t index) override;

        virtual size_t getNumberOfRelays() override;
        virtual RelayInfo getRelayInfo(size_t index) override;
        virtual ISState getRelayState(size_t index) override;
        virtual void setRelayState(size_t index, ISState state) override;

        virtual size_t getNumberOfInputs() override;
        virtual InputInfo getInputInfo(size_t index) override;
        virtual ISState getInputValue(size_t index) override;

        virtual ISState getInputState(AbstractInput input) override;
        virtual int setOutputState(AbstractOutput output, ISState state) override;

        virtual void setHomeSensorPolarity(HomeSensorPolarity polarity) override;
    private:
        // Commands
        typedef enum
        {
            ACK_c = 1,
            FunctionNotSupported,
            MotionConflict,
            ParamError,
            FuncBufferError,
            ConnectionTest = 15,
            SetAllDigital,
            ClearDigitalChannel,
            ClearAllDigital,
            SetDigitalChannel,
            GetDigitalChannel,
            GetAllDigital,

            GetCounter,
            ResetCounter,
            SetCounterDebounceTime,
            SetCounterMax,
            GetCounterMax,
            SetCounterMin,
            GetCounterMin,
            CCWRotation,
            CWRotation,

            GetAnalogChannel,
            OutputAnalogChannel1,
            OutputAnalogChannel2,
            OutputAllAnalog,
            ClearAnalogChannel,
            SetAllAnalog,
            ClearAllAnalog,
            SetAnalogChannel,
            GetVersionFirmware,

            SetAllRelays,
            ClearRelay,
            SetRelay,

            GetStatus,
            GetTemp1,
            GetTemp2,
            GetTemp3,
            GetTemp4,
            GetTemp5,
            GetDscnt,
            GetHum,
            GetTempHum,
            GetAnalog1,
            GetAnalog2,
            Get230,
            EnableAutoClose,
            DisableAutoClose,
            GetAutoClose,

            EnablePosSave,
            DisablePosSave,
            GetPosSave,

            GetCounterExt,
            ResetCounterExt,
            SetCounterDebounceTimeExt,
            SetCounterMaxExt,
            GetCounterMaxExt,

            SetCounterMinExt,
            GetCounterMinExt,

            GetAllDigitalExt,
            StandbyOff,
            StandbyOn,
            GetPowerState,
            SetImpPerTurn,

            UpdateFirmware,
            UpdateRotaryFirmwareSerial,
            UpdateRotaryFirmwareRf,

            GoHome,

            GetMainAnalog1,
            GetMainAnalog2,

            GetPressure,
            GetTempIn,
            GetTempOut,

            GetRotaryCounter1,
            GetRotaryCounter2,
            ResetRotaryCounter1,
            ResetRotaryCounter2,

            RotaryAutoOpen1,
            RotaryAutoOpen2,

            RotaryAutoClose1,
            RotaryAutoClose2,

            GetLinkStrength,

            GetLowVoltageMain,
            SetLowVoltageMain,
            GetLowVoltageRotary,
            SetLowVoltageRotary,

            GetHomeSensorPosition,
            SetHomeSensorPosition,

            GetImpPerTurn,
            Stop,

            GetStartCnt,
            Ready,

            SetStopTime,
            GetStopTime,

            GetCounterDebounceTimeExt,

            SetDebounceTimeInputs,
            GetDebounceTimeInputs,

            FindHome,
            NegHomeSensorActiveState,

            // PowerOnlyAtHome,

            SetAutoCloseEvents,
            GetAutoCloseEvents,
            SetAutoCloseTime,
            GetAutoCloseTime,

            SetShutterConfig,
            GetShutterConfig,

            GetVersionFirmwareRotary,
            GetCommunicationMode,
            SetCommunicationMode,

            SetTherm1Mode,
            SetTherm1Out1,
            SetTherm1Out2,
            SetTherm1Hist,
            SetTherm1VAL,

            GetTherm1Mode,
            GetTherm1Out1,
            GetTherm1Out2,
            GetTherm1Hist,
            GetTherm1VAL,

            SetTherm2Mode,
            SetTherm2Out1,
            SetTherm2Out2,
            SetTherm2Hist,
            SetTherm2VAL,

            GetTherm2Mode,
            GetTherm2Out1,
            GetTherm2Out2,
            GetTherm2Hist,
            GetTherm2VAL,

            SetTherm3Mode,
            SetTherm3Out1,
            SetTherm3Out2,
            SetTherm3Hist,
            SetTherm3VAL,

            GetTherm3Mode,
            GetTherm3Out1,
            GetTherm3Out2,
            GetTherm3Hist,
            GetTherm3VAL,
            StartSafeCommunication,
            StopSafeCommunication,
            SetAutoCloseOrder,
            GetAutoCloseOrder,

            FullSystemCal,
            IsFullSystemCalReq
        } Command;

        typedef enum
        {
            NO_ERROR                       = 0,
            FT_INVALID_HANDLE              = 1,
            FT_DEVICE_NOT_FOUND            = 2,
            FT_DEVICE_NOT_OPENED           = 3,
            FT_IO_ERROR                    = 4,
            FT_INSUFFICIENT_RESOURCES      = 5,
            FT_INVALID_PARAMETER           = 6,
            FT_INVALID_BAUD_RATE           = 7,
            FT_DEVICE_NOT_OPENED_FOR_ERASE = 8,
            FT_DEVICE_NOT_OPENED_FOR_WRITE = 9,
            FT_FAILED_TO_WRITE_DEVICE      = 10,
            FT_EEPROM_READ_FAILED          = 11,
            FT_EEPROM_WRITE_FAILED         = 12,
            FT_EEPROM_ERASE_FAILED         = 13,
            FT_EEPROM_NOT_PRESENT          = 14,
            FT_EEPROM_NOT_PROGRAMMED       = 15,
            FT_INVALID_ARGS                = 16,
            FT_NOT_SUPPORTED               = 17,
            FT_OTHER_ERROR                 = 18,
            NO_CONNECTION                  = 100,
            READ_TIMEOUT_ERROR,
            WRITE_TIMEOUT_ERROR,
            CHECKSUM_ERROR,
            PACKET_LENGTH_ERROR,
            FUNCTION_NOT_SUPPORTED_BY_FIRMWARE,
            PARAM_ERROR,
            BUSY_ERROR,
            AUTHORISATION_ERROR,
            MOTION_CONFLICT,
            FUNCTION_NOT_SUPPORTED,
            COMMAND_SYNC_ERROR,
            CARD_REOPEN,
        } ErrorCode;

        typedef enum
        {
            OUT_CCW            = 0,
            OUT_CW             = 1,
            OUT_OPEN1          = 2,
            OUT_CLOSE1         = 3,
            OUT_FAN            = 4,
            OUT_LIGHT          = 5,
            OUT_CCD            = 6,
            OUT_SCOPE          = 7,
            IN_REMOTE1         = 8,
            IN_REMOTE2         = 9,
            IN_REMOTE3         = 10,
            IN_REMOTE4         = 11,
            IN_ENCODER         = 12,
            IN_HOME            = 13,
            IN_OPEN1           = 14,
            IN_CLOSED1         = 15,
            IN_FREE            = 16,
            IN_S_HOME          = 17,
            IN_SAFE            = 18,
            IN_CLOUD           = 19,
            OUT_RELAY1         = 20,
            OUT_RELAY2         = 21,
            OUT_RELAY3         = 22,
            OUT_RELAY4         = 23,
            OUT_OPEN2          = 24,
            OUT_CLOSE2         = 25,
            IN_OPEN2           = 26,
            IN_CLOSED2         = 27,
            IN_SHIFT           = 28,
            IN_SCOPE_SYNC      = 29,
            IN_WIND_SYNC       = 30,
            IN_WEATHER_PROTECT = 31,
            IN_CLOUDS          = 32,
            IN_ENCODER_ROT     = 33,
            IN_HOME_ROT        = 34,
            IN_ROT_LINK        = 35
        } DigitalIO;

        typedef enum
        {
            STATUS_RESET            = 1,
            STATUS_MOVING           = 2,
            STATUS_HOMING           = 4,
            STATUS_OPEN1            = 8,
            STATUS_OPEN2            = 0x10,
            STATUS_AUTO_CLOSE1      = 0x20,
            STATUS_AUTO_CLOSE2      = 0x40,
            STATUS_CALIBRATING      = 0x80,
            STATUS_FINDING_POWER    = 0x100,
            STATUS_CALIBRATION_STOP = 0x200
        } StatusBits;

        // I/O helper functions
        bool readFloat(Command cmd, float &dst);
        bool readU8(Command cmd, uint8_t &dst);
        bool readS8(Command cmd, int8_t &dst);
        bool readU16(Command cmd, uint16_t &dst);
        bool readS16(Command cmd, int16_t &dst);
        bool readU32(Command cmd, uint32_t &dst);
        bool readS32(Command cmd, int32_t &dst);
        int readBuffer(Command cmd, int len, uint8_t *cbuf);

        int writeCmd(Command cmd);
        int writeU8(Command cmd, uint8_t value);
        int writeU16(Command cmd, uint16_t value);
        int writeU32(Command cmd, uint32_t value);
        int writeBuffer(Command cmd, int len, uint8_t *cbuf);

        int writeBuf(Command Command, uint8_t len, uint8_t *buff);
        int write(Command cmd);
        int readBuf(Command &cmd, uint8_t len, uint8_t *buff);
        int read(Command &cmd);

        ISState getInputState(DigitalIO channel);
        int setOutputState(DigitalIO channel, ISState state);

        void reconnect();
        uint8_t CRC(uint8_t crc, uint8_t data);

        ScopeDome *parent;
        int PortFD;

        // Shadowed card state
        Command prevcmd;
        uint8_t digitalSensorState[5];

        uint8_t linkStrength {0};

        uint16_t status {0};

        int16_t counter {0};

        int32_t counterExt {0};

        float sensors[10];
};
