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

#include "indi_gemini_pbh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <utility>

#include <basedevice.h>
#include <connectionplugins/connectionserial.h>
#include <indicom.h>
#include <indistandardproperty.h>
#include <indilogger.h>

#include "indi-gemini-pbh/version.h"

namespace
{
constexpr const char *kDefaultDeviceName = "Gemini Power Box Hub Advanced v3";
constexpr const char *kEnvironmentTab = "Environment";
constexpr uint32_t kHousekeepingPeriodMs = 1000;
constexpr auto kTelemetryStaleTimeout = std::chrono::seconds(10);
constexpr auto kReconnectInterval = std::chrono::seconds(5);
constexpr auto kOperationConfirmationTimeout = std::chrono::seconds(8);
constexpr int kMinimumSupportedFirmware = 308;
constexpr const char *kSupportedIdentity = "GeminiPowerBoxPlusAdv3";
constexpr size_t kDcOutputCount = 4;
constexpr size_t kDewOutputCount = 2;
constexpr size_t kUsbOutputCount = 6;
constexpr const char *kDcLabels[kDcOutputCount] = {"DC 2", "DC 3", "DC 4", "DC 5"};
constexpr const char *kDewLabels[kDewOutputCount] = {"DEW 6", "DEW 7"};
constexpr const char *kUsbLabels[kUsbOutputCount] = {"USB A", "USB B", "USB C", "USB D", "USB E", "USB F"};

bool loggerSwitchEnabled(ISwitch switches[], unsigned int count, const char *name)
{
    for (unsigned int i = 0; i < count; ++i)
    {
        if (std::strcmp(switches[i].name, name) == 0)
            return switches[i].s == ISS_ON;
    }
    return false;
}

bool indiLevelEnabled(const char *debugLevelName, const char *loggingLevelName)
{
    const unsigned int count = INDI::Logger::customLevel;
    return loggerSwitchEnabled(INDI::Logger::DebugLevelS, count, debugLevelName) ||
           loggerSwitchEnabled(INDI::Logger::LoggingLevelS, count, loggingLevelName);
}

#ifndef GEMINI_TESTING
std::unique_ptr<GeminiPBH> driver(new GeminiPBH());
#endif
}

namespace gindi = geminipbh::indi_driver;

GeminiPBH::GeminiPBH() : INDI::PowerInterface(this)
{
    device_.reset(new geminipbh::Device());
    device_->setInstrumentationLevel(geminipbh::instrumentation::Level::Off);
    registerDeviceCallbacks();

    setVersion(GEMINIPBH_VERSION_MAJOR, GEMINIPBH_VERSION_MINOR);
    setDriverInterface(INDI::BaseDevice::AUX_INTERFACE | INDI::BaseDevice::POWER_INTERFACE);

    SetCapability(POWER_HAS_DC_OUT |
                  POWER_HAS_DEW_OUT |
                  POWER_HAS_VOLTAGE_SENSOR |
                  POWER_HAS_OVERALL_CURRENT |
                  POWER_HAS_USB_TOGGLE);

}

GeminiPBH::~GeminiPBH()
{
    shutdownInProgress_.store(true, std::memory_order_release);
    userDisconnectRequested_ = true;
    callbackStaging_.beginShutdown();
    removeDeviceCallbacks();

    if (device_)
    {
        device_->disconnect();
    }

    pendingOperations_.clear();
    callbackStaging_.clear();
}

void GeminiPBH::registerDeviceCallbacks()
{
    if (!device_)
        return;

    telemetryCallbackHandle_ = device_->registerTelemetryCallback(
                                   [this](const geminipbh::TelemetrySnapshot & snapshot)
    {
        handleTelemetry(snapshot);
    });
    connectionCallbackHandle_ = device_->registerConnectionCallback(
                                    [this](geminipbh::ConnectionEvent event)
    {
        handleConnectionEvent(event);
    });
    instrumentationListenerHandle_ = device_->registerInstrumentationListener(
                                         [this](const geminipbh::instrumentation::Event & event)
    {
        handleInstrumentation(event);
    });
}

void GeminiPBH::removeDeviceCallbacks()
{
    if (!device_)
        return;

    if (telemetryCallbackHandle_ != 0)
    {
        device_->removeTelemetryCallback(telemetryCallbackHandle_);
        telemetryCallbackHandle_ = 0;
    }
    if (connectionCallbackHandle_ != 0)
    {
        device_->removeConnectionCallback(connectionCallbackHandle_);
        connectionCallbackHandle_ = 0;
    }
    if (instrumentationListenerHandle_ != 0)
    {
        device_->removeInstrumentationListener(instrumentationListenerHandle_);
        instrumentationListenerHandle_ = 0;
    }
}

const char *GeminiPBH::getDefaultName()
{
    return kDefaultDeviceName;
}

bool GeminiPBH::initProperties()
{
    INDI::DefaultDevice::initProperties();

    if (!serialConnection_)
    {
        serialConnection_.reset(new Connection::Serial(this));
        serialConnection_->setDefaultBaudRate(Connection::Serial::B_19200);
        registerConnection(serialConnection_.get());
        setActiveConnection(serialConnection_.get());
    }

    addDebugControl();
    setDefaultPollingPeriod(kHousekeepingPeriodMs);

    initializePowerInterfaceProperties();
    initializeCustomProperties();

    registerProperty(PowerChannelLabelsTP);
    registerProperty(DewChannelLabelsTP);
    registerProperty(USBPortLabelsTP);

    return true;
}

void GeminiPBH::initializePowerInterfaceProperties()
{
    INDI::PowerInterface::initProperties(MAIN_CONTROL_TAB, kDcOutputCount, kDewOutputCount, 0, 0, kUsbOutputCount);

    for (size_t i = 0; i < kDcOutputCount; ++i)
    {
        PowerChannelLabelsTP[i].setLabel(kDcLabels[i]);
        if (std::strncmp(PowerChannelLabelsTP[i].getText(), "Channel ", 8) == 0)
            PowerChannelLabelsTP[i].setText(kDcLabels[i]);
        PowerChannelsSP[i].setLabel(PowerChannelLabelsTP[i].getText());
    }

    for (size_t i = 0; i < kDewOutputCount; ++i)
    {
        DewChannelLabelsTP[i].setLabel(kDewLabels[i]);
        if (std::strncmp(DewChannelLabelsTP[i].getText(), "Channel ", 8) == 0)
            DewChannelLabelsTP[i].setText(kDewLabels[i]);
        DewChannelsSP[i].setLabel(DewChannelLabelsTP[i].getText());
        DewChannelDutyCycleNP[i].setLabel((std::string(DewChannelLabelsTP[i].getText()) + " (%)").c_str());
    }

    for (size_t i = 0; i < kUsbOutputCount; ++i)
    {
        USBPortLabelsTP[i].setLabel(kUsbLabels[i]);
        if (std::strncmp(USBPortLabelsTP[i].getText(), "Port ", 5) == 0)
            USBPortLabelsTP[i].setText(kUsbLabels[i]);
        USBPortSP[i].setLabel(USBPortLabelsTP[i].getText());
    }
}

