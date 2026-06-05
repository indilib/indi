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

#include <cerrno>
#include <chrono>
#include <cstring>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

MXHDSerial::MXHDSerial(int fd) : m_fd(fd) {}

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
    const uint8_t *p = static_cast<const uint8_t *>(data);
    size_t written = 0;
    while (written < len)
    {
        const ssize_t n = ::write(m_fd, p + written, len - written);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            err = std::string("write failed: ") + std::strerror(errno);
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

bool MXHDSerial::writeString(const std::string &s, std::string &err)
{
    return writeRaw(s.data(), s.size(), err);
}

static bool waitReadable(int fd, int timeoutMs, std::string &err)
{
    while (true)
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        const int r = ::select(fd + 1, &rfds, nullptr, nullptr, &tv);
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            err = std::string("select failed: ") + std::strerror(errno);
            return false;
        }
        if (r == 0)
        {
            err = "read timeout";
            return false;
        }
        return true;
    }
}

static int remainingTimeoutMs(std::chrono::steady_clock::time_point deadline)
{
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline)
        return 0;
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
}

bool MXHDSerial::readExact(std::vector<uint8_t> &out, size_t n, int timeoutMs, std::string &err)
{
    out.clear();
    out.resize(n);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    size_t got = 0;
    while (got < n)
    {
        const int leftMs = remainingTimeoutMs(deadline);
        const int slice = leftMs > 200 ? 200 : leftMs;
        if (slice <= 0)
        {
            err = "read timeout";
            return false;
        }
        std::string werr;
        if (!waitReadable(m_fd, slice, werr))
        {
            err = werr;
            return false;
        }

        const ssize_t r = ::read(m_fd, out.data() + got, n - got);
        if (r < 0)
        {
            if (errno == EINTR) continue;
            err = std::string("read failed: ") + std::strerror(errno);
            return false;
        }
        if (r == 0)
        {
            err = "read returned EOF";
            return false;
        }
        got += static_cast<size_t>(r);
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
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (true)
    {
        const int leftMs = remainingTimeoutMs(deadline);
        const int slice = leftMs > 200 ? 200 : leftMs;
        if (slice <= 0)
        {
            err = "read timeout";
            return false;
        }
        std::string werr;
        if (!waitReadable(m_fd, slice, werr))
        {
            err = werr;
            return false;
        }

        uint8_t ch = 0;
        const ssize_t r = ::read(m_fd, &ch, 1);
        if (r < 0)
        {
            if (errno == EINTR) continue;
            err = std::string("read failed: ") + std::strerror(errno);
            return false;
        }
        if (r == 0)
        {
            err = "read returned EOF";
            return false;
        }

        if (ch == '#')
        {
            return true;
        }

        out.push_back(static_cast<char>(ch));
    }
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
