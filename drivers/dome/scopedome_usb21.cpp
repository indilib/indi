/*******************************************************************************
 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2024 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Windows ASCOM driver version 5.1.30

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

#include "scopedome_usb21.h"
#include "indicom.h"

#include <termios.h>
#include <unistd.h>
#include <vector>

#define SCOPEDOME_TIMEOUT 2

static const uint8_t header = 0xaa;

const char* ScopeDomeUSB21::cmdToString(Command cmd) const
{
    static const CmdStr cmdStrMap[] =
    {
        { ACK_c, "ACK_c" },
        { FunctionNotSupported, "FunctionNotSupported" },
        { MotionConflict, "MotionConflict" },
        { ParamError, "ParamError" },
        { FuncBufferError, "FuncBufferError" },
        { ConnectionTest, "ConnectionTest" },
        { SetAllDigital, "SetAllDigital" },
        { ClearDigitalChannel, "ClearDigitalChannel" },
        { ClearAllDigital, "ClearAllDigital" },
        { SetDigitalChannel, "SetDigitalChannel" },
        { GetDigitalChannel, "GetDigitalChannel" },
        { GetAllDigital, "GetAllDigital" },

        { GetCounter, "GetCounter" },
        { ResetCounter, "ResetCounter" },
        { SetCounterDebounceTime, "SetCounterDebounceTime" },
        { SetCounterMax, "SetCounterMax" },
        { GetCounterMax, "GetCounterMax" },
        { SetCounterMin, "SetCounterMin" },
        { GetCounterMin, "GetCounterMin" },
        { CCWRotation, "CCWRotation" },
        { CWRotation, "CWRotation" },

        { GetAnalogChannel, "GetAnalogChannel" },
        { OutputAnalogChannel1, "OutputAnalogChannel1" },
        { OutputAnalogChannel2, "OutputAnalogChannel2" },
        { OutputAllAnalog, "OutputAllAnalog" },
        { ClearAnalogChannel, "ClearAnalogChannel" },
        { SetAllAnalog, "SetAllAnalog" },
        { ClearAllAnalog, "ClearAllAnalog" },
        { SetAnalogChannel, "SetAnalogChannel" },
        { GetVersionFirmware, "GetVersionFirmware" },

        { SetAllRelays, "SetAllRelays" },
        { ClearRelay, "ClearRelay" },
        { SetRelay, "SetRelay" },

        { GetStatus, "GetStatus" },
        { GetTemp1, "GetTemp1" },
        { GetTemp2, "GetTemp2" },
        { GetTemp3, "GetTemp3" },
        { GetTemp4, "GetTemp4" },
        { GetTemp5, "GetTemp5" },
        { GetDscnt, "GetDscnt" },
        { GetHum, "GetHum" },
        { GetTempHum, "GetTempHum" },
        { GetAnalog1, "GetAnalog1" },
        { GetAnalog2, "GetAnalog2" },
        { Get230, "Get230" },
        { EnableAutoClose, "EnableAutoClose" },
        { DisableAutoClose, "DisableAutoClose" },
        { GetAutoClose, "GetAutoClose" },

        { EnablePosSave, "EnablePosSave" },
        { DisablePosSave, "DisablePosSave" },
        { GetPosSave, "GetPosSave" },

        { GetCounterExt, "GetCounterExt" },
        { ResetCounterExt, "ResetCounterExt" },
        { SetCounterDebounceTimeExt, "SetCounterDebounceTimeExt" },
        { SetCounterMaxExt, "SetCounterMaxExt" },
        { GetCounterMaxExt, "GetCounterMaxExt" },

        { SetCounterMinExt, "SetCounterMinExt" },
        { GetCounterMinExt, "GetCounterMinExt" },

        { GetAllDigitalExt, "GetAllDigitalExt" },
        { StandbyOff, "StandbyOff" },
        { StandbyOn, "StandbyOn" },
        { GetPowerState, "GetPowerState" },
        { SetImpPerTurn, "SetImpPerTurn" },

        { UpdateFirmware, "UpdateFirmware" },
        { UpdateRotaryFirmwareSerial, "UpdateRotaryFirmwareSerial" },
        { UpdateRotaryFirmwareRf, "UpdateRotaryFirmwareRf" },

        { GoHome, "GoHome" },

        { GetMainAnalog1, "GetMainAnalog1" },
        { GetMainAnalog2, "GetMainAnalog2" },

        { GetPressure, "GetPressure" },
        { GetTempIn, "GetTempIn" },
        { GetTempOut, "GetTempOut" },

        { GetRotaryCounter1, "GetRotaryCounter1" },
        { GetRotaryCounter2, "GetRotaryCounter2" },
        { ResetRotaryCounter1, "ResetRotaryCounter1" },
        { ResetRotaryCounter2, "ResetRotaryCounter2" },

        { RotaryAutoOpen1, "RotaryAutoOpen1" },
        { RotaryAutoOpen2, "RotaryAutoOpen2" },

        { RotaryAutoClose1, "RotaryAutoClose1" },
        { RotaryAutoClose2, "RotaryAutoClose2" },

        { GetLinkStrength, "GetLinkStrength" },

        { GetLowVoltageMain, "GetLowVoltageMain" },
        { SetLowVoltageMain, "SetLowVoltageMain" },
        { GetLowVoltageRotary, "GetLowVoltageRotary" },
        { SetLowVoltageRotary, "SetLowVoltageRotary" },

        { GetHomeSensorPosition, "GetHomeSensorPosition" },
        { SetHomeSensorPosition, "SetHomeSensorPosition" },

        { GetImpPerTurn, "GetImpPerTurn" },
        { Stop, "Stop" },

        { GetStartCnt, "GetStartCnt" },
        { Ready, "Ready" },

        { SetStopTime, "SetStopTime" },
        { GetStopTime, "GetStopTime" },

        { GetCounterDebounceTimeExt, "GetCounterDebounceTimeExt" },

        { SetDebounceTimeInputs, "SetDebounceTimeInputs" },
        { GetDebounceTimeInputs, "GetDebounceTimeInputs" },

        { FindHome, "FindHome" },
        { NegHomeSensorActiveState, "NegHomeSensorActiveState" },

        // { PowerOnlyAtHome, "PowerOnlyAtHome" },

        { SetAutoCloseEvents, "SetAutoCloseEvents" },
        { GetAutoCloseEvents, "GetAutoCloseEvents" },
        { SetAutoCloseTime, "SetAutoCloseTime" },
        { GetAutoCloseTime, "GetAutoCloseTime" },

        { SetShutterConfig, "SetShutterConfig" },
        { GetShutterConfig, "GetShutterConfig" },

        { GetVersionFirmwareRotary, "GetVersionFirmwareRotary" },
        { GetCommunicationMode, "GetCommunicationMode" },
        { SetCommunicationMode, "SetCommunicationMode" },

        { SetTherm1Mode, "SetTherm1Mode" },
        { SetTherm1Out1, "SetTherm1Out1" },
        { SetTherm1Out2, "SetTherm1Out2" },
        { SetTherm1Hist, "SetTherm1Hist" },
        { SetTherm1VAL, "SetTherm1VAL" },

        { GetTherm1Mode, "GetTherm1Mode" },
        { GetTherm1Out1, "GetTherm1Out1" },
        { GetTherm1Out2, "GetTherm1Out2" },
        { GetTherm1Hist, "GetTherm1Hist" },
        { GetTherm1VAL, "GetTherm1VAL" },

        { SetTherm2Mode, "SetTherm2Mode" },
        { SetTherm2Out1, "SetTherm2Out1" },
        { SetTherm2Out2, "SetTherm2Out2" },
        { SetTherm2Hist, "SetTherm2Hist" },
        { SetTherm2VAL, "SetTherm2VAL" },

        { GetTherm2Mode, "GetTherm2Mode" },
        { GetTherm2Out1, "GetTherm2Out1" },
        { GetTherm2Out2, "GetTherm2Out2" },
        { GetTherm2Hist, "GetTherm2Hist" },
        { GetTherm2VAL, "GetTherm2VAL" },

        { SetTherm3Mode, "SetTherm3Mode" },
        { SetTherm3Out1, "SetTherm3Out1" },
        { SetTherm3Out2, "SetTherm3Out2" },
        { SetTherm3Hist, "SetTherm3Hist" },
        { SetTherm3VAL, "SetTherm3VAL" },

        { GetTherm3Mode, "GetTherm3Mode" },
        { GetTherm3Out1, "GetTherm3Out1" },
        { GetTherm3Out2, "GetTherm3Out2" },
        { GetTherm3Hist, "GetTherm3Hist" },
        { GetTherm3VAL, "GetTherm3VAL" },
        { StartSafeCommunication, "StartSafeCommunication" },
        { StopSafeCommunication, "StopSafeCommunication" },
        { SetAutoCloseOrder, "SetAutoCloseOrder" },
        { GetAutoCloseOrder, "GetAutoCloseOrder" },

        { FullSystemCal, "FullSystemCal" },
        { IsFullSystemCalReq, "IsFullSystemCalReq" }
    };

    for (auto i = 0u; i < sizeof(cmdStrMap) / sizeof(CmdStr); ++i)
    {
        if (cmdStrMap[i].cmd == cmd)
        {
            return cmdStrMap[i].str;
        }
    }
    return "Unknown command";
}

bool ScopeDomeUSB21::detect()
{
    int rc = -1;
    Command cmd;
    LOGF_DEBUG("Detect! %d", rc);
    rc = write(ConnectionTest);
    LOGF_DEBUG("write rc: %d", rc);
    rc = read(cmd);
    LOGF_DEBUG("read rc: %d, cmd %s (%d)", rc, cmdToString(cmd), (int)cmd);

    if (cmd != ConnectionTest)
    {
        return false;
    }
    // Disable "safe" communication mode that resets connection after few seconds
    rc = write(StopSafeCommunication);
    rc = read(cmd);
    return (cmd == StopSafeCommunication);
}

uint8_t ScopeDomeUSB21::CRC(uint8_t crc, uint8_t data)
{
    crc ^= data;

    for (int i = 0; i < 8; i++)
    {
        if (crc & 1)
            crc = ((crc >> 1) ^ 0x8C);
        else
            crc >>= 1;
    }
    return crc;
}

int ScopeDomeUSB21::writeBuf(Command cmd, uint8_t len, uint8_t *buff)
{
    int BytesToWrite   = len + 4;
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    std::vector<uint8_t> cbuf(BytesToWrite, 0);
    cbuf[0] = header;
    cbuf[3] = CRC(0, cbuf[0]);
    cbuf[1] = len;
    cbuf[3] = CRC(cbuf[3], cbuf[1]);
    cbuf[2] = cmd;
    cbuf[3] = CRC(cbuf[3], cbuf[2]);

    for (int i = 0; i < len; i++)
    {
        cbuf[i + 4] = buff[i];
        cbuf[3]     = CRC(cbuf[3], buff[i]);
    }

    tcflush(PortFD, TCIOFLUSH);

    prevcmd = cmd;

    // Write buffer
    LOGF_DEBUG("write buf: %x %x %x %x %x", cbuf[0], cbuf[1], cbuf[2], cbuf[3], cbuf[4]);
    if ((rc = tty_write(PortFD, (const char *)cbuf.data(), cbuf.size() * sizeof(cbuf[0]), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error writing command: %s. Cmd %s (%d)", errstr, cmdToString(cmd), cmd);
    }
    return rc;
}

int ScopeDomeUSB21::write(Command cmd)
{
    int nbytes_written = 0, rc = -1;
    uint8_t cbuf[4];
    char errstr[MAXRBUF];

    tcflush(PortFD, TCIOFLUSH);

    cbuf[0] = header;
    cbuf[3] = CRC(0, cbuf[0]);
    cbuf[1] = 0;
    cbuf[3] = CRC(cbuf[3], cbuf[1]);
    cbuf[2] = cmd;
    cbuf[3] = CRC(cbuf[3], cbuf[2]);

    prevcmd = cmd;

    // Write buffer
    LOGF_DEBUG("write cmd: %x %x %x %x", cbuf[0], cbuf[1], cbuf[2], cbuf[3]);
    if ((rc = tty_write(PortFD, (const char *)cbuf, sizeof(cbuf), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error writing command: %s. Cmd: %s (%d)", errstr, cmdToString(cmd), cmd);
    }
    return rc;
}

int ScopeDomeUSB21::readBuf(Command &cmd, uint8_t len, uint8_t *buff)
{
    int nbytes_read = 0, rc = -1;
    int BytesToRead = len + 4;
    std::vector<uint8_t> cbuf(BytesToRead, 0);
    char errstr[MAXRBUF];

    // Read buffer
    if ((rc = tty_read(PortFD, (char *)cbuf.data(), cbuf.size() * sizeof(cbuf[0]), SCOPEDOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error reading: %s. Cmd: %s (%d)", errstr, cmdToString(prevcmd), prevcmd);
        return rc;
    }

    LOGF_DEBUG("readbuf cmd: %x %x %x %x", cbuf[0], cbuf[1], cbuf[2], cbuf[3]);
    uint8_t Checksum = CRC(0, cbuf[0]);
    Checksum         = CRC(Checksum, cbuf[1]);
    cmd              = (Command)cbuf[2];
    Checksum         = CRC(Checksum, cbuf[2]);

    for (int i = 0; i < len; i++)
    {
        buff[i]  = cbuf[i + 4];
        Checksum = CRC(Checksum, cbuf[i + 4]);
    }

    if (cbuf[3] != Checksum)
    {
        LOGF_ERROR("readbuf checksum error, cmd: %s (%d)", cmdToString(prevcmd), prevcmd);
        return CHECKSUM_ERROR;
    }
    if (cmd == FunctionNotSupported)
    {
        LOGF_ERROR("readbuf not supported error, cmd: %s (%d)", cmdToString(prevcmd), prevcmd);
        return FUNCTION_NOT_SUPPORTED_BY_FIRMWARE;
    }

    if (cbuf[1] != len)
    {
        LOGF_ERROR("readbuf packet length error, cmd: %s (%d)", cmdToString(prevcmd), prevcmd);
        return PACKET_LENGTH_ERROR;
    }
    return rc;
}

int ScopeDomeUSB21::read(Command &cmd)
{
    int nbytes_read = 0, rc = -1;
    int err         = 0;
    uint8_t cbuf[4] = { 0 };
    char errstr[MAXRBUF];

    // Read buffer
    if ((rc = tty_read(PortFD, (char *)cbuf, sizeof(cbuf), SCOPEDOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error reading: %s. Cmd %s (%d)", errstr, cmdToString(prevcmd), prevcmd);
        return rc;
    }

    LOGF_DEBUG("read cmd: %x %x %x %x", cbuf[0], cbuf[1], cbuf[2], cbuf[3]);
    uint8_t Checksum = CRC(0, cbuf[0]);
    Checksum         = CRC(Checksum, cbuf[1]);
    cmd              = (Command)cbuf[2];
    Checksum         = CRC(Checksum, cbuf[2]);

    if (cbuf[3] != Checksum || cbuf[1] != 0)
    {
        LOGF_ERROR("read checksum error, cmd: %s (%d)", cmdToString(prevcmd), prevcmd);
        return CHECKSUM_ERROR;
    }
    switch (cmd)
    {
        case MotionConflict:
            LOGF_ERROR("read motion conflict, cmd: %s (%d)", cmdToString(prevcmd), prevcmd);
            err = MOTION_CONFLICT;
            break;

        case FunctionNotSupported:
            LOGF_ERROR("read function not supported, cmd: %s (%d)", cmdToString(prevcmd), prevcmd);
            err = FUNCTION_NOT_SUPPORTED;
            break;

        case ParamError:
            LOGF_ERROR("read param error, cmd: %s (%d)", cmdToString(prevcmd), prevcmd);
            err = PARAM_ERROR;
            break;
        default:
            break;
    }
    return err;
}

/************************************************************************************
 *
 * ***********************************************************************************/
