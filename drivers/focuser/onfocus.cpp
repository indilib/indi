/*
    OnFocus Focuser
    Copyright (C) 2018 Paul de Backer (74.0632@gmail.com)

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

#include "onfocus.h"
#include "indicom.h"

#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <memory>

#define ONFOCUS_TIMEOUT 4

#define POLLMS_OVERRIDE  1500

std::unique_ptr<OnFocus> onFocus(new OnFocus());

OnFocus::OnFocus()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
    lastPos = 0;
}

OnFocus::~OnFocus()
{
}

bool OnFocus::initProperties()
{
    INDI::Focuser::initProperties();
    // SetZero
    IUFillSwitch(&SetZeroS[0], "SETZERO", "Set Current Position to 0", ISS_OFF);
    IUFillSwitchVector(&SetZeroSP, SetZeroS, 1, getDeviceName(), "Zero Position", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);
    // Maximum Travel
    IUFillNumber(&MaxPosN[0], "MAXPOS", "Maximum Out Position", "%8.0f", 1., 10000000., 0, 0);
    IUFillNumberVector(&MaxPosNP, MaxPosN, 1, getDeviceName(), "FOCUS_MAXPOS", "Position", OPTIONS_TAB, IP_RW, 0, IPS_IDLE );


    /* Relative and absolute movement */
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(200.);
    FocusRelPosNP[0].setValue(0.);
    FocusRelPosNP[0].setStep(10.);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(10000000.);
    FocusAbsPosNP[0].setValue(0.);
    FocusAbsPosNP[0].setStep(500.);

    addDebugControl();

    return true;

}

bool OnFocus::updateProperties()
{
    INDI::Focuser::updateProperties();
    if (isConnected())
    {
        defineProperty(&MaxPosNP);
        defineProperty(&SetZeroSP);
        GetFocusParams();
        loadConfig(true);

        DEBUG(INDI::Logger::DBG_SESSION, "OnFocus parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(MaxPosNP.name);
        deleteProperty(SetZeroSP.name);
    }

    return true;

}

bool OnFocus::Handshake()
{
    if (Ack())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "OnFocus is online. Getting focus parameters...");
        return true;
    }
    return false;
}

const char * OnFocus::getDefaultName()
{
    return "OnFocus";
}

bool OnFocus::Ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16];
    sleep(2);
    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, ":IP#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Init error: %s.", errstr);
        return false;
    }
    if ( (rc = tty_read(PortFD, resp, 9, ONFOCUS_TIMEOUT * 2, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Init error: %s.", errstr);
        return false;
    }
    tcflush(PortFD, TCIOFLUSH);
    resp[nbytes_read] = '\0';
    if (!strcmp(resp, "On-Focus#"))
    {
        return true;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Ack Response: %s", resp);
        return false;
    }
}


bool OnFocus::updatePosition()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16];
    int pos = -1;

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, ":GP#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePosition error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', ONFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updatePosition error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read] = '\0';

    rc = sscanf(resp, "%d#", &pos);

    if (rc > 0)
    {
        FocusAbsPosNP[0].setValue(pos);
        FocusAbsPosNP.apply();

    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser position value (%s)", resp);
        return false;
    }

    return true;
}

bool OnFocus::updateMaxPos()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16];
    long maxposition;

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, ":GM#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateMaxPosition error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', ONFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateMaxPosition error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);
    resp[nbytes_read] = '\0';

    rc = sscanf(resp, "%ld#", &maxposition);

    if (rc > 0)
    {

        MaxPosN[0].value = maxposition;
        FocusAbsPosNP[0].setMax(maxposition);
        FocusAbsPosNP.apply();
        IDSetNumber(&MaxPosNP, NULL);
        return true;
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: focuser maxposition (%s)", resp);
    return false;
}

bool OnFocus::isMoving()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16];

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, ":IS#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "isMoving error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', ONFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "isMoving error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read] = '\0';
    if (!strcmp(resp, "M#"))
        return true;
    else if (!strcmp(resp, "S#"))
        return false;
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Unknown error: isMoving value (%s)", resp);
        return false;
    }

}

bool OnFocus::MoveMyFocuser(uint32_t position)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[24];

    snprintf(cmd, 24, ":MA%d#", position);

    // Set Position
    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setPosition error: %s.", errstr);
        return false;
    }
    return true;
}

void OnFocus::setZero()
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    // Set Zero
    if ( (rc = tty_write(PortFD, ":SZ#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "set Zero error: %s.", errstr);
        return;
    }
    updateMaxPos();
    return;
}


bool OnFocus::setMaxPos(uint32_t maxPos)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[24];

    snprintf(cmd, 24, ":SM%d#", maxPos);

    // Set Max Out
    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "setPosition error: %s.", errstr);
        return false;
    }
    updateMaxPos();
    return true;
}

bool OnFocus::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(SetZeroSP.name, name))
        {
            setZero();
            IUResetSwitch(&SetZeroSP);
            SetZeroSP.s = IPS_OK;
            IDSetSwitch(&SetZeroSP, NULL);
            return true;
        }

        return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
    }
    return false;
}

bool OnFocus::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp (name, MaxPosNP.name))
        {
            if (values[0] < FocusAbsPosNP[0].getValue())
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "Can't set max position lower than current absolute position ( %8.0f )",
                       FocusAbsPosNP[0].getValue());
                return false;
            }
            IUUpdateNumber(&MaxPosNP, values, names, n);
            FocusAbsPosNP[0].setMax(MaxPosN[0].value);
            setMaxPos(MaxPosN[0].value);
            MaxPosNP.s = IPS_OK;
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

}

void OnFocus::GetFocusParams ()
{
    updatePosition();
    updateMaxPos();
}


IPState OnFocus::MoveAbsFocuser(uint32_t targetTicks)
{
    uint32_t targetPos = targetTicks;

    bool rc = false;

    rc = MoveMyFocuser(targetPos);

    if (rc == false)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState OnFocus::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t newPosition = 0;
    bool rc = false;

    if (dir == FOCUS_INWARD)
        newPosition = uint32_t(FocusAbsPosNP[0].getValue()) - ticks;
    else
        newPosition = uint32_t(FocusAbsPosNP[0].getValue()) + ticks;

    rc = MoveMyFocuser(newPosition);

    if (rc == false)
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

void OnFocus::TimerHit()
{
    if (isConnected() == false)
    {
        SetTimer(POLLMS_OVERRIDE);
        return;
    }

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > 5)
        {
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
        }
    }


    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (isMoving() == false)
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
            DEBUG(INDI::Logger::DBG_SESSION, "Focuser reached requested position.");
        }
    }
    SetTimer(POLLMS_OVERRIDE);

}

bool OnFocus::AbortFocuser()
{
    int nbytes_written;
    if (tty_write(PortFD, ":MH#", 4, &nbytes_written) == TTY_OK)
    {
        FocusAbsPosNP.setState(IPS_IDLE);
        FocusRelPosNP.setState(IPS_IDLE);
        FocusAbsPosNP.apply();
        FocusRelPosNP.apply();
        return true;
    }
    else
        return false;
}


