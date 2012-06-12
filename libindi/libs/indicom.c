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

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/param.h>

#include <config.h>

#ifdef HAVE_NOVA_H
#include <libnova.h>
#endif

#include "indicom.h"
#ifdef _WIN32
#undef CX
#undef CY
#endif

#ifndef _WIN32
#include <termios.h>
#define PARITY_NONE    0
#define PARITY_EVEN    1
#define PARITY_ODD     2
#endif

#define MAXRBUF         2048

#include "indidevapi.h"

void getSexComponents(double value, int *d, int *m, int *s);


int extractISOTime(char *timestr, struct ln_date *iso_date)
{
  #ifdef HAVE_NOVA_H	
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
  #endif
  
  return (-1);
}


/* sprint the variable a in sexagesimal format into out[].
 * w is the number of spaces for the whole part.
 * fracbase is the number of pieces a whole is to broken into; valid options:
 *	360000:	<w>:mm:ss.ss
 *	36000:	<w>:mm:ss.s
 *	3600:	<w>:mm:ss
 *	600:	<w>:mm.m
 *	60:	<w>:mm
 * return number of characters written to out, not counting finaif (NOVA_FOUND)
    include_directories(${NOVA_INCLUDE_DIR})
endif (NOVA_FOUND)l '\0'.
 */
int
fs_sexa (char *out, double a, int w, int fracbase)
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
	d = n/fracbase;
	f = n%fracbase;

	/* form the whole part; "negative 0" is a special case */
	if (isneg && d == 0)
	    out += sprintf (out, "%*s-0", w-2, "");
	else
	    out += sprintf (out, "%*d", w, isneg ? -d : d);

	/* do the rest */
	switch (fracbase) {
	case 60:	/* dd:mm */
	    m = f/(fracbase/60);
	    out += sprintf (out, ":%02d", m);
	    break;
	case 600:	/* dd:mm.m */
	    out += sprintf (out, ":%02d.%1d", f/10, f%10);
	    break;
	case 3600:	/* dd:mm:ss */
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    out += sprintf (out, ":%02d:%02d", m, s);
	    break;
	case 36000:	/* dd:mm:ss.s*/
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    out += sprintf (out, ":%02d:%02d.%1d", m, s/10, s%10);
	    break;
	case 360000:	/* dd:mm:ss.ss */
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    out += sprintf (out, ":%02d:%02d.%02d", m, s/100, s%100);
	    break;
	default:
	    printf ("fs_sexa: unknown fracbase: %d\n", fracbase);
            return -1;
	}

	return (out - out0);
}

/* convert sexagesimal string str AxBxC to double.
 *   x can be anything non-numeric. Any missing A, B or C will be assumed 0.
 *   optional - and + can be anywhere.
 * return 0 if ok, -1 if can't find a thing.
 */
int
f_scansexa (
const char *str0,	/* input string */
double *dp)		/* cracked value, if return 0 */
{
	double a = 0, b = 0, c = 0;
	char str[128];
	char *neg;
	int r;

	/* copy str0 so we can play with it */
	strncpy (str, str0, sizeof(str)-1);
	str[sizeof(str)-1] = '\0';

	neg = strchr(str, '-');
	if (neg)
	    *neg = ' ';
	r = sscanf (str, "%lf%*[^0-9]%lf%*[^0-9]%lf", &a, &b, &c);
	if (r < 1)
	    return (-1);
	*dp = a + b/60 + c/3600;
	if (neg)
	    *dp *= -1;
	return (0);
}

void getSexComponents(double value, int *d, int *m, int *s)
{

  *d = (int) fabs(value);
  *m = (int) ((fabs(value) - *d) * 60.0);
  *s = (int) rint(((fabs(value) - *d) * 60.0 - *m) *60.0);

  if (value < 0)
   *d *= -1;
}

/* fill buf with properly formatted INumber string. return length */
int
numberFormat (char *buf, const char *format, double value)
{
        int w, f, s;
        char m;

        if (sscanf (format, "%%%d.%d%c", &w, &f, &m) == 3 && m == 'm') {
            /* INDI sexi format */
            switch (f) {
            case 9:  s = 360000; break;
            case 8:  s = 36000;  break;
            case 6:  s = 3600;   break;
            case 5:  s = 600;    break;
            default: s = 60;     break;
            }
            return (fs_sexa (buf, value, w-f, s));
        } else {
            /* normal printf format */
            return (sprintf (buf, format, value));
        }
}