bool ScopeDomeUSB21::readFloat(Command cmd, float &dst)
{
    float value;
    Command c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = write(cmd);
        if (rc == 0)
            rc = readBuf(c, 4, (uint8_t *)&value);
        else
            reconnect();
    }
    while (rc != 0 && --retryCount);
    LOGF_DEBUG("readFloat: %s (%d) = %f", cmdToString(cmd), cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDomeUSB21::readU8(Command cmd, uint8_t &dst)
{
    uint8_t value;
    Command c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = write(cmd);
        if (rc == 0)
            rc = readBuf(c, 1, &value);
        else
            reconnect();
    }
    while (rc != 0 && --retryCount);
    LOGF_DEBUG("readU8: %s (%d) = 0x%x", cmdToString(cmd), cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDomeUSB21::readS8(Command cmd, int8_t &dst)
{
    int8_t value;
    Command c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = write(cmd);
        if (rc == 0)
            rc = readBuf(c, 1, (uint8_t *)&value);
        else
            reconnect();
    }
    while (rc != 0 && --retryCount);
    LOGF_DEBUG("readS8: %s (%d) = 0x%x", cmdToString(cmd), cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDomeUSB21::readU16(Command cmd, uint16_t &dst)
{
    uint16_t value;
    Command c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = write(cmd);
        if (rc == 0)
            rc = readBuf(c, 2, (uint8_t *)&value);
        else
            reconnect();
    }
    while (rc != 0 && --retryCount);
    LOGF_DEBUG("readU16: %s (%d) = 0x%x", cmdToString(cmd), cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDomeUSB21::readS16(Command cmd, int16_t &dst)
{
    int16_t value;
    Command c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = write(cmd);
        if (rc == 0)
            rc = readBuf(c, 2, (uint8_t *)&value);
        else
            reconnect();
    }
    while (rc != 0 && --retryCount);
    LOGF_DEBUG("readS16: %s (%d) = 0x%x", cmdToString(cmd), cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDomeUSB21::readU32(Command cmd, uint32_t &dst)
{
    uint32_t value;
    Command c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = write(cmd);
        if (rc == 0)
            rc = readBuf(c, 4, (uint8_t *)&value);
        else
            reconnect();
    }
    while (rc != 0 && --retryCount);
    LOGF_DEBUG("readU32: %s (%d) = 0x%x", cmdToString(cmd), cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

bool ScopeDomeUSB21::readS32(Command cmd, int32_t &dst)
{
    int32_t value;
    Command c;
    int rc;
    int retryCount = 2;
    do
    {
        rc = write(cmd);
        if (rc == 0)
            rc = readBuf(c, 4, (uint8_t *)&value);
        else
            reconnect();
    }
    while (rc != 0 && --retryCount);
    LOGF_DEBUG("readS32: %s (%d) = 0x%x", cmdToString(cmd), cmd, value);
    if (rc == 0)
    {
        dst = value;
        return true;
    }
    return false;
}

int ScopeDomeUSB21::readBuffer(Command cmd, int len, uint8_t *cbuf)
{
    int rc;
    int retryCount = 2;
    Command c;
    do
    {
        rc = write(cmd);
        if (rc == 0)
            rc = readBuf(c, len, cbuf);
        else
            reconnect();
    }
    while (rc != 0 && --retryCount);
    return rc;
}

int ScopeDomeUSB21::writeCmd(Command cmd)
{
    int rc = write(cmd);
    if (rc != 0)
    {
        reconnect();
        return rc;
    }
    return read(cmd);
}

int ScopeDomeUSB21::writeU8(Command cmd, uint8_t value)
{
    int rc = writeBuf(cmd, 1, &value);
    if (rc != 0)
    {
        reconnect();
        return rc;
    }
    return read(cmd);
}

int ScopeDomeUSB21::writeU16(Command cmd, uint16_t value)
{
    int rc = writeBuf(cmd, 2, (uint8_t *)&value);
    if (rc != 0)
    {
        reconnect();
        return rc;
    }
    return read(cmd);
}

int ScopeDomeUSB21::writeU32(Command cmd, uint32_t value)
{
    int rc = writeBuf(cmd, 4, (uint8_t *)&value);
    if (rc != 0)
    {
        reconnect();
        return rc;
    }
    return read(cmd);
}

int ScopeDomeUSB21::writeBuffer(Command cmd, int len, uint8_t *cbuf)
{
    int rc = writeBuf(cmd, len, cbuf);
    if (rc != 0)
    {
        reconnect();
        return rc;
    }
    return read(cmd);
}

void ScopeDomeUSB21::reconnect()
{
    parent->reconnect();
}

int ScopeDomeUSB21::updateState()
{
    int rc = readU16(GetStatus, status);
    rc |= readS16(GetCounter, counter);
    rc |= readBuffer(GetAllDigitalExt, 5, digitalSensorState);
    return rc;
}

uint32_t ScopeDomeUSB21::getStatus()
{
    return status;
}

void ScopeDomeUSB21::getFirmwareVersions(double &main, double &rotary)
{
    uint16_t fwVersion = 0;
    readU16(GetVersionFirmware, fwVersion);
    main = fwVersion / 100.0;

    uint8_t fwVersionRotary = 0;
    readU8(GetVersionFirmwareRotary, fwVersionRotary);
    rotary = (fwVersionRotary + 9) / 10.0;
}

uint32_t ScopeDomeUSB21::getStepsPerRevolution()
{
    uint32_t stepsPerRevolution = 0;
    readU32(GetImpPerTurn, stepsPerRevolution);
    return stepsPerRevolution;
}

// Abstract versions
ISState ScopeDomeUSB21::getInputState(AbstractInput input)
{
    DigitalIO channel;
    switch(input)
    {
        case HOME:
            channel = IN_HOME;
            break;
        case OPEN1:
            channel = IN_OPEN1;
            break;
        case CLOSED1:
            channel = IN_CLOSED1;
            break;
        case OPEN2:
            channel = IN_OPEN2;
            break;
        case CLOSED2:
            channel = IN_CLOSED1;
            break;
        case ROTARY_LINK:
            channel = IN_ROT_LINK;
            break;
        default:
            LOG_ERROR("invalid input");
            return ISS_OFF;
    }
    return getInputState(channel);
}

int ScopeDomeUSB21::setOutputState(AbstractOutput output, ISState onOff)
{
    DigitalIO channel;
    switch(output)
    {
        case RESET:
            channel = OUT_RELAY1;
            break;
        case CW:
            channel = OUT_CW;
            break;
        case CCW:
            channel = OUT_CCW;
            break;
        default:
            LOG_ERROR("invalid output");
            return ISS_OFF;
    }
    return setOutputState(channel, onOff);
}

// Internal versions
ISState ScopeDomeUSB21::getInputState(DigitalIO channel)
{
    int ch      = (int)channel;
    int byte    = ch >> 3;
    uint8_t bit = 1 << (ch & 7);
    return (digitalSensorState[byte] & bit) ? ISS_ON : ISS_OFF;
}

int ScopeDomeUSB21::setOutputState(DigitalIO channel, ISState onOff)
{
    return writeU8(onOff == ISS_ON ? SetDigitalChannel : ClearDigitalChannel, (uint8_t)channel);
}

int ScopeDomeUSB21::getRotationCounter()
{
    return counter;
}

int ScopeDomeUSB21::getRotationCounterExt()
{
    readS32(GetCounterExt, counterExt);
    return counterExt;
}

bool ScopeDomeUSB21::isCalibrationNeeded()
{
    uint8_t calibrationNeeded = false;
    readU8(IsFullSystemCalReq, calibrationNeeded);
    return calibrationNeeded != 0;
}

void ScopeDomeUSB21::abort()
{
    writeCmd(Stop);
}

void ScopeDomeUSB21::calibrate()
{
    writeCmd(FullSystemCal);
}

void ScopeDomeUSB21::findHome()
{
    writeCmd(FindHome);
}

void ScopeDomeUSB21::controlShutter(ShutterOperation operation)
{
    switch(operation)
    {
        case OPEN_SHUTTER:
            setOutputState(OUT_CLOSE1, ISS_OFF);
            setOutputState(OUT_OPEN1, ISS_ON);
            break;
        case CLOSE_SHUTTER:
            setOutputState(OUT_CLOSE1, ISS_ON);
            setOutputState(OUT_OPEN1, ISS_OFF);
            break;
        case STOP_SHUTTER:
            setOutputState(OUT_CLOSE1, ISS_OFF);
            setOutputState(OUT_OPEN1, ISS_OFF);
            break;
    }
}

void ScopeDomeUSB21::resetCounter()
{
    writeCmd(ResetCounter);
    writeCmd(ResetCounterExt);
}

void ScopeDomeUSB21::move(int steps)
{
    if(steps < 0)
    {
        writeU16(CCWRotation, -steps);
    }
    else
    {
        writeU16(CWRotation, steps);
    }
}

size_t ScopeDomeUSB21::getNumberOfSensors()
{
    return 11;
}

ScopeDomeCard::SensorInfo ScopeDomeUSB21::getSensorInfo(size_t index)
{
    ScopeDomeCard::SensorInfo info;
    switch(index)
    {
        case 0:
            info.propName = "LINK_STRENGTH";
            info.label = "Shutter link strength";
            info.format = "%3.0f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 1:
            info.propName = "SHUTTER_POWER";
            info.label = "Shutter internal power";
            info.format = "%2.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 2:
            info.propName = "SHUTTER_BATTERY";
            info.label = "Shutter battery power";
            info.format = "%2.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 3:
            info.propName = "CARD_POWER";
            info.label = "Card internal power";
            info.format = "%2.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 4:
            info.propName = "CARD_BATTERY";
            info.label = "Card battery power";
            info.format = "%2.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 5:
            info.propName = "TEMP_DOME_IN";
            info.label = "Temperature in dome";
            info.format = "%2.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 6:
            info.propName = "TEMP_DOME_OUT";
            info.label = "Temperature outside dome";
            info.format = "%2.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 7:
            info.propName = "TEMP_DOME_HUMIDITY";
            info.label = "Temperature humidity sensor";
            info.format = "%2.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        case 8:
            info.propName = "HUMIDITY";
            info.label = "Humidity";
            info.format = "%3.2f";
            info.minValue = 0;
            info.maxValue = 100;
            break;
        case 9:
            info.propName = "PRESSURE";
            info.label = "Pressure";
            info.format = "%4.1f";
            info.minValue = 0;
            info.maxValue = 2000;
            break;
        case 10:
            info.propName = "DEW_POINT";
            info.label = "Dew point";
            info.format = "%2.2f";
            info.minValue = -100;
            info.maxValue = 100;
            break;
        default:
            LOG_ERROR("invalid sensor index");
            break;
    }
    return info;
}

double ScopeDomeUSB21::getSensorValue(size_t index)
{
    double value = 0;

    switch(index)
    {
        case 0:
        {
            readU8(GetLinkStrength, linkStrength);
            value = linkStrength;

            // My shutter unit occasionally disconnects so implement a simple watchdog
            // to check for link strength and reset the controller if link is lost for
            // more than 5 polling cycles
            static int count = 0;
            if (linkStrength == 0)
            {
                if (++count > 5)
                {
                    // Issue reset
                    setOutputState(ScopeDomeCard::RESET, ISS_ON);
                    count = 0;
                }
            }
            else
            {
                count = 0;
            }
            break;
        }
        case 1:
            readFloat(GetAnalog1, sensors[0]);
            value = sensors[0];
            break;
        case 2:
            readFloat(GetAnalog2, sensors[1]);
            value = sensors[1];
            break;
        case 3:
            readFloat(GetMainAnalog1, sensors[2]);
            value = sensors[2];
            break;
        case 4:
            readFloat(GetMainAnalog2, sensors[3]);
            value = sensors[3];
            break;
        case 5:
            readFloat(GetTempIn, sensors[4]);
            value = sensors[4];
            break;
        case 6:
            readFloat(GetTempOut, sensors[5]);
            value = sensors[5];
            break;
        case 7:
            readFloat(GetTempHum, sensors[6]);
            value = sensors[6];
            break;
        case 8:
            readFloat(GetHum, sensors[7]);
            value = sensors[7];
            break;
        case 9:
            readFloat(GetPressure, sensors[8]);
            value = sensors[8];
            break;
        case 10:
            value = parent->getDewPoint(sensors[7], sensors[6]);
            break;
        default:
            LOG_ERROR("invalid sensor index");
            break;
    }
    return value;
}

size_t ScopeDomeUSB21::getNumberOfRelays()
{
    return 8;
}

ScopeDomeCard::RelayInfo ScopeDomeUSB21::getRelayInfo(size_t index)
{
    ScopeDomeCard::RelayInfo info;
    switch(index)
    {
        case 0:
            info.propName = "CCD";
            info.label = "CCD";
            break;
        case 1:
            info.propName = "SCOPE";
            info.label = "Telescope";
            break;
        case 2:
            info.propName = "LIGHT";
            info.label = "Light";
            break;
        case 3:
            info.propName = "FAN";
            info.label = "Fan";
            break;
        case 4:
            info.propName = "RELAY_1";
            info.label = "Relay 1 (reset)";
            break;
        case 5:
            info.propName = "RELAY_2";
            info.label = "Relay 2 (heater)";
            break;
        case 6:
            info.propName = "RELAY_3";
            info.label = "Relay 3";
            break;
        case 7:
            info.propName = "RELAY_4";
            info.label = "Relay 4";
            break;
        default:
            LOG_ERROR("invalid relay index");
            break;
    }
    return info;
}

ISState ScopeDomeUSB21::getRelayState(size_t index)
{
    switch(index)
    {
        case 0:
            return getInputState(OUT_CCD);
        case 1:
            return getInputState(OUT_SCOPE);
        case 2:
            return getInputState(OUT_LIGHT);
        case 3:
            return getInputState(OUT_FAN);
        case 4:
            return getInputState(OUT_RELAY1);
        case 5:
            return getInputState(OUT_RELAY2);
        case 6:
            return getInputState(OUT_RELAY3);
        case 7:
            return getInputState(OUT_RELAY4);
        default:
            LOG_ERROR("invalid relay index");
            break;
    }
    return ISS_OFF;
}

void ScopeDomeUSB21::setRelayState(size_t index, ISState state)
{
    switch(index)
    {
        case 0:
            setOutputState(OUT_CCD, state);
            break;
        case 1:
            setOutputState(OUT_SCOPE, state);
            break;
        case 2:
            setOutputState(OUT_LIGHT, state);
            break;
        case 3:
            setOutputState(OUT_FAN, state);
            break;
        case 4:
            setOutputState(OUT_RELAY1, state);
            break;
        case 5:
            setOutputState(OUT_RELAY2, state);
            break;
        case 6:
            setOutputState(OUT_RELAY3, state);
            break;
        case 7:
            setOutputState(OUT_RELAY4, state);
            break;
        default:
            LOG_ERROR("invalid relay index");
            break;
    }
}

size_t ScopeDomeUSB21::getNumberOfInputs()
{
    return 12;
}

ScopeDomeCard::InputInfo ScopeDomeUSB21::getInputInfo(size_t index)
{
    ScopeDomeCard::InputInfo info;

    switch(index)
    {
        case 0:
            info.propName = "AZ_COUNTER";
            info.label = "Az counter";
            break;
        case 1:
            info.propName = "HOME";
            info.label = "Dome at home";
            break;
        case 2:
            info.propName = "OPEN_1";
            info.label = "Shutter 1 open";
            break;
        case 3:
            info.propName = "CLOSE_1";
            info.label = "Shutter 1 closed";
            break;
        case 4:
            info.propName = "OPEN_2";
            info.label = "Shutter 2 open";
            break;
        case 5:
            info.propName = "CLOSE_2";
            info.label = "Shutter 2 closed";
            break;
        case 6:
            info.propName = "SCOPE_HOME";
            info.label = "Scope at home";
            break;
        case 7:
            info.propName = "RAIN";
            info.label = "Rain sensor";
            break;
        case 8:
            info.propName = "CLOUD";
            info.label = "Cloud sensor";
            break;
        case 9:
            info.propName = "SAFE";
            info.label = "Observatory safe";
            break;
        case 10:
            info.propName = "LINK";
            info.label = "Rotary link";
            break;
        case 11:
            info.propName = "FREE";
            info.label = "Free input";
            break;
        default:
            LOG_ERROR("invalid input index");
            break;
    }
    return info;
}

ISState ScopeDomeUSB21::getInputValue(size_t index)
{
    switch(index)
    {
        case 0:
            return getInputState(IN_ENCODER);
        case 1:
            return getInputState(IN_HOME);
        case 2:
            return getInputState(IN_OPEN1);
        case 3:
            return getInputState(IN_CLOSED1);
        case 4:
            return getInputState(IN_OPEN2);
        case 5:
            return getInputState(IN_CLOSED2);
        case 6:
            return getInputState(IN_S_HOME);
        case 7:
            return getInputState(IN_CLOUDS);
        case 8:
            return getInputState(IN_CLOUD);
        case 9:
            return getInputState(IN_SAFE);
        case 10:
            return getInputState(IN_ROT_LINK);
        case 11:
            return getInputState(IN_FREE);
        default:
            LOG_ERROR("invalid input index");
            break;
    }
    return ISS_OFF;
}

void ScopeDomeUSB21::setHomeSensorPolarity(HomeSensorPolarity polarity)
{
    uint8_t negate = 0;
    switch(polarity)
    {
        case ACTIVE_HIGH:
            negate = 0;
            break;
        case ACTIVE_LOW:
            negate = 1;
            break;
    }

    writeU8(NegHomeSensorActiveState, negate);
}
