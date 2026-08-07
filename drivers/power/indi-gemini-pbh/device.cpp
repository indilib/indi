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

#include "indi-gemini-pbh/device.h"

#include <array>
#include <atomic>
#include <mutex>
#include <utility>

#include "indi-gemini-pbh/protocol.h"
#include "indi-gemini-pbh/status_frame.h"

namespace geminipbh
{

namespace
{
constexpr std::size_t kDcOutputCount = 4;
constexpr std::size_t kUsbOutputCount = 6;
constexpr std::size_t kHeaterCount = 2;
constexpr unsigned kDcFirmwareBase = 2;
constexpr unsigned kUsbFirmwareBase = 6;
constexpr unsigned kHeaterFirmwareBase = 6;

Result commandResult(int rc, bool connected, bool communicationFailed)
{
    switch (rc)
    {
        case 0:
            return Result::success();
        case 1:
            return Result::failure(Error::ProtocolRejected);
        case 2:
            return Result::failure(Error::QueueRejected);
        case 3:
            return Result::failure(communicationFailed ? Error::CommunicationFailure :
                                   (connected ? Error::CommunicationFailure : Error::NotConnected));
        default:
            return Result::failure(Error::ProtocolRejected);
    }
}

Result connectResult(int rc, bool cancellationObserved)
{
    if (cancellationObserved) return Result::failure(Error::Cancelled);

    switch (rc)
    {
        case 0:
            return Result::success();
        case 6:
            return Result::failure(Error::Cancelled);
        case 5:
        case 7:
            return Result::failure(Error::CommunicationFailure);
        case 2:
        case 3:
        case 4:
        default:
            return Result::failure(Error::ProtocolRejected);
    }
}

bool toProtocolMode(HeaterMode mode, DewMode &protocolMode)
{
    switch (mode)
    {
        case HeaterMode::Auto:
            protocolMode = DewMode::Automatic;
            return true;
        case HeaterMode::ManualPwm:
            protocolMode = DewMode::Manual;
            return true;
        case HeaterMode::BinarySwitch:
            protocolMode = DewMode::Switch;
            return true;
    }
    return false;
}
}

struct Device::Impl
{
    Impl()
    {
        protocol.setCancelFunction([this]
        {
            std::function<bool()> callback;
            {
                std::lock_guard<std::mutex> lock(cancelMutex);
                callback = cancelFunction;
            }
            const bool cancelled = callback && callback();
            if (cancelled) cancellationObserved.store(true, std::memory_order_release);
            return cancelled;
        });

        internalTelemetryCallback_ = protocol.registerTelemetryCallback(
                                         [this](const TelemetrySnapshot & snapshot)
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            latestTelemetry = std::make_shared<TelemetrySnapshot>(snapshot);
        });

        internalConnectionCallback_ = protocol.registerConnectionCallback(
                                          [this](ConnectionEvent event)
        {
            switch (event)
            {
                case ConnectionEvent::Connected:
                case ConnectionEvent::ReconnectSuccess:
                    state.store(ConnectionState::Connected, std::memory_order_release);
                    break;
                case ConnectionEvent::CommunicationFailure:
                    invalidateTelemetry();
                    state.store(ConnectionState::CommunicationFailure, std::memory_order_release);
                    break;
                case ConnectionEvent::Disconnected:
                    invalidateTelemetry();
                    state.store(ConnectionState::Disconnected, std::memory_order_release);
                    break;
            }
        });
    }

    ~Impl()
    {
        protocol.disconnect();
        protocol.removeTelemetryCallback(internalTelemetryCallback_);
        protocol.removeConnectionCallback(internalConnectionCallback_);
    }

    Result requireConnected() const
    {
        switch (state.load(std::memory_order_acquire))
    {
        case ConnectionState::Connected:
            return Result::success();
            case ConnectionState::CommunicationFailure:
                return Result::failure(Error::CommunicationFailure);
            case ConnectionState::Connecting:
            case ConnectionState::Reconnecting:
                return Result::failure(Error::Busy);
            case ConnectionState::Disconnected:
                return Result::failure(Error::NotConnected);
        }
        return Result::failure(Error::InvalidState);
    }

