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

#include "baseclient.h"

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
#include <WinSock2.h>
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

static userio io;

INDI::BaseClient::BaseClient() : cServer("localhost"), cPort(7624)
{
    io.write = [](void *user, const void * ptr, size_t count) -> size_t
    {
        BaseClient *self = static_cast<BaseClient *>(user);
        return self->sendData(ptr, count);
    };

    io.vprintf = [](void *user, const char * format, va_list ap) -> int
    {
        BaseClient *self = static_cast<BaseClient *>(user);
        char message[MAXRBUF];
        vsnprintf(message, MAXRBUF, format, ap);
        return self->sendData(message, strlen(message));
    };

    sConnected = false;
    verbose    = false;

    timeout_sec = 3;
    timeout_us  = 0;
}

INDI::BaseClient::~BaseClient()
{
    if (isServerConnected())
        disconnectServer();

    std::unique_lock<std::mutex> locker(sSocketBusy);
    if (!sSocketChanged.wait_for(locker, std::chrono::milliseconds(500), [this]{ return sConnected == false; }))
    {
        IDLog("BaseClient::~BaseClient: Probability of detecting a deadlock.\n");
        /* #PS:
         * KStars bug - suspicion
         *   The function thread 'BaseClient::listenINDI' could not be terminated
         *   because the 'dispatchCommand' function is in progress.
         *
         *   The function 'dispatchCommand' cannot be completed
         *   because it is related to the function call 'ClientManager::newProperty'.
         *
         *   There is a call that uses BlockingQueuedConnection to the thread that is currently busy
         *   destroying the BaseClient object.
         *
         */
    }
}

void INDI::BaseClient::clear()
{
    while (!cDevices.empty())
    {
        delete cDevices.back();
        cDevices.pop_back();
    }
    cDevices.clear();
    while (!blobModes.empty())
    {
        delete blobModes.back();
        blobModes.pop_back();
    }
    blobModes.clear();
}

void INDI::BaseClient::setVerbose(bool enable)
{
    verbose = enable;
}

bool INDI::BaseClient::isVerbose() const
{
    return verbose;
}

void INDI::BaseClient::setConnectionTimeout(uint32_t seconds, uint32_t microseconds)
{
    timeout_sec = seconds;
    timeout_us  = microseconds;
}

void INDI::BaseClient::setServer(const char *hostname, unsigned int port)
{
    cServer = hostname;
    cPort   = port;
}

void INDI::BaseClient::watchDevice(const char *deviceName)
{
    // Watch for duplicates. Should have used std::set from the beginning but let's
    // avoid changing API now.
    if (std::find(cDeviceNames.begin(), cDeviceNames.end(), deviceName) != cDeviceNames.end())
        return;

    cDeviceNames.emplace_back(deviceName);
}

void INDI::BaseClient::watchProperty(const char *deviceName, const char *propertyName)
{
    watchDevice(deviceName);
    cWatchProperties[deviceName].insert(propertyName);
}

bool INDI::BaseClient::connectServer()
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

    struct timeval ts;
    ts.tv_sec  = timeout_sec;
    ts.tv_usec = timeout_us;

    struct sockaddr_in serv_addr;
    struct hostent *hp;
    int ret = 0;

    /* lookup host address */
    hp = gethostbyname(cServer.c_str());
    if (!hp)
    {
        perror("gethostbyname");
        return false;
    }

    /* create a socket to the INDI server */
    (void)memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    serv_addr.sin_port        = htons(cPort);
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

    /* set the socket in non-blocking */
    //set socket nonblocking flag
#ifdef _WINDOWS
    u_long iMode = 0;
    iResult = ioctlsocket(sockfd, FIONBIO, &iMode);
    if (iResult != NO_ERROR)
    {
        IDLog("ioctlsocket failed with error: %ld\n", iResult);
        return false;
    }
#else
    int flags = 0;
    if ((flags = fcntl(sockfd, F_GETFL, 0)) < 0)
        return false;

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
        return false;
