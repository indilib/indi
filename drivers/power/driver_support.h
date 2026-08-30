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

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

#include "indi-gemini-pbh/connection_event.h"
#include "indi-gemini-pbh/instrumentation.h"
#include "indi-gemini-pbh/operation_confirmation.h"

namespace geminipbh::indi_driver
{

enum class DriverPropertyState
{
    Idle,
    Ok,
    Busy,
    Alert
};

enum class PendingOperationKind
{
    DcOutput,
    UsbOutput,
    HeaterEnabled,
    HeaterMode
};

enum class PendingEvaluation
{
    Confirmed,
    Pending,
    Timeout,
    NotConfirmable,
    InvalidTarget,
    ValueUnavailable
};

struct PendingOperation
{
    PendingOperation(PendingOperationKind kind,
                     std::size_t index,
                     OperationTarget target,
                     std::chrono::steady_clock::time_point deadline)
        : kind(kind), index(index), target(target), deadline(deadline)
    {
    }

    PendingOperationKind kind;
    std::size_t index = 0;
    OperationTarget target;
    std::chrono::steady_clock::time_point deadline;
};

inline const char *errorName(Error error)
{
    switch (error)
    {
        case Error::None:
            return "none";
        case Error::InvalidChannel:
            return "invalid channel";
        case Error::InvalidValue:
            return "invalid value";
        case Error::NotConnected:
            return "not connected";
        case Error::CommunicationFailure:
            return "communication failure";
        case Error::QueueRejected:
            return "command queue rejected the request";
        case Error::ProtocolRejected:
            return "protocol rejected the request";
        case Error::ValueUnavailable:
            return "value unavailable";
        case Error::InvalidState:
            return "invalid connection state";
        case Error::Busy:
            return "device busy";
        case Error::Cancelled:
            return "operation cancelled";
    }
    return "unknown error";
}

inline DriverPropertyState propertyStateForImmediateResult(Result result)
{
    return result ? DriverPropertyState::Busy : DriverPropertyState::Alert;
}

inline const char *pendingOperationName(PendingOperationKind kind)
{
    switch (kind)
    {
        case PendingOperationKind::DcOutput:
            return "DC output";
        case PendingOperationKind::UsbOutput:
            return "USB output";
        case PendingOperationKind::HeaterEnabled:
            return "heater enabled state";
        case PendingOperationKind::HeaterMode:
            return "heater mode";
    }
    return "operation";
}

inline bool sameLogicalOperation(const PendingOperation &operation, PendingOperationKind kind, std::size_t index)
{
    return operation.kind == kind && operation.index == index;
}

inline void replacePendingOperation(std::vector<PendingOperation> &operations, PendingOperation replacement)
{
    operations.erase(std::remove_if(operations.begin(), operations.end(),
                                    [&](const PendingOperation & operation)
    {
        return sameLogicalOperation(operation, replacement.kind, replacement.index);
    }), operations.end());
    operations.push_back(replacement);
}

inline ValueResult<OperationTarget> makeBooleanTarget(PendingOperationKind kind, std::size_t index, bool enabled)
{
    switch (kind)
    {
        case PendingOperationKind::DcOutput:
            return OperationTarget::dcOutput(index, enabled);
        case PendingOperationKind::UsbOutput:
            return OperationTarget::usbOutput(index, enabled);
        case PendingOperationKind::HeaterEnabled:
            return OperationTarget::heaterEnabled(index, enabled);
        case PendingOperationKind::HeaterMode:
            return ValueResult<OperationTarget>::failure(Error::InvalidValue);
    }
    return ValueResult<OperationTarget>::failure(Error::InvalidValue);
}

inline ValueResult<OperationTarget> makeHeaterModeTarget(std::size_t index, HeaterMode mode)
{
    return OperationTarget::heaterMode(index, mode);
}

inline PendingEvaluation evaluatePendingOperation(const PendingOperation &operation,
        const TelemetrySnapshot *snapshot,
        std::chrono::steady_clock::time_point now)
{
    if (now > operation.deadline)
        return PendingEvaluation::Timeout;

    if (!snapshot)
        return PendingEvaluation::ValueUnavailable;

    switch (confirmOperation(operation.target, snapshot))
    {
        case Confirmation::Confirmed:
            return PendingEvaluation::Confirmed;
        case Confirmation::NotConfirmed:
            return PendingEvaluation::Pending;
        case Confirmation::NotConfirmable:
            return PendingEvaluation::NotConfirmable;
        case Confirmation::InvalidTarget:
            return PendingEvaluation::InvalidTarget;
        case Confirmation::ValueUnavailable:
            return PendingEvaluation::ValueUnavailable;
    }
    return PendingEvaluation::InvalidTarget;
}

inline const char *heaterModeName(HeaterMode mode)
{
    switch (mode)
    {
        case HeaterMode::Auto:
            return "Auto";
        case HeaterMode::ManualPwm:
            return "Manual PWM";
        case HeaterMode::BinarySwitch:
            return "Switch";
    }
    return "Unknown";
}

inline bool heaterModeFromSwitchName(const char *name, HeaterMode &mode)
{
    if (std::strcmp(name, "AUTO") == 0)
    {
        mode = HeaterMode::Auto;
        return true;
    }
    if (std::strcmp(name, "MANUAL_PWM") == 0)
    {
        mode = HeaterMode::ManualPwm;
        return true;
    }
    if (std::strcmp(name, "SWITCH") == 0)
    {
        mode = HeaterMode::BinarySwitch;
        return true;
    }
    return false;
}

inline std::size_t heaterModeSwitchIndex(HeaterMode mode)
{
    switch (mode)
    {
        case HeaterMode::Auto:
            return 0;
        case HeaterMode::ManualPwm:
            return 1;
        case HeaterMode::BinarySwitch:
            return 2;
    }
    return 0;
}

inline bool manualPowerIsTelemetryConfirmable()
{
    return false;
}

inline bool instrumentationDuplicatesSemanticDriverLog(const instrumentation::Event &event)
{
    if (event.type != instrumentation::EventType::Connection)
        return false;

    const auto *payload = std::get_if<instrumentation::ConnectionPayload>(&event.payload);
    if (payload == nullptr)
        return false;

    switch (payload->state)
    {
        case instrumentation::ConnectionState::Connected:
        case instrumentation::ConnectionState::Disconnected:
        case instrumentation::ConnectionState::ReconnectSuccess:
            return true;
        case instrumentation::ConnectionState::CommunicationFailure:
            return false;
    }
    return false;
}

class CallbackStaging
{
    public:
        bool stageTelemetry(const TelemetrySnapshot &snapshot)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (shutdown_)
                return false;
            stagedTelemetry_.emplace(snapshot);
            latestTelemetry_.emplace(snapshot);
            telemetryDirty_ = true;
            return true;
        }

