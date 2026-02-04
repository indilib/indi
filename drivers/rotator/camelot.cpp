/*
    INDI Camelot Rotator
    Copyright (C) 2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "camelot.h"
#include "indicom.h"

#include <cmath>
#include <memory>
#include <regex>
#include <termios.h>
#include <cstring>
#include <sys/ioctl.h>
#include <chrono>
#include <math.h>
#include <iomanip>

// We declare an auto pointer to Camelot.
static std::unique_ptr<Camelot> camelot(new Camelot());

Camelot::Camelot()
{
    setVersion(1, 0);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Camelot::initProperties()
{
    INDI::Rotator::initProperties();

    SetCapability(ROTATOR_CAN_ABORT |
                  ROTATOR_CAN_REVERSE |
                  ROTATOR_CAN_SYNC);

    addAuxControls();

    // Speed
    RotatorSpeedSP[SPEED_FAST].fill("SPEED_FAST", "Fast", ISS_OFF);
    RotatorSpeedSP[SPEED_MEDIUM].fill("SPEED_MEDIUM", "Medium", ISS_ON);
    RotatorSpeedSP[SPEED_SLOW].fill("SPEED_SLOW", "Slow", ISS_OFF);
    RotatorSpeedSP.fill(getDeviceName(), "ROTATOR_SPEED", "Speed", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Power
    RotatorPowerNP[POWER_NORMAL].fill("POWER_NORMAL", "Normal", "%.f", 0, 255, 1, 120);
    RotatorPowerNP[POWER_HOLD].fill("POWER_HOLD", "Hold", "%.f", 0, 255, 1, 100);
    RotatorPowerNP.fill(getDeviceName(), "ROTATOR_POWER", "Power", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Camelot::updateProperties()
{
    // Need to call this before INDI::Rotator::updateProperties();
    if (isConnected())
    {
        char res[DRIVER_LEN] = {0};

        // Position
        if (sendCommand("P#", res))
        {
            try
            {
                double position = std::stod(res) / 10.0;
                GotoRotatorNP[0].setValue(position);
                GotoRotatorNP.setState(IPS_OK);
            }
            catch (const std::invalid_argument& ia)
            {
                LOGF_ERROR("Invalid argument for std::stod in updateProperties: %s", ia.what());
                GotoRotatorNP.setState(IPS_ALERT);
            }
            catch (const std::out_of_range& oor)
            {
                LOGF_ERROR("Out of range for std::stod in updateProperties: %s", oor.what());
                GotoRotatorNP.setState(IPS_ALERT);
            }
        }

        // Direction
        if (sendCommand("K#", res))
        {
            if (strstr(res, "Normal"))
                ReverseRotatorSP[INDI_DISABLED].setState(ISS_ON);
            else
                ReverseRotatorSP[INDI_ENABLED].setState(ISS_ON);
        }
    }

    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        queryStatus();

        defineProperty(RotatorSpeedSP);
        defineProperty(RotatorPowerNP);
    }
    else
    {
        deleteProperty(RotatorSpeedSP);
        deleteProperty(RotatorPowerNP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char * Camelot::getDefaultName()
{
    return "Camelot Rotator";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Camelot::Handshake()
{
    char res[DRIVER_LEN] = {0};
    // Try up to 3 times
    for (int i = 0; i < 3; i++)
    {
        if (sendCommand("#", res))
        {
            if (strstr(res, "OK.ROT!"))
                return true;
        }

        usleep(100000);
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState Camelot::MoveRotator(double degrees)
{
    char cmd[DRIVER_LEN] = {0};
    // Command is T#### where #### is degrees * 10
    snprintf(cmd, DRIVER_LEN, "T%04d", static_cast<int>(round(degrees * 10.0)));
    if (sendCommand(cmd))
    {
        return IPS_BUSY;
    }

    return IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Camelot::AbortRotator()
{
    return sendCommand("L#");
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Camelot::ReverseRotator(bool enabled)
{
    INDI_UNUSED(enabled);
    return sendCommand("D#");
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Camelot::SyncRotator(double degrees)
{
    char cmd[DRIVER_LEN] = {0};
    // Command is S### where ### is degrees * 10
    snprintf(cmd, DRIVER_LEN, "S%03d", static_cast<int>(round(degrees * 10.0)));
    return sendCommand(cmd);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Camelot::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (RotatorSpeedSP.isNameMatch(name))
        {
            updateProperty(RotatorSpeedSP, states, names, n, [this, states, n]
            {
                char cmd[DRIVER_LEN] = {0};
                snprintf(cmd, DRIVER_LEN, "Z%d", IUFindOnStateIndex(states, n));
                return sendCommand(cmd);
            }, true);
            return true;
        }
    }

    return INDI::Rotator::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Camelot::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (RotatorPowerNP.isNameMatch(name))
        {
            updateProperty(RotatorPowerNP, values, names, n, [this, values]()
            {
                char cmd[DRIVER_LEN] = {0};
                snprintf(cmd, DRIVER_LEN, "*%d", static_cast<int>(values[POWER_NORMAL]));
                auto normalOk = sendCommand(cmd);
                snprintf(cmd, DRIVER_LEN, "+%d", static_cast<int>(values[POWER_HOLD]));
                auto holdOk = sendCommand(cmd);
                return normalOk && holdOk;
            }, true);
            return true;
        }
    }
    return INDI::Rotator::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Camelot::queryStatus()
{
    char res[DRIVER_LEN] = {0};

    // Speed
    if (sendCommand("Y#", res))
    {
        if (strstr(res, "Fast"))
            RotatorSpeedSP[SPEED_FAST].setState(ISS_ON);
        else if (strstr(res, "Medium"))
            RotatorSpeedSP[SPEED_MEDIUM].setState(ISS_ON);
        else if (strstr(res, "Slow"))
            RotatorSpeedSP[SPEED_SLOW].setState(ISS_ON);
    }

    // Power
    if (sendCommand("R1#", res))
    {
        RotatorPowerNP[POWER_NORMAL].setValue(std::stoi(res));
    }

    if (sendCommand("R0#", res))
    {
        RotatorPowerNP[POWER_HOLD].setValue(std::stoi(res));
    }
    RotatorPowerNP.setState(IPS_OK);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void Camelot::TimerHit()
{
    if (!isConnected())
        return;

    char res[DRIVER_LEN] = {0};
    double current_pos = GotoRotatorNP[0].getValue();
    IPState current_state = GotoRotatorNP.getState();

    if (current_state == IPS_BUSY)
    {
        if (sendCommand("J#", res) && strstr(res, "M0:OK"))
            GotoRotatorNP.setState(IPS_OK);
    }

    if (sendCommand("P#", res))
    {
        try
        {
            GotoRotatorNP[0].setValue(std::stod(res) / 10.0);
        }
        catch (const std::invalid_argument& ia)
        {
            LOGF_ERROR("Invalid argument for std::stod in TimerHit: %s", ia.what());
        }
        catch (const std::out_of_range& oor)
        {
            LOGF_ERROR("Out of range for std::stod in TimerHit: %s", oor.what());
        }
    }

    if (std::abs(current_pos - GotoRotatorNP[0].getValue()) > 0.1 || current_state != GotoRotatorNP.getState())
    {
        GotoRotatorNP.apply();
    }

    SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
/// Save configuration items
/////////////////////////////////////////////////////////////////////////////
bool Camelot::saveConfigItems(FILE *fp)
{
    INDI::Rotator::saveConfigItems(fp);
    RotatorSpeedSP.save(fp);
    RotatorPowerNP.save(fp);
    return true;

}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool Camelot::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    int retries = 0;

    do
    {
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
            snprintf(formatted_command, DRIVER_LEN, "%s", cmd);
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

        if (res_len > 0)
            rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
        else
            rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

        if (rc == TTY_OK)
            break;

        if (rc == TTY_TIME_OUT)
            retries++;
        else
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("%s Serial read error: %s.", cmd, errstr);
            return false;
        }
    }
    while (retries < 3);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s Serial read error after 3 retries: %s.", cmd, errstr);
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
        // Remove extra #
        res[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void Camelot::hexDump(char * buf, const char * data, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}