void GeminiPBH::initializeCustomProperties()
{
    DeviceInfoTP[0].fill("FIRMWARE_VERSION", "Firmware Version", "Unavailable");
    DeviceInfoTP.fill(getDeviceName(), "DEVICE_INFO", "Device Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    EnvironmentNP[0].fill("DEVICE_SURFACE_TEMPERATURE", "Device Surface Temperature (C)", "%.1f", -50, 100, 0, 0);
    EnvironmentNP[1].fill("AIR_TEMPERATURE", "Air Temperature (C)", "%.1f", -50, 100, 0, 0);
    EnvironmentNP[2].fill("HUMIDITY", "Humidity (%)", "%.1f", 0, 100, 0, 0);
    EnvironmentNP[3].fill("DEW_POINT", "Calculated Dew Point (C)", "%.1f", -50, 100, 0, 0);
    EnvironmentNP.fill(getDeviceName(), "PBH_ENVIRONMENT", "Environment", kEnvironmentTab, IP_RO, 60, IPS_IDLE);

    auto fillModeProperty = [this](INDI::PropertySwitch & property, size_t index)
    {
        property[0].fill("AUTO", "Auto", ISS_ON);
        property[1].fill("MANUAL_PWM", "Manual PWM", ISS_OFF);
        property[2].fill("SWITCH", "Switch", ISS_OFF);
        const std::string name = "DEW_" + std::to_string(index + 1) + "_MODE";
        const std::string label = std::string(kDewLabels[index]) + " Mode";
        property.fill(getDeviceName(), name.c_str(), label.c_str(), DEW_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    };
    fillModeProperty(Dew1ModeSP, 0);
    fillModeProperty(Dew2ModeSP, 1);

    DewOutputsNP[0].fill("DEW_CHANNEL_1", "DEW 6 Current Output (%)", "%.0f", 0, 100, 0, 0);
    DewOutputsNP[1].fill("DEW_CHANNEL_2", "DEW 7 Current Output (%)", "%.0f", 0, 100, 0, 0);
    DewOutputsNP.fill(getDeviceName(), "DEW_OUTPUTS", "Current Output", DEW_TAB, IP_RO, 60, IPS_IDLE);
}

bool GeminiPBH::updateProperties()
{
    if (isConnected())
        defineConnectedProperties();
    else
        deleteConnectedProperties();

    return true;
}

void GeminiPBH::defineConnectedProperties()
{
    if (connectedPropertiesDefined_)
        return;

    defineProperty(DeviceInfoTP);
    defineProperty(PowerSensorsNP);
    defineProperty(EnvironmentNP);
    defineProperty(PowerChannelsSP);
    defineProperty(DewChannelsSP);
    defineProperty(DewChannelDutyCycleNP);
    defineProperty(Dew1ModeSP);
    defineProperty(Dew2ModeSP);
    defineProperty(DewOutputsNP);
    defineProperty(USBPortSP);

    connectedPropertiesDefined_ = true;
}

void GeminiPBH::deleteConnectedProperties()
{
    if (!connectedPropertiesDefined_)
        return;

    deleteProperty(DeviceInfoTP);
    deleteProperty(PowerSensorsNP);
    deleteProperty(EnvironmentNP);
    deleteProperty(PowerChannelsSP);
    deleteProperty(DewChannelsSP);
    deleteProperty(DewChannelDutyCycleNP);
    deleteProperty(Dew1ModeSP);
    deleteProperty(Dew2ModeSP);
    deleteProperty(DewOutputsNP);
    deleteProperty(USBPortSP);

    connectedPropertiesDefined_ = false;
}

bool GeminiPBH::Connect()
{
    if (!device_)
    {
        LOG_ERROR("Gemini Power Box Hub Advanced v3 semantic device is unavailable.");
        return false;
    }

    userDisconnectRequested_ = false;
    driverState_ = DriverState::Connecting;
    pendingOperations_.clear();
    haveTelemetry_ = false;
    callbackStaging_.clear();

    const char *configuredPort = serialConnection_ ? serialConnection_->port() : "";
    const bool haveConfiguredPort = configuredPort != nullptr && *configuredPort != '\0';
    const bool canAutoSearch = autoSearchEnabled();
    if (!haveConfiguredPort && !canAutoSearch)
    {
        driverState_ = DriverState::Disconnected;
        LOG_ERROR("No serial port configured for Gemini Power Box Hub Advanced v3.");
        return false;
    }

    if (haveConfiguredPort)
    {
        LOGF_INFO("Connecting to Gemini Power Box Hub Advanced v3 on %s.", configuredPort);

        const geminipbh::Result result = device_->connect(configuredPort);
        if (result)
            return finishSuccessfulConnection(false);

        if (!canAutoSearch)
        {
            driverState_ = DriverState::Disconnected;
            logDeviceError("connect", result.error());
            deleteConnectedProperties();
            return false;
        }

        LOGF_WARN("Connection to %s failed; starting Gemini Power Box Hub Advanced v3 auto search.", configuredPort);
    }
    else
    {
        LOG_WARN("No serial port configured; starting Gemini Power Box Hub Advanced v3 auto search.");
    }

    const geminipbh::DiscoveryResult discovery = runDiscovery();
    if (discovery.cancelled)
    {
        driverState_ = DriverState::Disconnected;
        LOG_ERROR("Gemini Power Box Hub Advanced v3 auto search was cancelled.");
        deleteConnectedProperties();
        return false;
    }

    const std::vector<geminipbh::ProbeResult> supportedDevices = supportedDiscoveryDevices(discovery);
    if (supportedDevices.empty())
    {
        driverState_ = DriverState::Disconnected;
        LOGF_ERROR("Gemini Power Box Hub Advanced v3 auto search found no supported device among %zu probe(s).", discovery.probes.size());
        deleteConnectedProperties();
        return false;
    }
    if (supportedDevices.size() > 1)
    {
        driverState_ = DriverState::Disconnected;
        for (const auto &device : supportedDevices)
            LOGF_ERROR("Auto search candidate: %s firmware %s.",
                       device.preferredPort.c_str(), device.firmware.raw.c_str());
        LOG_ERROR("Multiple supported Gemini Power Box Hub Advanced v3 devices detected; select the desired serial port manually.");
        deleteConnectedProperties();
        return false;
    }

    const geminipbh::ProbeResult &detected = supportedDevices.front();
    if (!updateSerialPortSelection(detected.preferredPort))
    {
        driverState_ = DriverState::Disconnected;
        LOGF_ERROR("Auto search detected %s but failed to update DEVICE_PORT.", detected.preferredPort.c_str());
        deleteConnectedProperties();
        return false;
    }

    LOGF_INFO("Auto search selected Gemini Power Box Hub Advanced v3 on %s (identity %s, firmware %s).",
              detected.preferredPort.c_str(), detected.identity.c_str(), detected.firmware.raw.c_str());

    const geminipbh::Result detectedConnect = device_->connect(detected.preferredPort);
    if (!detectedConnect)
    {
        driverState_ = DriverState::Disconnected;
        logDeviceError("connect after auto search", detectedConnect.error());
        deleteConnectedProperties();
        return false;
    }

    return finishSuccessfulConnection(true);
}

bool GeminiPBH::finishSuccessfulConnection(bool detectedPort)
{
    driverState_ = DriverState::Connected;
    defineConnectedProperties();
    flushConnectionEvents();
    flushTelemetryToINDI();
    refreshPropertiesFromDevice();
    SetTimer(kHousekeepingPeriodMs);
    if (detectedPort)
    {
#ifdef GEMINI_TESTING
        detectedPortSaveRequestedForTesting_ = true;
#else
        saveConfig(true, INDI::SP::DEVICE_PORT);
#endif
    }
    LOG_INFO("Gemini Power Box Hub Advanced v3 connected.");
    return true;
}

bool GeminiPBH::autoSearchEnabled() const
{
#ifdef GEMINI_TESTING
    if (autoSearchOverrideForTesting_)
{
    return *autoSearchOverrideForTesting_;
}
#endif

INDI::PropertySwitch property = getSwitch(INDI::SP::DEVICE_AUTO_SEARCH);
if (!property || property.isEmpty())
    {
        return false;
    }
    return property.isSwitchOn("INDI_ENABLED");
}

bool GeminiPBH::updateSerialPortSelection(const std::string &port)
{
    if (port.empty())
        return false;

    INDI::PropertyText property = getText(INDI::SP::DEVICE_PORT);
    if (property && !property.isEmpty() && property.size() > 0)
    {
        property[0].setText(port.c_str());
        property.setState(IPS_OK);
        property.apply();
        return true;
    }

    if (!serialConnection_)
        return false;
    serialConnection_->setDefaultPort(port.c_str());
    return true;
}

bool GeminiPBH::discoveryCancellationRequested() const
{
    return shutdownInProgress_.load(std::memory_order_acquire) || userDisconnectRequested_;
}

geminipbh::DiscoveryResult GeminiPBH::runDiscovery()
{
#ifdef GEMINI_TESTING
    if (discoveryFunctionForTesting_)
        return discoveryFunctionForTesting_();
#endif

    if (serialConnection_)
        serialConnection_->Refresh(true);

    geminipbh::DiscoveryOptions options;
    options.shouldCancel = [this]
    {
        return discoveryCancellationRequested();
    };
    return geminipbh::discoverGeminiDevices(options);
}

std::vector<geminipbh::ProbeResult> GeminiPBH::supportedDiscoveryDevices(
    const geminipbh::DiscoveryResult &discovery) const
{
    std::vector<geminipbh::ProbeResult> supported;
    for (const auto &device : discovery.devices)
    {
        std::string reason;
        if (isSupportedDiscoveryDevice(device, reason))
        {
            supported.push_back(device);
        }
        else
        {
            LOGF_WARN("Ignoring discovered Gemini device on %s: %s.",
                      device.preferredPort.c_str(), reason.c_str());
        }
    }
    return supported;
}

bool GeminiPBH::isSupportedDiscoveryDevice(const geminipbh::ProbeResult &device, std::string &reason) const
{
    if (device.status != geminipbh::ProbeStatus::Identified)
    {
        reason = std::string("probe status is ") + geminipbh::probeStatusName(device.status);
        return false;
    }
    if (device.identity != kSupportedIdentity)
    {
        reason = "identity '" + device.identity + "' is not supported by this driver";
        return false;
    }
    if (!device.firmware.numeric)
    {
        reason = "firmware version is unavailable";
        return false;
    }
    if (*device.firmware.numeric < kMinimumSupportedFirmware)
    {
        reason = "firmware " + std::to_string(*device.firmware.numeric) + " is below the supported minimum " +
                 std::to_string(kMinimumSupportedFirmware);
        return false;
    }
    return true;
}

bool GeminiPBH::Disconnect()
{
    userDisconnectRequested_ = true;
    driverState_ = DriverState::Disconnected;
    failPendingOperations(IPS_IDLE);
    haveTelemetry_ = false;

    geminipbh::Result result = geminipbh::Result::success();
    if (device_)
        result = device_->disconnect();

    if (!result && result.error() != geminipbh::Error::InvalidState)
        logDeviceError("disconnect", result.error());

    callbackStaging_.clear();

    deleteConnectedProperties();
    LOG_INFO("Gemini Power Box Hub Advanced v3 disconnected.");
    return true;
}

void GeminiPBH::TimerHit()
{
    flushConnectionEvents();
    flushTelemetryToINDI();
    checkTelemetryFreshness();
    checkPendingOperations();
    attemptReconnectIfDue();
    scheduleNextTimer();
}

void GeminiPBH::scheduleNextTimer()
{
    if (!userDisconnectRequested_ && driverState_ != DriverState::Disconnected)
        SetTimer(kHousekeepingPeriodMs);
}

void GeminiPBH::debugTriggered(bool enable)
{
    INDI::DefaultDevice::debugTriggered(enable);
    syncInstrumentationLevelWithIndiDebug();
}

bool GeminiPBH::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && std::strcmp(dev, getDeviceName()) != 0)
        return false;

    if (PowerChannelsSP.isNameMatch(name))
        return handlePowerSwitchChange(PowerChannelsSP, states, names, n, gindi::PendingOperationKind::DcOutput);
    if (USBPortSP.isNameMatch(name))
        return handlePowerSwitchChange(USBPortSP, states, names, n, gindi::PendingOperationKind::UsbOutput);
    if (DewChannelsSP.isNameMatch(name))
        return handlePowerSwitchChange(DewChannelsSP, states, names, n, gindi::PendingOperationKind::HeaterEnabled);
    if (Dew1ModeSP.isNameMatch(name))
        return handleDewModeChange(0, Dew1ModeSP, states, names, n);
    if (Dew2ModeSP.isNameMatch(name))
        return handleDewModeChange(1, Dew2ModeSP, states, names, n);

    const bool affectsInstrumentationLevel = name != nullptr &&
        (std::strcmp(name, "DEBUG") == 0 ||
         std::strcmp(name, "DEBUG_LEVEL") == 0 ||
         std::strcmp(name, "LOGGING_LEVEL") == 0);
    const bool handled = INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
    if (handled && affectsInstrumentationLevel)
        syncInstrumentationLevelWithIndiDebug();
    return handled;
}

