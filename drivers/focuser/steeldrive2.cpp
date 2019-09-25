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
    IUFillText(&InfoT[INFO_NAME], "INFO_NAME", "Name", "NA");
    IUFillText(&InfoT[INFO_VERSION], "INFO_VERSION", "Version", "NA");
    IUFillTextVector(&InfoTP, InfoT, 2, getDeviceName(), "INFO", "Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Focuser Device Operation
    IUFillSwitch(&OperationS[OPERATION_REBOOT], "OPERATION_REBOOT", "Reboot", ISS_OFF);
    IUFillSwitch(&OperationS[OPERATION_RESET], "OPERATION_RESET", "Factory Reset", ISS_OFF);
    IUFillSwitch(&OperationS[OPERATION_ZEROING], "OPERATION_ZEROING", "Zero Home", ISS_OFF);
    IUFillSwitchVector(&OperationSP, OperationS, 3, getDeviceName(), "OPERATION", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

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

        defineText(&InfoTP);
        defineSwitch(&OperationSP);

    }
    else
    {
        deleteProperty(InfoTP.name);
        deleteProperty(OperationSP.name);
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
        if (!strcmp(OperationSP.name, name))
        {
            IUUpdateSwitch(&OperationSP, states, names, n);
            if (OperationS[OPERATION_RESET].s == ISS_ON)
            {
                IUResetSwitch(&OperationSP);
                if (m_ConfirmFactoryReset == false)
                {
                    LOG_WARN("Click button again to confirm factory reset.");
                    OperationSP.s = IPS_IDLE;
                    IDSetSwitch(&OperationSP, nullptr);
                    return true;
                }
                else
                {
                    m_ConfirmFactoryReset = false;
                    if (!sendCommandOK("RESET"))
                    {
                        OperationSP.s = IPS_ALERT;
                        LOG_ERROR("Failed to reset to factory settings.");
                        IDSetSwitch(&OperationSP, nullptr);
                        return true;
                    }
                }
            }

            if (OperationS[OPERATION_REBOOT].s == ISS_ON)
            {
                IUResetSwitch(&OperationSP);
                if (!sendCommand("REBOOT"))
                {
                    OperationSP.s = IPS_ALERT;
                    LOG_ERROR("Failed to reboot device.");
                    IDSetSwitch(&OperationSP, nullptr);
                    return true;
                }

                LOG_INFO("Device is rebooting...");
                OperationSP.s = IPS_OK;
                IDSetSwitch(&OperationSP, nullptr);
                return true;
            }

            if (OperationS[OPERATION_ZEROING].s == ISS_ON)
            {
                if (!sendCommandOK("SET USE_ENDSTOP:1"))
                {
                    LOG_WARN("Failed to enable homing sensor magnet!");
                }

                if (!sendCommandOK("ZEROING"))
                {
                    IUResetSwitch(&OperationSP);
                    LOG_ERROR("Failed to zero to home position.");
                    OperationSP.s = IPS_ALERT;
                }
                else
                {
                    OperationSP.s = IPS_BUSY;
                    LOG_INFO("Zeroing to home position in progress...");
                }

                IDSetSwitch(&OperationSP, nullptr);
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

    int reversed = (FocusReverseS[REVERSED_ENABLED].s == ISS_ON) ? -1 : 1;

    int targetAbsPosition = (dir == FOCUS_INWARD) ? FocusAbsPosN[0].value - (ticks * reversed)
                            : FocusAbsPosN[0].value + (ticks * reversed);

    targetAbsPosition = std::min(limit, static_cast<uint32_t>(std::max(static_cast<int>(FocusAbsPosN[0].min), targetAbsPosition)));

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

    uint32_t summaryPosition = std::stoul(m_Summary[POSITION]);

    // Check if we're idle but the focuser is in motion
    if (FocusAbsPosNP.s != IPS_BUSY && (m_State == GOING_UP || m_State == GOING_DOWN))
    {
        IUResetSwitch(&FocusMotionSP);
        FocusMotionS[FOCUS_INWARD].s = (m_State == GOING_DOWN) ? ISS_ON : ISS_OFF;
        FocusMotionS[FOCUS_OUTWARD].s = (m_State == GOING_DOWN) ? ISS_OFF : ISS_ON;
        FocusMotionSP.s = IPS_BUSY;
        FocusAbsPosNP.s = FocusRelPosNP.s = IPS_BUSY;
        FocusAbsPosN[0].value = summaryPosition;

        IDSetSwitch(&FocusMotionSP, nullptr);
        IDSetNumber(&FocusRelPosNP, nullptr);
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }
    else if (FocusAbsPosNP.s == IPS_BUSY && (m_State == STOPPED || m_State == ZEROED))
    {
        if (OperationSP.s == IPS_BUSY)
        {
            IUResetSwitch(&OperationSP);
            LOG_INFO("Homing is complete");
            OperationSP.s = IPS_OK;
            IDSetSwitch(&OperationSP, nullptr);
        }

        FocusAbsPosNP.s = IPS_OK;
        FocusAbsPosN[0].value = summaryPosition;
        if (FocusRelPosNP.s == IPS_BUSY)
        {
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusRelPosNP, nullptr);
        }
        if (FocusMotionSP.s == IPS_BUSY)
        {
            FocusMotionSP.s = IPS_IDLE;
            IDSetSwitch(&FocusMotionSP, nullptr);
        }

        IDSetNumber(&FocusAbsPosNP, nullptr);
    }
    else if (std::fabs(FocusAbsPosN[0].value - summaryPosition) > 0)
    {
        FocusAbsPosN[0].value = summaryPosition;
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    SetTimer(POLLMS);
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
        IUSaveText(&InfoT[INFO_VERSION], value.c_str());

    if (getParameter("VERSION", value))
        IUSaveText(&InfoT[INFO_VERSION], value.c_str());

    getSummary();

    FocusMaxPosN[0].value = std::stoul(m_Summary[LIMIT]);

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
    if (params.size() != 10)
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
