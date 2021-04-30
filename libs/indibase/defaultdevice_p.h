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

#include "basedevice_p.h"
#include "defaultdevice.h"

#include <cstring>
#include <list>
#include <mutex>

#include "indipropertyswitch.h"
#include "indipropertynumber.h"
#include "indipropertytext.h"

namespace INDI
{
class DefaultDevicePrivate: public BaseDevicePrivate
{
    public:
        DefaultDevicePrivate(DefaultDevice *defaultDevice);
        virtual ~DefaultDevicePrivate();

        DefaultDevice *defaultDevice;

        bool isInit { false };
        bool isDebug { false };
        bool isSimulation { false };
        bool isDefaultConfigLoaded {false};
        bool isConfigLoading { false };

        uint16_t majorVersion { 1 };
        uint16_t minorVersion { 0 };
        uint16_t interfaceDescriptor { 0 };
        int m_ConfigConnectionMode {-1};

        PropertySwitch SimulationSP     { 2 };
        PropertySwitch DebugSP          { 2 };
        PropertySwitch ConfigProcessSP  { 4 };
        PropertySwitch ConnectionSP     { 2 };
        PropertyNumber PollPeriodNP     { 1 };
        PropertyText   DriverInfoTP     { 4 };
        PropertySwitch ConnectionModeSP { 0 }; // dynamic count of switches

        std::vector<Connection::Interface *> connections;
        Connection::Interface *activeConnection = nullptr;

        /**
         * @brief pollingPeriod Period in milliseconds to call TimerHit(). Default 1000 ms
         */
        uint32_t pollingPeriod = 1000;

        bool defineDynamicProperties {true};
        bool deleteDynamicProperties {true};

    public:
        static std::list<DefaultDevicePrivate*> devices;
        static std::recursive_mutex             devicesLock;
};

}
