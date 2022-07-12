#if 0
LX200 Astro - Physics Driver
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

#include <cmath>
#include "lx200apdriver.h"

#include "indicom.h"
#include "indilogger.h"
#include "lx200driver.h"

#include <cstring>
#include <unistd.h>

#ifndef _WIN32
#include <termios.h>
#endif

#define LX200_TIMEOUT 5 /* FD timeout in seconds */

// maximum guide pulse request to send to controller
#define MAX_LX200AP_PULSE_LEN 999

char lx200ap_name[MAXINDIDEVICE];
unsigned int AP_DBG_SCOPE;

void set_lx200ap_name(const char *deviceName, unsigned int debug_level)
{
    strncpy(lx200ap_name, deviceName, MAXINDIDEVICE);
    AP_DBG_SCOPE = debug_level;
}

int check_lx200ap_connection(int fd)
{
    const struct timespec timeout = {0, 50000000L};
    int i = 0;
    char temp_string[256];
    int error_type;
    int nbytes_write = 0;
    int nbytes_read  = 0;

    DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "Testing telescope's connection using #:GG#...");

    if (fd <= 0)
    {
        DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR,
                    "check_lx200ap_connection: not a valid file descriptor received");

        return -1;
    }
    for (i = 0; i < 2; i++)
    {
        // This is the command to get the UTC offset. Used as a connection test.
        if ((error_type = tty_write_string(fd, "#:GG#", &nbytes_write)) != TTY_OK)
        {
            DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR,
                         "check_lx200ap_connection: unsuccessful write to telescope, %d", nbytes_write);

            return error_type;
        }
        tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read);
        tcflush(fd, TCIFLUSH);
        if (nbytes_read > 1)
        {
            temp_string[nbytes_read - 1] = '\0';

            DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "check_lx200ap_connection: received bytes %d, [%s]",
                         nbytes_write, temp_string);

            return 0;
        }
        nanosleep(&timeout, nullptr);
    }

    DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "check_lx200ap_connection: wrote, but nothing received.");

    return -1;
}

// get UTC offset.
int getAPUTCOffset(int fd, double *value)
{
    int error_type;
    int nbytes_write = 0;
    int nbytes_read  = 0;

    char temp_string[256];
    temp_string[0] = 0;
    temp_string[1] = 0;

    const char *cmd = "#:GG#";
    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    if ((error_type = tty_read_section(fd, temp_string, '#', LX200_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "getAPUTCOffset: saying good bye %d, %d", error_type,
                     nbytes_read);
        return error_type;
    }

    tcflush(fd, TCIFLUSH);

    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "RES <%s>", temp_string);

    /* Negative offsets, see AP keypad manual p. 77 */
    if ((temp_string[0] == 'A') || ((temp_string[0] == '0') && (temp_string[1] == '0')) || (temp_string[0] == '@'))
    {
        int i;
        for (i = nbytes_read; i > 0; i--)
        {
            temp_string[i] = temp_string[i - 1];
        }
        temp_string[0]               = '-';
        temp_string[nbytes_read + 1] = '\0';

        if (temp_string[1] == 'A')
        {
            temp_string[1] = '0';
            switch (temp_string[2])
            {
                case '5':

                    temp_string[2] = '1';
                    break;
                case '4':

                    temp_string[2] = '2';
                    break;
                case '3':

                    temp_string[2] = '3';
                    break;
                case '2':

                    temp_string[2] = '4';
                    break;
                case '1':

                    temp_string[2] = '5';
                    break;
                default:
                    DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "getAPUTCOffset: string not handled %s",
                                 temp_string);
                    return -1;
                    break;
            }
        }
        else if (temp_string[1] == '0')
        {
            temp_string[1] = '0';
            temp_string[2] = '6';
        }
        else if (temp_string[1] == '@')
        {
            temp_string[1] = '0';
            switch (temp_string[2])
            {
                case '9':

                    temp_string[2] = '7';
                    break;
                case '8':

                    temp_string[2] = '8';
                    break;
                case '7':

                    temp_string[2] = '9';
                    break;
                case '6':

                    temp_string[2] = '0';
                    break;
                case '5':
                    temp_string[1] = '1';
                    temp_string[2] = '1';
                    break;
                case '4':

                    temp_string[1] = '1';
                    temp_string[2] = '2';
                    break;
                default:
                    DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "getAPUTCOffset: string not handled %s",
                                 temp_string);
                    return -1;
                    break;
            }
        }
        else
        {
            DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "getAPUTCOffset: string not handled %s", temp_string);
        }
    }
    else
    {
        temp_string[nbytes_read - 1] = '\0';
    }

    if (f_scansexa(temp_string, value))
    {
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "getAPUTCOffset: unable to process %s", temp_string);
        return -1;
    }
    return 0;
}

