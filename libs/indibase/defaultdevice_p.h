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
    DefaultDevicePrivate()
    {
        memset(&ConnectionModeSP, 0, sizeof(ConnectionModeSP));
    }

    bool isInit { false };
    bool isDebug { false };
    bool isSimulation { false };
    bool isDefaultConfigLoaded {false};
    bool isConfigLoading { false };

    uint16_t majorVersion { 1 };
    uint16_t minorVersion { 0 };
    uint16_t interfaceDescriptor { 0 };

    ISwitch DebugS[2];
    ISwitch SimulationS[2];
    ISwitch ConfigProcessS[4];
    ISwitch ConnectionS[2];
    INumber PollPeriodN[1];

    ISwitchVectorProperty DebugSP;
    ISwitchVectorProperty SimulationSP;
    ISwitchVectorProperty ConfigProcessSP;
    ISwitchVectorProperty ConnectionSP;
    INumberVectorProperty PollPeriodNP;

    IText DriverInfoT[4] {};
    ITextVectorProperty DriverInfoTP;

    // Connection modes
    ISwitch *ConnectionModeS = nullptr;
    ISwitchVectorProperty ConnectionModeSP;

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
