/*
    OnStepX INDI Driver — Communication layer (shared by both binaries)
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

    All LX200-framed I/O in one place.  Zero raw read()/write() calls outside
    this class.  A mutex serialises every send+receive; stale input is drained
    before each send.

    Reply types — choose the call that matches the firmware handler
    (OnStepX src/libApp/commands/ProcessCmds.cpp, CommandProcessor::poll()):

      Handler leaves numericReply == true (the firmware default)
        -> bare '1' (success) or '0' (failure), NO '#'.       sendBool()
           Unknown commands also reply a bare '0'.

      Handler sets numericReply = false and fills reply
        -> "text#".                                           sendText()
           Many of these fall back to a bare '0' on error, which sendText()
           reports as a failure.

      Handler sets numericReply = false and leaves reply empty
        -> NOTHING is sent, even when the command fails.      sendNoReply()

      Handler sets numericReply = false and suppressFrame = true
        -> a single char, no '#' (e.g. :MS#, :FA#).           sendChar()

      Binary replies (:Gu#)                                   sendReadN()

    Focusers: address a slot directly with ":F<n><cmd>#" (e.g. ":F2G#").  The
    firmware handles this atomically and, unlike :FA<n>#, it does not change
    the active focuser that other clients (SHC, SWS) rely on.
*/

#pragma once

#include <defaultdevice.h>

#include <cstdint>
#include <ctime>
#include <mutex>

namespace INDI
{
class DefaultDevice;
}

class OnStepXComm
{
    public:
        void setFd(int fd);
        void setDevice(INDI::DefaultDevice *dev);

        static constexpr int CMD_MAX_LEN    = 32;
        // Firmware replies are built in char reply[80] (ProcessCmds.cpp), so at
        // most 79 chars + '#'.  Keep headroom above that.
        static constexpr int REPLY_BUF_SIZE = 96;

        // "text#" reply.  reply (>= REPLY_BUF_SIZE bytes) receives the text
        // without the '#'.  Returns false on timeout, on an over-long or
        // unterminated reply, or when the firmware answers with a bare '0'/'1'
        // (its error path); in that last case reply holds the bare char.
        // quiet=true logs failures at debug level (use while probing).
        bool sendText(const char *cmd, char *reply, int timeout_ms = 2000, bool quiet = false);

        // Bare '1'/'0' reply.  Returns true only for '1'.
        bool sendBool(const char *cmd, int timeout_ms = 2000, bool quiet = false);

        // Firmware sends nothing back.  Returns false only if the write fails.
        bool sendNoReply(const char *cmd);

        // As sendNoReply(), but without first waiting out a pending resync
        // (up to RESYNC_MAX_MS after a controller stall).  For stop commands
        // such as :Q#: with no reply there is nothing a late byte could be
        // mistaken for, and the resync stays pending for the next command.
        bool sendUrgent(const char *cmd);

        // Single unframed character reply.
        bool sendChar(const char *cmd, char &reply, int timeout_ms = 2000, bool quiet = false);

        // Read exactly nbytes (binary replies).
        bool sendReadN(const char *cmd, uint8_t *buf, int nbytes, int timeout_ms = 2000, bool quiet = false);

        // Drain all pending input bytes from the fd.
        void flushIO();

        bool isOpen() const
        {
            return m_fd >= 0;
        }

        // Link health.  A transaction fails when nothing usable comes back
        // (timeout, incomplete reply) or the write fails; any reply, even a
        // bare '0', proves the link.  After the first failure in a run the
        // rest are logged at debug level (one warning says so), so a dead
        // link does not flood the log; recovery is logged once.
        int failStreak() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_failStreak;
        }
        // Seconds since the last reply, or since setFd() opened the link.
        double secondsSinceReply() const;
        // True when the polling device should give up: no reply for
        // LINK_LOST_S and at least LINK_LOST_FAILURES failures in a row.
        // Long enough to ride out an ESP32 reboot (a few seconds) and NV
        // stalls (~2 s each).
        static constexpr double LINK_LOST_S        = 30.0;
        static constexpr int    LINK_LOST_FAILURES = 3;
        bool linkLost() const;
        // Tests shorten LINK_LOST_S.
        void setLinkLostSeconds(double s)
        {
            m_linkLostS = s;
        }

        // A caller found a reply invalid (e.g. a binary reply that failed
        // validation): discard any late bytes before the next command.
        void requestResync()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_resync = true;
        }

    private:
        int                  m_fd  { -1 };
        INDI::DefaultDevice *m_dev { nullptr };
        mutable std::mutex   m_mutex;

        int                  m_failStreak { 0 };
        double               m_linkLostS { LINK_LOST_S };
        struct timespec      m_lastReply {};
        // Callers hold m_mutex.  noteFailure() returns true when the failure
        // should be logged quietly (not the first in a run).
        void noteReply();
        bool noteFailure();

        // Set when a reply was not read completely (timeout, partial or
        // unexpected data).  The controller can stall for ~2 s (NV writes on
        // ESP32) and then send the late reply; without a resync it would be
        // read as the reply to the NEXT command, shifting every reply by one.
        bool                 m_resync { false };

        enum class ReadResult { FRAMED, BARE, FAILED };

        // All no-lock; callers hold m_mutex.
        void       doFlush();
        void       drainUntilQuiet(int quietMs, int maxMs);
        bool       beginCommand(const char *cmd, const char *what, bool quiet);
        bool       writeCommand(const char *cmd);
        ReadResult readReply(char *buf, int maxLen, int timeout_ms);
        bool       readSingleChar(char &c, int timeout_ms);
        void       logFail(bool quiet, const char *fmt, ...) const;

        const char *getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
