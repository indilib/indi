/* INDI Server for protocol version 1.7.
 * Copyright (C) 2007 Elwood C. Downey <ecdowney@clearskyinstitute.com>
                 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>
                 2022 Ludovic Pollet
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

#include "MsgQueue.hpp"
#include "lilxml.h"

#include <list>
#include <set>
#include <string>

class Msg;
class Property;

/* info for each connected driver */
class DvrInfo: public MsgQueue
{
        /* add dev/name to this device's snooping list.
         * init with blob mode set to B_NEVER.
         */
        void addSDevice(const std::string &dev, const std::string &name);

    public:
        /* return Property if dp is this driver is snooping dev/name, else NULL.
         */
        Property *findSDevice(const std::string &dev, const std::string &name) const;

    protected:
        /* send message to each interested client
         */
        void onMessage(XMLEle *root, std::list<int> &sharedBuffers) override;

        /* override to kill driver that are not reachable anymore */
        void closeWritePart() override;


        /* Construct an instance that will start the same driver */
        DvrInfo(const DvrInfo &model);

    public:
        std::string name;               /* persistent name */

        std::set<std::string> dev;      /* device served by this driver */
        std::list<Property*>sprops;     /* props we snoop */
        int restarts;                   /* times process has been restarted */
        bool restart = true;            /* Restart on shutdown */

        DvrInfo(bool useSharedBuffer);
        virtual ~DvrInfo();

        bool isHandlingDevice(const std::string &dev) const;

        /* start the INDI driver process or connection.
         * exit if trouble.
         */
        virtual void start() = 0;

        /* close down the given driver and restart if set*/
        void close() override;

        /* Allocate an instance that will start the same driver */
        virtual DvrInfo * clone() const = 0;

        void log(const std::string &log) const override;

        virtual const std::string remoteServerUid() const = 0;

        /* put Msg mp on queue of each driver responsible for dev, or all drivers
         * if dev empty.
         */
        static void q2RDrivers(const std::string &dev, Msg *mp, XMLEle *root);

        /* put Msg mp on queue of each driver snooping dev/name.
         * if BLOB always honor current mode.
         */
        static void q2SDrivers(DvrInfo *me, int isblob, const std::string &dev, const std::string &name, Msg *mp, XMLEle *root);

        /* Reference to all active drivers */
        static ConcurrentSet<DvrInfo> drivers;

        // decoding of attached blobs from driver is not supported ATM. Be conservative here
        bool acceptSharedBuffers() const override
        {
            return false;
        }
};
