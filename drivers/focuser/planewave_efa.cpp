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
    InfoTP[INFO_VERSION].fill("INFO_VERSION", "Version", "NA");
    InfoTP.fill(getDeviceName(), "INFO", "Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Focuser temperature
    TemperatureNP[TEMPERATURE_PRIMARY].fill("TEMPERATURE_PRIMARY", "Primary (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP[TEMPERATURE_AMBIENT].fill("TEMPERATURE_AMBIENT", "Ambient (c)", "%.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Fan Control
    FanStateSP[FAN_ON].fill("FAN_ON", "On", ISS_OFF);
    FanStateSP[FAN_OFF].fill("FAN_OFF", "Off", ISS_ON);
    FanStateSP.fill(getDeviceName(), "FOCUS_FAN", "Fans", FAN_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Fan Control Mode
    FanControlSP[FAN_MANUAL].fill("FAN_MANUAL", "Manual", ISS_ON);
    FanControlSP[FAN_AUTOMATIC_ABSOLUTE].fill("FAN_AUTOMATIC_ABSOLUTE", "Auto. Absolute", ISS_OFF);
    FanControlSP[FAN_AUTOMATIC_RELATIVE].fill("FAN_AUTOMATIC_RELATIVE", "Auto. Relative", ISS_OFF);
    FanControlSP.fill(getDeviceName(), "FOCUS_FAN_CONTROL", "Control Mode", FAN_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Fan Control Parameters
    FanControlNP[FAN_MAX_ABSOLUTE].fill("FAN_MAX_ABSOLUTE", "Max Primary (c)", "%.2f", 0, 50., 5., 25.);
    FanControlNP[FAN_MAX_RELATIVE].fill("FAN_MAX_RELATIVE", "Max Relative (c)", "%.2f", 0., 30., 1., 2.5);
    FanControlNP[FAN_DEADZONE].fill("FAN_DEADZONE", "Deadzone (c)", "%.2f", 0.1, 10, 0.5, 0.5);
    FanControlNP.fill(getDeviceName(), "FOCUS_FAN_PARAMS", "Control Params",
                       FAN_TAB, IP_RW, 0, IPS_IDLE);

    // Fan Off on Disconnect
    FanDisconnectSP[FAN_OFF_ON_DISCONNECT].fill("FAN_OFF_ON_DISCONNECT", "Switch Off", ISS_ON);
    FanDisconnectSP.fill(getDeviceName(), "FOCUS_FAN_DISCONNECT", "On Disconnect", FAN_TAB,
                       IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    // Calibration Control
    CalibrationStateSP[CALIBRATION_ON].fill("CALIBRATION_ON", "Calibrated", ISS_OFF);
    CalibrationStateSP[CALIBRATION_OFF].fill("CALIBRATION_OFF", "Not Calibrated", ISS_ON);
    CalibrationStateSP.fill(getDeviceName(), "FOCUS_CALIBRATION", "Calibration",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Setup limits
    FocusMaxPosNP[0].setValue(1e7);
    FocusMaxPosNP[0].setMax(1e7);
    FocusMaxPosNP[0].setStep(FocusMaxPosNP[0].getMax() / 50);

    FocusAbsPosNP[0].setMax(1e7);
    FocusAbsPosNP[0].setStep(FocusAbsPosNP[0].getMax() / 50);

    FocusSyncNP[0].setMax(1e7);
    FocusSyncNP[0].setStep(FocusSyncNP[0].getMax() / 50);

    FocusRelPosNP[0].setMax(FocusAbsPosNP[0].getMax() / 2);
    FocusRelPosNP[0].setStep(FocusRelPosNP[0].getMax() / 50);

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

        defineProperty(InfoTP);
        defineProperty(CalibrationStateSP);

        // Fan
        defineProperty(FanStateSP);
        defineProperty(FanControlSP);
        FanControlSP.load();
        defineProperty(FanDisconnectSP);

        defineProperty(TemperatureNP);
    }
    else
    {
        deleteProperty(InfoTP);
        deleteProperty(CalibrationStateSP);

        deleteProperty(FanStateSP);
        deleteProperty(FanControlSP);
        deleteProperty(FanControlNP);
        deleteProperty(FanDisconnectSP);

        deleteProperty(TemperatureNP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool EFA::Disconnect()
{
    if (FanDisconnectSP[FAN_OFF_ON_DISCONNECT].getState() == ISS_ON)
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
    InfoTP[INFO_VERSION].setText(version.c_str());

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
        if (CalibrationStateSP.isNameMatch(name))
        {
            bool enabled = strcmp(CalibrationStateSP[CALIBRATION_ON].getName(), IUFindOnSwitchName(states, names, n)) == 0;
            if (setCalibrationEnabled(enabled))
            {
                CalibrationStateSP.update(states, names, n);
                CalibrationStateSP.setState(IPS_OK);
            }
            else
            {
                CalibrationStateSP.setState(IPS_ALERT);
            }

            CalibrationStateSP.apply();
            return true;
        }
        // Fan State
        else if (FanStateSP.isNameMatch(name))
        {
            if (FanControlSP[FAN_MANUAL].getState() == ISS_OFF)
            {
                FanStateSP.setState(IPS_IDLE);
                LOG_WARN("Cannot control fan while manual control is turned off.");
                FanControlSP.apply();
                return true;
            }

            bool enabled = strcmp(FanStateSP[FAN_ON].getName(), IUFindOnSwitchName(states, names, n)) == 0;
            if (setFanEnabled(enabled))
            {
                FanStateSP.update(states, names, n);
                FanStateSP.setState(enabled ? IPS_OK : IPS_IDLE);
            }
            else
            {
                FanStateSP.setState(IPS_ALERT);
            }

            FanStateSP.apply();
            return true;
        }
        // Fan Control
        else if (FanControlSP.isNameMatch(name))
        {
            FanControlSP.update(states, names, n);
            if (FanControlSP[FAN_MANUAL].getState() == ISS_ON)
            {
                deleteProperty(FanControlNP);
                LOG_INFO("Fan is now controlled manually.");
            }
            else
            {
                LOG_INFO("Fan is now controlled automatically per the control parameters.");
                defineProperty(FanControlNP);
            }

            FanControlSP.setState(IPS_OK);
            FanControlSP.apply();
            return true;
        }
        // Fan Disconnect
        else if (FanDisconnectSP.isNameMatch(name))
        {
            FanDisconnectSP.update(states, names, n);

            if (FanDisconnectSP[FAN_OFF_ON_DISCONNECT].getState() == ISS_ON)
                LOG_INFO("Fan shall be turned off upon device disconnection.");
            else
                LOG_INFO("Fan shall left as-is upon device disconnection.");

            FanDisconnectSP.setState(IPS_OK);
            FanDisconnectSP.apply();
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
        if (FanControlNP.isNameMatch(name))
        {
            FanControlNP.update(values, names, n);
            FanControlNP.setState(IPS_OK);
            FanControlNP.apply();
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
    int reversed = (FocusReverseSP[INDI_ENABLED].getState() == ISS_ON) ? -1 : 1;
    int relative = static_cast<int>(ticks);
    int targetAbsPosition = FocusAbsPosNP[0].getValue() + (relative * direction * reversed);

    targetAbsPosition = std::min(static_cast<uint32_t>(FocusAbsPosNP[0].getMax()),
                                 static_cast<uint32_t>(std::max(static_cast<int>(FocusAbsPosNP[0].getMin()), targetAbsPosition)));

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
        for (size_t i = 0; i < TemperatureNP.count(); i++)
        {
            if (std::fabs(TemperatureNP[i].getValue() - m_LastTemperature[i]) > TEMPERATURE_THRESHOLD)
            {
                aboveThreshold = true;
                m_LastTemperature[i] = TemperatureNP[i].getValue();
            }
        }

        if (aboveThreshold)
            TemperatureNP.apply();


        if (FanControlSP[FAN_MANUAL].getState() == ISS_OFF)
        {
            bool turnOn = false, turnOff = false;
            const bool isFanOn = FanStateSP[FAN_ON].getState() == ISS_ON;

            // Check if we need to do automatic regulation of fan
            if (FanControlSP[FAN_AUTOMATIC_ABSOLUTE].getState() == ISS_ON)
            {
                // Adjust delta for deadzone
                double min_delta = FanControlNP[FAN_MAX_ABSOLUTE].getValue() - FanControlNP[FAN_DEADZONE].getValue();
                double max_delta = FanControlNP[FAN_MAX_ABSOLUTE].getValue() + FanControlNP[FAN_DEADZONE].getValue();

                turnOn = TemperatureNP[TEMPERATURE_PRIMARY].getValue() > max_delta;
                turnOff = TemperatureNP[TEMPERATURE_PRIMARY].getValue() < min_delta;
            }
            else if (FanControlSP[FAN_AUTOMATIC_RELATIVE].getState() == ISS_ON)
            {
                // Temperature delta
                double tDiff = TemperatureNP[TEMPERATURE_PRIMARY].getValue() - TemperatureNP[TEMPERATURE_AMBIENT].getValue();
                // Adjust delta for deadzone
                double min_delta = FanControlNP[FAN_MAX_RELATIVE].getValue() - FanControlNP[FAN_DEADZONE].getValue();
                double max_delta = FanControlNP[FAN_MAX_RELATIVE].getValue() + FanControlNP[FAN_DEADZONE].getValue();

                // Check if we need to turn off/on fan
                turnOn = tDiff > max_delta;
                turnOff = tDiff < min_delta;
            }

            if (isFanOn && turnOff)
            {
                setFanEnabled(false);
                FanStateSP[FAN_ON].setState(ISS_OFF);
                FanStateSP[FAN_OFF].setState(ISS_ON);
                FanStateSP.setState(IPS_IDLE);
                FanStateSP.apply();
            }
            else if (!isFanOn && turnOn)
            {
                setFanEnabled(true);
                FanStateSP[FAN_ON].setState(ISS_ON);
                FanStateSP[FAN_OFF].setState(ISS_OFF);
                FanStateSP.setState(IPS_OK);
                FanStateSP.apply();
            }
        }
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (isGOTOComplete())
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            LOG_INFO("Focuser reached requested position.");
        }
        else
            FocusAbsPosNP.apply();
    }
    else if (std::fabs(FocusAbsPosNP[0].getValue() - m_LastPosition) > 0)
    {
        m_LastPosition = FocusAbsPosNP[0].getValue();
        FocusAbsPosNP.apply();
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

    FanControlSP.save(fp);
    if (FanControlNP.getState() == IPS_OK)
        FanControlNP.save(fp);
    FanDisconnectSP.save(fp);
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
        FocusAbsPosNP[0].setMax(FocusMaxPosNP[0].getMax());
        FocusAbsPosNP[0].setStep(FocusAbsPosNP[0].getMax() / 50);

        FocusRelPosNP[0].setValue(FocusAbsPosNP[0].getMax() / 50);
        FocusRelPosNP[0].setMax(FocusAbsPosNP[0].getMax() / 2);
        FocusRelPosNP[0].setStep(FocusRelPosNP[0].getMax() / 50);

        FocusMaxPosNP[0].setValue(FocusMaxPosNP[0].getMax());
        FocusMaxPosNP[0].setStep(FocusMaxPosNP[0].getMax() / 50);

        FocusRelPosNP.updateMinMax();
        FocusAbsPosNP.updateMinMax();
        FocusMaxPosNP.updateMinMax();
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

    FocusAbsPosNP[0].setValue(res[5] << 16 | res[6] << 8 | res[7]);
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
        FocusMaxPosNP[0].setMax(limit);
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

    FanStateSP[FAN_ON].setState(enabled ? ISS_ON : ISS_OFF);
    FanStateSP[FAN_OFF].setState(enabled ? ISS_OFF : ISS_ON);

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

    CalibrationStateSP[CALIBRATION_ON].setState(enabled ? ISS_ON : ISS_OFF);
    CalibrationStateSP[CALIBRATION_OFF].setState(enabled ? ISS_OFF : ISS_ON);

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

        TemperatureNP[i].setValue(calculateTemperature(res[5], res[6]));
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
