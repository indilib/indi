/*
    Gemini Power Box Hub Advanced v3 INDI Driver

    Copyright (C) 2026 Dieter R Kedrowitsch <dieter@kedrowitsch.net>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA  02111-1307, USA.

    The full GNU General Public License is included in this distribution in the
    file called LICENSE.
*/

#include "indi-gemini-pbh/protocol.h"
#include <cerrno>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <exception>
#include <sstream>
#include <iomanip>
#include <locale>
#include <chrono>
#include <thread>
#include <utility>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>

namespace geminipbh
{

struct GeminiProtocol::TelemetryCallbackEntry
{
    TelemetryCallbackEntry(TelemetryCallbackHandle handle, TelemetryCallback callback)
        : handle(handle), callback(std::move(callback))
    {
    }

    const TelemetryCallbackHandle handle;
    TelemetryCallback callback;
    bool active = true;
    unsigned activeCalls = 0;
    std::condition_variable cv;
};

struct GeminiProtocol::ConnectionCallbackEntry
{
    ConnectionCallbackEntry(ConnectionCallbackHandle handle, ConnectionCallback callback)
        : handle(handle), callback(std::move(callback))
    {
    }

    const ConnectionCallbackHandle handle;
    ConnectionCallback callback;
    bool active = true;
    unsigned activeCalls = 0;
    std::condition_variable cv;
};

namespace
{
thread_local GeminiProtocol::TelemetryCallbackHandle currentTelemetryCallbackHandle = 0;
thread_local GeminiProtocol::ConnectionCallbackHandle currentConnectionCallbackHandle = 0;
}

GeminiProtocol::GeminiProtocol() : writeFn_(::write) {}
GeminiProtocol::~GeminiProtocol()
{
    disconnect();
}

static bool isAsciiWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

bool GeminiProtocol::open(const std::string &port, int settleMs)
{
    return open(port, settleMs, false);
}

bool GeminiProtocol::open(const std::string &port, int settleMs, bool exclusive)
{
    close();
    lastOpenErrno_ = 0;
    lastOpenBusy_ = false;
    fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0)
    {
        lastOpenErrno_ = errno;
        lastOpenBusy_ = errno == EBUSY;
        return false;
    }

#if !defined(__CYGWIN__)
    if (exclusive && ioctl(fd_, TIOCEXCL) != 0)
    {
        lastOpenErrno_ = errno;
        lastOpenBusy_ = errno == EBUSY;
        ::close(fd_);
        fd_ = -1;
        return false;
    }
#else
    (void)exclusive;
#endif

    struct termios tio;
    std::memset(&tio, 0, sizeof(tio));
    if (tcgetattr(fd_, &tio) != 0)
    {
        lastOpenErrno_ = errno;
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    cfsetospeed(&tio, B19200);
    cfsetispeed(&tio, B19200);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CRTSCTS;
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    tio.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    tio.c_oflag &= ~OPOST;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    tcflush(fd_, TCIOFLUSH);
    if (tcsetattr(fd_, TCSANOW, &tio) != 0)
    {
        lastOpenErrno_ = errno;
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    int flags = TIOCM_DTR | TIOCM_RTS;
    if (ioctl(fd_, TIOCMSET, &flags) != 0)
    {
#ifdef GEMINI_TESTING
        if (errno == ENOTTY || errno == EINVAL)
        {
            // Linux PTYs used in integration tests do not support modem-control
            // ioctls. Production builds still fail this path as required.
        }
        else
#endif
        {
            lastOpenErrno_ = errno;
            if (instrumentationEnabled(instrumentation::Level::Error))
            {
                const std::string message = "failed to assert RTS/DTR on " + port + ": " + std::strerror(errno);
                emitWarningInstrumentation(instrumentation::Level::Error,
                                           instrumentation::WarningCode::TransportError,
                                           message);
            }
            ::close(fd_);
            fd_ = -1;
            return false;
        }
    }

    if (settleMs > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(settleMs));
    }
    return true;
}

void GeminiProtocol::close()
{
    if (fd_ >= 0)
    {
        ::tcflush(fd_, TCIOFLUSH);
        int flags = ::fcntl(fd_, F_GETFL, 0);
        if (flags >= 0) ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        ::close(fd_);
        fd_ = -1;
    }
}
bool GeminiProtocol::isOpen() const
{
    return fd_ >= 0;
}

int GeminiProtocol::lastOpenErrno() const
{
    return lastOpenErrno_;
}

bool GeminiProtocol::lastOpenWasBusy() const
{
    return lastOpenBusy_;
}

void GeminiProtocol::setInstrumentationLevel(instrumentation::Level level)
{
    instrumentation_.setLevel(level);
}

instrumentation::Level GeminiProtocol::instrumentationLevel() const
{
    return instrumentation_.level();
}

bool GeminiProtocol::instrumentationEnabled(instrumentation::Level level) const
{
    return instrumentation_.enabled(level);
}

GeminiProtocol::InstrumentationListenerHandle GeminiProtocol::registerInstrumentationListener(
    InstrumentationListener listener)
{
    return instrumentation_.registerListener(std::move(listener));
}

bool GeminiProtocol::removeInstrumentationListener(InstrumentationListenerHandle handle)
{
    return instrumentation_.removeListener(handle);
}

bool GeminiProtocol::openWakePipe()
{
    if (wakeReadFd_ >= 0 && wakeWriteFd_ >= 0) return true;

    int fds[2] = {-1, -1};
    if (::pipe(fds) != 0)
    {
        if (instrumentationEnabled(instrumentation::Level::Error))
        {
            const std::string message = std::string("failed to create reader wake pipe: ") + std::strerror(errno);
            emitWarningInstrumentation(instrumentation::Level::Error,
                                       instrumentation::WarningCode::TransportError,
                                       message);
        }
        return false;
    }

    for (int fd : fds)
    {
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0)
        {
            if (instrumentationEnabled(instrumentation::Level::Error))
            {
                const std::string message = std::string("failed to configure reader wake pipe: ") + std::strerror(errno);
                emitWarningInstrumentation(instrumentation::Level::Error,
                                           instrumentation::WarningCode::TransportError,
                                           message);
            }
            ::close(fds[0]);
            ::close(fds[1]);
            return false;
        }
    }

    wakeReadFd_ = fds[0];
    wakeWriteFd_ = fds[1];
    return true;
}

void GeminiProtocol::closeWakePipe()
{
    if (wakeReadFd_ >= 0)
    {
        ::close(wakeReadFd_);
        wakeReadFd_ = -1;
    }
    if (wakeWriteFd_ >= 0)
    {
        ::close(wakeWriteFd_);
        wakeWriteFd_ = -1;
    }
}

void GeminiProtocol::signalReaderWake()
{
    if (wakeWriteFd_ < 0) return;

    const char byte = 'x';
    while (::write(wakeWriteFd_, &byte, 1) < 0)
    {
        if (errno == EINTR) continue;
        break;
    }
}

void GeminiProtocol::drainReaderWake(int wakeFd)
{
    if (wakeFd < 0) return;

    char buf[64];
    while (::read(wakeFd, buf, sizeof(buf)) > 0) {}
}

void GeminiProtocol::emitInstrumentationEvent(instrumentation::Event event) const
{
    event.source = "geminipbh.GeminiProtocol";
    event.connectionGeneration = connectionGeneration_.load(std::memory_order_acquire);
    instrumentation_.emit(std::move(event));
}

void GeminiProtocol::emitConnectionInstrumentation(instrumentation::ConnectionState state,
        instrumentation::Level level,
        const std::string &reason) const
{
    if (!instrumentationEnabled(level)) return;
    emitInstrumentationEvent(instrumentation::makeEvent(
    level,
    instrumentation::EventType::Connection,
    instrumentation::ConnectionPayload{state, reason}));
}

void GeminiProtocol::emitFrameInstrumentation(instrumentation::FrameDirection direction,
        const std::string &frame) const
{
    if (!instrumentationEnabled(instrumentation::Level::Trace)) return;
    std::string token;
    if (frame.size() >= 2 && (frame[0] == '>' || frame[0] == '*')) token.assign(1, frame[1]);
            emitInstrumentationEvent(instrumentation::makeEvent(
                                         instrumentation::Level::Trace,
                                         direction == instrumentation::FrameDirection::Tx ? instrumentation::EventType::FrameTransmitted :
                                         instrumentation::EventType::FrameReceived,
                                         instrumentation::FramePayload{direction, frame, token, true}));
        }