int setAPObjectAZ(int fd, double az)
{
    int h, m, s;
    char cmd[256];

    // The azimuth should be 0-360.
    while (az < 0) az += 360.0;
    while (az > 360.0) az -= 360.0;

    getSexComponents(az, &h, &m, &s);

    snprintf(cmd, sizeof(cmd), "#:Sz %03d*%02d:%02d#", h, m, s);

    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    return (setStandardProcedure(fd, cmd));
}

/* wildi Valid set Values are positive, add error condition */

int setAPObjectAlt(int fd, double alt)
{
    int d, m, s;
    char cmd[256];

    getSexComponents(alt, &d, &m, &s);
    if (d < 0) d = -d;
    snprintf(cmd, sizeof(cmd), "#:Sa %s%02d*%02d:%02d#",
             alt >= 0 ? "+" : "-", d, m, s);

    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    return (setStandardProcedure(fd, cmd));
}

// Set the UTC offset.
// Previously this only set positive offsets.
// Added the sign according to the doc in https://astro-physics.info/tech_support/mounts/protocol-cp3-cp4.pdf
int setAPUTCOffset(int fd, double hours)
{
    int h, m, s;

    char cmd[256];

    getSexComponents(hours, &h, &m, &s);
    if (h < 0) h = -h;
    snprintf(cmd, sizeof(cmd), "#:SG %s%02d:%02d:%02d#",
             hours >= 0 ? "+" : "-", h, m, s);

    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    return (setStandardProcedure(fd, cmd));
}
int APSyncCM(int fd, char *matchedObject)
{
    const struct timespec timeout = {0, 10000000L};
    int error_type;
    int nbytes_write = 0;
    int nbytes_read  = 0;

    const char *cmd = "#:CM#";
    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    if ((error_type = tty_read_section(fd, matchedObject, '#', LX200_TIMEOUT, &nbytes_read)) != TTY_OK)
        return error_type;

    matchedObject[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "RES <%s>", matchedObject);

    /* Sleep 10ms before flushing. This solves some issues with LX200 compatible devices. */
    nanosleep(&timeout, nullptr);

    tcflush(fd, TCIFLUSH);

    return 0;
}

int APSyncCMR(int fd, char *matchedObject)
{
    const struct timespec timeout = {0, 10000000L};
    int error_type;
    int nbytes_write = 0;
    int nbytes_read  = 0;

    const char *cmd = "#:CMR#";
    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    if ((error_type = tty_read_section(fd, matchedObject, '#', LX200_TIMEOUT, &nbytes_read)) != TTY_OK)
        return error_type;

    matchedObject[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "RES <%s>", matchedObject);

    /* Sleep 10ms before flushing. This solves some issues with LX200 compatible devices. */
    nanosleep(&timeout, nullptr);

    tcflush(fd, TCIFLUSH);

    return 0;
}


int sendAPCommand(int fd, const char *cmd, const char *comment)
{
    int error_type;
    int nbytes_write = 0;

    DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, comment);
    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;
    return 0;
}

int selectAPPECState(int fd, int pecstate)
{
    switch (pecstate)
    {
        case AP_PEC_OFF:
            return sendAPCommand(fd, "#:p#", "selectAPPECState: Setting PEC OFF");
        case AP_PEC_ON:
            return sendAPCommand(fd, "#:pP#", "selectAPPECState: Setting PEC ON");
        case AP_PEC_RECORD:
            return sendAPCommand(fd, "#:pR#", "selectAPPECState: Enabling PEC RECORD");
        default:
            return -1;
    }

    return 0;
}


