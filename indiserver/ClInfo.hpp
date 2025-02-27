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

#include "indicore/indidevapi.h"
#include "MsgQueue.hpp"
#include "lilxml.h"

class DvrInfo;
class Property;

/* info for each connected client */
class ClInfo: public MsgQueue
{
    protected:
        /* send message to each appropriate driver.
         * also send all newXXX() to all other interested clients.
         */
        virtual void onMessage(XMLEle *root, std::list<int> &sharedBuffers);

        /* Update the client property BLOB handling policy */
        void crackBLOBHandling(const std::string &dev, const std::string &name, const char *enableBLOB);

        /* close down the given client */
        virtual void close();

    public:
        std::list<Property*> props;     /* props we want */
        int allprops = 0;               /* saw getProperties w/o device */
        BLOBHandling blob = B_NEVER;    /* when to send setBLOBs */

        ClInfo(bool useSharedBuffer);
        virtual ~ClInfo();

        /* return 0 if cp may be interested in dev/name else -1
         */
        int findDevice(const std::string &dev, const std::string &name) const;

        /* add the given device and property to the props[] list of client if new.
         */
        void addDevice(const std::string &dev, const std::string &name, int isblob);

        virtual void log(const std::string &log) const;

        /* put Msg mp on queue of each chained server client, except notme.
         */
        static void q2Servers(DvrInfo *me, Msg *mp, XMLEle *root);

        /* put Msg mp on queue of each client interested in dev/name, except notme.
         * if BLOB always honor current mode.
         */
        static void q2Clients(ClInfo *notme, int isblob, const std::string &dev, const std::string &name, Msg *mp, XMLEle *root);

        /* Reference to all active clients */
        static ConcurrentSet<ClInfo> clients;
};