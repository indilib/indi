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
#include "watchdeviceproperty.h"

namespace INDI
{

std::vector<BaseDevice *> WatchDeviceProperty::getDevices() const
{
    std::vector<BaseDevice *> result;
    for (const auto &it : data)
    {
        result.push_back(it.second.device.get());
    }
    return result;
}

std::vector<std::shared_ptr<BaseDevice>> WatchDeviceProperty::getSharedDevices() const
{
    std::vector<std::shared_ptr<BaseDevice>> result;
    for (const auto &it : data)
    {
        result.push_back(it.second.device);
    }
    return result;
}


bool WatchDeviceProperty::getDeviceByName(const char *name, std::shared_ptr<BaseDevice> &device) const
{
    auto it = data.find(name);
    if (it != data.end())
    {
        device = it->second.device;
        return true;
    }

    return false;
}

WatchDeviceProperty::DeviceInfo &WatchDeviceProperty::ensureDeviceByName(const char *name,
        const std::function<BaseDevice*()> &constructor)
{
    auto &it = data[name];
    if (it.device == nullptr)
    {
        it.device.reset(constructor());
        it.device->setDeviceName(name);
        if (auto mediator = it.device->getMediator())
        {
            mediator->newDevice(it.device);
            // Deprecated legacy call to be removed in 2.0
            mediator->newDevice(it.device.get());
        }
        it.emitWatchDevice();
    }
    return it;
}

bool WatchDeviceProperty::isEmpty() const
{
    return data.empty();
}

bool WatchDeviceProperty::isDeviceWatched(const char *name) const
{
    return watchedDevice.size() == 0 || watchedDevice.find(name) != watchedDevice.end();
}

void WatchDeviceProperty::unwatchDevices()
{
    watchedDevice.clear();
}

void WatchDeviceProperty::watchDevice(const std::string &deviceName)
{
    watchedDevice.insert(deviceName);
}

void WatchDeviceProperty::watchDevice(const std::string &deviceName, const std::function<void (BaseDevice)> &callback)
{
    watchedDevice.insert(deviceName);
    data[deviceName].newDeviceCallback = callback;
}

void WatchDeviceProperty::watchProperty(const std::string &deviceName, const std::string &propertyName)
{
    watchedDevice.insert(deviceName);
    data[deviceName].properties.insert(propertyName);
}

void WatchDeviceProperty::clear()
{
    data.clear();
}

void WatchDeviceProperty::clearDevices()
{
    for (auto &deviceInfo : data)
    {
        deviceInfo.second.device.reset();
    }
}

bool WatchDeviceProperty::deleteDevice(const std::shared_ptr<BaseDevice> &device)
{
    for (auto it = data.begin(); it != data.end();)
    {
        if (it->second.device == device)
        {
            it = data.erase(it);
            return true;
        }
        else
            ++it;
    }
    return false;
}

int WatchDeviceProperty::processXml(const INDI::LilXmlElement &root, char *errmsg,
                                    const std::function<BaseDevice*()> &constructor)
{
    auto deviceName = root.getAttribute("device");
    if (!deviceName.isValid() || deviceName.toString() == "" || !isDeviceWatched(deviceName))
    {
        return 0;
    }

    // Get the device information, if not available, create it
    auto &deviceInfo = ensureDeviceByName(deviceName, constructor);

    // If we are asked to watch for specific properties only, we ignore everything else
    if (deviceInfo.properties.size() != 0)
    {
        const auto it = deviceInfo.properties.find(root.getAttribute("name").toString());
        if (it == deviceInfo.properties.end())
            return 0;
    }

    static const std::set<std::string> defVectors
    {
        "defTextVector",  "defNumberVector", "defSwitchVector",
        "defLightVector", "defBLOBVector"
    };

    if (defVectors.find(root.tagName()) != defVectors.end())
    {
        return deviceInfo.device->buildProp(root, errmsg);
    }

    static const std::set<std::string> setVectors
    {
        "setTextVector",  "setNumberVector", "setSwitchVector",
        "setLightVector", "setBLOBVector"
    };

    if (setVectors.find(root.tagName()) != setVectors.end())
    {
        return deviceInfo.device->setValue(root, errmsg);
    }

    return INDI_DISPATCH_ERROR;
}

}