// Should return a number between 0 and 969 inclusive.
// It is a "normalized worm position", normalized to the number of PEM datapoints per revolution.
// It is based on the gear angle and remembered through a power cycle.
int getAPWormPosition(int fd, int *position)
{
    int nbytes_read  = 0;
    char response[128];

    int res = sendAPCommand(fd, "#:Gp#", "getAWormPosition");
    if (res != TTY_OK)
    {
        DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "getAPWormPosition: write failed.");
        return res;
    }

    res = tty_read_section(fd, response, '#', LX200_TIMEOUT, &nbytes_read);
    if (res != TTY_OK)
    {
        // This does happen occasionally, not sure why, but isn't critical.
        // DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "getAPWormPosition: read failed.");
        return res;
    }

    tcflush(fd, TCIFLUSH);
    if (nbytes_read > 1)
    {
        response[nbytes_read - 1] = '\0';
        response[3] = '\0';
        sscanf(response, "%d", position);
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "getAPWormPosition: response: %d", *position);
        return TTY_OK;
    }
    DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "getAPWormPosition: wrote, but bad response.");
    return -1;
}

int selectAPMoveToRate(int fd, int moveToRate)
{
    switch (moveToRate)
    {
        case 0:
            return sendAPCommand(fd, "#:RC0#", "selectAPMoveToRate: Setting move to rate to 12x");
        case 1:
            return sendAPCommand(fd, "#:RC1#", "selectAPMoveToRate: Setting move to rate to 64x");
        case 2:
            return sendAPCommand(fd, "#:RC2#", "selectAPMoveToRate: Setting move to rate to 600x");
        case 3:
            return sendAPCommand(fd, "#:RC3#", "selectAPMoveToRate: Setting move to rate to 1200x");
        default:
            return -1;
    }
    return 0;
}

int selectAPSlewRate(int fd, int slewRate)
{
    switch (slewRate)
    {
        case 0:
            return sendAPCommand(fd, "#:RS0#", "selectAPSlewRate: Setting slew to rate to 600x");
        case 1:
            return sendAPCommand(fd, "#:RS1#", "selectAPSlewRate: Setting slew to rate to 900x");
        case 2:
            return sendAPCommand(fd, "#:RS2#", "selectAPSlewRate: Setting slew to rate to 1200x");
        default:
            return -1;
    }
    return 0;
}

int selectAPTrackingMode(int fd, int trackMode)
{
    switch (trackMode)
    {
        case AP_TRACKING_SIDEREAL:
            return sendAPCommand(fd, "#:RT2#", "selectAPTrackingMode: Setting tracking mode to sidereal.");
        case AP_TRACKING_SOLAR:
            return sendAPCommand(fd, "#:RT1#", "selectAPTrackingMode: Setting tracking mode to solar.");
        case AP_TRACKING_LUNAR:
            return sendAPCommand(fd, "#:RT0#", "selectAPTrackingMode: Setting tracking mode to lunar.");
        case AP_TRACKING_CUSTOM:
            DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG,
                        "selectAPTrackingMode: Setting tracking mode to Custom -- NOT IMPLEMENTED!.");
            break;
        case AP_TRACKING_OFF:
            return sendAPCommand(fd, "#:RT9#", "selectAPTrackingMode: Setting tracking mode to Zero.");
        default:
            return -1;
    }
    return 0;
}

int selectAPGuideRate(int fd, int guideRate)
{
    switch (guideRate)
    {
        case 0:
            return sendAPCommand(fd, "#:RG0#", "selectAPGuideRate: Setting guide to rate to 0.25x");
        case 1:
            return sendAPCommand(fd, "#:RG1#", "selectAPGuideRate: Setting guide to rate to 0.50x");
        case 2:
            return sendAPCommand(fd, "#:RG2#", "selectAPGuideRate: Setting guide to rate to 1.00x");
        default:
            return -1;
    }
    return 0;
}

int swapAPButtons(int fd, int currentSwap)
{
    switch (currentSwap)
    {
        case 0:
            return sendAPCommand(fd, "#:NS#", "swapAPButtons: Swapping NS.");
        case 1:
            return sendAPCommand(fd, "#:EW#", "swapAPButtons: Swapping EW.");
        default:
            return -1;
    }
    return 0;
}

int setAPObjectRA(int fd, double ra)
{
    /*ToDo AP accepts "#:Sr %02d:%02d:%02d.%1d#"*/
    int h, m, s;
    char cmd[256];

    // Make sure RA is 0-24.
    while (ra < 0) ra += 24.0;
    while (ra > 24.0) ra -= 24.0;

    getSexComponents(ra, &h, &m, &s);

    snprintf(cmd, sizeof(cmd), "#:Sr %02d:%02d:%02d#", h, m, s);

    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    return (setStandardProcedure(fd, cmd));
}

