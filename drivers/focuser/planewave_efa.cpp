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

void ISGetProperties(const char *dev)
{
    steelDrive->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    steelDrive->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    steelDrive->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    steelDrive->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    steelDrive->ISSnoopDevice(root);
}

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
        loadConfig(true, FanControlSP.getName());
        defineProperty(FanDisconnectSP);

        defineProperty(TemperatureNP);
    }
    else
    {
        deleteProperty(InfoTP.getName());
        deleteProperty(CalibrationStateSP.getName());

        deleteProperty(FanStateSP.getName());
        deleteProperty(FanControlSP.getName());
        deleteProperty(FanControlNP.getName());
        deleteProperty(FanDisconnectSP.getName());

        deleteProperty(TemperatureNP.getName());
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
    InfoTP[INFO_VERSION].setText(version);

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
                deleteProperty(FanControlNP.getName());
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
    int reversed = (FocusReverseSP[INDI_ENABLED].getState() == ISS_ON) ? -1 : 1;
    int relative = static_cast<int>(ticks);
    int targetAbsPosition = FocusAbsPosNP[0].getValue() + (relative * direction * reversed);

    targetAbsPosition = std::min(static_cast<uint32_t>(FocusAbsPosNP[0].max),
                                 static_cast<uint32_t>(std::max(static_cast<int>(FocusAbsPosNP[0].min), targetAbsPosition)));

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
        for (size_t i = 0; i < TemperatureNP.size(); i++)
        {
            if (std::fabs(TemperatureNP[i].value - m_LastTemperature[i]) > TEMPERATURE_THRESHOLD)
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
                double min_delta = FanControlNP[FAN_MAX_ABSOLUTE].getValue() - FanControlNP[FAN_DEADZONE].value;
                double max_delta = FanControlNP[FAN_MAX_ABSOLUTE].getValue() + FanControlNP[FAN_DEADZONE].value;

                turnOn = TemperatureNP[TEMPERATURE_PRIMARY].getValue() > max_delta;
                turnOff = TemperatureNP[TEMPERATURE_PRIMARY].getValue() < min_delta;
            }
            else if (FanControlSP[FAN_AUTOMATIC_RELATIVE].getState() == ISS_ON)
            {
                // Temperature delta
                double tDiff = TemperatureNP[TEMPERATURE_PRIMARY].getValue() - TemperatureNP[TEMPERATURE_AMBIENT].value;
                // Adjust delta for deadzone
                double min_delta = FanControlNP[FAN_MAX_RELATIVE].getValue() - FanControlNP[FAN_DEADZONE].value;
                double max_delta = FanControlNP[FAN_MAX_RELATIVE].getValue() + FanControlNP[FAN_DEADZONE].value;

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
    else if (std::fabs(FocusAbsPosNP[0].value - m_LastPosition) > 0)
    {
        m_LastPosition = FocusAbsPosNP[0].getValue();
        FocusAbsPosNP.apply();
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
        FocusAbsPosNP[0].setMax(FocusMaxPosNP[0].max);
        FocusAbsPosNP[0].setStep(FocusAbsPosNP[0].getMax() / 50);

        FocusRelPosNP[0].setValue(FocusAbsPosNP[0].getMax() / 50);
        FocusRelPosNP[0].setMax(FocusAbsPosNP[0].getMax() / 2);
        FocusRelPosNP[0].setStep(FocusRelPosNP[0].getMax() / 50);

        FocusMaxPosNP[0].setValue(FocusMaxPosNP[0].max);
        FocusMaxPosNP[0].setStep(FocusMaxPosNP[0].getMax() / 50);

        FocusRelPosNP.updateMinMax();
        FocusAbsPosNP.updateMinMax();
        FocusMaxPosNP.updateMinMax();
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

    FocusAbsPosNP[0].setValue(res[5] << 16 | res[6] << 8 | res[7]);
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

    FanStateSP[FAN_ON].setState(enabled ? ISS_ON : ISS_OFF);
    FanStateSP[FAN_OFF].setState(enabled ? ISS_OFF : ISS_ON);

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

    CalibrationStateSP[CALIBRATION_ON].setState(enabled ? ISS_ON : ISS_OFF);
    CalibrationStateSP[CALIBRATION_OFF].setState(enabled ? ISS_OFF : ISS_ON);

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
