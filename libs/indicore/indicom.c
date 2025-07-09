/*
    INDI LIB
    Common routines used by all drivers
    Copyright (C) 2003 by Jason Harris (jharris@30doradus.org)
                          Elwood C. Downey

    This is the C version of the astronomical library in KStars
    modified by Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#define _GNU_SOURCE 1

#include "indicom.h"

#include "indidevapi.h"
#include "locale_compat.h"
#include "base64.h"

#include "config.h"

#if defined(HAVE_LIBNOVA)
#include <libnova/julian_day.h>
#include <libnova/sidereal_time.h>
#include <libnova/ln_types.h>
#include <libnova/transform.h>
#endif // HAVE_LIBNOVA

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(_WIN32) && !defined(__CYGWIN__)
#include <netinet/in.h>
#endif


#if defined(__linux__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/ioctl.h>
#endif

#ifdef __APPLE__
#include <sys/param.h>
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#ifdef __FreeBSD__
#include <sys/param.h>
#endif

#if defined(BSD) && !defined(__GNU__)
#ifdef __APPLE__
#include <IOKit/serial/ioss.h>
#endif
#include <sys/ioctl.h>
#endif

#ifdef __GNU__
#include <sys/ioctl.h>
#endif

#ifdef _WIN32
#undef CX
#undef CY
#endif

#ifndef _WIN32
#include <unistd.h>
#include <termios.h>
#include <sys/param.h>
#define PARITY_NONE 0
#define PARITY_EVEN 1
#define PARITY_ODD  2
#endif

#include "userio.h"
#include "indiuserio.h"

#if defined(_MSC_VER)
#define snprintf _snprintf
#pragma warning(push)
///@todo Introduce platform independent safe functions as macros to fix this
#pragma warning(disable : 4996)
#endif

#define MAXRBUF 2048

static int tty_debug = 0;
static int tty_gemini_udp_format = 0;
static int tty_generic_udp_format = 0;
static int tty_generic_udp_read_error_occured = 0;
static int tty_auto_reset_udp_session = 0;
static int tty_sequence_number = 1;
static int tty_clear_trailing_lf = 0;

#if defined(HAVE_LIBNOVA)
int extractISOTime(const char *timestr, struct ln_date *iso_date)
{
    struct tm utm;

    if (strptime(timestr, "%Y/%m/%dT%H:%M:%S", &utm))
    {
        ln_get_date_from_tm(&utm, iso_date);
        return (0);
    }

    if (strptime(timestr, "%Y-%m-%dT%H:%M:%S", &utm))
    {
        ln_get_date_from_tm(&utm, iso_date);
        return (0);
    }

    return (-1);
}
#endif

/* sprint the variable a in sexagesimal format into out[].
 * w is the number of spaces for the whole part.
 * fracbase is the number of pieces a whole is to broken into; valid options:
 *	360000:	<w>:mm:ss.ss
 *	36000:	<w>:mm:ss.s
 *	3600:	<w>:mm:ss
 *	600:	<w>:mm.m
 *	60:	<w>:mm
 * return number of characters written to out, not counting final '\0'.
 */
int fs_sexa(char *out, double a, int w, int fracbase)
{
    char *out0 = out;
    unsigned long n;
    int d;
    int f;
    int m;
    int s;
    int isneg;

    /* save whether it's negative but do all the rest with a positive */
    isneg = (a < 0);
    if (isneg)
        a = -a;

    /* convert to an integral number of whole portions */
    n = (unsigned long)(a * fracbase + 0.5);
    d = n / fracbase;
    f = n % fracbase;

    /* form the whole part; "negative 0" is a special case */
    if (isneg && d == 0)
        out += snprintf(out, MAXINDIFORMAT, "%*s-0", w - 2, "");
    else
        out += snprintf(out, MAXINDIFORMAT, "%*d", w, isneg ? -d : d);

    /* do the rest */
    switch (fracbase)
    {
    case 60: /* dd:mm */
        m = f / (fracbase / 60);
        out += snprintf(out, MAXINDIFORMAT, ":%02d", m);
        break;
    case 600: /* dd:mm.m */
        out += snprintf(out, MAXINDIFORMAT, ":%02d.%1d", f / 10, f % 10);
        break;
    case 3600: /* dd:mm:ss */
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        out += snprintf(out, MAXINDIFORMAT, ":%02d:%02d", m, s);
        break;
    case 36000: /* dd:mm:ss.s*/
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        out += snprintf(out, MAXINDIFORMAT, ":%02d:%02d.%1d", m, s / 10, s % 10);
        break;
    case 360000: /* dd:mm:ss.ss */
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        out += snprintf(out, MAXINDIFORMAT, ":%02d:%02d.%02d", m, s / 100, s % 100);
        break;
    default:
        printf("fs_sexa: unknown fracbase: %d\n", fracbase);
        return -1;
    }

    return (int)(out - out0);
}

/* convert sexagesimal string str AxBxC to double.
 *   x can be anything non-numeric. Any missing A, B or C will be assumed 0.
 *   optional - and + can be anywhere.
 * return 0 if ok, -1 if can't find a thing.
 */
