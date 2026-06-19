/*
    Hot Plug Capable Device Class Header File

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

#include "defaultdevice.h"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace INDI
{

class HotPlugCapableDevice
{
    public:
        HotPlugCapableDevice() = default;
        virtual ~HotPlugCapableDevice() = default;

        /**
         * @brief Static name used for LOGGING purposes.
         */
        static const char* getDeviceName()
        {
            return "HotPlugCapableDevice";
        }

        /**
         * @brief Discovers currently connected devices of this driver's type.
         * @return A vector of unique string identifiers for connected devices.
         */
        virtual std::vector<std::string> discoverConnectedDeviceIdentifiers() = 0;

        /**
         * @brief Factory method to create a new device instance.
         * @param identifier The unique string identifier of the device to create.
         * @return A shared pointer to the newly created DefaultDevice instance.
         */
        virtual std::shared_ptr<DefaultDevice> createDevice(const std::string& identifier) = 0;

        /**
         * @brief Destroys a device instance and performs driver-specific cleanup.
         * @param device A shared pointer to the device to destroy.
         */
        virtual void destroyDevice(std::shared_ptr<DefaultDevice> device) = 0;

        /**
         * @brief Provides a unified map view of currently managed devices.
         * @return A const reference to a map of managed devices, keyed by their unique string identifiers.
         */
        virtual const std::map<std::string, std::shared_ptr<DefaultDevice>>& getManagedDevices() const = 0;
};

} // namespace INDI
