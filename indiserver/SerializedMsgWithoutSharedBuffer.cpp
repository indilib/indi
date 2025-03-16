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
#include "SerializedMsgWithoutSharedBuffer.hpp"
#include "Utils.hpp"
#include "Msg.hpp"
#include "MsgChunck.hpp"
#include "base64.h"

#include <unordered_map>

SerializedMsgWithoutSharedBuffer::SerializedMsgWithoutSharedBuffer(Msg * parent): SerializedMsg(parent)
{
}

SerializedMsgWithoutSharedBuffer::~SerializedMsgWithoutSharedBuffer()
{
}

bool SerializedMsgWithoutSharedBuffer::generateContentAsync() const
{
    return owner->hasInlineBlobs || owner->hasSharedBufferBlobs;
}

void SerializedMsgWithoutSharedBuffer::generateContent()
{
    // Convert every shared buffer into an inline base64
    auto xmlContent = owner->xmlContent;

    std::vector<XMLEle*> cdata;
    // Every cdata will have either sharedBuffer or sharedCData
    std::vector<int> sharedBuffers;
    std::vector<ssize_t> xmlSizes;
    std::vector<XMLEle *> sharedCData;

    std::unordered_map<XMLEle*, XMLEle*> replacement;

    int ownerSharedBufferId = 0;

    // Identify shared buffer blob to base64 them
    // Identify base64 blob to avoid copying them (we'll copy the cdata)
    for(auto blobContent : findBlobElements(xmlContent))
    {
        std::string attached = findXMLAttValu(blobContent, "attached");

        if (attached != "true" && pcdatalenXMLEle(blobContent) == 0)
        {
            continue;
        }

        XMLEle * clone = shallowCloneXMLEle(blobContent);
        rmXMLAtt(clone, "attached");
        editXMLEle(clone, "_");

        replacement[blobContent] = clone;
        cdata.push_back(clone);

        if (attached == "true")
        {
            rmXMLAtt(clone, "enclen");

            // Get the size if present
            ssize_t size = -1;
            parseBlobSize(clone, size);

            // FIXME: we could add enclen there

            // Put something here for later replacement
            sharedBuffers.push_back(owner->sharedBuffers[ownerSharedBufferId++]);
            xmlSizes.push_back(size);
            sharedCData.push_back(nullptr);
        }
        else
        {
            sharedBuffers.push_back(-1);
            xmlSizes.push_back(-1);
            sharedCData.push_back(blobContent);
        }
    }

    if (replacement.empty())
    {
        // Just print the content as is...

        char * model = (char*)malloc(sprlXMLEle(xmlContent, 0) + 1);
        int modelSize = sprXMLEle(model, xmlContent, 0);

        ownBuffers.push_back(model);

        async_pushChunck(MsgChunck(model, modelSize));

        // FIXME: lower requirements asap... how to do that ?
        // requirements.xml = false;
        // requirements.sharedBuffers.clear();

    }
    else
    {
        // Create a replacement that shares original CData buffers
        xmlContent = cloneXMLEleWithReplacementMap(xmlContent, replacement);

        std::vector<size_t> modelCdataOffset(cdata.size());

        char * model = (char*)malloc(sprlXMLEle(xmlContent, 0) + 1);
        int modelSize = sprXMLEle(model, xmlContent, 0);

        ownBuffers.push_back(model);

        // Get the element offset
        for(size_t i = 0; i < cdata.size(); ++i)
        {
            modelCdataOffset[i] = sprXMLCDataOffset(xmlContent, cdata[i], 0);
        }
        delXMLEle(xmlContent);

        std::vector<int> fds(cdata.size());
        std::vector<void*> blobs(cdata.size());
        std::vector<size_t> sizes(cdata.size());
        std::vector<size_t> attachedSizes(cdata.size());

        // Attach all blobs
        for(size_t i = 0; i < cdata.size(); ++i)
        {
            if (sharedBuffers[i] != -1)
            {
                fds[i] = sharedBuffers[i];

                size_t dataSize;
                blobs[i] = attachSharedBuffer(fds[i], dataSize);
                attachedSizes[i] = dataSize;

                // check dataSize is compatible with the blob element's size
                // It's mandatory for attached blob to give their size
                if (xmlSizes[i] != -1 && ((size_t)xmlSizes[i]) <= dataSize)
                {
                    dataSize = xmlSizes[i];
                }
                sizes[i] = dataSize;
            }
            else
            {
                fds[i] = -1;
            }
        }

        // Copy from model or blob (streaming base64 encode)
        int modelOffset = 0;
        for(size_t i = 0; i < cdata.size(); ++i)
        {
            int cdataOffset = modelCdataOffset[i];
            if (cdataOffset > modelOffset)
            {
                async_pushChunck(MsgChunck(model + modelOffset, cdataOffset - modelOffset));
            }
            // Skip the dummy cdata completely
            modelOffset = cdataOffset + 1;

            // Perform inplace base64
            // FIXME: could be streamed/splitted

            if (fds[i] != -1)
            {
                // Add a binary chunck. This needs base64 convertion
                // FIXME: the size here should be the size of the blob element
                unsigned long buffSze = sizes[i];
                const unsigned char* src = (const unsigned char*)blobs[i];

                // split here in smaller chunks for faster startup
                // This allow starting write before the whole blob is converted
                while(buffSze > 0)
                {
                    // We need a block size multiple of 24 bits (3 bytes)
                    unsigned long sze = buffSze > 3 * 16384 ? 3 * 16384 : buffSze;

                    char* buffer = (char*) malloc(4 * sze / 3 + 4);
                    ownBuffers.push_back(buffer);
                    int base64Count = to64frombits_s((unsigned char*)buffer, src, sze, (4 * sze / 3 + 4));

                    async_pushChunck(MsgChunck(buffer, base64Count));

                    buffSze -= sze;
                    src += sze;
                }


                // Dettach blobs ASAP
                dettachSharedBuffer(fds[i], blobs[i], attachedSizes[i]);

                // requirements.sharedBuffers.erase(fds[i]);
            }
            else
            {
                // Add an already ready cdata section

                auto len = pcdatalenXMLEle(sharedCData[i]);
                auto data = pcdataXMLEle(sharedCData[i]);
                async_pushChunck(MsgChunck(data, len));
            }
        }

        if (modelOffset < modelSize)
        {
            async_pushChunck(MsgChunck(model + modelOffset, modelSize - modelOffset));
            modelOffset = modelSize;
        }
    }
    async_done();
}
