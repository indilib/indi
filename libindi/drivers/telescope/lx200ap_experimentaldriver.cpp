#if 0
LX200 Astro- Physics Driver
Copyright (C) 2007 Markus Wildi

This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

#endif


// This file contains functions which require the 'V' firmware level and will NOT work with previous versions!
// Used only by the lx200ap_experimental driver in conjuction with the routines in lx200ap_driver.cpp which work
// with all firmware versions


#include <cmath>
//#include "lx200apdriver.h"

#include "indicom.h"
#include "indilogger.h"
#include "lx200ap_experimentaldriver.h"

#include <cstring>
#include <unistd.h>

#ifndef _WIN32
#include <termios.h>
#endif

#define LX200_TIMEOUT 5 /* FD timeout in seconds */

char lx200ap_exp_name[MAXINDIDEVICE];
unsigned int AP_EXP_DBG_SCOPE;

void set_lx200ap_exp_name(const char *deviceName, unsigned int debug_level)
{
    strncpy(lx200ap_exp_name, deviceName, MAXINDIDEVICE);
    AP_EXP_DBG_SCOPE = debug_level;
}


// experimental functions!!!

int setAPMeridianDelay(int fd, double mdelay)
{
    char cmd[32];
    char hourstr[16];
    int nbytes_write = 0;

    DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "<%s>", __FUNCTION__);

    if (mdelay < 0)
    {
        DEBUGFDEVICE(lx200ap_exp_name, INDI::Logger::DBG_ERROR, "Meridian delay < 0 not supp\
                     orted! mdelay=%f", mdelay);
                return -1;
    }

    // convert from decimal hours to format for command
    if (fs_sexa(hourstr, mdelay, 2, 3600) < 0)
    {
        DEBUGFDEVICE(lx200ap_exp_name, INDI::Logger::DBG_ERROR, "Unable to format meridian d\
                     elay %f to time format!", mdelay);
                     return -1;
    }

    DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "Meridian Delay %f -> %s", mdelay, hourstr)\
            ;

    sprintf(cmd, ":SM%s#", hourstr);

    DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "CMD <%s>", cmd);

    tty_write_string(fd, cmd, &nbytes_write);

    tcflush(fd, TCIFLUSH);

    return 0;
}

int getAPMeridianDelay(int fd, double *mdelay)
{
    int error_type;
    int nbytes_write = 0;
    int nbytes_read  = 0;
    char temp_string[16];

    DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "<%s>", __FUNCTION__);

    DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "CMD <%s>", "#:GM#");

    if ((error_type = tty_write_string(fd, "#:GM#", &nbytes_write)) != TTY_OK)
        return error_type;

    if ((error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read\
                                       )) != TTY_OK)
    {
        DEBUGFDEVICE(lx200ap_exp_name, INDI::Logger::DBG_ERROR, "getAPMeridianDelay: error %\
                     d, %d", error_type,
                     nbytes_read);
        return error_type;
    }

    tcflush(fd, TCIFLUSH);

    DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "RES <%s>", temp_string);

    if (f_scansexa(temp_string, mdelay))
    {
        DEBUGFDEVICE(lx200ap_exp_name, INDI::Logger::DBG_ERROR, "getAPMeridianDelay: unable \
                     to process %s", temp_string);
                     return -1;
    }

    return 0;
}




int check_lx200ap_status(int fd, char *parkStatus, char *slewStatus)
{
    char temp_string[64];
    int error_type;
    int nbytes_write = 0;
    int nbytes_read  = 0;

    DEBUGDEVICE(lx200ap_exp_name, INDI::Logger::DBG_DEBUG, "EXPERIMENTAL: check status...");

    if (fd <= 0)
    {
        DEBUGDEVICE(lx200ap_exp_name, INDI::Logger::DBG_ERROR,
                    "check_lx200ap_connection: not a valid file descriptor received");

        return -1;
    }

    if ((error_type = tty_write_string(fd, "#:GOS#", &nbytes_write)) != TTY_OK)
    {
        DEBUGFDEVICE(lx200ap_exp_name, INDI::Logger::DBG_ERROR,
                     "check_lx200ap_connection: unsuccessful write to telescope, %d", nbytes_write);

        return error_type;
    }
    tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);
    if (nbytes_read > 1)
    {
        temp_string[nbytes_read - 1] = '\0';

        DEBUGFDEVICE(lx200ap_exp_name, INDI::Logger::DBG_DEBUG, "check_lx200ap_status: received bytes %d, [%s]",
                     nbytes_write, temp_string);

        *parkStatus = temp_string[0];
        *slewStatus = temp_string[3];

        return 0;
    }


    DEBUGDEVICE(lx200ap_exp_name, INDI::Logger::DBG_ERROR, "check_lx200ap_status: wrote, but nothing received.");

    return -1;
}