void GeminiProtocol::emitWarningInstrumentation(instrumentation::Level level,
        instrumentation::WarningCode code,
        const std::string &message) const
{
    if (!instrumentationEnabled(level)) return;
    emitInstrumentationEvent(instrumentation::makeEvent(
    level,
    instrumentation::EventType::Warning,
    instrumentation::WarningPayload{code, message}));
}

void GeminiProtocol::emitParserAnomalyInstrumentation(instrumentation::Level level,
        instrumentation::ParserAnomalyCode code,
        const std::string &detail,
        const std::string &frame) const
{
    if (!instrumentationEnabled(level)) return;
    emitInstrumentationEvent(instrumentation::makeEvent(
    level,
    instrumentation::EventType::ParserAnomaly,
    instrumentation::ParserAnomalyPayload{code, detail, frame}));
}

void GeminiProtocol::emitReconnectInstrumentation(unsigned attempt,
        unsigned maxAttempts,
        std::chrono::milliseconds delayBeforeAttempt,
        bool finalAttempt,
        instrumentation::ReconnectResult result) const
{
    instrumentation::Level level = instrumentation::Level::Info;
    if (result == instrumentation::ReconnectResult::AttemptFailed ||
            result == instrumentation::ReconnectResult::Cancelled)
    {
        level = instrumentation::Level::Verbose;
    }
    else if (result == instrumentation::ReconnectResult::Exhausted)
    {
        level = instrumentation::Level::Error;
    }
    if (!instrumentationEnabled(level)) return;
    emitInstrumentationEvent(instrumentation::makeEvent(
                                 level,
                                 instrumentation::EventType::ReconnectAttempt,
                                 instrumentation::ReconnectAttemptPayload{attempt, maxAttempts, delayBeforeAttempt, finalAttempt, result}));
}

void GeminiProtocol::emitTimingInstrumentation(instrumentation::TimingOperation operation,
        std::chrono::steady_clock::time_point start,
        bool success) const
{
    if (!instrumentationEnabled(instrumentation::Level::Verbose)) return;
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::steady_clock::now() - start);
    emitInstrumentationEvent(instrumentation::makeEvent(
                                 instrumentation::Level::Verbose,
                                 instrumentation::EventType::Timing,
                                 instrumentation::TimingPayload{operation, duration, success}));
}

void GeminiProtocol::emitVerificationInstrumentation(instrumentation::VerificationOutcome outcome,
        const std::string &detail,
        unsigned long long relatedSequence) const
{
    const instrumentation::Level level = outcome == instrumentation::VerificationOutcome::Passed ?
                                         instrumentation::Level::Info : instrumentation::Level::Error;
    if (!instrumentationEnabled(level)) return;
    instrumentation::Event event = instrumentation::makeEvent(
                                       level,
                                       instrumentation::EventType::Verification,
                                       instrumentation::VerificationPayload{instrumentation::VerificationOperation::CommandStateConfirm,
                                           outcome,
                                           detail,
                                           relatedSequence});
    event.sequence = relatedSequence;
    emitInstrumentationEvent(std::move(event));
}

void GeminiProtocol::drainInputLocked()
{
    if (fd_ < 0) return;
    char buf[256];
    struct timeval tv = {0, 0};
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd_, &set);
    while (select(fd_ + 1, &set, nullptr, nullptr, &tv) > 0)
    {
        if (::read(fd_, buf, sizeof(buf)) <= 0) break;
        FD_ZERO(&set);
        FD_SET(fd_, &set);
    }
}

bool GeminiProtocol::writeAllLocked(const std::string &command)
{
    if (fd_ < 0) return false;
    std::string frame = command + "\r\n";
    emitFrameInstrumentation(instrumentation::FrameDirection::Tx, frame);
    size_t written = 0;
    while (written < frame.size())
    {
        ssize_t n = writeFn_(fd_, frame.data() + written, frame.size() - written);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            if (instrumentationEnabled(instrumentation::Level::Error))
            {
                const std::string message = std::string("serial write failed: ") + std::strerror(errno);
                emitWarningInstrumentation(instrumentation::Level::Error,
                                           instrumentation::WarningCode::SerialWriteFailed,
                                           message);
            }
            return false;
        }
        if (n == 0)
        {
            emitWarningInstrumentation(instrumentation::Level::Error,
                                       instrumentation::WarningCode::SerialWriteFailed,
                                       "serial write returned 0 bytes");
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

bool GeminiProtocol::readFrameOnce(std::string &response, int timeoutMs)
{
    response.clear();
    if (fd_ < 0) return false;
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeoutMs);
    char buf;
    bool seenNonWhitespace = false;
    while (std::chrono::steady_clock::now() < deadline && !cancelled())
    {
        struct timeval tv = {0, 50000};
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd_, &set);
        int rc = select(fd_ + 1, &set, nullptr, nullptr, &tv);
        if (rc < 0)
        {
            if (errno == EINTR) continue;
            return false;
        }
        if (rc == 0) continue;
        ssize_t n = ::read(fd_, &buf, 1);
        if (n <= 0) continue;
        if (buf == '#')
        {
            emitFrameInstrumentation(instrumentation::FrameDirection::Rx, response);
            return !response.empty();
        }
        if (!seenNonWhitespace)
        {
            if (isAsciiWhitespace(buf)) continue;
            seenNonWhitespace = true;
        }
        response += buf;
    }
    return false;
}

bool GeminiProtocol::readTokenFrameOnce(char token, std::string &response, int timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline && !cancelled())
    {
        int remaining = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                            deadline - std::chrono::steady_clock::now()).count();
        std::string frame;
        if (!readFrameOnce(frame, remaining > 0 ? remaining : 1)) return false;
        if (frame.size() >= 2 && frame[0] == '*' && frame[1] == token)
        {
            response = frame;
            return true;
        }
        if (frame.size() >= 2 && frame[0] == '*' && frame[1] == 'G')
        {
            StatusFrame parsed;
            std::string err;
            if (parseStatusPayload(frame.substr(2), parsed, err))
            {
                storeStatusFrame(parsed, false);
            }
            else
            {
                const bool anomalyEnabled = instrumentationEnabled(instrumentation::Level::Verbose);
                if (anomalyEnabled)
                {
                    const std::string detail = std::string("ignored malformed G while waiting for ") + token + ": " + err;
                    emitParserAnomalyInstrumentation(instrumentation::Level::Verbose,
                                                     instrumentation::ParserAnomalyCode::MalformedStatusFrame,
                                                     detail,
                                                     frame + "#");
                }
            }
            continue;
        }
        const bool anomalyEnabled = instrumentationEnabled(instrumentation::Level::Verbose);
        if (anomalyEnabled)
        {
            const std::string detail = std::string("ignored frame while waiting for ") + token;
            emitParserAnomalyInstrumentation(instrumentation::Level::Verbose,
                                             instrumentation::ParserAnomalyCode::IgnoredFrameWhileWaiting,
                                             detail,
                                             frame + "#");
        }
    }
    return false;
}

