/*******************************************************************************
 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2019 Jarno Paananen. All rights reserved.

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

#include "indidome.h"
#include <memory>

/* Smart Widget-Property */
#include "indipropertynumber.h"
#include "indipropertyswitch.h"

/**
 * Interface to either a real ScopeDome card or simulator
 */

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
} ScopeDomeCommand;

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
} ScopeDomeError;

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
} ScopeDomeDigitalIO;

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
} ScopeDomeStatusBits;

class ScopeDomeCard
{
  public:
    /** Destructor. */
    virtual ~ScopeDomeCard() = default;

    virtual bool detect()                                                      = 0;
    virtual int writeBuf(ScopeDomeCommand Command, uint8_t len, uint8_t *buff) = 0;
    virtual int write(ScopeDomeCommand cmd)                                    = 0;
    virtual int readBuf(ScopeDomeCommand &cmd, uint8_t len, uint8_t *buff)     = 0;
    virtual int read(ScopeDomeCommand &cmd)                                    = 0;
    const char *getDeviceName() { return (const char *)"ScopeDome Dome"; };
    virtual void setPortFD(int fd) = 0;

  protected:
    /** Default constructor. */
    ScopeDomeCard() = default;

  private:
    /** Prevent copy construction. */
    ScopeDomeCard(const ScopeDomeCard &rOriginalP);
    /** Prevent assignment. */
    ScopeDomeCard &operator=(const ScopeDomeCard &rRhsP);
};

/**
 * ScopeDome USB Card 2.1
 */
class ScopeDomeUSB21 : public ScopeDomeCard
{
  public:
    /** Default constructor. */
    ScopeDomeUSB21(int fd) { PortFD = fd; };
    /** Destructor. */
    virtual ~ScopeDomeUSB21() = default;

    virtual bool detect() override;
    virtual int writeBuf(ScopeDomeCommand Command, uint8_t len, uint8_t *buff) override;
    virtual int write(ScopeDomeCommand cmd) override;
    virtual int readBuf(ScopeDomeCommand &cmd, uint8_t len, uint8_t *buff) override;
    virtual int read(ScopeDomeCommand &cmd) override;
    virtual void setPortFD(int fd) override { PortFD = fd; };

  private:
    uint8_t CRC(uint8_t crc, uint8_t data);

    /** Prevent copy construction. */
    ScopeDomeUSB21(const ScopeDomeUSB21 &rOriginalP);
    /** Prevent assignment. */
    ScopeDomeUSB21 &operator=(const ScopeDomeUSB21 &rRhsP);

    int PortFD;

    ScopeDomeCommand prevcmd;
};

/**
 * ScopeDome simulator
 */
class ScopeDomeSim : public ScopeDomeCard
{
  public:
    /** Default constructor. */
    ScopeDomeSim(){};
    /** Destructor. */
    virtual ~ScopeDomeSim() = default;

    virtual bool detect() override;
    virtual int writeBuf(ScopeDomeCommand Command, uint8_t len, uint8_t *buff) override;
    virtual int write(ScopeDomeCommand cmd) override;
    virtual int readBuf(ScopeDomeCommand &cmd, uint8_t len, uint8_t *buff) override;
    virtual int read(ScopeDomeCommand &cmd) override;
    virtual void setPortFD(int fd) override { (void)fd; };

  private:
    ScopeDomeCommand lastCmd;

    int stepsPerRevolution;
    int currentStep;

    int shutterStatus;

    /** Prevent copy construction. */
    ScopeDomeSim(const ScopeDomeSim &rOriginalP);
    /** Prevent assignment. */
    ScopeDomeSim &operator=(const ScopeDomeSim &rRhsP);
};

class ScopeDome : public INDI::Dome
{
  public:
    typedef enum
    {
        DOME_UNKNOWN,
        DOME_CALIBRATING,
        DOME_READY,
        DOME_HOMING,
        DOME_DEROTATING
    } DomeStatus;

    ScopeDome();
    virtual ~ScopeDome() = default;

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool saveConfigItems(FILE *fp) override;

    virtual bool Handshake() override;

    virtual void TimerHit() override;

    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
    virtual IPState MoveRel(double azDiff) override;
    virtual IPState MoveAbs(double az) override;
    virtual IPState ControlShutter(ShutterOperation operation) override;
    virtual bool Abort() override;

    // Parking
    virtual IPState Park() override;
    virtual IPState UnPark() override;
    virtual bool SetCurrentPark() override;
    virtual bool SetDefaultPark() override;

  protected:
    // Commands
    bool Ack();
    bool UpdatePosition();
    bool UpdateShutterStatus();
    bool UpdateSensorStatus();
    bool UpdateRelayStatus();

    // Misc
    bool SetupParms();

    DomeStatus status{ DOME_UNKNOWN };
    double targetAz{ 0 };
    bool refineMove{ false };
    ShutterOperation targetShutter{ SHUTTER_OPEN };
    bool sim{ false };
    double simShutterTimer{ 0 };
    ShutterState simShutterStatus{ SHUTTER_OPENED };
    INDI::PropertyNumber DomeHomePositionNP {1};

    INDI::PropertySwitch FindHomeSP {1};

    INDI::PropertySwitch DerotateSP {1};

    INDI::PropertySwitch PowerRelaysSP {4};

    INDI::PropertySwitch RelaysSP {4};

    INDI::PropertySwitch ParkShutterSP {2};

    INDI::PropertySwitch AutoCloseSP {8};

    INDI::PropertyNumber EnvironmentSensorsNP {11};

    INDI::PropertySwitch SensorsSP {13};

    INDI::PropertyNumber StepsPerRevolutionNP {1};

    INDI::PropertySwitch CalibrationNeededSP {1};

    INDI::PropertySwitch StartCalibrationSP {1};

    INDI::PropertyNumber FirmwareVersionsNP {2};

  private:
    uint8_t digitalSensorState[5];
    uint16_t currentStatus;
    int32_t currentRotation;
    int16_t rotationCounter;

    uint8_t linkStrength;
    float sensors[9];
    uint32_t stepsPerTurn;
    int homePosition;

    std::unique_ptr<ScopeDomeCard> interface;

    void reconnect();

    IPState sendMove(double azDiff);

    // I/O helper functions
    bool readFloat(ScopeDomeCommand cmd, float &dst);
    bool readU8(ScopeDomeCommand cmd, uint8_t &dst);
    bool readS8(ScopeDomeCommand cmd, int8_t &dst);
    bool readU16(ScopeDomeCommand cmd, uint16_t &dst);
    bool readS16(ScopeDomeCommand cmd, int16_t &dst);
    bool readU32(ScopeDomeCommand cmd, uint32_t &dst);
    bool readS32(ScopeDomeCommand cmd, int32_t &dst);
    int readBuffer(ScopeDomeCommand cmd, int len, uint8_t *cbuf);

    int writeCmd(ScopeDomeCommand cmd);
    int writeU8(ScopeDomeCommand cmd, uint8_t value);
    int writeU16(ScopeDomeCommand cmd, uint16_t value);
    int writeU32(ScopeDomeCommand cmd, uint32_t value);
    int writeBuffer(ScopeDomeCommand cmd, int len, uint8_t *cbuf);

    // Dew point calculation
    float getDewPoint(float RH, float T);

    // Input/Output management
    ISState getInputState(ScopeDomeDigitalIO channel);
    int setOutputState(ScopeDomeDigitalIO channel, ISState state);

    // Dome inertia compensation
    std::vector<uint8_t> inertiaTable;
    uint16_t compensateInertia(uint16_t steps);
};
