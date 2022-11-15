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
        BaseDevicePrivate(BaseDevice *parent);
        virtual ~BaseDevicePrivate();

        /** @brief Parse and store BLOB in the respective vector */
        int setBLOB(INDI::PropertyBlob &propertyBlob, const INDI::LilXmlElement &root, char *errmsg);

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
        void mediateNewDevice()
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->newDevice(parent);
#endif
                mediator->newDevice(*parent);
            }
        }

        void mediateRemoveDevice()
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->removeDevice(parent);
#endif
                mediator->removeDevice(*parent);
            }
        }

        void mediate(PropertyNumber property)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->newNumber(property.getNumber());
#endif
                mediator->newNumber(property);
            }
        }

        void mediate(PropertySwitch property)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->newSwitch(property.getSwitch());
#endif
                mediator->newSwitch(property);
            }
        }

        void mediate(PropertyText property)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->newText(property.getText());
#endif
                mediator->newText(property);
            }
        }

        void mediate(PropertyLight property)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->newLight(property.getLight());
#endif
                mediator->newLight(property);
            }
        }

        void mediate(IBLOB *blob)
        {
            if (mediator)
            {
                mediator->newBLOB(blob);
                // #PS: TODO - Blob requires complete refactoring,
                //             there is no certainty how long the data is kept.
            }
        }

        void mediateMessage(int messageID)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->newMessage(parent, messageID);
#endif
                mediator->newMessage(*parent, messageID);
            }
        }

        void mediateProperty(Property property)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->newProperty((Property *)property);
#endif
                mediator->newProperty(property);
            }
        }

        void mediateRemoveProperty(Property property)
        {
            if (mediator)
            {
#if INDI_VERSION_MAJOR < 2
                mediator->removeProperty((Property *)property);
#endif
                mediator->removeProperty(property);
            }
        }

    public:
        BaseDevice *parent;
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
