/*
    Hot Plug Manager Class Header File

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

#pragma once

#include "hotplugcapabledevice.h"
#include "inditimer.h"
#include <memory>
#include <vector>
#include <mutex>
#include <map>
#include <atomic>   // For std::atomic_bool
#include <chrono>   // For std::chrono::steady_clock::time_point

#ifdef HAVE_UDEV
#include <libudev.h> // For udev hotplugging
#endif

namespace INDI
{

class HotPlugManager
{
    private:
        HotPlugManager(); // Private constructor for singleton
        ~HotPlugManager(); // Private destructor

    public:
        // Delete copy constructor and assignment operator to prevent copying
        HotPlugManager(const HotPlugManager &) = delete;
        HotPlugManager &operator=(const HotPlugManager &) = delete;

        /**
         * @brief Get the singleton instance of HotPlugManager.
         * @return Reference to the HotPlugManager instance.
         */
        static HotPlugManager &getInstance();

        /**
         * @brief Static name used for LOGGING purposes.
         */
        static const char* getDeviceName()
        {
            return "HotPlugManager";
        }

        /**
         * @brief Register a HotPlugCapableDevice handler with the manager.
         * @param handler A shared pointer to the HotPlugCapableDevice implementation.
         */
        void registerHandler(std::shared_ptr<HotPlugCapableDevice> handler);

        /**
         * @brief Unregister a HotPlugCapableDevice handler from the manager.
         * @param handler A shared pointer to the HotPlugCapableDevice implementation to unregister.
         */
        void unregisterHandler(std::shared_ptr<HotPlugCapableDevice> handler);

        /**
         * @brief Starts the hot-plugging timer.
         * @param intervalMs The interval in milliseconds for periodic checks.
         * @param oneShot If true, hotplugging will stop after initial device check.
         */
        void start(uint32_t intervalMs, bool oneShot = false);

        /**
         * @brief Stops the hot-plugging timer.
         */
        void stop();

        /**
         * @brief Set the maximum polling duration for non-udev systems (macOS, Windows).
         * @param seconds Maximum duration in seconds. 0 = unlimited polling, -1 = use default (60 seconds).
         * @note This only affects systems without udev. Linux with udev uses event-driven monitoring with no time limit.
         */
        void setNonUdevPollingDuration(int seconds);

        /**
         * @brief Set the maximum initial polling duration for systems with udev.
         * @param seconds Maximum duration in seconds. Must be between 1 and MAX_NON_UDEV_POLL_DURATION_SECONDS. -1 = use default (5 seconds).
         * @note This only affects the initial polling period on systems with udev. After initial polling, event-driven monitoring is used.
         */
        void setInitialPollingDuration(int seconds);

    private:
        /**
         * @brief Periodically checks for hot-plug events across all registered handlers.
         */
        void checkHotPlugEvents();
        bool initUdev();
        void deinitUdev();

        /**
         * @brief Handles udev events when the file descriptor becomes readable.
         * @param fd The udev monitor file descriptor
         */
        void handleUdevEvent(int fd);

        /**
         * @brief Static callback wrapper for the event loop (C linkage compatible).
         */
        static void udevCallbackWrapper(int fd, void* userdata);

        std::vector<std::shared_ptr<HotPlugCapableDevice>> registeredHandlers;
        INDI::Timer hotPlugTimer;
        std::recursive_mutex handlersMutex; // Mutex to protect access to registeredHandlers

#ifdef HAVE_UDEV
        udev* udevContext;
        udev_monitor* udevMonitor;
        int udevCallbackId;  // Callback ID for event loop
#endif
        std::atomic_int pollingCount;
        std::atomic_bool oneShotMode;
        std::chrono::steady_clock::time_point nonUdevPollingStartTime;
        std::atomic_int nonUdevPollingDurationSeconds;  // Configurable max duration for non-udev polling (-1 = default 60s, 0 = unlimited)
        std::atomic_int initialPollingDurationSeconds;  // Configurable max duration for initial polling with udev (-1 = default 5s)
        std::atomic_bool udevEventReceived;
        INDI::Timer mainThreadDebounceTimer;
};

} // namespace INDI