bool GeminiPBH::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && std::strcmp(dev, getDeviceName()) != 0)
        return false;

    if (DewChannelDutyCycleNP.isNameMatch(name))
        return handleDewDutyCycleChange(values, names, n);

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool GeminiPBH::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && std::strcmp(dev, getDeviceName()) != 0)
        return false;

    if (PowerChannelLabelsTP.isNameMatch(name) || DewChannelLabelsTP.isNameMatch(name) || USBPortLabelsTP.isNameMatch(name))
    {
        const bool processed = INDI::PowerInterface::processText(dev, name, texts, names, n);
        if (processed)
            refreshLabelsFromTextProperties();
        return processed;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool GeminiPBH::handlePowerSwitchChange(INDI::PropertySwitch &property, ISState *states, char *names[], int n,
                                        gindi::PendingOperationKind kind)
{
    bool accepted = true;
    for (int request = 0; request < n; ++request)
    {
        for (size_t index = 0; index < property.size(); ++index)
        {
            if (!property[index].isNameMatch(names[request]))
                continue;

            const bool enabled = states[request] == ISS_ON;
            bool rc = false;
            if (kind == gindi::PendingOperationKind::DcOutput)
                rc = requestDcOutputChange(index, enabled);
            else if (kind == gindi::PendingOperationKind::UsbOutput)
                rc = requestUsbOutputChange(index, enabled);
            else if (kind == gindi::PendingOperationKind::HeaterEnabled)
                rc = requestHeaterEnabledChange(index, enabled);

            if (!rc)
            {
                accepted = false;
            }
        }
    }

    property.setState(accepted ? IPS_BUSY : IPS_ALERT);
    property.apply();
    return true;
}

bool GeminiPBH::handleDewModeChange(size_t heaterIndex, INDI::PropertySwitch &property, ISState *states,
                                    char *names[], int n)
{
    geminipbh::HeaterMode target = geminipbh::HeaterMode::Auto;
    bool foundTarget = false;
    for (int request = 0; request < n; ++request)
    {
        if (states[request] != ISS_ON)
            continue;

        for (size_t i = 0; i < property.size(); ++i)
        {
            if (!property[i].isNameMatch(names[request]))
                continue;

            if (gindi::heaterModeFromSwitchName(names[request], target))
            {
                foundTarget = true;
                break;
            }
        }
    }

    if (!foundTarget)
    {
        property.setState(IPS_ALERT);
        property.apply();
        return true;
    }

    if (target != geminipbh::HeaterMode::ManualPwm)
        preservedManualPower_[heaterIndex] = static_cast<unsigned>(std::round(DewChannelDutyCycleNP[heaterIndex].getValue()));

    if (!requestHeaterModeChange(heaterIndex, target))
    {
        property.setState(IPS_ALERT);
        property.apply();
        return true;
    }

    property.setState(IPS_BUSY);
    property.apply();
    return true;
}

bool GeminiPBH::handleDewDutyCycleChange(double values[], char *names[], int n)
{
    bool accepted = true;
    for (int request = 0; request < n; ++request)
    {
        for (size_t index = 0; index < DewChannelDutyCycleNP.size(); ++index)
        {
            if (!DewChannelDutyCycleNP[index].isNameMatch(names[request]))
                continue;

            if (!requestHeaterManualPowerChange(index, values[request]))
            {
                accepted = false;
                continue;
            }
        }
    }

    DewChannelDutyCycleNP.setState(accepted ? IPS_OK : IPS_ALERT);
    DewChannelDutyCycleNP.apply();
    return true;
}

bool GeminiPBH::SetPowerPort(size_t port, bool enabled)
{
    return requestDcOutputChange(port, enabled);
}

bool GeminiPBH::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    if (!requestHeaterEnabledChange(port, enabled))
        return false;
    return requestHeaterManualPowerChange(port, dutyCycle);
}

