/*******************************************************************************
 Copyright(c) 2022 Ludovic Pollet. All rights reserved.

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

#include "sharedblob_parse.h"

#define INDI_SHARED_BLOB_SUPPORT
#include "sharedblob.h"

#include <map>
#include <sstream>
#include <mutex>
#include <cstdint>

#include <unistd.h>

namespace INDI
{

static std::mutex attachedBlobMutex;
static std::map<std::string, int> receivedFds;
static uint64_t idGenerator = rand();


std::string allocateBlobUid(int fd)
{
    std::lock_guard<std::mutex> lock(attachedBlobMutex);

    std::stringstream ss;
    ss << idGenerator++;

    std::string id = ss.str();

    receivedFds[id] = fd;
    return id;
}

void * attachBlobByUid(const std::string &identifier, size_t size)
{
    int fd;
    {
        std::lock_guard<std::mutex> lock(attachedBlobMutex);
        auto where = receivedFds.find(identifier);
        if (where == receivedFds.end())
        {
            return nullptr;
        }
        fd = where->second;
        receivedFds.erase(where);
    }

    return IDSharedBlobAttach(fd, size);
}

void releaseBlobUids(const std::vector<std::string> &blobs)
{
    std::vector<int> toDestroy;
    {
        std::lock_guard<std::mutex> lock(attachedBlobMutex);
        for(auto id : blobs)
        {
            auto idPos = receivedFds.find(id);
            if (idPos != receivedFds.end())
            {
                toDestroy.push_back(idPos->second);
                receivedFds.erase(idPos);
            }
        }
    }

    for(auto fd : toDestroy)
    {
        ::close(fd);
    }
}

}
