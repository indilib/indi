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
#include <poll.h>    // For poll()
#endif

namespace INDI
{

const int MAX_INITIAL_POLL = 5; // Maximum number of initial polls when udev is available
const int MAX_NON_UDEV_POLL_DURATION_SECONDS = 60; // Maximum duration for non-udev polling in seconds
const int NON_UDEV_POLL_INTERVAL_MS = 1000; // Default interval for non-udev polling if not specified by driver


HotPlugManager::HotPlugManager() : udevMonitorRunning(false), pollingCount(0), oneShotMode(false),
    udevEventReceived(false) // Initialize udevEventReceived
{
#ifdef HAVE_UDEV
    udevContext = nullptr;
    udevMonitor = nullptr;
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
    if (hotPlugTimer.isActive() || udevMonitorRunning.load())
    {
        LOG_DEBUG("HotPlugManager already running.");
        return;
    }

    this->oneShotMode.store(oneShot);
    pollingCount.store(0); // Reset polling count on start

#ifdef HAVE_UDEV
    if (udevMonitor && udevContext)
    {
        // Initial polling for 5 times (1000ms interval)
        hotPlugTimer.setSingleShot(false); // Keep polling until count reached
        hotPlugTimer.setInterval(1000); // 1 second interval
        hotPlugTimer.callOnTimeout([this, intervalMs]()
        {
            // Phase 1: Initial Polling (if udev is available and not yet completed)
            if (udevMonitor && udevContext && pollingCount.load() < MAX_INITIAL_POLL)
            {
                checkHotPlugEvents(); // Perform a hotplug check
                pollingCount++;
                LOGF_DEBUG("HotPlugManager: Initial polling count: %d/%d", pollingCount.load(), MAX_INITIAL_POLL);

                if (pollingCount.load() >= MAX_INITIAL_POLL)
                {
                    // Initial polling finished. Now switch hotPlugTimer to debounce monitoring mode.
                    LOGF_DEBUG("HotPlugManager: Initial polling finished (%d times). Switching hotPlugTimer to debounce monitoring mode.",
                               MAX_INITIAL_POLL);
                    hotPlugTimer.setInterval(100); // Set a frequent interval for checking debounce flag

                    if (this->oneShotMode.load())
                    {
                        LOG_DEBUG("HotPlugManager: Hotplugging disabled (one-shot mode) after initial polling.");
                        hotPlugTimer.stop(); // Stop hotPlugTimer completely in one-shot mode
                    }
                    else
                    {
                        // Start udev monitor thread and keep hotPlugTimer running for debounce management
                        udevMonitorRunning.store(true);
                        udevMonitorThread = std::thread(&HotPlugManager::udevEventMonitor, this);
                    }
                }
            }
            // Phase 2: UDEV Event Debounce Monitoring (after initial polling, if udev is available and not one-shot)
            else if (udevMonitor && udevContext && udevMonitorRunning.load() && !this->oneShotMode.load())
            {
                if (udevEventReceived.exchange(false)) // Atomically check and reset the flag
                {
                    LOG_DEBUG("HotPlugManager: Udev event signaled. Debouncing...");
                    mainThreadDebounceTimer.start(); // Start/restart the debounce timer
                }
            }
            // Phase 3: Fallback to continuous polling (if udev is not available)
            else if (!udevMonitor || !udevContext)
            {
                checkHotPlugEvents(); // Perform a hotplug check

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - nonUdevPollingStartTime).count();

                if (elapsed >= MAX_NON_UDEV_POLL_DURATION_SECONDS)
                {
                    hotPlugTimer.stop();
                    LOGF_DEBUG("HotPlugManager: Non-udev polling stopped after %d seconds (max %d seconds reached).",
                               elapsed, MAX_NON_UDEV_POLL_DURATION_SECONDS);
                }
            }
        });
        hotPlugTimer.start();
        LOGF_DEBUG("HotPlugManager started with initial polling (1000ms interval, %d times)%s.", MAX_INITIAL_POLL,
                   this->oneShotMode.load() ? ", then disabled" : ", then udev events");
    }
    else
#endif
    {
        // Fallback to continuous polling if udev is not available or HAVE_UDEV is not defined
        // We need to ensure that the polling does not exceed MAX_NON_UDEV_POLL_DURATION_SECONDS
        // if the driver sets a very long interval.
        hotPlugTimer.setSingleShot(false);
        hotPlugTimer.setInterval(intervalMs); // Use the interval provided by the driver
        nonUdevPollingStartTime = std::chrono::steady_clock::now(); // Record start time

        hotPlugTimer.callOnTimeout([this, intervalMs]()
        {
            checkHotPlugEvents(); // Perform a hotplug check

            // Check if the maximum non-udev polling duration has been reached
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - nonUdevPollingStartTime).count();

            if (elapsed >= MAX_NON_UDEV_POLL_DURATION_SECONDS)
            {
                hotPlugTimer.stop();
                LOGF_DEBUG("HotPlugManager: Non-udev polling stopped after %d seconds (max %d seconds reached).",
                           elapsed, MAX_NON_UDEV_POLL_DURATION_SECONDS);
            }
        });
        hotPlugTimer.start();
        LOGF_DEBUG("HotPlugManager started with continuous polling interval: %u ms (udev not available). Max duration: %d seconds.",
                   intervalMs, MAX_NON_UDEV_POLL_DURATION_SECONDS);
    }
}

