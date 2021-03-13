/*
    Baader SteelDriveII Focuser

    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "steeldrive2.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <regex>

static std::unique_ptr<SteelDriveII> steelDrive(new SteelDriveII());

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
SteelDriveII::SteelDriveII()
{
    setVersion(1, 0);

    // Focuser Capabilities
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_REVERSE |
                      FOCUSER_CAN_SYNC);
}

bool SteelDriveII::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser Information
    InfoTP[INFO_NAME].fill("INFO_NAME", "Name", "NA");
    InfoTP[INFO_VERSION].fill("INFO_VERSION", "Version", "NA");
    InfoTP.fill(getDeviceName(), "INFO", "Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Focuser Device Operation
    OperationSP[OPERATION_REBOOT].fill("OPERATION_REBOOT", "Reboot", ISS_OFF);
    OperationSP[OPERATION_RESET].fill("OPERATION_RESET", "Factory Reset", ISS_OFF);
    OperationSP[OPERATION_ZEROING].fill("OPERATION_ZEROING", "Zero Home", ISS_OFF);
    OperationSP.fill(getDeviceName(), "OPERATION", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Temperature Compensation
    TemperatureCompensationSP[TC_ENABLED].fill("TC_ENABLED", "Enabled", ISS_OFF);
    TemperatureCompensationSP[TC_DISABLED].fill("TC_DISABLED", "Disabled", ISS_ON);
    TemperatureCompensationSP.fill(getDeviceName(), "TC_COMPENSATE", "Compensation", COMPENSATION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // TC State
    TemperatureStateSP[TC_ENABLED].fill("TC_ACTIVE", "Active", ISS_OFF);
    TemperatureStateSP[TC_DISABLED].fill("TC_PAUSED", "Paused", ISS_ON);
    TemperatureStateSP.fill(getDeviceName(), "TC_State", "State", COMPENSATION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Temperature Compensation Settings
    TemperatureSettingsNP[TC_FACTOR].fill("TC_FACTOR", "Factor", "%.2f", 0, 1, 0.1, 0);
    TemperatureSettingsNP[TC_PERIOD].fill("TC_PERIOD", "Period (ms)", "%.f", 10, 600000, 1000, 0);
    TemperatureSettingsNP[TC_DELTA].fill("TC_DELTA", "Delta (C)", "%.2f", 0, 10, 0.1, 0);
    TemperatureSettingsNP.fill(getDeviceName(), "TC_SETTINGS", "Settings", COMPENSATION_TAB, IP_RW, 60, IPS_IDLE);

    // Temperature Sensors
    TemperatureSensorNP[TEMP_0].fill("TEMP_0", "Motor (C)", "%.2f", -60, 60, 0, 0);
    TemperatureSensorNP[TEMP_1].fill("TEMP_1", "Controller (C)", "%.f", -60, 60, 0, 0);
    TemperatureSensorNP[TEMP_AVG].fill("TEMP_AVG", "Average (C)", "%.2f", -60, 60, 0, 0);
    TemperatureSensorNP.fill(getDeviceName(), "TC_SENSOR", "Sensor", COMPENSATION_TAB, IP_RO, 60, IPS_IDLE);

    // Stepper Drive
    StepperDriveNP[CURRENT_MOVE].fill("STEPPER_DRIVE_CURRENT_MOVE", "Inverse Current Move", "%.f", 0, 127, 1, 25);
    StepperDriveNP[CURRENT_HOLD].fill("STEPPER_DRIVE_CURRENT_HOLD", "Inverse Current Hold", "%.f", 0, 127, 1, 100);
    StepperDriveNP.fill(getDeviceName(), "STEPPER_DRIVE", "Stepper Drive", OPTIONS_TAB, IP_RW, 60, IPS_IDLE
    );

    addAuxControls();
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    setDefaultPollingPeriod(500);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        getStartupValues();

        defineProperty(InfoTP);
        defineProperty(OperationSP);

        defineProperty(TemperatureCompensationSP);
        defineProperty(TemperatureStateSP);
        defineProperty(TemperatureSettingsNP);
        defineProperty(TemperatureSensorNP);
        defineProperty(StepperDriveNP);
    }
    else
    {
        deleteProperty(InfoTP.getName());
        deleteProperty(OperationSP.getName());

        deleteProperty(TemperatureCompensationSP.getName());
        deleteProperty(TemperatureStateSP.getName());
        deleteProperty(TemperatureSettingsNP.getName());
        deleteProperty(TemperatureSensorNP.getName());
        deleteProperty(StepperDriveNP.getName());
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::Handshake()
{
    std::string version;

    if (!getParameter("VERSION", version))
        return false;

    LOGF_INFO("Detected version %s", version.c_str());

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *SteelDriveII::getDefaultName()
{
    return "Baader SteelDriveII";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temperature Compensation
        if (TemperatureCompensationSP.isNameMatch(name))
        {
            bool enabled = !strcmp(IUFindOnSwitchName(states, names, n), TemperatureCompensationSP[TC_ENABLED].getName());
            bool rc = setParameter("TCOMP", enabled ? "1" : "0");

            if (rc)
            {
                TemperatureCompensationSP.update(states, names, n);
                TemperatureCompensationSP.setState(IPS_OK);
                LOGF_INFO("Temperature compensation is %s.", enabled ? "enabled" : "disabled");
            }
            else
            {
                TemperatureCompensationSP.setState(IPS_ALERT);
            }

            TemperatureCompensationSP.apply();
            return true;
        }

        // Temperature State (Paused or Active)
        if (TemperatureStateSP.isNameMatch(name))
        {
            bool active = !strcmp(IUFindOnSwitchName(states, names, n), TemperatureStateSP[TC_ACTIVE].getName());
            bool rc = setParameter("TCOMP_PAUSE", active ? "0" : "1");

            if (rc)
            {
                TemperatureStateSP.update(states, names, n);
                TemperatureStateSP.setState(IPS_OK);
                LOGF_INFO("Temperature compensation is %s.", active ? "active" : "paused");
            }
            else
            {
                TemperatureStateSP.setState(IPS_ALERT);
            }

            TemperatureStateSP.apply();
            return true;
        }

        // Operations
        if (OperationSP.isNameMatch(name))
        {
            OperationSP.update(states, names, n);
            if (OperationSP[OPERATION_RESET].getState() == ISS_ON)
            {
                OperationSP.reset();
                if (m_ConfirmFactoryReset == false)
                {
                    LOG_WARN("Click button again to confirm factory reset.");
                    OperationSP.setState(IPS_IDLE);
                    OperationSP.apply();
                    return true;
                }
                else
                {
                    m_ConfirmFactoryReset = false;
                    if (!sendCommandOK("RESET"))
                    {
                        OperationSP.setState(IPS_ALERT);
                        LOG_ERROR("Failed to reset to factory settings.");
                        OperationSP.apply();
                        return true;
                    }
                }
            }

            if (OperationSP[OPERATION_REBOOT].getState() == ISS_ON)
            {
                OperationSP.reset();
                if (!sendCommand("REBOOT"))
                {
                    OperationSP.setState(IPS_ALERT);
                    LOG_ERROR("Failed to reboot device.");
                    OperationSP.apply();
                    return true;
                }

                LOG_INFO("Device is rebooting...");
                OperationSP.setState(IPS_OK);
                OperationSP.apply();
                return true;
            }

            if (OperationSP[OPERATION_ZEROING].getState() == ISS_ON)
            {
                if (!sendCommandOK("SET USE_ENDSTOP:1"))
                {
                    LOG_WARN("Failed to enable homing sensor magnet!");
                }

                if (!sendCommandOK("ZEROING"))
                {
                    OperationSP.reset();
                    LOG_ERROR("Failed to zero to home position.");
                    OperationSP.setState(IPS_ALERT);
                }
                else
                {
                    OperationSP.setState(IPS_BUSY);
                    LOG_INFO("Zeroing to home position in progress...");
                }

                OperationSP.apply();
                return true;
            }
        }

    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (TemperatureSettingsNP.isNameMatch(name))
        {
            double factor = TemperatureSettingsNP[TC_FACTOR].getValue();
            double period = TemperatureSettingsNP[TC_PERIOD].getValue();
            double delta  = TemperatureSettingsNP[TC_DELTA].getValue();
            bool rc1 = true, rc2 = true, rc3 = true;
            TemperatureSettingsNP.update(values, names, n);

            if (factor != TemperatureSettingsNP[TC_FACTOR].getValue())
                rc1 = setParameter("TCOMP_FACTOR", to_string(factor));
            if (period != TemperatureSettingsNP[TC_PERIOD].getValue())
                rc2 = setParameter("TCOMP_PERIOD", to_string(period));
            if (delta != TemperatureSettingsNP[TC_DELTA].getValue())
                rc3 = setParameter("TCOMP_DELTA", to_string(delta));

            TemperatureSettingsNP.setState((rc1 && rc2 && rc3) ? IPS_OK : IPS_ALERT);
            TemperatureSettingsNP.apply();
            return true;
        }

        if (StepperDriveNP.isNameMatch(name))
        {
            StepperDriveNP.setState(IPS_OK);

            if (StepperDriveNP[CURRENT_MOVE].value != values[CURRENT_MOVE])
            {
                if (setParameter("CURRENT_MOVE", to_string(values[CURRENT_MOVE], 0)))
                    StepperDriveNP[CURRENT_MOVE].setValue(values[CURRENT_MOVE]);
                else
                    StepperDriveNP.setState(IPS_ALERT);
            }

            if (StepperDriveNP[CURRENT_HOLD].value != values[CURRENT_HOLD])
            {
                if (setParameter("CURRENT_HOLD", to_string(values[CURRENT_HOLD], 0)))
                    StepperDriveNP[CURRENT_HOLD].setValue(values[CURRENT_HOLD]);
                else
                    StepperDriveNP.setState(IPS_ALERT);
            }

            StepperDriveNP.apply();
            return true;
        }

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// Sync focuser
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::SyncFocuser(uint32_t ticks)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "SET POS:%u", ticks);
    return sendCommandOK(cmd);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState SteelDriveII::MoveAbsFocuser(uint32_t targetTicks)
{
    if (targetTicks < std::stoul(m_Summary[LIMIT]))
    {
        char cmd[DRIVER_LEN] = {0};
        snprintf(cmd, DRIVER_LEN, "GO %u", targetTicks);
        if (!sendCommandOK(cmd))
            return IPS_ALERT;

        return IPS_BUSY;
    }

    return IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState SteelDriveII::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t limit = std::stoul(m_Summary[LIMIT]);

    int direction = (dir == FOCUS_INWARD) ? -1 : 1;
    int reversed = (FocusReverseSP[INDI_ENABLED].getState() == ISS_ON) ? -1 : 1;
    int relative = static_cast<int>(ticks);
    int targetAbsPosition = FocusAbsPosNP[0].getValue() + (relative * direction * reversed);

    targetAbsPosition = std::min(limit, static_cast<uint32_t>(std::max(static_cast<int>(FocusAbsPosNP[0].min), targetAbsPosition)));

    return MoveAbsFocuser(targetAbsPosition);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void SteelDriveII::TimerHit()
{
    if (!isConnected())
        return;

    getSummary();

    uint32_t summaryPosition = std::max(0, std::stoi(m_Summary[POSITION]));

    // Check if we're idle but the focuser is in motion
    if (FocusAbsPosNP.getState() != IPS_BUSY && (m_State == GOING_UP || m_State == GOING_DOWN))
    {
        FocusMotionSP.reset();
        FocusMotionSP[FOCUS_INWARD].setState((m_State == GOING_DOWN) ? ISS_ON : ISS_OFF);
        FocusMotionSP[FOCUS_OUTWARD].setState((m_State == GOING_DOWN) ? ISS_OFF : ISS_ON);
        FocusMotionSP.setState(IPS_BUSY);
        FocusAbsPosNP.setState(IPS_BUSY);
        FocusRelPosNP.setState(IPS_BUSY);
        FocusAbsPosNP[0].setValue(summaryPosition);

        FocusMotionSP.apply();
        FocusRelPosNP.apply();
        FocusAbsPosNP.apply();
    }
    else if (FocusAbsPosNP.getState() == IPS_BUSY && (m_State == STOPPED || m_State == ZEROED))
    {
        if (OperationSP.getState() == IPS_BUSY)
        {
            OperationSP.reset();
            LOG_INFO("Homing is complete");
            OperationSP.setState(IPS_OK);
            OperationSP.apply();
        }

        FocusAbsPosNP.setState(IPS_OK);
        FocusAbsPosNP[0].setValue(summaryPosition);
        if (FocusRelPosNP.getState() == IPS_BUSY)
        {
            FocusRelPosNP.setState(IPS_OK);
            FocusRelPosNP.apply();
        }
        if (FocusMotionSP.getState() == IPS_BUSY)
        {
            FocusMotionSP.setState(IPS_IDLE);
            FocusMotionSP.apply();
        }

        FocusAbsPosNP.apply();
    }
    else if (std::fabs(FocusAbsPosNP[0].value - summaryPosition) > 0)
    {
        FocusAbsPosNP[0].setValue(summaryPosition);
        FocusAbsPosNP.apply();
    }

    if (std::fabs(FocusMaxPosNP[0].value - std::stoul(m_Summary[LIMIT])) > 0)
    {
        FocusMaxPosNP[0].setValue(std::stoul(m_Summary[LIMIT]));
        FocusMaxPosNP.apply();
    }

    double temp0 = std::stod(m_Summary[TEMP0]);
    double temp1 = std::stod(m_Summary[TEMP1]);
    double tempa = std::stod(m_Summary[TEMPAVG]);

    if (temp0 != TemperatureSensorNP[TEMP_0].value ||
            temp1 != TemperatureSensorNP[TEMP_1].value ||
            tempa != TemperatureSensorNP[TEMP_AVG].getValue())
    {
        TemperatureSensorNP[TEMP_0].setValue(temp0);
        TemperatureSensorNP[TEMP_1].setValue(temp1);
        TemperatureSensorNP[TEMP_AVG].setValue(tempa);
        TemperatureSensorNP.setState(IPS_OK);
        TemperatureSensorNP.apply();
    }

    SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::AbortFocuser()
{
    return sendCommandOK("STOP");
}

/////////////////////////////////////////////////////////////////////////////
/// Reverse Focuser Motion
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::SetFocuserMaxPosition(uint32_t ticks)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "SET LIMIT:%u", ticks);
    return sendCommandOK(cmd);
}

/////////////////////////////////////////////////////////////////////////////
/// Reverse Focuser Motion
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::ReverseFocuser(bool enabled)
{
    INDI_UNUSED(enabled);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get Startup values
/////////////////////////////////////////////////////////////////////////////
void SteelDriveII::getStartupValues()
{
    std::string value;

    if (getParameter("NAME", value))
        InfoTP[INFO_NAME].setText(value);

    if (getParameter("VERSION", value))
        InfoTP[INFO_VERSION].setText(value);

    if (getParameter("TCOMP", value))
    {
        TemperatureCompensationSP[TC_ENABLED].setState((value == "1") ? ISS_ON : ISS_OFF);
        TemperatureCompensationSP[TC_DISABLED].setState((value == "1") ? ISS_OFF : ISS_ON);
    }

    if (getParameter("TCOMP_FACTOR", value))
    {
        TemperatureSettingsNP[TC_FACTOR].setValue(std::stod(value));
    }

    if (getParameter("TCOMP_PERIOD", value))
    {
        TemperatureSettingsNP[TC_PERIOD].setValue(std::stod(value));
    }

    if (getParameter("TCOMP_DELTA", value))
    {
        TemperatureSettingsNP[TC_DELTA].setValue(std::stod(value));
    }

    if (getParameter("TCOMP_PAUSE", value))
    {
        TemperatureStateSP[TC_ACTIVE].setState((value == "0") ? ISS_ON : ISS_OFF);
        TemperatureStateSP[TC_PAUSED].setState((value == "0") ? ISS_OFF : ISS_ON);
    }

    StepperDriveNP.setState(IPS_OK);
    if (getParameter("CURRENT_MOVE", value))
        StepperDriveNP[CURRENT_MOVE].setValue(std::stod(value));
    else
        StepperDriveNP.setState(IPS_ALERT);

    if (getParameter("CURRENT_HOLD", value))
        StepperDriveNP[CURRENT_HOLD].setValue(std::stod(value));
    else
        StepperDriveNP.setState(IPS_ALERT);

    getSummary();

    FocusMaxPosNP[0].setValue(std::stoul(m_Summary[LIMIT]));
    FocusMaxPosNP.apply();

    TemperatureSensorNP[TEMP_0].setValue(std::stod(m_Summary[TEMP0]));
    TemperatureSensorNP[TEMP_1].setValue(std::stod(m_Summary[TEMP1]));
    TemperatureSensorNP[TEMP_AVG].setValue(std::stod(m_Summary[TEMPAVG]));

}

/////////////////////////////////////////////////////////////////////////////
/// Get Focuser State
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::getSummary()
{
    char res[DRIVER_LEN] = {0};

    if (!sendCommand("SUMMARY", res))
        return false;

    std::vector<std::string> params = split(res, ";");
    if (params.size() < 10)
        return false;

    for (int i = 0; i < 10; i++)
    {
        std::vector<std::string> value = split(params[i], ":");
        m_Summary[static_cast<Summary>(i)] = value[1];
    }

    if (m_Summary[STATE] == "GOING_UP")
        m_State = GOING_UP;
    else if (m_Summary[STATE] == "GOING_DOWN")
        m_State = GOING_DOWN;
    else if (m_Summary[STATE] == "STOPPED")
        m_State = STOPPED;
    else if (m_Summary[STATE] == "ZEROED")
        m_State = ZEROED;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get Single Parameter
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::getParameter(const std::string &parameter, std::string &value)
{
    char res[DRIVER_LEN] = {0};

    std::string cmd = "GET " + parameter;
    if (sendCommand(cmd.c_str(), res) == false)
        return false;

    std::vector<std::string> values = split(res, ":");
    if (values.size() != 2)
        return false;

    value = values[1];

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Set Single Parameter
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::setParameter(const std::string &parameter, const std::string &value)
{
    std::string cmd = "SET " + parameter + ":" + value;
    return sendCommandOK(cmd.c_str());
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::sendCommandOK(const char * cmd)
{
    char res[DRIVER_LEN] = {0};

    if (!sendCommand(cmd, res))
        return false;

    return strstr(res, "OK");
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool SteelDriveII::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);

        char formatted_command[DRIVER_LEN] = {0};
        snprintf(formatted_command, DRIVER_LEN, "$BS %s\r\n", cmd);
        rc = tty_write_string(PortFD, formatted_command, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    char rawResponse[DRIVER_LEN * 2] = {0};

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    else
    {
        // Read echo
        tty_nread_section(PortFD, rawResponse, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);
        // Read actual respose
        rc = tty_nread_section(PortFD, rawResponse, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        // Remove extra \r\n
        rawResponse[nbytes_read - 2] = 0;
        // Remove the $BS
        strncpy(res, rawResponse + 4, DRIVER_LEN);
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void SteelDriveII::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> SteelDriveII::split(const std::string &input, const std::string &regex)
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
std::string SteelDriveII::to_string(const T a_value, const int n)
{
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
}