bool GeminiPBH::SetUSBPort(size_t port, bool enabled)
{
    return requestUsbOutputChange(port, enabled);
}

bool GeminiPBH::requestDcOutputChange(size_t port, bool enabled)
{
    const geminipbh::Result ready = requireWriteReady();
    if (!ready)
    {
        logDeviceError("set DC output", ready.error());
        return false;
    }

    const geminipbh::Result result = device_->setDcOutput(port, enabled);
    if (!result)
    {
        logDeviceError("set DC output", result.error());
        return false;
    }

    auto target = gindi::makeBooleanTarget(gindi::PendingOperationKind::DcOutput, port, enabled);
    if (!target)
    {
        logDeviceError("create DC confirmation target", target.error());
        return false;
    }
    addPendingOperation(gindi::PendingOperation(gindi::PendingOperationKind::DcOutput,
            port,
            target.value(),
            std::chrono::steady_clock::now() + kOperationConfirmationTimeout));
    LOGF_DEBUG("DC output %zu accepted; waiting for telemetry confirmation.", port + 1);
    return true;
}

bool GeminiPBH::requestUsbOutputChange(size_t port, bool enabled)
{
    const geminipbh::Result ready = requireWriteReady();
    if (!ready)
    {
        logDeviceError("set USB output", ready.error());
        return false;
    }

    const geminipbh::Result result = device_->setUsbOutput(port, enabled);
    if (!result)
    {
        logDeviceError("set USB output", result.error());
        return false;
    }

    auto target = gindi::makeBooleanTarget(gindi::PendingOperationKind::UsbOutput, port, enabled);
    if (!target)
    {
        logDeviceError("create USB confirmation target", target.error());
        return false;
    }
    addPendingOperation(gindi::PendingOperation(gindi::PendingOperationKind::UsbOutput,
            port,
            target.value(),
            std::chrono::steady_clock::now() + kOperationConfirmationTimeout));
    LOGF_DEBUG("USB output %zu accepted; waiting for telemetry confirmation.", port + 1);
    return true;
}

bool GeminiPBH::requestHeaterEnabledChange(size_t port, bool enabled)
{
    const geminipbh::Result ready = requireWriteReady();
    if (!ready)
    {
        logDeviceError("set heater enabled state", ready.error());
        return false;
    }

    const geminipbh::Result result = device_->setHeaterEnabled(port, enabled);
    if (!result)
    {
        logDeviceError("set heater enabled state", result.error());
        return false;
    }

    auto target = gindi::makeBooleanTarget(gindi::PendingOperationKind::HeaterEnabled, port, enabled);
    if (!target)
    {
        logDeviceError("create heater-enabled confirmation target", target.error());
        return false;
    }
    addPendingOperation(gindi::PendingOperation(gindi::PendingOperationKind::HeaterEnabled,
            port,
            target.value(),
            std::chrono::steady_clock::now() + kOperationConfirmationTimeout));
    LOGF_DEBUG("Heater %zu enabled-state change accepted; waiting for telemetry confirmation.", port + 1);
    return true;
}