int setAPObjectDEC(int fd, double dec)
{
    int d, m, s;
    char cmd[256];

    getSexComponents(dec, &d, &m, &s);
    if (d < 0) d = -d;
    snprintf(cmd, sizeof(cmd), "#:Sd %s%02d*%02d:%02d#",
             dec >= 0 ? "+" : "-", d, m, s);

    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    return (setStandardProcedure(fd, cmd));
}

// Set the longitude.
int setAPSiteLongitude(int fd, double Long)
{
    int d, m, s;
    char cmd[256];

    // Make sure longitude is 0-360.
    while (Long < 0) Long += 360.0;
    while (Long > 360.0) Long -= 360.0;

    getSexComponents(Long, &d, &m, &s);
    snprintf(cmd, sizeof(cmd), "#:Sg %03d*%02d:%02d#", d, m, s);

    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    return (setStandardProcedure(fd, cmd));
}

// Set the latitude.
int setAPSiteLatitude(int fd, double Lat)
{
    int d, m, s;
    char cmd[256];

    getSexComponents(Lat, &d, &m, &s);
    if (d < 0) d = -d;
    snprintf(cmd, sizeof(cmd), "#:St %s%02d*%02d:%02d#",
             Lat >= 0 ? "+" : "-", d, m, s);

    DEBUGFDEVICE(lx200ap_name, AP_DBG_SCOPE, "CMD <%s>", cmd);

    return (setStandardProcedure(fd, cmd));
}

int setAPRATrackRate(int fd, double rate)
{
    char cmd[16];
    char sign;
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (rate < 0)
        sign = '-';
    else
        sign = '+';

    snprintf(cmd, 16, "#:RR%c%03.4f#", sign, fabs(rate));

    DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    tcflush(fd, TCIFLUSH);

    if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return errcode;
    }

    if ((errcode = tty_read(fd, response, 1, LX200_TIMEOUT, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return errcode;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return 0;
    }

    DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return -1;
}

int setAPDETrackRate(int fd, double rate)
{
    char cmd[16];
    char sign;
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (rate < 0)
        sign = '-';
    else
        sign = '+';

    snprintf(cmd, 16, "#:RD%c%03.4f#", sign, fabs(rate));

    DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);


    tcflush(fd, TCIFLUSH);

    if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return errcode;
    }

    if ((errcode = tty_read(fd, response, 1, LX200_TIMEOUT, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return errcode;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return 0;
    }

    DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return -1;
}

int APSendPulseCmd(int fd, int direction, int duration_msec)
{
    char cmd[20];

    // GTOCP3 supports 3 digits for msec duration
    if (duration_msec > MAX_LX200AP_PULSE_LEN)
    {
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "APSendPulseCmd requested %d msec limited to 999 msec!", duration_msec);
        duration_msec = 999;
    }

    switch (direction)
    {
        case LX200_NORTH:
            sprintf(cmd, "#:Mn%03d#", duration_msec);
            break;
        case LX200_SOUTH:
            sprintf(cmd, "#:Ms%03d#", duration_msec);
            break;
        case LX200_EAST:
            sprintf(cmd, "#:Me%03d#", duration_msec);
            break;
        case LX200_WEST:
            sprintf(cmd, "#:Mw%03d#", duration_msec);
            break;
        default:
            return 1;
    }

    int res = sendAPCommand(fd, cmd, "APSendPulseCmd: Sending pulse command.");
    tcflush(fd, TCIFLUSH);
    return res;
}

int APParkMount(int fd)
{
    return sendAPCommand(fd, "#:KA#", "APParkMount: Sending park command.");
}


int APUnParkMount(int fd)
{
    return sendAPCommand(fd, "#:PO#", "APUnParkMount: Sending unpark command.");
}