    void invalidateTelemetry()
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        latestTelemetry.reset();
    }

    std::shared_ptr<const TelemetrySnapshot> snapshot() const
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        return latestTelemetry;
    }

    Result finishCommand(int rc) const
    {
        ConnectionState current = state.load(std::memory_order_acquire);
        return commandResult(rc,
                             current == ConnectionState::Connected,
                             current == ConnectionState::CommunicationFailure);
    }

    GeminiProtocol protocol;
    mutable std::mutex stateMutex;
    std::shared_ptr<const TelemetrySnapshot> latestTelemetry;
    std::array<unsigned, kHeaterCount> heaterManualPower{};
    std::array<bool, kHeaterCount> haveHeaterManualPower{{false, false}};
    std::atomic<ConnectionState> state{ConnectionState::Disconnected};
    mutable std::mutex lifecycleMutex;
    mutable std::mutex cancelMutex;
    std::function<bool()> cancelFunction;
    std::atomic<bool> cancellationObserved{false};
    GeminiProtocol::TelemetryCallbackHandle internalTelemetryCallback_ = 0;
    GeminiProtocol::ConnectionCallbackHandle internalConnectionCallback_ = 0;
};

Device::Device() : impl_(new Impl()) {}

Device::~Device() = default;

Result Device::connect(const std::string &port, int settleMs)
{
    std::unique_lock<std::mutex> lifecycleLock(impl_->lifecycleMutex, std::try_to_lock);
    if (!lifecycleLock.owns_lock()) return Result::failure(Error::Busy);
    if (impl_->state.load(std::memory_order_acquire) != ConnectionState::Disconnected)
        return Result::failure(Error::InvalidState);

    impl_->invalidateTelemetry();
    impl_->state.store(ConnectionState::Connecting, std::memory_order_release);
    impl_->cancellationObserved.store(false, std::memory_order_release);
    const int rc = impl_->protocol.connect(port, settleMs);
    Result result = connectResult(rc, impl_->cancellationObserved.load(std::memory_order_acquire));
    if (!result)
    {
        impl_->invalidateTelemetry();
        impl_->state.store(ConnectionState::Disconnected, std::memory_order_release);
    }
    return result;
}

Result Device::reconnect()
{
    std::unique_lock<std::mutex> lifecycleLock(impl_->lifecycleMutex, std::try_to_lock);
    if (!lifecycleLock.owns_lock()) return Result::failure(Error::Busy);

    ConnectionState current = impl_->state.load(std::memory_order_acquire);
    if (current == ConnectionState::Disconnected) return Result::failure(Error::NotConnected);
    if (current == ConnectionState::Connecting || current == ConnectionState::Reconnecting)
        return Result::failure(Error::Busy);

    impl_->invalidateTelemetry();
    impl_->state.store(ConnectionState::Reconnecting, std::memory_order_release);
    impl_->cancellationObserved.store(false, std::memory_order_release);
    const int rc = impl_->protocol.reconnect();
    Result result = connectResult(rc, impl_->cancellationObserved.load(std::memory_order_acquire));
    if (!result)
    {
        impl_->invalidateTelemetry();
        impl_->state.store(ConnectionState::CommunicationFailure, std::memory_order_release);
    }
    return result;
}

Result Device::disconnect()
{
    std::unique_lock<std::mutex> lifecycleLock(impl_->lifecycleMutex, std::try_to_lock);
    if (!lifecycleLock.owns_lock()) return Result::failure(Error::Busy);

    ConnectionState current = impl_->state.load(std::memory_order_acquire);
    if (current == ConnectionState::Disconnected) return Result::failure(Error::InvalidState);
    if (current == ConnectionState::Connecting || current == ConnectionState::Reconnecting)
        return Result::failure(Error::Busy);

    impl_->invalidateTelemetry();
    impl_->protocol.disconnect();
    impl_->state.store(ConnectionState::Disconnected, std::memory_order_release);
    return Result::success();
}

Result Device::recoverTelemetryStream()
{
    std::unique_lock<std::mutex> lifecycleLock(impl_->lifecycleMutex, std::try_to_lock);
    if (!lifecycleLock.owns_lock()) return Result::failure(Error::Busy);

    Result ready = impl_->requireConnected();
    if (!ready) return ready;
    return impl_->finishCommand(impl_->protocol.recoverStream());
}

ConnectionState Device::connectionState() const
{
    return impl_->state.load(std::memory_order_acquire);
}

bool Device::isConnected() const
{
    return connectionState() == ConnectionState::Connected;
}

bool Device::hasCommunicationFailure() const
{
    return connectionState() == ConnectionState::CommunicationFailure;
}

Device::TelemetryCallbackHandle Device::registerTelemetryCallback(TelemetryCallback callback)
{
    return impl_->protocol.registerTelemetryCallback(std::move(callback));
}

bool Device::removeTelemetryCallback(TelemetryCallbackHandle handle)
{
    return impl_->protocol.removeTelemetryCallback(handle);
}

