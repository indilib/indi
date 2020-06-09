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

// Timeout in seconds
#define LX200_TIMEOUT 5

static char lx200ap_exp_name[MAXINDIDEVICE];
static uint32_t AP_EXP_DBG_SCOPE;

void set_lx200ap_exp_name(const char *deviceName, unsigned int debug_level)
{
    strncpy(lx200ap_exp_name, deviceName, MAXINDIDEVICE);
    AP_EXP_DBG_SCOPE = debug_level;
}

// make this a function with logging instead of a #define like in legacy driver
int APParkMount(int fd)
{
    int error_type;
    int nbytes_write = 0;

    DEBUGDEVICE(lx200ap_exp_name, INDI::Logger::DBG_DEBUG, "APParkMount: Sending park command.");
    DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "CMD <%s>", "#:KA");

    if ((error_type = tty_write_string(fd, "#:KA", &nbytes_write)) != TTY_OK)
        return error_type;

    return 0;
}

// make this a function with logging instead of a #define like in legacy driver
int APUnParkMount(int fd)
{
    int error_type;
    int nbytes_write = 0;

    DEBUGDEVICE(lx200ap_exp_name, INDI::Logger::DBG_DEBUG, "APUnParkMount: Sending unpark command.");
    DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "CMD <%s>", "#:PO");

    if ((error_type = tty_write_string(fd, "#:PO", &nbytes_write)) != TTY_OK)
        return error_type;

    return 0;
}

// This is a modified version of selectAPMoveRate() from lx200apdriver.cpp
// This version allows changing the rate to GUIDE as well as 12x/64x/600x/1200x
// and is required some the experimental AP driver properly handles
// pulse guide requests over 999ms by simulated it by setting the move rate
// to GUIDE and then starting and halting a move of the correct duration.
int selectAPCenterRate(int fd, int centerRate)
{
    int error_type;
    int nbytes_write = 0;

    switch (centerRate)
    {
    /* Guide */
    case 0:
        DEBUGDEVICE(lx200ap_exp_name, INDI::Logger::DBG_DEBUG, "selectAPMoveToRate: Setting move to rate to GUIDE");
        DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "CMD <%s>", "#:RG#");

        if ((error_type = tty_write_string(fd, "#:RG#", &nbytes_write)) != TTY_OK)
            return error_type;
        break;

    /* 12x */
    case 1:
        DEBUGDEVICE(lx200ap_exp_name, INDI::Logger::DBG_DEBUG, "selectAPMoveToRate: Setting move to rate to 12x");
        DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "CMD <%s>", "#:RC0#");

        if ((error_type = tty_write_string(fd, "#:RC0#", &nbytes_write)) != TTY_OK)
            return error_type;
        break;

    /* 64x */
    case 2:
        DEBUGDEVICE(lx200ap_exp_name, INDI::Logger::DBG_DEBUG, "selectAPMoveToRate: Setting move to rate to 64x");
        DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "CMD <%s>", "#:RC1#");

        if ((error_type = tty_write_string(fd, "#:RC1#", &nbytes_write)) != TTY_OK)
            return error_type;
        break;

    /* 600x */
    case 3:
        DEBUGDEVICE(lx200ap_exp_name, INDI::Logger::DBG_DEBUG, "selectAPMoveToRate: Setting move to rate to 600x");
        DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "CMD <%s>", "#:RC2#");
        if ((error_type = tty_write_string(fd, "#:RC2#", &nbytes_write)) != TTY_OK)
            return error_type;
        break;

    /* 1200x */
    case 4:
        DEBUGDEVICE(lx200ap_exp_name, INDI::Logger::DBG_DEBUG, "selectAPMoveToRate: Setting move to rate to 1200x");
        DEBUGFDEVICE(lx200ap_exp_name, AP_EXP_DBG_SCOPE, "CMD <%s>", "#:RC3#");

        if ((error_type = tty_write_string(fd, "#:RC3#", &nbytes_write)) != TTY_OK)
            return error_type;
        break;

    default:
        return -1;
        break;
    }
    return 0;
}

// experimental functions!!!


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
