/*
    ZEQ25 INDI driver

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
#include "lx200zeq25.h"

LX200ZEQ25::LX200ZEQ25()
{
    setVersion(1, 0);
}

bool LX200ZEQ25::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        getMountInfo();

        // Delete unsupported properties
        deleteProperty(AlignmentSP.name);
        deleteProperty(FocusMotionSP.name);
        deleteProperty(FocusTimerNP.name);
        deleteProperty(FocusModeSP.name);
    }

    return true;
}

const char * LX200ZEQ25::getDefaultName()
{
    return (char *)"ZEQ25";
}

bool LX200ZEQ25::checkConnection()
{
    char initCMD[] = ":V#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUG(INDI::Logger::DBG_DEBUG, "Initializing IOptron using :V# CMD...");

    for (int i=0; i < 2; i++)
    {
        if ( (errcode = tty_write(PortFD, initCMD, 3, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            usleep(50000);
            continue;
        }

        if ( (errcode = tty_read_section(PortFD, response, '#', 3, &nbytes_read)))
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

            if (!strcmp(response, "V1.00#"))
                return true;
        }

        usleep(50000);
    }

    return false;
}

bool LX200ZEQ25::isSlewComplete()
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read=0;
    int nbytes_written=0;

    strncpy(cmd, ":SE#", 16);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if ( (errcode = tty_write(PortFD, cmd, 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if ( (errcode = tty_read(PortFD, response, 1, 3, &nbytes_read)))
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

        if (response[0] == '1')
            return true;
        else
            return false;
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool LX200ZEQ25::getMountInfo()
{
    char cmd[] = ":MountInfo#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if ( (errcode = tty_read(PortFD, response, 4, 3, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        if (nbytes_read == 4)
        {
            if (!strcmp(response, "8407"))
                DEBUG(INDI::Logger::DBG_SESSION, "Detected iEQ45/iEQ30 Mount.");
            else if (!strcmp(response, "8497"))
                DEBUG(INDI::Logger::DBG_SESSION, "Detected iEQ45 AA Mount.");
            else if (!strcmp(response, "8408"))
                DEBUG(INDI::Logger::DBG_SESSION, "Detected ZEQ25 Mount.");
            else if (!strcmp(response, "8498"))
                DEBUG(INDI::Logger::DBG_SESSION, "Detected SmartEQ Mount.");
            else
                DEBUG(INDI::Logger::DBG_SESSION, "Unknown mount detected.");

            tcflush(PortFD, TCIFLUSH);

            return true;
        }
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 4.", nbytes_read);
    return false;

}

