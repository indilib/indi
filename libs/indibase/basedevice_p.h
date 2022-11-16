/*******************************************************************************
  Copyright(c) 2011 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indimacros.h"
#include "basedevice.h"
#include "lilxml.h"
#include "indibase.h"

#include <deque>
#include <string>
#include <mutex>
#include <map>
#include <functional>

#include "indipropertyblob.h"
#include "indililxml.h"

namespace INDI
{

class BaseDevice;
class BaseDevicePrivate
{
    public:
        BaseDevicePrivate();
        virtual ~BaseDevicePrivate();

        /** @brief Parse and store BLOB in the respective vector */
        int setBLOB(INDI::PropertyBlob propertyBlob, const INDI::LilXmlElement &root, char *errmsg);

        void addProperty(const INDI::Property &property)
        {
            {
                std::unique_lock<std::mutex> lock(m_Lock);
                pAll.push_back(property);
            }

            auto it = watchPropertyMap.find(property.getName());
            if (it != watchPropertyMap.end())
            {
                it->second(property);
            }
        }

    public: // mediator
        void mediateNewDevice(BaseDevice &baseDevice)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->newDevice(&baseDevice);
#endif
                mediator->newDevice(baseDevice);
            }
        }

        void mediateRemoveDevice(BaseDevice &baseDevice)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->removeDevice(&baseDevice);
#endif
                mediator->removeDevice(baseDevice);
            }
        }

        void mediateNewProperty(Property &property)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->newProperty((Property *)property);
#endif
                mediator->newProperty(property);
            }
        }

        void mediateUpdateProperty(Property property)
        {
            if (mediator)
            {
                mediator->updateProperty(property);
#if INDI_VERSION_MAJOR < 2
                switch (property.getType())
                {
                    case INDI_NUMBER:
                        mediator->newNumber(property.getNumber());
                        break;
                    case INDI_SWITCH:
                        mediator->newSwitch(property.getSwitch());
                        break;
                    case INDI_TEXT:
                        mediator->newText(property.getText());
                        break;
                    case INDI_LIGHT:
                        mediator->newLight(property.getLight());
                        break;
                    case INDI_BLOB:
                        for (auto &it : PropertySwitch(property))
                        {
                            mediator->newBLOB(&it);
                        }
                        break;
                    case INDI_UNKNOWN:
                        ;
                }
#endif
            }
        }

        void mediateRemoveProperty(Property &property)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->removeProperty((Property *)property);
#endif
                mediator->removeProperty(property);
            }
        }

        void mediateNewMessage(BaseDevice &baseDevice, int messageID)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->newMessage(&baseDevice, messageID);
#endif
                mediator->newMessage(baseDevice, messageID);
            }
        }

    public:
        BaseDevice self {make_shared_weak(this)}; // backward compatibile (for operators as pointer)
        std::string deviceName;
        BaseDevice::Properties pAll;
        std::map<std::string, std::function<void(INDI::Property)>> watchPropertyMap;
        LilXmlParser xmlParser;

        INDI::BaseMediator *mediator {nullptr};
        std::deque<std::string> messageLog;
        mutable std::mutex m_Lock;

        bool valid = true;
        uint16_t interfaceDescriptor {0};
};

}