bool GeminiPBH::requestHeaterModeChange(size_t port, geminipbh::HeaterMode mode)
{
    const geminipbh::Result ready = requireWriteReady();
    if (!ready)
    {
        logDeviceError("set heater mode", ready.error());
        return false;
    }

    const geminipbh::Result result = device_->setHeaterMode(port, mode);
    if (!result)
    {
        logDeviceError("set heater mode", result.error());
        return false;
    }

    auto target = gindi::makeHeaterModeTarget(port, mode);
    if (!target)
    {
        logDeviceError("create heater-mode confirmation target", target.error());
        return false;
    }
    addPendingOperation(gindi::PendingOperation(gindi::PendingOperationKind::HeaterMode,
            port,
            target.value(),
            std::chrono::steady_clock::now() + kOperationConfirmationTimeout));
    LOGF_DEBUG("Heater %zu mode change to %s accepted; waiting for telemetry confirmation.",
               port + 1, heaterModeName(mode));
    return true;
}

bool GeminiPBH::requestHeaterManualPowerChange(size_t port, double percent)
{
    const geminipbh::Result ready = requireWriteReady();
    if (!ready)
    {
        logDeviceError("set heater manual power", ready.error());
        return false;
    }
    if (port >= kDewOutputCount || percent < 0.0 || percent > 100.0)
    {
        logDeviceError("set heater manual power", geminipbh::Error::InvalidValue);
        return false;
    }

    auto mode = device_->heaterMode(port);
    if (!mode)
    {
        logDeviceError("set heater manual power", mode.error());
        return false;
    }
    if (mode.value() != geminipbh::HeaterMode::ManualPwm)
    {
        LOGF_WARN("Heater %zu manual power is editable only in Manual PWM mode.", port + 1);
        return false;
    }

    const unsigned rounded = static_cast<unsigned>(std::round(percent));
    const geminipbh::Result result = device_->setHeaterManualPower(port, rounded);
    if (!result)
    {
        logDeviceError("set heater manual power", result.error());
        return false;
    }

    preservedManualPower_[port] = rounded;
    auto configured = device_->heaterManualPower(port);
    DewChannelDutyCycleNP[port].setValue(configured ? configured.value() : rounded);
    LOGF_DEBUG("Heater %zu manual power setpoint accepted as driver/library configuration; telemetry does not independently confirm it.",
               port + 1);
    return true;
}

geminipbh::Result GeminiPBH::requireWriteReady() const
{
    if (shutdownInProgress_.load(std::memory_order_acquire) || !device_)
    return geminipbh::Result::failure(geminipbh::Error::InvalidState);

    switch (driverState_)
    {
        case DriverState::Connected:
            return geminipbh::Result::success();
            case DriverState::CommunicationFailure:
                return geminipbh::Result::failure(geminipbh::Error::CommunicationFailure);
            case DriverState::Connecting:
            case DriverState::Reconnecting:
                return geminipbh::Result::failure(geminipbh::Error::Busy);
            case DriverState::Disconnected:
                return geminipbh::Result::failure(geminipbh::Error::NotConnected);
        }
    return geminipbh::Result::failure(geminipbh::Error::InvalidState);
}

void GeminiPBH::handleTelemetry(const geminipbh::TelemetrySnapshot &snapshot)
{
    if (shutdownInProgress_.load(std::memory_order_acquire))
        return;

    callbackStaging_.stageTelemetry(snapshot);
}

void GeminiPBH::handleConnectionEvent(geminipbh::ConnectionEvent event)
{
    if (shutdownInProgress_.load(std::memory_order_acquire))
        return;

    callbackStaging_.stageConnectionEvent(event);
}

void GeminiPBH::syncInstrumentationLevelWithIndiDebug()
{
    if (!device_)
        return;

    namespace instr = geminipbh::instrumentation;

    instr::Level level = instr::Level::Off;
    if (isDebug())
    {
        if (indiLevelEnabled("DBG_DEBUG", "LOG_DEBUG"))
            level = instr::Level::Verbose;
        else if (indiLevelEnabled("DBG_SESSION", "LOG_SESSION"))
            level = instr::Level::Info;
        else if (indiLevelEnabled("DBG_ERROR", "LOG_ERROR"))
            level = instr::Level::Error;
    }

    protocolInstrumentationForwarding_.store(level != instr::Level::Off, std::memory_order_release);
    device_->setInstrumentationLevel(level);
}

void GeminiPBH::handleInstrumentation(const geminipbh::instrumentation::Event &event)
{
    if (shutdownInProgress_.load(std::memory_order_acquire))
        return;
    if (!protocolInstrumentationForwarding_.load(std::memory_order_acquire))
        return;
    if (event.level == geminipbh::instrumentation::Level::Trace)
        return;
    if (gindi::instrumentationDuplicatesSemanticDriverLog(event))
        return;

    const std::string message = geminipbh::instrumentation::EventFormatter().format(event);
    if (event.type == geminipbh::instrumentation::EventType::Warning)
        LOGF_WARN("libgeminipbh: %s", message.c_str());
    else if (event.level == geminipbh::instrumentation::Level::Error)
        LOGF_ERROR("libgeminipbh: %s", message.c_str());
    else if (event.level == geminipbh::instrumentation::Level::Info)
        LOGF_INFO("libgeminipbh: %s", message.c_str());
    else
        LOGF_DEBUG("libgeminipbh: %s", message.c_str());
}

void GeminiPBH::flushConnectionEvents()
{
    std::deque<geminipbh::ConnectionEvent> events = callbackStaging_.takeConnectionEvents();

    for (geminipbh::ConnectionEvent event : events)
        processConnectionEvent(event);
}

void GeminiPBH::processConnectionEvent(geminipbh::ConnectionEvent event)
{
    switch (event)
    {
        case geminipbh::ConnectionEvent::Connected:
            if (userDisconnectRequested_ || shutdownInProgress_.load(std::memory_order_acquire))
                return;
            driverState_ = DriverState::Connected;
            defineConnectedProperties();
            refreshPropertiesFromDevice();
            LOG_INFO("Gemini Power Box Hub Advanced v3 semantic connection established.");
            break;

        case geminipbh::ConnectionEvent::CommunicationFailure:
            if (userDisconnectRequested_ || shutdownInProgress_.load(std::memory_order_acquire))
                return;
            markCommunicationFailure("library reported communication failure");
            break;

        case geminipbh::ConnectionEvent::ReconnectSuccess:
            if (userDisconnectRequested_ || shutdownInProgress_.load(std::memory_order_acquire))
                return;
            driverState_ = DriverState::Connected;
            defineConnectedProperties();
            refreshPropertiesFromDevice();
            LOG_INFO("Gemini Power Box Hub Advanced v3 reconnect succeeded.");
            break;

        case geminipbh::ConnectionEvent::Disconnected:
            if (!userDisconnectRequested_ && !shutdownInProgress_.load(std::memory_order_acquire))
                markCommunicationFailure("library reported disconnect");
            break;
    }
}

