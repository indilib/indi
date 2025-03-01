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
#include "SerializedMsgWithSharedBuffer.hpp"

#include "MsgChunck.hpp"
#include "Utils.hpp"
#include "Msg.hpp"
#include "sharedblob.h"
#include "base64.h"

#include <unistd.h>

bool SerializedMsgWithSharedBuffer::detectInlineBlobs()
{
    for(auto blobContent : findBlobElements(owner->xmlContent))
    {
        // C'est pas trivial, dans ce cas, car il faut les réattacher
        std::string attached = findXMLAttValu(blobContent, "attached");
        if (attached != "true")
        {
            return true;
        }
    }
    return false;
}

bool SerializedMsgWithSharedBuffer::generateContentAsync() const
{
    return owner->hasInlineBlobs;
}

void SerializedMsgWithSharedBuffer::generateContent()
{
    // Convert every inline base64 blob from xml into an attached buffer
    auto xmlContent = owner->xmlContent;

    std::vector<int> sharedBuffers = owner->sharedBuffers;

    std::unordered_map<XMLEle*, XMLEle*> replacement;
    int blobPos = 0;
    for(auto blobContent : findBlobElements(owner->xmlContent))
    {
        if (!pcdatalenXMLEle(blobContent))
        {
            continue;
        }
        std::string attached = findXMLAttValu(blobContent, "attached");
        if (attached != "true")
        {
            // We need to replace.
            XMLEle * clone = shallowCloneXMLEle(blobContent);
            rmXMLAtt(clone, "enclen");
            rmXMLAtt(clone, "attached");
            addXMLAtt(clone, "attached", "true");

            replacement[blobContent] = clone;

            int base64datalen = pcdatalenXMLEle(blobContent);
            char * base64data = pcdataXMLEle(blobContent);
            // Shall we really trust the size here ?

            ssize_t size;
            if (!parseBlobSize(blobContent, size))
            {
                log("Missing size value for blob");
                size = 1;
            }

            void * blob = IDSharedBlobAlloc(size);
            if (blob == nullptr)
            {
                log(fmt("Unable to allocate shared buffer of size %d : %s\n", size, strerror(errno)));
                ::exit(1);
            }
            log(fmt("Blob allocated at %p\n", blob));

            int actualLen = from64tobits_fast((char*)blob, base64data, base64datalen);

            if (actualLen != size)
            {
                // FIXME: WTF ? at least prevent overflow ???
                log(fmt("Blob size mismatch after base64dec: %lld vs %lld\n", (long long int)actualLen, (long long int)size));
            }

            int newFd = IDSharedBlobGetFd(blob);
            ownSharedBuffers.insert(newFd);

            IDSharedBlobDettach(blob);

            sharedBuffers.insert(sharedBuffers.begin() + blobPos, newFd);
        }
        blobPos++;
    }

    if (!replacement.empty())
    {
        // Work on a copy --- but we don't want to copy the blob !!!
        xmlContent = cloneXMLEleWithReplacementMap(xmlContent, replacement);
    }

    // Now create a Chunk from xmlContent
    MsgChunck chunck;

    chunck.content = (char*)malloc(sprlXMLEle(xmlContent, 0) + 1);
    ownBuffers.push_back(chunck.content);
    chunck.contentLength = sprXMLEle(chunck.content, xmlContent, 0);
    chunck.sharedBufferIdsToAttach = sharedBuffers;

    async_pushChunck(chunck);

    if (!replacement.empty())
    {
        delXMLEle(xmlContent);
    }
    async_done();
}

SerializedMsgWithSharedBuffer::SerializedMsgWithSharedBuffer(Msg * parent): SerializedMsg(parent), ownSharedBuffers()
{
}

SerializedMsgWithSharedBuffer::~SerializedMsgWithSharedBuffer()
{
    for(auto id : ownSharedBuffers)
    {
        close(id);
    }
}