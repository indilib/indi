/*
    TTY Base

    Base class for TTY Serial Connections.

    Copyright (C) 2018 Jasem Mutlaq
    Copyright (C) 2011 Wildi Markus

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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "ttybase.h"

#include "locale_compat.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __APPLE__
#include <sys/param.h>
#endif

#if defined(BSD) && !defined(__GNU__)
#include <IOKit/serial/ioss.h>
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

#if defined(_MSC_VER)
#define snprintf _snprintf
#pragma warning(push)
///@todo Introduce plattform indipendent safe functions as macros to fix this
#pragma warning(disable : 4996)
#endif

TTYBase::TTYBase(const char *driverName) : m_DriverName(driverName)
{

}

TTYBase::~TTYBase()
{
    if (m_PortFD != -1)
        disconnect();
}

TTYBase::TTY_RESPONSE TTYBase::checkTimeout(uint8_t timeout)
{
#if defined(_WIN32) || defined(ANDROID)
    INDI_UNUSED(timeout);
    return TTY_ERRNO;
#else

    if (m_PortFD == -1)
        return TTY_ERRNO;

    struct timeval tv;
    fd_set readout;
    int retval;

    FD_ZERO(&readout);
    FD_SET(m_PortFD, &readout);

    /* wait for 'timeout' seconds */
    tv.tv_sec  = timeout;
    tv.tv_usec = 0;

    /* Wait till we have a change in the m_PortFD status */
    retval = select(m_PortFD + 1, &readout, nullptr, nullptr, &tv);

    /* Return 0 on successful m_PortFD change */
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

TTYBase::TTY_RESPONSE TTYBase::write(const uint8_t *buffer, uint32_t nbytes, uint32_t *nbytes_written)
{
#ifdef _WIN32
    return TTY_ERRNO;
#else

    if (m_PortFD == -1)
        return TTY_ERRNO;

    int bytes_w     = 0;
    *nbytes_written = 0;

    while (nbytes > 0)
    {
        bytes_w = ::write(m_PortFD, buffer + (*nbytes_written), nbytes);

        if (bytes_w < 0)
            return TTY_WRITE_ERROR;

        for (uint32_t i = *nbytes_written; i < bytes_w+*nbytes_written; i++)
            DEBUGFDEVICE(m_DriverName, m_DebugChannel, "%s: buffer[%d]=%#X (%c)", __FUNCTION__, i, buffer[i], buffer[i]);

        *nbytes_written += bytes_w;
        nbytes -= bytes_w;
    }

    return TTY_OK;

#endif
}

TTYBase::TTY_RESPONSE TTYBase::writeString(const char *string, uint32_t *nbytes_written)
{
    uint32_t nbytes = strlen(string);
    return write(reinterpret_cast<const uint8_t*>(string), nbytes, nbytes_written);
}

TTYBase::TTY_RESPONSE TTYBase::read(uint8_t *buffer, uint32_t nbytes, uint8_t timeout, uint32_t *nbytes_read)
{
#ifdef _WIN32
    return TTY_ERRNO;
#else

    if (m_PortFD == -1)
        return TTY_ERRNO;

    uint32_t numBytesToRead =  nbytes;
    int bytesRead = 0;
    TTY_RESPONSE timeoutResponse = TTY_OK;
    *nbytes_read  = 0;

    if (nbytes <= 0)
        return TTY_PARAM_ERROR;

    DEBUGFDEVICE(m_DriverName, m_DebugChannel, "%s: Request to read %d bytes with %d timeout for m_PortFD %d", __FUNCTION__, nbytes, timeout, m_PortFD);

    while (numBytesToRead > 0)
    {
        if ((timeoutResponse = checkTimeout(timeout)))
            return timeoutResponse;

        bytesRead = ::read(m_PortFD, buffer + (*nbytes_read), numBytesToRead);

        if (bytesRead < 0)
            return TTY_READ_ERROR;

        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "%d bytes read and %d bytes remaining...", bytesRead, numBytesToRead - bytesRead);
        for (uint32_t i = *nbytes_read; i < (*nbytes_read + bytesRead); i++)
            DEBUGFDEVICE(m_DriverName, m_DebugChannel, "%s: buffer[%d]=%#X (%c)", __FUNCTION__, i, buffer[i], buffer[i]);

        *nbytes_read += bytesRead;
        numBytesToRead -= bytesRead;
    }

    return TTY_OK;

#endif
}