void HotPlugManager::stop()
{
    if (hotPlugTimer.isActive())
    {
        hotPlugTimer.stop();
        LOG_DEBUG("HotPlugManager stopped polling timer.");
    }

#ifdef HAVE_UDEV
    if (udevMonitorRunning.load())
    {
        udevMonitorRunning.store(false);
        if (udevMonitorThread.joinable())
        {
            // To unblock select() in udevEventMonitor, we need to close the FD
            // This will cause select() to return with an error, and the thread can exit.
            // However, closing the FD here might affect other udev operations if any.
            // A more robust way would be to use a pipe to signal the thread to exit.
            // For now, we'll rely on the atomic flag and a short timeout in select.
            // Or, if the FD is closed, select will return -1 and errno will be EBADF.
            // For simplicity, we'll just set the flag and join.
            // The udevEventMonitor will need to handle the shutdown gracefully.
            LOG_DEBUG("HotPlugManager: Joining udev monitor thread.");
            udevMonitorThread.join();
            LOG_DEBUG("HotPlugManager: udev monitor thread joined.");
        }
    }
#else
    {
        LOG_DEBUG("HotPlugManager not running.");
    }
#endif
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

    LOG_DEBUG("HotPlugManager: udev monitor initialized successfully.");
    return true;
}

void HotPlugManager::deinitUdev()
{
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
void HotPlugManager::udevEventMonitor()
{
    int fd = udev_monitor_get_fd(udevMonitor);
    if (fd < 0)
    {
        LOG_ERROR("HotPlugManager: Failed to get udev monitor file descriptor.");
        udevMonitorRunning.store(false);
        return;
    }

    struct pollfd fds[1];
    fds[0].fd = fd;
    fds[0].events = POLLIN;

    while (udevMonitorRunning.load())
    {
        // Use poll with a timeout to allow the thread to check udevMonitorRunning flag
        int ret = poll(fds, 1, 1000); // 1 second timeout

        if (ret < 0)
        {
            if (errno != EINTR) // Ignore interrupted system calls
            {
                LOGF_DEBUG("HotPlugManager: poll failed: %s", strerror(errno));
            }
            continue;
        }

        if (ret == 0) // Timeout
        {
            continue;
        }

        if (fds[0].revents & POLLIN)
        {
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
                udev_device_unref(dev);
            }
            else
            {
                LOG_ERROR("HotPlugManager: udev_monitor_receive_device returned null.");
            }
        }
    }
    LOG_DEBUG("HotPlugManager: udev event monitor stopped.");
}
#endif

} // namespace INDI
