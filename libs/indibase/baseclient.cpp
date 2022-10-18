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

#include "indistandardproperty.h"
#include "base64.h"
#include "basedevice.h"
#include "sharedblob_parse.h"
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
#include "indililxml.h"

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
#include <signal.h>
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
#include "abstractbaseclient.h"
#include "abstractbaseclient_p.h"

namespace INDI
{

// BaseClientPrivate

BaseClientPrivate::BaseClientPrivate(BaseClient *parent)
    : AbstractBaseClientPrivate(parent)
{ }

BaseClientPrivate::~BaseClientPrivate()
{
    if (sConnected)
        disconnect(0);

    std::unique_lock<std::mutex> locker(sSocketBusy);
    if (!sSocketChanged.wait_for(locker, std::chrono::milliseconds(500), [this] { return sConnected == false; }))
    {
        IDLog("BaseClient::~BaseClient: Probability of detecting a deadlock.\n");
    }
}

size_t BaseClientPrivate::sendData(const void *data, size_t size)
{
    int ret;

    do
    {
        std::lock_guard<std::mutex> locker(sSocketBusy);
        if (sConnected == false)
            return 0;
        ret = net_write(sockfd, data, size);
    }
    while(ret == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));

    if (ret < 0)
    {
        disconnect(-1);
    }

    return std::max(ret, 0);
}

void BaseClientPrivate::sendString(const char *fmt, ...)
{
    char message[MAXRBUF];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(message, MAXRBUF, fmt, ap);
    va_end(ap);
    sendData(message, strlen(message));
}

#ifndef _WINDOWS

static void initUnixSocketAddr(const std::string &unixAddr, struct sockaddr_un &serv_addr_un, socklen_t &addrlen, bool bind)
{
    memset(&serv_addr_un, 0, sizeof(serv_addr_un));
    serv_addr_un.sun_family = AF_UNIX;

#ifdef __linux__
    INDI_UNUSED(bind);

    // Using abstract socket path to avoid filesystem boilerplate
    strncpy(serv_addr_un.sun_path + 1, unixAddr.c_str(), sizeof(serv_addr_un.sun_path) - 1);

    int len = offsetof(struct sockaddr_un, sun_path) + unixAddr.size() + 1;

    addrlen = len;
#else
    // Using filesystem socket path
    strncpy(serv_addr_un.sun_path, unixAddr.c_str(), sizeof(serv_addr_un.sun_path) - 1);

    int len = offsetof(struct sockaddr_un, sun_path) + unixAddr.size();

    if (bind)
    {
        unlink(unixAddr.c_str());
    }
#endif
    addrlen = len;
}

#endif

// Using this prefix for name allow specifying the unix socket path
static const char * unixDomainPrefix = "localhost:";

static const char * unixDefaultPath = "/tmp/indiserver";

bool BaseClientPrivate::establish(const std::string &cServer)
{
    struct sockaddr_un serv_addr_un;
    struct sockaddr_in serv_addr_in;
    const struct sockaddr *sockaddr;
    socklen_t addrlen;

    struct timeval ts;
    ts.tv_sec  = timeout_sec;
    ts.tv_usec = timeout_us;

    int ret;

    // Special handling for localhost: addresses
    // pos=0 limits the search to the prefix
    unixSocket = cServer.rfind(unixDomainPrefix, 0) == 0;
    std::string unixAddr;
    if (unixSocket)
    {
        unixAddr = cServer.substr(strlen(unixDomainPrefix));
    }

    if (unixSocket)
    {
#ifndef _WINDOWS
        if (unixAddr.empty())
        {
            unixAddr = unixDefaultPath;
        }

        initUnixSocketAddr(unixAddr, serv_addr_un, addrlen, false);

        sockaddr = (struct sockaddr *)&serv_addr_un;

        if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        {
            perror("socket");
            return false;
        }
#else
        IDLog("local domain not supported on windows");
        return false;
#endif
    }
    else
    {
        struct hostent *hp;

        /* lookup host address */
        hp = gethostbyname(cServer.c_str());
        if (!hp)
        {
            perror("gethostbyname");
            return false;
        }

        /* create a socket to the INDI server */
        (void)memset((char *)&serv_addr_in, 0, sizeof(serv_addr_in));
        serv_addr_in.sin_family      = AF_INET;
        serv_addr_in.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
        serv_addr_in.sin_port        = htons(cPort);

        sockaddr = (struct sockaddr *)&serv_addr_in;
        addrlen = sizeof(serv_addr_in);
#ifdef _WINDOWS
        if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
        {
            IDLog("Socket error: %d\n", WSAGetLastError());
            WSACleanup();
            return false;
        }
#else
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("socket");
            return false;
        }
#endif
    }

    /* set the socket in non-blocking */
    //set socket nonblocking flag