// This is a modified version of selectAPMoveRate() from lx200apdriver.cpp
// This version allows changing the rate to GUIDE as well as 12x/64x/600x/1200x
// and is required some the experimental AP driver properly handles
// pulse guide requests over 999ms by simulated it by setting the move rate
// to GUIDE and then starting and halting a move of the correct duration.
int selectAPCenterRate(int fd, int centerRate)
{
    switch (centerRate)
    {
        case 0:
            return sendAPCommand(fd, "#:RG#", "selectAPMoveToRate: Setting move to rate to GUIDE");
        case 1:
            return sendAPCommand(fd, "#:RC0#", "selectAPMoveToRate: Setting move to rate to 12x");
        case 2:
            return sendAPCommand(fd, "#:RC1#", "selectAPMoveToRate: Setting move to rate to 64x");
        case 3:
            return sendAPCommand(fd, "#:RC2#", "selectAPMoveToRate: Setting move to rate to 600x");
        case 4:
            return sendAPCommand(fd, "#:RC3#", "selectAPMoveToRate: Setting move to rate to 1200x");
        default:
            return -1;
    }
    return 0;
}

// Doc for the :GOS command fom A-P:
//
// Response for GTOCP3 Rev “T” through GTOCP4 Rev VCP4-P01-14 is a 13 character string: ABCDEFGHIJKLM.
//
// Note the addition of the last two characters: L & M. (Rev “S” had the 1st 11 characters)
// 14th character “N” added in VCPx-P02-xx and later
//
// Possible values for each variable are as follows (Please note the difference between “0” and “O” in the responses)
// Note the differences starting with the GTOCP4, especially with P02-01 and later!!
//
// A: Park Status                         'P' parked, '0' not parked, '1' auto-park - ON
// B: RA Tracking Status                  '0'=Lunar Rate, '1'=Solar Rate, '2'=Sidereal, '9'=Tracking Stopped,
//                                        'C'=Custom RA Tracking Rate (read specific value with :Rr# command)
// C: Dec Tracking Status                 '9'=No Motion (to mimic RA Tracking), 'C'=Custom DEC Tracking Rate
//                                        (read specific value with :Rd# command)
// D: Slewing Satus (GOTO Slews)          'S'=Slewing, '0'=Not slewing
// E: Moving RA Axis                      'E'=Moving East, 'W'=Moving West, '0'=Not Moving
//    (via a Move command/Slew/ST4 Port signal)
// F: Moving Dec Axis                     'N'=Moving North (counter-clockwise), 'S'=Moving South (clockwise), '0'=Not Moving
//    (via a Move command/Slew/ST4 Port signal):
// G: Guide Rate                          '0'=0.25x, '1'=0.50x, '2'=1.00x
// H: Center/Move Rate < P02-xx           '0'=12x, '1'=64x, '2'=600x, '3'=1200x, 'C'=Custom Rate
//                                        NOTE: Divide rates for '2' and '3' by 2 for 3600GTO
//                                        Read “C” rate with  :Rc#  command.  Do not divide by 2 for 3600GTO
// H: Center/Move Rate > P02-xx           '0'=12x, '1'=64x, '2'=200x, '3'= 400x - 600x  see table,
//                                        '4'= 600x - 1200x  see table, '5'=0.25x, '6'=0.5x, '7'=1.0x, 'C'=Custom Rate
// I: Slew Rate                           '0'=600x     Slow, '1'=900x     Medium, '2'=1200x   Fast, 'C'=Custom Rate
//                                        NOTE: Rates are scaled for “0”, “1” & “2” by different amounts for 3600GTOs
//                                        and for some 400GTO and 600EGTO mounts.
//                                        Read 'C' rate with  :Rs#  command.  Do not scale custom rates.
// J: PEM                                'O'=Off, 'P'=Playback, 'R'=Recording, 'E'=Encoder
// K: Mount Status                       '0'=Normal, '1'=Stalled, '2'=Low Power Supply,
//                                       '4'=Servo fault / number problem, '8'=Reserved  (CP3 only)
// K: Mount Status                       '0'=Normal 'Z'=Stalled, 'Y'=Low Power Supply, 'X'=Servo fault / number problem,
//    VCP4-P02-01 and later              'N'=CCW Internal Declination Limit or AE Limit, 'S'=CW Internal Declination Limit or AE Limit,
//                                       'E'=East Internal RA Limit or AE Limit, 'W'=West Internal RA Limit or AE Limit,
//                                       'z'=Kill Function has been issued
// L: E – W button reversal              '0'=Normal  E = E  and W = W, '1'=Reversed   E = W  and  W = E
// M: N – S button reversal              '0'=Normal   N = CCW  and  S = CW, '1'=Reversed    N = CW  and  S = CCW
// N: Button / Slew Rate Table           '0'=Normal, '1'= ~75% speed reduction, '2'= ~50% speed reduction, '3'= High Speed for Mach2GTO
//    VCP4-P02-01 and later
//

