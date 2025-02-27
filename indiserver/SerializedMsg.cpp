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
#include "SerializedMsg.hpp"
#include "Msg.hpp"
#include "MsgChunck.hpp"
#include "MsgChunckIterator.hpp"
#include "MsgQueue.hpp"

#include <thread>

SerializedMsg::SerializedMsg(Msg * parent) : asyncProgress(), owner(parent), awaiters(), chuncks(), ownBuffers()
{
    blockedProducer = nullptr;
    // At first, everything is required.
    for(auto fd : parent->sharedBuffers)
    {
        if (fd != -1)
        {
            requirements.sharedBuffers.insert(fd);
        }
    }
    requirements.xml = true;
    asyncStatus = SerializationStatus::pending;
    asyncProgress.set<SerializedMsg, &SerializedMsg::async_progressed>(this);
}

// Delete occurs when no async task is running and no awaiters are left
SerializedMsg::~SerializedMsg()
{
    for(auto buff : ownBuffers)
    {
        free(buff);
    }
}

bool SerializedMsg::async_canceled()
{
    std::lock_guard<std::recursive_mutex> guard(lock);
    return asyncStatus == SerializationStatus::canceling;
}

void SerializedMsg::async_updateRequirement(const SerializationRequirement &req)
{
    std::lock_guard<std::recursive_mutex> guard(lock);
    if (this->requirements == req)
    {
        return;
    }
    this->requirements = req;
    asyncProgress.send();
}

void SerializedMsg::async_pushChunck(const MsgChunck &m)
{
    std::lock_guard<std::recursive_mutex> guard(lock);

    this->chuncks.push_back(m);
    asyncProgress.send();
}

void SerializedMsg::async_done()
{
    std::lock_guard<std::recursive_mutex> guard(lock);
    asyncStatus = SerializationStatus::terminated;
    asyncProgress.send();
}

void SerializedMsg::async_start()
{
    std::lock_guard<std::recursive_mutex> guard(lock);
    if (asyncStatus != SerializationStatus::pending)
    {
        return;
    }

    asyncStatus = SerializationStatus::running;
    if (generateContentAsync())
    {
        asyncProgress.start();

        std::thread t([this]()
        {
            generateContent();
        });
        t.detach();
    }
    else
    {
        generateContent();
    }
}

void SerializedMsg::async_progressed()
{
    std::lock_guard<std::recursive_mutex> guard(lock);

    if (asyncStatus == SerializationStatus::terminated)
    {
        // FIXME: unblock ?
        asyncProgress.stop();
    }

    // Update ios of awaiters
    for(auto awaiter : awaiters)
    {
        awaiter->messageMayHaveProgressed(this);
    }

    // Then prune
    owner->prune();
}

bool SerializedMsg::isAsyncRunning()
{
    std::lock_guard<std::recursive_mutex> guard(lock);

    return (asyncStatus == SerializationStatus::running) || (asyncStatus == SerializationStatus::canceling);
}


bool SerializedMsg::requestContent(const MsgChunckIterator &position)
{
    std::lock_guard<std::recursive_mutex> guard(lock);

    if (asyncStatus == SerializationStatus::pending)
    {
        async_start();
    }

    if (asyncStatus == SerializationStatus::terminated)
    {
        return true;
    }

    // Not reached the last chunck
    if (position.chunckId < chuncks.size())
    {
        return true;
    }

    return false;
}

bool SerializedMsg::getContent(MsgChunckIterator &from, void* &data, ssize_t &size,
                               std::vector<int, std::allocator<int> > &sharedBuffers)
{
    std::lock_guard<std::recursive_mutex> guard(lock);

    if (asyncStatus != SerializationStatus::terminated && from.chunckId >= chuncks.size())
    {
        // Not ready yet
        return false;
    }

    if (from.chunckId == chuncks.size())
    {
        // Done
        data = 0;
        size = 0;
        from.endReached = true;
        return true;
    }

    const MsgChunck &ck = chuncks[from.chunckId];

    if (from.chunckOffset == 0)
    {
        sharedBuffers = ck.sharedBufferIdsToAttach;
    }
    else
    {
        sharedBuffers.clear();
    }

    data = ck.content + from.chunckOffset;
    size = ck.contentLength - from.chunckOffset;
    return true;
}

void SerializedMsg::advance(MsgChunckIterator &iter, ssize_t s)
{
    std::lock_guard<std::recursive_mutex> guard(lock);

    MsgChunck &cur = chuncks[iter.chunckId];
    iter.chunckOffset += s;
    if (iter.chunckOffset >= cur.contentLength)
    {
        iter.chunckId ++ ;
        iter.chunckOffset = 0;
        if (iter.chunckId >= chuncks.size() && asyncStatus == SerializationStatus::terminated)
        {
            iter.endReached = true;
        }
    }
}

void SerializedMsg::addAwaiter(MsgQueue * q)
{
    awaiters.insert(q);
}

void SerializedMsg::release(MsgQueue * q)
{
    awaiters.erase(q);
    if (awaiters.empty() && !isAsyncRunning())
    {
        owner->releaseSerialization(this);
    }
}

void SerializedMsg::collectRequirements(SerializationRequirement &sr)
{
    sr.add(requirements);
}

// This is called when a received message require additional // work, to avoid overflow
void SerializedMsg::blockReceiver(MsgQueue * receiver)
{
    // TODO : implement or remove
    (void) receiver;
}

ssize_t SerializedMsg::queueSize()
{
    return owner->queueSize;
}