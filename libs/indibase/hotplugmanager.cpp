/*
    Hot Plug Manager Class Source File

    Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "hotplugmanager.h"
#include "indilogger.h" // For LOG_DEBUG, LOG_DEBUG, etc.
#include <algorithm>    // For std::remove_if
#include <set>          // For std::set to easily find differences
#include <errno.h>      // For program_invocation_short_name
#include <chrono>       // For std::chrono::steady_clock

#ifdef HAVE_UDEV
#include <libudev.h> // For udev
#endif

// Event loop functions (C API)
extern "C" {
    int IEAddCallback(int readfiledes, void (*fp)(int, void *), void *p);
    void IERmCallback(int callbackid);
}

namespace INDI
{

[[maybe_unused]] const int MAX_INITIAL_POLL = 5; // Maximum number of initial polls when udev is available
const int MAX_NON_UDEV_POLL_DURATION_SECONDS = 60; // Maximum duration for non-udev polling in seconds
[[maybe_unused]] const int NON_UDEV_POLL_INTERVAL_MS =
    1000; // Default interval for non-udev polling if not specified by driver


HotPlugManager::HotPlugManager() : pollingCount(0), oneShotMode(false),
    nonUdevPollingDurationSeconds(-1), initialPollingDurationSeconds(-1),
    udevEventReceived(false) // Initialize with -1 (use defaults)
{
#ifdef HAVE_UDEV
    udevContext = nullptr;
    udevMonitor = nullptr;
    udevCallbackId = -1;
#endif
    hotPlugTimer.setSingleShot(false); // Timer runs periodically
    hotPlugTimer.callOnTimeout(std::bind(&HotPlugManager::checkHotPlugEvents, this));

    // Initialize mainThreadDebounceTimer for debouncing udev events in the main thread
    mainThreadDebounceTimer.setSingleShot(true);
    mainThreadDebounceTimer.setInterval(100);
    mainThreadDebounceTimer.callOnTimeout(std::bind(&HotPlugManager::checkHotPlugEvents, this));

    LOG_DEBUG("HotPlugManager initialized.");
#ifdef HAVE_UDEV
    initUdev(); // Attempt to initialize udev
#endif
}

HotPlugManager::~HotPlugManager()
{
    stop();
#ifdef HAVE_UDEV
    deinitUdev(); // Deinitialize udev
#endif
    LOG_DEBUG("HotPlugManager shut down.");
}

HotPlugManager &HotPlugManager::getInstance()
{
    static HotPlugManager instance;
    return instance;
}

void HotPlugManager::registerHandler(std::shared_ptr<HotPlugCapableDevice> handler)
{
    std::unique_lock<std::recursive_mutex> lock(handlersMutex);
    // Check if handler is already registered
    for (const auto& existingHandler : registeredHandlers)
    {
        if (existingHandler == handler)
        {
            LOG_DEBUG("Attempted to register an already registered HotPlugCapableDevice handler.");
            return;
        }
    }
    registeredHandlers.push_back(handler);
    LOG_DEBUG("HotPlugCapableDevice handler registered.");
}

void HotPlugManager::unregisterHandler(std::shared_ptr<HotPlugCapableDevice> handler)
{
    std::unique_lock<std::recursive_mutex> lock(handlersMutex);
    auto it = std::remove_if(registeredHandlers.begin(), registeredHandlers.end(),
                             [&](const std::shared_ptr<HotPlugCapableDevice> &h)
    {
        return h == handler;
    });

    if (it != registeredHandlers.end())
    {
        registeredHandlers.erase(it, registeredHandlers.end());
        LOG_DEBUG("HotPlugCapableDevice handler unregistered.");
    }
    else
    {
        LOG_DEBUG("Attempted to unregister a HotPlugCapableDevice handler that was not registered.");
    }
}

void HotPlugManager::start(uint32_t intervalMs, bool oneShot)
{
    if (hotPlugTimer.isActive())
    {
        LOG_DEBUG("HotPlugManager already running.");
        return;
    }

    this->oneShotMode.store(oneShot);
    pollingCount.store(0); // Reset polling count on start

#ifdef HAVE_UDEV
    if (udevMonitor && udevContext && udevCallbackId >= 0)
    {
        // Event-driven mode: udev events handled by event loop callback
        // Initial polling to detect already connected devices

        // Determine the maximum number of initial polls based on configuration
        int maxInitialPolls = initialPollingDurationSeconds.load();
        if (maxInitialPolls == -1)
        {
            maxInitialPolls = MAX_INITIAL_POLL; // Use default
        }

        hotPlugTimer.setSingleShot(false);
        hotPlugTimer.setInterval(1000); // 1 second interval
        hotPlugTimer.callOnTimeout([this, maxInitialPolls]()
        {
            if (pollingCount.load() < maxInitialPolls)
            {
                checkHotPlugEvents(); // Perform a hotplug check
                pollingCount++;
                LOGF_DEBUG("HotPlugManager: Initial polling count: %d/%d", pollingCount.load(), maxInitialPolls);

                if (pollingCount.load() >= maxInitialPolls)
                {
                    LOGF_DEBUG("HotPlugManager: Initial polling finished (%d times).", maxInitialPolls);
                    hotPlugTimer.stop();

                    if (this->oneShotMode.load())
                    {
                        LOG_DEBUG("HotPlugManager: Hotplugging disabled (one-shot mode) after initial polling.");
                    }
                    else
                    {
                        LOG_DEBUG("HotPlugManager: Now monitoring udev events via event loop callback.");
                        // The event loop callback will trigger handleUdevEvent() when devices change
                        // No more timer needed - completely event-driven!
                    }
                }
            }
        });
        hotPlugTimer.start();

        if (maxInitialPolls == MAX_INITIAL_POLL)
        {
            LOGF_DEBUG("HotPlugManager started with initial polling (1000ms interval, %d times - default)%s.", maxInitialPolls,
                       this->oneShotMode.load() ? ", then disabled" : ", then event-driven via callback");
        }
        else
        {
            LOGF_DEBUG("HotPlugManager started with initial polling (1000ms interval, %d times)%s.", maxInitialPolls,
                       this->oneShotMode.load() ? ", then disabled" : ", then event-driven via callback");
        }
    }
    else
#endif
    {
        // Fallback to continuous polling if udev is not available or HAVE_UDEV is not defined
        hotPlugTimer.setSingleShot(false);
        hotPlugTimer.setInterval(intervalMs); // Use the interval provided by the driver
        nonUdevPollingStartTime = std::chrono::steady_clock::now(); // Record start time

        hotPlugTimer.callOnTimeout([this]()
        {
            checkHotPlugEvents(); // Perform a hotplug check

            // Determine the maximum duration based on configuration
            int maxDuration = nonUdevPollingDurationSeconds.load();
            if (maxDuration == -1)
            {
                maxDuration = MAX_NON_UDEV_POLL_DURATION_SECONDS; // Use default
            }

            // Check if the maximum non-udev polling duration has been reached (0 = unlimited)
            if (maxDuration > 0)
            {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - nonUdevPollingStartTime).count();

                if (elapsed >= maxDuration)
                {
                    hotPlugTimer.stop();
                    LOGF_DEBUG("HotPlugManager: Non-udev polling stopped after %d seconds (max %d seconds reached).",
                               elapsed, maxDuration);
                }
            }
        });
        hotPlugTimer.start();

        // Log the configured duration
        int maxDuration = nonUdevPollingDurationSeconds.load();
        if (maxDuration == -1)
        {
            LOGF_DEBUG("HotPlugManager started with continuous polling interval: %u ms (udev not available). Max duration: %d seconds (default).",
                       intervalMs, MAX_NON_UDEV_POLL_DURATION_SECONDS);
        }
        else if (maxDuration == 0)
        {
            LOGF_DEBUG("HotPlugManager started with continuous polling interval: %u ms (udev not available). Max duration: unlimited.",
                       intervalMs);
        }
        else
        {
            LOGF_DEBUG("HotPlugManager started with continuous polling interval: %u ms (udev not available). Max duration: %d seconds.",
                       intervalMs, maxDuration);
        }
    }
}

void HotPlugManager::stop()
{
    if (hotPlugTimer.isActive())
    {
        hotPlugTimer.stop();
        LOG_DEBUG("HotPlugManager stopped polling timer.");
    }

    // Stop the debounce timer if active
    if (mainThreadDebounceTimer.isActive())
    {
        mainThreadDebounceTimer.stop();
        LOG_DEBUG("HotPlugManager stopped debounce timer.");
    }

    // Note: The event loop callback remains registered and will be cleaned up
    // when deinitUdev() is called in the destructor
}

void HotPlugManager::setNonUdevPollingDuration(int seconds)
{
    nonUdevPollingDurationSeconds.store(seconds);

    if (seconds == -1)
    {
        LOGF_DEBUG("HotPlugManager: Non-udev polling duration set to default (%d seconds).", MAX_NON_UDEV_POLL_DURATION_SECONDS);
    }
    else if (seconds == 0)
    {
        LOG_DEBUG("HotPlugManager: Non-udev polling duration set to unlimited.");
    }
    else
    {
        LOGF_DEBUG("HotPlugManager: Non-udev polling duration set to %d seconds.", seconds);
    }
}

void HotPlugManager::setInitialPollingDuration(int seconds)
{
    // Clamp the value between 1 and MAX_NON_UDEV_POLL_DURATION_SECONDS, or use -1 for default
    if (seconds != -1 && seconds > 0)
    {
        seconds = std::min(seconds, MAX_NON_UDEV_POLL_DURATION_SECONDS);
    }

    initialPollingDurationSeconds.store(seconds);

    if (seconds == -1)
    {
        LOGF_DEBUG("HotPlugManager: Initial polling duration set to default (%d seconds).", MAX_INITIAL_POLL);
    }
    else
    {
        LOGF_DEBUG("HotPlugManager: Initial polling duration set to %d seconds.", seconds);
    }
}

void HotPlugManager::checkHotPlugEvents()
{
    std::unique_lock<std::recursive_mutex> lock(handlersMutex);
    LOG_DEBUG("HotPlugManager: Checking for hot-plug events...");

    for (const auto& handler : registeredHandlers)
    {
        LOG_DEBUG("HotPlugManager: Checking handler for a device type.");

        // 1. Get devices currently managed by the handler
        std::map<std::string, std::shared_ptr<DefaultDevice>> managedDevices = handler->getManagedDevices(); // This is a copy now
        std::set<std::string> currentlyManaged;
        for (const auto& pair : managedDevices)
        {
            currentlyManaged.insert(pair.first);
        }

        // 2. Discover currently connected devices
        std::vector<std::string> discoveredIdentifiers = handler->discoverConnectedDeviceIdentifiers();
        std::set<std::string> currentConnected(discoveredIdentifiers.begin(), discoveredIdentifiers.end());

        // 3. Reconcile differences

        // Devices to remove (managed but not currently connected)
        for (const std::string& identifier : currentlyManaged)
        {
            if (currentConnected.find(identifier) == currentConnected.end())
            {
                // Device disconnected
                LOGF_DEBUG("HotPlugManager: Device disconnected: %s", identifier.c_str());
                std::shared_ptr<DefaultDevice> deviceToRemove = managedDevices.at(identifier);
                handler->destroyDevice(deviceToRemove);
                // Note: The handler's internal map should be updated by destroyDevice or subsequent createDevice calls
                // For std::deque, this means removing the element.
            }
        }

        // Devices to add (connected but not currently managed)
        for (const std::string& identifier : currentConnected)
        {
            if (currentlyManaged.find(identifier) == currentlyManaged.end())
            {
                // New device connected
                LOGF_DEBUG("HotPlugManager: New device connected: %s", identifier.c_str());
                std::shared_ptr<DefaultDevice> newDevice = handler->createDevice(identifier);
                if (newDevice)
                {
                    // Define properties for the new device
                    newDevice->ISGetProperties(nullptr);
                }
                else
                {
                    LOGF_ERROR("HotPlugManager: Failed to create device for identifier: %s", identifier.c_str());
                }
            }
        }
    }
}

#ifdef HAVE_UDEV
bool HotPlugManager::initUdev()
{
    udevContext = udev_new();
    if (!udevContext)
    {
        LOG_ERROR("HotPlugManager: Failed to create udev context.");
        return false;
    }

    udevMonitor = udev_monitor_new_from_netlink(udevContext, "udev");
    if (!udevMonitor)
    {
        LOG_ERROR("HotPlugManager: Failed to create udev monitor.");
        udev_unref(udevContext);
        udevContext = nullptr;
        return false;
    }

    // Filter for USB devices
    udev_monitor_filter_add_match_subsystem_devtype(udevMonitor, "usb", nullptr);
    udev_monitor_filter_add_match_subsystem_devtype(udevMonitor, "usb_device", nullptr);

    if (udev_monitor_enable_receiving(udevMonitor) < 0)
    {
        LOG_ERROR("HotPlugManager: Failed to enable udev monitor receiving.");
        udev_monitor_unref(udevMonitor);
        udevMonitor = nullptr;
        udev_unref(udevContext);
        udevContext = nullptr;
        return false;
    }

    // Register the udev file descriptor with the event loop
    int fd = udev_monitor_get_fd(udevMonitor);
    if (fd < 0)
    {
        LOG_ERROR("HotPlugManager: Failed to get udev monitor file descriptor.");
        udev_monitor_unref(udevMonitor);
        udevMonitor = nullptr;
        udev_unref(udevContext);
        udevContext = nullptr;
        return false;
    }

    udevCallbackId = IEAddCallback(fd, udevCallbackWrapper, this);
    if (udevCallbackId < 0)
    {
        LOG_ERROR("HotPlugManager: Failed to register udev callback with event loop.");
        udev_monitor_unref(udevMonitor);
        udevMonitor = nullptr;
        udev_unref(udevContext);
        udevContext = nullptr;
        return false;
    }

    LOGF_DEBUG("HotPlugManager: udev monitor initialized successfully (callback ID: %d).", udevCallbackId);
    return true;
}

void HotPlugManager::deinitUdev()
{
    // Unregister the event loop callback
    if (udevCallbackId >= 0)
    {
        IERmCallback(udevCallbackId);
        udevCallbackId = -1;
        LOG_DEBUG("HotPlugManager: udev callback unregistered from event loop.");
    }

    if (udevMonitor)
    {
        udev_monitor_unref(udevMonitor);
        udevMonitor = nullptr;
        LOG_DEBUG("HotPlugManager: udev monitor deinitialized.");
    }
    if (udevContext)
    {
        udev_unref(udevContext);
        udevContext = nullptr;
        LOG_DEBUG("HotPlugManager: udev context deinitialized.");
    }
}
#endif

#ifdef HAVE_UDEV
// Static callback wrapper for the event loop
void HotPlugManager::udevCallbackWrapper(int fd, void* userdata)
{
    INDI_UNUSED(fd);
    HotPlugManager* manager = static_cast<HotPlugManager*>(userdata);
    if (manager)
    {
        manager->handleUdevEvent(fd);
    }
}

// Handle udev events when the file descriptor becomes readable
void HotPlugManager::handleUdevEvent(int fd)
{
    INDI_UNUSED(fd);

    udev_device* dev = udev_monitor_receive_device(udevMonitor);
    if (dev)
    {
        const char* action = udev_device_get_action(dev);
        const char* subsystem = udev_device_get_subsystem(dev);
        const char* devnode = udev_device_get_devnode(dev);

        LOGF_DEBUG("HotPlugManager: udev event: %s %s %s", action ? action : "N/A",
                   subsystem ? subsystem : "N/A", devnode ? devnode : "N/A");

        // Signal that a udev event has been received
        udevEventReceived.store(true);

        // Start/restart the debounce timer
        mainThreadDebounceTimer.start();

        udev_device_unref(dev);
    }
    else
    {
        LOG_ERROR("HotPlugManager: udev_monitor_receive_device returned null.");
    }
}
#endif

} // namespace INDI
