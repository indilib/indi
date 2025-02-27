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

#include <vector>
#include <list>
#include <set>

#include "lilxml.h"

class MsgQueue;
class SerializedMsg;
class SerializedMsgWithSharedBuffer;
class SerializedMsgWithoutSharedBuffer;

class Msg
{
        friend class SerializedMsg;
        friend class SerializedMsgWithSharedBuffer;
        friend class SerializedMsgWithoutSharedBuffer;
    private:
        // Present for sure until message queueing is doned. Prune asap then
        XMLEle * xmlContent;

        // Present until message was queued.
        MsgQueue * from;

        int queueSize;
        bool hasInlineBlobs;
        bool hasSharedBufferBlobs;

        std::vector<int> sharedBuffers; /* fds of shared buffer */

        // Convertion task and resultat of the task
        SerializedMsg* convertionToSharedBuffer;
        SerializedMsg* convertionToInline;

        SerializedMsg * buildConvertionToSharedBuffer();
        SerializedMsg * buildConvertionToInline();

        bool fetchBlobs(std::list<int> &incomingSharedBuffers);

        void releaseXmlContent();
        void releaseSharedBuffers(const std::set<int> &keep);

        // Remove resources that can be removed.
        // Will be called when queuingDone is true and for every change of status from convertionToXXX
        void prune();

        void releaseSerialization(SerializedMsg * form);

        ~Msg();

    public:
        /* Message will not be queued anymore. Release all possible resources, incl self */
        void queuingDone();

        Msg(MsgQueue * from, XMLEle * root);

        static Msg * fromXml(MsgQueue * from, XMLEle * root, std::list<int> &incomingSharedBuffers);

        /**
         * Handle multiple cases:
         *
         *  - inline => attached.
         * Exceptional. The inline is already in memory within xml. It must be converted to shared buffer async.
         * FIXME: The convertion should block the emitter.
         *
         *  - attached => attached
         * Default case. No convertion is required.
         *
         *  - inline => inline
         * Frequent on system not supporting attachment.
         *
         *  - attached => inline
         * Frequent. The convertion will be made during write. The convert/write must be offshored to a dedicated thread.
         *
         * The returned AsyncTask will be ready once "to" can write the message
         */
        SerializedMsg * serialize(MsgQueue * from);
};