#endif

    //clear out descriptor sets for select
    //add socket to the descriptor sets
    fd_set rset, wset;
    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    wset = rset; //structure assignment okok

    /* connect */
    if ((ret = ::connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
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
            return false;
        //we had a timeout
        if (ret == 0)
        {
#ifdef _WINDOWS
            IDLog("select timeout\n");
#else
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
            return false;
        }
    }
    else
        return false;

    /* check if we had a socket error */
    if (error)
    {
        errno = error;
        perror("socket");
        return false;
    }
#endif

#ifndef _WINDOWS
    int pipefd[2];
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);

    if (ret < 0)
    {
        IDLog("notify pipe: %s\n", strerror(errno));
        return false;
    }

    m_receiveFd = pipefd[0];
    m_sendFd    = pipefd[1];
#endif

    sConnected = true;
    sAboutToClose = false;
    sSocketChanged.notify_all();
    std::thread(std::bind(&BaseClient::listenINDI, this)).detach();

    return true;
}

bool INDI::BaseClient::disconnectServer(int exit_code)
{
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
    ssize_t ret = write(m_sendFd, &c, sizeof(c));
    if (ret != sizeof(c))
    {
        IDLog("INDI::BaseClient::disconnectServer: Error. The socket cannot be woken up.\n");
    }
#endif
    sExitCode = exit_code;
    return true;
}

// #PS: avoid calling pure virtual method
void INDI::BaseClient::serverDisconnected(int exit_code)
{
    INDI_UNUSED(exit_code);
}

bool INDI::BaseClient::isServerConnected() const
{
    return sConnected;
}

void INDI::BaseClient::connectDevice(const char *deviceName)
{
    setDriverConnection(true, deviceName);
}

void INDI::BaseClient::disconnectDevice(const char *deviceName)
{
    setDriverConnection(false, deviceName);
}

void INDI::BaseClient::setDriverConnection(bool status, const char *deviceName)
{
    INDI::BaseDevice *drv                 = getDevice(deviceName);
    ISwitchVectorProperty *drv_connection = nullptr;

    if (drv == nullptr)
    {
        IDLog("INDI::BaseClient: Error. Unable to find driver %s\n", deviceName);
        return;
    }

    drv_connection = drv->getSwitch(INDI::SP::CONNECTION);

    if (drv_connection == nullptr)
        return;

    // If we need to connect
    if (status)
    {
        // If there is no need to do anything, i.e. already connected.
        if (drv_connection->sp[0].s == ISS_ON)
            return;

        IUResetSwitch(drv_connection);
        drv_connection->s       = IPS_BUSY;
        drv_connection->sp[0].s = ISS_ON;
        drv_connection->sp[1].s = ISS_OFF;

        sendNewSwitch(drv_connection);
    }
    else
    {
        // If there is no need to do anything, i.e. already disconnected.
        if (drv_connection->sp[1].s == ISS_ON)
            return;

        IUResetSwitch(drv_connection);
        drv_connection->s       = IPS_BUSY;
        drv_connection->sp[0].s = ISS_OFF;
        drv_connection->sp[1].s = ISS_ON;

        sendNewSwitch(drv_connection);
    }
}

INDI::BaseDevice *INDI::BaseClient::getDevice(const char *deviceName)
{
    for (auto &device : cDevices)
    {
        if (!strcmp(deviceName, device->getDeviceName()))
            return device;
    }
    return nullptr;
}

const std::vector<INDI::BaseDevice *> &INDI::BaseClient::getDevices() const
{
    return cDevices;
}

const char *INDI::BaseClient::getHost() const
{
    return cServer.c_str();
}

int INDI::BaseClient::getPort() const
{
    return cPort;
}