bool GeminiProtocol::parseFirmwareFrame(const std::string &frame, std::string &raw, int &numeric) const
{
    if (frame.size() < 3 || frame[0] != '*' || frame[1] != 'V') return false;
    raw = frame.substr(2);
    if (raw.empty()) return false;
    try
    {
        size_t pos = 0;
        numeric = std::stoi(raw, &pos);
        if (pos != raw.size()) return false;
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool GeminiProtocol::cancelled() const
{
    return cancelFn_ && cancelFn_();
}

bool GeminiProtocol::sleepCancelable(int milliseconds) const
{
    int elapsed = 0;
    while (elapsed < milliseconds)
    {
        if (cancelled()) return false;
        int slice = std::min(50, milliseconds - elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(slice));
        elapsed += slice;
    }
    return !cancelled();
}

bool GeminiProtocol::transactOnce(const std::string &command, std::string &response, int timeoutMs)
{
    std::lock_guard<std::mutex> wlock(writeMutex_);
    drainInputLocked();
    if (!writeAllLocked(command)) return false;
    return readFrameOnce(response, timeoutMs);
}

GeminiProtocol::DirectQueryStatus GeminiProtocol::queryTokenFrame(const std::string &command,
        char token,
        int timeoutMs,
        std::string &response,
        std::string *detail)
{
    response.clear();
    if (detail) detail->clear();

    std::lock_guard<std::mutex> wlock(writeMutex_);
    drainInputLocked();
    if (!writeAllLocked(command))
    {
        if (detail) *detail = "write failed";
        return DirectQueryStatus::WriteFailed;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::string firstUnexpected;
    while (std::chrono::steady_clock::now() < deadline && !cancelled())
    {
        const int remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count());
        std::string frame;
        if (!readFrameOnce(frame, remaining > 0 ? remaining : 1))
            break;

        if (frame.size() >= 2 && frame[0] == '*' && frame[1] == token)
        {
            response = frame;
            return DirectQueryStatus::Success;
        }

        if (frame.size() >= 2 && frame[0] == '*' && frame[1] == 'G')
        {
            StatusFrame parsed;
            std::string err;
            if (parseStatusPayload(frame.substr(2), parsed, err))
                storeStatusFrame(parsed, false);
            continue;
        }

        if (firstUnexpected.empty())
            firstUnexpected = frame;
    }

    if (!firstUnexpected.empty())
    {
        if (detail) *detail = firstUnexpected;
        return DirectQueryStatus::UnexpectedFrame;
    }

    if (detail) *detail = "timeout";
    return DirectQueryStatus::Timeout;
}

GeminiProtocol::DirectQueryStatus GeminiProtocol::queryIdentity(std::string &identity, int timeoutMs,
        std::string *detail)
{
    identity.clear();
    std::string frame;
    DirectQueryStatus status = queryTokenFrame(">H#", 'H', timeoutMs, frame, detail);
    if (status != DirectQueryStatus::Success)
        return status;

    if (frame.size() < 3)
    {
        if (detail) *detail = frame;
        return DirectQueryStatus::MalformedResponse;
    }

    identity = frame.substr(2);
    if (identity.empty())
    {
        if (detail) *detail = frame;
        return DirectQueryStatus::MalformedResponse;
    }
    return DirectQueryStatus::Success;
}

GeminiProtocol::DirectQueryStatus GeminiProtocol::queryFirmware(std::string &raw, int &numeric, int timeoutMs,
        std::string *detail)
{
    raw.clear();
    numeric = 0;
    std::string frame;
    DirectQueryStatus status = queryTokenFrame(">V#", 'V', timeoutMs, frame, detail);
    if (status != DirectQueryStatus::Success)
        return status;

    if (!parseFirmwareFrame(frame, raw, numeric))
    {
        if (frame.size() >= 2 && frame[0] == '*' && frame[1] == 'V')
            raw = frame.substr(2);
        if (detail) *detail = frame;
        return DirectQueryStatus::MalformedResponse;
    }

    return DirectQueryStatus::Success;
}

bool GeminiProtocol::handshake()
{
    std::string identity;
    return queryIdentity(identity, 2000) == DirectQueryStatus::Success &&
           identity == "GeminiPowerBoxPlusAdv3";
}

bool GeminiProtocol::readFirmware(std::string &raw, int &numeric)
{
    return queryFirmware(raw, numeric, 2000) == DirectQueryStatus::Success;
}

bool GeminiProtocol::firmwareVersion(std::string &raw, int &numeric) const
{
    std::lock_guard<std::mutex> lock(firmwareMutex_);
    if (!haveFirmwareVersion_) return false;
    raw = firmwareVersionRaw_;
    numeric = firmwareVersionNumeric_;
    return true;
}

static bool parseBit(const std::string &s, size_t idx)
{
    return idx < s.size() && s[idx] == '1';
}

static bool validateBits(const std::string &s, const std::string &name, std::string &error)
{
    for (char c : s)
    {
        if (c != '0' && c != '1')
        {
            error = name + " field contains non-binary char";
            return false;
        }
    }
    return true;
}

static bool parseDbl(const std::string &s, double &v, const std::string &name, std::string &error)
{
    if (s.empty())
    {
        error = name + " field empty";
        return false;
    }
    std::istringstream iss(s);
    iss.imbue(std::locale::classic());
    iss >> v;
    if (iss.fail() || !iss.eof())
    {
        error = name + " field not numeric: '" + s + "'";
        return false;
    }
    if (!std::isfinite(v))
    {
        error = name + " field is not finite";
        return false;
    }
    return true;
}

static bool parseBool01(const std::string &s, bool &v, const std::string &name, std::string &error)
{
    if (s == "0")
    {
        v = false;
        return true;
    }
    if (s == "1")
    {
        v = true;
        return true;
    }
    error = name + " field must be 0 or 1";
    return false;
}

static bool parseMode(const std::string &s, int &mode, const std::string &name, std::string &error)
{
    try
    {
        size_t p = 0;
        mode = std::stoi(s, &p);
        if (p != s.size())
        {
            error = name + " field has trailing garbage";
            return false;
        }
    }
    catch (...)
    {
        error = name + " field not integer";
        return false;
    }
    if (mode < 0 || mode > 2)
    {
        error = name + " field must be 0, 1, or 2";
        return false;
    }
    return true;
}

static bool parseRange(const std::string &s, double &v, double lo, double hi,
                       const std::string &name, std::string &error)
{
    if (!parseDbl(s, v, name, error)) return false;
    if (v < lo || v > hi)
    {
        error = name + " field out of range [" +
                std::to_string(lo) + ", " + std::to_string(hi) + "]";
        return false;
    }
    return true;
}

bool GeminiProtocol::parseStatusPayload(const std::string &payload, StatusFrame &status, std::string &error) const
{
    static const char terminators[17] =
    {
        'D', 'U', 'A', 'T', 'A', 'M', 'B', 'M', 'C', 'C', 'S', 'T', 'H', 'D', 'V', 'C', 'P'
    };
    std::string fields[17];
    size_t pos = 0;
    for (int i = 0; i < 17; ++i)
    {
        size_t mpos = payload.find(terminators[i], pos);
        if (mpos == std::string::npos)
        {
            error = std::string("missing terminator ") + terminators[i];
            return false;
        }
        fields[i] = payload.substr(pos, mpos - pos);
        pos = mpos + 1;
    }
    if (pos != payload.size())
    {
        error = "trailing data after final terminator P";
        return false;
    }

    if (fields[0].size() != 4)
    {
        error = "DC field length != 4";
        return false;
    }
    if (fields[1].size() != 6)
    {
        error = "USB field length != 6";
        return false;
    }
    if (!validateBits(fields[0], "DC", error)) return false;
    if (!validateBits(fields[1], "USB", error)) return false;
    if (!parseBool01(fields[2], status.aht20Attached, "AHT20", error)) return false;
    if (!parseBool01(fields[3], status.ds18b20Attached, "DS18B20", error)) return false;
    if (!parseBool01(fields[4], status.dew6Enabled, "dew6Enabled", error)) return false;
    if (!parseMode(fields[5], status.dew6Mode, "dew6Mode", error)) return false;
    if (!parseBool01(fields[6], status.dew7Enabled, "dew7Enabled", error)) return false;
    if (!parseMode(fields[7], status.dew7Mode, "dew7Mode", error)) return false;
    if (!parseRange(fields[8], status.dew6OutputPercent, 0.0, 100.0, "dew6OutputPercent", error)) return false;
    if (!parseRange(fields[9], status.dew7OutputPercent, 0.0, 100.0, "dew7OutputPercent", error)) return false;
    if (!parseDbl(fields[10], status.deviceTemperatureC, "deviceTemperatureC", error)) return false;
    if (!parseDbl(fields[11], status.airTemperatureC, "airTemperatureC", error)) return false;
    if (!parseRange(fields[12], status.humidityPercent, 0.0, 100.0, "humidityPercent", error)) return false;
    if (!parseDbl(fields[13], status.dewPointC, "dewPointC", error)) return false;
    if (!parseDbl(fields[14], status.inputVoltageV, "inputVoltageV", error)) return false;
    if (!parseDbl(fields[15], status.outputCurrentA, "outputCurrentA", error)) return false;
    if (!parseDbl(fields[16], status.outputPowerW, "outputPowerW", error)) return false;

    status.dc = { parseBit(fields[0], 0), parseBit(fields[0], 1), parseBit(fields[0], 2), parseBit(fields[0], 3) };
    status.usb = { parseBit(fields[1], 0), parseBit(fields[1], 1), parseBit(fields[1], 2), parseBit(fields[1], 3), parseBit(fields[1], 4), parseBit(fields[1], 5) };
    return true;
}

bool GeminiProtocol::latestStatus(StatusFrame &out, unsigned long long &frameSeq)
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    if (frameSeq_ == 0) return false;
    out = cachedStatus_;
    frameSeq = frameSeq_;
    return true;
}

GeminiProtocol::TelemetryCallbackHandle GeminiProtocol::registerTelemetryCallback(TelemetryCallback callback)
{
    if (!callback) return 0;

    std::lock_guard<std::mutex> lock(telemetryCallbackMutex_);
    TelemetryCallbackHandle handle = nextTelemetryCallbackHandle_++;
    if (nextTelemetryCallbackHandle_ == 0) ++nextTelemetryCallbackHandle_;
    telemetryCallbacks_.push_back(std::make_shared<TelemetryCallbackEntry>(handle, std::move(callback)));
    return handle;
}

bool GeminiProtocol::removeTelemetryCallback(TelemetryCallbackHandle handle)
{
    if (handle == 0) return false;

    std::shared_ptr<TelemetryCallbackEntry> entry;
    std::unique_lock<std::mutex> lock(telemetryCallbackMutex_);
    auto it = std::find_if(telemetryCallbacks_.begin(), telemetryCallbacks_.end(),
                           [handle](const std::shared_ptr<TelemetryCallbackEntry> &candidate)
    {
        return candidate->handle == handle;
    });
    if (it == telemetryCallbacks_.end()) return false;

    entry = *it;
    entry->active = false;
    telemetryCallbacks_.erase(it);

    if (currentTelemetryCallbackHandle == handle) return true;

    entry->cv.wait(lock, [&] { return entry->activeCalls == 0; });
    return true;
}

GeminiProtocol::ConnectionCallbackHandle GeminiProtocol::registerConnectionCallback(ConnectionCallback callback)
{
    if (!callback) return 0;

    std::lock_guard<std::mutex> lock(connectionCallbackMutex_);
    ConnectionCallbackHandle handle = nextConnectionCallbackHandle_++;
    if (nextConnectionCallbackHandle_ == 0) ++nextConnectionCallbackHandle_;
    connectionCallbacks_.push_back(std::make_shared<ConnectionCallbackEntry>(handle, std::move(callback)));
    return handle;
}

bool GeminiProtocol::removeConnectionCallback(ConnectionCallbackHandle handle)
{
    if (handle == 0) return false;

    std::shared_ptr<ConnectionCallbackEntry> entry;
    std::unique_lock<std::mutex> lock(connectionCallbackMutex_);
    auto it = std::find_if(connectionCallbacks_.begin(), connectionCallbacks_.end(),
                           [handle](const std::shared_ptr<ConnectionCallbackEntry> &candidate)
    {
        return candidate->handle == handle;
    });
    if (it == connectionCallbacks_.end()) return false;

    entry = *it;
    entry->active = false;
    connectionCallbacks_.erase(it);

    if (currentConnectionCallbackHandle == handle) return true;

    entry->cv.wait(lock, [&] { return entry->activeCalls == 0; });
    return true;
}

static HeaterMode heaterModeFromStatus(int mode)
{
    switch (mode)
    {
        case 0:
            return HeaterMode::Auto;
        case 1:
            return HeaterMode::ManualPwm;
        case 2:
            return HeaterMode::BinarySwitch;
    }
    return HeaterMode::Auto;
}

static void addStatusField(std::vector<instrumentation::NamedValue> &fields,
                           const std::string &name,
                           instrumentation::Value value)
{
    fields.push_back(instrumentation::NamedValue{name, std::move(value)});
}

static std::vector<instrumentation::NamedValue> statusFields(const StatusFrame &status)
{
    std::vector<instrumentation::NamedValue> fields;
    fields.reserve(25);
    for (std::size_t i = 0; i < status.dc.size(); ++i)
        addStatusField(fields, "dc[" + std::to_string(i) + "]", status.dc[i]);
    for (std::size_t i = 0; i < status.usb.size(); ++i)
        addStatusField(fields, "usb[" + std::to_string(i) + "]", status.usb[i]);
    addStatusField(fields, "aht20Attached", status.aht20Attached);
    addStatusField(fields, "ds18b20Attached", status.ds18b20Attached);
    addStatusField(fields, "dew6Enabled", status.dew6Enabled);
    addStatusField(fields, "dew6Mode", static_cast<std::int64_t>(status.dew6Mode));
    addStatusField(fields, "dew7Enabled", status.dew7Enabled);
    addStatusField(fields, "dew7Mode", static_cast<std::int64_t>(status.dew7Mode));
    addStatusField(fields, "dew6OutputPercent", status.dew6OutputPercent);
    addStatusField(fields, "dew7OutputPercent", status.dew7OutputPercent);
    addStatusField(fields, "deviceTemperatureC", status.deviceTemperatureC);
    addStatusField(fields, "airTemperatureC", status.airTemperatureC);
    addStatusField(fields, "humidityPercent", status.humidityPercent);
    addStatusField(fields, "dewPointC", status.dewPointC);
    addStatusField(fields, "inputVoltageV", status.inputVoltageV);
    addStatusField(fields, "outputCurrentA", status.outputCurrentA);
    addStatusField(fields, "outputPowerW", status.outputPowerW);
    return fields;
}

static void addStatusChange(std::vector<instrumentation::FieldChange> &changes,
                            const std::string &name,
                            instrumentation::Value previous,
                            instrumentation::Value current)
{
    if (previous != current)
    {
        changes.push_back(instrumentation::FieldChange{name, std::move(previous), std::move(current)});
    }
}

static std::vector<instrumentation::FieldChange> statusChanges(const StatusFrame &previous,
        const StatusFrame &current)
{
    std::vector<instrumentation::FieldChange> changes;
    changes.reserve(25);
    for (std::size_t i = 0; i < current.dc.size(); ++i)
        addStatusChange(changes, "dc[" + std::to_string(i) + "]", previous.dc[i], current.dc[i]);
    for (std::size_t i = 0; i < current.usb.size(); ++i)
        addStatusChange(changes, "usb[" + std::to_string(i) + "]", previous.usb[i], current.usb[i]);
    addStatusChange(changes, "aht20Attached", previous.aht20Attached, current.aht20Attached);
    addStatusChange(changes, "ds18b20Attached", previous.ds18b20Attached, current.ds18b20Attached);
    addStatusChange(changes, "dew6Enabled", previous.dew6Enabled, current.dew6Enabled);
    addStatusChange(changes, "dew6Mode", static_cast<std::int64_t>(previous.dew6Mode),
                    static_cast<std::int64_t>(current.dew6Mode));
    addStatusChange(changes, "dew7Enabled", previous.dew7Enabled, current.dew7Enabled);
    addStatusChange(changes, "dew7Mode", static_cast<std::int64_t>(previous.dew7Mode),
                    static_cast<std::int64_t>(current.dew7Mode));
    addStatusChange(changes, "dew6OutputPercent", previous.dew6OutputPercent, current.dew6OutputPercent);
    addStatusChange(changes, "dew7OutputPercent", previous.dew7OutputPercent, current.dew7OutputPercent);
    addStatusChange(changes, "deviceTemperatureC", previous.deviceTemperatureC, current.deviceTemperatureC);
    addStatusChange(changes, "airTemperatureC", previous.airTemperatureC, current.airTemperatureC);
    addStatusChange(changes, "humidityPercent", previous.humidityPercent, current.humidityPercent);
    addStatusChange(changes, "dewPointC", previous.dewPointC, current.dewPointC);
    addStatusChange(changes, "inputVoltageV", previous.inputVoltageV, current.inputVoltageV);
    addStatusChange(changes, "outputCurrentA", previous.outputCurrentA, current.outputCurrentA);
    addStatusChange(changes, "outputPowerW", previous.outputPowerW, current.outputPowerW);
    return changes;
}

TelemetrySnapshot GeminiProtocol::makeTelemetrySnapshot(const StatusFrame &status,
        unsigned long long sequence,
        std::chrono::steady_clock::time_point timestamp) const
{
    return TelemetrySnapshot(
           status.dc,
           status.usb,
           status.aht20Attached,
           status.ds18b20Attached,
    {status.dew6Enabled, status.dew7Enabled},
    {heaterModeFromStatus(status.dew6Mode), heaterModeFromStatus(status.dew7Mode)},
    {status.dew6OutputPercent, status.dew7OutputPercent},
    status.deviceTemperatureC,
    status.airTemperatureC,
    status.humidityPercent,
    status.dewPointC,
    status.inputVoltageV,
    status.outputCurrentA,
    status.outputPowerW,
    sequence,
    timestamp);
}

TelemetrySnapshot GeminiProtocol::storeStatusFrame(const StatusFrame &status, bool dispatchCallbacks)
{
    const auto timestamp = std::chrono::steady_clock::now();
    const bool protocolInstrumentationEnabled = instrumentationEnabled(instrumentation::Level::Protocol);
    const bool verboseInstrumentationEnabled = instrumentationEnabled(instrumentation::Level::Verbose);
    StatusFrame previousStatus;
    bool havePreviousStatus = false;
    unsigned long long sequence = 0;
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        if (verboseInstrumentationEnabled && frameSeq_ != 0)
        {
            previousStatus = cachedStatus_;
            havePreviousStatus = true;
        }
        cachedStatus_ = status;
        sequence = ++frameSeq_;
        lastFrameTime_ = timestamp;
        recoveryAttempted_.store(false, std::memory_order_release);
    }
    frameCv_.notify_all();

    TelemetrySnapshot snapshot = makeTelemetrySnapshot(status, sequence, timestamp);
    if (protocolInstrumentationEnabled)
    {
        instrumentation::StatusDecodedPayload payload;
        payload.frameType = "G";
        payload.sequence = sequence;
        if (verboseInstrumentationEnabled) payload.fields = statusFields(status);
        instrumentation::Event event = instrumentation::makeEvent(
                                           instrumentation::Level::Protocol,
                                           instrumentation::EventType::StatusDecoded,
                                           std::move(payload));
        event.sequence = sequence;
        emitInstrumentationEvent(std::move(event));
    }
    if (verboseInstrumentationEnabled && havePreviousStatus)
    {
        auto changes = statusChanges(previousStatus, status);
        if (!changes.empty())
        {
            instrumentation::FieldChangesPayload payload;
            payload.sequence = sequence;
            payload.changes = std::move(changes);
            instrumentation::Event event = instrumentation::makeEvent(
                                               instrumentation::Level::Verbose,
                                               instrumentation::EventType::FieldChanges,
                                               std::move(payload));
            event.sequence = sequence;
            emitInstrumentationEvent(std::move(event));
        }
    }
    if (dispatchCallbacks)
    {
        dispatchTelemetrySnapshot(snapshot);
    }
    return snapshot;
}

