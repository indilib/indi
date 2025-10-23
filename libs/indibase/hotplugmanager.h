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
#include <thread>   // For std::thread
#include <atomic>   // For std::atomic_bool

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
         * @brief Get the executable name of the running program.
         * @return The executable name as a const char*.
         */
        static const char* getDeviceName()
        {
            return program_invocation_short_name;
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

    private:
        /**
         * @brief Periodically checks for hot-plug events across all registered handlers.
         */
        void checkHotPlugEvents();
        bool initUdev();
        void deinitUdev();
        void udevEventMonitor();

        std::vector<std::shared_ptr<HotPlugCapableDevice>> registeredHandlers;
        INDI::Timer hotPlugTimer;
        std::recursive_mutex handlersMutex; // Mutex to protect access to registeredHandlers

#ifdef HAVE_UDEV
        udev* udevContext;
        udev_monitor* udevMonitor;
        std::thread udevMonitorThread;
#endif
        std::atomic_bool udevMonitorRunning;
        std::atomic_int pollingCount;
        std::atomic_bool oneShotMode;
};

} // namespace INDI