int f_scansexa(const char *str0, /* input string */
               double *dp)       /* cracked value, if return 0 */
{
    locale_char_t *orig = indi_locale_C_numeric_push();

    double a = 0, b = 0, c = 0;
    char str[128];
    //char *neg;
    uint8_t isNegative=0;
    int r= 0;

    /* copy str0 so we can play with it */
    strncpy(str, str0, sizeof(str) - 1);
    str[sizeof(str) - 1] = '\0';

    /* remove any spaces */
    char* i = str;
    char* j = str;
    while(*j != 0)
    {
        *i = *j++;
        if(*i != ' ')
            i++;
    }
    *i = 0;

    // This has problem process numbers in scientific notations e.g. 1e-06
    /*neg = strchr(str, '-');
    if (neg)
        *neg = ' ';
    */
    if (str[0] == '-')
    {
        isNegative = 1;
        str[0] = ' ';
    }

    r = sscanf(str, "%lf%*[^0-9]%lf%*[^0-9]%lf", &a, &b, &c);

    indi_locale_C_numeric_pop(orig);

    if (r < 1)
        return (-1);
    *dp = a + b / 60 + c / 3600;
    if (isNegative)
        *dp *= -1;
    return (0);
}

void getSexComponents(double value, int *d, int *m, int *s)
{
    *d = (int32_t)fabs(value);
    *m = (int32_t)((fabs(value) - *d) * 60.0);
    *s = (int32_t)rint(((fabs(value) - *d) * 60.0 - *m) * 60.0);

    // Special case if seconds are >= 59.5 so it will be rounded by rint above
    // to 60
    if (*s == 60)
    {
        *s  = 0;
        *m += 1;
    }
    if (*m == 60)
    {
        *m  = 0;
        *d += 1;
    }

    if (value < 0)
        *d *= -1;
}

void getSexComponentsIID(double value, int *d, int *m, double *s)
{
    *d = (int32_t)fabs(value);
    *m = (int32_t)((fabs(value) - *d) * 60.0);
    *s = (double)(((fabs(value) - *d) * 60.0 - *m) * 60.0);

    if (value < 0)
        *d *= -1;
}

/* fill buf with properly formatted INumber string. return length */
int numberFormat(char *buf, const char *format, double value)
{
    int w, f, s;
    char m;

    if (sscanf(format, "%%%d.%d%c", &w, &f, &m) == 3 && m == 'm')
    {
        /* INDI sexi format */
        switch (f)
        {
        case 9:  s = 360000;  break;
        case 8:  s = 36000;   break;
        case 6:  s = 3600;    break;
        case 5:  s = 600;     break;
        default: s = 60;      break;
        }
        return (fs_sexa(buf, value, w - f, s));
    }
    else
    {
        /* normal printf format */
        return (snprintf(buf, MAXINDIFORMAT, format, value));
    }
}

/* log message locally.
 * this has nothing to do with XML or any Clients.
 */
void IDLog(const char *fmt, ...)
{
    va_list ap;
    /* JM: Since all INDI's stderr are timestampped now, we don't need to time stamp ID Log */
    /*fprintf (stderr, "%s ", indi_timestamp());*/
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

double time_ns()
{
    struct timespec ts;
#if defined(HAVE_TIMESPEC_GET)
    timespec_get(&ts, TIME_UTC);
#elif defined(HAVE_CLOCK_GETTIME)
    clock_gettime(CLOCK_REALTIME, &ts);
#elif defined(__APPLE__) // OS X does not have clock_gettime, use clock_get_time
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts.tv_sec = mts.tv_sec;
    ts.tv_nsec = mts.tv_nsec;
#else
    #error "Unsupported platform"
#endif
    return (double)ts.tv_sec+(double)(ts.tv_nsec%1000000000)/1000000000.0;
}

/* return current system time in message format */
const char *indi_timestamp()
{
    static char ts[32];
    struct tm *tp;
    time_t t;

    time(&t);
    tp = gmtime(&t);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
    return (ts);
}

void tty_set_debug(int debug)
{
    tty_debug = debug;
}

void tty_set_auto_reset_udp_session(int flag)
{
    tty_auto_reset_udp_session = flag;
}

void tty_set_gemini_udp_format(int enabled)
{
    tty_gemini_udp_format = enabled;
}

void tty_set_generic_udp_format(int enabled)
{
    tty_generic_udp_format = enabled;
}

void tty_clr_trailing_read_lf(int enabled)
{
    tty_clear_trailing_lf = enabled;
}

int tty_timeout(int fd, int timeout)
{
    return tty_timeout_microseconds(fd, timeout, 0);
}

int tty_timeout_microseconds(int fd, long timeout_seconds, long timeout_microseconds)
{
    #if defined(_WIN32) || defined(ANDROID)
    INDI_UNUSED(fd);
    INDI_UNUSED(timeout_seconds);
    INDI_UNUSED(timeout_microseconds);
    return TTY_ERRNO;
    #else

    if (fd == -1)
        return TTY_ERRNO;

    struct timeval tv;
    fd_set readout;
    int retval;

    FD_ZERO(&readout);
    FD_SET(fd, &readout);

    /* wait for 'timeout' seconds + microseconds */
    tv.tv_sec  = timeout_seconds;
    tv.tv_usec = timeout_microseconds;

    /* Wait till we have a change in the fd status */
    retval = select(fd + 1, &readout, NULL, NULL, &tv);

    /* Return 0 on successful fd change */
    if (retval > 0)
        return TTY_OK;
    /* Return -1 due to an error */
    else if (retval == -1)
        return TTY_SELECT_ERROR;
    /* Return -2 if time expires before anything interesting happens */
    else
        return TTY_TIME_OUT;

    #endif
}

int tty_write(int fd, const char *buf, int nbytes, int *nbytes_written)
{
#ifdef _WIN32
    return TTY_ERRNO;
#else
    int geminiBuffer[66]={0};
    char *buffer = (char *)buf;

    if (tty_gemini_udp_format)
    {
        buffer = (char*)geminiBuffer;
        geminiBuffer[0] = ++tty_sequence_number;
        geminiBuffer[1] = 0;
        memcpy((char *)&geminiBuffer[2], buf, nbytes);
        // Add on the 8 bytes for the header and 1 byte for the null terminator
        nbytes += 9;
    }

    if (fd == -1)
        return TTY_ERRNO;

    int bytes_w     = 0;
    *nbytes_written = 0;

    if (tty_debug)
    {
        int i = 0;
        for (i = 0; i < nbytes; i++)
            IDLog("%s: buffer[%d]=%#X (%c)\n", __FUNCTION__, i, (unsigned char)buf[i], buf[i]);
    }

    if(tty_generic_udp_format && tty_auto_reset_udp_session > 0) {
        int only_if_timeout = tty_auto_reset_udp_session == 1 ? 1 : 0;
        tty_reset_udp_session(fd,only_if_timeout);
    }

    while (nbytes > 0)
    {
        bytes_w = write(fd, buffer + (*nbytes_written), nbytes);

        if (bytes_w < 0)
            return TTY_WRITE_ERROR;

        *nbytes_written += bytes_w;
        nbytes -= bytes_w;
    }

    if (tty_gemini_udp_format)
        *nbytes_written -= 9;

    return TTY_OK;

#endif
}

int tty_write_string(int fd, const char *buf, int *nbytes_written)
{
    int nbytes;
    nbytes = (int)strlen(buf);

    return tty_write(fd, buf, nbytes, nbytes_written);
}

void tty_reset_udp_session(int fd,int only_if_timeout_happened)
{
#ifdef _WIN32
    (void)fd;
    (void)only_if_timeout_happened;
#else
    int new_fd = -1;
    struct sockaddr_in server_addr;
    struct timeval ts;

    if(!tty_generic_udp_format)
        return;
    
    if(only_if_timeout_happened && !tty_generic_udp_read_error_occured)
        return;

    if (tty_debug) {
        IDLog("%s: Request to reset session for fd %d\n", __FUNCTION__, fd);
    }
    
    new_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(new_fd < 0)
        return;
    
    socklen_t len = sizeof(server_addr);

    /* Connect to same address */
    if(getpeername(fd,(struct sockaddr *)&server_addr, &len) < 0) 
        goto error_exit;

    if (connect(new_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
        goto error_exit;

    /* copy timeout options */
    len = sizeof(ts);
    if(getsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &ts, &len) == 0) {
        setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, &ts,len);
    }
    
    len = sizeof(ts);
    if(getsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &ts, &len) == 0) {
        setsockopt(new_fd, SOL_SOCKET, SO_SNDTIMEO, &ts,len);
    }

    /* reset existing file descriptor */

    if(dup2(new_fd,fd) < 0)
        goto error_exit;
    
    
    /* reset error */
    close(new_fd);
    tty_generic_udp_read_error_occured = 0;
    return;

error_exit:
    if (tty_debug) {
        IDLog("%s: Failed to create new session for fd %d: %s\n", __FUNCTION__, fd, strerror(errno));
    }

    perror("tty_reset_udp_session");

    if(new_fd >= 0)
        close(new_fd);
#endif
}

