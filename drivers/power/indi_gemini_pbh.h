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

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <defaultdevice.h>
#include <indipowerinterface.h>
#include <indipropertynumber.h>
#include <indipropertyswitch.h>
#include <indipropertytext.h>

#include "indi-gemini-pbh/connection_event.h"
#include "indi-gemini-pbh/device.h"
#include "indi-gemini-pbh/discovery.h"
#include "indi-gemini-pbh/telemetry_snapshot.h"

#include "driver_support.h"

namespace Connection
{
class Serial;
}

class GeminiPBH : public INDI::DefaultDevice, public INDI::PowerInterface
{
    public:
        GeminiPBH();
        ~GeminiPBH() override;

        bool initProperties() override;
        bool updateProperties() override;
        bool Connect() override;
        bool Disconnect() override;
        void TimerHit() override;
        void debugTriggered(bool enable) override;

        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    protected:
        const char *getDefaultName() override;
        bool saveConfigItems(FILE *fp) override;

        bool SetPowerPort(size_t port, bool enabled) override;
        bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
        bool SetUSBPort(size_t port, bool enabled) override;

#ifdef GEMINI_TESTING
    public:
        void simulateConnectedTelemetryForTesting(const geminipbh::TelemetrySnapshot &snapshot);
        void checkTelemetryFreshnessForTesting(std::chrono::steady_clock::time_point now);
        bool communicationFailureForTesting() const;
        IPState powerSensorsStateForTesting() const;
        IPState environmentStateForTesting() const;
        IPState powerChannelsStateForTesting() const;
        IPState dewModeStateForTesting(size_t index) const;
        void simulateHeaterModePendingForTesting(size_t heaterIndex, geminipbh::HeaterMode targetMode,
                std::chrono::steady_clock::time_point deadline);
        void checkPendingOperationsForTesting(const geminipbh::TelemetrySnapshot &snapshot);
        geminipbh::instrumentation::Level instrumentationLevelForTesting() const;
        void syncInstrumentationLevelForTesting();
        void setDebugForTesting(bool enable);
        void setSerialPortForTesting(const std::string &port);
        std::string serialPortForTesting() const;
        void setAutoSearchForTesting(bool enabled);
        void setDiscoveryFunctionForTesting(std::function<geminipbh::DiscoveryResult()> fn);
        bool detectedPortSaveRequestedForTesting() const;
#endif

    private:
        enum class DriverState
        {
            Disconnected,
            Connecting,
            Connected,
            CommunicationFailure,
            Reconnecting
        };

        void initializePowerInterfaceProperties();
        void initializeCustomProperties();
        void defineConnectedProperties();
        void deleteConnectedProperties();
        void refreshLabelsFromTextProperties();
        bool finishSuccessfulConnection(bool detectedPort);

        bool autoSearchEnabled() const;
        bool updateSerialPortSelection(const std::string &port);
        bool discoveryCancellationRequested() const;
        geminipbh::DiscoveryResult runDiscovery();
        std::vector<geminipbh::ProbeResult> supportedDiscoveryDevices(const geminipbh::DiscoveryResult &discovery) const;
        bool isSupportedDiscoveryDevice(const geminipbh::ProbeResult &device, std::string &reason) const;

        bool handlePowerSwitchChange(INDI::PropertySwitch &property, ISState *states, char *names[], int n,
                                     geminipbh::indi_driver::PendingOperationKind kind);
        bool handleDewModeChange(size_t heaterIndex, INDI::PropertySwitch &property, ISState *states,
                                 char *names[], int n);
        bool handleDewDutyCycleChange(double values[], char *names[], int n);

        void registerDeviceCallbacks();
        void removeDeviceCallbacks();
        void handleTelemetry(const geminipbh::TelemetrySnapshot &snapshot);
        void handleConnectionEvent(geminipbh::ConnectionEvent event);
        void handleInstrumentation(const geminipbh::instrumentation::Event &event);
        void syncInstrumentationLevelWithIndiDebug();
        void flushConnectionEvents();
        void processConnectionEvent(geminipbh::ConnectionEvent event);
        void flushTelemetryToINDI();
        void refreshPropertiesFromDevice();
        void checkTelemetryFreshness();
        void checkTelemetryFreshness(std::chrono::steady_clock::time_point now);
        void checkPendingOperations();
        void checkPendingOperations(const geminipbh::TelemetrySnapshot *snapshot);
        void attemptReconnectIfDue();
        void markCommunicationFailure(const std::string &reason);
        void markHardwareProperties(IPState state);
        void failPendingOperations(IPState state);
        void scheduleNextTimer();

        bool requestDcOutputChange(size_t port, bool enabled);
        bool requestUsbOutputChange(size_t port, bool enabled);
        bool requestHeaterEnabledChange(size_t port, bool enabled);
        bool requestHeaterModeChange(size_t port, geminipbh::HeaterMode mode);
        bool requestHeaterManualPowerChange(size_t port, double percent);
        geminipbh::Result requireWriteReady() const;
        void addPendingOperation(geminipbh::indi_driver::PendingOperation operation);
        bool hasPendingOperation(geminipbh::indi_driver::PendingOperationKind kind) const;
        bool hasPendingOperation(geminipbh::indi_driver::PendingOperationKind kind, size_t index) const;
        void applyPendingAwareLiveStates();
        void logDeviceError(const char *action, geminipbh::Error error) const;

        static const char *heaterModeName(geminipbh::HeaterMode mode);

        INDI::PropertyText DeviceInfoTP {1};
        INDI::PropertyNumber EnvironmentNP {4};
        INDI::PropertySwitch Dew1ModeSP {3};
        INDI::PropertySwitch Dew2ModeSP {3};
        INDI::PropertyNumber DewOutputsNP {2};

        std::unique_ptr<geminipbh::Device> device_;
        geminipbh::Device::TelemetryCallbackHandle telemetryCallbackHandle_ = 0;
        geminipbh::Device::ConnectionCallbackHandle connectionCallbackHandle_ = 0;
        geminipbh::Device::InstrumentationListenerHandle instrumentationListenerHandle_ = 0;
        std::unique_ptr<Connection::Serial> serialConnection_;

#ifdef GEMINI_TESTING
        std::optional<bool> autoSearchOverrideForTesting_;
        std::function<geminipbh::DiscoveryResult()> discoveryFunctionForTesting_;
        bool detectedPortSaveRequestedForTesting_ = false;
#endif

        geminipbh::indi_driver::CallbackStaging callbackStaging_;
        std::atomic<bool> protocolInstrumentationForwarding_{false};
        bool haveTelemetry_ = false;
        std::atomic<bool> shutdownInProgress_{false};

        DriverState driverState_ = DriverState::Disconnected;
        bool userDisconnectRequested_ = true;
        bool connectedPropertiesDefined_ = false;

        std::array<unsigned, 2> preservedManualPower_{{0, 0}};
        std::vector<geminipbh::indi_driver::PendingOperation> pendingOperations_;
        std::chrono::steady_clock::time_point lastReconnectAttempt_{};
        std::chrono::steady_clock::time_point lastTelemetryTime_{};
};
