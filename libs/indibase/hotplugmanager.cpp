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

#ifdef HAVE_UDEV
#include <libudev.h> // For udev
#include <poll.h>    // For poll()
#endif

namespace INDI
{

HotPlugManager::HotPlugManager() : udevMonitorRunning(false), pollingCount(0), oneShotMode(false)
{
#ifdef HAVE_UDEV
    udevContext = nullptr;
    udevMonitor = nullptr;
#endif
    hotPlugTimer.setSingleShot(false); // Timer runs periodically
    hotPlugTimer.callOnTimeout(std::bind(&HotPlugManager::checkHotPlugEvents, this));
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
            checkHotPlugEvents(); // Perform a hotplug check
            pollingCount++;

            if (pollingCount.load() >= 5)
            {
                hotPlugTimer.stop(); // Stop the polling timer

                if (this->oneShotMode.load())
                {
                    LOG_DEBUG("HotPlugManager: Initial polling finished (5 times). Hotplugging disabled (one-shot mode).");
                }
                else
                {
                    LOG_DEBUG("HotPlugManager: Initial polling finished (5 times). Starting udev event monitoring.");
                    udevMonitorRunning.store(true);
                    udevMonitorThread = std::thread(&HotPlugManager::udevEventMonitor, this);
                }
            }
            else
            {
                LOGF_DEBUG("HotPlugManager: Initial polling count: %d/5", pollingCount.load());
            }
        });
        hotPlugTimer.start();
        LOGF_DEBUG("HotPlugManager started with initial polling (1000ms interval, 5 times)%s.",
                   this->oneShotMode.load() ? ", then disabled" : ", then udev events");
    }
    else
#endif
    {
        // Fallback to continuous polling if udev is not available or HAVE_UDEV is not defined
        hotPlugTimer.setSingleShot(false);
        hotPlugTimer.setInterval(intervalMs);
        hotPlugTimer.start();
        LOGF_DEBUG("HotPlugManager started with continuous polling interval: %u ms (udev not available)", intervalMs);
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

        // 1. Discover currently connected devices
        std::vector<std::string> discoveredIdentifiers = handler->discoverConnectedDeviceIdentifiers();
        std::set<std::string> currentConnected(discoveredIdentifiers.begin(), discoveredIdentifiers.end());

        // 2. Get devices currently managed by the handler
        const std::map<std::string, std::shared_ptr<DefaultDevice>>& managedDevices = handler->getManagedDevices();
        std::set<std::string> currentlyManaged;
        for (const auto& pair : managedDevices)
        {
            currentlyManaged.insert(pair.first);
        }

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

                // Trigger hotplug check for all handlers
                checkHotPlugEvents();
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
