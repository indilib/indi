#if 0
LX200 Driver
Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "lx200driver.h"

#include "indicom.h"
#include "indilogger.h"

#include <cstring>
#include <unistd.h>

#ifndef _WIN32
#include <termios.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <string.h>
#endif

/* Add mutex */

#include <mutex>

#define LX200_TIMEOUT 5 /* FD timeout in seconds */
#define RB_MAX_LEN    64


int eq_format; /* For possible values see enum TEquatorialFormat */
int geo_format = LX200_GEO_SHORT_FORMAT; /* For possible values see enum TGeographicFormat */
char lx200Name[MAXINDIDEVICE];
/* ESN DEBUG */
unsigned int DBG_SCOPE = 8;

/* Add mutex to communications */
std::mutex lx200CommsLock;

void setLX200Debug(const char *deviceName, unsigned int debug_level)
{
    strncpy(lx200Name, deviceName, MAXINDIDEVICE);
    DBG_SCOPE = debug_level;
}

int check_lx200_connection(int in_fd)
{
    const struct timespec timeout = {0, 50000000L};
    int i       = 0;
    char ack[1] = { 0x06 };
    char MountAlign[64];
    int nbytes_read = 0;

    DEBUGDEVICE(lx200Name, INDI::Logger::DBG_DEBUG, "Testing telescope connection using ACK...");

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    if (in_fd <= 0)
        return -1;

    for (i = 0; i < 2; i++)
    {
        // Meade Telescope Serial Command Protocol Revision 2010.10
        // ACK <0x06> Query of alignment mounting mode.
        // Returns:
        // A If scope in AltAz Mode
        // D If scope is currently in the Downloader [Autostar II & Autostar]
        // L If scope in Land Mode
        // P If scope in Polar Mode
        if (write(in_fd, ack, 1) < 0)
            return -1;
        tty_read(in_fd, MountAlign, 1, LX200_TIMEOUT, &nbytes_read);
        if (nbytes_read == 1)
        {
            DEBUGDEVICE(lx200Name, INDI::Logger::DBG_DEBUG, "Testing successful!");
            return 0;
        }
        nanosleep(&timeout, nullptr);
    }

    DEBUGDEVICE(lx200Name, INDI::Logger::DBG_DEBUG, "Failure. Telescope is not responding to ACK!");
    return -1;
}

/**********************************************************************
* GET
**********************************************************************/

char ACK(int fd)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);

    char ack[1] = { 0x06 };
    char MountAlign[2];
    int nbytes_write = 0, nbytes_read = 0, error_type;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%#02X>", ack[0]);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // ACK <0x06> Query of alignment mounting mode.
    // Returns:
    // A If scope in AltAz Mode
    // D If scope is currently in the Downloader [Autostar II & Autostar]
    // L If scope in Land Mode
    // P If scope in Polar Mode
    nbytes_write = write(fd, ack, 1);

    if (nbytes_write < 0)
        return -1;

    error_type = tty_read(fd, MountAlign, 1, LX200_TIMEOUT, &nbytes_read);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%c>", MountAlign[0]);

    if (nbytes_read == 1)
        return MountAlign[0];
    else
        return error_type;
}

int getCommandSexa(int fd, double *value, const char *cmd)
{
    char read_buffer[RB_MAX_LEN] = {0};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    tcflush(fd, TCIFLUSH);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", cmd);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN,  '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);
    if (error_type != TTY_OK)
        return error_type;

    read_buffer[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    if (f_scansexa(read_buffer, value))
    {
        DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse response");
        return -1;
    }

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%g]", *value);

    tcflush(fd, TCIFLUSH);
    return 0;
}

int getCommandInt(int fd, int *value, const char *cmd)
{
    char read_buffer[RB_MAX_LEN] = {0};
    float temp_number;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    tcflush(fd, TCIFLUSH);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", cmd);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);
    if (error_type != TTY_OK)
        return error_type;

    read_buffer[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    /* Float */
    if (strchr(read_buffer, '.'))
    {
        if (sscanf(read_buffer, "%f", &temp_number) != 1)
            return -1;

        *value = static_cast<int>(temp_number);
    }
    /* Int */
    else if (sscanf(read_buffer, "%d", value) != 1)
        return -1;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%d]", *value);

    return 0;
}

int getCommandString(int fd, char *data, const char *cmd)
{
    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", cmd);

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, data, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    if (error_type != TTY_OK)
        return error_type;

    term = strchr(data, '#');
    if (term)
        *term = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", data);

    return 0;
}

int isSlewComplete(int fd)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    /* update for classic lx200, total string returned is 33 bytes */
    char data[33] = { 0 };
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;
    const char *cmd = ":D#";

    /* update for slew complete lx200 classic 3.2. roms */
    int i;


    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", cmd);

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :D#
    // Requests a string of bars indicating the distance to the current target location.
    // Returns:
    // LX200's – a string of bar characters indicating the distance.
    // Autostars and Autostar II – a string containing one bar until a slew is complete, then a null string is returned
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :D#
    // Requests a string indicating the progress of the current slew operation.
    // Returns:
    // the string “■#”, where the block character has ascii code 127 (0x7F), if a slew is in
    // progress or a slew has ended from less than the settle time set in command :Sstm.
    // the string “#” if a slew has been completed or no slew is underway.
    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, data, 33, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIOFLUSH);

    if (error_type != TTY_OK)
        return error_type;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", data);
    /* update for slewComplete

       The below should handle classic lx200, autostar and autostar 2
       classic returns string of 33 bytes, and non space (0x20) before terminator is not done yet
       autostar and autostar 2 return a few bytes, with '#' terminator
          first char
    */
    for(i = 0; i < 33; i++)
    {
        if(data[i] == '#') return 1;
        if(data[i] != 0x20) return 0;
    }
    return 1;
    /* out for slewComplete update
        if (data[0] == '#')
            return 1;
        else
            return 0;
    END out for slewComplete update */
}

int getCalendarDate(int fd, char *date)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int dd, mm, yy, YYYY;
    int error_type;
    int nbytes_read = 0;
    char mell_prefix[3] = {0};
    int len = 0;

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :GC#
    // Get current date.
    // Returns: MM/DD/YY#
    // The current local calendar date for the telescope.
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :GC#
    // Get current date. Returns the current date formatted as follows:
    // Emulation and precision                    Return value
    // LX200 emulation, low and high precision    MM/DD/YY# (month, day, year)
    // Extended emulation, low and high precision MM:DD:YY# (month, day, year) – note that the separator character is ':' instead of '/'.
    // Any emulation, ultra precision             YYYY-MM-DD# (year, month, day) – note that the separator character is '-' instead of '/'.
    if ((error_type = getCommandString(fd, date, ":GC#")))
        return error_type;
    len = strnlen(date, 32);
    if (len == 10)
    {
        /* 10Micron Ultra Precision mode calendar date format is YYYY-MM-DD */
        nbytes_read = sscanf(date, "%4d-%2d-%2d", &YYYY, &mm, &dd);
        if (nbytes_read < 3)
            return -1;
        /* We're done, date is already in ISO format */
    }
    else
    {
        /* Meade format is MM/DD/YY */
        nbytes_read = sscanf(date, "%d%*c%d%*c%d", &mm, &dd, &yy);
        if (nbytes_read < 3)
            return -1;
        /* We consider years 50 or more to be in the last century, anything less in the 21st century.*/
        if (yy > 50)
            strncpy(mell_prefix, "19", 3);
        else
            strncpy(mell_prefix, "20", 3);
        /* We need to have it in YYYY-MM-DD ISO format */
        snprintf(date, 32, "%s%02d-%02d-%02d", mell_prefix, yy, mm, dd);
    }
    return (0);
}