TTYBase::TTY_RESPONSE TTYBase::readSection(uint8_t *buffer, uint32_t nsize, uint8_t stop_byte, uint8_t timeout, uint32_t *nbytes_read)
{
#ifdef _WIN32
    return TTY_ERRNO;
#else

    if (m_PortFD == -1)
        return TTY_ERRNO;

    int bytesRead = 0;
    TTY_RESPONSE timeoutResponse = TTY_OK;
    *nbytes_read  = 0;
    memset(buffer, 0, nsize);

    uint8_t *read_char = nullptr;

    DEBUGFDEVICE(m_DriverName, m_DebugChannel, "%s: Request to read until stop char '%#02X' with %d timeout for m_PortFD %d", __FUNCTION__, stop_byte, timeout, m_PortFD);

    for (;;)
    {
        if ((timeoutResponse = checkTimeout(timeout)))
            return timeoutResponse;

        read_char = reinterpret_cast<uint8_t*>(buffer + *nbytes_read);
        bytesRead = ::read(m_PortFD, read_char, 1);

        if (bytesRead < 0)
            return TTY_READ_ERROR;

        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "%s: buffer[%d]=%#X (%c)", __FUNCTION__, (*nbytes_read), *read_char, *read_char);

        (*nbytes_read)++;

        if (*read_char == stop_byte)
            return TTY_OK;
        else if (*nbytes_read >= nsize)
            return TTY_OVERFLOW;
    }

#endif
}

#if defined(BSD) && !defined(__GNU__)
// BSD - OSX version
TTYBase::TTY_RESPONSE TTYBase::connect(const char *device, uint32_t bit_rate, uint8_t word_size, uint8_t parity, uint8_t stop_bits)
{
    int t_fd = -1;
    int bps;
    int handshake;
    struct termios tty_setting;

    // Open the serial port read/write, with no controlling terminal, and don't wait for a connection.
    // The O_NONBLOCK flag also causes subsequent I/O on the device to be non-blocking.
    // See open(2) ("man 2 open") for details.

    t_fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (t_fd == -1)
    {
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "Error opening serial port (%s) - %s(%d).", device, strerror(errno), errno);
        goto error;
    }

    // Note that open() follows POSIX semantics: multiple open() calls to the same file will succeed
    // unless the TIOCEXCL ioctl is issued. This will prevent additional opens except by root-owned
    // processes.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.

    if (ioctl(t_fd, TIOCEXCL) == -1)
    {
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "Error setting TIOCEXCL on %s - %s(%d).", device, strerror(errno), errno);
        goto error;
    }

    // Now that the device is open, clear the O_NONBLOCK flag so subsequent I/O will block.
    // See fcntl(2) ("man 2 fcntl") for details.

    if (fcntl(t_fd, F_SETFL, 0) == -1)
    {
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "Error clearing O_NONBLOCK %s - %s(%d).", device, strerror(errno), errno);
        goto error;
    }

    // Get the current options and save them so we can restore the default settings later.
    if (tcgetattr(t_fd, &tty_setting) == -1)
    {
        DEBUGFDEVICE(m_DriverName, m_DebugChannel,"Error getting tty attributes %s - %s(%d).", device, strerror(errno), errno);
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
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "connect: %d is not a valid bit rate.", bit_rate);
        return TTY_PARAM_ERROR;
    }

    cfsetspeed(&tty_setting, bps); // Set baud rate
    /* word size */
    switch (word_size)
    {
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
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "connect: %d is not a valid data bit count.", word_size);
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
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "connect: %d is not a valid parity selection value.", parity);
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
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "connect: %d is not a valid stop bit count.", stop_bits);
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
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "Error setting tty attributes %s - %s(%d).", device, strerror(errno), errno);
        goto error;
    }

    // To set the modem handshake lines, use the following ioctls.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.

    if (ioctl(t_fd, TIOCSDTR) == -1) // Assert Data Terminal Ready (DTR)
    {
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "Error asserting DTR %s - %s(%d).", device, strerror(errno), errno);
    }

    if (ioctl(t_fd, TIOCCDTR) == -1) // Clear Data Terminal Ready (DTR)
    {
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "Error clearing DTR %s - %s(%d).", device, strerror(errno), errno);
    }

    handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
    if (ioctl(t_fd, TIOCMSET, &handshake) == -1)
        // Set the modem lines depending on the bits set in handshake
    {
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "Error setting handshake lines %s - %s(%d).", device, strerror(errno), errno);
    }

    // To read the state of the modem lines, use the following ioctl.
    // See tty(4) ("man 4 tty") and ioctl(2) ("man 2 ioctl") for details.

    if (ioctl(t_fd, TIOCMGET, &handshake) == -1)
    // Store the state of the modem lines in handshake
    {
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "Error getting handshake lines %s - %s(%d).", device, strerror(errno), errno);
    }

    DEBUGFDEVICE(m_DriverName, m_DebugChannel, "Handshake lines currently set to %d", handshake);

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
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "Error setting read latency %s - %s(%d).\n", device, strerror(errno), errno);
        goto error;
    }