int tty_read(int fd, char *buf, int nbytes, int timeout, int *nbytes_read)
{
    return tty_read_expanded(fd, buf, nbytes, timeout, 0, nbytes_read);
}

int tty_read_expanded(int fd, char *buf, int nbytes, long timeout_seconds, long timeout_microseconds, int *nbytes_read)
{
#ifdef _WIN32
    return TTY_ERRNO;
#else

    if (fd == -1)
        return TTY_ERRNO;

    int numBytesToRead =  nbytes;
    int bytesRead = 0;
    int err       = 0;
    *nbytes_read  = 0;

    if (nbytes <= 0)
        return TTY_PARAM_ERROR;

    if (tty_debug)
        IDLog("%s: Request to read %d bytes with %ld s, %ld us timeout for fd %d\n", __FUNCTION__, nbytes, timeout_seconds, timeout_microseconds, fd);

    char geminiBuffer[257]={0};
    char* buffer = buf;

    if (tty_gemini_udp_format)
    {
        numBytesToRead = nbytes + 8;
        buffer = geminiBuffer;
    }

    while (numBytesToRead > 0)
    {
        if ((err = tty_timeout_microseconds(fd, timeout_seconds, timeout_microseconds))) {
            if(tty_generic_udp_format)
                tty_generic_udp_read_error_occured = 1;
            return err;
        }

        bytesRead = read(fd, buffer + (*nbytes_read), ((uint32_t)numBytesToRead));

        if (bytesRead < 0)
            return TTY_READ_ERROR;

        if (tty_debug)
        {
            IDLog("%d bytes read and %d bytes remaining...\n", bytesRead, numBytesToRead - bytesRead);
            int i = 0;
            for (i = *nbytes_read; i < (*nbytes_read + bytesRead); i++)
                IDLog("%s: buffer[%d]=%#X (%c)\n", __FUNCTION__, i, (unsigned char)buf[i], buf[i]);
        }

        if (*nbytes_read == 0 && tty_clear_trailing_lf && *buffer == 0x0A)
        {
            if (tty_debug)
                IDLog("%s: Cleared LF char left in buf\n", __FUNCTION__);

            memcpy(buffer, buffer+1,bytesRead);
            --bytesRead;
        }

        *nbytes_read += bytesRead;
        numBytesToRead -= bytesRead;
    }


    if (tty_gemini_udp_format)
    {
        int *intSizedBuffer = (int *)geminiBuffer;
        if (intSizedBuffer[0] != tty_sequence_number)
        {
            // Not the right reply just do the read again.
            return tty_read_expanded(fd, buf, nbytes, timeout_seconds, timeout_microseconds, nbytes_read);
        }

        *nbytes_read -= 8;
        memcpy(buf, geminiBuffer+8, *nbytes_read);
    }

    return TTY_OK;

#endif
}