int getApStatusStringInternal(int fd, char *statusString, bool complain)
{
    int nbytes_write = 0;
    int nbytes_read  = 0;

    if (fd <= 0)
    {
        if (complain) DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR,
                                      "getApStatusString: not a valid file descriptor received");
        return -1;
    }

    int res = sendAPCommand(fd, "#:GOS#", "getApStatusString");
    if (res != TTY_OK)
    {
        if (complain) DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR,
                                       "getApStatusString: unsuccessful write to telescope, %d", nbytes_write);
        return res;
    }

    tty_read_section(fd, statusString, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);
    if (nbytes_read > 3)
    {
        statusString[nbytes_read - 1] = '\0';

        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "getApStatusString: received bytes %d, [%s]",
                     nbytes_write, statusString);

        return 0;
    }

    if (complain) DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "getApStatusString: wrote, but nothing received.");

    return -1;
}

int getApStatusString(int fd, char *statusString)
{
    // I seem to get intermittant failures.
    // Try again on these after a 50ms delay, and the 250ms delay.
    if (getApStatusStringInternal(fd, statusString, false) != TTY_OK)
    {
        const struct timespec timeout50ms = {0, 50000000L};
        nanosleep(&timeout50ms, nullptr);
        if (getApStatusStringInternal(fd, statusString, true) == TTY_OK)
            return TTY_OK;
        else
        {
            const struct timespec timeout250ms = {0, 250000000L};
            nanosleep(&timeout250ms, nullptr);
            return getApStatusStringInternal(fd, statusString, true);
        }
    }
    return TTY_OK;
}

int check_lx200ap_status(int fd, char *parkStatus, char *slewStatus)
{
    char status_string[256];
    int res = getApStatusString(fd, status_string);
    if (res != TTY_OK) return res;

    *parkStatus = status_string[0];
    *slewStatus = status_string[3];
    return TTY_OK;
}

// See above for the full list.
// K: Mount Status                       '0'=Normal, '1'=Stalled, '2'=Low Power Supply,
//                                       '4'=Servo fault / number problem, '8'=Reserved  (CP3 only)
// K: Mount Status                       '0'=Normal 'Z'=Stalled, 'Y'=Low Power Supply, 'X'=Servo fault / number problem,
//    VCP4-P02-01 and later              'N'=CCW Internal Declination Limit or AE Limit, 'S'=CW Internal Declination Limit or AE Limit,
//                                       'E'=East Internal RA Limit or AE Limit, 'W'=West Internal RA Limit or AE Limit,
//                                       'z'=Kill Function has been issued
const char *apMountStatus(const char *statusString)
{
    if (strlen(statusString) < 11)
        return "????";
    const char statusChar = statusString[10];
    switch (statusChar)
    {
        case '0':
            return "Normal";
        case '1':
        case 'Z':
            return "Stalled";
        case '2':
        case 'Y':
            return "Low Power Supply";
        case '4':
        case 'X':
            return "Servo Fault";
        case 'N':
            return "CCW DEC or AE Limit";
        case 'S':
            return "CW DEC or AE Limit";
        case 'E':
            return "East RA or AE Limit";
        case 'W':
            return "West RA or AE Limit";
        case 'z':
            return "Kill Function issued";
        case '8':
        default:
            return "";
    }
}

bool apStatusParked(const char *statusString)
{
    return statusString[0] == 'P';
}

bool apStatusSlewing(const char *statusString)
{
    return statusString[3] !=  '0';
}

// The 14th character in the status string "N" tells up about the rate table.
APRateTableState apRateTable(const char *statusString)
{
    if (strlen(statusString) >= 14)
    {
        switch (statusString[13])
        {

            case '0':
                return AP_RATE_TABLE_0;
                break;
            case '1':
                return AP_RATE_TABLE_1;
                break;
            case '2':
                return AP_RATE_TABLE_2;
                break;
            case '3':
                return AP_RATE_TABLE_3;
                break;
            default:
                return AP_RATE_TABLE_DEFAULT;
                break;
        }
    }
    return AP_RATE_TABLE_DEFAULT;
}


