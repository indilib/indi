/*******************************************************************************
  Copyright(c) 2022 Jasem Mutlaq. All rights reserved.

  Pegasus INDIGO Filter Wheel

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "pegasus_indigo.h"
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

static std::unique_ptr<PegasusINDIGO> falcon(new PegasusINDIGO());

PegasusINDIGO::PegasusINDIGO()
{
    setVersion(1, 0);
    setFilterConnection(CONNECTION_SERIAL | CONNECTION_TCP);
}

bool PegasusINDIGO::initProperties()
{
    INDI::FilterWheel::initProperties();

    addAuxControls();

    ////////////////////////////////////////////////////////////////////////////
    /// Main Control Panel
    ////////////////////////////////////////////////////////////////////////////
    // Firmware
    FirmwareTP[0].fill("VERSION", "Version", "NA");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    CurrentFilter      = 1;
    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(7);

    return true;
}

bool PegasusINDIGO::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        // Main Control
        getFirmware();
        defineProperty(FirmwareTP);
    }
    else
    {
        // Main Control
        deleteProperty(FirmwareTP);
    }

    return true;
}

const char * PegasusINDIGO::getDefaultName()
{
    return "Pegasus INDIGO";
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusINDIGO::Handshake()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("W#", res))
    {
        return strstr(res, "FW_OK");
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusINDIGO::getFirmware()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("WV", res))
    {
        FirmwareTP[0].setText(res + 3);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusINDIGO::SelectFilter(int position)
{
    TargetFilter = position;
    char command[DRIVER_LEN] = {0}, response[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, "WM:%d", position);
    return sendCommand(command, response);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusINDIGO::TimerHit()
{
    if (isConnected() && FilterSlotNP.getState() == IPS_BUSY)
    {
        char response[DRIVER_LEN] = {0};
        if (sendCommand("WF", response))
        {
            int position = -1;
            if (sscanf(response, "WF:%d", &position) == 1)
            {
                if (position == TargetFilter)
                    SelectFilterDone(position);
            }
        }
    }

    SetTimer(getPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool PegasusINDIGO::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
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
        snprintf(formatted_command, DRIVER_LEN, "%s\n", cmd);
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
        // Remove \r\n
        res[nbytes_read - 2] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void PegasusINDIGO::hexDump(char * buf, const char * data, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::vector<std::string> PegasusINDIGO::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