void GeminiProtocol::beginCallbackDispatch()
{
    std::unique_lock<std::mutex> lock(callbackDispatchMutex_);
    callbackDispatchCv_.wait(lock, [&] { return !callbackDispatchInProgress_; });
    callbackDispatchInProgress_ = true;
}

void GeminiProtocol::endCallbackDispatch()
{
    {
        std::lock_guard<std::mutex> lock(callbackDispatchMutex_);
        callbackDispatchInProgress_ = false;
    }
    callbackDispatchCv_.notify_all();
}

void GeminiProtocol::dispatchTelemetrySnapshot(const TelemetrySnapshot &snapshot)
{
    beginCallbackDispatch();
    try
    {

        std::vector<std::shared_ptr<TelemetryCallbackEntry>> entries;
        {
            std::lock_guard<std::mutex> lock(telemetryCallbackMutex_);
            entries = telemetryCallbacks_;
        }

        for (const auto &entry : entries)
        {
            TelemetryCallback callback;
            {
                std::lock_guard<std::mutex> lock(telemetryCallbackMutex_);
                if (!entry->active) continue;
                ++entry->activeCalls;
                callback = entry->callback;
            }

            currentTelemetryCallbackHandle = entry->handle;
            std::string diagnostic;
            try
            {
                callback(snapshot);
            }
            catch (const std::exception &ex)
            {
                diagnostic = std::string("telemetry callback threw exception: ") + ex.what();
            }
            catch (...)
            {
                diagnostic = "telemetry callback threw unknown exception";
            }
            currentTelemetryCallbackHandle = 0;

            {
                std::lock_guard<std::mutex> lock(telemetryCallbackMutex_);
                --entry->activeCalls;
                if (entry->activeCalls == 0) entry->cv.notify_all();
            }

            if (!diagnostic.empty())
            {
                try
                {
                    emitWarningInstrumentation(instrumentation::Level::Error,
                                               instrumentation::WarningCode::CallbackException,
                                               diagnostic);
                }
                catch (...) {}
            }
        }
    }
    catch (...)
    {
        endCallbackDispatch();
        throw;
    }

    endCallbackDispatch();
}

