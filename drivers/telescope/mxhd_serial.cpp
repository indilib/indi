/*******************************************************************************
  Copyright(c) 2026 Makoto Kasahara All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file LICENSE.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#include "mxhd_serial.h"

#include <indicom.h>

#include <cerrno>
#include <cstring>
#include <termios.h>
#include <unistd.h>

MXHDSerial::MXHDSerial(int fd) : m_fd(fd) {}

static void setTTYError(int rc, std::string &err)
{
    char msg[1024] = {0};
    tty_error_msg(rc, msg, sizeof(msg));
    err = msg;
}

static void splitTimeoutMs(int timeoutMs, long &seconds, long &microseconds)
{
    if (timeoutMs < 0)
        timeoutMs = 0;

    seconds      = timeoutMs / 1000;
    microseconds = static_cast<long>(timeoutMs % 1000) * 1000L;
}

bool MXHDSerial::discardInput(std::string &err)
{
    if (::tcflush(m_fd, TCIFLUSH) != 0)
    {
        err = std::string("tcflush(TCIFLUSH) failed: ") + std::strerror(errno);
        return false;
    }
    return true;
}

bool MXHDSerial::writeRaw(const void *data, size_t len, std::string &err)
{
    int nbytesWritten = 0;
    const auto *buffer = static_cast<const char *>(data);
    const int rc = tty_write(m_fd, buffer, static_cast<int>(len), &nbytesWritten);
    if (rc != TTY_OK)
    {
        setTTYError(rc, err);
        return false;
    }

    if (static_cast<size_t>(nbytesWritten) != len)
    {
        err = "short write";
        return false;
    }

    return true;
}

bool MXHDSerial::writeString(const std::string &s, std::string &err)
{
    int nbytesWritten = 0;
    const int rc = tty_write_string(m_fd, s.c_str(), &nbytesWritten);
    if (rc != TTY_OK)
    {
        setTTYError(rc, err);
        return false;
    }

    if (nbytesWritten != static_cast<int>(s.size()))
    {
        err = "short write";
        return false;
    }

    return true;
}

bool MXHDSerial::readExact(std::vector<uint8_t> &out, size_t n, int timeoutMs, std::string &err)
{
    out.clear();
    out.resize(n);

    long seconds = 0;
    long microseconds = 0;
    splitTimeoutMs(timeoutMs, seconds, microseconds);

    int nbytesRead = 0;
    const int rc = tty_read_expanded(m_fd, reinterpret_cast<char *>(out.data()), static_cast<int>(n), seconds, microseconds,
                                     &nbytesRead);
    if (rc != TTY_OK)
    {
        setTTYError(rc, err);
        return false;
    }

    if (static_cast<size_t>(nbytesRead) != n)
    {
        err = "short read";
        return false;
    }

    return true;
}

bool MXHDSerial::readChar(char &ch, int timeoutMs, std::string &err)
{
    uint8_t byte = 0;
    std::vector<uint8_t> buf;
    if (!readExact(buf, 1, timeoutMs, err))
        return false;
    byte = buf[0];
    ch = static_cast<char>(byte);
    return true;
}

bool MXHDSerial::readHashTerminated(std::string &out, int timeoutMs, std::string &err)
{
    out.clear();

    long seconds = 0;
    long microseconds = 0;
    splitTimeoutMs(timeoutMs, seconds, microseconds);

    char buffer[1024] = {0};
    int nbytesRead = 0;
    const int rc = tty_nread_section_expanded(m_fd, buffer, sizeof(buffer), '#', seconds, microseconds, &nbytesRead);
    if (rc != TTY_OK)
    {
        setTTYError(rc, err);
        return false;
    }

    out.assign(buffer, nbytesRead);
    if (!out.empty() && out.back() == '#')
        out.pop_back();

    return true;
}

bool MXHDSerial::queryHash(const std::string &cmd, std::string &response, int timeoutMs, std::string &err)
{
    // Drain stale bytes first. This is important for mixed command patterns.
    if (!discardInput(err))
        return false;
    if (!writeString(cmd, err))
        return false;
    return readHashTerminated(response, timeoutMs, err);
}

bool MXHDSerial::queryAck(const std::string &cmd, char &ack, int timeoutMs, std::string &err)
{
    if (!discardInput(err))
        return false;
    if (!writeString(cmd, err))
        return false;
    if (!readChar(ack, timeoutMs, err))
        return false;
    return true;
}

bool MXHDSerial::queryStatus3(const std::string &cmd, uint8_t &b1, uint8_t &b2, uint8_t &b3, int timeoutMs, std::string &err)
{
    // Drain stale bytes before binary status read.
    if (!discardInput(err))
        return false;
    if (!writeString(cmd, err))
        return false;
    std::vector<uint8_t> buf;
    if (!readExact(buf, 3, timeoutMs, err))
        return false;
    b1 = buf[0];
    b2 = buf[1];
    b3 = buf[2];
    return true;
}
