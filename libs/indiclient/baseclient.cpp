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

#include "abstractbaseclient.h"
#include "abstractbaseclient_p.h"

#include "baseclient.h"
#include "baseclient_p.h"

#define MAXINDIBUF 49152
#define DISCONNECTION_DELAY_US 500000
#define MAXFD_PER_MESSAGE 16 /* No more than 16 buffer attached to a message */

#ifdef ENABLE_INDI_SHARED_MEMORY
# include "sharedblob_parse.h"
# include <sys/socket.h>
# include <sys/un.h>
# include <unistd.h>
#endif

#ifndef __linux__
# include <fcntl.h>
#endif

namespace INDI
{

//ClientSharedBlobs
#ifdef ENABLE_INDI_SHARED_MEMORY
ClientSharedBlobs::Blobs::~Blobs()
{
    releaseBlobUids(*this);
}

void ClientSharedBlobs::enableDirectBlobAccess(const char * dev, const char * prop)
{
    if (dev == nullptr || !dev[0])
    {
        directBlobAccess[""].insert("");
        return;
    }
    if (prop == nullptr || !prop[0])
    {
        directBlobAccess[dev].insert("");
    }
    else
    {
        directBlobAccess[dev].insert(prop);
    }
}

void ClientSharedBlobs::disableDirectBlobAccess()
{
    directBlobAccess.clear();
}

bool ClientSharedBlobs::parseAttachedBlobs(const INDI::LilXmlElement &root, ClientSharedBlobs::Blobs &blobs)
{
    // parse all elements in root that are attached.
    // Create for each a new GUID and associate it in a global map
    // modify the xml to add an attribute with the guid
    for (auto &blobContent : root.getElementsByTagName("oneBLOB"))
    {
        auto attached = blobContent.getAttribute("attached");

        if (attached.toString() != "true")
            continue;

        auto device = root.getAttribute("dev");
        auto name   = root.getAttribute("name");

        blobContent.removeAttribute("attached");
        blobContent.removeAttribute("enclen");

        if (incomingSharedBuffers.empty())
        {
            return false;
        }

        int fd = *incomingSharedBuffers.begin();
        incomingSharedBuffers.pop_front();

        auto id = allocateBlobUid(fd);
        blobs.push_back(id);

        // Put something here for later replacement
        blobContent.removeAttribute("attached-data-id");
        blobContent.removeAttribute("attachment-direct");
        blobContent.addAttribute("attached-data-id", id.c_str());
        if (isDirectBlobAccess(device.toString(), name.toString()))
        {
            // If client support read-only shared blob, mark it here
            blobContent.addAttribute("attachment-direct",  "true");
        }
    }
    return true;
}

bool ClientSharedBlobs::hasDirectBlobAccessEntry(const std::map<std::string, std::set<std::string>> &directBlobAccess,
        const std::string &dev, const std::string &prop)
{
    auto devAccess = directBlobAccess.find(dev);
    if (devAccess == directBlobAccess.end())
    {
        return false;
    }
    return devAccess->second.find(prop) != devAccess->second.end();
}

bool ClientSharedBlobs::isDirectBlobAccess(const std::string &dev, const std::string &prop) const
{
    return hasDirectBlobAccessEntry(directBlobAccess, "", "")
           || hasDirectBlobAccessEntry(directBlobAccess, dev, "")
           || hasDirectBlobAccessEntry(directBlobAccess, dev, prop);
}

void ClientSharedBlobs::addIncomingSharedBuffer(int fd)
{
    incomingSharedBuffers.push_back(fd);
}

void ClientSharedBlobs::clear()
{
    for (int fd : incomingSharedBuffers)
    {
        ::close(fd);
    }
    incomingSharedBuffers.clear();
}

void TcpSocketSharedBlobs::readyRead()
{
    char buffer[MAXINDIBUF];

    struct msghdr msgh;
    struct iovec iov;

    int recvflag = MSG_DONTWAIT;
#ifdef __linux__
    recvflag |= MSG_CMSG_CLOEXEC;
#endif

    union
    {
        struct cmsghdr cmsgh;
        /* Space large enough to hold an 'int' */
        char control[CMSG_SPACE(MAXFD_PER_MESSAGE * sizeof(int))];
    } control_un;

    iov.iov_base = buffer;
    iov.iov_len = MAXINDIBUF;

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_flags = 0;
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    int n = recvmsg(reinterpret_cast<ptrdiff_t>(socketDescriptor()), &msgh, recvflag);

    if (n >= 0)
    {
        for (struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msgh); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msgh, cmsg))
        {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
            {
                int fdCount = 0;
                while(cmsg->cmsg_len >= CMSG_LEN((fdCount + 1) * sizeof(int)))
                {
                    fdCount++;
                }
                //IDLog("Received %d fds\n", fdCount);
                int * fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
                for(int i = 0; i < fdCount; ++i)
                {
                    int fd = fds[i];
                    //IDLog("Received fd %d\n", fd);
#ifndef __linux__
                    fcntl(fds[i], F_SETFD, FD_CLOEXEC);
#endif
                    sharedBlobs.addIncomingSharedBuffer(fd);
                }
            }
            else
            {
                IDLog("Ignoring ancillary data level %d, type %d\n", cmsg->cmsg_level, cmsg->cmsg_type);
            }
        }
    }

    if (n <= 0)
    {
        setSocketError(TcpSocket::ConnectionRefusedError);
        return;
    }

    emitData(buffer, n);
}
#endif
// BaseClientPrivate

