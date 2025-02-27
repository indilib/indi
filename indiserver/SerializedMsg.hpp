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

#include "SerializationRequirement.hpp"

#include <mutex>
#include <vector>
#include <list>
#include <ev++.h>

class Msg;
class MsgChunck;
class MsgChunckIterator;
class MsgQueue;

enum class SerializationStatus { pending, running, canceling, terminated };

class SerializedMsg
{
        friend class Msg;
        friend class MsgChunckIterator;

        std::recursive_mutex lock;
        ev::async asyncProgress;

        // Start a thread for execution of asyncRun
        void async_start();
        void async_cancel();

        // Called within main loop when async task did some progress
        void async_progressed();

        // The requirements. Prior to starting, everything is required.
        SerializationRequirement requirements;

        void produce(bool sync);

    protected:
        // These methods are to be called from asyncRun
        bool async_canceled();
        void async_updateRequirement(const SerializationRequirement &n);
        void async_pushChunck(const MsgChunck &m);
        void async_done();

        // True if a producing thread is active
        bool isAsyncRunning();

        SerializationStatus asyncStatus;
        Msg * owner;

        MsgQueue* blockedProducer;

        std::set<MsgQueue *> awaiters;
    private:
        std::vector<MsgChunck> chuncks;

    protected:
        // Buffers malloced during asyncRun
        std::list<void*> ownBuffers;

        // This will notify awaiters and possibly release the owner
        void onDataReady();

        virtual bool generateContentAsync() const = 0;
        virtual void generateContent() = 0;

        void collectRequirements(SerializationRequirement &req);

        // The task will cancel itself if all owner release it
        void abort();

        // Make sure the given receiver will not be processed until this task complete
        // TODO : to implement + make sure the task start when it actually block something
        void blockReceiver(MsgQueue * toblock);

    public:
        SerializedMsg(Msg * parent);
        virtual ~SerializedMsg();

        // Calling requestContent will start production
        // Return true if some content is available
        bool requestContent(const MsgChunckIterator &position);

        // Return true if some content is available
        // It is possible to have 0 to send, meaning end was actually reached
        bool getContent(MsgChunckIterator &position, void * &data, ssize_t &nsend, std::vector<int> &sharedBuffers);

        void advance(MsgChunckIterator &position, ssize_t s);

        // When a queue is done with sending this message
        void release(MsgQueue * from);

        void addAwaiter(MsgQueue * awaiter);

        ssize_t queueSize();
};

