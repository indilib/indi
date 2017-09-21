/*
    Optec Pyrix Rotator
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "pyrix.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>

#define PYRIX_TIMEOUT 3

#define POLLMS 500
#define SETTINGS_TAB "Settings"

std::unique_ptr<Pyrix> pyrix(new Pyrix());

void ISGetProperties(const char *dev)
{
    pyrix->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    pyrix->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int n)
{
    pyrix->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    pyrix->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                char *formats[], char *names[], int n)
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

void ISSnoopDevice (XMLEle *root)
{
    pyrix->ISSnoopDevice(root);
}

Pyrix::Pyrix()
{
    // We do not have absolute ticks
    SetRotatorCapability(ROTATOR_CAN_HOME | ROTATOR_CAN_SYNC | ROTATOR_CAN_REVERSE);

    setRotatorConnection(CONNECTION_SERIAL);
}

bool Pyrix::initProperties()
{
    INDI::Rotator::initProperties();

    updatePeriodMS = POLLMS;

    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    return true;
}

bool Pyrix::Handshake()
{
    if (Ack())
        return true;

    DEBUG(INDI::Logger::DBG_SESSION, "Error retreiving data from Pyrix, please ensure Pyrix controller is powered and the port is correct.");
    return false;
}

const char * Pyrix::getDefaultName()
{
    return "Pyrix";
}

bool Pyrix::Ack()
{
    return false;
}

#if 0
bool Pyrix::gotoMotor(MotorType type, int32_t position)
{
    char cmd[16] = {0};
    char res[16] = {0};

    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    snprintf(cmd, 16, "%dSN %d#", type+1, position);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s: %s.", __FUNCTION__, errstr);
        return false;
    }

    if ( (rc = tty_read(PortFD, res, 1, Pyrix_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s error: %s.", __FUNCTION__, errstr);
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES <%s>", res);

    return startMotor(type);
}
#endif

IPState Pyrix::HomeRotator()
{
    return IPS_ALERT;
}

IPState Pyrix::MoveRotator(double angle)
{
    INDI_UNUSED(angle);
    return IPS_ALERT;
}

bool Pyrix::SyncRotator(double angle)
{
    INDI_UNUSED(angle);
    return false;
}

bool Pyrix::ReverseRotator(bool enabled)
{
    INDI_UNUSED(enabled);
    return false;
}

void Pyrix::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(POLLMS);
        return;
    }
}