int tty_read_section(int fd, char *buf, char stop_char, int timeout, int *nbytes_read)
{
    return tty_read_section_expanded(fd, buf, stop_char, (long) timeout, (long) 0, nbytes_read);
}

int tty_read_section_expanded(int fd, char *buf, char stop_char, long timeout_seconds, long timeout_microseconds, int *nbytes_read)
{
#ifdef _WIN32
    return TTY_ERRNO;
#else

    char readBuffer[257]={0};

    if (fd == -1)
        return TTY_ERRNO;

    int bytesRead = 0;
    int err       = TTY_OK;
    *nbytes_read  = 0;

    uint8_t *read_char = 0;

    if (tty_debug)
        IDLog("%s: Request to read until stop char '%#02X' with %ld s %ld us timeout for fd %d\n", __FUNCTION__, stop_char, timeout_seconds, timeout_microseconds, fd);

    if (tty_gemini_udp_format)
    {
        if ((err = tty_timeout_microseconds(fd, timeout_seconds, timeout_microseconds)))
            return err;
        bytesRead = read(fd, readBuffer, 255);

        if (bytesRead < 0)
            return TTY_READ_ERROR;

        int *intSizedBuffer = (int *)readBuffer;
        if (intSizedBuffer[0] != tty_sequence_number)
        {
            // Not the right reply just do the read again.
            return tty_read_section_expanded(fd, buf, stop_char, timeout_seconds, timeout_microseconds, nbytes_read);
        }

        for (int index = 8; index < bytesRead; index++)
        {
            (*nbytes_read)++;

            if (*(readBuffer+index) == stop_char)
            {
                strncpy(buf, readBuffer+8, *nbytes_read);
                return TTY_OK;
            }
        }
    }
    else if (tty_generic_udp_format)
    {
        if ((err = tty_timeout_microseconds(fd, timeout_seconds, timeout_microseconds))) {
            tty_generic_udp_read_error_occured = 1;
            return err;
        }
        bytesRead = read(fd, readBuffer, 255);
        if (bytesRead < 0)
            return TTY_READ_ERROR;
        for (int index = 0; index < bytesRead; index++)
        {
            (*nbytes_read)++;

            if (*(readBuffer+index) == stop_char)
            {
                strncpy(buf, readBuffer, *nbytes_read);
                return TTY_OK;
            }
        }
    }
    else
    {
        for (;;)
        {
            if ((err = tty_timeout_microseconds(fd, timeout_seconds, timeout_microseconds)))
                return err;

            read_char = (uint8_t*)(buf + *nbytes_read);
            bytesRead = read(fd, read_char, 1);

            if (bytesRead < 0)
                return TTY_READ_ERROR;

            if (tty_debug)
                IDLog("%s: buffer[%d]=%#X (%c)\n", __FUNCTION__, (*nbytes_read), *read_char, *read_char);

            if (!(tty_clear_trailing_lf && *read_char == 0X0A && *nbytes_read == 0))
                (*nbytes_read)++;
            else {
                if (tty_debug)
                    IDLog("%s: Cleared LF char left in buf\n", __FUNCTION__);
            }

            if (*read_char == stop_char) {
                return TTY_OK;
            }
        }
    }

    return TTY_TIME_OUT;

#endif
}

int tty_nread_section(int fd, char *buf, int nsize, char stop_char, int timeout, int *nbytes_read)
{
    return tty_nread_section_expanded(fd, buf, nsize, stop_char, (long) timeout, (long) 0, nbytes_read);
}

int tty_nread_section_expanded(int fd, char *buf, int nsize, char stop_char, long timeout_seconds, long timeout_microseconds, int *nbytes_read)
{
#ifdef _WIN32
    return TTY_ERRNO;
#else

    if (fd == -1)
        return TTY_ERRNO;

    // For Gemini
    if (tty_gemini_udp_format || tty_generic_udp_format)
        return tty_read_section_expanded(fd, buf, stop_char, timeout_seconds, timeout_microseconds, nbytes_read);

    int bytesRead = 0;
    int err       = TTY_OK;
    *nbytes_read  = 0;
    uint8_t *read_char = 0;
    memset(buf, 0, nsize);

    if (tty_debug)
        IDLog("%s: Request to read until stop char '%#02X' with %ld s %ld us timeout for fd %d\n", __FUNCTION__, stop_char, timeout_seconds, timeout_microseconds, fd);

    for (;;)
    {
        if ((err = tty_timeout_microseconds(fd, timeout_seconds, timeout_microseconds)))
            return err;

        read_char = (uint8_t*)(buf + *nbytes_read);
        bytesRead = read(fd, read_char, 1);

        if (bytesRead < 0)
            return TTY_READ_ERROR;

        if (tty_debug)
            IDLog("%s: buffer[%d]=%#X (%c)\n", __FUNCTION__, (*nbytes_read), *read_char, *read_char);

        if (!(tty_clear_trailing_lf && *read_char == 0X0A && *nbytes_read == 0))
            (*nbytes_read)++;
        else {
            if (tty_debug)
                IDLog("%s: Cleared LF char left in buf\n", __FUNCTION__);
        }

        if (*read_char == stop_char)
            return TTY_OK;
        else if (*nbytes_read >= nsize)
            return TTY_OVERFLOW;
    }

#endif
}


