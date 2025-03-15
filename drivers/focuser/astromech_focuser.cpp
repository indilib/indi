/*******************************************************************************
 Copyright(c) 2019 Christian Liska. All rights reserved.

 Implementation based on Lacerta MFOC driver
 (written 2018 by Franck Le Rhun and Christian Liska).

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "astromech_focuser.h"
#include "config.h"

#include <indicom.h>
#include <connectionplugins/connectionserial.h>

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <chrono>
#include <regex>
#include <thread>

static std::unique_ptr<astromechanics_foc> Astromechanics_foc(new astromechanics_foc());

// Delay for receiving messages
#define FOC_POSMAX_HARDWARE 32767
#define FOC_POSMIN_HARDWARE 0

bool astromechanics_foc::Disconnect()
{
    SetApperture(0);
    return true;
}

void ISSnoopDevice(XMLEle *root)
{
    Astromechanics_foc->ISSnoopDevice(root);
}

/************************************************************************************
 *
************************************************************************************/
astromechanics_foc::astromechanics_foc()
{
    setVersion(0, 2);
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);
}

/************************************************************************************
 *
************************************************************************************/
const char *astromechanics_foc::getDefaultName()
{
    return "Astromechanics FOC";
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::initProperties()
{
    INDI::Focuser::initProperties();

    FocusMaxPosNP[0].setMin(FOC_POSMIN_HARDWARE);
    FocusMaxPosNP[0].setMax(FOC_POSMAX_HARDWARE);
    FocusMaxPosNP[0].setStep(500);
    FocusMaxPosNP[0].setValue(FOC_POSMAX_HARDWARE);

    FocusAbsPosNP[0].setMin(FOC_POSMIN_HARDWARE);
    FocusAbsPosNP[0].setMax(FOC_POSMAX_HARDWARE);
    FocusAbsPosNP[0].setStep(500);
    FocusAbsPosNP[0].setValue(0);

    FocusRelPosNP[0].setMin(FocusAbsPosNP[0].getMin());
    FocusRelPosNP[0].setMax(FocusAbsPosNP[0].getMax() / 2);
    FocusRelPosNP[0].setStep(250);
    FocusRelPosNP[0].setValue(0);

    // Aperture
    AppertureNP[0].fill("LENS_APP", "Index", "%.f", 0, 22, 1, 0);
    AppertureNP.fill(getDeviceName(), "LENS_APP_SETTING", "Apperture", MAIN_CONTROL_TAB, IP_RW,
                       60, IPS_IDLE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_38400);
    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::updateProperties()
{
    // Get Initial Position before we define it in the INDI::Focuser class
    FocusAbsPosNP[0].setValue(GetAbsFocuserPosition());

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(AppertureNP);
    }
    else
    {
        deleteProperty(AppertureNP);
    }

    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::Handshake()
{
    char res[DRIVER_LEN] = {0};
    int position = 0;
    for (int i = 0; i < 3; i++)
    {
        if (!sendCommand("P#", res))
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        else
        {
            sscanf(res, "%d", &position);
            FocusAbsPosNP[0].setValue(position);
            FocusAbsPosNP.setState(IPS_OK);
            SetApperture(0);
            return true;
        }
    }

    return false;
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "LENS_APP_SETTING") == 0)
        {
            AppertureNP.update(values, names, n);
            AppertureNP.setState(IPS_OK);
            AppertureNP.apply();
            SetApperture(AppertureNP[0].getValue());
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************
 *
************************************************************************************/
IPState astromechanics_foc::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "M%u#", targetTicks);
    if (sendCommand(cmd))
    {
        FocusAbsPosNP[0].setValue(GetAbsFocuserPosition());
        return IPS_OK;
    }

    return IPS_ALERT;
}

/************************************************************************************
 *
************************************************************************************/
IPState astromechanics_foc::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    // Clamp
    int32_t offset = ((dir == FOCUS_INWARD) ? -1 : 1) * static_cast<int32_t>(ticks);
    int32_t newPosition = FocusAbsPosNP[0].getValue() + offset;
    newPosition = std::max(static_cast<int32_t>(FocusAbsPosNP[0].getMin()),
                           std::min(static_cast<int32_t>(FocusAbsPosNP[0].getMax()),
                                    newPosition));

    FocusAbsPosNP.setState(IPS_BUSY);
    FocusAbsPosNP.apply();

    return MoveAbsFocuser(newPosition);
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::SetApperture(uint32_t index)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "A%u#", index);
    return sendCommand(cmd);
}

/************************************************************************************
 *
************************************************************************************/
uint32_t astromechanics_foc::GetAbsFocuserPosition()
{
    char res[DRIVER_LEN] = {0};
    int position = 0;

    if (!sendCommand("P#", res))
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    else
    {
        sscanf(res, "%d", &position);
        return position;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool astromechanics_foc::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
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
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
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
    {
        // Read respose
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);
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
        // Remove extra #
        res[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void astromechanics_foc::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> astromechanics_foc::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
