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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "indi-gemini-pbh/connection_event.h"
#include "indi-gemini-pbh/instrumentation.h"
#include "indi-gemini-pbh/result.h"
#include "indi-gemini-pbh/telemetry_snapshot.h"

namespace geminipbh
{

struct StatusFrame;

enum class ConnectionState
{
    Disconnected,
    Connecting,
    Connected,
    CommunicationFailure,
    Reconnecting
};

class Device
{
    public:
        using TelemetryCallback = std::function<void(const TelemetrySnapshot &)>;
        using TelemetryCallbackHandle = std::uint64_t;
        using ConnectionCallback = std::function<void(ConnectionEvent)>;
        using ConnectionCallbackHandle = std::uint64_t;
        using InstrumentationListener = instrumentation::Dispatcher::Listener;
        using InstrumentationListenerHandle = instrumentation::Dispatcher::ListenerHandle;

        Device();
        ~Device();

        Device(const Device &) = delete;
        Device &operator=(const Device &) = delete;
        Device(Device &&) = delete;
        Device &operator=(Device &&) = delete;

        Result connect(const std::string &port, int settleMs = 0);
        Result reconnect();
        Result disconnect();
        Result recoverTelemetryStream();

        ConnectionState connectionState() const;
        bool isConnected() const;
        bool hasCommunicationFailure() const;

        TelemetryCallbackHandle registerTelemetryCallback(TelemetryCallback callback);
        bool removeTelemetryCallback(TelemetryCallbackHandle handle);
        ConnectionCallbackHandle registerConnectionCallback(ConnectionCallback callback);
        bool removeConnectionCallback(ConnectionCallbackHandle handle);

        void setInstrumentationLevel(instrumentation::Level level);
        instrumentation::Level instrumentationLevel() const;
        bool instrumentationEnabled(instrumentation::Level level) const;
        InstrumentationListenerHandle registerInstrumentationListener(InstrumentationListener listener);
        bool removeInstrumentationListener(InstrumentationListenerHandle handle);

        void setCancelFunction(std::function<bool()> callback);

        Result setDcOutput(std::size_t channel, bool enabled);
        ValueResult<bool> dcOutputState(std::size_t channel) const;

        Result setUsbOutput(std::size_t channel, bool enabled);
        ValueResult<bool> usbOutputState(std::size_t channel) const;

        Result setHeaterEnabled(std::size_t channel, bool enabled);
        ValueResult<bool> heaterEnabled(std::size_t channel) const;

        Result setHeaterMode(std::size_t channel, HeaterMode mode);
        ValueResult<HeaterMode> heaterMode(std::size_t channel) const;

        Result setHeaterManualPower(std::size_t channel, unsigned percent);
        ValueResult<unsigned> heaterManualPower(std::size_t channel) const;
        ValueResult<double> heaterCurrentOutput(std::size_t channel) const;

        ValueResult<double> deviceSurfaceTemperature() const;
        ValueResult<double> airTemperature() const;
        ValueResult<double> humidity() const;
        ValueResult<double> dewPoint() const;

        ValueResult<double> inputVoltage() const;
        ValueResult<double> inputCurrent() const;
        ValueResult<double> inputPower() const;

        ValueResult<std::string> firmwareVersion() const;

#ifdef GEMINI_TESTING
        void injectStatusForTesting(const StatusFrame &status);
        void emitConnectionEventForTesting(ConnectionEvent event);
        void setConnectedForTesting(bool connected);
        std::size_t queuedCommandCountForTesting() const;
#endif

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
};

} // namespace geminipbh