Device::ConnectionCallbackHandle Device::registerConnectionCallback(ConnectionCallback callback)
{
    return impl_->protocol.registerConnectionCallback(std::move(callback));
}

bool Device::removeConnectionCallback(ConnectionCallbackHandle handle)
{
    return impl_->protocol.removeConnectionCallback(handle);
}

void Device::setInstrumentationLevel(instrumentation::Level level)
{
    impl_->protocol.setInstrumentationLevel(level);
}

instrumentation::Level Device::instrumentationLevel() const
{
    return impl_->protocol.instrumentationLevel();
}

bool Device::instrumentationEnabled(instrumentation::Level level) const
{
    return impl_->protocol.instrumentationEnabled(level);
}

Device::InstrumentationListenerHandle Device::registerInstrumentationListener(InstrumentationListener listener)
{
    return impl_->protocol.registerInstrumentationListener(std::move(listener));
}

bool Device::removeInstrumentationListener(InstrumentationListenerHandle handle)
{
    return impl_->protocol.removeInstrumentationListener(handle);
}

void Device::setCancelFunction(std::function<bool()> callback)
{
    std::lock_guard<std::mutex> lock(impl_->cancelMutex);
    impl_->cancelFunction = std::move(callback);
}

Result Device::setDcOutput(std::size_t channel, bool enabled)
{
    if (channel >= kDcOutputCount) return Result::failure(Error::InvalidChannel);
    std::unique_lock<std::mutex> lifecycleLock(impl_->lifecycleMutex, std::try_to_lock);
    if (!lifecycleLock.owns_lock()) return Result::failure(Error::Busy);
    Result ready = impl_->requireConnected();
    if (!ready) return ready;
    return impl_->finishCommand(impl_->protocol.setOutput(static_cast<unsigned>(kDcFirmwareBase + channel), enabled));
}

ValueResult<bool> Device::dcOutputState(std::size_t channel) const
{
    if (channel >= kDcOutputCount) return ValueResult<bool>::failure(Error::InvalidChannel);
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<bool>::failure(Error::ValueUnavailable);
    return ValueResult<bool>(snapshot->dcOutputs[channel]);
}

Result Device::setUsbOutput(std::size_t channel, bool enabled)
{
    if (channel >= kUsbOutputCount) return Result::failure(Error::InvalidChannel);
    std::unique_lock<std::mutex> lifecycleLock(impl_->lifecycleMutex, std::try_to_lock);
    if (!lifecycleLock.owns_lock()) return Result::failure(Error::Busy);
    Result ready = impl_->requireConnected();
    if (!ready) return ready;
    return impl_->finishCommand(impl_->protocol.setOutput(static_cast<unsigned>(kUsbFirmwareBase + channel), enabled));
}

ValueResult<bool> Device::usbOutputState(std::size_t channel) const
{
    if (channel >= kUsbOutputCount) return ValueResult<bool>::failure(Error::InvalidChannel);
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<bool>::failure(Error::ValueUnavailable);
    return ValueResult<bool>(snapshot->usbOutputs[channel]);
}

Result Device::setHeaterEnabled(std::size_t channel, bool enabled)
{
    if (channel >= kHeaterCount) return Result::failure(Error::InvalidChannel);
    std::unique_lock<std::mutex> lifecycleLock(impl_->lifecycleMutex, std::try_to_lock);
    if (!lifecycleLock.owns_lock()) return Result::failure(Error::Busy);
    Result ready = impl_->requireConnected();
    if (!ready) return ready;
    return impl_->finishCommand(impl_->protocol.setDewEnabled(static_cast<unsigned>(kHeaterFirmwareBase + channel), enabled));
}

ValueResult<bool> Device::heaterEnabled(std::size_t channel) const
{
    if (channel >= kHeaterCount) return ValueResult<bool>::failure(Error::InvalidChannel);
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<bool>::failure(Error::ValueUnavailable);
    return ValueResult<bool>(snapshot->heaterEnabled[channel]);
}

Result Device::setHeaterMode(std::size_t channel, HeaterMode mode)
{
    if (channel >= kHeaterCount) return Result::failure(Error::InvalidChannel);
    DewMode protocolMode = DewMode::Automatic;
    if (!toProtocolMode(mode, protocolMode)) return Result::failure(Error::InvalidValue);
    std::unique_lock<std::mutex> lifecycleLock(impl_->lifecycleMutex, std::try_to_lock);
    if (!lifecycleLock.owns_lock()) return Result::failure(Error::Busy);
    Result ready = impl_->requireConnected();
    if (!ready) return ready;
    return impl_->finishCommand(impl_->protocol.setDewMode(static_cast<unsigned>(kHeaterFirmwareBase + channel), protocolMode));
}