void GeminiProtocol::dispatchConnectionEvent(ConnectionEvent event, const std::string &reason)
{
    beginCallbackDispatch();
    try
    {

        std::vector<std::shared_ptr<ConnectionCallbackEntry>> entries;
        {
            std::lock_guard<std::mutex> lock(connectionCallbackMutex_);
            entries = connectionCallbacks_;
        }

        for (const auto &entry : entries)
        {
            ConnectionCallback callback;
            {
                std::lock_guard<std::mutex> lock(connectionCallbackMutex_);
                if (!entry->active) continue;
                ++entry->activeCalls;
                callback = entry->callback;
            }

            currentConnectionCallbackHandle = entry->handle;
            std::string diagnostic;
            try
            {
                callback(event);
            }
            catch (const std::exception &ex)
            {
                diagnostic = std::string("connection callback threw exception: ") + ex.what();
            }
            catch (...)
            {
                diagnostic = "connection callback threw unknown exception";
            }
            currentConnectionCallbackHandle = 0;

            {
                std::lock_guard<std::mutex> lock(connectionCallbackMutex_);
                --entry->activeCalls;
                if (entry->activeCalls == 0) entry->cv.notify_all();
            }

            if (!diagnostic.empty())
            {
                try
                {
                    emitWarningInstrumentation(instrumentation::Level::Error,
                                               instrumentation::WarningCode::CallbackException,
                                               diagnostic);
                }
                catch (...) {}
            }
        }
    }
    catch (...)
    {
        endCallbackDispatch();
        throw;
    }

    endCallbackDispatch();

    instrumentation::ConnectionState state = instrumentation::ConnectionState::Disconnected;
    instrumentation::Level level = instrumentation::Level::Info;
    switch (event)
    {
        case ConnectionEvent::Connected:
            state = instrumentation::ConnectionState::Connected;
            break;
        case ConnectionEvent::Disconnected:
            state = instrumentation::ConnectionState::Disconnected;
            break;
        case ConnectionEvent::CommunicationFailure:
            state = instrumentation::ConnectionState::CommunicationFailure;
            level = instrumentation::Level::Error;
            break;
        case ConnectionEvent::ReconnectSuccess:
            state = instrumentation::ConnectionState::ReconnectSuccess;
            break;
    }
    emitConnectionInstrumentation(state, level, reason);
}

void GeminiProtocol::notifyCommunicationFailure(const std::string &reason)
{
    if (!connected_.load(std::memory_order_acquire)) return;
    bool alreadyNotified = communicationFailureNotified_.exchange(true, std::memory_order_acq_rel);
    if (alreadyNotified) return;

    acceptingWrites_.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> qlock(queueMutex_);
        sessionStopRequested_.store(true, std::memory_order_release);
    }
    signalReaderWake();
    connected_.store(false, std::memory_order_release);
    queueCv_.notify_all();
    frameCv_.notify_all();
    dispatchConnectionEvent(ConnectionEvent::CommunicationFailure, reason);
}

bool GeminiProtocol::waitForFrameAfter(unsigned long long minSeq, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(cacheMutex_);
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeoutMs);
    return frameCv_.wait_until(lock, deadline, [&] { return frameSeq_ > minSeq; });
}

bool GeminiProtocol::waitForFrameAfter(unsigned long long minSeq, int timeoutMs,
                                       StatusFrame &out, unsigned long long &frameSeq)
{
    std::unique_lock<std::mutex> lock(cacheMutex_);
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeoutMs);
    if (!frameCv_.wait_until(lock, deadline, [&] { return frameSeq_ > minSeq; }))
    {
        return false;
    }
    out = cachedStatus_;
    frameSeq = frameSeq_;
    return true;
}