#if defined(BSD) && !defined(__GNU__)
// BSD - OSX version
int tty_connect(const char *device, int bit_rate, int word_size, int parity, int stop_bits, int *fd)
{
    int t_fd = -1;
    int bps;
    char msg[80];
    int handshake;
    struct termios tty_setting;

    // Open the serial port read/write, with no controlling terminal, and don't wait for a connection.
    // The O_NONBLOCK flag also causes subsequent I/O on the device to be non-blocking.
    // See open(2) ("man 2 open") for details.

    t_fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (t_fd == -1)
    {
        IDLog("Error opening serial port (%s) - %s(%d).\n", device, strerror(errno), errno);
        goto error;
    }

    // Note that open() follows POSIX semantics: multiple open() calls to the same file will succeed
    // unless the TIOCEXCL ioctl is issued. This will prevent additional opens except by root-owned
    // processes.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.

    if (ioctl(t_fd, TIOCEXCL) == -1)
    {
        IDLog("Error setting TIOCEXCL on %s - %s(%d).\n", device, strerror(errno), errno);
        goto error;
    }

    // Now that the device is open, clear the O_NONBLOCK flag so subsequent I/O will block.
    // See fcntl(2) ("man 2 fcntl") for details.

    if (fcntl(t_fd, F_SETFL, 0) == -1)
    {
        IDLog("Error clearing O_NONBLOCK %s - %s(%d).\n", device, strerror(errno), errno);
        goto error;
    }

    // Get the current options and save them so we can restore the default settings later.
    if (tcgetattr(t_fd, &tty_setting) == -1)
    {
        IDLog("Error getting tty attributes %s - %s(%d).\n", device, strerror(errno), errno);
        goto error;
    }

    // Set raw input (non-canonical) mode, with reads blocking until either a single character
    // has been received or a one second timeout expires.
    // See tcsetattr(4) ("man 4 tcsetattr") and termios(4) ("man 4 termios") for details.

    cfmakeraw(&tty_setting);
    tty_setting.c_cc[VMIN]  = 1;
    tty_setting.c_cc[VTIME] = 10;

    // The baud rate, word length, and handshake options can be set as follows:
    switch (bit_rate)
    {
    case 0:      bps = B0;      break;
    case 50:     bps = B50;     break;
    case 75:     bps = B75;     break;
    case 110:    bps = B110;    break;
    case 134:    bps = B134;    break;
    case 150:    bps = B150;    break;
    case 200:    bps = B200;    break;
    case 300:    bps = B300;    break;
    case 600:    bps = B600;    break;
    case 1200:   bps = B1200;   break;
    case 1800:   bps = B1800;   break;
    case 2400:   bps = B2400;   break;
    case 4800:   bps = B4800;   break;
    case 9600:   bps = B9600;   break;
    case 19200:  bps = B19200;  break;
    case 38400:  bps = B38400;  break;
    case 57600:  bps = B57600;  break;
    case 115200: bps = B115200; break;
    case 230400: bps = B230400; break;
#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
    case 460800: bps = B460800; break;
    case 576000: bps = B576000; break;
    case 921600: bps = B921600; break;
#endif
    default:
        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid bit rate.", bit_rate) < 0)
            perror(NULL);
        else
            perror(msg);
        return TTY_PARAM_ERROR;
    }

    cfsetspeed(&tty_setting, bps); // Set baud rate
    /* word size */
    switch (word_size)
    {
    case 5: tty_setting.c_cflag |= CS5; break;
    case 6: tty_setting.c_cflag |= CS6; break;
    case 7: tty_setting.c_cflag |= CS7; break;
    case 8: tty_setting.c_cflag |= CS8; break;
    default:
        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid data bit count.", word_size) < 0)
            perror(NULL);
        else
            perror(msg);

        return TTY_PARAM_ERROR;
    }

    /* parity */
    switch (parity)
    {
    case PARITY_NONE:
        break;
    case PARITY_EVEN:
        tty_setting.c_cflag |= PARENB;
        break;
    case PARITY_ODD:
        tty_setting.c_cflag |= PARENB | PARODD;
        break;
    default:
        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid parity selection value.", parity) < 0)
            perror(NULL);
        else
            perror(msg);

        return TTY_PARAM_ERROR;
    }

    /* stop_bits */
    switch (stop_bits)
    {
    case 1:
        break;
    case 2:
        tty_setting.c_cflag |= CSTOPB;
        break;
    default:
        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid stop bit count.", stop_bits) < 0)
            perror(NULL);
        else
            perror(msg);

        return TTY_PARAM_ERROR;
    }

#if defined(MAC_OS_X_VERSION_10_4) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4)
    // Starting with Tiger, the IOSSIOSPEED ioctl can be used to set arbitrary baud rates
    // other than those specified by POSIX. The driver for the underlying serial hardware
    // ultimately determines which baud rates can be used. This ioctl sets both the input
    // and output speed.

    speed_t speed = 14400; // Set 14400 baud
    if (ioctl(t_fd, IOSSIOSPEED, &speed) == -1)
    {
        IDLog("Error calling ioctl(..., IOSSIOSPEED, ...) - %s(%d).\n", strerror(errno), errno);
    }
#endif

    // Cause the new options to take effect immediately.
    if (tcsetattr(t_fd, TCSANOW, &tty_setting) == -1)
    {
        IDLog("Error setting tty attributes %s - %s(%d).\n", device, strerror(errno), errno);
        goto error;
    }

    // To set the modem handshake lines, use the following ioctls.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.

    if (ioctl(t_fd, TIOCSDTR) == -1) // Assert Data Terminal Ready (DTR)
    {
        IDLog("Error asserting DTR %s - %s(%d).\n", device, strerror(errno), errno);
    }

    if (ioctl(t_fd, TIOCCDTR) == -1) // Clear Data Terminal Ready (DTR)
    {
        IDLog("Error clearing DTR %s - %s(%d).\n", device, strerror(errno), errno);
    }

    handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
    if (ioctl(t_fd, TIOCMSET, &handshake) == -1)
        // Set the modem lines depending on the bits set in handshake
    {
        IDLog("Error setting handshake lines %s - %s(%d).\n", device, strerror(errno), errno);
    }

    // To read the state of the modem lines, use the following ioctl.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.

    if (ioctl(t_fd, TIOCMGET, &handshake) == -1)
        // Store the state of the modem lines in handshake
    {
        IDLog("Error getting handshake lines %s - %s(%d).\n", device, strerror(errno), errno);
    }

    IDLog("Handshake lines currently set to %d\n", handshake);

#if defined(MAC_OS_X_VERSION_10_3) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_3)
    unsigned long mics = 1UL;

    // Set the receive latency in microseconds. Serial drivers use this value to determine how often to
    // dequeue characters received by the hardware. Most applications don't need to set this value: if an
    // app reads lines of characters, the app can't do anything until the line termination character has been
    // received anyway. The most common applications which are sensitive to read latency are MIDI and IrDA
    // applications.

    if (ioctl(t_fd, IOSSDATALAT, &mics) == -1)
    {
        // set latency to 1 microsecond
        IDLog("Error setting read latency %s - %s(%d).\n", device, strerror(errno), errno);
        goto error;
    }
#endif

    *fd = t_fd;
    /* return success */
    return TTY_OK;

    // Failure path
error:
    if (t_fd != -1)
    {
        close(t_fd);
        *fd = -1;
    }

    return TTY_PORT_FAILURE;
}
#else
int tty_connect(const char *device, int bit_rate, int word_size, int parity, int stop_bits, int *fd)
{
#ifdef _WIN32
    return TTY_PORT_FAILURE;

#else
    int t_fd = -1;
    int i = 0;
    char msg[128]={0};
    int bps;
    struct termios tty_setting;
    // Check for bluetooth & virtualcom which can be shared
    int ignore_exclusive_close = strstr(device, "rfcomm") || strstr(device, "Bluetooth") || strstr(device, "virtualcom");


    // Open as Read/Write, no fnctl, and close on exclusive
    for (i = 0 ; i < 3 ; i++)
    {
        // Do not use O_CLOEXEC when ignored
        t_fd = open(device, O_RDWR | O_NOCTTY | (ignore_exclusive_close ? 0 : O_CLOEXEC));
        if (t_fd > 0)
            break;
        else
        {
            *fd = -1;
            if (errno == EBUSY)
            {
                usleep(1e6);
                continue;
            }
            else
                return TTY_PORT_FAILURE;
        }
    }

    if (t_fd == -1)
        return TTY_PORT_BUSY;

#if !defined(__CYGWIN__)
    // Set port in exclusive mode to prevent other non-root processes from opening it.
    // JM 2019-08-12: Do not set it when ignored
    if (ignore_exclusive_close == 0 && ioctl(t_fd, TIOCEXCL) == -1)
    {
        perror("tty_connect: Error setting TIOCEXC.");
        close(t_fd);
        return TTY_PORT_FAILURE;
    }
#endif

    // Get the current options and save them so we can restore the default settings later.
    if (tcgetattr(t_fd, &tty_setting) == -1)
    {
        perror("tty_connect: failed getting tty attributes.");
        close(t_fd);
        return TTY_PORT_FAILURE;
    }

    /* Control Modes
    Set bps rate */
    switch (bit_rate)
    {
    case 0:      bps = B0;      break;
    case 50:     bps = B50;     break;
    case 75:     bps = B75;     break;
    case 110:    bps = B110;    break;
    case 134:    bps = B134;    break;
    case 150:    bps = B150;    break;
    case 200:    bps = B200;    break;
    case 300:    bps = B300;    break;
    case 600:    bps = B600;    break;
    case 1200:   bps = B1200;   break;
    case 1800:   bps = B1800;   break;
    case 2400:   bps = B2400;   break;
    case 4800:   bps = B4800;   break;
    case 9600:   bps = B9600;   break;
    case 19200:  bps = B19200;  break;
    case 38400:  bps = B38400;  break;
    case 57600:  bps = B57600;  break;
    case 115200: bps = B115200; break;
    case 230400: bps = B230400; break;
    case 460800: bps = B460800; break;
    case 576000: bps = B576000; break;
    case 921600: bps = B921600; break;
    default:
        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid bit rate.", bit_rate) < 0)
            perror(NULL);
        else
            perror(msg);
        close(t_fd);
        return TTY_PARAM_ERROR;
    }
    if ((cfsetispeed(&tty_setting, bps) < 0) || (cfsetospeed(&tty_setting, bps) < 0))
    {
        perror("tty_connect: failed setting bit rate.");
        close(t_fd);
        return TTY_PORT_FAILURE;
    }

    /* Control Modes
    set no flow control word size, parity and stop bits.
    Also don't hangup automatically and ignore modem status.
    Finally enable receiving characters. */
    tty_setting.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | HUPCL | CRTSCTS);
    tty_setting.c_cflag |= (CLOCAL | CREAD);

    /* word size */
    switch (word_size)
    {
    case 5: tty_setting.c_cflag |= CS5; break;
    case 6: tty_setting.c_cflag |= CS6; break;
    case 7: tty_setting.c_cflag |= CS7; break;
    case 8: tty_setting.c_cflag |= CS8; break;
    default:

        fprintf(stderr, "Default\n");
        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid data bit count.", word_size) < 0)
            perror(NULL);
        else
            perror(msg);

        close(t_fd);
        return TTY_PARAM_ERROR;
    }

    /* parity */
    switch (parity)
    {
    case PARITY_NONE:
        break;
    case PARITY_EVEN:
        tty_setting.c_cflag |= PARENB;
        break;
    case PARITY_ODD:
        tty_setting.c_cflag |= PARENB | PARODD;
        break;
    default:

        fprintf(stderr, "Default1\n");
        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid parity selection value.", parity) < 0)
            perror(NULL);
        else
            perror(msg);

        close(t_fd);
        return TTY_PARAM_ERROR;
    }

    /* stop_bits */
    switch (stop_bits)
    {
    case 1:
        break;
    case 2:
        tty_setting.c_cflag |= CSTOPB;
        break;
    default:
        fprintf(stderr, "Default2\n");
        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid stop bit count.", stop_bits) < 0)
            perror(NULL);
        else
            perror(msg);

        close(t_fd);
        return TTY_PARAM_ERROR;
    }
    /* Control Modes complete */

    /* Ignore bytes with parity errors and make terminal raw and dumb.*/
    tty_setting.c_iflag &= ~(PARMRK | ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON | IXANY);
    tty_setting.c_iflag |= INPCK | IGNPAR | IGNBRK;

    /* Raw output.*/
    tty_setting.c_oflag &= ~(OPOST | ONLCR);

    /* Local Modes
    Don't echo characters. Don't generate signals.
    Don't process any characters.*/
    tty_setting.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN | NOFLSH | TOSTOP);
    tty_setting.c_lflag |= NOFLSH;

    /* blocking read until 1 char arrives */
    tty_setting.c_cc[VMIN]  = 1;
    tty_setting.c_cc[VTIME] = 0;

    /* now clear input and output buffers and activate the new terminal settings */
    tcflush(t_fd, TCIOFLUSH);
    if (tcsetattr(t_fd, TCSANOW, &tty_setting))
    {
        perror("tty_connect: failed setting attributes on serial port.");
        tty_disconnect(t_fd);
        return TTY_PORT_FAILURE;
    }

    *fd = t_fd;
    /* return success */
    return TTY_OK;