ValueResult<HeaterMode> Device::heaterMode(std::size_t channel) const
{
    if (channel >= kHeaterCount) return ValueResult<HeaterMode>::failure(Error::InvalidChannel);
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<HeaterMode>::failure(Error::ValueUnavailable);
    return ValueResult<HeaterMode>(snapshot->heaterModes[channel]);
}

Result Device::setHeaterManualPower(std::size_t channel, unsigned percent)
{
    if (channel >= kHeaterCount) return Result::failure(Error::InvalidChannel);
    if (percent > 100) return Result::failure(Error::InvalidValue);
    std::unique_lock<std::mutex> lifecycleLock(impl_->lifecycleMutex, std::try_to_lock);
    if (!lifecycleLock.owns_lock()) return Result::failure(Error::Busy);
    Result ready = impl_->requireConnected();
    if (!ready) return ready;

    Result result = impl_->finishCommand(impl_->protocol.setDewManualPercent(static_cast<unsigned>
        (kHeaterFirmwareBase + channel), percent));
    if (result)
    {
        std::lock_guard<std::mutex> lock(impl_->stateMutex);
        impl_->heaterManualPower[channel] = percent;
        impl_->haveHeaterManualPower[channel] = true;
    }
    return result;
}

ValueResult<unsigned> Device::heaterManualPower(std::size_t channel) const
{
    if (channel >= kHeaterCount) return ValueResult<unsigned>::failure(Error::InvalidChannel);
    std::lock_guard<std::mutex> lock(impl_->stateMutex);
    if (!impl_->haveHeaterManualPower[channel]) return ValueResult<unsigned>::failure(Error::ValueUnavailable);
    return ValueResult<unsigned>(impl_->heaterManualPower[channel]);
}

ValueResult<double> Device::heaterCurrentOutput(std::size_t channel) const
{
    if (channel >= kHeaterCount) return ValueResult<double>::failure(Error::InvalidChannel);
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<double>::failure(Error::ValueUnavailable);
    return ValueResult<double>(snapshot->heaterOutputPercent[channel]);
}

ValueResult<double> Device::deviceSurfaceTemperature() const
{
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<double>::failure(Error::ValueUnavailable);
    return ValueResult<double>(snapshot->deviceSurfaceTemperatureC);
}

ValueResult<double> Device::airTemperature() const
{
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<double>::failure(Error::ValueUnavailable);
    return ValueResult<double>(snapshot->airTemperatureC);
}

ValueResult<double> Device::humidity() const
{
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<double>::failure(Error::ValueUnavailable);
    return ValueResult<double>(snapshot->humidityPercent);
}

ValueResult<double> Device::dewPoint() const
{
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<double>::failure(Error::ValueUnavailable);
    return ValueResult<double>(snapshot->dewPointC);
}

ValueResult<double> Device::inputVoltage() const
{
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<double>::failure(Error::ValueUnavailable);
    return ValueResult<double>(snapshot->inputVoltageV);
}

ValueResult<double> Device::inputCurrent() const
{
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<double>::failure(Error::ValueUnavailable);
    return ValueResult<double>(snapshot->outputCurrentA);
}

ValueResult<double> Device::inputPower() const
{
    auto snapshot = impl_->snapshot();
    if (!snapshot) return ValueResult<double>::failure(Error::ValueUnavailable);
    return ValueResult<double>(snapshot->outputPowerW);
}

ValueResult<std::string> Device::firmwareVersion() const
{
    std::string raw;
    int numeric = 0;
    if (!impl_->protocol.firmwareVersion(raw, numeric)) return ValueResult<std::string>::failure(Error::ValueUnavailable);
    return ValueResult<std::string>(raw);
}

#ifdef GEMINI_TESTING
void Device::injectStatusForTesting(const StatusFrame &status)
{
    impl_->protocol.injectStatusForTesting(status);
}

void Device::emitConnectionEventForTesting(ConnectionEvent event)
{
    impl_->protocol.emitConnectionEventForTesting(event);
}

void Device::setConnectedForTesting(bool connected)
{
    impl_->state.store(connected ? ConnectionState::Connected : ConnectionState::Disconnected, std::memory_order_release);
    impl_->protocol.acceptingWrites_.store(connected, std::memory_order_release);
}

std::size_t Device::queuedCommandCountForTesting() const
{
    return impl_->protocol.queuedCommandCountForTesting();
}
#endif

} // namespace geminipbh