/* log message locally.
 * this has nothing to do with XML or any Clients.
 */
void
IDLog (const char *fmt, ...)
{
	va_list ap;
	/* JM: Since all INDI's stderr are timestampped now, we don't need to time stamp ID Log */
	/*fprintf (stderr, "%s ", timestamp());*/
	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	va_end (ap);
}

/* return current system time in message format */
const char *
timestamp()
{
	static char ts[32];
	struct tm *tp;
	time_t t;

	time (&t);
	tp = gmtime (&t);
	strftime (ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
	return (ts);
}

int tty_timeout(int fd, int timeout)
{
 if (fd == -1)
        return TTY_ERRNO;

  struct timeval tv;
  fd_set readout;
  int retval;

  FD_ZERO(&readout);
  FD_SET(fd, &readout);

  /* wait for 'timeout' seconds */
  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  /* Wait till we have a change in the fd status */
  retval = select (fd+1, &readout, NULL, NULL, &tv);

  /* Return 0 on successful fd change */
  if (retval > 0)
   return TTY_OK;
  /* Return -1 due to an error */
  else if (retval == -1)
   return TTY_SELECT_ERROR;
  /* Return -2 if time expires before anything interesting happens */
  else 
    return TTY_TIME_OUT;
  
}

int tty_write(int fd, const char * buf, int nbytes, int *nbytes_written)
{
    if (fd == -1)
           return TTY_ERRNO;

  int bytes_w = 0;   
  *nbytes_written = 0;
   
  while (nbytes > 0)
  {
    
    bytes_w = write(fd, buf, nbytes);

    if (bytes_w < 0)
     return TTY_WRITE_ERROR;

    *nbytes_written += bytes_w;
    buf += bytes_w;
    nbytes -= bytes_w;
  }

  return TTY_OK;
}

int tty_write_string(int fd, const char * buf, int *nbytes_written)
{
    if (fd == -1)
           return TTY_ERRNO;

  unsigned int nbytes;
  int bytes_w = 0;
  *nbytes_written = 0;
   
  nbytes = strlen(buf);

  while (nbytes > 0)
  {
    
    bytes_w = write(fd, buf, nbytes);

    if (bytes_w < 0)
     return TTY_WRITE_ERROR;

   *nbytes_written += bytes_w;
    buf += bytes_w;
    nbytes -= bytes_w;
  }

  return TTY_OK;
}

int tty_read(int fd, char *buf, int nbytes, int timeout, int *nbytes_read)
{
    if (fd == -1)
           return TTY_ERRNO;

 int bytesRead = 0;
 int err = 0;
 *nbytes_read =0;

  if (nbytes <=0)
	return TTY_PARAM_ERROR;

  while (nbytes > 0)
  {
     if ( (err = tty_timeout(fd, timeout)) )
      return err;

     bytesRead = read(fd, buf, ((unsigned) nbytes));

     if (bytesRead < 0 )
      return TTY_READ_ERROR;

     buf += bytesRead;
     *nbytes_read += bytesRead;
     nbytes -= bytesRead;
  }

  return TTY_OK;
}

int tty_read_section(int fd, char *buf, char stop_char, int timeout, int *nbytes_read)
{
    if (fd == -1)
           return TTY_ERRNO;

 int bytesRead = 0;
 int err = TTY_OK;
 *nbytes_read = 0;

 for (;;)
 {
         if ( (err = tty_timeout(fd, timeout)) )
	   return err;

         bytesRead = read(fd, buf, 1);

         if (bytesRead < 0 )
            return TTY_READ_ERROR;

        if (bytesRead)
          (*nbytes_read)++;

        if (*buf == stop_char)
	   return TTY_OK;

        buf += bytesRead;
  }

  return TTY_TIME_OUT;
}

#if defined(BSD) && !defined(__GNU__)
// BSD - OSX version
int tty_connect(const char *device, int bit_rate, int word_size, int parity, int stop_bits, int *fd)
{
       int	t_fd = -1;
       int bps;
       char msg[80];
    int	handshake;
    struct termios	tty_setting;

    // Open the serial port read/write, with no controlling terminal, and don't wait for a connection.
    // The O_NONBLOCK flag also causes subsequent I/O on the device to be non-blocking.
    // See open(2) ("man 2 open") for details.

    t_fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (t_fd == -1)
    {
        printf("Error opening serial port %s - %s(%d).\n",
               device, strerror(errno), errno);
        goto error;
    }

    // Note that open() follows POSIX semantics: multiple open() calls to the same file will succeed
    // unless the TIOCEXCL ioctl is issued. This will prevent additional opens except by root-owned
    // processes.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.

    if (ioctl(t_fd, TIOCEXCL) == -1)
    {
        printf("Error setting TIOCEXCL on %s - %s(%d).\n",
            device, strerror(errno), errno);
        goto error;
    }

    // Now that the device is open, clear the O_NONBLOCK flag so subsequent I/O will block.
    // See fcntl(2) ("man 2 fcntl") for details.

    if (fcntl(t_fd, F_SETFL, 0) == -1)
    {
        printf("Error clearing O_NONBLOCK %s - %s(%d).\n",
            device, strerror(errno), errno);
        goto error;
    }

    // Get the current options and save them so we can restore the default settings later.
    if (tcgetattr(t_fd, &tty_setting) == -1)
    {
        printf("Error getting tty attributes %s - %s(%d).\n",
            device, strerror(errno), errno);
        goto error;
    }

    // Set raw input (non-canonical) mode, with reads blocking until either a single character
    // has been received or a one second timeout expires.
    // See tcsetattr(4) ("man 4 tcsetattr") and termios(4) ("man 4 termios") for details.

    cfmakeraw(&tty_setting);
    tty_setting.c_cc[VMIN] = 1;
    tty_setting.c_cc[VTIME] = 10;

    // The baud rate, word length, and handshake options can be set as follows:
        switch (bit_rate) {
                case 0:
                        bps = B0;
                        break;
                case 50:
                        bps = B50;
                        break;
                case 75:
                        bps = B75;
                        break;
                case 110:
                        bps = B110;
                        break;
                case 134:
                        bps = B134;
                        break;
                case 150:
                        bps = B150;
                        break;
                case 200:
                        bps = B200;
                        break;
                case 300:
                        bps = B300;
                        break;
                case 600:
                        bps = B600;
                        break;
                case 1200:
                        bps = B1200;
                        break;
                case 1800:
                        bps = B1800;
                        break;
                case 2400:
                        bps = B2400;
                        break;
                case 4800:
                        bps = B4800;
                        break;
                case 9600:
                        bps = B9600;
                        break;
                case 19200:
                        bps = B19200;
                        break;
                case 38400:
                        bps = B38400;
                        break;
                case 57600:
                        bps = B57600;
                        break;
                case 115200:
                        bps = B115200;
                        break;
                case 230400:
                        bps = B230400;
                        break;
                default:
                        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid bit rate.", bit_rate) < 0)
                                perror(NULL);
                        else
                                perror(msg);
                        return TTY_PARAM_ERROR;
        }

     cfsetspeed(&tty_setting, bps);		// Set baud rate
        /* word size */
        switch (word_size) {
                case 5:
                        tty_setting.c_cflag |= CS5;
                        break;
                case 6:
                        tty_setting.c_cflag |= CS6;
                        break;
                case 7:
                        tty_setting.c_cflag |= CS7;
                        break;
                case 8:
                        tty_setting.c_cflag |= CS8;
                        break;
                default:

                        fprintf( stderr, "Default\n") ;
                        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid data bit count.", word_size) < 0)
                                perror(NULL);
                        else
                                perror(msg);

                        return TTY_PARAM_ERROR;
        }

        /* parity */
        switch (parity) {
                case PARITY_NONE:
                        break;
                case PARITY_EVEN:
                        tty_setting.c_cflag |= PARENB;
                        break;
                case PARITY_ODD:
                        tty_setting.c_cflag |= PARENB | PARODD;
                        break;
                default:

                        fprintf( stderr, "Default1\n") ;
                        if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid parity selection value.", parity) < 0)
                                perror(NULL);
                        else
                                perror(msg);

                        return TTY_PARAM_ERROR;
        }

        /* stop_bits */
        switch (stop_bits) {
                case 1:
                        break;
                case 2:
                        tty_setting.c_cflag |= CSTOPB;
                        break;
                default:
                        fprintf( stderr, "Default2\n") ;
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
    if (ioctl(fileDescriptor, IOSSIOSPEED, &speed) == -1)
    {
        printf("Error calling ioctl(..., IOSSIOSPEED, ...) %s - %s(%d).\n",
            bsdPath, strerror(errno), errno);
    }
#endif

    // Cause the new options to take effect immediately.
    if (tcsetattr(t_fd, TCSANOW, &tty_setting) == -1)
    {
        printf("Error setting tty attributes %s - %s(%d).\n",
            device, strerror(errno), errno);
        goto error;
    }

    // To set the modem handshake lines, use the following ioctls.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.

    if (ioctl(t_fd, TIOCSDTR) == -1) // Assert Data Terminal Ready (DTR)
    {
        printf("Error asserting DTR %s - %s(%d).\n",
            device, strerror(errno), errno);
    }

    if (ioctl(t_fd, TIOCCDTR) == -1) // Clear Data Terminal Ready (DTR)
    {
        printf("Error clearing DTR %s - %s(%d).\n",
            device, strerror(errno), errno);
    }

    handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
    if (ioctl(t_fd, TIOCMSET, &handshake) == -1)
    // Set the modem lines depending on the bits set in handshake
    {
        printf("Error setting handshake lines %s - %s(%d).\n",
            device, strerror(errno), errno);
    }

    // To read the state of the modem lines, use the following ioctl.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.

    if (ioctl(t_fd, TIOCMGET, &handshake) == -1)
    // Store the state of the modem lines in handshake
    {
        printf("Error getting handshake lines %s - %s(%d).\n",
            device, strerror(errno), errno);
    }

    printf("Handshake lines currently set to %d\n", handshake);

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
        printf("Error setting read latency %s - %s(%d).\n",
            device, strerror(errno), errno);
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
 int t_fd=-1;
 char msg[80];
 int bps;
 struct termios tty_setting;

 if ( (t_fd = open(device, O_RDWR | O_NOCTTY )) == -1)
 {
     *fd = -1;
    return TTY_PORT_FAILURE;
 }

    /* Control Modes
   Set bps rate */
  switch (bit_rate) {
    case 0:
      bps = B0;
      break;
    case 50:
      bps = B50;
      break;
    case 75:
      bps = B75;
      break;
    case 110:
      bps = B110;
      break;
    case 134:
      bps = B134;
      break;
    case 150:
      bps = B150;
      break;
    case 200:
      bps = B200;
      break;
    case 300:
      bps = B300;
      break;
    case 600:
      bps = B600;
      break;
    case 1200:
      bps = B1200;
      break;
    case 1800:
      bps = B1800;
      break;
    case 2400:
      bps = B2400;
      break;
    case 4800:
      bps = B4800;
      break;
    case 9600:
      bps = B9600;
      break;
    case 19200:
      bps = B19200;
      break;
    case 38400:
      bps = B38400;
      break;
    case 57600:
      bps = B57600;
      break;
    case 115200:
      bps = B115200;
      break;
    case 230400:
      bps = B230400;
      break;
    default:
      if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid bit rate.", bit_rate) < 0)
        perror(NULL);
      else
        perror(msg);
      return TTY_PARAM_ERROR;
  }
  if ((cfsetispeed(&tty_setting, bps) < 0) ||
      (cfsetospeed(&tty_setting, bps) < 0))
  {
    perror("tty_connect: failed setting bit rate.");
    return TTY_PORT_FAILURE;
  }

   /* Control Modes
   set no flow control word size, parity and stop bits.
   Also don't hangup automatically and ignore modem status.
   Finally enable receiving characters. */
  tty_setting.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | HUPCL | CRTSCTS);
  tty_setting.c_cflag |= (CLOCAL | CREAD);

  /* word size */
  switch (word_size) {
    case 5:
      tty_setting.c_cflag |= CS5;
      break;
    case 6:
      tty_setting.c_cflag |= CS6;
      break;
    case 7:
      tty_setting.c_cflag |= CS7;
      break;
    case 8:
      tty_setting.c_cflag |= CS8;
      break;
    default:

      fprintf( stderr, "Default\n") ;
      if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid data bit count.", word_size) < 0)
        perror(NULL);
      else
        perror(msg);

      return TTY_PARAM_ERROR;
  }

  /* parity */
  switch (parity) {
    case PARITY_NONE:
      break;
    case PARITY_EVEN:
      tty_setting.c_cflag |= PARENB;
      break;
    case PARITY_ODD:
      tty_setting.c_cflag |= PARENB | PARODD;
      break;
    default:

   fprintf( stderr, "Default1\n") ;
      if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid parity selection value.", parity) < 0)
        perror(NULL);
      else
        perror(msg);

      return TTY_PARAM_ERROR;
  }

  /* stop_bits */
  switch (stop_bits) {
    case 1:
      break;
    case 2:
      tty_setting.c_cflag |= CSTOPB;
      break;
    default:
   fprintf( stderr, "Default2\n") ;
      if (snprintf(msg, sizeof(msg), "tty_connect: %d is not a valid stop bit count.", stop_bits) < 0)
        perror(NULL);
      else
        perror(msg);

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
  tty_setting.c_lflag |=  NOFLSH;

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
      char error_string[512];

  switch (err_code)
 {
	case TTY_OK:
		strncpy(err_msg, "No Error", err_msg_len);
		break;

	case TTY_READ_ERROR:
		snprintf(error_string, 512, "Read Error: %s", strerror(errno));
		strncpy(err_msg, error_string, err_msg_len);
		break;
		
       case TTY_WRITE_ERROR:
		snprintf(error_string, 512, "Write Error: %s", strerror(errno));
		strncpy(err_msg, error_string, err_msg_len);
		break;

	case TTY_SELECT_ERROR:
		snprintf(error_string, 512, "Select Error: %s", strerror(errno));
		strncpy(err_msg, error_string, err_msg_len);
		break;

	case TTY_TIME_OUT:
		strncpy(err_msg, "Timeout error", err_msg_len);
		break;

	case TTY_PORT_FAILURE:
		snprintf(error_string, 512, "Port failure Error: %s", strerror(errno));
		strncpy(err_msg, error_string, err_msg_len);
		break;

	case TTY_PARAM_ERROR:
		strncpy(err_msg, "Parameter error", err_msg_len);
		break;

	case TTY_ERRNO:
		snprintf(error_string, 512, "%s", strerror(errno));
		strncpy(err_msg, error_string, err_msg_len);
		break;

	default:
		strncpy(err_msg, "Error: unrecognized error code", err_msg_len);
		break;


   }	
}

/* return static string corresponding to the given property or light state */
const char *
pstateStr (IPState s)
{
        switch (s) {
        case IPS_IDLE:  return ("Idle");
        case IPS_OK:    return ("Ok");
        case IPS_BUSY:  return ("Busy");
        case IPS_ALERT: return ("Alert");
        default:
            fprintf (stderr, "Impossible IPState %d\n", s);
            return NULL;
        }
}

/* crack string into IPState.
 * return 0 if ok, else -1
 */
int
crackIPState (const char *str, IPState *ip)
{
             if (!strcmp (str, "Idle"))  *ip = IPS_IDLE;
        else if (!strcmp (str, "Ok"))    *ip = IPS_OK;
        else if (!strcmp (str, "Busy"))  *ip = IPS_BUSY;
        else if (!strcmp (str, "Alert")) *ip = IPS_ALERT;
        else return (-1);
        return (0);
}

/* crack string into ISState.
 * return 0 if ok, else -1
 */
int
crackISState (const char *str, ISState *ip)
{
             if (!strcmp (str, "On"))  *ip = ISS_ON;
        else if (!strcmp (str, "Off")) *ip = ISS_OFF;
        else return (-1);
        return (0);
}

int
crackIPerm (const char *str, IPerm *ip)
{
             if (!strcmp (str, "rw"))  *ip = IP_RW;
        else if (!strcmp (str, "ro")) *ip = IP_RO;
        else if (!strcmp (str, "wo")) *ip = IP_WO;
        else return (-1);
        return (0);
}

int crackISRule (const char *str, ISRule *ip)
{
    if (!strcmp (str, "OneOfMany"))  *ip = ISR_1OFMANY;
    else if (!strcmp (str, "AtMostOne")) *ip = ISR_ATMOST1;
    else if (!strcmp (str, "AnyOfMany")) *ip = ISR_NOFMANY;
    else return (-1);
  return (0);
}

/* return static string corresponding to the given switch state */
const char *
sstateStr (ISState s)
{
        switch (s) {
        case ISS_ON:  return ("On");
        case ISS_OFF: return ("Off");
        default:
            fprintf (stderr, "Impossible ISState %d\n", s);
            return NULL;
        }
}

/* return static string corresponding to the given Rule */
const char *
ruleStr (ISRule r)
{
        switch (r) {
        case ISR_1OFMANY: return ("OneOfMany");
        case ISR_ATMOST1: return ("AtMostOne");
        case ISR_NOFMANY: return ("AnyOfMany");
        default:
            fprintf (stderr, "Impossible ISRule %d\n", r);
            return NULL;
        }
}

/* return static string corresponding to the given IPerm */
const char *
permStr (IPerm p)
{
        switch (p) {
        case IP_RO: return ("ro");
        case IP_WO: return ("wo");
        case IP_RW: return ("rw");
        default:
            fprintf (stderr, "Impossible IPerm %d\n", p);
            return NULL;
        }
}

/* print the boilerplate comment introducing xml */
void
xmlv1()
{
        printf ("<?xml version='1.0'?>\n");
}

/* pull out device and name attributes from root.
 * return 0 if ok else -1 with reason in msg[].
 */
int
crackDN (XMLEle *root, char **dev, char **name, char msg[])
{
        XMLAtt *ap;

        ap = findXMLAtt (root, "device");
        if (!ap) {
            sprintf (msg, "%s requires 'device' attribute", tagXMLEle(root));
            return (-1);
        }
        *dev = valuXMLAtt(ap);

        ap = findXMLAtt (root, "name");
        if (!ap) {
            sprintf (msg, "%s requires 'name' attribute", tagXMLEle(root));
            return (-1);
        }
        *name = valuXMLAtt(ap);

        return (0);
}

/* find a member of an IText vector, else NULL */
IText *
IUFindText  (const ITextVectorProperty *tvp, const char *name)
{
        int i;

        for (i = 0; i < tvp->ntp; i++)
            if (strcmp (tvp->tp[i].name, name) == 0)
                return (&tvp->tp[i]);
        fprintf (stderr, "No IText '%s' in %s.%s\n",name,tvp->device,tvp->name);
        return (NULL);
}

/* find a member of an INumber vector, else NULL */
INumber *
IUFindNumber(const INumberVectorProperty *nvp, const char *name)
{
        int i;

        for (i = 0; i < nvp->nnp; i++)
            if (strcmp (nvp->np[i].name, name) == 0)
                return (&nvp->np[i]);
        fprintf(stderr,"No INumber '%s' in %s.%s\n",name,nvp->device,nvp->name);
        return (NULL);
}

/* find a member of an ISwitch vector, else NULL */
ISwitch *
IUFindSwitch(const ISwitchVectorProperty *svp, const char *name)
{
        int i;

        for (i = 0; i < svp->nsp; i++)
            if (strcmp (svp->sp[i].name, name) == 0)
                return (&svp->sp[i]);
        fprintf(stderr,"No ISwitch '%s' in %s.%s\n",name,svp->device,svp->name);
        return (NULL);
}

/* find a member of an ILight vector, else NULL */
ILight *
IUFindLight(const ILightVectorProperty *lvp, const char *name)
{
        int i;

        for (i = 0; i < lvp->nlp; i++)
            if (strcmp (lvp->lp[i].name, name) == 0)
                return (&lvp->lp[i]);
        fprintf(stderr,"No ILight '%s' in %s.%s\n",name,lvp->device,lvp->name);
        return (NULL);
}

/* find a member of an IBLOB vector, else NULL */
IBLOB *
IUFindBLOB(const IBLOBVectorProperty *bvp, const char *name)
{
        int i;

        for (i = 0; i < bvp->nbp; i++)
            if (strcmp (bvp->bp[i].name, name) == 0)
                return (&bvp->bp[i]);
        fprintf(stderr,"No IBLOB '%s' in %s.%s\n",name,bvp->device,bvp->name);
        return (NULL);
}

/* find an ON member of an ISwitch vector, else NULL.
 * N.B. user must make sense of result with ISRule in mind.
 */
ISwitch *
IUFindOnSwitch(const ISwitchVectorProperty *svp)
{
        int i;

        for (i = 0; i < svp->nsp; i++)
            if (svp->sp[i].s == ISS_ON)
                return (&svp->sp[i]);
        /*fprintf(stderr, "No ISwitch On in %s.%s\n", svp->device, svp->name);*/
        return (NULL);
}

/* Find index of the ON member of an ISwitchVectorProperty */
int IUFindOnSwitchIndex(const ISwitchVectorProperty *svp)
{
        int i;

        for (i = 0; i < svp->nsp; i++)
            if (svp->sp[i].s == ISS_ON)
                return i;
        return -1;
}

/* Set all switches to off */
void
IUResetSwitch(ISwitchVectorProperty *svp)
{
  int i;

  for (i = 0; i < svp->nsp; i++)
    svp->sp[i].s = ISS_OFF;
}

/* save malloced copy of newtext in tp->text, reusing if not first time */
void
IUSaveText (IText *tp, const char *newtext)
{
        /* seed for realloc */
        if (tp->text == NULL)
            tp->text = malloc (1);

        /* copy in fresh string */
        tp->text = strcpy (realloc (tp->text, strlen(newtext)+1), newtext);
}