#endif

    m_PortFD = t_fd;
    /* return success */
    return TTY_OK;

    // Failure path
error:
    if (t_fd != -1)
    {
        close(t_fd);
        m_PortFD = -1;
    }

    return TTY_PORT_FAILURE;
}
#else
TTYBase::TTY_RESPONSE TTYBase::connect(const char *device, uint32_t bit_rate, uint8_t word_size, uint8_t parity, uint8_t stop_bits)
{
#ifdef _WIN32
    return TTY_PORT_FAILURE;

#else
    int t_fd = -1;
    int bps;
    struct termios tty_setting;

    if ((t_fd = open(device, O_RDWR | O_NOCTTY)) == -1)
    {
        m_PortFD = -1;

        return TTY_PORT_FAILURE;
    }

    // Get the current options and save them so we can restore the default settings later.
    if (tcgetattr(t_fd, &tty_setting) == -1)
    {
        DEBUGDEVICE(m_DriverName, m_DebugChannel, "connect: failed getting tty attributes.");
        return TTY_PORT_FAILURE;
    }

    /* Control Modes
    Set bps rate */
    switch (bit_rate)
    {
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
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "connect: %d is not a valid bit rate.", bit_rate);
        return TTY_PARAM_ERROR;
    }
    if ((cfsetispeed(&tty_setting, bps) < 0) || (cfsetospeed(&tty_setting, bps) < 0))
    {
        perror("connect: failed setting bit rate.");
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
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "connect: %d is not a valid data bit count.", word_size);
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
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "connect: %d is not a valid parity selection value.", parity);
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
        DEBUGFDEVICE(m_DriverName, m_DebugChannel, "connect: %d is not a valid stop bit count.", stop_bits);
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
        DEBUGDEVICE(m_DriverName, m_DebugChannel, "connect: failed setting attributes on serial port.");
        close(t_fd);
        return TTY_PORT_FAILURE;
    }

    m_PortFD = t_fd;
    /* return success */
    return TTY_OK;
#endif
}
// Unix - Linux version

#endif

TTYBase::TTY_RESPONSE TTYBase::disconnect()
{
    if (m_PortFD == -1)
        return TTY_ERRNO;

#ifdef _WIN32
    return TTY_ERRNO;
#else
    tcflush(m_PortFD, TCIOFLUSH);
    int err = close(m_PortFD);

    if (err != 0)
        return TTY_ERRNO;

    return TTY_OK;
#endif
}

const std::string TTYBase::error(TTY_RESPONSE code) const
{
    char error_string[512]={0};

    switch (code)
    {
    case TTY_OK:
        return "No Error";

    case TTY_READ_ERROR:
        snprintf(error_string, 512, "Read Error: %s", strerror(errno));
        return error_string;

    case TTY_WRITE_ERROR:
        snprintf(error_string, 512, "Write Error: %s", strerror(errno));
        return error_string;

    case TTY_SELECT_ERROR:
        snprintf(error_string, 512, "Select Error: %s", strerror(errno));
        return error_string;

    case TTY_TIME_OUT:
        return "Timeout error";

    case TTY_PORT_FAILURE:
        if (errno == EACCES)
            snprintf(error_string, 512,
                     "Port failure Error: %s. Try adding your user to the dialout group and restart (sudo adduser "
                     "$USER dialout)",
                     strerror(errno));
        else
            snprintf(error_string, 512, "Port failure Error: %s. Check if device is connected to this port.",
                     strerror(errno));
        return error_string;

    case TTY_PARAM_ERROR:
        return "Parameter error";


    case TTY_ERRNO:
        snprintf(error_string, 512, "%s", strerror(errno));
        return error_string;

    case TTY_OVERFLOW:
        return "Read overflow";
    }

    return "Error: unrecognized error code";
}

#if defined(_MSC_VER)
#undef snprintf
#pragma warning(pop)
#endif
