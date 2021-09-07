/*******************************************************************************
  Copyright(c) 2020 Jasem Mutlaq. All rights reserved.

  Pegasus Falcon Rotator

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

#include "pegasus_falcon.h"
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

// We declare an auto pointer to PegasusFalcon.
static std::unique_ptr<PegasusFalcon> falcon(new PegasusFalcon());

PegasusFalcon::PegasusFalcon()
{
    setVersion(1, 0);
    lastStatusData.reserve(7);
}

bool PegasusFalcon::initProperties()
{
    INDI::Rotator::initProperties();

    SetCapability(ROTATOR_CAN_ABORT |
                  ROTATOR_CAN_REVERSE |
                  ROTATOR_CAN_SYNC);

    addAuxControls();

    ////////////////////////////////////////////////////////////////////////////
    /// Main Control Panel
    ////////////////////////////////////////////////////////////////////////////
    // Reload Firmware
    IUFillSwitch(&ReloadFirmwareS[0], "RELOAD", "Reload", ISS_OFF);
    IUFillSwitchVector(&ReloadFirmwareSP, ReloadFirmwareS, 1, getDeviceName(), "RELOAD_FIRMWARE", "Firmware", MAIN_CONTROL_TAB,
                       IP_RW, ISR_ATMOST1,
                       60, IPS_IDLE);

    // Derotate
    IUFillNumber(&DerotateN[0], "INTERVAL", "Interval (ms)", "%.f", 0, 10000, 1000, 0);
    IUFillNumberVector(&DerotateNP, DerotateN, 1, getDeviceName(), "ROTATOR_DEROTATE", "Derotation", MAIN_CONTROL_TAB, IP_RW,
                       60, IPS_IDLE);

    // Firmware
    IUFillText(&FirmwareT[0], "VERSION", "Version", "NA");
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "FIRMWARE_INFO", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60,
                     IPS_IDLE);

    return true;
}

bool PegasusFalcon::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        // Main Control
        defineProperty(&DerotateNP);
        defineProperty(&FirmwareTP);
        defineProperty(&ReloadFirmwareSP);

    }
    else
    {
        // Main Control
        deleteProperty(DerotateNP.name);
        deleteProperty(FirmwareTP.name);
        deleteProperty(ReloadFirmwareSP.name);
    }

    return true;
}

const char * PegasusFalcon::getDefaultName()
{
    return "Pegasus Falcon";
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::Handshake()
{
    return getFirmware();
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // De-rotation
        if (!strcmp(name, DerotateNP.name))
        {
            const uint32_t ms = static_cast<uint32_t>(values[0]);
            if (setDerotation(ms))
            {
                DerotateN[0].value = values[0];
                if (values[0] > 0)
                    LOGF_INFO("De-rotation is enabled and set to 1 step per %u milliseconds.", ms);
                else
                    LOG_INFO("De-rotaiton is disabled.");
            }
            else
                DerotateNP.s = IPS_ALERT;
            IDSetNumber(&DerotateNP, nullptr);
            return true;
        }
    }

    return Rotator::ISNewNumber(dev, name, values, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // ReloadFirmware
        if (!strcmp(name, ReloadFirmwareSP.name))
        {
            ReloadFirmwareSP.s = reloadFirmware() ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&ReloadFirmwareSP, nullptr);
            LOG_INFO("Reloading firmware...");
            return true;
        }
    }

    return Rotator::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
IPState PegasusFalcon::MoveRotator(double angle)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "MD:%.2f", angle);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd) ? IPS_BUSY : IPS_ALERT);
    }

    return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::AbortRotator()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("FH", res))
    {
        return (!strcmp(res, "FH:1"));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::ReverseRotator(bool enabled)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "FR:%d", enabled ? 1 : 0);
    if (sendCommand(cmd, res))
    {
        return (!strcmp(res, cmd));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::SyncRotator(double angle)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "SD:%.2f", angle);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::reloadFirmware()
{
    return sendCommand("FF");
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::setDerotation(uint32_t ms)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "DR:%d", ms);
    return sendCommand(cmd, nullptr);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::saveConfigItems(FILE * fp)
{
    INDI::Rotator::saveConfigItems(fp);
    IUSaveConfigNumber(fp, &DerotateNP);
    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusFalcon::TimerHit()
{
    if (!isConnected())
        return;
    getStatusData();
    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::getFirmware()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("FV", res))
    {
        IUSaveText(&FirmwareT[0], res + 3);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool PegasusFalcon::getStatusData()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand("FA", res))
    {
        std::vector<std::string> result = split(res, ":");
        if (result.size() != 7)
        {
            LOG_WARN("Received wrong number of detailed sensor data. Retrying...");
            return false;
        }

        if (result == lastStatusData)
            return true;

        // Position
        const double position = std::stod(result[2]);
        // Is running?
        const IPState motionState = std::stoi(result[3]) == 1 ? IPS_BUSY : IPS_OK;

        // Update Absolute Position property if either position changes, or status changes.
        if (std::abs(position - GotoRotatorN[0].value) > 0.01 || GotoRotatorNP.s != motionState)
        {
            GotoRotatorN[0].value = position;
            GotoRotatorNP.s = motionState;
            IDSetNumber(&GotoRotatorNP, nullptr);
        }

        // TODO add this later to properties (Light?)
        //const bool limit = std::stoi(result[4]) == 1;

        const bool derotation = std::stoi(result[5]) == 1;
        const bool wasDerotated = DerotateN[0].value > 0;
        // TODO check if we get value from firmware
        if (derotation != wasDerotated)
        {
            DerotateNP.s = derotation ? IPS_BUSY : IPS_IDLE;;
            IDSetNumber(&DerotateNP, nullptr);
        }

        const bool reversed = std::stoi(result[6]) == 1;
        const bool wasReversed = ReverseRotatorS[INDI_ENABLED].s == ISS_ON;
        if (reversed != wasReversed)
        {
            ReverseRotatorS[INDI_ENABLED].s = reversed ? ISS_ON : ISS_OFF;
            ReverseRotatorS[INDI_DISABLED].s = reversed ? ISS_OFF : ISS_ON;
            IDSetSwitch(&ReverseRotatorSP, nullptr);
        }

        lastStatusData = result;
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool PegasusFalcon::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
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
        // Remove extra \r
        res[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void PegasusFalcon::hexDump(char * buf, const char * data, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
std::vector<std::string> PegasusFalcon::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
void PegasusFalcon::cleanupResponse(char *response)
{
    std::string s(response);
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](unsigned char x)
    {
        return std::isspace(x);
    }), s.end());
    strncpy(response, s.c_str(), DRIVER_LEN);
}