#endif
}
// Unix - Linux version

#endif

int tty_disconnect(int fd)
{
    if (fd == -1)
        return TTY_ERRNO;

#ifdef _WIN32
    return TTY_ERRNO;
#else
    int err;
    tcflush(fd, TCIOFLUSH);
    err = close(fd);

    if (err != 0)
        return TTY_ERRNO;

    return TTY_OK;
#endif
}

void tty_error_msg(int err_code, char *err_msg, int err_msg_len)
{
    switch (err_code)
    {
    case TTY_OK:
        snprintf(err_msg, err_msg_len, "No Error");
        break;

    case TTY_READ_ERROR:
        snprintf(err_msg, err_msg_len, "Read Error: %s", strerror(errno));
        break;

    case TTY_WRITE_ERROR:
        snprintf(err_msg, err_msg_len, "Write Error: %s", strerror(errno));
        break;

    case TTY_SELECT_ERROR:
        snprintf(err_msg, err_msg_len, "Select Error: %s", strerror(errno));
        break;

    case TTY_TIME_OUT:
        snprintf(err_msg, err_msg_len, "Timeout error");
        break;

    case TTY_PORT_FAILURE:
        if (errno == EACCES)
            snprintf(err_msg, err_msg_len,
                     "Port failure Error: %s. Try adding your user to the dialout group and restart (sudo adduser "
                     "$USER dialout)",
                     strerror(errno));
        else
            snprintf(err_msg, err_msg_len, "Port failure Error: %s. Check if device is connected to this port.",
                     strerror(errno));

        break;

    case TTY_PARAM_ERROR:
        snprintf(err_msg, err_msg_len, "Parameter error");
        break;

    case TTY_ERRNO:
        snprintf(err_msg, err_msg_len, "%s", strerror(errno));
        break;

    case TTY_OVERFLOW:
        snprintf(err_msg, err_msg_len, "Read overflow");
        break;

    case TTY_PORT_BUSY:
        snprintf(err_msg, err_msg_len, "Port is busy");
        break;

    default:
        snprintf(err_msg, err_msg_len, "Error: unrecognized error code");
        break;
    }
}