void INDI::BaseClient::listenINDI()
{
    char buffer[MAXINDIBUF];
    char msg[MAXRBUF];
#ifdef _WINDOWS
    SOCKET maxfd = 0;
#else
    int maxfd = 0;
#endif
    fd_set rs;
    XMLEle **nodes = nullptr;
    XMLEle *root = nullptr;
    int inode = 0;

    serverConnected();

    if (cDeviceNames.empty())
    {
        IUUserIOGetProperties(&io, this, nullptr, nullptr);
        if (verbose)
            IUUserIOGetProperties(userio_file(), stderr, nullptr, nullptr);
    }
    else
    {
        for (const auto &oneDevice : cDeviceNames)
        {
            // If there are no specific properties to watch, we watch the complete device
            if (cWatchProperties.find(oneDevice) == cWatchProperties.end())
            {
                IUUserIOGetProperties(&io, this, oneDevice.c_str(), nullptr);
                if (verbose)
                    IUUserIOGetProperties(userio_file(), stderr, oneDevice.c_str(), nullptr);
            }
            else
            {
                for (const auto &oneProperty : cWatchProperties[oneDevice])
                {
                    IUUserIOGetProperties(&io, this, oneDevice.c_str(), oneProperty.c_str());
                    if (verbose)
                        IUUserIOGetProperties(userio_file(), stderr, oneDevice.c_str(), oneProperty.c_str());
                }
            }
        }
    }

    FD_ZERO(&rs);

    FD_SET(sockfd, &rs);
    maxfd = std::max(maxfd, sockfd);

#ifndef _WINDOWS
    FD_SET(m_receiveFd, &rs);
    maxfd = std::max(maxfd, m_receiveFd);
#endif

    clear();
    LilXML *lillp = newLilXML();

    /* read from server, exit if find all requested properties */
    while (!sAboutToClose)
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
            n = recv(sockfd, buffer, MAXINDIBUF, MSG_DONTWAIT);
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

            nodes = parseXMLChunk(lillp, buffer, n, msg);

            if (!nodes)
            {
                if (msg[0])
                {
                    IDLog("Bad XML from %s/%d: %s\n%s\n", cServer.c_str(), cPort, msg, buffer);
                }
                break;
            }
            root = nodes[inode];
            while (root)
            {
                if (verbose)
                    prXMLEle(stderr, root, 0);

                int err_code = dispatchCommand(root, msg);

                if (err_code < 0)
                {
                    // Silenty ignore property duplication errors
                    if (err_code != INDI_PROPERTY_DUPLICATED)
                    {
                        IDLog("Dispatch command error(%d): %s\n", err_code, msg);
                        prXMLEle(stderr, root, 0);
                    }
                }

                delXMLEle(root); // not yet, delete and continue
                inode++;
                root = nodes[inode];
            }
            free(nodes);
            inode = 0;
        }
    }

    delLilXML(lillp);

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
        close(m_receiveFd);
        close(m_sendFd);
#endif
        clear();
        cDeviceNames.clear();
        sConnected = false;
        sSocketChanged.notify_all();

        exit_code = sAboutToClose ? sExitCode : -1;
    }

    serverDisconnected(exit_code);
}