void GeminiPBH::flushTelemetryToINDI()
{
    auto snapshot = callbackStaging_.takeTelemetry();

    if (!snapshot || driverState_ != DriverState::Connected)
        return;

    haveTelemetry_ = true;
    lastTelemetryTime_ = snapshot->timestamp;

    for (size_t i = 0; i < kDcOutputCount; ++i)
        PowerChannelsSP[i].setState(snapshot->dcOutputs[i] ? ISS_ON : ISS_OFF);

    for (size_t i = 0; i < kUsbOutputCount; ++i)
        USBPortSP[i].setState(snapshot->usbOutputs[i] ? ISS_ON : ISS_OFF);

    for (size_t i = 0; i < kDewOutputCount; ++i)
    {
        DewChannelsSP[i].setState(snapshot->heaterEnabled[i] ? ISS_ON : ISS_OFF);
        DewChannelDutyCycleNP[i].setValue(preservedManualPower_[i]);
        DewOutputsNP[i].setValue(snapshot->heaterOutputPercent[i]);

        INDI::PropertySwitch &modeProperty = (i == 0) ? Dew1ModeSP : Dew2ModeSP;
        const size_t modeIndex = gindi::heaterModeSwitchIndex(snapshot->heaterModes[i]);
        for (size_t mode = 0; mode < modeProperty.size(); ++mode)
            modeProperty[mode].setState(mode == modeIndex ? ISS_ON : ISS_OFF);
    }
    DewOutputsNP.setState(IPS_OK);

    PowerSensorsNP[SENSOR_VOLTAGE].setValue(snapshot->inputVoltageV);
    PowerSensorsNP[SENSOR_CURRENT].setValue(snapshot->outputCurrentA);
    PowerSensorsNP[SENSOR_POWER].setValue(snapshot->outputPowerW);
    PowerSensorsNP.setState(IPS_OK);

    EnvironmentNP[0].setValue(snapshot->deviceSurfaceTemperatureC);
    EnvironmentNP[1].setValue(snapshot->airTemperatureC);
    EnvironmentNP[2].setValue(snapshot->humidityPercent);
    EnvironmentNP[3].setValue(snapshot->dewPointC);
    EnvironmentNP.setState(IPS_OK);

    auto firmware = device_ ? device_->firmwareVersion() : geminipbh::ValueResult<std::string>::failure(
                        geminipbh::Error::ValueUnavailable);
    if (firmware)
        DeviceInfoTP[0].setText(firmware.value().c_str());
    DeviceInfoTP.setState(firmware ? IPS_OK : IPS_ALERT);

    PowerChannelsSP.setState(IPS_OK);
    USBPortSP.setState(IPS_OK);
    DewChannelsSP.setState(IPS_OK);
    DewChannelDutyCycleNP.setState(IPS_OK);
    Dew1ModeSP.setState(IPS_OK);
    Dew2ModeSP.setState(IPS_OK);

    checkPendingOperations(&*snapshot);
    applyPendingAwareLiveStates();

    PowerChannelsSP.apply();
    USBPortSP.apply();
    DewChannelsSP.apply();
    DewChannelDutyCycleNP.apply();
    Dew1ModeSP.apply();
    Dew2ModeSP.apply();
    DewOutputsNP.apply();
    PowerSensorsNP.apply();
    EnvironmentNP.apply();
    DeviceInfoTP.apply();
}

void GeminiPBH::refreshPropertiesFromDevice()
{
    if (!device_ || !connectedPropertiesDefined_)
        return;

    auto firmware = device_->firmwareVersion();
    if (firmware)
        DeviceInfoTP[0].setText(firmware.value().c_str());
    DeviceInfoTP.setState(firmware ? IPS_OK : IPS_ALERT);

    for (size_t i = 0; i < kDcOutputCount; ++i)
    {
        auto value = device_->dcOutputState(i);
        if (value) PowerChannelsSP[i].setState(value.value() ? ISS_ON : ISS_OFF);
    }
    for (size_t i = 0; i < kUsbOutputCount; ++i)
    {
        auto value = device_->usbOutputState(i);
        if (value) USBPortSP[i].setState(value.value() ? ISS_ON : ISS_OFF);
    }
    for (size_t i = 0; i < kDewOutputCount; ++i)
    {
        auto enabled = device_->heaterEnabled(i);
        if (enabled) DewChannelsSP[i].setState(enabled.value() ? ISS_ON : ISS_OFF);

        auto mode = device_->heaterMode(i);
        if (mode)
        {
            INDI::PropertySwitch &modeProperty = (i == 0) ? Dew1ModeSP : Dew2ModeSP;
            const size_t modeIndex = gindi::heaterModeSwitchIndex(mode.value());
            for (size_t j = 0; j < modeProperty.size(); ++j)
                modeProperty[j].setState(j == modeIndex ? ISS_ON : ISS_OFF);
        }

        auto manual = device_->heaterManualPower(i);
        if (manual)
            preservedManualPower_[i] = manual.value();
        DewChannelDutyCycleNP[i].setValue(preservedManualPower_[i]);

        auto output = device_->heaterCurrentOutput(i);
        if (output) DewOutputsNP[i].setValue(output.value());
    }

    auto voltage = device_->inputVoltage();
    auto current = device_->inputCurrent();
    auto power = device_->inputPower();
    if (voltage) PowerSensorsNP[SENSOR_VOLTAGE].setValue(voltage.value());
    if (current) PowerSensorsNP[SENSOR_CURRENT].setValue(current.value());
    if (power) PowerSensorsNP[SENSOR_POWER].setValue(power.value());
    PowerSensorsNP.setState(voltage && current && power ? IPS_OK : IPS_ALERT);

    auto surface = device_->deviceSurfaceTemperature();
    auto air = device_->airTemperature();
    auto humidity = device_->humidity();
    auto dewPoint = device_->dewPoint();
    if (surface) EnvironmentNP[0].setValue(surface.value());
    if (air) EnvironmentNP[1].setValue(air.value());
    if (humidity) EnvironmentNP[2].setValue(humidity.value());
    if (dewPoint) EnvironmentNP[3].setValue(dewPoint.value());
    EnvironmentNP.setState(surface && air && humidity && dewPoint ? IPS_OK : IPS_ALERT);

    DewOutputsNP.setState(IPS_OK);
    PowerChannelsSP.setState(IPS_OK);
    USBPortSP.setState(IPS_OK);
    DewChannelsSP.setState(IPS_OK);
    DewChannelDutyCycleNP.setState(IPS_OK);
    Dew1ModeSP.setState(IPS_OK);
    Dew2ModeSP.setState(IPS_OK);
    applyPendingAwareLiveStates();

    DeviceInfoTP.apply();
    PowerSensorsNP.apply();
    EnvironmentNP.apply();
    PowerChannelsSP.apply();
    USBPortSP.apply();
    DewChannelsSP.apply();
    DewChannelDutyCycleNP.apply();
    Dew1ModeSP.apply();
    Dew2ModeSP.apply();
    DewOutputsNP.apply();
}

void GeminiPBH::checkTelemetryFreshness()
{
    checkTelemetryFreshness(std::chrono::steady_clock::now());
}

