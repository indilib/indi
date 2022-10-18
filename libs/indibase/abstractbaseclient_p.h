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

#include "watchdeviceproperty.h"
#include "indidevapi.h"
#include "indiuserio.h"
#include "indililxml.h"

#include <atomic>
#include <string>
#include <map>
#include <set>

namespace INDI
{

struct BLOBMode
{
    std::string device;
    std::string property;
    BLOBHandling blobMode;
};

class AbstractBaseClient;
class AbstractBaseClientPrivate
{
    public:
        AbstractBaseClientPrivate(AbstractBaseClient *parent);
        virtual ~AbstractBaseClientPrivate() = default;

    public:
        virtual size_t sendData(const void *data, size_t size) = 0;

    public:
        void clear();

    public:
        /** @brief Dispatch command received from INDI server to respective devices handled by the client */
        int dispatchCommand(const INDI::LilXmlElement &root, char *errmsg);

        /** @brief Remove device */
        int deleteDevice(const char *devName, char *errmsg);

        /** @brief Delete property command */
        int delPropertyCmd(const INDI::LilXmlElement &root, char *errmsg);

        /**  Process messages */
        int messageCmd(const INDI::LilXmlElement &root, char *errmsg);

    public:
        void userIoGetProperties();

    public:
        /** @brief Connect/Disconnect to INDI driver
            @param status If true, the client will attempt to turn on CONNECTION property within the driver (i.e. turn on the device).
             Otherwise, CONNECTION will be turned off.
            @param deviceName Name of the device to connect to.
        */
        void setDriverConnection(bool status, const char *deviceName);

    public:
        BLOBMode *findBLOBMode(const std::string &device, const std::string &property);

    public:
        bool isDirectBlobAccess(const std::string &dev, const std::string &prop) const;

    public:
        AbstractBaseClient *parent;

        std::list<BLOBMode> blobModes;
        std::map<std::string, std::set<std::string>> directBlobAccess;

        std::string cServer {"localhost"};
        uint32_t cPort      {7624};

        std::atomic_bool sConnected {false};

        bool verbose {false};

        uint32_t timeout_sec {3}, timeout_us {0};

        WatchDeviceProperty watchDevice;

        static userio io;
};

}