int INDI::BaseClient::dispatchCommand(XMLEle *root, char *errmsg)
{
    const char *tag = tagXMLEle(root);

    if (!strcmp(tag, "message"))
        return messageCmd(root, errmsg);
    else if (!strcmp(tag, "delProperty"))
        return delPropertyCmd(root, errmsg);
    // Just ignore any getProperties we might get
    else if (!strcmp(tag, "getProperties"))
        return INDI_PROPERTY_DUPLICATED;

    /* Get the device, if not available, create it */
    INDI::BaseDevice *dp = findDev(root, 1, errmsg);
    if (dp == nullptr)
    {
        strcpy(errmsg, "No device available and none was created");
        return INDI_DEVICE_NOT_FOUND;
    }

    // Ignore echoed newXXX
    if (strstr(tag, "new"))
        return 0;

    // If device is set to BLOB_ONLY, we ignore everything else
    // not related to blobs
    if (getBLOBMode(dp->getDeviceName()) == B_ONLY)
    {
        if (!strcmp(tag, "defBLOBVector"))
            return dp->buildProp(root, errmsg);
        else if (!strcmp(tag, "setBLOBVector"))
            return dp->setValue(root, errmsg);

        // Ignore everything else
        return 0;
    }

    // If we are asked to watch for specific properties only, we ignore everything else
    if (cWatchProperties.size() > 0)
    {
        const char *device = findXMLAttValu(root, "device");
        const char *name = findXMLAttValu(root, "name");
        if (device && name)
        {
            if (cWatchProperties.find(device) == cWatchProperties.end() ||
                    cWatchProperties[device].find(name) == cWatchProperties[device].end())
                return 0;
        }
    }

    if ((!strcmp(tag, "defTextVector")) || (!strcmp(tag, "defNumberVector")) ||
            (!strcmp(tag, "defSwitchVector")) || (!strcmp(tag, "defLightVector")) ||
            (!strcmp(tag, "defBLOBVector")))
        return dp->buildProp(root, errmsg);
    else if (!strcmp(tag, "setTextVector") || !strcmp(tag, "setNumberVector") ||
             !strcmp(tag, "setSwitchVector") || !strcmp(tag, "setLightVector") ||
             !strcmp(tag, "setBLOBVector"))
        return dp->setValue(root, errmsg);

    return INDI_DISPATCH_ERROR;
}

/* delete the property in the given device, including widgets and data structs.
 * when last property is deleted, delete the device too.
 * if no property name attribute at all, delete the whole device regardless.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDI::BaseClient::delPropertyCmd(XMLEle *root, char *errmsg)
{
    XMLAtt *ap;
    INDI::BaseDevice *dp;

    /* dig out device and optional property name */
    dp = findDev(root, 0, errmsg);
    if (!dp)
        return INDI_DEVICE_NOT_FOUND;

    dp->checkMessage(root);

    ap = findXMLAtt(root, "name");

    /* Delete property if it exists, otherwise, delete the whole device */
    if (ap)
    {
        INDI::Property *rProp = dp->getProperty(valuXMLAtt(ap));
        if (rProp == nullptr)
        {
            // Silently ignore B_ONLY clients.
            if (blobModes.empty() || blobModes[0]->blobMode == B_ONLY)
                return 0;

            snprintf(errmsg, MAXRBUF, "Cannot delete property %s as it is not defined yet. Check driver.", valuXMLAtt(ap));
            return -1;
        }
        if (sConnected)
            removeProperty(rProp);
        int errCode = dp->removeProperty(valuXMLAtt(ap), errmsg);

        return errCode;
    }
    // delete the whole device
    else
        return deleteDevice(dp->getDeviceName(), errmsg);
}

int INDI::BaseClient::deleteDevice(const char *devName, char *errmsg)
{
    std::vector<INDI::BaseDevice *>::iterator devicei;

    for (devicei = cDevices.begin(); devicei != cDevices.end();)
    {
        if (!strcmp(devName, (*devicei)->getDeviceName()))
        {
            removeDevice(*devicei);
            delete *devicei;
            devicei = cDevices.erase(devicei);
            return 0;
        }
        else
            ++devicei;
    }

    snprintf(errmsg, MAXRBUF, "Device %s not found", devName);
    return INDI_DEVICE_NOT_FOUND;
}

INDI::BaseDevice *INDI::BaseClient::findDev(const char *devName, char *errmsg)
{
    auto pos = std::find_if(cDevices.begin(), cDevices.end(), [devName](INDI::BaseDevice * oneDevice)
    {
        return !strcmp(oneDevice->getDeviceName(), devName);
    });

    if (pos != cDevices.end())
        return *pos;

    snprintf(errmsg, MAXRBUF, "Device %s not found", devName);
    return nullptr;
}