BaseClientPrivate::BaseClientPrivate(BaseClient *parent)
    : AbstractBaseClientPrivate(parent)
{
    clientSocket.onData([this](const char *data, size_t size)
    {
        char msg[MAXRBUF];
        auto documents = xmlParser.parseChunk(data, size);

        if (documents.size() == 0)
        {
            if (xmlParser.hasErrorMessage())
            {
                IDLog("Bad XML from %s/%d: %s\n%.*s\n", cServer.c_str(), cPort, xmlParser.errorMessage(), int(size), data);
            }
            return;
        }

        for (const auto &doc : documents)
        {
            LilXmlElement root = doc.root();

            if (verbose)
                root.print(stderr, 0);

#ifdef ENABLE_INDI_SHARED_MEMORY
            ClientSharedBlobs::Blobs blobs;

            if (!clientSocket.sharedBlobs.parseAttachedBlobs(root, blobs))
            {
                IDLog("Missing attachment from %s/%d\n", cServer.c_str(), cPort);
                return;
            }
#endif

            int err_code = dispatchCommand(root, msg);

            if (err_code < 0)
            {
                // Silenty ignore property duplication errors
                if (err_code != INDI_PROPERTY_DUPLICATED)
                {
                    IDLog("Dispatch command error(%d): %s\n", err_code, msg);
                    root.print(stderr, 0);
                }
            }
        }
    });

    clientSocket.onErrorOccurred([this] (TcpSocket::SocketError)
    {
        this->parent->serverDisconnected(exitCode);
        clear();
        watchDevice.unwatchDevices();
    });
}

BaseClientPrivate::~BaseClientPrivate()
{ }

size_t BaseClientPrivate::sendData(const void *data, size_t size)
{
    return clientSocket.write(static_cast<const char *>(data), size);
}

// BaseClient

BaseClient::BaseClient()
    : AbstractBaseClient(std::unique_ptr<AbstractBaseClientPrivate>(new BaseClientPrivate(this)))
{ }

BaseClient::~BaseClient()
{
    D_PTR(BaseClient);
    d->clear();
}

bool BaseClientPrivate::connectToHostAndWait(std::string hostname, unsigned short port)
{
    if (hostname == "localhost:")
    {
        hostname = "localhost:/tmp/indiserver";
    }
    clientSocket.connectToHost(hostname, port);
    return clientSocket.waitForConnected(timeout_sec * 1000 + timeout_us / 1000);
}

bool BaseClient::connectServer()
{
    D_PTR(BaseClient);

    if (d->sConnected.exchange(true) == true)
    {
        IDLog("INDI::BaseClient::connectServer: Already connected.\n");
        return false;
    }

    d->exitCode = -1;

    IDLog("INDI::BaseClient::connectServer: creating new connection...\n");

#ifndef _WINDOWS
    // System with unix support automatically connect over unix domain
    if (d->cServer != "localhost" || d->cServer != "127.0.0.1" || d->connectToHostAndWait("localhost:", d->cPort) == false)
#endif
    {
        if (d->connectToHostAndWait(d->cServer, d->cPort) == false)
        {
            d->sConnected = false;
            return false;
        }
    }

    d->clear();

    serverConnected();

    d->userIoGetProperties();

    return true;
}

bool BaseClient::disconnectServer(int exit_code)
{
    D_PTR(BaseClient);

    if (d->sConnected.exchange(false) == false)
    {
        IDLog("INDI::BaseClient::disconnectServer: Already disconnected.\n");
        return false;
    }

    d->exitCode = exit_code;
    d->clientSocket.disconnectFromHost();
    bool ret = d->clientSocket.waitForDisconnected();
    // same behavior as in `BaseClientQt::disconnectServer`
    serverDisconnected(exit_code);
    return ret;
}

void BaseClient::enableDirectBlobAccess(const char * dev, const char * prop)
{
#ifdef ENABLE_INDI_SHARED_MEMORY
    D_PTR(BaseClient);
    d->clientSocket.sharedBlobs.enableDirectBlobAccess(dev, prop);
#else
    INDI_UNUSED(dev);
    INDI_UNUSED(prop);
#endif
}

}