double rangeHA(double r)
{
    double res = r;
    while (res < -12.0)
        res += 24.0;
    while (res >= 12.0)
        res -= 24.0;
    return res;
}

double range24(double r)
{
    double res = r;
    while (res < 0.0)
        res += 24.0;
    while (res > 24.0)
        res -= 24.0;
    return res;
}

double range360(double r)
{
    double res = r;
    while (res < 0.0)
        res += 360.0;
    while (res > 360.0)
        res -= 360.0;
    return res;
}

double range180(double r)
{
    double res = r;
    while (res < -180.0)
        res += 360.0;
    while (res > 180.0)
        res -= 360.0;
    return res;
}

double rangeDec(double decdegrees)
{
    if ((decdegrees >= 270.0) && (decdegrees <= 360.0))
        return (decdegrees - 360.0);
    if ((decdegrees >= 180.0) && (decdegrees < 270.0))
        return (180.0 - decdegrees);
    if ((decdegrees >= 90.0) && (decdegrees < 180.0))
        return (180.0 - decdegrees);
    return decdegrees;
}

#if defined(HAVE_LIBNOVA)
double get_local_sidereal_time(double longitude)
{
    return range24(ln_get_apparent_sidereal_time(ln_get_julian_from_sys()) + longitude / 15.0);
}

