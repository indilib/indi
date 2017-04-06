/*
    Losmandy Gemini INDI driver

    Copyright (C) 2017 Jasem Mutlaq

    Difference from LX200 Generic:

    1. Added Side of Pier
    2. Reimplemented isSlewComplete to use :Gv# since it is more reliable

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
#include <termios.h>
#include "lx200gemini.h"

LX200Gemini::LX200Gemini()
{
    setVersion(1, 1);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE,4);
}

const char * LX200Gemini::getDefaultName()
{
    return (char *)"Losmandy Gemini";
}

bool LX200Gemini::isSlewComplete()
{
    // Send ':Gv#'
    const char * cmd = ":Gv#";
    // Response
    char response[2] = {0};
    int rc=0, nbytes_read=0, nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ( (rc = tty_write(PortFD, cmd, 4, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    // Read 1 character
    if ( (rc = tty_read(PortFD, response, 1, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error reading from device %s (%d)", errmsg, rc);
        return false;
    }

    response[1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", response);

    if (response[0] == 'T' || response[0] == 'G' || response[0] == 'N')
        return true;
    else
        return false;
}

bool LX200Gemini::ReadScopeStatus()
{
   syncSideOfPier();

   return LX200Generic::ReadScopeStatus();
}

void LX200Gemini::syncSideOfPier()
{
    // Send ':Gv#'
    const char * cmd = ":Gm#";
    // Response
    char response[2] = {0};
    int rc=0, nbytes_read=0, nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %s", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ( (rc = tty_write(PortFD, cmd, 4, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return;
    }

    // Read 1 character
    if ( (rc = tty_read(PortFD, response, 1, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error reading from device %s (%d)", errmsg, rc);
        return;
    }

    response[1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", response);

    setPierSide(response[0] == 'E' ? INDI::Telescope::PIER_EAST : INDI::Telescope::PIER_WEST);
}
