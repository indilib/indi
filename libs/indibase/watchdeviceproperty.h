/*
    Copyright (C) 2022 Pawel Soja <kernel32.pl@gmail.com>
    Copyright (C) 2011 Jasem Mutlaq. All rights reserved.

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

#include "basedevice.h"
#include "parentdevice.h"
#include "indililxml.h"

#include <functional>
#include <map>
#include <set>
#include <vector>

namespace INDI
{
// Internal use only - Common implementatnion for client and driver side
class WatchDeviceProperty
{
    public:
        struct DeviceInfo
        {
            ParentDevice device{ParentDevice::Invalid};
            std::function<void (BaseDevice)> newDeviceCallback; // call if device available
            std::set<std::string> properties; // watch only specific properties only

            void emitWatchDevice()
            {
                if (newDeviceCallback)
                    newDeviceCallback(device);
            }
        };

    public:
        std::vector<BaseDevice> getDevices() const;
        BaseDevice getDeviceByName(const char *name);
        DeviceInfo &ensureDeviceByName(const char *name, const std::function<ParentDevice()> &constructor);

    public:
        bool isEmpty() const;

        /**
         * @brief checks if the device is being watched by something
         * 
         * @param deviceName name of the searched device on the list
         * @return true if the device is in the list or the list is empty 
         */
        bool isDeviceWatched(const char *deviceName) const;

    public:
        void unwatchDevices();

        void watchDevice(const std::string &deviceName);
        void watchDevice(const std::string &deviceName, const std::function<void (BaseDevice)> &callback);

        void watchProperty(const std::string &deviceName, const std::string &propertyName);

        void clear();
        void clearDevices();
        bool deleteDevice(const BaseDevice &device);

    public:
        int processXml(const INDI::LilXmlElement &root, char *errmsg, const std::function<ParentDevice()> &constructor = [] { return ParentDevice(ParentDevice::Valid); } );

    public:
        std::map<std::string, DeviceInfo>::iterator begin()
        {
            return data.begin();
        }

        std::map<std::string, DeviceInfo>::iterator end()
        {
            return data.end();
        }

    protected:
        std::set<std::string> watchedDevice;
        std::map<std::string, DeviceInfo> data;
};

}