#ifdef _WINDOWS
    u_long iMode = 0;
    iResult = ioctlsocket(sockfd, FIONBIO, &iMode);
    if (iResult != NO_ERROR)
    {
        IDLog("ioctlsocket failed with error: %ld\n", iResult);
        net_close(sockfd);
        return false;
    }
#else
    int flags = 0;
    if ((flags = fcntl(sockfd, F_GETFL, 0)) < 0)
    {
        net_close(sockfd);
        return false;
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        net_close(sockfd);
        return false;
    }

    // Handle SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    //clear out descriptor sets for select
    //add socket to the descriptor sets
    fd_set rset, wset;
    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    wset = rset; //structure assignment okok

    /* connect */
    if ((ret = ::connect(sockfd, sockaddr, addrlen)) < 0)
    {
        if (errno != EINPROGRESS)
        {
            perror("connect");
            net_close(sockfd);
            return false;
        }
    }

    /* If it is connected, continue, otherwise wait */
    if (ret != 0)
    {
        //we are waiting for connect to complete now
        if ((ret = select(sockfd + 1, &rset, &wset, nullptr, &ts)) < 0)
        {
            net_close(sockfd);
            return false;
        }
        //we had a timeout
        if (ret == 0)
        {
#ifdef _WINDOWS
            IDLog("select timeout\n");
#else
            net_close(sockfd);

            errno = ETIMEDOUT;
            perror("select timeout");
#endif
            return false;
        }
    }

    /* we had a positivite return so a descriptor is ready */
#ifndef _WINDOWS
    int error     = 0;
    socklen_t len = sizeof(error);
    if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset))
    {
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        {
            perror("getsockopt");
            net_close(sockfd);
            return false;
        }
    }
    else
    {
        net_close(sockfd);
        return false;
    }

    /* check if we had a socket error */
    if (error)
    {
        errno = error;
        perror("socket");
        net_close(sockfd);
        return false;
    }
#endif
    return true;
}

bool BaseClientPrivate::disconnect(int exit_code)
{
    for(int fd : this->incomingSharedBuffers)
    {
        close(fd);
    }
    this->incomingSharedBuffers.clear();

    //IDLog("Server disconnected called\n");
    std::lock_guard<std::mutex> locker(sSocketBusy);
    if (sConnected == false)
    {
        IDLog("INDI::BaseClient::disconnectServer: Already disconnected.\n");
        return false;
    }
    sAboutToClose = true;
    sSocketChanged.notify_all();
#ifdef _WINDOWS
    net_close(sockfd); // close and wakeup 'select' function
    WSACleanup();
    sockfd = INVALID_SOCKET;
#else
    shutdown(sockfd, SHUT_RDWR); // no needed
    size_t c = 1;
    // wakeup 'select' function
    ssize_t ret = write(sendFd, &c, sizeof(c));
    if (ret != sizeof(c))
    {
        IDLog("INDI::BaseClient::disconnectServer: Error. The socket cannot be woken up.\n");
    }
#endif
    sExitCode = exit_code;
    return true;
}

void BaseClientPrivate::listenINDI()
{
    char buffer[MAXINDIBUF];
    char msg[MAXRBUF];
#ifdef _WINDOWS
    SOCKET maxfd = 0;
#else
    int maxfd = 0;
#endif
    fd_set rs;

    userIoGetProperties();

    FD_ZERO(&rs);

    FD_SET(sockfd, &rs);
    maxfd = std::max(maxfd, sockfd);

#ifndef _WINDOWS
    FD_SET(receiveFd, &rs);
    maxfd = std::max(maxfd, receiveFd);
#endif

    clear();

    INDI::LilXmlParser xmlParser;

    bool clientFatalError = false;

    /* read from server, exit if find all requested properties */
    while ((!sAboutToClose) && (!clientFatalError))
    {
        int n = select(maxfd + 1, &rs, nullptr, nullptr, nullptr);

        // Woken up by disconnectServer function.
        if (sAboutToClose == true)
        {
            break;
        }

        if (n < 0)
        {
            IDLog("INDI server %s/%d disconnected.\n", cServer.c_str(), cPort);
            break;
        }

        if (n == 0)
        {
            continue;
        }

        if (FD_ISSET(sockfd, &rs))
        {
#ifdef _WINDOWS
            n = recv(sockfd, buffer, MAXINDIBUF, 0);
#else
            // Use recvmsg for ancillary data
            struct msghdr msgh;
            struct iovec iov;

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

            int recvflag = MSG_DONTWAIT;
#ifdef __linux__
            recvflag |= MSG_CMSG_CLOEXEC;
#endif
            n = recvmsg(sockfd, &msgh, recvflag);

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
                            incomingSharedBuffers.push_back(fd);
                        }
                    }
                    else
                    {
                        IDLog("Ignoring ancillary data level %d, type %d\n", cmsg->cmsg_level, cmsg->cmsg_type);
                    }
                }
            }
