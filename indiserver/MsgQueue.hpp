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

#include "lilxml.h"
#include "Collectable.hpp"
#include "MsgChunckIterator.hpp"
#include "indicore/indidevapi.h"

#include <ev++.h>
#include <list>
#include <set>

class SerializedMsg;
class Msg;

class MsgQueue: public Collectable
{
        static constexpr unsigned maxFDPerMessage {16}; /* No more than 16 buffer attached to a message */
        static constexpr unsigned maxReadBufferLength {49152};
        static constexpr unsigned maxWriteBufferLength {49152};

        int rFd, wFd;
        LilXML * lp;         /* XML parsing context */
        ev::io   rio, wio;   /* Event loop io events */
        void ioCb(ev::io &watcher, int revents);

        // Update the status of FD read/write ability
        void updateIos();

        std::set<SerializedMsg*> readBlocker;     /* The message that block this queue */

        std::list<SerializedMsg*> msgq;           /* To send msg queue */
        std::list<int> incomingSharedBuffers; /* During reception, fds accumulate here */

        // Position in the head message
        MsgChunckIterator nsent;

        // Handle fifo or socket case
        size_t doRead(char * buff, size_t len);
        void readFromFd();

        /* write the next chunk of the current message in the queue to the given
         * client. pop message from queue when complete and free the message if we are
         * the last one to use it. shut down this client if trouble.
         */
        void writeToFd();

    protected:
        bool useSharedBuffer;
        int getRFd() const
        {
            return rFd;
        }
        int getWFd() const
        {
            return wFd;
        }

        /* print key attributes and values of the given xml to stderr. */
        void traceMsg(const std::string &log, XMLEle *root);

        /* Close the connection. (May be restarted later depending on driver logic) */
        virtual void close() = 0;

        /* Close the writing part of the connection. By default, shutdown the write part, but keep on reading. May delete this */
        virtual void closeWritePart();

        /* Handle a message. root will be freed by caller. fds of buffers will be closed, unless set to -1 */
        virtual void onMessage(XMLEle *root, std::list<int> &sharedBuffers) = 0;

        /* convert the string value of enableBLOB to our B_ state value.
         * no change if unrecognized
         */
        static void crackBLOB(const char *enableBLOB, BLOBHandling *bp);

        MsgQueue(bool useSharedBuffer);
    public:
        virtual ~MsgQueue();

        void pushMsg(Msg * msg);

        /* return storage size of all Msqs on the given q */
        unsigned long msgQSize() const;

        SerializedMsg * headMsg() const;
        void consumeHeadMsg();

        /* Remove all messages from queue */
        void clearMsgQueue();

        void messageMayHaveProgressed(const SerializedMsg * msg);

        void setFds(int rFd, int wFd);

        virtual bool acceptSharedBuffers() const
        {
            return useSharedBuffer;
        }

        virtual void log(const std::string &log) const;
};