/* add new device */
INDI::BaseDevice *INDI::BaseClient::addDevice(XMLEle *dep, char *errmsg)
{
    //devicePtr dp(new INDI::BaseDriver());
    INDI::BaseDevice *dp = new INDI::BaseDevice();
    char *device_name;

    /* allocate new INDI::BaseDriver */
    XMLAtt *ap = findXMLAtt(dep, "device");
    if (!ap)
    {
        strncpy(errmsg, "Unable to find device attribute in XML element. Cannot add device.", MAXRBUF);
        delete (dp);
        return nullptr;
    }

    device_name = valuXMLAtt(ap);

    dp->setMediator(this);
    dp->setDeviceName(device_name);

    cDevices.push_back(dp);

    newDevice(dp);

    /* ok */
    return dp;
}

INDI::BaseDevice *INDI::BaseClient::findDev(XMLEle *root, int create, char *errmsg)
{
    XMLAtt *ap;
    INDI::BaseDevice *dp;
    char *dn;

    /* get device name */
    ap = findXMLAtt(root, "device");
    if (!ap)
    {
        snprintf(errmsg, MAXRBUF, "No device attribute found in element %s", tagXMLEle(root));
        return (nullptr);
    }

    dn = valuXMLAtt(ap);

    if (*dn == '\0')
    {
        snprintf(errmsg, MAXRBUF, "Device name is empty! %s", tagXMLEle(root));
        return (nullptr);
    }

    dp = findDev(dn, errmsg);

    if (dp)
        return dp;

    /* not found, create if ok */
    if (create)
        return (addDevice(root, errmsg));

    snprintf(errmsg, MAXRBUF, "INDI: <%s> no such device %s", tagXMLEle(root), dn);
    return nullptr;
}

/* a general message command received from the device.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDI::BaseClient::messageCmd(XMLEle *root, char *errmsg)
{
    INDI::BaseDevice *dp = findDev(root, 0, errmsg);

    if (dp)
        dp->checkMessage(root);
    else
    {
        XMLAtt *message;
        XMLAtt *time_stamp;

        char msgBuffer[MAXRBUF];

        /* prefix our timestamp if not with msg */
        time_stamp = findXMLAtt(root, "timestamp");

        /* finally! the msg */
        message = findXMLAtt(root, "message");
        if (!message)
        {
            strncpy(errmsg, "No message content found.", MAXRBUF);
            return -1;
        }

        if (time_stamp)
            snprintf(msgBuffer, MAXRBUF, "%s: %s", valuXMLAtt(time_stamp), valuXMLAtt(message));
        else
        {
            char ts[32];
            struct tm *tp;
            time_t t;
            time(&t);
            tp = gmtime(&t);
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
            snprintf(msgBuffer, MAXRBUF, "%s: %s", ts, valuXMLAtt(message));
        }

        std::string finalMsg = msgBuffer;

        newUniversalMessage(finalMsg);
    }

    return (0);
}

void INDI::BaseClient::newUniversalMessage(std::string message)
{
    IDLog("%s\n", message.c_str());
}


void INDI::BaseClient::sendNewText(ITextVectorProperty *tvp)
{
    tvp->s = IPS_BUSY;
    IUUserIONewText(&io, this, tvp);
}

void INDI::BaseClient::sendNewText(const char *deviceName, const char *propertyName, const char *elementName,
                                   const char *text)
{
    INDI::BaseDevice *drv = getDevice(deviceName);

    if (drv == nullptr)
        return;

    ITextVectorProperty *tvp = drv->getText(propertyName);

    if (tvp == nullptr)
        return;

    IText *tp = IUFindText(tvp, elementName);

    if (tp == nullptr)
        return;

    IUSaveText(tp, text);

    sendNewText(tvp);
}

void INDI::BaseClient::sendNewNumber(INumberVectorProperty *nvp)
{
    nvp->s = IPS_BUSY;
    IUUserIONewNumber(&io, this, nvp);
}

