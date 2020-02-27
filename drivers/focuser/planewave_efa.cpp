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
    setVersion(1, 0);

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
    IUFillNumber(&TemperatureN[TEMPERATURE_SECONDARY], "TEMPERATURE_SECONDARY", "Secondary (c)", "%.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 3, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Fan Control
    IUFillSwitch(&FanStateS[FAN_ON], "FAN_ON", "On", ISS_OFF);
    IUFillSwitch(&FanStateS[FAN_OFF], "FAN_OFF", "Off", ISS_ON);
    IUFillSwitchVector(&FanStateSP, FanStateS, 2, getDeviceName(), "FOCUS_FAN", "Fans", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Calibration Control
    IUFillSwitch(&CalibrationStateS[CALIBRATION_ON], "CALIBRATION_ON", "Calibrated", ISS_OFF);
    IUFillSwitch(&CalibrationStateS[CALIBRATION_OFF], "CALIBRATION_OFF", "Not Calibrated", ISS_ON);
    IUFillSwitchVector(&CalibrationStateSP, CalibrationStateS, 2, getDeviceName(), "FOCUS_CALIBRATION", "Calibration", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

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

        defineText(&InfoTP);
        defineSwitch(&CalibrationStateSP);
        defineSwitch(&FanStateSP);
        defineNumber(&TemperatureNP);
    }
    else
    {
        deleteProperty(InfoTP.name);
        deleteProperty(CalibrationStateSP.name);
        deleteProperty(FanStateSP.name);
        deleteProperty(TemperatureNP.name);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool EFA::Handshake()
{
    std::string version;

    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = GET_VERSION;
    cmd[5] = calculateCheckSum(cmd);

    if (!sendCommand(cmd, res, 5, 8))
        return false;

    version = std::to_string(res[5]) + "." + std::to_string(res[6]);

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
            bool enabled = strcmp(FanStateS[FAN_ON].name, IUFindOnSwitchName(states, names, n)) == 0;
            if (setFanEnabled(enabled))
            {
                IUUpdateSwitch(&FanStateSP, states, names, n);
                FanStateSP.s = IPS_OK;
            }
            else
            {
                FanStateSP.s = IPS_ALERT;
            }

            IDSetSwitch(&FanStateSP, nullptr);
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

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// Sync focuser
/////////////////////////////////////////////////////////////////////////////
bool EFA::SyncFocuser(uint32_t ticks)
{
    char cmd[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x06;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_OFFSET_CNT;
    cmd[5] = (ticks >> 16) & 0xFF;
    cmd[6] = (ticks >>  8) & 0xFF;
    cmd[7] = (ticks >>  0) & 0xFF;
    cmd[8] = calculateCheckSum(cmd);
    return sendCommandOk(cmd, 9);
}

/////////////////////////////////////////////////////////////////////////////
/// Move Absolute Focuser
/////////////////////////////////////////////////////////////////////////////
IPState EFA::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x06;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GOTO_POS2;
    cmd[5] = (targetTicks >> 16) & 0xFF;
    cmd[6] = (targetTicks >>  8) & 0xFF;
    cmd[7] = (targetTicks >>  0) & 0xFF;
    cmd[8] = calculateCheckSum(cmd);

    return sendCommandOk(cmd, 9) ? IPS_BUSY : IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////////////
/// Move Focuser relatively
/////////////////////////////////////////////////////////////////////////////
IPState EFA::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int direction = (dir == FOCUS_INWARD) ? -1 : 1;
    int reversed = (FocusReverseS[REVERSED_ENABLED].s == ISS_ON) ? -1 : 1;
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
        if (std::fabs(TemperatureN[0].value - m_LastTemperature) > TEMPERATURE_THRESHOLD)
        {
            m_LastTemperature = TemperatureN[0].value;
            IDSetNumber(&TemperatureNP, nullptr);
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
    }
    else if (std::fabs(FocusAbsPosN[0].value - m_LastPosition) > 0)
    {
        m_LastPosition = FocusAbsPosN[0].value;
        IDSetNumber(&TemperatureNP, nullptr);
    }

    SetTimer(POLLMS);
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
    char cmd[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x06;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_SLEWLIMITMAX;
    cmd[5] = (ticks >> 16) & 0xFF;
    cmd[6] = (ticks >>  8) & 0xFF;
    cmd[7] = (ticks >>  0) & 0xFF;
    cmd[8] = calculateCheckSum(cmd);
    return sendCommandOk(cmd, 9);
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
bool EFA::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);
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
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool EFA::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    char hex_cmd[DRIVER_LEN * 3] = {0};
    hexDump(hex_cmd, cmd, cmd_len);
    LOGF_DEBUG("CMD <%s>", hex_cmd);
    rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    uint8_t chk = calculateCheckSum(res);

    if (chk != res[res_len])
    {
        LOG_ERROR("Invalid checksum!");
        return false;
    }

    char hex_res[DRIVER_LEN * 3] = {0};
    hexDump(hex_res, res, res_len);
    LOGF_DEBUG("RES <%s>", hex_res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool EFA::sendCommandOk(const char * cmd, int cmd_len)
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res, cmd_len, 1))
        return false;

    return res[0] == 1;
}

/////////////////////////////////////////////////////////////////////////////
/// Read Position
/////////////////////////////////////////////////////////////////////////////
bool EFA::readPosition()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FAN;
    cmd[4] = MTR_GET_POS;
    cmd[5] = calculateCheckSum(cmd);

    if (!sendCommand(cmd, res, 6, 9))
        return false;

    FocusAbsPosN[0].value = res[5] << 16 | res[6] << 8 | res[7];
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Is Slew over?
/////////////////////////////////////////////////////////////////////////////
bool EFA::isGOTOComplete()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x06;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GOTO_OVER;
    cmd[5] = calculateCheckSum(cmd);
    if (!sendCommand(cmd, res, 6, 1))
        return false;

    return (res[0] != 0);
}

/////////////////////////////////////////////////////////////////////////////
/// Set Fan Enabled/Disabled
/////////////////////////////////////////////////////////////////////////////
bool EFA::setFanEnabled(bool enabled)
{
    char cmd[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x04;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FAN;
    cmd[4] = FANS_SET;
    cmd[5] = enabled ? 1 : 0;
    cmd[6] = calculateCheckSum(cmd);

    return sendCommandOk(cmd, 7);
}

/////////////////////////////////////////////////////////////////////////////
/// Get Fan State
/////////////////////////////////////////////////////////////////////////////
bool EFA::readFanState()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FAN;
    cmd[4] = FANS_GET;
    cmd[5] = calculateCheckSum(cmd);

    if (!sendCommand(cmd, res, 6, 7))
        return false;

    bool enabled = (res[6] == 1);

    FanStateS[FAN_ON].s  = enabled ? ISS_ON : ISS_OFF;
    FanStateS[FAN_OFF].s = enabled ? ISS_OFF : ISS_ON;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Set Calibration Enabled/Disabled
/////////////////////////////////////////////////////////////////////////////
bool EFA::setCalibrationEnabled(bool enabled)
{
    char cmd[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x05;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_SET_CALIBRATION_STATE;
    cmd[5] = 0x40;
    cmd[6] = enabled ? 1 : 0;
    cmd[7] = calculateCheckSum(cmd);

    return sendCommandOk(cmd, 8);
}

/////////////////////////////////////////////////////////////////////////////
/// Get Calibration State
/////////////////////////////////////////////////////////////////////////////
bool EFA::readCalibrationState()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    cmd[0] = DRIVER_SOM;
    cmd[1] = 0x03;
    cmd[2] = DEVICE_PC;
    cmd[3] = DEVICE_FOC;
    cmd[4] = MTR_GET_CALIBRATION_STATE;
    cmd[5] = calculateCheckSum(cmd);

    if (!sendCommand(cmd, res, 6, 7))
        return false;

    bool enabled = (res[6] == 1);

    CalibrationStateS[CALIBRATION_ON].s  = enabled ? ISS_ON : ISS_OFF;
    CalibrationStateS[CALIBRATION_OFF].s = enabled ? ISS_OFF : ISS_ON;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get Temperature
/////////////////////////////////////////////////////////////////////////////
bool EFA::readTemperature()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    for (int i=0; i < 3; i++)
    {
        cmd[0] = DRIVER_SOM;
        cmd[1] = 0x04;
        cmd[2] = DEVICE_PC;
        cmd[3] = DEVICE_TEMP;
        cmd[4] = TEMP_GET;
        cmd[5] = i;
        cmd[6] = calculateCheckSum(cmd);

        if (!sendCommand(cmd, res, 7, 8))
            return false;

        TemperatureN[i].value = calculateTemperature(res[6], res[7]);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Calculate temperature from bytes
/////////////////////////////////////////////////////////////////////////////
double EFA::calculateTemperature(uint8_t byte2, uint8_t byte3)
{
    bool is_neg = false;
    int raw_temperature = byte2 * 256 + byte3;
    if (raw_temperature > 32768)
    {
        is_neg = true;
        raw_temperature = 65536 - raw_temperature;
    }

    int integer_part = raw_temperature / 16;
    int fraction_part = (raw_temperature - integer_part) * 625 / 1000;
    double celcius = integer_part + fraction_part / 10.0;
    if (is_neg)
        celcius = -celcius;

    return celcius;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void EFA::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

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
uint8_t EFA::calculateCheckSum(const char *cmd)
{
    uint32_t sum = 0;
    for (int i = 1; i < 8; i++)
        sum += cmd[i];
    return (-sum & 0xFF);
}
