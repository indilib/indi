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

namespace INDI
{
class DefaultDevicePrivate: public BaseDevicePrivate
{
public:
    DefaultDevicePrivate();
    virtual ~DefaultDevicePrivate();

    bool isInit { false };
    bool isDebug { false };
    bool isSimulation { false };
    bool isDefaultConfigLoaded {false};
    bool isConfigLoading { false };

    uint16_t majorVersion { 1 };
    uint16_t minorVersion { 0 };
    uint16_t interfaceDescriptor { 0 };

    WidgetView<ISwitch> DebugS[2];
    WidgetView<ISwitch> SimulationS[2];
    WidgetView<ISwitch> ConfigProcessS[4];
    WidgetView<ISwitch> ConnectionS[2];
    WidgetView<INumber> PollPeriodN[1];

    PropertyView<ISwitch> DebugSP;
    PropertyView<ISwitch> SimulationSP;
    PropertyView<ISwitch> ConfigProcessSP;
    PropertyView<ISwitch> ConnectionSP;
    PropertyView<INumber> PollPeriodNP;

    WidgetView<IText> DriverInfoT[4] {};
    PropertyView<IText> DriverInfoTP;

    // Connection modes
    WidgetView<ISwitch> *ConnectionModeS = nullptr;
    PropertyView<ISwitch> ConnectionModeSP;

    std::vector<Connection::Interface *> connections;
    Connection::Interface *activeConnection = nullptr;

    /**
     * @brief pollingPeriod Period in milliseconds to call TimerHit(). Default 1000 ms
     */
    uint32_t pollingPeriod = 1000;

    bool defineDynamicProperties {true};
    bool deleteDynamicProperties {true};
};

}
