/*
    Rainbow Astro Focuser
    Copyright (C) 2020 Abdulaziz Bouland (boulandab@ikarustech.com)

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

#include "rainbowRSF.h"
#include "indicom.h"

#include <cmath>
#include <cstring>
#include <termios.h>

static std::unique_ptr<RainbowRSF> rainbowRSF(new RainbowRSF());

RainbowRSF::RainbowRSF()
{
    setVersion(1, 0);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *RainbowRSF::getDefaultName()
{
    return "Rainbow Astro RSF";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::initProperties()
{
    INDI::Focuser::initProperties();

    // Go home switch
    IUFillSwitch(&GoHomeS[0], "GO_HOME", "Go Home", ISS_OFF);
    IUFillSwitchVector(&GoHomeSP, GoHomeS, 1, getDeviceName(), "FOCUS_GO_HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                       60, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB,
                       IP_RO, 0, IPS_IDLE);

    // Focuser Limits
    FocusAbsPosN[0].min = 0;
    FocusAbsPosN[0].max = 16000;
    FocusAbsPosN[0].step = 1000;

    FocusMaxPosN[0].min = 0 ;
    FocusMaxPosN[0].max = 16000;
    FocusMaxPosN[0].step = 1000;
    FocusMaxPosN[0].value = 16000;
    FocusMaxPosNP.p = IP_RO;

    FocusRelPosN[0].min = 0;
    FocusRelPosN[0].max = 8000;
    FocusRelPosN[0].step = 1000;

    addSimulationControl();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&TemperatureNP);
        defineProperty(&GoHomeSP);
    }
    else
    {
        deleteProperty(TemperatureNP.name);
        deleteProperty(GoHomeSP.name);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Find Home
        if (!strcmp(name, GoHomeSP.name))
        {
            GoHomeSP.s = findHome() ? IPS_BUSY : IPS_ALERT;

            if (GoHomeSP.s == IPS_BUSY)
                LOG_INFO("Moving to home position...");
            else
                LOG_ERROR("Failed to move to home position.");

            IDSetSwitch(&GoHomeSP, nullptr);
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::Handshake()
{
    if (updateTemperature())
    {
        LOG_INFO("Rainbow Astro is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO(
        "Error retrieving data from Rainbow Astro, please ensure Rainbow Astro controller is powered and the port is correct.");
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::updatePosition()
{
    char res[DRIVER_LEN] = {0};

    /////////////////////////////////////////////////////////////////////////////
    /// Simulation
    /////////////////////////////////////////////////////////////////////////////
    if (isSimulation())
    {
        // Move the focuser
        if (FocusAbsPosN[0].value > m_TargetPosition)
        {
            FocusAbsPosN[0].value -= 500;
            if (FocusAbsPosN[0].value < m_TargetPosition)
                FocusAbsPosN[0].value = m_TargetPosition;
        }
        else if (FocusAbsPosN[0].value < m_TargetPosition)
        {
            FocusAbsPosN[0].value += 500;
            if (FocusAbsPosN[0].value > m_TargetPosition)
                FocusAbsPosN[0].value = m_TargetPosition;
        }

        // update the states
        if (FocusAbsPosN[0].value == m_TargetPosition)
        {
            if (GoHomeSP.s == IPS_BUSY)
            {
                GoHomeSP.s = IPS_OK;
                FocusAbsPosNP.s = IPS_OK;
                FocusRelPosNP.s = IPS_OK;
                IDSetSwitch(&GoHomeSP, nullptr);
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
                LOG_INFO("Focuser reached home position.");
            }

            else if (FocusAbsPosNP.s == IPS_BUSY)
            {
                FocusAbsPosNP.s = IPS_OK;
                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
                LOG_INFO("Focuser reached target position.");
            }
        }

        return true;
    }

    /////////////////////////////////////////////////////////////////////////////
    /// Real Driver
    /////////////////////////////////////////////////////////////////////////////
    else if (sendCommand(":Fp#", res) == false)
        return false;

    int newPosition { 0 };
    if (sscanf(res, ":FP%d#", &newPosition) == 1)
    {
        FocusAbsPosN[0].value = newPosition + 8000;

        if (FocusAbsPosN[0].value == m_TargetPosition)
        {
            if (GoHomeSP.s == IPS_BUSY)
            {
                GoHomeSP.s = IPS_OK;
                GoHomeS[0].s = ISS_OFF;
                IDSetSwitch(&GoHomeSP, nullptr);

                FocusAbsPosNP.s = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, nullptr);

                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusRelPosNP, nullptr);

                LOG_INFO("Focuser reached home position.");
            }

            else if (FocusAbsPosNP.s == IPS_BUSY)
            {
                FocusAbsPosNP.s = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, nullptr);

                FocusRelPosNP.s = IPS_OK;

                IDSetNumber(&FocusRelPosNP, nullptr);
                LOG_INFO("Focuser reached target position.");
            }
        }
        return true;
    }
    else
    {
        FocusAbsPosNP.s = IPS_ALERT;
        return false;
    }

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::updateTemperature()
{
    char res[DRIVER_LEN] = {0};
    float temperature = 0;

    if (isSimulation())
        strncpy(res, ":FT1+23.5#", DRIVER_LEN);

    else if (sendCommand(":Ft1#", res) == false)
        return false;

    if (sscanf(res, ":FT1%g", &temperature) == 1)
    {
        TemperatureN[0].value = temperature;
        TemperatureNP.s = IPS_OK;
        return true;
    }
    else
    {
        TemperatureNP.s = IPS_ALERT;
        return false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
IPState RainbowRSF::MoveAbsFocuser(uint32_t targetTicks)
{
    m_TargetPosition = targetTicks;

    char cmd[DRIVER_LEN] = {0};
    int steps = targetTicks - 8000;

    snprintf(cmd, 16, ":Fm%c%04d#", steps >= 0 ? '+' : '-', std::abs(steps));

    if (isSimulation() == false)
    {
        if (sendCommand(cmd) == false)
            return IPS_ALERT;
    }
    return IPS_BUSY;
}

IPState RainbowRSF::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (IUFindOnSwitchIndex(&FocusReverseSP) == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosN[0].value + relativeTicks;

    bool rc = MoveAbsFocuser(newPosition);

    return (rc ? IPS_BUSY : IPS_ALERT);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::findHome()
{
    if (isSimulation())
    {
        MoveAbsFocuser(homePosition);
        FocusAbsPosNP.s = IPS_BUSY;
        return true;
    }
    else
    {
        m_TargetPosition = homePosition;
        FocusAbsPosNP.s = IPS_BUSY;
        return sendCommand(":Fh#");
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void RainbowRSF::TimerHit()
{
    // position update
    bool rc = updatePosition();
    if (rc)
    {
        if (abs(m_LastPosition - FocusAbsPosN[0].value) > 0)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            m_LastPosition = FocusAbsPosN[0].value;

            if (GoHomeSP.s == IPS_BUSY && FocusAbsPosN[0].value == homePosition)
            {
                GoHomeSP.s = IPS_OK;
                LOG_INFO("Focuser arrived at home position.");
                IDSetSwitch(&GoHomeSP, nullptr);
            }
        }
    }

    // temperature update
    if (m_TemperatureCounter++ == DRIVER_TEMPERATURE_FREQ)
    {
        rc = updateTemperature();
        if (rc)
        {
            if (abs(m_LastTemperature - TemperatureN[0].value) >= 0.05)
            {
                IDSetNumber(&TemperatureNP, nullptr);
                m_LastTemperature = TemperatureN[0].value;
            }
        }
        // Reset the counter
        m_TemperatureCounter = 0;
    }

    SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
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
        snprintf(formatted_command, DRIVER_LEN, "%s\r", cmd);
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

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void RainbowRSF::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}