void get_hrz_from_equ(struct ln_equ_posn *object, struct ln_lnlat_posn *observer, double JD, struct ln_hrz_posn *position)
{
    ln_get_hrz_from_equ(object, observer, JD, position);
    position->az -= 180;
    if (position->az < 0)
        position->az += 360;
}

void get_equ_from_hrz(struct ln_hrz_posn *object, struct ln_lnlat_posn *observer, double JD, struct ln_equ_posn *position)
{
    struct ln_hrz_posn libnova_object;
    libnova_object.az = object->az + 180;
    if (libnova_object.az > 360)
        libnova_object.az -= 360;
    libnova_object.alt = object->alt;

    ln_get_equ_from_hrz(&libnova_object, observer, JD, position);
}

#endif // HAVE_LIBNOVA

double get_local_hour_angle(double sideral_time, double ra)
{
    double HA = sideral_time - ra;
    return rangeHA(HA);
}

void get_alt_az_coordinates(double Ha, double Dec, double Lat, double* Alt, double *Az)
{
    double alt, az;
    Ha *= M_PI / 180.0;
    Dec *= M_PI / 180.0;
    Lat *= M_PI / 180.0;
    alt = asin(sin(Dec) * sin(Lat) + cos(Dec) * cos(Lat) * cos(Ha));
    az = acos((sin(Dec) - sin(alt)*sin(Lat)) / (cos(alt) * cos(Lat)));
    alt *= 180.0 / M_PI;
    az *= 180.0 / M_PI;
    if (sin(Ha) >= 0.0)
        az = 360 - az;
    *Alt = alt;
    *Az = az;
}

double estimate_geocentric_elevation(double Lat, double El)
{
    Lat *= M_PI / 180.0;
    Lat = sin(Lat);
    El += Lat * (EARTHRADIUSPOLAR - EARTHRADIUSEQUATORIAL);
    return El;
}

double estimate_field_rotation_rate(double Alt, double Az, double Lat)
{
    Alt *= M_PI / 180.0;
    Az *= M_PI / 180.0;
    Lat *= M_PI / 180.0;
    double ret = cos(Lat) * cos(Az) / cos(Alt);
    ret *= 180.0 / M_PI;
    return ret;
}

double estimate_field_rotation(double HA, double rate)
{
    HA *= rate;
    while(HA >= 360.0)
        HA -= 360.0;
    while(HA < 0)
        HA += 360.0;
    return HA;
}

double as2rad(double as)
{
    return as * M_PI / (60.0*60.0*12.0);
}

double rad2as(double rad)
{
    return rad * (60.0*60.0*12.0) / M_PI;
}

double estimate_distance(double parsecs, double parallax_radius)
{
    return parallax_radius / sin(as2rad(parsecs));
}

double m2au(double m)
{
    return m / ASTRONOMICALUNIT;
}

double calc_delta_magnitude(double mag_ratio, double *spectrum, double *ref_spectrum, int spectrum_size)
{
    double delta_mag = 0;
    for(int l = 0; l < spectrum_size; l++) {
        delta_mag += spectrum[l] * mag_ratio * ref_spectrum[l] / spectrum[l];
    }
    delta_mag /= spectrum_size;
    return delta_mag;
}

double calc_star_mass(double delta_mag, double ref_size)
{
    return delta_mag * ref_size;
}

double estimate_orbit_radius(double obs_lambda, double ref_lambda, double period)
{
    return M_PI*2*DOPPLER(REDSHIFT(obs_lambda, ref_lambda), LIGHTSPEED)/period;
}

double estimate_secondary_mass(double star_mass, double star_drift, double orbit_radius)
{
    return orbit_radius*pow(star_drift*orbit_radius, 3)*3*star_mass;
}

double estimate_secondary_size(double star_size, double dropoff_ratio)
{
    return pow(dropoff_ratio*pow(star_size, 2), 0.5);
}

double calc_photon_flux(double rel_magnitude, double filter_bandwidth, double wavelength, double steradian)
{
    return pow(10, rel_magnitude*-0.4)*(LUMEN(wavelength)*steradian*filter_bandwidth);
}

double calc_rel_magnitude(double photon_flux, double filter_bandwidth, double wavelength, double steradian)
{
    return pow(10, 1.0/(photon_flux/(LUMEN(wavelength)*steradian*filter_bandwidth)))/-0.4;
}

double estimate_absolute_magnitude(double delta_dist, double delta_mag)
{
    return sqrt(delta_dist) * delta_mag;
}

void baseline_2d_projection(double alt, double az, double baseline[3], double wavelength, double uvresult[2])
{
    az *= M_PI / 180.0;
    alt *= M_PI / 180.0;
    uvresult[0] = (baseline[0] * sin(az) + baseline[1] * cos(az));
    uvresult[1] = (baseline[1] * sin(alt) * sin(az) - baseline[0] * sin(alt) * cos(az) + baseline[2] * cos(alt));
    uvresult[0] *= AIRY / wavelength;
    uvresult[1] *= AIRY / wavelength;
}

double baseline_delay(double alt, double az, double baseline[3])
{
    az *= M_PI / 180.0;
    alt *= M_PI / 180.0;
    return cos(az) * baseline[1] * cos(alt) - baseline[0] * sin(az) * cos(alt) + sin(alt) * baseline[2];
}

#if defined(_MSC_VER)
#undef snprintf
#pragma warning(pop)
#endif