bool GeminiProtocol::waitForVerifiedState(unsigned long long preWriteSeq,
        StatusVerifier verifier,
        int timeoutMs,
        std::string &error,
        unsigned long long &verifiedSeq)
{
    auto start = std::chrono::steady_clock::now();
    auto deadline = start + std::chrono::milliseconds(timeoutMs);
    auto remainingMs = [&]
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return 0;
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
    };

    StatusFrame ignored;
    unsigned long long firstSeq = 0;
    if (!waitForFrameAfter(preWriteSeq, remainingMs(), ignored, firstSeq))
    {
        error = "verification timed out waiting for first post-write telemetry frame";
        emitVerificationInstrumentation(instrumentation::VerificationOutcome::Failed, error, 0);
        return false;
    }

    StatusFrame candidate;
    if (!waitForFrameAfter(firstSeq, remainingMs(), candidate, verifiedSeq))
    {
        error = "verification timed out waiting for second post-write telemetry frame";
        emitVerificationInstrumentation(instrumentation::VerificationOutcome::Failed, error, firstSeq);
        return false;
    }

    if (!verifier || !verifier(candidate))
    {
        error = "second post-write telemetry frame did not confirm requested state";
        emitVerificationInstrumentation(instrumentation::VerificationOutcome::Failed, error, verifiedSeq);
        return false;
    }

    emitVerificationInstrumentation(instrumentation::VerificationOutcome::Passed,
                                    "verified by telemetry frame " + std::to_string(verifiedSeq),
                                    verifiedSeq);
    return true;
}

static bool outputCommand(unsigned firmwareIndex, bool enabled, std::string &cmd)
{
    if (firmwareIndex < 2 || firmwareIndex > 11) return false;
    cmd = enabled ? ">O" : ">C";
    cmd += std::to_string(firmwareIndex) + "#";
    return true;
}

static bool dewModeCommand(unsigned channel, DewMode mode, std::string &cmd)
{
    if (channel != 6 && channel != 7) return false;
    unsigned prefix = channel == 6 ? 10 : 20;
    unsigned suffix = 0;
    switch (mode)
    {
        case DewMode::Automatic:
            suffix = 0;
            break;
        case DewMode::Manual:
            suffix = 1;
            break;
        case DewMode::Switch:
            suffix = 2;
            break;
    }
    cmd = ">M" + std::to_string(prefix + suffix) + "#";
    return true;
}

static bool dewPercentCommand(unsigned channel, unsigned percent, std::string &cmd)
{
    if ((channel != 6 && channel != 7) || percent > 100) return false;
    char cmdChar = channel == 6 ? 'X' : 'Y';
    cmd = std::string(">") + cmdChar + std::to_string(percent) + "#";
    return true;
}

static bool dewEnableCommand(unsigned channel, bool enabled, std::string &cmd)
{
    if (channel == 6)
    {
        cmd = enabled ? ">Z11#" : ">Z10#";
        return true;
    }
    if (channel == 7)
    {
        cmd = enabled ? ">Z21#" : ">Z20#";
        return true;
    }
    return false;
}

int GeminiProtocol::syncCommand(const std::string &command)
{
    if (!acceptingWrites_.load(std::memory_order_acquire)) return 3;
    const bool timingEnabled = instrumentationEnabled(instrumentation::Level::Verbose);
    const auto timingStart = timingEnabled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    bool ok = false;
    {
        std::lock_guard<std::mutex> wlock(writeMutex_);
        ok = writeAllLocked(command);
    }
    if (timingEnabled) emitTimingInstrumentation(instrumentation::TimingOperation::SyncWrite, timingStart, ok);
    if (!ok) notifyCommunicationFailure("serial write failed");
    return ok ? 0 : 4;
}

#ifdef GEMINI_TESTING
int GeminiProtocol::syncWriteForTesting(const std::string &command)
{
    acceptingWrites_.store(true, std::memory_order_release);
    return syncCommand(command);
}

size_t GeminiProtocol::queuedCommandCountForTesting()
{
    std::lock_guard<std::mutex> qlock(queueMutex_);
    return queue_.size();
}

void GeminiProtocol::injectStatusForTesting(const StatusFrame &status)
{
    storeStatusFrame(status, true);
}

void GeminiProtocol::emitConnectionEventForTesting(ConnectionEvent event)
{
    dispatchConnectionEvent(event);
}

bool GeminiProtocol::parseFirmwarePayloadForTesting(const std::string &frame, std::string &raw, int &numeric) const
{
    return parseFirmwareFrame(frame, raw, numeric);
}
#endif

int GeminiProtocol::setOutputSync(unsigned firmwareIndex, bool enabled)
{
    std::string cmd;
    if (!outputCommand(firmwareIndex, enabled, cmd)) return 1;
    return syncCommand(cmd);
}

int GeminiProtocol::setDewModeSync(unsigned channel, DewMode mode)
{
    std::string cmd;
    if (!dewModeCommand(channel, mode, cmd)) return 1;
    return syncCommand(cmd);
}

int GeminiProtocol::setDewManualPercentSync(unsigned channel, unsigned percent)
{
    std::string cmd;
    if (!dewPercentCommand(channel, percent, cmd)) return 1;
    return syncCommand(cmd);
}

int GeminiProtocol::setDewEnabledSync(unsigned channel, bool enabled)
{
    std::string cmd;
    if (!dewEnableCommand(channel, enabled, cmd)) return 1;
    return syncCommand(cmd);
}

int GeminiProtocol::setOutput(unsigned firmwareIndex, bool enabled)
{
    std::string cmd;
    if (!outputCommand(firmwareIndex, enabled, cmd)) return 1;
    return enqueueCommand(cmd);
}

int GeminiProtocol::setDewMode(unsigned channel, DewMode mode)
{
    std::string cmd;
    if (!dewModeCommand(channel, mode, cmd)) return 1;
    return enqueueCommand(cmd);
}

int GeminiProtocol::setDewManualPercent(unsigned channel, unsigned percent)
{
    std::string cmd;
    if (!dewPercentCommand(channel, percent, cmd)) return 1;
    return enqueueCommand(cmd);
}

int GeminiProtocol::setDewEnabled(unsigned channel, bool enabled)
{
    std::string cmd;
    if (!dewEnableCommand(channel, enabled, cmd)) return 1;
    return enqueueCommand(cmd);
}

int GeminiProtocol::recoverStream()
{
    return enqueueCommand(">G#");
}

int GeminiProtocol::enqueueCommand(const std::string &command)
{
    std::lock_guard<std::mutex> qlock(queueMutex_);
    if (!acceptingWrites_.load(std::memory_order_acquire))
    {
        const bool warningEnabled = instrumentationEnabled(instrumentation::Level::Verbose);
        if (warningEnabled)
        {
            const std::string message = "writes not accepted, rejecting: " + command;
            emitWarningInstrumentation(instrumentation::Level::Verbose,
                                       instrumentation::WarningCode::WritesNotAccepted,
                                       message);
        }
        return 3;
    }
    if (queue_.size() >= kQueueCap)
    {
        const bool warningEnabled = instrumentationEnabled(instrumentation::Level::Verbose);
        if (warningEnabled)
        {
            const std::string message = "queue full, rejecting: " + command;
            emitWarningInstrumentation(instrumentation::Level::Verbose,
                                       instrumentation::WarningCode::QueueFull,
                                       message);
        }
        return 2;
    }
    queue_.push_back(command);
    queueCv_.notify_one();
    return 0;
}

