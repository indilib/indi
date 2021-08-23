/*******************************************************************************
  Copyright(c) 2011 Jasem Mutlaq. All rights reserved.

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

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "baseclient.h"
#include "basedevice.h"
#include "basedevice_p.h"

#include "indistandardproperty.h"
#include "base64.h"
#include "basedevice.h"
#include "locale_compat.h"

#include <cerrno>
#include <fcntl.h>
#include <cstdlib>
#include <stdarg.h>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <functional>
#include <assert.h>

#include "indiuserio.h"

#ifdef _WINDOWS
#include <ws2tcpip.h>
#include <windows.h>

#define net_read(x,y,z) recv(x,y,z,0)
#define net_write(x,y,z) send(x,(const char *)(y),z,0)
#define net_close closesocket

#pragma comment(lib, "Ws2_32.lib")
#else
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#define net_read read
#define net_write write
#define net_close close
#endif

#ifdef _MSC_VER
# define snprintf _snprintf
#endif

#define MAXINDIBUF 49152
#define DISCONNECTION_DELAY_US 500000
#define MAXFD_PER_MESSAGE 16 /* No more than 16 buffer attached to a message */

#include "baseclient_p.h"
#include <sstream>

namespace INDI
{

static std::mutex attachedBlobMutex;
static std::map<std::string, int> receivedFds;
static uint64_t idGenerator = rand();

static std::string declareReceivedFd(int fd) {
    std::stringstream ss;
    ss << idGenerator;

    std::string id = ss.str();

    std::lock_guard<std::mutex> lock(attachedBlobMutex);
    fprintf(stderr, "received fd: %d\n", fd);
    receivedFds[id] = fd;
    return id;
}

static std::vector<XMLEle *> findBlobElements(XMLEle * root) {
    std::vector<XMLEle *> result;
    for (auto ep = nextXMLEle(root, 1); ep; ep = nextXMLEle(root, 0))
    {
        if (strcmp(tagXMLEle(ep), "oneBLOB") == 0)
        {
            result.push_back(ep);
        }
    }
    return result;
}

bool BaseClientPrivate::parseAttachedBlobs(XMLEle *root, std::vector<std::string> & blobs)
{
    fprintf(stderr, "parseAttachedBlobs\n");
    // parse all elements in root that are attached.
    // Create for each a new GUID and associate it in a global map
    // modify the xml to add an attribute with the guid
    for(auto blobContent : findBlobElements(root)) {
        std::string attached = findXMLAttValu(blobContent, "attached");

        if (attached == "true") {
            fprintf(stderr, "found AttachedBlobs\n");
            rmXMLAtt(blobContent, "attached");
            rmXMLAtt(blobContent, "enclen");

            if (incomingSharedBuffers.empty()) {
                return false;
            }
            int fd = *incomingSharedBuffers.begin();
            incomingSharedBuffers.pop_front();
            fprintf(stderr, "AttachedBlobs id is %d\n", fd);

            auto id = declareReceivedFd(fd);
            blobs.push_back(id);

            // Put something here for later replacement
            rmXMLAtt(blobContent, "attached-data-id");
            addXMLAtt(blobContent, "attached-data-id", id.c_str());
        }
    }
    return true;
}

void BaseClientPrivate::flushBlobs(const std::vector<std::string> & blobs)
{
    std::vector<int> toDestroy;
    {
        std::lock_guard<std::mutex> lock(attachedBlobMutex);
        for(auto id : blobs) {
            auto idPos = receivedFds.find(id);
            if (idPos != receivedFds.end()) {
                toDestroy.push_back(idPos->second);
                receivedFds.erase(idPos);
            }
        }
    }

    for(auto fd : toDestroy) {
        ::close(fd);
    }
}

void * BaseDevicePrivate::accessAttachedBlob(const std::string & identifier, size_t size)
{
    int fd;
    {
        std::lock_guard<std::mutex> lock(attachedBlobMutex);
        auto where = receivedFds.find(identifier);
        if (where == receivedFds.end()) {
            return nullptr;
        }
        fd = where->second;
        receivedFds.erase(where);
    }

    return IDSharedBlobAttach(fd, size);
}

}