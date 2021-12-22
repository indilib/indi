/*******************************************************************************
 Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Simple SkyCommander DSC Driver

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

#include "skycommander.h"

#include "indicom.h"

#include <memory>
#include <termios.h>

#define SKYCOMMANDER_TIMEOUT 3

// We declare an auto pointer to SkyCommander.
std::unique_ptr<SkyCommander> skycommander(new SkyCommander());

SkyCommander::SkyCommander()
{
    SetTelescopeCapability(0, 0);
}

const char *SkyCommander::getDefaultName()
{
    return (const char *)"SkyCommander";
}

bool SkyCommander::Handshake()
{
    return true;
}

bool SkyCommander::ReadScopeStatus()
{
    char CR[1] = { 0x0D };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: %#02X", CR[0]);

    tcflush(PortFD, TCIFLUSH);

    if ((rc = tty_write(PortFD, CR, 1, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to SkyCommander %s (%d)", errmsg, rc);
        return false;
    }

    char coords[16];
    if ((rc = tty_read(PortFD, coords, 16, SKYCOMMANDER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from SkyCommander %s (%d)", errmsg, rc);
        return false;
    }

    LOGF_DEBUG("RES: %s", coords);

    float RA = 0.0, DEC = 0.0;
    nbytes_read = sscanf(coords, " %g %g", &RA, &DEC);

    if (nbytes_read < 2)
    {
        LOGF_ERROR("Error in Sky commander number format (%s).", coords);
        return false;
    }

    char RAStr[64], DecStr[64];
    fs_sexa(RAStr, RA, 2, 3600);
    fs_sexa(DecStr, DEC, 2, 3600);
    LOGF_DEBUG("Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(RA, DEC);
    return true;
}
