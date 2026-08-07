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

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <variant>
#include <vector>

namespace geminipbh::instrumentation
{

enum class Level
{
    Off = 0,
    Error,
    Info,
    Protocol,
    Verbose,
    Trace
};

enum class EventType
{
    Connection,
    FrameTransmitted,
    FrameReceived,
    StatusDecoded,
    FieldChanges,
    Verification,
    Warning,
    ParserAnomaly,
    ReconnectAttempt,
    Timing
};

enum class ConnectionState
{
    Connected,
    Disconnected,
    CommunicationFailure,
    ReconnectSuccess
};

enum class FrameDirection { Tx, Rx };
enum class VerificationOperation { CommandStateConfirm };
enum class VerificationOutcome { Passed, Failed };

enum class WarningCode
{
    FirmwareBelowRecommended,
    QueueFull,
    WritesNotAccepted,
    QueuedCommandsDiscarded,
    CallbackException,
    TransportError,
    SerialWriteFailed,
    ProtocolRejected
};

enum class ParserAnomalyCode
{
    MalformedStatusFrame,
    IgnoredFrameWhileWaiting,
    UnexpectedFrame,
    OversizedFrameDiscarded,
    MissingTerminator,
    TrailingData,
    InvalidField,
    OutOfRangeField
};

enum class ReconnectResult
{
    Starting,
    AttemptFailed,
    Succeeded,
    Cancelled,
    Exhausted
};

enum class TimingOperation
{
    Open,
    Handshake,
    ReadFirmware,
    FirstStatusFrame,
    Connect,
    ReconnectAttempt,
    SyncWrite,
    AsyncWrite,
    ParseStatusPayload
};

using Value = std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double, std::string>;

struct NamedValue
{
    std::string name;
    Value value;
};

struct FieldChange
{
    std::string name;
    Value previous;
    Value current;
};

struct ConnectionPayload
{
    ConnectionState state = ConnectionState::Disconnected;
    std::string reason;
};

struct FramePayload
{
    FrameDirection direction = FrameDirection::Tx;
    std::string frame;
    std::string token;
    bool complete = true;
};

struct StatusDecodedPayload
{
    std::string frameType;
    std::uint64_t sequence = 0;
    std::vector<NamedValue> fields;
};

struct FieldChangesPayload
{
    std::uint64_t sequence = 0;
    std::vector<FieldChange> changes;
};

struct VerificationPayload
{
    VerificationOperation operation = VerificationOperation::CommandStateConfirm;
    VerificationOutcome outcome = VerificationOutcome::Failed;
    std::string detail;
    std::uint64_t relatedSequence = 0;
};

struct WarningPayload
{
    WarningCode code = WarningCode::CallbackException;
    std::string message;
};

struct ParserAnomalyPayload
{
    ParserAnomalyCode code = ParserAnomalyCode::UnexpectedFrame;
    std::string detail;
    std::string frame;
};

struct ReconnectAttemptPayload
{
    unsigned attempt = 0;
    unsigned maxAttempts = 0;
    std::chrono::milliseconds delayBeforeAttempt{0};
    bool finalAttempt = false;
    ReconnectResult result = ReconnectResult::Starting;
};

struct TimingPayload
{
    TimingOperation operation = TimingOperation::Connect;
    std::chrono::microseconds duration{0};
    bool success = false;
};

using Payload = std::variant <
                ConnectionPayload,
                FramePayload,
                StatusDecodedPayload,
                FieldChangesPayload,
                VerificationPayload,
                WarningPayload,
                ParserAnomalyPayload,
                ReconnectAttemptPayload,
                TimingPayload >;

struct Event
{
    Level level = Level::Info;
    EventType type = EventType::Connection;
    std::chrono::system_clock::time_point wallTime;
    std::chrono::steady_clock::time_point steadyTime;
    std::string source;
    std::uint64_t connectionGeneration = 0;
    std::uint64_t sequence = 0;
    Payload payload;
};

class Dispatcher
{
    public:
        using Listener = std::function<void(const Event &)>;
        using ListenerHandle = std::uint64_t;

        Dispatcher();
        ~Dispatcher();

        Dispatcher(const Dispatcher &) = delete;
        Dispatcher &operator=(const Dispatcher &) = delete;
        Dispatcher(Dispatcher &&) = delete;
        Dispatcher &operator=(Dispatcher &&) = delete;

        void setLevel(Level level);
        Level level() const;
        bool enabled(Level eventLevel) const;

        ListenerHandle registerListener(Listener listener);
        bool removeListener(ListenerHandle handle);

        void emit(Event event) noexcept;

    private:
        struct ListenerEntry;

        void beginDispatch();
        void endDispatch();

        std::atomic<Level> level_{Level::Off};
        std::atomic<std::size_t> listenerCount_{0};
        std::mutex listenerMutex_;
        std::vector<std::shared_ptr<ListenerEntry>> listeners_;
        ListenerHandle nextListenerHandle_ = 1;

        std::mutex dispatchMutex_;
        std::condition_variable dispatchCv_;
        bool dispatchInProgress_ = false;
};

Event makeEvent(Level level, EventType type, Payload payload);

const char *toString(Level level);
const char *toString(EventType type);
const char *toString(ConnectionState state);
const char *toString(FrameDirection direction);
const char *toString(VerificationOperation operation);
const char *toString(VerificationOutcome outcome);
const char *toString(WarningCode code);
const char *toString(ParserAnomalyCode code);
const char *toString(ReconnectResult result);
const char *toString(TimingOperation operation);

std::string formatValue(const Value &value);
std::string escapeFrame(const std::string &frame);

class EventFormatter
{
    public:
        std::string format(const Event &event) const;
};

class EventLogger
{
    public:
        using Sink = std::function<void(Level, const std::string &)>;

        EventLogger() = default;
        explicit EventLogger(Sink sink);

        void setSink(Sink sink);
        void operator()(const Event &event) const noexcept;

    private:
        Sink sink_;
        EventFormatter formatter_;
};

} // namespace geminipbh::instrumentation
