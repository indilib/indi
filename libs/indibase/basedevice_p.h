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

class BaseDevicePrivate
{
    public:
        BaseDevicePrivate();
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

    public:
        std::string deviceName;
        BaseDevice::Properties pAll;
        std::map<std::string, std::function<void(INDI::Property)>> watchPropertyMap;
        LilXmlParser xmlParser;

        INDI::BaseMediator *mediator {nullptr};
        std::deque<std::string> messageLog;
        mutable std::mutex m_Lock;
};

}
