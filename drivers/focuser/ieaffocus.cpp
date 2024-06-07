/*
    iOptron iEAF Focuser

    Copyright (C) 2013 Paul de Backer (74.0632@gmail.com)
    Copyright (C) 2024 Jasem Mutlaq

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

#include "ieaffocus.h"
#include "indicom.h"

#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <memory>
#include <connectionplugins/connectionserial.h>

#define iEAFFOCUS_TIMEOUT 4
#define TEMPERATURE_THRESHOLD 0.1

std::unique_ptr<iEAFFocus> ieafFocus(new iEAFFocus());

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
iEAFFocus::iEAFFocus()
{
    setVersion(1, 1);
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_REVERSE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
iEAFFocus::~iEAFFocus()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEAFFocus::initProperties()
{
    INDI::Focuser::initProperties();

    setDefaultPollingPeriod(1500);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%2.2f", 0., 50., 0., 50.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    SetZeroSP[0].fill("SETZERO", "Set Current Position to 0", ISS_OFF);
    SetZeroSP.fill(getDeviceName(), "Zero Position", "Zero Position", OPTIONS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosN[0].min = 0.;
    FocusRelPosN[0].max = 5000.;
    FocusRelPosN[0].value = 0.;
    FocusRelPosN[0].step = 10.;

    FocusAbsPosN[0].min = 0.;
    FocusAbsPosN[0].max = 99999.;
    FocusAbsPosN[0].value = 0.;
    FocusAbsPosN[0].step = 10.;

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEAFFocus::updateProperties()
{
    INDI::Focuser::updateProperties();
    if (isConnected())
    {
        defineProperty(TemperatureNP);
        defineProperty(SetZeroSP);
        GetFocusParams();
    }
    else
    {
        deleteProperty(TemperatureNP);
        deleteProperty(SetZeroSP);
    }

    return true;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEAFFocus::Handshake()
{
    if (Ack())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "iEAFFocus is online. Getting focus parameters...");
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char * iEAFFocus::getDefaultName()
{
    return "iEAFFocus";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEAFFocus::Ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char resp[16];
    int ieafpos, ieafmodel, ieaflast;
    sleep(2);
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":DeviceInfo#", 12, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Init send getdeviceinfo  error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT * 2, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Init read deviceinfo error: %s.", errstr);
        return false;
    }
    tcflush(PortFD, TCIOFLUSH);
    resp[nbytes_read] = '\0';
    sscanf(resp, "%6d%2d%4d", &ieafpos, &ieafmodel, &ieaflast);
    if (ieafmodel == 2)
    {
        return true;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Ack Response: %s", resp);
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEAFFocus::updateInfo()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16] = {0};
    char joedeviceinfo[16] = {0};
    int ieafpos, ieafmove, ieaftemp, ieafdir;

    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":FI#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateInfo error: %s.", errstr);
    }
    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateInfo  error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read] = '\0';

    sscanf(resp, "%14s", joedeviceinfo);
    rc = sscanf(resp, "%7d%1d%5d%1d", &ieafpos, &ieafmove, &ieaftemp, &ieafdir);

    if (rc != 4)
    {
        LOGF_ERROR("Could not parse response %s", resp);
        return false;
    }

    m_isMoving = ieafmove == 1;
    auto temperature = ieaftemp / 100.0 - 273.15;
    auto reversed = (ieafdir == 0);
    auto currentlyReversed = FocusReverseS[INDI_ENABLED].s == ISS_ON;

    // Only update if there is change
    if (std::abs(temperature - TemperatureNP[0].getValue()) > TEMPERATURE_THRESHOLD)
    {
        TemperatureNP[0].setValue(temperature);
        TemperatureNP.apply();
    }

    // Only update if there is change
    if (reversed != currentlyReversed)
    {
        FocusReverseS[INDI_ENABLED].s = reversed ? ISS_ON : ISS_OFF;
        FocusReverseS[INDI_DISABLED].s = reversed ? ISS_OFF : ISS_ON;
        FocusReverseSP.s = IPS_OK;
        IDSetSwitch(&FocusReverseSP, nullptr);
    }

    // If absolute position is different, let's update
    if (ieafpos != FocusAbsPosN[0].value)
    {
        FocusAbsPosN[0].value = ieafpos;
        // Check if we are busy or not.
        if ((m_isMoving == false && FocusAbsPosNP.s == IPS_BUSY) || (m_isMoving && FocusAbsPosNP.s != IPS_BUSY))
        {
            FocusAbsPosNP.s = m_isMoving ? IPS_BUSY : IPS_OK;
            FocusRelPosNP.s = m_isMoving ? IPS_BUSY : IPS_OK;
            IDSetNumber(&FocusRelPosNP, nullptr);
        }
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }
    // Update status if required
    else  if ((m_isMoving == false && FocusAbsPosNP.s == IPS_BUSY) || (m_isMoving && FocusAbsPosNP.s != IPS_BUSY))
    {
        FocusAbsPosNP.s = m_isMoving ? IPS_BUSY : IPS_OK;
        FocusRelPosNP.s = m_isMoving ? IPS_BUSY : IPS_OK;
        IDSetNumber(&FocusRelPosNP, nullptr);
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEAFFocus::SetFocuserMaxPosition(uint32_t ticks)
{
    INDI_UNUSED(ticks);
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEAFFocus::MoveMyFocuser(uint32_t position)
{
    int nbytes_written = 0, rc = -1;
    char cmd[12] = {0};

    snprintf(cmd, 12, ":FM%7u#", position);

    // Set Position
    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setPosition error: %s.", errstr);
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEAFFocus::ReverseFocuser(bool enabled)
{
    int nbytes_written = 0, rc = -1;

    // If there is no change, return.
    if ((enabled && FocusReverseS[INDI_ENABLED].s == ISS_ON) || (!enabled && FocusReverseS[INDI_DISABLED].s == ISS_ON))
        return true;

    // Change Direction
    if ( (rc = tty_write(PortFD, ":FR#", 4, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "change Direction error: %s.", errstr);
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void iEAFFocus::setZero()
{
    int nbytes_written = 0, rc = -1;
    // Set Zero
    if ( (rc = tty_write(PortFD, ":FZ#", 4, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "set Zero error: %s.", errstr);
        return;
    }
    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEAFFocus::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (SetZeroSP.isNameMatch(name))
        {
            setZero();
            SetZeroSP.setState(IPS_OK);
            SetZeroSP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void iEAFFocus::GetFocusParams ()
{
    updateInfo();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState iEAFFocus::MoveAbsFocuser(uint32_t targetTicks)
{
    uint32_t targetPos = targetTicks;

    bool rc = false;

    rc = MoveMyFocuser(targetPos);

    if (rc == false)
        return IPS_ALERT;

    FocusAbsPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
IPState iEAFFocus::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (IUFindOnSwitchIndex(&FocusReverseSP) == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    uint32_t newPosition = FocusAbsPosN[0].value + relativeTicks;
    newPosition = std::max(0u, std::min(static_cast<uint32_t>(FocusAbsPosN[0].max), newPosition));

    auto rc = MoveMyFocuser(newPosition);

    if (rc == false)
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void iEAFFocus::TimerHit()
{
    if (!isConnected())
        return;

    updateInfo();
    SetTimer(getPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool iEAFFocus::AbortFocuser()
{
    int nbytes_written;
    if (tty_write(PortFD, ":FQ#", 4, &nbytes_written) == TTY_OK)
    {
        FocusAbsPosNP.s = IPS_IDLE;
        FocusRelPosNP.s = IPS_IDLE;
        IDSetNumber(&FocusAbsPosNP, NULL);
        IDSetNumber(&FocusRelPosNP, NULL);
        return true;
    }
    else
        return false;
}