void INDI::BaseClient::sendNewNumber(const char *deviceName, const char *propertyName, const char *elementName,
                                     double value)
{
    INDI::BaseDevice *drv = getDevice(deviceName);

    if (drv == nullptr)
        return;

    INumberVectorProperty *nvp = drv->getNumber(propertyName);

    if (nvp == nullptr)
        return;

    INumber *np = IUFindNumber(nvp, elementName);

    if (np == nullptr)
        return;

    np->value = value;

    sendNewNumber(nvp);
}

void INDI::BaseClient::sendNewSwitch(ISwitchVectorProperty *svp)
{
    svp->s = IPS_BUSY;
    IUUserIONewSwitch(&io, this, svp);
}

void INDI::BaseClient::sendNewSwitch(const char *deviceName, const char *propertyName, const char *elementName)
{
    INDI::BaseDevice *drv = getDevice(deviceName);

    if (drv == nullptr)
        return;

    ISwitchVectorProperty *svp = drv->getSwitch(propertyName);

    if (svp == nullptr)
        return;

    ISwitch *sp = IUFindSwitch(svp, elementName);

    if (sp == nullptr)
        return;

    sp->s = ISS_ON;

    sendNewSwitch(svp);
}

void INDI::BaseClient::startBlob(const char *devName, const char *propName, const char *timestamp)
{
    IUUserIONewBLOBStart(&io, this, devName, propName, timestamp);
}

void INDI::BaseClient::sendOneBlob(IBLOB *bp)
{
    IUUserIOBLOBContextOne(
        &io, this,
        bp->name, bp->size, bp->bloblen, bp->blob, bp->format
    );
}

void INDI::BaseClient::sendOneBlob(const char *blobName, unsigned int blobSize, const char *blobFormat,
                                   void *blobBuffer)
{
    IUUserIOBLOBContextOne(
        &io, this,
        blobName, blobSize, blobSize, blobBuffer, blobFormat
    );
}

void INDI::BaseClient::finishBlob()
{
    IUUserIONewBLOBFinish(&io, this);
}

void INDI::BaseClient::setBLOBMode(BLOBHandling blobH, const char *dev, const char *prop)
{
    if (!dev[0])
        return;

    BLOBMode *bMode = findBLOBMode(std::string(dev), (prop ? std::string(prop) : std::string()));

    if (bMode == nullptr)
    {
        BLOBMode *newMode = new BLOBMode();
        newMode->device   = std::string(dev);
        newMode->property = (prop ? std::string(prop) : std::string());
        newMode->blobMode = blobH;
        blobModes.push_back(newMode);
    }
    else
    {
        // If nothing changed, nothing to to do
        if (bMode->blobMode == blobH)
            return;

        bMode->blobMode = blobH;
    }

    IUUserIOEnableBLOB(&io, this, dev, prop, blobH);
}

BLOBHandling INDI::BaseClient::getBLOBMode(const char *dev, const char *prop)
{
    BLOBHandling bHandle = B_ALSO;

    BLOBMode *bMode = findBLOBMode(dev, (prop ? std::string(prop) : std::string()));

    if (bMode)
        bHandle = bMode->blobMode;

    return bHandle;
}

INDI::BaseClient::BLOBMode *INDI::BaseClient::findBLOBMode(const std::string &device, const std::string &property)
{
    for (auto &blob : blobModes)
    {
        if (blob->device == device && (property.empty() || blob->property == property))
            return blob;
    }

    return nullptr;
}

bool INDI::BaseClient::getDevices(std::vector<INDI::BaseDevice *> &deviceList, uint16_t driverInterface )
{
    for (INDI::BaseDevice *device : cDevices)
    {
        if (device->getDriverInterface() & driverInterface)
            deviceList.push_back(device);
    }

    return (deviceList.size() > 0);
}

size_t INDI::BaseClient::sendData(const void *data, size_t size)
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
        disconnectServer(-1);
    }

    return std::max(ret, 0);
}

void INDI::BaseClient::sendString(const char *fmt, ...)
{
    char message[MAXRBUF];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(message, MAXRBUF, fmt, ap);
    va_end(ap);
    sendData(message, strlen(message));
}