void GeminiPBH::checkTelemetryFreshness(std::chrono::steady_clock::time_point now)
{
    if (driverState_ != DriverState::Connected || !haveTelemetry_)
        return;

    if (now - lastTelemetryTime_ > kTelemetryStaleTimeout)
        markCommunicationFailure("telemetry is stale");
}

#ifdef GEMINI_TESTING
void GeminiPBH::simulateConnectedTelemetryForTesting(const geminipbh::TelemetrySnapshot &snapshot)
{
    userDisconnectRequested_ = false;
    driverState_ = DriverState::Connected;
    haveTelemetry_ = false;
    pendingOperations_.clear();
    callbackStaging_.clear();
    defineConnectedProperties();
    callbackStaging_.stageTelemetry(snapshot);
    flushTelemetryToINDI();
}

void GeminiPBH::checkTelemetryFreshnessForTesting(std::chrono::steady_clock::time_point now)
{
    checkTelemetryFreshness(now);
}

bool GeminiPBH::communicationFailureForTesting() const
{
    return driverState_ == DriverState::CommunicationFailure;
}

IPState GeminiPBH::powerSensorsStateForTesting() const
{
    return PowerSensorsNP.getState();
}

IPState GeminiPBH::environmentStateForTesting() const
{
    return EnvironmentNP.getState();
}

IPState GeminiPBH::powerChannelsStateForTesting() const
{
    return PowerChannelsSP.getState();
}

IPState GeminiPBH::dewModeStateForTesting(size_t index) const
{
    if (index == 0)
        return Dew1ModeSP.getState();
    if (index == 1)
        return Dew2ModeSP.getState();
    return IPS_ALERT;
}

void GeminiPBH::simulateHeaterModePendingForTesting(size_t heaterIndex, geminipbh::HeaterMode targetMode,
        std::chrono::steady_clock::time_point deadline)
{
    auto target = gindi::makeHeaterModeTarget(heaterIndex, targetMode);
    if (!target) return;
    addPendingOperation(gindi::PendingOperation(gindi::PendingOperationKind::HeaterMode,
            heaterIndex,
            target.value(),
            deadline));
}

void GeminiPBH::checkPendingOperationsForTesting(const geminipbh::TelemetrySnapshot &snapshot)
{
    checkPendingOperations(&snapshot);
}

geminipbh::instrumentation::Level GeminiPBH::instrumentationLevelForTesting() const
{
    return device_ ? device_->instrumentationLevel() : geminipbh::instrumentation::Level::Off;
}

void GeminiPBH::syncInstrumentationLevelForTesting()
{
    syncInstrumentationLevelWithIndiDebug();
}

void GeminiPBH::setDebugForTesting(bool enable)
{
    setDebug(enable);
}

void GeminiPBH::setSerialPortForTesting(const std::string &port)
{
    updateSerialPortSelection(port);
}

std::string GeminiPBH::serialPortForTesting() const
{
    const char *port = serialConnection_ ? serialConnection_->port() : "";
    return port ? port : "";
}

void GeminiPBH::setAutoSearchForTesting(bool enabled)
{
    autoSearchOverrideForTesting_ = enabled;
}

void GeminiPBH::setDiscoveryFunctionForTesting(std::function<geminipbh::DiscoveryResult()> fn)
{
    discoveryFunctionForTesting_ = std::move(fn);
}

bool GeminiPBH::detectedPortSaveRequestedForTesting() const
{
    return detectedPortSaveRequestedForTesting_;
}

#endif

void GeminiPBH::checkPendingOperations()
{
    auto snapshot = callbackStaging_.latestTelemetry();

    checkPendingOperations(snapshot ? &*snapshot : nullptr);
}

void GeminiPBH::checkPendingOperations(const geminipbh::TelemetrySnapshot *snapshot)
{
    if (pendingOperations_.empty())
        return;

    const auto now = std::chrono::steady_clock::now();
    bool dcAlert = false;
    bool usbAlert = false;
    bool dewEnabledAlert = false;
    std::array<bool, kDewOutputCount> dewModeAlert{};

    auto markAlert = [&](gindi::PendingOperationKind kind, size_t index)
    {
        switch (kind)
        {
            case gindi::PendingOperationKind::DcOutput:
                dcAlert = true;
                break;
            case gindi::PendingOperationKind::UsbOutput:
                usbAlert = true;
                break;
            case gindi::PendingOperationKind::HeaterEnabled:
                dewEnabledAlert = true;
                break;
            case gindi::PendingOperationKind::HeaterMode:
                if (index < kDewOutputCount)
                    dewModeAlert[index] = true;
                break;
        }
    };

    pendingOperations_.erase(std::remove_if(pendingOperations_.begin(), pendingOperations_.end(),
                                            [&](const gindi::PendingOperation & operation)
    {
        switch (gindi::evaluatePendingOperation(operation, snapshot, now))
        {
            case gindi::PendingEvaluation::Confirmed:
                LOGF_DEBUG("Confirmed %s %zu from telemetry.",
                           gindi::pendingOperationName(operation.kind), operation.index + 1);
                return true;

            case gindi::PendingEvaluation::Pending:
            case gindi::PendingEvaluation::ValueUnavailable:
                return false;

            case gindi::PendingEvaluation::Timeout:
                LOGF_WARN("Telemetry confirmation timed out for %s %zu.",
                          gindi::pendingOperationName(operation.kind), operation.index + 1);
                markAlert(operation.kind, operation.index);
                return true;

            case gindi::PendingEvaluation::NotConfirmable:
                LOGF_ERROR("Internal driver error: non-confirmable operation was added as pending for %s %zu.",
                           gindi::pendingOperationName(operation.kind), operation.index + 1);
                markAlert(operation.kind, operation.index);
                return true;

            case gindi::PendingEvaluation::InvalidTarget:
                LOGF_ERROR("Internal driver error: invalid pending target for %s %zu.",
                           gindi::pendingOperationName(operation.kind), operation.index + 1);
                markAlert(operation.kind, operation.index);
                return true;
        }
        return false;
    }), pendingOperations_.end());

    if (dcAlert) PowerChannelsSP.setState(IPS_ALERT);
    if (usbAlert) USBPortSP.setState(IPS_ALERT);
    if (dewEnabledAlert) DewChannelsSP.setState(IPS_ALERT);
    if (dewModeAlert[0]) Dew1ModeSP.setState(IPS_ALERT);
    if (dewModeAlert[1]) Dew2ModeSP.setState(IPS_ALERT);

    applyPendingAwareLiveStates();
    PowerChannelsSP.apply();
    USBPortSP.apply();
    DewChannelsSP.apply();
    Dew1ModeSP.apply();
    Dew2ModeSP.apply();
}

