/*******************************************************************************
  Copyright(c) 2016 Jasem Mutlaq. All rights reserved.
  Copyright(c) 2022 Pawel Soja <kernel32.pl@gmail.com>

 INDI Qt Client

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


#include "indidevapi.h" // BLOBHandling
#include <vector>

namespace INDI
{
class BaseClientQtPrivate
{
    public:
        BaseClientQtPrivate(BaseClientQt *parent);

    public:
        typedef struct
        {
            std::string device;
            std::string property;
            BLOBHandling blobMode;
        } BLOBMode;

        BLOBMode *findBLOBMode(const std::string &device, const std::string &property);

        /** @brief Connect/Disconnect to INDI driver
            @param status If true, the client will attempt to turn on CONNECTION property within the driver (i.e. turn on the device).
             Otherwise, CONNECTION will be turned off.
            @param deviceName Name of the device to connect to.
        */
        void setDriverConnection(bool status, const char *deviceName);

        /**
         * @brief clear Clear devices and blob modes
         */
        void clear();

    public:
        BaseClientQt *parent;

        std::string cServer {"localhost"};
        uint32_t cPort      {7624};

        std::atomic_bool sConnected {false};

        bool verbose {false};

        uint32_t timeout_sec {3}, timeout_us {0};

        LilXML *lillp = nullptr; /* XML parser context */


        std::vector<INDI::BaseDevice *> cDevices;
        std::vector<std::string> cDeviceNames;
        std::vector<BLOBMode *> blobModes;
        std::map<std::string, std::set<std::string>> cWatchProperties;

        QTcpSocket client_socket;
};
}
