/*
    Pulsar 2 INDI driver

    Copyright (C) 2015 Jasem Mutlaq

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

#include <math.h>
#include <unistd.h>
#include <termios.h>

#include "indicom.h"
#include "lx200pulsar2.h"
#include "lx200driver.h"

#define PULSAR2_BUF             32
#define PULSAR2_TIMEOUT         3

LX200Pulsar2::LX200Pulsar2()
{
    setVersion(1, 0);
}

bool LX200Pulsar2::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        // Delete unsupported properties
        deleteProperty(AlignmentSP.name);
        deleteProperty(FocusMotionSP.name);
        deleteProperty(FocusTimerNP.name);
        deleteProperty(FocusModeSP.name);
        deleteProperty(SiteSP.name);
        deleteProperty(SiteNameTP.name);
    }

    return true;
}

const char * LX200Pulsar2::getDefaultName()
{
    return (char *)"Pulsar2";
}

bool LX200Pulsar2::checkConnection()
{
    char initCMD[] = "#:YV#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[PULSAR2_BUF];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "Initializing Pulsar2 using %s CMD...", initCMD);

    for (int i=0; i < 2; i++)
    {
        if ( (errcode = tty_write(PortFD, initCMD, strlen(initCMD), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            usleep(50000);
            continue;
        }

        if ( (errcode = tty_read_section(PortFD, response, '#', PULSAR2_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            usleep(50000);
            continue;
        }

        if (nbytes_read > 0)
        {
            response[nbytes_read] = '\0';
            DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

            DEBUGF(INDI::Logger::DBG_SESSION, "Detected %s", response);

            return true;
        }

        usleep(50000);
    }

    return false;
}

bool LX200Pulsar2::isSlewComplete()
{
    char cmd[PULSAR2_BUF];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[2];
    int nbytes_read=0;
    int nbytes_written=0;

    strncpy(cmd, "#:YGi#", PULSAR2_BUF);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if ( (errcode = tty_read(PortFD, response, 1, PULSAR2_TIMEOUT, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(PortFD, TCIFLUSH);

        if (response[0] == '0')
            return true;
        else
            return false;
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

void LX200Pulsar2::getBasicData()
{
    // TODO
}