void GeminiPBH::attemptReconnectIfDue()
{
    if (userDisconnectRequested_ || shutdownInProgress_.load(std::memory_order_acquire)
            || driverState_ != DriverState::CommunicationFailure || !device_)
        return;

    const auto now = std::chrono::steady_clock::now();
    if (lastReconnectAttempt_.time_since_epoch().count() != 0 && now - lastReconnectAttempt_ < kReconnectInterval)
        return;

    driverState_ = DriverState::Reconnecting;
    lastReconnectAttempt_ = now;
    failPendingOperations(IPS_ALERT);
    markHardwareProperties(IPS_ALERT);
    LOG_INFO("Attempting Gemini Power Box Hub Advanced v3 reconnect through libgeminipbh.");

    const geminipbh::Result result = device_->reconnect();
    if (!result)
    {
        driverState_ = DriverState::CommunicationFailure;
        logDeviceError("reconnect", result.error());
        markHardwareProperties(IPS_ALERT);
        return;
    }

    driverState_ = DriverState::Connected;
    defineConnectedProperties();
    flushConnectionEvents();
    flushTelemetryToINDI();
    refreshPropertiesFromDevice();
}

void GeminiPBH::markCommunicationFailure(const std::string &reason)
{
    if (driverState_ == DriverState::CommunicationFailure || userDisconnectRequested_)
        return;

    driverState_ = DriverState::CommunicationFailure;
    haveTelemetry_ = false;
    failPendingOperations(IPS_ALERT);

    PowerSensorsNP.setState(IPS_ALERT);
    EnvironmentNP.setState(IPS_ALERT);
    PowerChannelsSP.setState(IPS_ALERT);
    USBPortSP.setState(IPS_ALERT);
    DewChannelsSP.setState(IPS_ALERT);
    DewChannelDutyCycleNP.setState(IPS_ALERT);
    Dew1ModeSP.setState(IPS_ALERT);
    Dew2ModeSP.setState(IPS_ALERT);
    DewOutputsNP.setState(IPS_ALERT);

    PowerSensorsNP.apply();
    EnvironmentNP.apply();
    PowerChannelsSP.apply();
    USBPortSP.apply();
    DewChannelsSP.apply();
    DewChannelDutyCycleNP.apply();
    Dew1ModeSP.apply();
    Dew2ModeSP.apply();
    DewOutputsNP.apply();

    LOGF_WARN("Gemini Power Box Hub Advanced v3 communication failure: %s", reason.c_str());
}

void GeminiPBH::markHardwareProperties(IPState state)
{
    if (!connectedPropertiesDefined_)
        return;

    DeviceInfoTP.setState(state);
    PowerSensorsNP.setState(state);
    EnvironmentNP.setState(state);
    PowerChannelsSP.setState(state);
    USBPortSP.setState(state);
    DewChannelsSP.setState(state);
    DewChannelDutyCycleNP.setState(state);
    Dew1ModeSP.setState(state);
    Dew2ModeSP.setState(state);
    DewOutputsNP.setState(state);

    DeviceInfoTP.apply();
    PowerSensorsNP.apply();
    EnvironmentNP.apply();
    PowerChannelsSP.apply();
    USBPortSP.apply();
    DewChannelsSP.apply();
    DewChannelDutyCycleNP.apply();
    Dew1ModeSP.apply();
    Dew2ModeSP.apply();
    DewOutputsNP.apply();
}

void GeminiPBH::failPendingOperations(IPState state)
{
    if (pendingOperations_.empty())
        return;

    pendingOperations_.clear();
    if (!connectedPropertiesDefined_)
        return;

    PowerChannelsSP.setState(state);
    USBPortSP.setState(state);
    DewChannelsSP.setState(state);
    Dew1ModeSP.setState(state);
    Dew2ModeSP.setState(state);

    PowerChannelsSP.apply();
    USBPortSP.apply();
    DewChannelsSP.apply();
    Dew1ModeSP.apply();
    Dew2ModeSP.apply();
}

void GeminiPBH::addPendingOperation(gindi::PendingOperation operation)
{
    gindi::replacePendingOperation(pendingOperations_, operation);
}

bool GeminiPBH::hasPendingOperation(gindi::PendingOperationKind kind) const
{
    return std::any_of(pendingOperations_.begin(), pendingOperations_.end(),
    [kind](const gindi::PendingOperation & operation)
    {
        return operation.kind == kind;
    });
}

bool GeminiPBH::hasPendingOperation(gindi::PendingOperationKind kind, size_t index) const
{
    return std::any_of(pendingOperations_.begin(), pendingOperations_.end(),
    [kind, index](const gindi::PendingOperation & operation)
    {
        return operation.kind == kind && operation.index == index;
    });
}

void GeminiPBH::applyPendingAwareLiveStates()
{
    if (!connectedPropertiesDefined_ || driverState_ != DriverState::Connected)
        return;

    auto setState = [](auto & property, bool pending)
    {
        if (pending)
            property.setState(IPS_BUSY);
        else if (property.getState() != IPS_ALERT)
            property.setState(IPS_OK);
    };

    setState(PowerChannelsSP, hasPendingOperation(gindi::PendingOperationKind::DcOutput));
    setState(USBPortSP, hasPendingOperation(gindi::PendingOperationKind::UsbOutput));
    setState(DewChannelsSP, hasPendingOperation(gindi::PendingOperationKind::HeaterEnabled));
    setState(Dew1ModeSP, hasPendingOperation(gindi::PendingOperationKind::HeaterMode, 0));
    setState(Dew2ModeSP, hasPendingOperation(gindi::PendingOperationKind::HeaterMode, 1));
    if (DewChannelDutyCycleNP.getState() != IPS_ALERT)
        DewChannelDutyCycleNP.setState(IPS_OK);
}

void GeminiPBH::logDeviceError(const char *action, geminipbh::Error error) const
{
    LOGF_ERROR("Gemini Power Box Hub Advanced v3 %s failed: %s.", action, gindi::errorName(error));
}

void GeminiPBH::refreshLabelsFromTextProperties()
{
    for (size_t i = 0; i < kDcOutputCount; ++i)
        PowerChannelsSP[i].setLabel(PowerChannelLabelsTP[i].getText());
    for (size_t i = 0; i < kDewOutputCount; ++i)
    {
        DewChannelsSP[i].setLabel(DewChannelLabelsTP[i].getText());
        DewChannelDutyCycleNP[i].setLabel((std::string(DewChannelLabelsTP[i].getText()) + " (%)").c_str());
        DewOutputsNP[i].setLabel((std::string(DewChannelLabelsTP[i].getText()) + " Current Output (%)").c_str());
    }
    for (size_t i = 0; i < kUsbOutputCount; ++i)
        USBPortSP[i].setLabel(USBPortLabelsTP[i].getText());
}

bool GeminiPBH::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    PowerChannelLabelsTP.save(fp);
    DewChannelLabelsTP.save(fp);
    USBPortLabelsTP.save(fp);
    return true;
}

const char *GeminiPBH::heaterModeName(geminipbh::HeaterMode mode)
{
    return gindi::heaterModeName(mode);
}
