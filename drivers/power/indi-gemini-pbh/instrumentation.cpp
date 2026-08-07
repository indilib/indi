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

#include "indi-gemini-pbh/instrumentation.h"

#include <algorithm>
#include <exception>
#include <iomanip>
#include <sstream>
#include <utility>

namespace geminipbh::instrumentation
{

struct Dispatcher::ListenerEntry
{
    ListenerEntry(ListenerHandle handle, Listener listener)
        : handle(handle), listener(std::move(listener))
    {
    }

    const ListenerHandle handle;
    Listener listener;
    bool active = true;
    unsigned activeCalls = 0;
    std::condition_variable cv;
};

namespace
{
thread_local const Dispatcher *currentDispatcher = nullptr;
thread_local Dispatcher::ListenerHandle currentListenerHandle = 0;

bool levelAllows(Level configured, Level eventLevel)
{
    return eventLevel != Level::Off &&
           configured != Level::Off &&
           static_cast<int>(configured) >= static_cast<int>(eventLevel);
}
}

Dispatcher::Dispatcher() = default;

Dispatcher::~Dispatcher()
{
    std::unique_lock<std::mutex> dispatchLock(dispatchMutex_);
    dispatchCv_.wait(dispatchLock, [&] { return !dispatchInProgress_; });
    dispatchLock.unlock();

    std::lock_guard<std::mutex> listenerLock(listenerMutex_);
    for (const auto &entry : listeners_) entry->active = false;
    listeners_.clear();
    listenerCount_.store(0, std::memory_order_release);
}

void Dispatcher::setLevel(Level level)
{
    level_.store(level, std::memory_order_release);
}

Level Dispatcher::level() const
{
    return level_.load(std::memory_order_acquire);
}

bool Dispatcher::enabled(Level eventLevel) const
{
    return listenerCount_.load(std::memory_order_acquire) != 0 &&
           levelAllows(level_.load(std::memory_order_acquire), eventLevel);
}

Dispatcher::ListenerHandle Dispatcher::registerListener(Listener listener)
{
    if (!listener) return 0;

    std::lock_guard<std::mutex> lock(listenerMutex_);
    ListenerHandle handle = nextListenerHandle_++;
    if (nextListenerHandle_ == 0) ++nextListenerHandle_;
    listeners_.push_back(std::make_shared<ListenerEntry>(handle, std::move(listener)));
    listenerCount_.store(listeners_.size(), std::memory_order_release);
    return handle;
}

bool Dispatcher::removeListener(ListenerHandle handle)
{
    if (handle == 0) return false;

    std::shared_ptr<ListenerEntry> entry;
    std::unique_lock<std::mutex> lock(listenerMutex_);
    auto it = std::find_if(listeners_.begin(), listeners_.end(),
                           [handle](const std::shared_ptr<ListenerEntry> &candidate)
    {
        return candidate->handle == handle;
    });
    if (it == listeners_.end()) return false;

    entry = *it;
    entry->active = false;
    listeners_.erase(it);
    listenerCount_.store(listeners_.size(), std::memory_order_release);

    if (currentDispatcher == this && currentListenerHandle == handle) return true;

    entry->cv.wait(lock, [&] { return entry->activeCalls == 0; });
    return true;
}

void Dispatcher::beginDispatch()
{
    std::unique_lock<std::mutex> lock(dispatchMutex_);
    dispatchCv_.wait(lock, [&] { return !dispatchInProgress_; });
    dispatchInProgress_ = true;
}

void Dispatcher::endDispatch()
{
    {
        std::lock_guard<std::mutex> lock(dispatchMutex_);
        dispatchInProgress_ = false;
    }
    dispatchCv_.notify_all();
}

void Dispatcher::emit(Event event) noexcept
{
    if (!enabled(event.level)) return;

    try
    {
        beginDispatch();
        }
        catch (...)
        {
            return;
        }

    struct DispatchGuard
    {
        Dispatcher *dispatcher;
        ~DispatchGuard()
        {
            dispatcher->endDispatch();
        }
    } guard{this};

    try
    {
        std::vector<std::shared_ptr<ListenerEntry>> entries;
        {
            std::lock_guard<std::mutex> lock(listenerMutex_);
            entries = listeners_;
        }

        for (const auto &entry : entries)
        {
            Listener listener;
            {
                std::lock_guard<std::mutex> lock(listenerMutex_);
                if (!entry->active) continue;
                listener = entry->listener;
                ++entry->activeCalls;
            }

            currentDispatcher = this;
            currentListenerHandle = entry->handle;
            try
            {
                listener(event);
            }
            catch (...)
            {
            }
            currentListenerHandle = 0;
            currentDispatcher = nullptr;

            {
                std::lock_guard<std::mutex> lock(listenerMutex_);
                --entry->activeCalls;
                if (entry->activeCalls == 0) entry->cv.notify_all();
            }
        }
    }
    catch (...)
    {
    }
}

Event makeEvent(Level level, EventType type, Payload payload)
{
    Event event;
    event.level = level;
    event.type = type;
    event.wallTime = std::chrono::system_clock::now();
    event.steadyTime = std::chrono::steady_clock::now();
    event.payload = std::move(payload);
    return event;
}

const char *toString(Level level)
{
    switch (level)
    {
        case Level::Off:
            return "off";
        case Level::Error:
            return "error";
        case Level::Info:
            return "info";
        case Level::Protocol:
            return "protocol";
        case Level::Verbose:
            return "verbose";
        case Level::Trace:
            return "trace";
    }
    return "unknown";
}

const char *toString(EventType type)
{
    switch (type)
    {
        case EventType::Connection:
            return "connection";
        case EventType::FrameTransmitted:
            return "frame_transmitted";
        case EventType::FrameReceived:
            return "frame_received";
        case EventType::StatusDecoded:
            return "status_decoded";
        case EventType::FieldChanges:
            return "field_changes";
        case EventType::Verification:
            return "verification";
        case EventType::Warning:
            return "warning";
        case EventType::ParserAnomaly:
            return "parser_anomaly";
        case EventType::ReconnectAttempt:
            return "reconnect_attempt";
        case EventType::Timing:
            return "timing";
    }
    return "unknown";
}

const char *toString(ConnectionState state)
{
    switch (state)
    {
        case ConnectionState::Connected:
            return "connected";
        case ConnectionState::Disconnected:
            return "disconnected";
        case ConnectionState::CommunicationFailure:
            return "communication_failure";
        case ConnectionState::ReconnectSuccess:
            return "reconnect_success";
    }
    return "unknown";
}

const char *toString(FrameDirection direction)
{
    switch (direction)
    {
        case FrameDirection::Tx:
            return "TX";
        case FrameDirection::Rx:
            return "RX";
    }
    return "unknown";
}

const char *toString(VerificationOperation operation)
{
    switch (operation)
    {
        case VerificationOperation::CommandStateConfirm:
            return "command_state_confirm";
    }
    return "unknown";
}

const char *toString(VerificationOutcome outcome)
{
    switch (outcome)
    {
        case VerificationOutcome::Passed:
            return "passed";
        case VerificationOutcome::Failed:
            return "failed";
    }
    return "unknown";
}

const char *toString(WarningCode code)
{
    switch (code)
    {
        case WarningCode::FirmwareBelowRecommended:
            return "firmware_below_recommended";
        case WarningCode::QueueFull:
            return "queue_full";
        case WarningCode::WritesNotAccepted:
            return "writes_not_accepted";
        case WarningCode::QueuedCommandsDiscarded:
            return "queued_commands_discarded";
        case WarningCode::CallbackException:
            return "callback_exception";
        case WarningCode::TransportError:
            return "transport_error";
        case WarningCode::SerialWriteFailed:
            return "serial_write_failed";
        case WarningCode::ProtocolRejected:
            return "protocol_rejected";
    }
    return "unknown";
}

const char *toString(ParserAnomalyCode code)
{
    switch (code)
    {
        case ParserAnomalyCode::MalformedStatusFrame:
            return "malformed_status_frame";
        case ParserAnomalyCode::IgnoredFrameWhileWaiting:
            return "ignored_frame_while_waiting";
        case ParserAnomalyCode::UnexpectedFrame:
            return "unexpected_frame";
        case ParserAnomalyCode::OversizedFrameDiscarded:
            return "oversized_frame_discarded";
        case ParserAnomalyCode::MissingTerminator:
            return "missing_terminator";
        case ParserAnomalyCode::TrailingData:
            return "trailing_data";
        case ParserAnomalyCode::InvalidField:
            return "invalid_field";
        case ParserAnomalyCode::OutOfRangeField:
            return "out_of_range_field";
    }
    return "unknown";
}

const char *toString(ReconnectResult result)
{
    switch (result)
    {
        case ReconnectResult::Starting:
            return "starting";
        case ReconnectResult::AttemptFailed:
            return "attempt_failed";
        case ReconnectResult::Succeeded:
            return "succeeded";
        case ReconnectResult::Cancelled:
            return "cancelled";
        case ReconnectResult::Exhausted:
            return "exhausted";
    }
    return "unknown";
}

const char *toString(TimingOperation operation)
{
    switch (operation)
    {
        case TimingOperation::Open:
            return "open";
        case TimingOperation::Handshake:
            return "handshake";
        case TimingOperation::ReadFirmware:
            return "read_firmware";
        case TimingOperation::FirstStatusFrame:
            return "first_status_frame";
        case TimingOperation::Connect:
            return "connect";
        case TimingOperation::ReconnectAttempt:
            return "reconnect_attempt";
        case TimingOperation::SyncWrite:
            return "sync_write";
        case TimingOperation::AsyncWrite:
            return "async_write";
        case TimingOperation::ParseStatusPayload:
            return "parse_status_payload";
    }
    return "unknown";
}

std::string formatValue(const Value &value)
{
    struct Visitor
    {
        std::string operator()(std::monostate) const
        {
            return "null";
        }
        std::string operator()(bool value) const
        {
            return value ? "true" : "false";
        }
        std::string operator()(std::int64_t value) const
        {
            return std::to_string(value);
        }
        std::string operator()(std::uint64_t value) const
        {
            return std::to_string(value);
        }
        std::string operator()(double value) const
        {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
        std::string operator()(const std::string &value) const
        {
            return value;
        }
    };
    return std::visit(Visitor{}, value);
}

std::string escapeFrame(const std::string &frame)
{
    std::ostringstream oss;
    for (char c : frame)
    {
        unsigned char byte = static_cast<unsigned char>(c);
        if (c == '\r') oss << "<CR>";
        else if (c == '\n') oss << "<LF>";
        else if (c == '#') oss << "<HASH>";
        else if (byte < 0x20
                 || byte >= 0x7f) oss << "<0x" << std::hex << std::uppercase << static_cast<int>(byte) << ">" << std::dec;
        else oss << c;
    }
    return oss.str();
}

namespace
{
std::string formatNamedValues(const std::vector<NamedValue> &fields)
{
    std::ostringstream oss;
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        if (i > 0) oss << ", ";
        oss << fields[i].name << '=' << formatValue(fields[i].value);
    }
    return oss.str();
}

std::string formatFieldChanges(const std::vector<FieldChange> &changes)
{
    std::ostringstream oss;
    for (std::size_t i = 0; i < changes.size(); ++i)
    {
        if (i > 0) oss << ", ";
        oss << changes[i].name << '=' << formatValue(changes[i].previous)
            << "->" << formatValue(changes[i].current);
    }
    return oss.str();
}
}

std::string EventFormatter::format(const Event &event) const
{
    return std::visit([&](const auto & payload) -> std::string
    {
        using PayloadType = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same<PayloadType, ConnectionPayload>::value)
        {
            if (payload.state == ConnectionState::CommunicationFailure && !payload.reason.empty())
            {
                return "communication failure: " + payload.reason;
            }
            std::string out = std::string("connection: ") + toString(payload.state);
            if (!payload.reason.empty()) out += ": " + payload.reason;
            return out;
        }
        else if constexpr (std::is_same<PayloadType, FramePayload>::value)
        {
            return std::string(toString(payload.direction)) + ": " + escapeFrame(payload.frame);
        }
        else if constexpr (std::is_same<PayloadType, StatusDecodedPayload>::value)
        {
            std::string out = "status decoded";
            if (payload.sequence != 0) out += " seq=" + std::to_string(payload.sequence);
            if (!payload.frameType.empty()) out += " type=" + payload.frameType;
            if (!payload.fields.empty()) out += ": " + formatNamedValues(payload.fields);
            return out;
        }
        else if constexpr (std::is_same<PayloadType, FieldChangesPayload>::value)
        {
            std::string out = "field changes";
            if (payload.sequence != 0) out += " seq=" + std::to_string(payload.sequence);
            if (!payload.changes.empty()) out += ": " + formatFieldChanges(payload.changes);
            return out;
        }
        else if constexpr (std::is_same<PayloadType, VerificationPayload>::value)
        {
            std::string out = std::string("verification ") + toString(payload.outcome) + " " + toString(payload.operation);
            if (payload.relatedSequence != 0) out += " seq=" + std::to_string(payload.relatedSequence);
            if (!payload.detail.empty()) out += ": " + payload.detail;
            return out;
        }
        else if constexpr (std::is_same<PayloadType, WarningPayload>::value)
        {
            if (!payload.message.empty()) return payload.message;
            return std::string("warning: ") + toString(payload.code);
        }
        else if constexpr (std::is_same<PayloadType, ParserAnomalyPayload>::value)
        {
            std::string out = std::string("parser anomaly ") + toString(payload.code);
            if (!payload.detail.empty()) out += ": " + payload.detail;
            if (!payload.frame.empty()) out += " (raw: " + escapeFrame(payload.frame) + ")";
            return out;
        }
        else if constexpr (std::is_same<PayloadType, ReconnectAttemptPayload>::value)
        {
            if (payload.result == ReconnectResult::Exhausted)
            {
                return "reconnect: exhausted retries, exiting";
            }
            if (payload.result == ReconnectResult::Starting && payload.attempt != 0 && payload.maxAttempts != 0)
            {
                return "reconnect: attempt " + std::to_string(payload.attempt) + "/" + std::to_string(payload.maxAttempts);
            }
            std::string out = "reconnect: ";
            if (payload.attempt != 0 && payload.maxAttempts != 0)
                out += "attempt " + std::to_string(payload.attempt) + "/" + std::to_string(payload.maxAttempts) + " ";
            out += toString(payload.result);
            if (payload.delayBeforeAttempt.count() > 0)
                out += " after " + std::to_string(payload.delayBeforeAttempt.count()) + "ms";
            return out;
        }
        else if constexpr (std::is_same<PayloadType, TimingPayload>::value)
        {
            return std::string("timing: ") + toString(payload.operation) + " " +
                   (payload.success ? "ok" : "failed") + " " +
                   std::to_string(payload.duration.count()) + "us";
        }
        return std::string(toString(event.type));
    }, event.payload);
}

EventLogger::EventLogger(Sink sink) : sink_(std::move(sink)) {}

void EventLogger::setSink(Sink sink)
{
    sink_ = std::move(sink);
}

void EventLogger::operator()(const Event &event) const noexcept
{
    if (!sink_) return;
    try
    {
        sink_(event.level, formatter_.format(event));
        }
        catch (...)
        {
        }
}

} // namespace geminipbh::instrumentation