// Doc for the :G_E command fom A-P:
// Note that for CP3, must send G control-E but CP4 and CP5 will also accept G_E.
// This function just sends the G control-E which should work for all three controllers.
//
// Get Mount Features
// Command:	:G<cntl>E#
// Response:	xxxx#
// History:		All firmware versions
// Gets the bit mask associated with mount features.
// Bit Weighting  Meaning
// 0      1       Mount Type:  0 = Equatorial Mount, 1 = Fork Mount
// 1      2       0 = Normal Speed Range, 2 = Slew Scaling on Standard Rates ( >= 600x)
//                This function has been eliminated beginning P02-01, in favor of the rate tables.
// 2      4       0 = Encoders not Supported, 4 = Encoders Supported
// 3-5            Bit encoded indication of what encoder types are supported
// 6     64       Motor Type:  0 = Servo Motors, 64 = Stepper Motors
// 7    128       Encoder Reference: 0 = Clutch Dependent, 128 = Clutch Independent (ex. Mach2GTO)
//                This bit is only meaningful if bit 2 is set.
// 8    256       0 = Modeling not Enabled, 256 = Modeling Enabled,
//                This bit is only meaningful in the GTOCP4, as modeling is always enabled in the GTOCP5,
//                and isn’t available for the GTOCP1-3
// 9-31           (reserved for future use)
int getApMountFeatures(int fd, bool *hasEncoder, bool *clutchAware)
{
    bool complain = false;
    int nbytes_write = 0;
    int nbytes_read  = 0;
    constexpr int RB_MAX_LEN = 256;
    char readBuffer[RB_MAX_LEN];
    *hasEncoder = false;
    *clutchAware = false;

    if (fd <= 0)
    {
        if (complain) DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR,
                                      "getApStatusString: not a valid file descriptor received");
        return TTY_READ_ERROR;
    }

    int res = sendAPCommand(fd, "#:G\005#", "getApStatusString");
    if (res != TTY_OK)
    {
        if (complain) DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR,
                                       "getApMountFeatures: unsuccessful write to telescope, %d", nbytes_write);
        return res;
    }

    tty_read_section(fd, readBuffer, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);
    if (nbytes_read > 1)
    {
        readBuffer[nbytes_read - 1] = '\0';

        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "getApMountFeatures: received bytes %d, [%s]",
                     nbytes_write, readBuffer);
        int value;
        if (sscanf(readBuffer, "%d", &value) > 0)
        {
            *hasEncoder = value & 4;
            *clutchAware = value & 128;
        }

        return TTY_OK;
    }

    if (complain) DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR, "getApReadBuffer: wrote, but nothing received.");

    return TTY_READ_ERROR;
}

bool apCanHome(int fd)
{
    bool hasEncoder = false;
    bool clutchAware = false;
    return (getApMountFeatures(fd, &hasEncoder, &clutchAware) == TTY_OK) &&
           hasEncoder && clutchAware;
}

// This would just work with a clutch-aware encoder mount running a CP5. Currently only Mach2.
int apHomeAndSync(int fd)
{
    return sendAPCommand(fd, "#$HA#", "AP Home and Sync");
}

int isAPInitialized(int fd, bool *isInitialized)
{
    constexpr int RB_MAX_LEN = 256;
    char readBuffer[RB_MAX_LEN];
    int error_type;
    int nbytes_write = 0;
    int nbytes_read  = 0;

    DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "Check initialized...");

    if (fd <= 0)
    {
        DEBUGDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR,
                    "isAPInitialized: not a valid file descriptor received");

        return -1;
    }

    if ((error_type = tty_write_string(fd, "#:GR#", &nbytes_write)) != TTY_OK)
    {
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_ERROR,
                     "isAPInitialized: unsuccessful write to telescope, %d", nbytes_write);

        return error_type;
    }

    error_type = tty_nread_section(fd, readBuffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);


    if (nbytes_read < 1)
    {
        DEBUGFDEVICE(lx200ap_name, INDI::Logger::DBG_DEBUG, "RES ERROR <%d>", error_type);
        return error_type;
    }

    readBuffer[nbytes_read - 1] = '\0';

    if (!strcmp("00:00.0", readBuffer))
        *isInitialized = false;
    else if (!strcmp("00:00:00.0", readBuffer))
        *isInitialized = false; // not sure about this one--high precision 0.
    else
        *isInitialized = true; // Should I test further????

    tcflush(fd, TCIFLUSH);
    return 0;
}