void GeminiProtocol::readerLoop(int serialFd, int wakeFd)
{
    std::string acc;
    acc.reserve(256);
    std::string failureReason;
    while (running_.load(std::memory_order_acquire) &&
            !sessionStopRequested_.load(std::memory_order_acquire))
    {
        if (serialFd < 0 && wakeFd < 0) break;
        struct timeval tv = {1, 0};
        fd_set set;
        FD_ZERO(&set);
        int maxFd = -1;
        if (serialFd >= 0)
        {
            FD_SET(serialFd, &set);
            maxFd = std::max(maxFd, serialFd);
        }
        if (wakeFd >= 0)
        {
            FD_SET(wakeFd, &set);
            maxFd = std::max(maxFd, wakeFd);
        }
        int rc = select(maxFd + 1, &set, nullptr, nullptr, &tv);
        if (rc < 0)
        {
            if (errno == EINTR) continue;
            failureReason = std::string("serial select failed: ") + std::strerror(errno);
            break;
        }
        if (rc == 0) continue;
        if (wakeFd >= 0 && FD_ISSET(wakeFd, &set))
        {
            drainReaderWake(wakeFd);
            if (!running_.load(std::memory_order_acquire) ||
                    sessionStopRequested_.load(std::memory_order_acquire))
            {
                break;
            }
        }
        if (serialFd < 0 || !FD_ISSET(serialFd, &set)) continue;
        char buf;
        ssize_t n = ::read(serialFd, &buf, 1);
        if (n < 0)
        {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            failureReason = std::string("serial read failed: ") + std::strerror(errno);
            break;
        }
        if (n == 0)
        {
            failureReason = "serial read returned end-of-file";
            break;
        }
        if (buf == '#')
        {
            if (!acc.empty()) emitFrameInstrumentation(instrumentation::FrameDirection::Rx, acc);
            if (acc.size() >= 2 && acc[0] == '*' && acc[1] == 'G')
            {
                std::string payload = acc.substr(2);
                StatusFrame parsed;
                std::string err;
                if (parseStatusPayload(payload, parsed, err))
                {
                    storeStatusFrame(parsed, true);
                }
                else
                {
                    const bool anomalyEnabled = instrumentationEnabled(instrumentation::Level::Verbose);
                    if (anomalyEnabled)
                    {
                        const std::string detail = "status parse error: " + err;
                        const std::string raw = "*G" + payload + "#";
                        emitParserAnomalyInstrumentation(instrumentation::Level::Verbose,
                                                         instrumentation::ParserAnomalyCode::MalformedStatusFrame,
                                                         detail,
                                                         raw);
                    }
                }
            }
            else if (!acc.empty())
            {
                const bool anomalyEnabled = instrumentationEnabled(instrumentation::Level::Verbose);
                if (anomalyEnabled)
                {
                    const std::string raw = acc + "#";
                    emitParserAnomalyInstrumentation(instrumentation::Level::Verbose,
                                                     instrumentation::ParserAnomalyCode::UnexpectedFrame,
                                                     "ignored frame",
                                                     raw);
                }
            }
            acc.clear();
        }
        else if (acc.empty() && isAsciiWhitespace(buf))
        {
            continue;
        }
        else if (acc.size() < 4096)
        {
            acc += buf;
        }
        else
        {
            if (instrumentationEnabled(instrumentation::Level::Verbose))
            {
                emitParserAnomalyInstrumentation(instrumentation::Level::Verbose,
                                                 instrumentation::ParserAnomalyCode::OversizedFrameDiscarded,
                                                 "oversized frame discarded",
                                                 acc);
            }
            acc.clear();
        }
    }

    if (!failureReason.empty() &&
            running_.load(std::memory_order_acquire) &&
            !sessionStopRequested_.load(std::memory_order_acquire))
    {
        notifyCommunicationFailure(failureReason);
    }
}

void GeminiProtocol::writerLoop()
{
    while (running_.load(std::memory_order_acquire) &&
            !sessionStopRequested_.load(std::memory_order_acquire))
    {
        std::unique_lock<std::mutex> qlock(queueMutex_);
        queueCv_.wait(qlock,
                      [&] { return !running_.load(std::memory_order_acquire)
                                   || sessionStopRequested_.load(std::memory_order_acquire)
                                   || !queue_.empty();
                          });
        if (!running_.load(std::memory_order_acquire) ||
                sessionStopRequested_.load(std::memory_order_acquire))
        {
            break;
        }
        if (queue_.empty()) continue;
        std::string cmd = queue_.front();
        queue_.pop_front();
        qlock.unlock();
        const bool timingEnabled = instrumentationEnabled(instrumentation::Level::Verbose);
        const auto timingStart = timingEnabled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        bool ok = false;
        {
            std::lock_guard<std::mutex> wlock(writeMutex_);
            ok = writeAllLocked(cmd);
        }
        if (timingEnabled) emitTimingInstrumentation(instrumentation::TimingOperation::AsyncWrite, timingStart, ok);
        if (!ok)
        {
            if (instrumentationEnabled(instrumentation::Level::Error))
            {
                const std::string message = "async write failed for command: " + cmd;
                emitWarningInstrumentation(instrumentation::Level::Error,
                                           instrumentation::WarningCode::SerialWriteFailed,
                                           message);
            }
            notifyCommunicationFailure("serial write failed");
            break;
        }
    }
    std::lock_guard<std::mutex> qlock(queueMutex_);
    if (!queue_.empty())
    {
        if (instrumentationEnabled(instrumentation::Level::Info))
        {
            const std::string message = "discarded " + std::to_string(queue_.size()) + " queued commands on session stop";
            emitWarningInstrumentation(instrumentation::Level::Info,
                                       instrumentation::WarningCode::QueuedCommandsDiscarded,
                                       message);
        }
        queue_.clear();
    }
}

int GeminiProtocol::connect(const std::string &port, int settleMs)
{
    return connectInternal(port, settleMs, ConnectionEvent::Connected);
}

