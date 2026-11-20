/*
    OnStepX INDI Driver — Communication layer
    Copyright (C) 2026 Ed Lee

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

#include "OnStepXComm.h"

#include <defaultdevice.h>
#include <indilogger.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>

namespace
{
// How long a lone '0'/'1' must stand before sendText() accepts it as the
// firmware's bare error reply rather than the start of "0#"/"1#"/"10.28q#".
// Only reached on error paths, so it costs nothing on success.
constexpr int BARE_REPLY_GAP_MS = 200;

// Line noise seen from OnStepX on ESP32 around NV (flash) writes: bursts of
// 0xFF (a lone start bit) ahead of a reply.  No text, bool or char reply can
// contain 0xFF, so those readers drop it.  Binary :Gu# is validated instead.
constexpr unsigned char LINE_NOISE = 0xFF;

// Resync after an incomplete reply: discard input until the line has been
// quiet for RESYNC_QUIET_MS, for at most RESYNC_MAX_MS (stalls of ~1.9 s seen).
constexpr int RESYNC_QUIET_MS = 250;
constexpr int RESYNC_MAX_MS   = 3000;

long elapsedMs(const struct timespec &start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) * 1000L + (now.tv_nsec - start.tv_nsec) / 1000000L;
}

// select() for readability with a millisecond timeout.  >0 ready, 0 timeout, <0 error.
int waitReadable(int fd, long ms)
{
    if (ms < 0)
        ms = 0;
    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(fd, &rfd);
    // Field types differ by platform (tv_usec is int on macOS, long on Linux),
    // so assign with casts rather than brace-initialise (a narrowing error).
    struct timeval tv;
    tv.tv_sec  = static_cast<time_t>(ms / 1000);
    tv.tv_usec = static_cast<suseconds_t>((ms % 1000) * 1000);
    return select(fd + 1, &rfd, nullptr, nullptr, &tv);
}
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void OnStepXComm::setFd(int fd)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fd = fd;
#ifdef SO_NOSIGPIPE
    if (fd >= 0)
    {
        const int on = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));   // fails harmlessly on a tty
    }
#endif
    m_failStreak = 0;
    clock_gettime(CLOCK_MONOTONIC, &m_lastReply);
}

double OnStepXComm::secondsSinceReply() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return elapsedMs(m_lastReply) / 1000.0;
}

bool OnStepXComm::linkLost() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fd >= 0 && m_failStreak >= LINK_LOST_FAILURES && elapsedMs(m_lastReply) >= m_linkLostS * 1000.0;
}

void OnStepXComm::noteReply()
{
    if (m_failStreak > 1)
        LOGF_INFO("Controller replying again after %.0f s", elapsedMs(m_lastReply) / 1000.0);
    m_failStreak = 0;
    clock_gettime(CLOCK_MONOTONIC, &m_lastReply);
}

bool OnStepXComm::noteFailure()
{
    if (++m_failStreak == 2)
        LOG_WARN("Controller not replying; retrying (further failures are logged at debug level)");
    return m_failStreak > 1;
}

void OnStepXComm::setDevice(INDI::DefaultDevice *dev)
{
    m_dev = dev;
}

// ---------------------------------------------------------------------------
// Public send functions
// ---------------------------------------------------------------------------

bool OnStepXComm::sendText(const char *cmd, char *reply, int timeout_ms, bool quiet)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    reply[0] = '\0';

    if (!beginCommand(cmd, "text", quiet))
        return false;

    switch (readReply(reply, REPLY_BUF_SIZE, timeout_ms))
    {
        case ReadResult::FRAMED:
            noteReply();
            LOGF_DEBUG("REPLY: %s", reply);
            return true;

        case ReadResult::BARE:
            noteReply();
            logFail(quiet, "'%s' rejected by controller (reply '%s')", cmd, reply);
            return false;

        case ReadResult::FAILED:
        default:
            quiet = noteFailure() || quiet;
            logFail(quiet, "no valid reply to '%s'", cmd);
            m_resync = true;
            return false;
    }
}

bool OnStepXComm::sendBool(const char *cmd, int timeout_ms, bool quiet)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!beginCommand(cmd, "bool", quiet))
        return false;

    char c = '\0';
    if (!readSingleChar(c, timeout_ms))
    {
        quiet = noteFailure() || quiet;
        logFail(quiet, "no reply to '%s'", cmd);
        m_resync = true;
        return false;
    }
    noteReply();

    LOGF_DEBUG("REPLY: %c", c);

    if (c == '1')
        return true;

    if (c != '0')
    {
        // Not a bare boolean: the command is probably mis-classified.  Drop the
        // rest so it cannot be read as the reply to the next command.
        logFail(quiet, "unexpected reply 0x%02X to '%s'", static_cast<unsigned char>(c), cmd);
        m_resync = true;
    }
    return false;
}

bool OnStepXComm::sendNoReply(const char *cmd)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return beginCommand(cmd, "no-reply", false);
}

bool OnStepXComm::sendUrgent(const char *cmd)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fd < 0)
    {
        logFail(false, "'%s' not sent: not connected", cmd);
        return false;
    }
    if (!writeCommand(cmd))
    {
        logFail(noteFailure(), "write failed for '%s'", cmd);
        return false;
    }
    LOGF_DEBUG("CMD (urgent): %s", cmd);
    return true;
}

bool OnStepXComm::sendChar(const char *cmd, char &reply, int timeout_ms, bool quiet)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!beginCommand(cmd, "char", quiet))
        return false;

    if (!readSingleChar(reply, timeout_ms))
    {
        quiet = noteFailure() || quiet;
        logFail(quiet, "no reply to '%s'", cmd);
        m_resync = true;
        return false;
    }
    noteReply();

    LOGF_DEBUG("REPLY: %c (0x%02X)", reply, static_cast<unsigned char>(reply));
    return true;
}

bool OnStepXComm::sendReadN(const char *cmd, uint8_t *buf, int nbytes, int timeout_ms, bool quiet)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!beginCommand(cmd, "binary", quiet))
        return false;

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int received = 0;
    while (received < nbytes)
    {
        const long remaining = timeout_ms - elapsedMs(start);
        if (remaining <= 0 || waitReadable(m_fd, remaining) <= 0)
        {
            quiet = noteFailure() || quiet;
            logFail(quiet, "timeout after %d/%d bytes for '%s'", received, nbytes, cmd);
            m_resync = true;
            return false;
        }

        const ssize_t n = read(m_fd, buf + received, nbytes - received);
        if (n <= 0)
        {
            quiet = noteFailure() || quiet;
            logFail(quiet, "read error for '%s'", cmd);
            m_resync = true;
            return false;
        }
        received += static_cast<int>(n);
    }

    noteReply();
    LOGF_DEBUG("REPLY: %d bytes", nbytes);
    return true;
}

void OnStepXComm::flushIO()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    doFlush();
}

// ---------------------------------------------------------------------------
// Private helpers — callers hold m_mutex
// ---------------------------------------------------------------------------

// Check the fd, drain stale input and write the command.
bool OnStepXComm::beginCommand(const char *cmd, const char *what, bool quiet)
{
    if (m_fd < 0)
    {
        logFail(quiet, "'%s' not sent: not connected", cmd);
        return false;
    }

    // After an incomplete reply, wait for any late bytes before sending, so
    // they cannot be taken as this command's reply.
    if (m_resync)
    {
        drainUntilQuiet(RESYNC_QUIET_MS, RESYNC_MAX_MS);
        m_resync = false;
    }

    doFlush();

    if (!writeCommand(cmd))
    {
        quiet = noteFailure() || quiet;
        logFail(quiet, "write failed for '%s'", cmd);
        return false;
    }

    LOGF_DEBUG("CMD (%s): %s", what, cmd);
    return true;
}

void OnStepXComm::doFlush()
{
    if (m_fd < 0)
        return;

    char buf[256];
    while (waitReadable(m_fd, 0) > 0)
    {
        if (read(m_fd, buf, sizeof(buf)) <= 0)
            break;
    }
}

void OnStepXComm::drainUntilQuiet(int quietMs, int maxMs)
{
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char buf[256];
    int discarded = 0;
    while (elapsedMs(start) < maxMs)
    {
        if (waitReadable(m_fd, quietMs) <= 0)
            break;                              // quiet: resynchronised
        const ssize_t n = read(m_fd, buf, sizeof(buf));
        if (n <= 0)
            break;
        discarded += static_cast<int>(n);
    }
    LOGF_DEBUG("resync: discarded %d late byte(s) after %ld ms", discarded, elapsedMs(start));
}

bool OnStepXComm::writeCommand(const char *cmd)
{
    const size_t total = strlen(cmd);
    size_t written = 0;

    while (written < total)
    {
        // On a socket (network command channel), send() without SIGPIPE: a
        // write to a dropped connection must fail, not kill the driver.
        // Serial ports are not sockets, so they fall back to write().
#ifdef MSG_NOSIGNAL
        ssize_t n = send(m_fd, cmd + written, total - written, MSG_NOSIGNAL);
        if (n < 0 && errno == ENOTSOCK)
            n = write(m_fd, cmd + written, total - written);
#else
        const ssize_t n = write(m_fd, cmd + written, total - written);   // SO_NOSIGPIPE set in setFd()
#endif
        if (n <= 0)
            return false;
        written += static_cast<size_t>(n);
    }
    return true;
}

// Read a reply into buf (which must hold maxLen bytes), without the '#'.
//   FRAMED : '#' received.
//   BARE   : a lone '0'/'1' followed by silence — the firmware's numeric reply.
//   FAILED : timeout, read error, over-long reply, or partial unterminated text.
OnStepXComm::ReadResult OnStepXComm::readReply(char *buf, int maxLen, int timeout_ms)
{
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int len = 0;
    buf[0] = '\0';

    while (true)
    {
        const long remaining = timeout_ms - elapsedMs(start);
        if (remaining <= 0)
            break;

        const bool bareCandidate = (len == 1 && (buf[0] == '0' || buf[0] == '1'));
        const long wait = bareCandidate && remaining > BARE_REPLY_GAP_MS ? BARE_REPLY_GAP_MS : remaining;

        const int sel = waitReadable(m_fd, wait);
        if (sel == 0 && bareCandidate)
            return ReadResult::BARE;
        if (sel <= 0)
            break;

        char c;
        if (read(m_fd, &c, 1) != 1)
            break;

        if (static_cast<unsigned char>(c) == LINE_NOISE)
            continue;

        if (c == '#')
        {
            buf[len] = '\0';
            return ReadResult::FRAMED;
        }

        // Longer than any valid firmware reply: corrupt.  The remainder is
        // drained by doFlush() before the next command.
        if (len >= maxLen - 1)
            break;

        buf[len++] = c;
        buf[len] = '\0';
    }

    return ReadResult::FAILED;
}

bool OnStepXComm::readSingleChar(char &c, int timeout_ms)
{
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (true)
    {
        const long remaining = timeout_ms - elapsedMs(start);
        if (remaining <= 0 || waitReadable(m_fd, remaining) <= 0)
            return false;
        if (read(m_fd, &c, 1) != 1)
            return false;
        if (static_cast<unsigned char>(c) != LINE_NOISE)
            return true;
    }
}

void OnStepXComm::logFail(bool quiet, const char *fmt, ...) const
{
    char msg[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (quiet)
        LOGF_DEBUG("%s", msg);
    else
        LOGF_ERROR("%s", msg);
}
