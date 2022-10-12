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

static userio io;

#include "baseclient_p.h"

namespace INDI
{

BaseClientPrivate::BaseClientPrivate(BaseClient *parent)
    : parent(parent)
{
    io.write = [](void *user, const void * ptr, size_t count) -> size_t
    {
        auto self = static_cast<BaseClientPrivate *>(user);
        return self->sendData(ptr, count);
    };

    io.vprintf = [](void *user, const char * format, va_list ap) -> int
    {
        auto self = static_cast<BaseClientPrivate *>(user);
        char message[MAXRBUF];
        vsnprintf(message, MAXRBUF, format, ap);
        return self->sendData(message, strlen(message));
    };
}

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

void BaseClientPrivate::clear()
{
    watchDevice.clearDevices();
    blobModes.clear();
    directBlobAccess.clear();
}

#ifndef _WINDOWS

static void initUnixSocketAddr(const std::string &unixAddr, struct sockaddr_un &serv_addr_un, socklen_t &addrlen, bool bind)
{
    memset(&serv_addr_un, 0, sizeof(serv_addr_un));
    serv_addr_un.sun_family = AF_UNIX;

#ifdef __linux__
    (void) bind;

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

bool BaseClientPrivate::connect()
{
    {
        std::unique_lock<std::mutex> locker(sSocketBusy);
        if (sConnected == true)
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
        if (cServer == "localhost")
        {
            if (!(establish(unixDomainPrefix) || establish(cServer)))
                return false;
        }
        else
        {
            if (!establish(cServer))
                return false;
        }
#else
        if (!establish(cServer))
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

        receiveFd = pipefd[0];
        sendFd    = pipefd[1];
#endif

        sConnected = true;
        sAboutToClose = false;
        sSocketChanged.notify_all();
        std::thread(std::bind(&BaseClientPrivate::listenINDI, this)).detach();
    }
    parent->serverConnected();

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

    connect();

    if (watchDevice.isEmpty())
    {
        IUUserIOGetProperties(&io, this, nullptr, nullptr);
        if (verbose)
            IUUserIOGetProperties(userio_file(), stderr, nullptr, nullptr);
    }
    else
    {
        for (const auto &deviceInfo : watchDevice /* first: device name, second: device info */)
        {
            // If there are no specific properties to watch, we watch the complete device
            if (deviceInfo.second.properties.size() == 0)
            {
                IUUserIOGetProperties(&io, this, deviceInfo.first.c_str(), nullptr);
                if (verbose)
                    IUUserIOGetProperties(userio_file(), stderr, deviceInfo.first.c_str(), nullptr);
            }
            else
            {
                for (const auto &oneProperty : deviceInfo.second.properties)
                {
                    IUUserIOGetProperties(&io, this, deviceInfo.first.c_str(), oneProperty.c_str());
                    if (verbose)
                        IUUserIOGetProperties(userio_file(), stderr, deviceInfo.first.c_str(), oneProperty.c_str());
                }
            }
        }
    }

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
        watchDevice.clearDevices();
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

int BaseClientPrivate::dispatchCommand(const INDI::LilXmlElement &root, char *errmsg)
{
    // Ignore echoed newXXX
    if (root.tagName().find("new") == 0)
    {
        return 0;       
    }

    if (root.tagName() == "pingRequest")
    {
        parent->sendPingReply(root.getAttribute("uid"));
        return 0;
    }

    if (root.tagName() == "pingReply")
    {
        parent->newPingReply(root.getAttribute("uid").toString());
        return 0;
    }

    if (root.tagName() == "message")
    {
        return messageCmd(root, errmsg);
    }

    if (root.tagName() == "delProperty")
    {
        return delPropertyCmd(root, errmsg);
    }

    // Just ignore any getProperties we might get
    if (root.tagName() == "getProperties")
    {
        return INDI_PROPERTY_DUPLICATED;
    }

    // If device is set to BLOB_ONLY, we ignore everything else
    // not related to blobs
    if (
        parent->getBLOBMode(root.getAttribute("device")) == B_ONLY &&
        root.tagName() != "defBLOBVector" &&
        root.tagName() != "setBLOBVector"
    )
    {
        return 0;
    }

    return watchDevice.processXml(root, errmsg, [this]() { // create new device if nessesery
        INDI::BaseDevice *device = new INDI::BaseDevice();
        device->setMediator(parent);
        return device;
    });
}

int BaseClientPrivate::deleteDevice(const char *devName, char *errmsg)
{
    if (auto device = watchDevice.getDeviceByName(devName))
    {
        parent->removeDevice(device);
        watchDevice.deleteDevice(device);
        return 0;
    }
    snprintf(errmsg, MAXRBUF, "Device %s not found", devName);
    return INDI_DEVICE_NOT_FOUND;
}


/* delete the property in the given device, including widgets and data structs.
 * when last property is deleted, delete the device too.
 * if no property name attribute at all, delete the whole device regardless.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int BaseClientPrivate::delPropertyCmd(const INDI::LilXmlElement &root, char *errmsg)
{
    /* dig out device and optional property name */
    INDI::BaseDevice *dp = watchDevice.getDeviceByName(root.getAttribute("device"));

    if (dp == nullptr)
        return INDI_DEVICE_NOT_FOUND;

    dp->checkMessage(root.handle());

    const auto propertyName = root.getAttribute("name");

    // Delete the whole device if propertyName does not exists
    if (!propertyName.isValid())
    {
        return deleteDevice(dp->getDeviceName(), errmsg);
    }

    // Delete property if it exists
    if (auto property = dp->getProperty(propertyName))
    {
        if (sConnected)
            parent->removeProperty(property);
        return dp->removeProperty(propertyName, errmsg);
    }

    // Silently ignore B_ONLY clients.
    if (blobModes.empty() || blobModes.front().blobMode == B_ONLY)
        return 0;
    snprintf(errmsg, MAXRBUF, "Cannot delete property %s as it is not defined yet. Check driver.", propertyName.toCString());
    return -1;
}

/* a general message command received from the device.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int BaseClientPrivate::messageCmd(const INDI::LilXmlElement &root, char *errmsg)
{
    INDI::BaseDevice *dp = watchDevice.getDeviceByName(root.getAttribute("device"));

    if (dp)
    {
        dp->checkMessage(root.handle());
        return 0;
    }

    char msgBuffer[MAXRBUF];

    auto timestamp = root.getAttribute("timestamp");
    auto message   = root.getAttribute("message");

    if (!message.isValid())
    {
        strncpy(errmsg, "No message content found.", MAXRBUF);
        return -1;
    }

    if (timestamp.isValid())
    {
        snprintf(msgBuffer, MAXRBUF, "%s: %s", timestamp.toCString(), message.toCString());
    }
    else
    {
        char ts[32];
        struct tm *tp;
        time_t t;
        time(&t);
        tp = gmtime(&t);
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
        snprintf(msgBuffer, MAXRBUF, "%s: %s", ts, message.toCString());
    }

    parent->newUniversalMessage(msgBuffer);

    return 0;
}

void BaseClientPrivate::enableDirectBlobAccess(const char * dev, const char * prop)
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

static bool hasDirectBlobAccessEntry(const std::map<std::string, std::set<std::string>> &directBlobAccess,
                                     const std::string &dev, const std::string &prop)
{
    auto devAccess = directBlobAccess.find(dev) ;
    if (devAccess == directBlobAccess.end())
    {
        return false;
    }
    return devAccess->second.find(prop) != devAccess->second.end();
}

bool BaseClientPrivate::isDirectBlobAccess(const std::string &dev, const std::string &prop) const
{
    return hasDirectBlobAccessEntry(directBlobAccess, "", "")
           || hasDirectBlobAccessEntry(directBlobAccess, dev, "")
           || hasDirectBlobAccessEntry(directBlobAccess, dev, prop);
}

BLOBMode *INDI::BaseClientPrivate::findBLOBMode(const std::string &device, const std::string &property)
{
    for (auto &blob : blobModes)
    {
        if (blob.device == device && (property.empty() || blob.property == property))
            return &blob;
    }

    return nullptr;
}

void BaseClientPrivate::setDriverConnection(bool status, const char *deviceName)
{
    INDI::BaseDevice *drv = parent->getDevice(deviceName);

    if (!drv)
    {
        IDLog("INDI::BaseClient: Error. Unable to find driver %s\n", deviceName);
        return;
    }

    auto drv_connection = drv->getSwitch(INDI::SP::CONNECTION);

    if (!drv_connection)
        return;

    // If we need to connect
    if (status)
    {
        // If there is no need to do anything, i.e. already connected.
        if (drv_connection->at(0)->getState() == ISS_ON)
            return;

        drv_connection->reset();
        drv_connection->setState(IPS_BUSY);
        drv_connection->at(0)->setState(ISS_ON);
        drv_connection->at(1)->setState(ISS_OFF);

        parent->sendNewSwitch(drv_connection);
    }
    else
    {
        // If there is no need to do anything, i.e. already disconnected.
        if (drv_connection->at(1)->getState() == ISS_ON)
            return;

        drv_connection->reset();
        drv_connection->setState(IPS_BUSY);
        drv_connection->at(0)->setState(ISS_OFF);
        drv_connection->at(1)->setState(ISS_ON);

        parent->sendNewSwitch(drv_connection);
    }
}

}

INDI::BaseClient::BaseClient()
    : d_ptr(new BaseClientPrivate(this))
{ }

INDI::BaseClient::~BaseClient()
{

}

void INDI::BaseClient::setVerbose(bool enable)
{
    D_PTR(BaseClient);
    d->verbose = enable;
}

bool INDI::BaseClient::isVerbose() const
{
    D_PTR(const BaseClient);
    return d->verbose;
}

void INDI::BaseClient::setConnectionTimeout(uint32_t seconds, uint32_t microseconds)
{
    D_PTR(BaseClient);
    d->timeout_sec = seconds;
    d->timeout_us  = microseconds;
}

void INDI::BaseClient::setServer(const char *hostname, unsigned int port)
{
    D_PTR(BaseClient);
    d->cServer = hostname;
    d->cPort   = port;
}

void INDI::BaseClient::watchDevice(const char *deviceName)
{
    D_PTR(BaseClient);
    d->watchDevice[deviceName]; // create empty map field
}

void INDI::BaseClient::watchDevice(const char *deviceName, const std::function<void (BaseDevice)> &callback)
{
    D_PTR(BaseClient);
    d->watchDevice[deviceName].newDeviceCallback = callback;
}

void INDI::BaseClient::watchProperty(const char *deviceName, const char *propertyName)
{
    D_PTR(BaseClient);
    d->watchDevice[deviceName].properties.insert(propertyName);
}

bool INDI::BaseClient::connectServer()
{
    D_PTR(BaseClient);
    return d->connect();
}

bool INDI::BaseClient::disconnectServer(int exit_code)
{
    D_PTR(BaseClient);
    return d->disconnect(exit_code);
}

// #PS: avoid calling pure virtual method
void INDI::BaseClient::serverDisconnected(int exit_code)
{
    INDI_UNUSED(exit_code);
}

bool INDI::BaseClient::isServerConnected() const
{
    D_PTR(const BaseClient);
    return d->sConnected;
}

void INDI::BaseClient::connectDevice(const char *deviceName)
{
    D_PTR(BaseClient);
    d->setDriverConnection(true, deviceName);
}

void INDI::BaseClient::disconnectDevice(const char *deviceName)
{
    D_PTR(BaseClient);
    d->setDriverConnection(false, deviceName);
}

INDI::BaseDevice *INDI::BaseClient::getDevice(const char *deviceName)
{
    D_PTR(BaseClient);
    return d->watchDevice.getDeviceByName(deviceName);
}

std::vector<INDI::BaseDevice *> INDI::BaseClient::getDevices() const
{
    D_PTR(const BaseClient);
    return d->watchDevice.getDevices();
}

const char *INDI::BaseClient::getHost() const
{
    D_PTR(const BaseClient);
    return d->cServer.c_str();
}

int INDI::BaseClient::getPort() const
{
    D_PTR(const BaseClient);
    return d->cPort;
}

void INDI::BaseClient::newUniversalMessage(std::string message)
{
    IDLog("%s\n", message.c_str());
}

void INDI::BaseClient::newPingReply(std::string uid)
{
    IDLog("Ping reply %s\n", uid.c_str());
}

void INDI::BaseClient::sendNewProperty(INDI::Property pp)
{
    D_PTR(BaseClient);
    pp.setState(IPS_BUSY);
    // #PS: TODO more generic
    switch (pp.getType())
    {
        case INDI_NUMBER:
            IUUserIONewNumber(&io, d, pp.getNumber());
            break;
        case INDI_SWITCH:
            IUUserIONewSwitch(&io, d, pp.getSwitch());
            break;
        case INDI_TEXT:
            IUUserIONewText(&io, d, pp.getText());
            break;
        case INDI_LIGHT:
            IDLog("Light type is not supported to send\n");
            break;
        case INDI_BLOB:
            IUUserIONewBLOB(&io, d, pp.getBLOB());
            break;
        case INDI_UNKNOWN:
            IDLog("Unknown type of property to send\n");
            break;
    }
}

void INDI::BaseClient::sendNewText(INDI::Property pp)
{
    D_PTR(BaseClient);
    pp.setState(IPS_BUSY);
    IUUserIONewText(&io, d, pp.getText());
}

void INDI::BaseClient::sendNewText(const char *deviceName, const char *propertyName, const char *elementName,
                                   const char *text)
{
    INDI::BaseDevice *drv = getDevice(deviceName);

    if (!drv)
        return;

    auto tvp = drv->getText(propertyName);

    if (!tvp)
        return;

    auto tp = tvp->findWidgetByName(elementName);

    if (!tp)
        return;

    tp->setText(text);

    sendNewText(tvp);
}

void INDI::BaseClient::sendNewNumber(INDI::Property pp)
{
    D_PTR(BaseClient);
    pp.setState(IPS_BUSY);
    IUUserIONewNumber(&io, d, pp.getNumber());
}

void INDI::BaseClient::sendNewNumber(const char *deviceName, const char *propertyName, const char *elementName,
                                     double value)
{
    INDI::BaseDevice *drv = getDevice(deviceName);

    if (!drv)
        return;

    auto nvp = drv->getNumber(propertyName);

    if (!nvp)
        return;

    auto np = nvp->findWidgetByName(elementName);

    if (!np)
        return;

    np->setValue(value);

    sendNewNumber(nvp);
}

void INDI::BaseClient::sendPingReply(const char * uuid)
{
    D_PTR(BaseClient);
    IUUserIOPingReply(&io, d, uuid);
}

void INDI::BaseClient::sendPingRequest(const char * uuid)
{
    D_PTR(BaseClient);
    IUUserIOPingRequest(&io, d, uuid);
}

void INDI::BaseClient::sendNewSwitch(INDI::Property pp)
{
    D_PTR(BaseClient);
    pp.setState(IPS_BUSY);
    IUUserIONewSwitch(&io, d, pp.getSwitch());
}

void INDI::BaseClient::sendNewSwitch(const char *deviceName, const char *propertyName, const char *elementName)
{
    INDI::BaseDevice *drv = getDevice(deviceName);

    if (!drv)
        return;

    auto svp = drv->getSwitch(propertyName);

    if (!svp)
        return;

    auto sp = svp->findWidgetByName(elementName);

    if (!sp)
        return;

    sp->setState(ISS_ON);

    sendNewSwitch(svp);
}

void INDI::BaseClient::startBlob(const char *devName, const char *propName, const char *timestamp)
{
    D_PTR(BaseClient);
    IUUserIONewBLOBStart(&io, d, devName, propName, timestamp);
}

void INDI::BaseClient::sendOneBlob(INDI::WidgetView<IBLOB> *blob)
{
    D_PTR(BaseClient);
    IUUserIOBLOBContextOne(
        &io, d,
        blob->getName(), blob->getSize(), blob->getBlobLen(), blob->getBlob(), blob->getFormat()
    );
}

void INDI::BaseClient::sendOneBlob(IBLOB *bp)
{
    sendOneBlob(static_cast<INDI::WidgetView<IBLOB>*>(bp));
}

void INDI::BaseClient::sendOneBlob(const char *blobName, unsigned int blobSize, const char *blobFormat,
                                   void *blobBuffer)
{
    D_PTR(BaseClient);
    IUUserIOBLOBContextOne(
        &io, d,
        blobName, blobSize, blobSize, blobBuffer, blobFormat
    );
}

void INDI::BaseClient::finishBlob()
{
    D_PTR(BaseClient);
    IUUserIONewBLOBFinish(&io, d);
}

void INDI::BaseClient::setBLOBMode(BLOBHandling blobH, const char *dev, const char *prop)
{
    D_PTR(BaseClient);
    if (!dev[0])
        return;

    BLOBMode *bMode = d->findBLOBMode(std::string(dev), (prop ? std::string(prop) : std::string()));

    if (bMode == nullptr)
    {
        BLOBMode newMode;
        newMode.device   = std::string(dev);
        newMode.property = (prop ? std::string(prop) : std::string());
        newMode.blobMode = blobH;
        d->blobModes.push_back(std::move(newMode));
    }
    else
    {
        // If nothing changed, nothing to to do
        if (bMode->blobMode == blobH)
            return;

        bMode->blobMode = blobH;
    }

    IUUserIOEnableBLOB(&io, d, dev, prop, blobH);
}

void INDI::BaseClient::enableDirectBlobAccess(const char * dev, const char * prop)
{
    D_PTR(BaseClient);
    d->enableDirectBlobAccess(dev, prop);
}

BLOBHandling INDI::BaseClient::getBLOBMode(const char *dev, const char *prop)
{
    D_PTR(BaseClient);
    BLOBHandling bHandle = B_ALSO;

    BLOBMode *bMode = d->findBLOBMode(dev, (prop ? std::string(prop) : std::string()));

    if (bMode)
        bHandle = bMode->blobMode;

    return bHandle;
}

bool INDI::BaseClient::getDevices(std::vector<INDI::BaseDevice *> &deviceList, uint16_t driverInterface )
{
    D_PTR(BaseClient);
    for (auto &it: d->watchDevice)
    {
        if (it.second.device->getDriverInterface() & driverInterface)
            deviceList.push_back(it.second.device.get());
    }

    return (deviceList.size() > 0);
}


void INDI::BaseClient::newDevice(INDI::BaseDevice *)
{ }

void INDI::BaseClient::removeDevice(INDI::BaseDevice *)
{ }

void INDI::BaseClient::newProperty(INDI::Property *)
{ }

void INDI::BaseClient::removeProperty(INDI::Property *)
{ }

void INDI::BaseClient::newBLOB(IBLOB *)
{ }

void INDI::BaseClient::newSwitch(ISwitchVectorProperty *)
{ }

void INDI::BaseClient::newNumber(INumberVectorProperty *)
{ }

void INDI::BaseClient::newText(ITextVectorProperty *)
{ }

void INDI::BaseClient::newLight(ILightVectorProperty *)
{ }

void INDI::BaseClient::newMessage(INDI::BaseDevice *, int)
{ }

void INDI::BaseClient::serverConnected()
{ }