int GeminiProtocol::connectInternal(const std::string &port, int settleMs, ConnectionEvent successEvent)
{
    const bool connectTimingEnabled = instrumentationEnabled(instrumentation::Level::Verbose);
    const auto connectTimingStart = connectTimingEnabled ? std::chrono::steady_clock::now() :
                                    std::chrono::steady_clock::time_point{};
    auto finishConnect = [&](int rc)
    {
        if (connectTimingEnabled)
        {
            emitTimingInstrumentation(instrumentation::TimingOperation::Connect, connectTimingStart, rc == 0);
        }
        return rc;
    };

    lastPort_ = port;
    lastSettleMs_ = settleMs;
    acceptingWrites_.store(false, std::memory_order_release);
    connected_.store(false, std::memory_order_release);
    communicationFailureNotified_.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cachedStatus_ = StatusFrame{};
        frameSeq_ = 0;
        lastFrameTime_ = {};
    }
    if (!open(port, settleMs))
    {
        if (instrumentationEnabled(instrumentation::Level::Error))
        {
            const std::string message = "connect: open(" + port + ") failed";
            emitWarningInstrumentation(instrumentation::Level::Error,
                                       instrumentation::WarningCode::TransportError,
                                       message);
        }
        return finishConnect(5);
    }

    auto start = std::chrono::steady_clock::now();
    auto deadline = start + std::chrono::seconds(8);
    auto remainingMs = [&]
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) return 0;
        return (int)std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    };

    // Step 1: handshake
    std::string resp;
    {
        std::lock_guard<std::mutex> wlock(writeMutex_);
        drainInputLocked();
        if (!writeAllLocked(">H#"))
        {
            close();
            return finishConnect(2);
        }
        if (!readTokenFrameOnce('H', resp, std::min(2000, remainingMs())))
        {
            close();
            return finishConnect(2);
        }
    }
    if (resp != "*HGeminiPowerBoxPlusAdv3")
    {
        if (instrumentationEnabled(instrumentation::Level::Error))
        {
            const std::string message = "connect: handshake mismatch: '" + resp + "'";
            emitWarningInstrumentation(instrumentation::Level::Error,
                                       instrumentation::WarningCode::ProtocolRejected,
                                       message);
            if (instrumentationEnabled(instrumentation::Level::Protocol))
            {
                emitParserAnomalyInstrumentation(instrumentation::Level::Protocol,
                                                 instrumentation::ParserAnomalyCode::UnexpectedFrame,
                                                 "handshake mismatch",
                                                 resp + "#");
            }
        }
        close();
        return finishConnect(2);
    }

    // Step 2: firmware
    std::string rawFw;
    int numericFw = 0;
    {
        std::lock_guard<std::mutex> wlock(writeMutex_);
        drainInputLocked();
        if (!writeAllLocked(">V#"))
        {
            close();
            return finishConnect(3);
        }
        if (!readTokenFrameOnce('V', resp, std::min(2000, remainingMs())))
        {
            close();
            return finishConnect(3);
        }
    }
    if (!parseFirmwareFrame(resp, rawFw, numericFw))
    {
        close();
        return finishConnect(3);
    }
    if (numericFw < 308)
    {
        if (instrumentationEnabled(instrumentation::Level::Error))
        {
            const std::string message = "connect: firmware " + std::to_string(numericFw) + " < 308, rejected";
            emitWarningInstrumentation(instrumentation::Level::Error,
                                       instrumentation::WarningCode::ProtocolRejected,
                                       message);
        }
        close();
        return finishConnect(3);
    }
    if (numericFw < 309)
    {
        if (instrumentationEnabled(instrumentation::Level::Info))
        {
            const std::string message = "warning: firmware " + std::to_string(numericFw) +
                                        " < 309; DS18B20 detection fix not present, recommend 309+";
            emitWarningInstrumentation(instrumentation::Level::Info,
                                       instrumentation::WarningCode::FirmwareBelowRecommended,
                                       message);
        }
    }

    // Step 3: G + first frame
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cachedStatus_ = StatusFrame{};
        frameSeq_ = 0;
        lastFrameTime_ = {};
    }
    {
        std::lock_guard<std::mutex> wlock(writeMutex_);
        drainInputLocked();
        if (!writeAllLocked(">G#"))
        {
            close();
            return finishConnect(4);
        }
        if (!readFrameOnce(resp, std::min(4000, remainingMs())))
        {
            close();
            return finishConnect(4);
        }
    }
    if (resp.size() < 4 || resp[0] != '*' || resp[1] != 'G')
    {
        if (instrumentationEnabled(instrumentation::Level::Error))
        {
            const std::string message = "connect: first G frame not received";
            emitWarningInstrumentation(instrumentation::Level::Error,
                                       instrumentation::WarningCode::ProtocolRejected,
                                       message);
            if (instrumentationEnabled(instrumentation::Level::Protocol))
            {
                emitParserAnomalyInstrumentation(instrumentation::Level::Protocol,
                                                 instrumentation::ParserAnomalyCode::UnexpectedFrame,
                                                 message,
                                                 resp.empty() ? std::string() : resp + "#");
            }
        }
        close();
        return finishConnect(4);
    }
    StatusFrame first;
    std::string err;
    if (!parseStatusPayload(resp.substr(2), first, err))
    {
        if (instrumentationEnabled(instrumentation::Level::Error))
        {
            const std::string message = "connect: first frame parse error: " + err;
            emitWarningInstrumentation(instrumentation::Level::Error,
                                       instrumentation::WarningCode::ProtocolRejected,
                                       message);
            if (instrumentationEnabled(instrumentation::Level::Protocol))
            {
                emitParserAnomalyInstrumentation(instrumentation::Level::Protocol,
                                                 instrumentation::ParserAnomalyCode::MalformedStatusFrame,
                                                 message,
                                                 resp + "#");
            }
        }
        close();
        return finishConnect(4);
    }
    if (!openWakePipe())
    {
        close();
        return finishConnect(5);
    }

    // Reset session flags and spawn worker threads
    sessionStopRequested_.store(false, std::memory_order_release);
    reconnectPending_.store(false, std::memory_order_release);
    recoveryAttempted_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> qlock(queueMutex_);
        queue_.clear();
    }
    acceptingWrites_.store(true, std::memory_order_release);
    connected_.store(true, std::memory_order_release);
    connectionGeneration_.fetch_add(1, std::memory_order_acq_rel);
    {
        std::lock_guard<std::mutex> lock(firmwareMutex_);
        firmwareVersionRaw_ = rawFw;
        firmwareVersionNumeric_ = numericFw;
        haveFirmwareVersion_ = true;
    }
    TelemetrySnapshot firstSnapshot = storeStatusFrame(first, false);
    const int readerSerialFd = fd_;
    const int readerWakeFd = wakeReadFd_;
    reader_ = std::thread([this, readerSerialFd, readerWakeFd] { readerLoop(readerSerialFd, readerWakeFd); });
    writer_ = std::thread([this] { writerLoop(); });
    dispatchTelemetrySnapshot(firstSnapshot);
    dispatchConnectionEvent(successEvent);
    return finishConnect(0);
}

int GeminiProtocol::reconnect()
{
    // Called by the supervisor main thread. This may be called either after
    // a reader-reported communication failure (reader already exited) or
    // proactively (reader still running). In both cases we set the stop flag,
    // signal the wake pipe, join both worker threads, and only then close the
    // serial descriptor. This guarantees neither thread can access the fd
    // after close() or after the fd number is reused by a subsequent open().
    acceptingWrites_.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> qlock(queueMutex_);
        sessionStopRequested_.store(true, std::memory_order_release);
    }
    signalReaderWake();
    connected_.store(false, std::memory_order_release);
    queueCv_.notify_all();
    frameCv_.notify_all();
    if (reader_.joinable()) reader_.join();
    if (writer_.joinable()) writer_.join();
    {
        std::lock_guard<std::mutex> wlock(writeMutex_);
        close();
    }
    closeWakePipe();
    {
        std::lock_guard<std::mutex> qlock(queueMutex_);
        if (!queue_.empty())
        {
            if (instrumentationEnabled(instrumentation::Level::Info))
            {
                const std::string message = "discarded " + std::to_string(queue_.size()) + " queued commands on reconnect";
                emitWarningInstrumentation(instrumentation::Level::Info,
                                           instrumentation::WarningCode::QueuedCommandsDiscarded,
                                           message);
            }
            queue_.clear();
        }
    }
    reconnectPending_.store(true, std::memory_order_release);

    int rc = -1;
    const int delaysMs[3] = {0, 1000, 2000};
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (delaysMs[attempt] > 0)
        {
            if (!sleepCancelable(delaysMs[attempt]))
            {
                emitReconnectInstrumentation(attempt + 1, 3,
                                             std::chrono::milliseconds(delaysMs[attempt]),
                                             attempt == 2,
                                             instrumentation::ReconnectResult::Cancelled);
                reconnectPending_.store(false, std::memory_order_release);
                return 6;
            }
        }
        if (!running_.load(std::memory_order_acquire) || cancelled())
        {
            // SIGINT happened during reconnect; abort.
            emitReconnectInstrumentation(attempt + 1, 3,
                                         std::chrono::milliseconds(delaysMs[attempt]),
                                         attempt == 2,
                                         instrumentation::ReconnectResult::Cancelled);
            reconnectPending_.store(false, std::memory_order_release);
            return 6;
        }
        emitReconnectInstrumentation(attempt + 1, 3,
                                     std::chrono::milliseconds(delaysMs[attempt]),
                                     attempt == 2,
                                     instrumentation::ReconnectResult::Starting);
        // The connect() method opens the port itself.
        rc = connectInternal(lastPort_, lastSettleMs_, ConnectionEvent::ReconnectSuccess);
        if (rc == 0)
        {
            emitReconnectInstrumentation(attempt + 1, 3,
                                         std::chrono::milliseconds(delaysMs[attempt]),
                                         attempt == 2,
                                         instrumentation::ReconnectResult::Succeeded);
            reconnectPending_.store(false, std::memory_order_release);
            return 0;
        }
        emitReconnectInstrumentation(attempt + 1, 3,
                                     std::chrono::milliseconds(delaysMs[attempt]),
                                     attempt == 2,
                                     instrumentation::ReconnectResult::AttemptFailed);
    }
    emitReconnectInstrumentation(3, 3,
                                 std::chrono::milliseconds(delaysMs[2]),
                                 true,
                                 instrumentation::ReconnectResult::Exhausted);
    reconnectPending_.store(false, std::memory_order_release);
    return rc ? rc : 7;
}

void GeminiProtocol::disconnect()
{
    const bool wasConnected = connected_.exchange(false, std::memory_order_acq_rel);
    acceptingWrites_.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> qlock(queueMutex_);
        sessionStopRequested_.store(true, std::memory_order_release);
        running_.store(false, std::memory_order_release);
    }
    signalReaderWake();
    queueCv_.notify_all();
    frameCv_.notify_all();
    if (reader_.joinable())
    {
        reader_.join();
    }
    if (writer_.joinable())
    {
        writer_.join();
    }
    {
        std::lock_guard<std::mutex> wlock(writeMutex_);
        close();
    }
    closeWakePipe();
    {
        std::lock_guard<std::mutex> qlock(queueMutex_);
        if (!queue_.empty())
        {
            if (instrumentationEnabled(instrumentation::Level::Info))
            {
                const std::string message = "discarded " + std::to_string(queue_.size()) + " queued commands on shutdown";
                emitWarningInstrumentation(instrumentation::Level::Info,
                                           instrumentation::WarningCode::QueuedCommandsDiscarded,
                                           message);
            }
            queue_.clear();
        }
    }
    if (wasConnected) dispatchConnectionEvent(ConnectionEvent::Disconnected);
}

} // namespace geminipbh
