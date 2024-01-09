/*
    PlaneWave EFA Protocol

    Hendrick Focuser

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

#include "planewave_efa.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <regex>
#include <sys/ioctl.h>

static std::unique_ptr<EFA> steelDrive(new EFA());

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
EFA::EFA()
{
    setVersion(1, 2);

    // Focuser Capabilities
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_REVERSE |
                      FOCUSER_CAN_SYNC);
}

bool EFA::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser Information
    IUFillText(&InfoT[INFO_VERSION], "INFO_VERSION", "Version", "NA");
    IUFillTextVector(&InfoTP, InfoT, 1, getDeviceName(), "INFO", "Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[TEMPERATURE_PRIMARY], "TEMPERATURE_PRIMARY", "Primary (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumber(&TemperatureN[TEMPERATURE_AMBIENT], "TEMPERATURE_AMBIENT", "Ambient (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 2, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Fan Control
    IUFillSwitch(&FanStateS[FAN_ON], "FAN_ON", "On", ISS_OFF);
    IUFillSwitch(&FanStateS[FAN_OFF], "FAN_OFF", "Off", ISS_ON);
    IUFillSwitchVector(&FanStateSP, FanStateS, 2, getDeviceName(), "FOCUS_FAN", "Fans", FAN_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Fan Control Mode
    IUFillSwitch(&FanControlS[FAN_MANUAL], "FAN_MANUAL", "Manual", ISS_ON);
    IUFillSwitch(&FanControlS[FAN_AUTOMATIC_ABSOLUTE], "FAN_AUTOMATIC_ABSOLUTE", "Auto. Absolute", ISS_OFF);
    IUFillSwitch(&FanControlS[FAN_AUTOMATIC_RELATIVE], "FAN_AUTOMATIC_RELATIVE", "Auto. Relative", ISS_OFF);
    IUFillSwitchVector(&FanControlSP, FanControlS, 3, getDeviceName(), "FOCUS_FAN_CONTROL", "Control Mode", FAN_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Fan Control Parameters
    IUFillNumber(&FanControlN[FAN_MAX_ABSOLUTE], "FAN_MAX_ABSOLUTE", "Max Primary (c)", "%.2f", 0, 50., 5., 25.);
    IUFillNumber(&FanControlN[FAN_MAX_RELATIVE], "FAN_MAX_RELATIVE", "Max Relative (c)", "%.2f", 0., 30., 1., 2.5);
    IUFillNumber(&FanControlN[FAN_DEADZONE], "FAN_DEADZONE", "Deadzone (c)", "%.2f", 0.1, 10, 0.5, 0.5);
    IUFillNumberVector(&FanControlNP, FanControlN, 3, getDeviceName(), "FOCUS_FAN_PARAMS", "Control Params",
                       FAN_TAB, IP_RW, 0, IPS_IDLE);

    // Fan Off on Disconnect
    IUFillSwitch(&FanDisconnectS[FAN_OFF_ON_DISCONNECT], "FAN_OFF_ON_DISCONNECT", "Switch Off", ISS_ON);
    IUFillSwitchVector(&FanDisconnectSP, FanDisconnectS, 1, getDeviceName(), "FOCUS_FAN_DISCONNECT", "On Disconnect", FAN_TAB,
                       IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    // Calibration Control
    IUFillSwitch(&CalibrationStateS[CALIBRATION_ON], "CALIBRATION_ON", "Calibrated", ISS_OFF);
    IUFillSwitch(&CalibrationStateS[CALIBRATION_OFF], "CALIBRATION_OFF", "Not Calibrated", ISS_ON);
    IUFillSwitchVector(&CalibrationStateSP, CalibrationStateS, 2, getDeviceName(), "FOCUS_CALIBRATION", "Calibration",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Setup limits
    FocusMaxPosN[0].value = 1e7;
    FocusMaxPosN[0].max   = 1e7;
    FocusMaxPosN[0].step  = FocusMaxPosN[0].max / 50;

    FocusAbsPosN[0].max   = 1e7;
    FocusAbsPosN[0].step  = FocusAbsPosN[0].max / 50;

    FocusSyncN[0].max     = 1e7;
    FocusSyncN[0].step    = FocusSyncN[0].max / 50;

    FocusRelPosN[0].max   = FocusAbsPosN[0].max / 2;
    FocusRelPosN[0].step  = FocusRelPosN[0].max / 50;

    addAuxControls();
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    setDefaultPollingPeriod(500);

    // Lower RTS so serial port not monopolized and hand controller can work
    int bits = TIOCM_RTS;
    (void) ioctl(PortFD, TIOCMBIC, &bits);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool EFA::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        getStartupValues();

        defineProperty(&InfoTP);
        defineProperty(&CalibrationStateSP);

        // Fan
        defineProperty(&FanStateSP);
        defineProperty(&FanControlSP);
        loadConfig(true, FanControlSP.name);
        defineProperty(&FanDisconnectSP);

        defineProperty(&TemperatureNP);
    }
    else
    {
        deleteProperty(InfoTP.name);
        deleteProperty(CalibrationStateSP.name);

        deleteProperty(FanStateSP.name);
        deleteProperty(FanControlSP.name);
        deleteProperty(FanControlNP.name);
        deleteProperty(FanDisconnectSP.name);

        deleteProperty(TemperatureNP.name);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool EFA::Disconnect()
{
    if (FanDisconnectS[FAN_OFF_ON_DISCONNECT].s == ISS_ON)
        setFanEnabled(false);

    return INDI::Focuser::Disconnect();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool EFA::Handshake()
{
    std::string version;

    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 6;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = GET_VERSION;
    cmd[5] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return false;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return false;

    version = std::to_string(res[5]) + "." + std::to_string(res[6]);
    IUSaveText(&InfoT[INFO_VERSION], version.c_str());

    LOGF_INFO("Detected version %s", version.c_str());

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *EFA::getDefaultName()
{
    return "PlaneWave EFA";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool EFA::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Calibration State
        if (!strcmp(CalibrationStateSP.name, name))
        {
            bool enabled = strcmp(CalibrationStateS[CALIBRATION_ON].name, IUFindOnSwitchName(states, names, n)) == 0;
            if (setCalibrationEnabled(enabled))
            {
                IUUpdateSwitch(&CalibrationStateSP, states, names, n);
                CalibrationStateSP.s = IPS_OK;
            }
            else
            {
                CalibrationStateSP.s = IPS_ALERT;
            }

            IDSetSwitch(&CalibrationStateSP, nullptr);
            return true;
        }
        // Fan State
        else if (!strcmp(FanStateSP.name, name))
        {
            if (FanControlS[FAN_MANUAL].s == ISS_OFF)
            {
                FanStateSP.s = IPS_IDLE;
                LOG_WARN("Cannot control fan while manual control is turned off.");
                IDSetSwitch(&FanControlSP, nullptr);
                return true;
            }

            bool enabled = strcmp(FanStateS[FAN_ON].name, IUFindOnSwitchName(states, names, n)) == 0;
            if (setFanEnabled(enabled))
            {
                IUUpdateSwitch(&FanStateSP, states, names, n);
                FanStateSP.s = enabled ? IPS_OK : IPS_IDLE;
            }
            else
            {
                FanStateSP.s = IPS_ALERT;
            }

            IDSetSwitch(&FanStateSP, nullptr);
            return true;
        }
        // Fan Control
        else if (!strcmp(FanControlSP.name, name))
        {
            IUUpdateSwitch(&FanControlSP, states, names, n);
            if (FanControlS[FAN_MANUAL].s == ISS_ON)
            {
                deleteProperty(FanControlNP.name);
                LOG_INFO("Fan is now controlled manually.");
            }
            else
            {
                LOG_INFO("Fan is now controlled automatically per the control parameters.");
                defineProperty(&FanControlNP);
            }

            FanControlSP.s = IPS_OK;
            IDSetSwitch(&FanControlSP, nullptr);
            return true;
        }
        // Fan Disconnect
        else if (!strcmp(FanDisconnectSP.name, name))
        {
            IUUpdateSwitch(&FanDisconnectSP, states, names, n);

            if (FanDisconnectS[FAN_OFF_ON_DISCONNECT].s == ISS_ON)
                LOG_INFO("Fan shall be turned off upon device disconnection.");
            else
                LOG_INFO("Fan shall left as-is upon device disconnection.");

            FanDisconnectSP.s = IPS_OK;
            IDSetSwitch(&FanDisconnectSP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool EFA::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Fan Params
        if (!strcmp(FanControlNP.name, name))
        {
            IUUpdateNumber(&FanControlNP, values, names, n);
            FanControlNP.s = IPS_OK;
            IDSetNumber(&FanControlNP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// Sync focuser
/////////////////////////////////////////////////////////////////////////////
bool EFA::SyncFocuser(uint32_t ticks)
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 9;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x06;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_OFFSET_CNT;
    cmd[5] = (ticks >> 16) & 0xFF;
    cmd[6] = (ticks >>  8) & 0xFF;
    cmd[7] = (ticks >>  0) & 0xFF;
    cmd[8] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return false;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return false;

    return (res[5] == 1);
}

/////////////////////////////////////////////////////////////////////////////
/// Move Absolute Focuser
/////////////////////////////////////////////////////////////////////////////
IPState EFA::MoveAbsFocuser(uint32_t targetTicks)
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 9;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x06;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GOTO_POS2;
    cmd[5] = (targetTicks >> 16) & 0xFF;
    cmd[6] = (targetTicks >>  8) & 0xFF;
    cmd[7] = (targetTicks >>  0) & 0xFF;
    cmd[8] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return IPS_ALERT;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return IPS_ALERT;

    return (res[5] == 1) ? IPS_BUSY : IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////////////
/// Move Focuser relatively
/////////////////////////////////////////////////////////////////////////////
IPState EFA::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int direction = (dir == FOCUS_INWARD) ? -1 : 1;
    int reversed = (FocusReverseS[INDI_ENABLED].s == ISS_ON) ? -1 : 1;
    int relative = static_cast<int>(ticks);
    int targetAbsPosition = FocusAbsPosN[0].value + (relative * direction * reversed);

    targetAbsPosition = std::min(static_cast<uint32_t>(FocusAbsPosN[0].max),
                                 static_cast<uint32_t>(std::max(static_cast<int>(FocusAbsPosN[0].min), targetAbsPosition)));

    return MoveAbsFocuser(targetAbsPosition);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void EFA::TimerHit()
{
    if (!isConnected())
        return;

    IN_TIMER = true;

    readPosition();

    if (readTemperature())
    {
        // Send temperature is above threshold
        bool aboveThreshold = false;
        for (int i = 0; i < TemperatureNP.nnp; i++)
        {
            if (std::fabs(TemperatureN[i].value - m_LastTemperature[i]) > TEMPERATURE_THRESHOLD)
            {
                aboveThreshold = true;
                m_LastTemperature[i] = TemperatureN[i].value;
            }
        }

        if (aboveThreshold)
            IDSetNumber(&TemperatureNP, nullptr);


        if (FanControlS[FAN_MANUAL].s == ISS_OFF)
        {
            bool turnOn = false, turnOff = false;
            const bool isFanOn = FanStateS[FAN_ON].s == ISS_ON;

            // Check if we need to do automatic regulation of fan
            if (FanControlS[FAN_AUTOMATIC_ABSOLUTE].s == ISS_ON)
            {
                // Adjust delta for deadzone
                double min_delta = FanControlN[FAN_MAX_ABSOLUTE].value - FanControlN[FAN_DEADZONE].value;
                double max_delta = FanControlN[FAN_MAX_ABSOLUTE].value + FanControlN[FAN_DEADZONE].value;

                turnOn = TemperatureN[TEMPERATURE_PRIMARY].value > max_delta;
                turnOff = TemperatureN[TEMPERATURE_PRIMARY].value < min_delta;
            }
            else if (FanControlS[FAN_AUTOMATIC_RELATIVE].s == ISS_ON)
            {
                // Temperature delta
                double tDiff = TemperatureN[TEMPERATURE_PRIMARY].value - TemperatureN[TEMPERATURE_AMBIENT].value;
                // Adjust delta for deadzone
                double min_delta = FanControlN[FAN_MAX_RELATIVE].value - FanControlN[FAN_DEADZONE].value;
                double max_delta = FanControlN[FAN_MAX_RELATIVE].value + FanControlN[FAN_DEADZONE].value;

                // Check if we need to turn off/on fan
                turnOn = tDiff > max_delta;
                turnOff = tDiff < min_delta;
            }

            if (isFanOn && turnOff)
            {
                setFanEnabled(false);
                FanStateS[FAN_ON].s = ISS_OFF;
                FanStateS[FAN_OFF].s = ISS_ON;
                FanStateSP.s = IPS_IDLE;
                IDSetSwitch(&FanStateSP, nullptr);
            }
            else if (!isFanOn && turnOn)
            {
                setFanEnabled(true);
                FanStateS[FAN_ON].s = ISS_ON;
                FanStateS[FAN_OFF].s = ISS_OFF;
                FanStateSP.s = IPS_OK;
                IDSetSwitch(&FanStateSP, nullptr);
            }
        }
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (isGOTOComplete())
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
            LOG_INFO("Focuser reached requested position.");
        }
        else
            IDSetNumber(&FocusAbsPosNP, nullptr);
    }
    else if (std::fabs(FocusAbsPosN[0].value - m_LastPosition) > 0)
    {
        m_LastPosition = FocusAbsPosN[0].value;
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    IN_TIMER = false;

    SetTimer(getCurrentPollingPeriod());
}


/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool EFA::AbortFocuser()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Set Maximum Position
/////////////////////////////////////////////////////////////////////////////
bool EFA::SetFocuserMaxPosition(uint32_t ticks)
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 9;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x06;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_SLEWLIMITMAX;
    cmd[5] = (ticks >> 16) & 0xFF;
    cmd[6] = (ticks >>  8) & 0xFF;
    cmd[7] = (ticks >>  0) & 0xFF;
    cmd[8] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return false;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return false;

    return (res[5] == 1);
}

/////////////////////////////////////////////////////////////////////////////
/// Reverse Focuser Motion
/////////////////////////////////////////////////////////////////////////////
bool EFA::ReverseFocuser(bool enabled)
{
    INDI_UNUSED(enabled);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool EFA::saveConfigItems(FILE * fp)
{
    INDI::Focuser::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &FanControlSP);
    if (FanControlNP.s == IPS_OK)
        IUSaveConfigNumber(fp, &FanControlNP);
    IUSaveConfigSwitch(fp, &FanDisconnectSP);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get Startup values
/////////////////////////////////////////////////////////////////////////////
void EFA::getStartupValues()
{
    readPosition();
    readCalibrationState();
    readFanState();
    readTemperature();

    if (readMaxSlewLimit())
    {
        FocusAbsPosN[0].max = FocusMaxPosN[0].max;
        FocusAbsPosN[0].step = FocusAbsPosN[0].max / 50;

        FocusRelPosN[0].value = FocusAbsPosN[0].max / 50;
        FocusRelPosN[0].max = FocusAbsPosN[0].max / 2;
        FocusRelPosN[0].step = FocusRelPosN[0].max / 50;

        FocusMaxPosN[0].value = FocusMaxPosN[0].max;
        FocusMaxPosN[0].step = FocusMaxPosN[0].max / 50;

        IUUpdateMinMax(&FocusRelPosNP);
        IUUpdateMinMax(&FocusAbsPosNP);
        IUUpdateMinMax(&FocusMaxPosNP);
    }
}

/////////////////////////////////////////////////////////////////////////////
/// Read Byte (wrapper for tty_read)
/////////////////////////////////////////////////////////////////////////////
inline int EFA::readByte(int fd, uint8_t *buf, int timeout, int *nbytes_read)
{
    return tty_read(fd, (char *)buf, 1, timeout, nbytes_read);
}

///////////////////////////////////////////////////////////////////////////////
/// Read Bytes (wrapper for tty_read)
///////////////////////////////////////////////////////////////////////////////
inline int EFA::readBytes(int fd, uint8_t *buf, int nbytes, int timeout, int *nbytes_read)
{
    return tty_read(fd, (char *)buf, nbytes, timeout, nbytes_read);
}

/////////////////////////////////////////////////////////////////////////////
/// Write Bytes (wrapper for tty_write)
/////////////////////////////////////////////////////////////////////////////
inline int EFA::writeBytes(int fd, const uint8_t *buf, int nbytes, int *nbytes_written)
{
    return tty_write(fd, (const char *)buf, nbytes, nbytes_written);
}

/////////////////////////////////////////////////////////////////////////////
/// Read Packet
/////////////////////////////////////////////////////////////////////////////
int EFA::readPacket(int fd, uint8_t *buf, int nbytes, int timeout, int *nbytes_read)
{
    int len = 0, rc = 0, read_bytes = *nbytes_read = 0;

    if (nbytes < 6)   // smallest packet is 6 bytes
    {
        LOGF_ERROR("Read needs at least 6 bytes; exceeds supplied buffer size (%d)", nbytes);
        return TTY_READ_ERROR;
    }

    for (int i = 0; i < 10; i++)
    {
        // look for SOM byte
        rc = readByte(fd, &buf[0], timeout, &read_bytes);
        if (rc == TTY_OK && read_bytes == 1)
        {
            if (buf[0] == DRIVER_SOM)
            {
                break;
            }
            else
            {
                LOGF_DEBUG("Looking for SOM (%02X); found %d byte (%02X)", DRIVER_SOM, read_bytes, buf[0]);
            }
        }
        else
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_DEBUG("Looking for SOM (%02X); found %s", DRIVER_SOM, errstr);
        }
    }
    if (rc != TTY_OK || read_bytes != 1 || buf[0] != DRIVER_SOM)
    {
        LOGF_DEBUG("%s byte not encountered", "SOM");
        if (rc == TTY_OK)
            return TTY_TIME_OUT;
        return rc;
    }

    if ((rc = readByte(fd, &buf[1], timeout, &read_bytes)) != TTY_OK)
    {
        LOGF_DEBUG("%s byte not encountered", "LEN");
        return rc;
    }
    len = buf[1];

    // read source
    if ((rc = readByte(fd, &buf[2], timeout, &read_bytes)) != TTY_OK)
    {
        LOGF_DEBUG("%s byte not encountered", "SRC");
        return rc;
    }
    // read receiver
    if ((rc = readByte(fd, &buf[3], timeout, &read_bytes)) != TTY_OK)
    {
        LOGF_DEBUG("%s byte not encountered", "RCV");
        return rc;
    }
    // read command
    if ((rc = readByte(fd, &buf[4], timeout, &read_bytes)) != TTY_OK)
    {
        LOGF_DEBUG("%s byte not encountered", "CMD");
        return rc;
    }

    if ((len + 3) > nbytes)
    {
        LOGF_ERROR("Read (%d) will exceed supplied buffer size (%d) for command %02X", (len + 3), nbytes, buf[4]);
        return TTY_READ_ERROR;
    }

    // read data
    int n;
    for (n = 0; n < (len - 3); ++n)
    {
        if ((rc = readByte(fd, &buf[5 + n], timeout, &read_bytes)) != TTY_OK)
        {
            LOGF_DEBUG("%s byte not encountered", "DATA");
            return rc;
        }
    }
    // read checksum
    if ((rc = readByte(fd, &buf[5 + n], timeout, &read_bytes)) != TTY_OK)
    {
        LOGF_DEBUG("%s byte not encountered", "DATA");
        return rc;
    }

    uint8_t chk = calculateCheckSum(buf, (len + 3));

    if (chk != buf[len + 2])
    {
        LOG_ERROR("Invalid checksum!");
        return TTY_OK; // not a tty error, nbytes_read is still zero and it is used to indicate there was this problem
    }

    *nbytes_read = len + 3;

    return rc;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool EFA::sendCommand(const uint8_t * cmd, uint8_t *res, uint32_t cmd_len, uint32_t res_len)
{
    int nbytes_written = 0, nbytes_read = 0, bits = 0, rc = 0, hexbuflen = (DRIVER_LEN * 3 + 4);
    char hexbuf[DRIVER_LEN * 3 + 4];

    for (int j = 0; j < 3; usleep(100000), j++)
    {
        // make sure RTS is lowered
        bits = TIOCM_RTS;
        (void) ioctl(PortFD, TIOCMBIC, &bits);
        bits = 0;

        // Wait until CTS is cleared.
        for (int i = 0; i < 10; i++)
        {
            if ((rc = ioctl(PortFD, TIOCMGET, &bits)) == 0 && (bits & TIOCM_CTS) == 0)
                break;
            usleep(100000);
        }

        if (rc < 0 || (bits & TIOCM_CTS) != 0)
        {
            LOGF_ERROR("CTS timed out: %s", strerror(errno));
            return false;
        }

        // Now raise RTS
        bits = TIOCM_RTS;
        ioctl(PortFD, TIOCMBIS, &bits); // was TIOCMSET

        if (!IN_TIMER && efaDump(hexbuf, hexbuflen, cmd, cmd_len) != NULL)
        {
            LOGF_DEBUG("CMD: %s", hexbuf);
        }
        rc = writeBytes(PortFD, cmd, cmd_len, &nbytes_written);

        if (rc != TTY_OK)
        {
            continue;
        }

        rc = readPacket(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);

        if (rc != TTY_OK || nbytes_read == 0)
        {
            continue;
        }

        if ((int)cmd_len == nbytes_read && memcmp(cmd, res, cmd_len) == 0)
        {
            // received an echo

            bits = TIOCM_RTS;
            ioctl(PortFD, TIOCMBIC, &bits);

            // Next read the actual response from EFA
            rc = readPacket(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);

            if (rc != TTY_OK || nbytes_read == 0)
            {
                continue;
            }
        }
        else if (efaDump(hexbuf, hexbuflen, cmd, cmd_len) != NULL)
        {
            // expected there to always be an echo, so note this occurrence
            LOGF_DEBUG("no echo for command packet: %s", hexbuf);
        }

        if (!IN_TIMER && efaDump(hexbuf, hexbuflen, res, nbytes_read) != NULL)
        {
            LOGF_DEBUG("RESP: %s", hexbuf);
        }

        if (cmd[2] != res[3] || cmd[3] != res[2] || cmd[4] != res[4])
        {
            LOGF_DEBUG("Send/Receive mismatch - %s!", "packet not for us");
            continue;
        }
        break;
    }

    // Extra lowering of RTS to make sure hand controller is made available
    bits = TIOCM_RTS;
    ioctl(PortFD, TIOCMBIC, &bits);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial I/O error: %s.", errstr);
        return false;
    }

    if (cmd[2] != res[3] || cmd[3] != res[2] || cmd[4] != res[4])
    {
        LOGF_ERROR("Send/Receive mismatch and %s", "timeout");
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Read Position
/////////////////////////////////////////////////////////////////////////////
bool EFA::readPosition()
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 6;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GET_POS;
    cmd[5] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return false;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return false;

    FocusAbsPosN[0].value = res[5] << 16 | res[6] << 8 | res[7];
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Read Max Slew Limit
/////////////////////////////////////////////////////////////////////////////
bool EFA::readMaxSlewLimit()
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 6;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_SLEWLIMITGETMAX;
    cmd[5] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return false;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return false;

    uint32_t limit = res[5] << 16 | res[6] << 8 | res[7];
    if (limit > 0)
    {
        FocusMaxPosN[0].max = limit;
        return true;
    }

    return false;

}

/////////////////////////////////////////////////////////////////////////////
/// Is Slew over?
/////////////////////////////////////////////////////////////////////////////
bool EFA::isGOTOComplete()
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 6;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GOTO_OVER;
    cmd[5] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return false;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return false;

    return (res[5] != 0);
}

/////////////////////////////////////////////////////////////////////////////
/// Set Fan Enabled/Disabled
/////////////////////////////////////////////////////////////////////////////
bool EFA::setFanEnabled(bool enabled)
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 7;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x04;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FAN;
    cmd[4] = FANS_SET;
    cmd[5] = enabled ? 1 : 0;
    cmd[6] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return false;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return false;

    return (res[5] == 1);
}

/////////////////////////////////////////////////////////////////////////////
/// Get Fan State
/////////////////////////////////////////////////////////////////////////////
bool EFA::readFanState()
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 6;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FAN;
    cmd[4] = FANS_GET;
    cmd[5] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return false;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return false;

    bool enabled = (res[5] == 0);

    FanStateS[FAN_ON].s  = enabled ? ISS_ON : ISS_OFF;
    FanStateS[FAN_OFF].s = enabled ? ISS_OFF : ISS_ON;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Set Calibration Enabled/Disabled
/////////////////////////////////////////////////////////////////////////////
bool EFA::setCalibrationEnabled(bool enabled)
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 8;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x05;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_SET_CALIBRATION_STATE;
    cmd[5] = 0x40;
    cmd[6] = enabled ? 1 : 0;
    cmd[7] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return false;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return false;

    return (res[5] == 1);
}

/////////////////////////////////////////////////////////////////////////////
/// Get Calibration State
/////////////////////////////////////////////////////////////////////////////
bool EFA::readCalibrationState()
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 7;

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x04;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GET_CALIBRATION_STATE;
    cmd[5] = 0x40;
    cmd[6] = calculateCheckSum(cmd, len);

    if (!validateLengths(cmd, len))
        return false;

    if (!sendCommand(cmd, res, len, DRIVER_LEN))
        return false;

    bool enabled = (res[5] == 1);

    CalibrationStateS[CALIBRATION_ON].s  = enabled ? ISS_ON : ISS_OFF;
    CalibrationStateS[CALIBRATION_OFF].s = enabled ? ISS_OFF : ISS_ON;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get Temperature
/////////////////////////////////////////////////////////////////////////////
bool EFA::readTemperature()
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0}, len = 7;

    for (uint8_t i = 0; i < 2; i++)
    {
        cmd[0] = DRIVER_SOM;
        cmd[1] = 0x04;
        cmd[2] = DEVICE_PC;
        cmd[3] = DEVICE_TEMP;
        cmd[4] = TEMP_GET;
        cmd[5] = i;
        cmd[6] = calculateCheckSum(cmd, len);

        if (!validateLengths(cmd, len))
            return false;

        if (!sendCommand(cmd, res, len, DRIVER_LEN))
            return false;

        TemperatureN[i].value = calculateTemperature(res[5], res[6]);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Calculate temperature from bytes
/////////////////////////////////////////////////////////////////////////////
double EFA::calculateTemperature(uint8_t byte2, uint8_t byte3)
{
    if (byte2 == 0x7F && byte3 == 0x7F)
        return -100;

    int raw_temperature = byte3 << 8 | byte2;
    if (raw_temperature & 0x8000)
        raw_temperature = raw_temperature - 0x10000;

    return raw_temperature / 16.0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
char * EFA::efaDump(char * buf, int buflen, const uint8_t * data, uint32_t size)
{
    int needed = 0, idx = 0;

    needed = size * 3 + 4; // each byte goes to 2 chars plus 1 space (or trailing null char) plus 4 marker characters

    if (needed > buflen)
    {
        return NULL;
    }

    for (uint32_t i = 0; i < size; i++)
    {
        if (i == 4)
        {
            (void) sprintf(buf + idx, "<%02X> ", data[i]);
            idx += 5;
        }
        else if (i == 5 && i < (size - 1))
        {
            buf[idx++] = '[';
            for (uint32_t j = i, k = 3; j < (size - 1) && k < data[1]; j++, k++)
            {
                (void) sprintf(buf + idx, "%02X ", data[j]);
                idx += 3;
                i = j;
            }
            buf[idx - 1] = ']';
            buf[idx++] = ' ';
        }
        else
        {
            (void) sprintf(buf + idx, "%02X ", data[i]);
            idx += 3;
        }
    }

    buf[idx - 1] = '\0';

    return buf;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> EFA::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

/////////////////////////////////////////////////////////////////////////////
/// From stack overflow #16605967
/////////////////////////////////////////////////////////////////////////////
template <typename T>
std::string EFA::to_string(const T a_value, const int n)
{
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
}

/////////////////////////////////////////////////////////////////////////////
/// Validate Lengths (simple sanity check, mainly for developer)
/////////////////////////////////////////////////////////////////////////////
bool EFA::validateLengths(const uint8_t *cmd, uint32_t len)
{
    if (len < 6)
    {
        LOGF_ERROR("packet length (%d) is too short for command %02X", len, cmd[4]);
        return false;
    }
    if (cmd[1] + 3 != (int)len)
    {
        LOGF_ERROR("packet length (%d) and data length (%d) discrepancy for command %02X", len, cmd[1], cmd[4]);
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Calculate Checksum
/////////////////////////////////////////////////////////////////////////////
uint8_t EFA::calculateCheckSum(const uint8_t *cmd, uint32_t len)
{
    int32_t sum = 0;

    for (uint32_t i = 1; i < len - 1; i++)
        sum += cmd[i];

    return ((-sum) & 0xFF);
}