        bool stageConnectionEvent(ConnectionEvent event)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (shutdown_)
                return false;
            stagedConnectionEvents_.push_back(event);
            return true;
        }

        std::optional<TelemetrySnapshot> takeTelemetry()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!telemetryDirty_ || !stagedTelemetry_)
                return std::nullopt;
            std::optional<TelemetrySnapshot> snapshot;
            snapshot.emplace(*stagedTelemetry_);
            telemetryDirty_ = false;
            return snapshot;
        }

        std::optional<TelemetrySnapshot> latestTelemetry() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!latestTelemetry_)
            return std::nullopt;
            std::optional<TelemetrySnapshot> snapshot;
            snapshot.emplace(*latestTelemetry_);
            return snapshot;
        }

    std::deque<ConnectionEvent> takeConnectionEvents()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::deque<ConnectionEvent> events;
            events.swap(stagedConnectionEvents_);
            return events;
        }

        void clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stagedTelemetry_.reset();
            latestTelemetry_.reset();
            stagedConnectionEvents_.clear();
            telemetryDirty_ = false;
        }

        void beginShutdown()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
            stagedTelemetry_.reset();
            latestTelemetry_.reset();
            stagedConnectionEvents_.clear();
            telemetryDirty_ = false;
        }

    private:
        mutable std::mutex mutex_;
        std::optional<TelemetrySnapshot> stagedTelemetry_;
        std::optional<TelemetrySnapshot> latestTelemetry_;
        std::deque<ConnectionEvent> stagedConnectionEvents_;
        bool telemetryDirty_ = false;
        bool shutdown_ = false;
};

} // namespace geminipbh::indi_driver