#endif
            if (n < 0)
            {
                continue;
            }

            if (n == 0)
            {
                IDLog("INDI server %s/%d disconnected.\n", cServer.c_str(), cPort);
                break;
            }

            auto documents = xmlParser.parseChunk(buffer, n);

            if (documents.size() == 0)
            {
                if (xmlParser.hasErrorMessage())
                {
                    IDLog("Bad XML from %s/%d: %s\n%s\n", cServer.c_str(), cPort, xmlParser.errorMessage(), buffer);
                }
                break;   
            }

            for (const auto &doc: documents)
            {
                INDI::LilXmlElement root = doc.root();

                if (verbose)
                    root.print(stderr, 0);
                
                std::vector<std::string> blobs;

                if (!parseAttachedBlobs(root, blobs))
                {
                    IDLog("Missing attachment from %s/%d\n", cServer.c_str(), cPort);
                    clientFatalError = true;
                    break;
                }
                int err_code;
                try
                {
                    err_code = dispatchCommand(root, msg);
                }
                catch(...)
                {
                    releaseBlobUids(blobs);
                    throw;
                }
                releaseBlobUids(blobs);

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

            if (clientFatalError)
            {
                break;
            }
        }
    }

    int exit_code;

    {
        std::lock_guard<std::mutex> locker(sSocketBusy);
#ifdef _WINDOWS
        if (sockfd != INVALID_SOCKET)
        {
            net_close(sockfd);
            WSACleanup();
            sockfd = INVALID_SOCKET;
        }
#else
        close(sockfd);
        close(receiveFd);
        close(sendFd);
#endif

        exit_code = sAboutToClose ? sExitCode : -1;
        sConnected = false;
        // JM 2021.09.08: Call serverDisconnected *before* clearing devices.
        parent->serverDisconnected(exit_code);

        clear();
        watchDevice.unwatchDevices();
        sSocketChanged.notify_all();
    }
}

bool BaseClientPrivate::parseAttachedBlobs(const INDI::LilXmlElement &root, std::vector<std::string> &blobs)
{
    // parse all elements in root that are attached.
    // Create for each a new GUID and associate it in a global map
    // modify the xml to add an attribute with the guid
    for (auto &blobContent: root.getElementsByTagName("oneBLOB"))
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

// BaseClient

BaseClient::BaseClient()
    : AbstractBaseClient(std::unique_ptr<AbstractBaseClientPrivate>(new BaseClientPrivate(this)))
{ }

BaseClient::~BaseClient()
{ }

bool BaseClient::connectServer()
{
    D_PTR(BaseClient);
    {
        std::unique_lock<std::mutex> locker(d->sSocketBusy);
        if (d->sConnected == true)
        {
            IDLog("INDI::BaseClient::connectServer: Already connected.\n");
            return false;
        }

        IDLog("INDI::BaseClient::connectServer: creating new connection...\n");

#ifdef _WINDOWS
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != NO_ERROR)
        {
            IDLog("Error at WSAStartup()\n");
            return false;
        }
#endif

#ifndef _WINDOWS
        // System with unix support automatically connect over unix domain
        if (d->cServer == "localhost")
        {
            if (!(d->establish(unixDomainPrefix) || d->establish(d->cServer)))
                return false;
        }
        else
        {
            if (!d->establish(d->cServer))
                return false;
        }
#else
        if (!establish(d->cServer))
            return false;
#endif

#ifndef _WINDOWS
        int pipefd[2];
        int ret;
        ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);

        if (ret < 0)
        {
            IDLog("notify pipe: %s\n", strerror(errno));
            return false;
        }

        d->receiveFd = pipefd[0];
        d->sendFd    = pipefd[1];
#endif

        d->sConnected = true;
        d->sAboutToClose = false;
        d->sSocketChanged.notify_all();
        std::thread(std::bind(&BaseClientPrivate::listenINDI, d)).detach();
    }
    serverConnected();

    return true;
}

bool BaseClient::disconnectServer(int exit_code)
{
    D_PTR(BaseClient);
    return d->disconnect(exit_code);
}

}