int getTimeFormat(int fd, int *format)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;
    int tMode;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Gc#");

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :Gc#
    // Get Clock Format
    // Returns: 12# or 24#
    // Depending on the current telescope format setting.
    if ((error_type = tty_write_string(fd, ":Gc#", &nbytes_write)) != TTY_OK)
        return error_type;

    if ((error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read)) != TTY_OK)
        return error_type;

    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
        return error_type;

    read_buffer[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    // The Losmandy Gemini puts () around it's time format
    if (strstr(read_buffer, "("))
        nbytes_read = sscanf(read_buffer, "(%d)", &tMode);
    else
        nbytes_read = sscanf(read_buffer, "%d", &tMode);

    if (nbytes_read < 1)
        return -1;
    else
        *format = tMode;

    return 0;
}

int getSiteName(int fd, char *siteName, int siteNum)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char *term;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :GM# // Get Site 1 Name // Returns: <string># // A ‘#’ terminated string with the name of the requested site.
    // :GN# // Get Site 2 Name // Returns: <string># // A ‘#’ terminated string with the name of the requested site.
    // :GO# // Get Site 3 Name // Returns: <string># // A ‘#’ terminated string with the name of the requested site.
    // :GP# // Get Site 4 Name // Returns: <string># // A ‘#’ terminated string with the name of the requested site.
    switch (siteNum)
    {
        case 1:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":GM#");
            if ((error_type = tty_write_string(fd, ":GM#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case 2:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":GN#");
            if ((error_type = tty_write_string(fd, ":GN#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case 3:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":GO#");
            if ((error_type = tty_write_string(fd, ":GO#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case 4:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":GP#");
            if ((error_type = tty_write_string(fd, ":GP#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        default:
            return -1;
    }

    error_type = tty_nread_section(fd, siteName, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
        return error_type;

    siteName[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", siteName);

    term = strchr(siteName, ' ');
    if (term)
        *term = '\0';

    term = strchr(siteName, '<');
    if (term)
        strcpy(siteName, "unused site");

    DEBUGFDEVICE(lx200Name, INDI::Logger::DBG_DEBUG, "Site Name <%s>", siteName);

    return 0;
}

int getSiteLatitude(int fd, int *dd, int *mm, double *ssf)
{
    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :Gt#
    // Get Current Site Latitude
    // Returns: sDD*MM#
    // The latitude of the current site. Positive inplies North latitude.
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :Gt#
    // Get current site latitude.
    // Returns the latitude of the current site formatted as follows:
    // Emulation and precision              Return value
    // Any emulation, low precision         sDD*MM# (sign, degrees, minutes)
    // LX200 emulation, high precision      sDD*MM# (sign, degrees, minutes)
    // Extended emulation, high precision   sDD*MM:SS# (sign, degrees, arcminutes, arcseconds)
    // Any emulation, ultra precision       sDD:MM:SS.S# (sign, degrees, arcminutes, arcseconds, tenths of arcsecond)
    // Positive implies north latitude.
    return getSiteLatitudeAlt( fd, dd, mm, ssf, ":Gt#");
}

// Meade classic handset defines longitude as 0 to 360 WESTWARD. However,
// Meade API expresses East Longitudes as negative, West Longitudes as positive.
// Source: https://www.meade.com/support/LX200CommandSet.pdf from 2002 at :Gg#
// (And also 10Micron has East Longitudes expressed as negative.)
// Also note that this is the opposite of cartography where East is positive.
int getSiteLongitude(int fd, int *ddd, int *mm, double *ssf)
{
    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :Gg#
    // Get Current Site Longitude
    // Returns: sDDD*MM#
    // The current site Longitude. East Longitudes are expressed as negative
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :Gg#
    // Get current site longitude. Note: East Longitudes are expressed as negative. Returns
    // the current site longitude formatted as follows:
    // Emulation and precision                 Return value
    // Any emulation, low precision or LX200   sDDD*MM# (sign, degrees, arcminutes)
    // emulation, high precision
    // Extended emulation, high precision      sDDD*MM:SS# (sign, degrees, arcminutes, arcseconds)
    // Any emulation, ultra precision          sDDD:MM:SS.S# (sign, degrees, arcminutes, arcseconds, tenths of arcsecond)
    return getSiteLongitudeAlt(fd, ddd, mm, ssf, ":Gg#");
}


int getSiteLatitudeAlt(int fd, int *dd, int *mm, double *ssf, const char *cmd)
{
    // :Gt# see getSiteLatitude()
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", cmd);

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);

    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
        return error_type;

    read_buffer[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    *ssf = 0.0;
    if (sscanf(read_buffer, "%d%*c%d:%lf", dd, mm, ssf) < 2)
    {
        DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Unable to parse %s response", cmd);
        return -1;
    }

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%d,%d,%.1lf]", *dd, *mm, *ssf);

    int new_geo_format;
    switch (nbytes_read)
    {
        case 9:
        case 10:
            new_geo_format = LX200_GEO_LONG_FORMAT;
            break;
        case 11:
        case 12:
            new_geo_format = LX200_GEO_LONGER_FORMAT;
            break;
        default:
            new_geo_format = LX200_GEO_SHORT_FORMAT;
            break;
    }
    if (new_geo_format != geo_format)
    {
        DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Updated geographic precision from setting %d to %d", geo_format, new_geo_format);
        geo_format = new_geo_format;
    }

    return 0;
}

int getSiteLongitudeAlt(int fd, int *ddd, int *mm, double *ssf, const char *cmd)
{
    // :Gg# see getSiteLongitude()
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", cmd);

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    if ((error_type = tty_write_string(fd, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);

    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
        return error_type;

    read_buffer[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    *ssf = 0.0;
    if (sscanf(read_buffer, "%d%*c%d:%lf", ddd, mm, ssf) < 2)
    {
        DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Unable to parse %s response", cmd);
        return -1;
    }
    *ddd *= -1.0; // Convert LX200Longitude to CartographicLongitude

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL in CartographicLongitude format [%d,%d,%.1lf]", *ddd, *mm, *ssf);

    int new_geo_format;
    switch (nbytes_read)
    {
        case 10:
        case 11:
            new_geo_format = LX200_GEO_LONG_FORMAT;
            break;
        case 12:
        case 13:
            new_geo_format = LX200_GEO_LONGER_FORMAT;
            break;
        default:
            new_geo_format = LX200_GEO_SHORT_FORMAT;
            break;
    }
    if (new_geo_format != geo_format)
    {
        DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Updated geographic precision from setting %d to %d", geo_format, new_geo_format);
        geo_format = new_geo_format;
    }

    return 0;
}

int getTrackFreq(int fd, double *value)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    float Freq;
    char read_buffer[RB_MAX_LEN] = {0};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":GT#");

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :GT#
    // Get tracking rate
    // Returns: TT.T#
    // Current Track Frequency expressed in hertz assuming a synchronous motor design where a 60.0 Hz motor clock
    // would produce 1 revolution of the telescope in 24 hours.
    if ((error_type = tty_write_string(fd, ":GT#", &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
        return error_type;

    read_buffer[nbytes_read] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    if (sscanf(read_buffer, "%f#", &Freq) < 1)
    {
        DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse response");
        return -1;
    }

    *value = static_cast<double>(Freq);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%g]", *value);

    return 0;
}

int getHomeSearchStatus(int fd, int *status)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":h?#");

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :h?#
    // Autostar, Autostar II and LX 16” Query Home Status
    // Returns:
    // 0 Home Search Failed
    // 1 Home Search Found
    // 2 Home Search in Progress
    // LX200 Not Supported
    if ((error_type = tty_write_string(fd, ":h?#", &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);
    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
        return error_type;

    read_buffer[1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    if (read_buffer[0] == '0')
        *status = 0;
    else if (read_buffer[0] == '1')
        *status = 1;
    else if (read_buffer[0] == '2')
        *status = 1;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%d]", *status);

    return 0;
}

int getOTATemp(int fd, double *value)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;
    float temp;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":fT#");

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :fT#
    // Autostar II – Return Optical Tube Assembly Temperature
    // Max/RCX – Return OTA Temperature
    // Returns <sdd.ddd># - a ‘#’ terminated signed ASCII real number indicating the Celsius ambient temperature.
    // All others – Not supported
    if ((error_type = tty_write_string(fd, ":fT#", &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);

    if (nbytes_read < 1)
        return error_type;

    read_buffer[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    if (sscanf(read_buffer, "%f", &temp) < 1)
    {
        DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse response");
        return -1;
    }

    *value = static_cast<double>(temp);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%g]", *value);

    return 0;
}

/**********************************************************************
* SET
**********************************************************************/

int setStandardProcedure(int fd, const char *data)
{
    char bool_return[2];
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", data);

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, data, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read(fd, bool_return, 1, LX200_TIMEOUT, &nbytes_read);

    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
        return error_type;

    if (bool_return[0] == '0')
    {
        DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s> failed.", data);
        return -1;
    }

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s> successful.", data);

    return 0;
}

int setCommandInt(int fd, int data, const char *cmd)
{
    char read_buffer[RB_MAX_LEN] = {0};
    int error_type;
    int nbytes_write = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    snprintf(read_buffer, sizeof(read_buffer), "%s%d#", cmd, data);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", read_buffer);

    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, read_buffer, &nbytes_write)) != TTY_OK)
    {
        DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s> failed.", read_buffer);
        return error_type;
    }

    tcflush(fd, TCIFLUSH);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s> successful.", read_buffer);

    return 0;
}

int setMinElevationLimit(int fd, int min)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :SoDD*#
    // Set lowest elevation to which the telescope will slew
    // Returns:
    // 0 – Invalid
    // 1 - Valid
    //
    // 10Micron adds a sign and limits but removes the * in their docs.
    // :SosDD#
    // Set the minimum altitude above the horizon to which the telescope will slew to sDD degrees.
    // Valid values are between –5 and +45 degrees.
    snprintf(read_buffer, sizeof(read_buffer), ":So%02d*#", min);

    return (setStandardProcedure(fd, read_buffer));
}

int setMaxElevationLimit(int fd, int max)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :ShDD#
    // Set the maximum object elevation limit to DD#
    // Returns:
    // 0 – Invalid
    // 1 - Valid
    snprintf(read_buffer, sizeof(read_buffer), ":Sh%02d#", max);

    return (setStandardProcedure(fd, read_buffer));
}

int setMaxSlewRate(int fd, int slewRate)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    if (slewRate < 2 || slewRate > 8)
        return -1;

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :SwN#
    // Set maximum slew rate to N degrees per second. N is the range (2..8)
    // Returns:
    // 0 – Invalid
    // 1 - Valid
    snprintf(read_buffer, sizeof(read_buffer), ":Sw%d#", slewRate);

    return (setStandardProcedure(fd, read_buffer));
}

int setObjectRA(int fd, double ra, bool addSpace)
{
    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :SrHH:MM.T#
    // :SrHH:MM:SS#
    // Set target object RA to HH:MM.T or HH:MM:SS depending on the current precision setting.
    // Returns:
    // 0 – Invalid
    // 1 - Valid
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :SrHH:MM.T# or :SrHH:MM:SS# or :SrHH:MM:SS.S# or :SrHH:MM:SS.SS#
    // Set target object RA to HH:MM.T (hours, minutes and tenths of minutes), HH:MM:SS
    // (hours, minutes, seconds), HH:MM:SS.S (hours, minutes, seconds and tenths of second)
    // or HH:MM:SS.SS (hours, minutes, seconds and hundredths of second).
    // Returns:
    // 0 invalid
    // 1 valid
    //
    // We support these formats:
    // LX200_EQ_SHORT_FORMAT  :SrHH:MM.T#     (hours, minutes and tenths of minutes)
    // LX200_EQ_LONG_FORMAT   :SrHH:MM:SS#    (hours, minutes, seconds)
    // LX200_EQ_LONGER_FORMAT :SrHH:MM:SS.SS# (hours, minutes, seconds and hundredths of second)
    // Add space is used to add space between the command the and rest of the arguments.
    // i.e. :Sr HH:MM:SS# for example since some mounts require space.
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);

    int h, m, s;
    char read_buffer[22] = {0};
    char cmd[8] = {0};
    if (addSpace)
        strcpy(cmd, "Sr ");
    else
        strcpy(cmd, "Sr");

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock);  */

    switch (eq_format)
    {
        case LX200_EQ_SHORT_FORMAT:
            int frac_m;
            getSexComponents(ra, &h, &m, &s);
            frac_m = (s / 60.0) * 10.;
            snprintf(read_buffer, sizeof(read_buffer), ":%s%02d:%02d.%01d#", cmd, h, m, frac_m);
            break;
        case LX200_EQ_LONG_FORMAT:
            getSexComponents(ra, &h, &m, &s);
            snprintf(read_buffer, sizeof(read_buffer), ":%s%02d:%02d:%02d#", cmd, h, m, s);
            break;
        case LX200_EQ_LONGER_FORMAT:
            double d_s;
            getSexComponentsIID(ra, &h, &m, &d_s);
            snprintf(read_buffer, sizeof(read_buffer), ":%s%02d:%02d:%05.02f#", cmd, h, m, d_s);
            break;
        default:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Unknown controller_format <%d>", eq_format);
            return -1;
    }

    return (setStandardProcedure(fd, read_buffer));
}

int setObjectDEC(int fd, double dec, bool addSpace)
{
    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :SdsDD*MM#
    // Set target object declination to sDD*MM or sDD*MM:SS depending on the current precision setting
    // Returns:
    // 1 - Dec Accepted
    // 0 – Dec invalid
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :SdsDD*MM# or :SdsDD*MM:SS# or :Sd sDD*MM:SS.S#
    // Set target object declination to sDD*MM (sign, degrees, arcminutes), sDD*MM:SS
    // (sign, degrees, arcminutes, arcseconds) or sDD*MM:SS.S (sign, degrees, arcminutes,
    // arcseconds and tenths of arcsecond)
    // Returns:
    // 0 invalid
    // 1 valid
    //
    // We support these formats:
    // LX200_EQ_SHORT_FORMAT  :SdsDD*MM#       (sign, degrees, arcminutes)
    // LX200_EQ_LONG_FORMAT   :SdsDD*MM:SS#    (sign, degrees, arcminutes, arcseconds)
    // LX200_EQ_LONGER_FORMAT :Sd sDD*MM:SS.S# (sign, degrees, arcminutes, arcseconds, tenths of arcsecond)
    // Add space is used to add space between the command the and rest of the arguments.
    // i.e. :Sd DD:MM:SS# for example since some mounts require space.
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    char cmd[8] = {0};
    if (addSpace)
        strcpy(cmd, "Sd ");
    else
        strcpy(cmd, "Sd");
    int d, m, s;
    char read_buffer[22] = {0};

    switch (eq_format)
    {
        case LX200_EQ_SHORT_FORMAT:
            getSexComponents(dec, &d, &m, &s);
            /* case with negative zero */
            if (!d && dec < 0)
                snprintf(read_buffer, sizeof(read_buffer), ":%s-%02d*%02d#", cmd, d, m);
            else
                snprintf(read_buffer, sizeof(read_buffer), ":%s%+03d*%02d#", cmd, d, m);
            break;
        case LX200_EQ_LONG_FORMAT:
            getSexComponents(dec, &d, &m, &s);
            /* case with negative zero */
            if (!d && dec < 0)
                snprintf(read_buffer, sizeof(read_buffer), ":%s-%02d*%02d:%02d#", cmd, d, m, s);
            else
                snprintf(read_buffer, sizeof(read_buffer), ":%s%+03d*%02d:%02d#", cmd, d, m, s);
            break;
        case LX200_EQ_LONGER_FORMAT:
            double d_s;
            getSexComponentsIID(dec, &d, &m, &d_s);
            /* case with negative zero */
            if (!d && dec < 0)
                snprintf(read_buffer, sizeof(read_buffer), ":%s-%02d*%02d:%04.1f#", cmd, d, m, d_s);
            else
                snprintf(read_buffer, sizeof(read_buffer), ":%s%+03d*%02d:%04.1f#", cmd, d, m, d_s);
            break;
        default:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Unknown controller_format <%d>", eq_format);
            return -1;
    }

    return (setStandardProcedure(fd, read_buffer));
}

int setCommandXYZ(int fd, int x, int y, int z, const char *cmd, bool addSpace)
{
    char read_buffer[RB_MAX_LEN] = {0};
    snprintf(read_buffer, sizeof(read_buffer), addSpace ? "%s %02d:%02d:%02d#" : "%s%02d:%02d:%02d#", cmd, x, y, z);

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    return (setStandardProcedure(fd, read_buffer));
}

int setAlignmentMode(int fd, unsigned int alignMode)
{
    int error_type;
    int nbytes_write = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :AL# // Sets telescope to Land alignment mode   // Returns: nothing
    // :AP# // Sets telescope to Polar alignment mode  // Returns: nothing
    // :AA# // Sets telescope the AltAz alignment mode // Returns: nothing
    switch (alignMode)
    {
        case LX200_ALIGN_POLAR:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":AP#");
            if ((error_type = tty_write_string(fd, ":AP#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_ALIGN_ALTAZ:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":AA#");
            if ((error_type = tty_write_string(fd, ":AA#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_ALIGN_LAND:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":AL#");
            if ((error_type = tty_write_string(fd, ":AL#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
    }

    tcflush(fd, TCIFLUSH);
    return 0;
}

int setCalenderDate(int fd, int dd, int mm, int yy, bool addSpace)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    const struct timespec timeout = {0, 10000000L};
    char read_buffer[RB_MAX_LEN];
    char dummy_buffer[RB_MAX_LEN];
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;
    yy = yy % 100;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :SCMM/DD/YY#
    // Change Handbox Date to MM/DD/YY
    // Returns: <D><string>
    //    D = '0' if the date is invalid. The string is the null string.
    //    D = '1' for valid dates and the string is "Updating Planetary Data#                  #"
    // Note: For LX200GPS/Autostar II this is the UTC data!
    //
    // 10Micron, the extended formats are documented here but not yet implemented.
    // :SCMM/DD/YY# or :SCMM/DD/YYYY# or :SCYYYY-MM-DD#
    // Set date to MM/DD/YY (month, day, year), MM/DD/YYYY (month, day, year) or YYYY-MM-DD (year, month, day).
    // The date is expressed in local time. Returns:
    // 0 if the date is invalid
    // The string "1Updating    Planetary Data. #                #" if the date is valid.
    // The string "1<32 spaces>#<32 spaces>#" in extended LX200 emulation mode.
    // The character "1" without additional strings in ultra-precision mode (regardless of emulation).
    snprintf(read_buffer, sizeof(read_buffer), addSpace ? ":SC %02d/%02d/%02d#" : ":SC%02d/%02d/%02d#", mm, dd, yy);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", read_buffer);

    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, read_buffer, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);
    // Read the next section which has 24 blanks and then a #
    // Can't just use the tcflush to clear the stream because it doesn't seem to work correctly on sockets
    tty_nread_section(fd, dummy_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);

    tcflush(fd, TCIFLUSH);

    if (nbytes_read < 1)
    {
        DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse response");
        return error_type;
    }

    read_buffer[1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    if (read_buffer[0] == '0')
        return -1;

    /* Sleep 10ms before flushing. This solves some issues with LX200 compatible devices. */
    nanosleep(&timeout, nullptr);
    tcflush(fd, TCIFLUSH);

    return 0;
}

int setUTCOffset(int fd, double hours)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :SGsHH.H#
    // Set the number of hours added to local time to yield UTC
    // Returns:
    // 0 – Invalid
    // 1 - Valid
    snprintf(read_buffer, sizeof(read_buffer), ":SG%+04.01lf#", hours);

    return (setStandardProcedure(fd, read_buffer));
}

// Meade classic handset defines longitude as 0 to 360 WESTWARD. However,
// Meade API expresses East Longitudes as negative, West Longitudes as positive.
// Source: https://www.meade.com/support/LX200CommandSet.pdf from 2002 at :Gg#
// (And also 10Micron has East Longitudes expressed as negative.)
// Also note that this is the opposite of cartography where East is positive.
int setSiteLongitude(int fd, double CartographicLongitude, bool addSpace)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int d, m, s;
    char read_buffer[RB_MAX_LEN] = {0};
    double LX200Longitude = -1.0 * CartographicLongitude;

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :SgDDD*MM#
    // Set current site’s longitude to DDD*MM an ASCII position string
    // Returns:
    // 0 – Invalid
    // 1 - Valid
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :SgsDDD*MM# or :SgsDDD*MM:SS# or :SgsDDD*MM:SS.S#
    // Set current site’s longitude to sDDD*MM (sign, degrees, arcminutes), sDDD*MM:SS
    // (sign, degrees, arcminutes, arcseconds) or sDDD*MM:SS.S (sign, degrees, arcminutes,
    // arcseconds and tenths of arcsecond). Note: East Longitudes are expressed as negative.
    // Returns:
    // 0 invalid
    // 1 valid
    switch (geo_format)
    {
        case LX200_GEO_SHORT_FORMAT: // d m
            getSexComponents(LX200Longitude, &d, &m, &s);
            snprintf(read_buffer, sizeof(read_buffer), addSpace ? ":Sg %03d*%02d#" : ":Sg%03d*%02d#", d, m);
            break;
        case LX200_GEO_LONG_FORMAT: // d m s
            getSexComponents(LX200Longitude, &d, &m, &s);
            snprintf(read_buffer, sizeof(read_buffer), addSpace ? ":Sg %03d*%02d:%02d#" : ":Sg%03d*%02d:%02d#", d, m, s);
            break;
        case LX200_GEO_LONGER_FORMAT: // d m s.f with f being tenths
            double s_f;
            getSexComponentsIID(LX200Longitude, &d, &m, &s_f);
            snprintf(read_buffer, sizeof(read_buffer), addSpace ? ":Sg %03d*%02d:%04.01lf#" : ":Sg%03d*%02d:%04.01lf#", d, m, s_f);
            break;
        default:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Unknown geographic format <%d>", geo_format);
            return -1;
    }

    return (setStandardProcedure(fd, read_buffer));
}

int setSiteLatitude(int fd, double Lat, bool addSpace)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int d, m, s;
    char read_buffer[RB_MAX_LEN] = {0};

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :StsDD*MM#
    // Sets the current site latitude to sDD*MM#
    // Returns:
    // 0 – Invalid
    // 1 - Valid
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :StsDD*MM# or :StsDD*MM:SS# or :StsDD*MM:SS.S#
    // Sets the current site latitude to sDD*MM (sign, degrees, arcminutes), sDD*MM:SS
    // (sign, degrees, arcminutes, arcseconds), or sDD*MM:SS.S (sign, degrees, arcminutes,
    // arcseconds and tenths of arcsecond)
    // Returns:
    // 0 invalid
    // 1 valid
    switch (geo_format)
    {
        case LX200_GEO_SHORT_FORMAT: // d m
            getSexComponents(Lat, &d, &m, &s);
            snprintf(read_buffer, sizeof(read_buffer), addSpace ? ":St %+03d*%02d#" : ":St%+03d*%02d#", d, m);
            break;
        case LX200_GEO_LONG_FORMAT: // d m s
            getSexComponents(Lat, &d, &m, &s);
            snprintf(read_buffer, sizeof(read_buffer), addSpace ? ":St %+03d*%02d:%02d#" : ":St%+03d*%02d:%02d#", d, m, s);
            break;
        case LX200_GEO_LONGER_FORMAT: // d m s.f with f being tenths
            double s_f;
            getSexComponentsIID(Lat, &d, &m, &s_f);
            snprintf(read_buffer, sizeof(read_buffer), addSpace ? ":St %+03d*%02d:%04.01lf#" : ":St%+03d*%02d:%04.01lf#", d, m, s_f);
            break;
        default:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Unknown geographic format <%d>", geo_format);
            return -1;
    }

    return (setStandardProcedure(fd, read_buffer));
}

int setObjAz(int fd, double az)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int d, m, s;
    char read_buffer[RB_MAX_LEN] = {0};

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    getSexComponents(az, &d, &m, &s);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :SzDDD*MM#
    // Sets the target Object Azimuth [LX 16” and Autostar II only]
    // Returns:
    // 0 – Invalid
    // 1 - Valid
    snprintf(read_buffer, sizeof(read_buffer), ":Sz%03d*%02d#", d, m);

    return (setStandardProcedure(fd, read_buffer));
}

int setObjAlt(int fd, double alt)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int d, m, s;
    char read_buffer[RB_MAX_LEN] = {0};

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    getSexComponents(alt, &d, &m, &s);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :SasDD*MM#
    // Set target object altitude to sDD*MM# or sDD*MM’SS# [LX 16”, Autostar, Autostar II]
    // Returns:
    // 1 Object within slew range
    // 0 Object out of slew range
    snprintf(read_buffer, sizeof(read_buffer), ":Sa%+02d*%02d#", d, m);

    return (setStandardProcedure(fd, read_buffer));
}

int setSiteName(int fd, char *siteName, int siteNum)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :SM<string>#        for site 1
    // :SN<string>#        for site 2
    // :SO<string>#        for site 3
    // :SP<string>#        for site 4
    // Set site name to be <string>. LX200s only accept 3 character strings. Other scopes accept up to 15 characters.
    // Returns:
    // 0 – Invalid
    // 1 - Valid
    switch (siteNum)
    {
        case 1:
            snprintf(read_buffer, sizeof(read_buffer), ":SM%s#", siteName);
            break;
        case 2:
            snprintf(read_buffer, sizeof(read_buffer), ":SN%s#", siteName);
            break;
        case 3:
            snprintf(read_buffer, sizeof(read_buffer), ":SO%s#", siteName);
            break;
        case 4:
            snprintf(read_buffer, sizeof(read_buffer), ":SP%s#", siteName);
            break;
        default:
            return -1;
    }

    return (setStandardProcedure(fd, read_buffer));
}

int setSlewMode(int fd, int slewMode)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int error_type;
    int nbytes_write = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :RS# // Set Slew rate to max (fastest)                // Returns: Nothing
    // :RM# // Set Slew rate to Guiding Rate (slowest)       // Returns: Nothing
    // :RC# // Set Slew rate to Centering rate (2nd slowest) // Returns: Nothing
    // :RG# // Set Slew rate to Find Rate (2nd Fastest)      // Returns: Nothing
    switch (slewMode)
    {
        case LX200_SLEW_MAX:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":RS#");
            if ((error_type = tty_write_string(fd, ":RS#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_SLEW_FIND:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":RM#");
            if ((error_type = tty_write_string(fd, ":RM#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_SLEW_CENTER:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":RC#");
            if ((error_type = tty_write_string(fd, ":RC#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_SLEW_GUIDE:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":RG#");
            if ((error_type = tty_write_string(fd, ":RG#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        default:
            break;
    }

    tcflush(fd, TCIFLUSH);
    return 0;
}

int setFocuserMotion(int fd, int motionType)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int error_type;
    int nbytes_write = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :F+# // Start Focuser moving inward (toward objective)     // Returns: None
    // :F-# // Start Focuser moving outward (away from objective) // Returns: None
    switch (motionType)
    {
        case LX200_FOCUSIN:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":F+#");
            if ((error_type = tty_write_string(fd, ":F+#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_FOCUSOUT:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":F-#");
            if ((error_type = tty_write_string(fd, ":F-#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
    }

    tcflush(fd, TCIFLUSH);
    return 0;
}

int setFocuserSpeedMode(int fd, int speedMode)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int error_type;
    int nbytes_write = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :FQ# // Halt Focuser Motion                // Returns: Nothing
    // :FS# // Set Focus speed to slowest setting // Returns: Nothing
    // :FF# // Set Focus speed to fastest setting // Returns: Nothing
    switch (speedMode)
    {
        case LX200_HALTFOCUS:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":FQ#");
            if ((error_type = tty_write_string(fd, ":FQ#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_FOCUSSLOW:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":FS#");
            if ((error_type = tty_write_string(fd, ":FS#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_FOCUSFAST:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":FF#");
            if ((error_type = tty_write_string(fd, ":FF#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
    }

    tcflush(fd, TCIFLUSH);
    return 0;
}

int setGPSFocuserSpeed(int fd, int speed)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char speed_str[8];
    int error_type;
    int nbytes_write = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :FQ# // Halt Focuser Motion // Returns: Nothing
    if (speed == 0)
    {
        DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":FQ#");
        if ((error_type = tty_write_string(fd, ":FQ#", &nbytes_write)) != TTY_OK)
            return error_type;

        tcflush(fd, TCIFLUSH);
        return 0;
    }

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :F<n># Autostar, Autostar II – set focuser speed to <n> where <n> is an ASCII digit 1..4
    // Returns: Nothing
    // All others – Not Supported
    snprintf(speed_str, 8, ":F%d#", speed);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", speed_str);

    if ((error_type = tty_write_string(fd, speed_str, &nbytes_write)) != TTY_OK)
        return error_type;

    tcflush(fd, TCIFLUSH);
    return 0;
}

int setTrackFreq(int fd, double trackF)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    // Meade Telescope Serial Command Protocol Revision 2002.10
    // :STTT.T#
    // Sets the current tracking rate to TTT.T hertz, assuming a model where a 60.0 Hertz synchronous motor will cause the RA
    // axis to make exactly one revolution in 24 hours.
    // Returns:
    // 0 – Invalid
    // 1 - Valid
    // Note: the definition :STTT.T# does not match the text.
    //
    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :STdddd.ddddddd# [Autostar II Only]
    // Sets the current tracking rate to ddd.dddd hertz, assuming a model where a 60.0000 Hertz synchronous motor will cause
    // the RA axis to make exactly one revolution in 24 hours.
    // Returns:
    // 0 – Invalid
    // 2 – Valid
    // Note1: the definition :STdddd.ddddddd# looks bogus and does not match the text.
    // Note2: the 'Valid' response value of 2 looks bogus.
    // Note3: its appendix A lists :STDDD.DDD# which differs from both the previous definition as well as the text.
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :STDDD.DDD#
    // Set the tracking rate to DDD.DDD, where DDD.DDD is a decimal number which is
    // four times the tracking rate expressed in arcseconds per second of time.
    // Returns:
    // 0 invalid
    // 1 valid
    //
    // Note: given the above definition mess the choice was made to implement :STTTT.T# which is probably what the 2002.10 spec intended.
    snprintf(read_buffer, sizeof(read_buffer), ":ST%05.01lf#", trackF);

    return (setStandardProcedure(fd, read_buffer));
}

int setPreciseTrackFreq(int fd, double trackF)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};

    // TODO see spec of setTrackFreq where none describe a :STdd.ddddd#
    snprintf(read_buffer, sizeof(read_buffer), ":ST%08.5f#", trackF);

    /* Add mutex */
    /*    std::unique_lock<std::mutex> guard(lx200CommsLock); */

    return (setStandardProcedure(fd, read_buffer));
}

/**********************************************************************
* Misc
*********************************************************************/

int Slew(int fd)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char slewNum[2];
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":MS#");

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :MS#
    // Slew to Target Object
    // Returns:
    // 0                 Slew is Possible
    // 1<string>#        Object Below Horizon w/string message
    // 2<string>#        Object Below Higher w/string message
    if ((error_type = tty_write_string(fd, ":MS#", &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read(fd, slewNum, 1, LX200_TIMEOUT, &nbytes_read);

    if (nbytes_read < 1)
    {
        DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES ERROR <%d>", error_type);
        return error_type;
    }

    /* We don't need to read the string message, just return corresponding error code */
    tcflush(fd, TCIFLUSH);

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%c>", slewNum[0]);

    error_type = slewNum[0] - '0';
    if ((error_type >= 0) && (error_type <= 9))
    {
        return error_type;
    }
    else
    {
        return -1;
    }
}

int MoveTo(int fd, int direction)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int nbytes_write = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :Mn# // Move Telescope North at current slew rate // Returns: Nothing
    // :Mw# // Move Telescope West at current slew rate  // Returns: Nothing
    // :Me# // Move Telescope East at current slew rate  // Returns: Nothing
    // :Ms# // Move Telescope South at current slew rate // Returns: Nothing
    switch (direction)
    {
        case LX200_NORTH:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Mn#");
            tty_write_string(fd, ":Mn#", &nbytes_write);
            break;
        case LX200_WEST:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Mw#");
            tty_write_string(fd, ":Mw#", &nbytes_write);
            break;
        case LX200_EAST:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Me#");
            tty_write_string(fd, ":Me#", &nbytes_write);
            break;
        case LX200_SOUTH:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Ms#");
            tty_write_string(fd, ":Ms#", &nbytes_write);
            break;
        default:
            break;
    }

    tcflush(fd, TCIFLUSH);
    return 0;
}

int SendPulseCmd(int fd, int direction, int duration_msec, bool wait_after_command, int max_wait_ms)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int nbytes_write = 0;
    char cmd[20];

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :MgnDDDD#
    // :MgsDDDD#
    // :MgeDDDD#
    // :MgwDDDD#
    // Guide telescope in the commanded direction (nsew) for the number of milliseconds indicated by the unsigned number
    // passed in the command. These commands support serial port driven guiding.
    // Returns – Nothing
    // LX200 – Not Supported
    switch (direction)
    {
        case LX200_NORTH:
            sprintf(cmd, ":Mgn%04d#", duration_msec);
            break;
        case LX200_SOUTH:
            sprintf(cmd, ":Mgs%04d#", duration_msec);
            break;
        case LX200_EAST:
            sprintf(cmd, ":Mge%04d#", duration_msec);
            break;
        case LX200_WEST:
            sprintf(cmd, ":Mgw%04d#", duration_msec);
            break;
        default:
            return 1;
    }

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", cmd);

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    tty_write_string(fd, cmd, &nbytes_write);

    tcflush(fd, TCIFLUSH);

    if(wait_after_command){
        if (duration_msec > max_wait_ms)
            duration_msec = max_wait_ms;
        struct timespec duration_ns = {.tv_sec = 0,.tv_nsec = duration_msec*1000000};
        nanosleep(&duration_ns, NULL);
    }
    return 0;
}

int HaltMovement(int fd, int direction)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int error_type;
    int nbytes_write = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :Qn# // Halt northward Slews     // Returns: Nothing
    // :Qw# // Halt westward Slews      // Returns: Nothing
    // :Qe# // Halt eastward Slews      // Returns: Nothing
    // :Qs# // Halt southward Slews     // Returns: Nothing
    // :Q#  // Halt all current slewing // Returns: Nothing
    switch (direction)
    {
        case LX200_NORTH:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Qn#");
            if ((error_type = tty_write_string(fd, ":Qn#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_WEST:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Qw#");
            if ((error_type = tty_write_string(fd, ":Qw#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_EAST:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Qe#");
            if ((error_type = tty_write_string(fd, ":Qe#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_SOUTH:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Qs#");
            if ((error_type = tty_write_string(fd, ":Qs#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_ALL:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Q#");
            if ((error_type = tty_write_string(fd, ":Q#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        default:
            return -1;
    }

    tcflush(fd, TCIFLUSH);
    return 0;
}

int abortSlew(int fd)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int error_type;
    int nbytes_write = 0;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":Q#");
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :Q#  // Halt all current slewing // Returns: Nothing
    if ((error_type = tty_write_string(fd, ":Q#", &nbytes_write)) != TTY_OK)
        return error_type;

    tcflush(fd, TCIFLUSH);
    return 0;
}

int Sync(int fd, char *matchedObject)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    const struct timespec timeout = {0, 10000000L};
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":CM#");
    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :CM#
    // Synchronizes the telescope's position with the currently selected database object's coordinates.
    // Returns:
    // LX200's - a "#" terminated string with the name of the object that was synced.
    // Autostars & Autostar II - At static string: " M31 EX GAL MAG 3.5 SZ178.0'#"
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :CM#
    // Synchronizes the position of the mount with the coordinates of the currently selected target.
    // Starting with version 2.8.15, this command has two possible behaviours depending on
    // the value passed to the last :CMCFGn# command. By default after startup, or after
    // the :CMCFG0# command has been given, the synchronization works by offsetting the
    // axis angles. If the :CMCFG1# command has been given, it works like the :CMS#
    // command, but returning the strings below.
    // Returns:
    // the string “Coordinates matched            #” if the coordinates have been synchronized
    // the string “Match fail: dist. too large#” if the coordinates have not been synchronized
    if ((error_type = tty_write_string(fd, ":CM#", &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, matchedObject, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);

    if (nbytes_read < 1)
        return error_type;

    matchedObject[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", matchedObject);

    /* Sleep 10ms before flushing. This solves some issues with LX200 compatible devices. */
    nanosleep(&timeout, nullptr);
    tcflush(fd, TCIFLUSH);

    return 0;
}

int selectSite(int fd, int siteNum)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int error_type;
    int nbytes_write = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2002.10
    // :W<n>#
    // Set current site to <n>, an ASCII digit in the range 0..3
    // Returns: Nothing
    //
    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :W<n>#
    // Set current site to <n>, an ASCII digit in the range 1..4
    // Returns: Nothing
    //
    // So Meade changed their mind on the offset :(
    // The azwing comments below implements of the 2002.10 versions.
    // TODO: auto determine which spec version to use !
    switch (siteNum)
    {
        case 1:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":W0#");
            if ((error_type = tty_write_string(fd, ":W0#", &nbytes_write)) != TTY_OK) //azwing index starts at 0 not 1
                return error_type;
            break;
        case 2:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":W1#");
            if ((error_type = tty_write_string(fd, ":W1#", &nbytes_write)) != TTY_OK) //azwing index starts at 0 not 1
                return error_type;
            break;
        case 3:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":W2#");
            if ((error_type = tty_write_string(fd, ":W2#", &nbytes_write)) != TTY_OK) //azwing index starts at 0 not 1
                return error_type;
            break;
        case 4:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":W3#");
            if ((error_type = tty_write_string(fd, ":W3#", &nbytes_write)) != TTY_OK) //azwing index starts at 0 not 1
                return error_type;
            break;
        default:
            return -1;
    }

    tcflush(fd, TCIFLUSH);
    return 0;
}

int selectCatalogObject(int fd, int catalog, int NNNN)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};
    int error_type;
    int nbytes_write = 0;

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :LSNNNN#
    // Select star NNNN as the current target object from the currently selected catalog
    // Returns: Nothing
    // Autostar II & AutoStar – Available in later firmwares
    //
    // :LCNNNN#
    // Set current target object to deep sky catalog object number NNNN
    // Returns : Nothing
    // Autostar II & Autostar – Implemented in later firmware revisions
    //
    // :LMNNNN#
    // Set current target object to Messier Object NNNN, an ASCII expressed decimal number.
    // Returns: Nothing.
    // Autostar II and Autostar – Implemented in later versions.
    switch (catalog)
    {
        case LX200_STAR_C:
            snprintf(read_buffer, sizeof(read_buffer), ":LS%d#", NNNN);
            break;
        case LX200_DEEPSKY_C:
            snprintf(read_buffer, sizeof(read_buffer), ":LC%d#", NNNN);
            break;
        case LX200_MESSIER_C:
            snprintf(read_buffer, sizeof(read_buffer), ":LM%d#", NNNN);
            break;
        default:
            return -1;
    }

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", read_buffer);

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);


    if ((error_type = tty_write_string(fd, read_buffer, &nbytes_write)) != TTY_OK)
        return error_type;

    tcflush(fd, TCIFLUSH);
    return 0;
}

int selectSubCatalog(int fd, int catalog, int subCatalog)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    char read_buffer[RB_MAX_LEN] = {0};

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :LsD#
    // Select star catalog D, an ASCII integer where D specifies:
    // 0 STAR library (Not supported on Autostar I & II)
    // 1 SAO library
    // 2 GCVS library
    // 3 Hipparcos (Autostar I & 2)
    // 4 HR (Autostar I & 2)
    // 5 HD (Autostar I & 2)
    // Returns:
    // 1 Catalog Available
    // 2 Catalog Not Found
    //
    // :LoD#
    // Select deep sky Library where D specifies
    // 0 - Objects CNGC / NGC in Autostar & Autostar II
    // 1 - Objects IC
    // 2 – UGC
    // 3 – Caldwell (Autostar & Autostar II)
    // 4 – Arp (LX200GPS/RCX)
    // 5 – Abell (LX200GPS/RCX)
    // Returns:
    // 1 Catalog available
    // 0 Catalog Not found
    switch (catalog)
    {
        case LX200_STAR_C:
            snprintf(read_buffer, sizeof(read_buffer), ":LsD%d#", subCatalog);
            break;
        case LX200_DEEPSKY_C:
            snprintf(read_buffer, sizeof(read_buffer), ":LoD%d#", subCatalog);
            break;
        case LX200_MESSIER_C:
            return 1;
        default:
            return 0;
    }

    return (setStandardProcedure(fd, read_buffer));
}

int getLX200EquatorialFormat()
{
    return eq_format;
}

int getLX200GeographicFormat()
{
    return geo_format;
}

int checkLX200EquatorialFormat(int fd)
{
    char read_buffer[RB_MAX_LEN] = {0};
    eq_format = LX200_EQ_LONG_FORMAT;
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":GR#");

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    tcflush(fd, TCIFLUSH);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :GR#
    // Get Telescope RA
    // Returns: HH:MM.T# or HH:MM:SS#
    // Depending which precision is set for the telescope
    //
    // 10Micron Mount Command Protocol software version 2.14.11 2016.11
    // :GR#
    // Get telescope right ascension. Returns the current telescope right ascension formatted as follows:
    // Emulation and precision            Return value
    // Any emulation, low precision       HH:MM.M# (hours, minutes and tenths of minutes)
    // LX200 emulation, high precision    HH:MM:SS# (hours, minutes, seconds)
    // Extended emulation, high precision HH:MM:SS.S# (hours, minutes, seconds and tenths of seconds)
    // Any emulation, ultra precision     HH:MM:SS.SS# (hours, minutes, seconds and hundredths of seconds)
    if ((error_type = tty_write_string(fd, ":GR#", &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);

    if (nbytes_read < 1)
    {
        DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES ERROR <%d>", error_type);
        return error_type;
    }

    read_buffer[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    // 10micron returns on U2 15:46:18.03 . Prevent setting it to a lower precision later by detecting this mode here.
    if (nbytes_read >= 11 && read_buffer[8] == '.')
    {
        eq_format = LX200_EQ_LONGER_FORMAT;
        DEBUGDEVICE(lx200Name, DBG_SCOPE, "Equatorial coordinate format is ultra high precision.");
        return 0;
    }

    /* If it's short format, try to toggle to high precision format */
    if (read_buffer[5] == '.')
    {
        DEBUGDEVICE(lx200Name, DBG_SCOPE, "Detected low precision equatorial format, attempting to switch to high precision.");
        // Meade Telescope Serial Command Protocol Revision 2010.10
        // :U#
        // Toggle between low/hi precision positions
        // Low - RA displays and messages HH:MM.T sDD*MM
        // High - Dec/Az/El displays and messages HH:MM:SS sDD*MM:SS
        // Returns Nothing
        //
        // 10Micron Mount Command Protocol software version 2.14.11 2016.11
        // :U#
        // Toggle between low and high precision modes. This controls the format of some values
        // that are returned by the mount. In extended LX200 emulation mode, switches always to
        // high precision (does not toggle).
        // Low precision: RA returned as HH:MM.T (hours, minutes and tenths of minutes),
        // Dec/Az/Alt returned as sDD*MM (sign, degrees, arcminutes).
        // High precision: RA returned as HH:MM:SS (hours, minutes, seconds), Dec/Az/Alt
        // returned as sDD*MM:SS (sign, degrees, arcminutes, arcseconds).
        // Returns: nothing
        // :U0#
        // Set low precision mode.
        // Returns: nothing
        // :U1#
        // Set high precision mode.
        // Returns: nothing
        // :U2#
        // Set ultra precision mode. In ultra precision mode, extra decimal digits are returned for
        // some commands, and there is no more difference between different emulation modes.
        // Returns: nothing
        // Available from version 2.10.
        if ((error_type = tty_write_string(fd, ":U#", &nbytes_write)) != TTY_OK)
            return error_type;
    }
    else if (read_buffer[8] == '.')
    {
        eq_format = LX200_EQ_LONGER_FORMAT;
        DEBUGDEVICE(lx200Name, DBG_SCOPE, "Equatorial coordinate format is ultra high precision.");
        return 0;
    }
    else
    {
        eq_format = LX200_EQ_LONG_FORMAT;
        DEBUGDEVICE(lx200Name, DBG_SCOPE, "Equatorial coordinate format is high precision.");
        return 0;
    }

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":GR#");

    tcflush(fd, TCIFLUSH);

    if ((error_type = tty_write_string(fd, ":GR#", &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_nread_section(fd, read_buffer, RB_MAX_LEN, '#', LX200_TIMEOUT, &nbytes_read);

    if (nbytes_read < 1)
    {
        DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES ERROR <%d>", error_type);
        return error_type;
    }

    read_buffer[nbytes_read - 1] = '\0';

    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s>", read_buffer);

    if (read_buffer[5] == '.')
    {
        eq_format = LX200_EQ_SHORT_FORMAT;
        DEBUGDEVICE(lx200Name, DBG_SCOPE, "Equatorial coordinate format is low precision.");
    }
    else
    {
        eq_format = LX200_EQ_LONG_FORMAT;
        DEBUGDEVICE(lx200Name, DBG_SCOPE, "Equatorial coordinate format is high precision.");
    }

    tcflush(fd, TCIFLUSH);

    return 0;
}

int selectTrackingMode(int fd, int trackMode)
{
    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int error_type;
    int nbytes_write = 0;

    /* Add mutex */
    std::unique_lock<std::mutex> guard(lx200CommsLock);

    // Meade Telescope Serial Command Protocol Revision 2010.10
    // :TQ# Selects sidereal tracking rate                     Returns: Nothing
    // :TS# Select Solar tracking rate. [LS Only]              Returns: Nothing
    // :TL# Set Lunar Tracking Rate                            Returns: Nothing
    // :TM# Select custom tracking rate [no-op in Autostar II] Returns: Nothing
    switch (trackMode)
    {
        case LX200_TRACK_SIDEREAL:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":TQ#");
            if ((error_type = tty_write_string(fd, ":TQ#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_TRACK_SOLAR:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":TS#");
            if ((error_type = tty_write_string(fd, ":TS#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_TRACK_LUNAR:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":TL#");
            if ((error_type = tty_write_string(fd, ":TL#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        case LX200_TRACK_MANUAL:
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", ":TM#");
            if ((error_type = tty_write_string(fd, ":TM#", &nbytes_write)) != TTY_OK)
                return error_type;
            break;
        default:
            return -1;
    }

    tcflush(fd, TCIFLUSH);
    return 0;
}

int setLocalTime(int fd, int x, int y, int z, bool addSpace)
{
    return setCommandXYZ(fd, x, y, z, ":SL", addSpace);
}

int setSDTime(int fd, int x, int y, int z, bool addSpace)
{
    return setCommandXYZ(fd, x, y, z, ":SS", addSpace);
}
