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
#include "Msg.hpp"
#include "MsgQueue.hpp"
#include "SerializationRequirement.hpp"
#include "SerializedMsg.hpp"
#include "SerializedMsgWithSharedBuffer.hpp"
#include "SerializedMsgWithoutSharedBuffer.hpp"
#include "Utils.hpp"

#include <string>
#include <assert.h>
#include <unistd.h>

Msg::Msg(MsgQueue * from, XMLEle * ele): sharedBuffers()
{
    this->from = from;
    xmlContent = ele;
    hasInlineBlobs = false;
    hasSharedBufferBlobs = false;

    convertionToSharedBuffer = nullptr;
    convertionToInline = nullptr;

    queueSize = sprlXMLEle(xmlContent, 0);
    for(auto blobContent : findBlobElements(xmlContent))
    {
        std::string attached = findXMLAttValu(blobContent, "attached");
        if (attached == "true")
        {
            hasSharedBufferBlobs = true;
        }
        else
        {
            hasInlineBlobs = true;
        }
    }
}

Msg::~Msg()
{
    // Assume convertionToSharedBlob and convertionToInlineBlob were already dropped
    assert(convertionToSharedBuffer == nullptr);
    assert(convertionToInline == nullptr);

    releaseXmlContent();
    releaseSharedBuffers(std::set<int>());
}

void Msg::releaseSerialization(SerializedMsg * msg)
{
    if (msg == convertionToSharedBuffer)
    {
        convertionToSharedBuffer = nullptr;
    }

    if (msg == convertionToInline)
    {
        convertionToInline = nullptr;
    }

    delete(msg);
    prune();
}

void Msg::releaseXmlContent()
{
    if (xmlContent != nullptr)
    {
        delXMLEle(xmlContent);
        xmlContent = nullptr;
    }
}

void Msg::releaseSharedBuffers(const std::set<int> &keep)
{
    for(size_t i = 0; i < sharedBuffers.size(); ++i)
    {
        auto fd = sharedBuffers[i];
        if (fd != -1 && keep.find(fd) == keep.end())
        {
            if (close(fd) == -1)
            {
                perror("Releasing shared buffer");
            }
            sharedBuffers[i] = -1;
        }
    }
}

void Msg::prune()
{
    // Collect resources required.
    SerializationRequirement req;
    if (convertionToSharedBuffer)
    {
        convertionToSharedBuffer->collectRequirements(req);
    }
    if (convertionToInline)
    {
        convertionToInline->collectRequirements(req);
    }
    // Free the resources.
    if (!req.xml)
    {
        releaseXmlContent();
    }

    releaseSharedBuffers(req.sharedBuffers);

    // Nobody cares anymore ?
    if (convertionToSharedBuffer == nullptr && convertionToInline == nullptr)
    {
        delete(this);
    }
}
/** Init a message from xml content & additional incoming buffers */
bool Msg::fetchBlobs(std::list<int> &incomingSharedBuffers)
{
    /* Consume every buffers */
    for(auto blobContent : findBlobElements(xmlContent))
    {
        ssize_t blobSize;
        if (!parseBlobSize(blobContent, blobSize))
        {
            log("Attached blob misses the size attribute");
            return false;
        }

        std::string attached = findXMLAttValu(blobContent, "attached");
        if (attached == "true")
        {
            if (incomingSharedBuffers.empty())
            {
                log("Missing shared buffer...\n");
                return false;
            }

            queueSize += blobSize;
            //log("Found one fd !\n");
            int fd = *incomingSharedBuffers.begin();
            incomingSharedBuffers.pop_front();

            sharedBuffers.push_back(fd);
        }
        else
        {
            // Check cdata length vs blobSize ?
        }
    }
    return true;
}

void Msg::queuingDone()
{
    prune();
}

Msg * Msg::fromXml(MsgQueue * from, XMLEle * root, std::list<int> &incomingSharedBuffers)
{
    Msg * m = new Msg(from, root);
    if (!m->fetchBlobs(incomingSharedBuffers))
    {
        delete(m);
        return nullptr;
    }
    return m;
}

SerializedMsg * Msg::buildConvertionToSharedBuffer()
{
    if (convertionToSharedBuffer)
    {
        return convertionToSharedBuffer;
    }

    convertionToSharedBuffer = new SerializedMsgWithSharedBuffer(this);
    if (hasInlineBlobs && from)
    {
        convertionToSharedBuffer->blockReceiver(from);
    }
    return convertionToSharedBuffer;
}

SerializedMsg * Msg::buildConvertionToInline()
{
    if (convertionToInline)
    {
        return convertionToInline;
    }

    return convertionToInline = new SerializedMsgWithoutSharedBuffer(this);
}

SerializedMsg * Msg::serialize(MsgQueue * to)
{
    if (hasSharedBufferBlobs || hasInlineBlobs)
    {
        if (to->acceptSharedBuffers())
        {
            return buildConvertionToSharedBuffer();
        }
        else
        {
            return buildConvertionToInline();
        }
    }
    else
    {
        // Just serialize using copy
        return buildConvertionToInline();
    }
}
