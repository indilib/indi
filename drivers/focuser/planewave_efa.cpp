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
    setVersion(1, 1);

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

    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = GET_VERSION;
    cmd[5] = calculateCheckSum(cmd, 6);

    if (!sendCommand(cmd, res, 6, 8))
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
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x06;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_OFFSET_CNT;
    cmd[5] = (ticks >> 16) & 0xFF;
    cmd[6] = (ticks >>  8) & 0xFF;
    cmd[7] = (ticks >>  0) & 0xFF;
    cmd[8] = calculateCheckSum(cmd, 9);

    if (!sendCommand(cmd, res, 9, 7))
        return false;

    return (res[5] == 1);
}

/////////////////////////////////////////////////////////////////////////////
/// Move Absolute Focuser
/////////////////////////////////////////////////////////////////////////////
IPState EFA::MoveAbsFocuser(uint32_t targetTicks)
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x06;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GOTO_POS2;
    cmd[5] = (targetTicks >> 16) & 0xFF;
    cmd[6] = (targetTicks >>  8) & 0xFF;
    cmd[7] = (targetTicks >>  0) & 0xFF;
    cmd[8] = calculateCheckSum(cmd, 9);

    if (!sendCommand(cmd, res, 9, 7))
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
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x06;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_SLEWLIMITMAX;
    cmd[5] = (ticks >> 16) & 0xFF;
    cmd[6] = (ticks >>  8) & 0xFF;
    cmd[7] = (ticks >>  0) & 0xFF;
    cmd[8] = calculateCheckSum(cmd, 9);

    if (!sendCommand(cmd, res, 9, 7))
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
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool EFA::sendCommand(const uint8_t * cmd, uint8_t *res, uint32_t cmd_len, uint32_t res_len)
{
    int nbytes_written = 0, nbytes_read = 0, bits = 0, rc = 0;
    char echo[DRIVER_LEN] = {0};

    for (int j = 0; j < 3; j++)
    {
        // Wait until CTS is cleared.
        for (int i = 0; i < 10; i++)
        {
            if ( (rc = ioctl(PortFD, TIOCMGET, &bits)) == 0 && (bits & TIOCM_CTS) == 0)
                break;
            usleep(100000);
        }

        if (rc < 0)
        {
            LOGF_ERROR("CTS timed out: %s", strerror(errno));
            return false;
        }

        // Now raise RTS
        bits = TIOCM_RTS;
        ioctl(PortFD, TIOCMSET, &bits);

        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, reinterpret_cast<const char *>(cmd), cmd_len, &nbytes_written);

        if (rc != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Serial write error: %s.", errstr);
            return false;
        }

        // Read back the echo
        tty_read(PortFD, echo, cmd_len, DRIVER_TIMEOUT, &nbytes_read);

        // Now lower RTS
        ioctl(PortFD, TIOCMBIC, &bits);

        // Next read the actual response from EFA
        rc = tty_read(PortFD, reinterpret_cast<char *>(res), res_len, DRIVER_TIMEOUT, &nbytes_read);

        if (rc == TTY_OK)
            break;

        usleep(100000);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    uint8_t chk = calculateCheckSum(res, res_len);

    if (chk != res[res_len - 1])
    {
        LOG_ERROR("Invalid checksum!");
        return false;
    }

    char hex_res[DRIVER_LEN * 3] = {0};
    hexDump(hex_res, res, res_len);
    LOGF_DEBUG("RES <%s>", hex_res);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Read Position
/////////////////////////////////////////////////////////////////////////////
bool EFA::readPosition()
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GET_POS;
    cmd[5] = calculateCheckSum(cmd, 6);

    if (!sendCommand(cmd, res, 6, 9))
        return false;

    FocusAbsPosN[0].value = res[5] << 16 | res[6] << 8 | res[7];
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Read Max Slew Limit
/////////////////////////////////////////////////////////////////////////////
bool EFA::readMaxSlewLimit()
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_SLEWLIMITGETMAX;
    cmd[5] = calculateCheckSum(cmd, 6);

    if (!sendCommand(cmd, res, 6, 9))
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
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GOTO_OVER;
    cmd[5] = calculateCheckSum(cmd, 6);

    if (!sendCommand(cmd, res, 6, 7))
        return false;

    return (res[5] != 0);
}

/////////////////////////////////////////////////////////////////////////////
/// Set Fan Enabled/Disabled
/////////////////////////////////////////////////////////////////////////////
bool EFA::setFanEnabled(bool enabled)
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x04;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FAN;
    cmd[4] = FANS_SET;
    cmd[5] = enabled ? 1 : 0;
    cmd[6] = calculateCheckSum(cmd, 7);

    if (!sendCommand(cmd, res, 7, 7))
        return false;

    return (res[5] == 1);
}

/////////////////////////////////////////////////////////////////////////////
/// Get Fan State
/////////////////////////////////////////////////////////////////////////////
bool EFA::readFanState()
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FAN;
    cmd[4] = FANS_GET;
    cmd[5] = calculateCheckSum(cmd, 6);

    if (!sendCommand(cmd, res, 6, 7))
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
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x05;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_SET_CALIBRATION_STATE;
    cmd[5] = 0x40;
    cmd[6] = enabled ? 1 : 0;
    cmd[7] = calculateCheckSum(cmd, 8);

    if (!sendCommand(cmd, res, 8, 7))
        return false;

    return (res[5] == 1);
}

/////////////////////////////////////////////////////////////////////////////
/// Get Calibration State
/////////////////////////////////////////////////////////////////////////////
bool EFA::readCalibrationState()
{
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x04;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GET_CALIBRATION_STATE;
    cmd[5] = calculateCheckSum(cmd, 6);

    if (!sendCommand(cmd, res, 6, 7))
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
    uint8_t cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    for (uint8_t i = 0; i < 2; i++)
    {
        cmd[0] = DRIVER_SOM;
        cmd[1] = 0x04;
        cmd[2] = DEVICE_PC;
        cmd[3] = DEVICE_TEMP;
        cmd[4] = TEMP_GET;
        cmd[5] = i;
        cmd[6] = calculateCheckSum(cmd, 7);

        if (!sendCommand(cmd, res, 7, 8))
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
void EFA::hexDump(char * buf, const uint8_t * data, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", data[i]);

    if (size > 0)
        buf[3 * size - 1] = '\0';
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
/// Calculate Checksum
/////////////////////////////////////////////////////////////////////////////
uint8_t EFA::calculateCheckSum(const uint8_t *cmd, uint32_t len)
{
    int32_t sum = 0;
    for (uint32_t i = 1; i < len - 1; i++)
        sum += cmd[i];
    return (sum & 0xFF);
}
