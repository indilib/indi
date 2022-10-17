/*******************************************************************************
  Copyright(c) 2016 Jasem Mutlaq. All rights reserved.

 INDI Qt Client

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

#include "baseclientqt.h"
#include "baseclientqt_p.h"

#include "base64.h"
#include "basedevice.h"
#include "locale_compat.h"
#include "indistandardproperty.h"
#include "indililxml.h"

#include "indiuserio.h"

#include <iostream>
#include <string>
#include <algorithm>

#include <cstdlib>
#include <assert.h>

#define MAXINDIBUF 49152

#if defined(_MSC_VER)
#define snprintf _snprintf
#pragma warning(push)
///@todo Introduce plattform indipendent safe functions as macros to fix this
#pragma warning(disable : 4996)
#endif

static userio io;

INDI::BaseClientQtPrivate::BaseClientQtPrivate(INDI::BaseClientQt *parent)
    : parent(parent)
{
    io.write = [](void *user, const void * ptr, size_t count) -> size_t
    {
        auto self = static_cast<BaseClientQtPrivate *>(user);
        return self->client_socket.write(static_cast<const char *>(ptr), count);
    };

    io.vprintf = [](void *user, const char * format, va_list ap) -> int
    {
        auto self = static_cast<BaseClientQtPrivate *>(user);
        char message[MAXRBUF];
        vsnprintf(message, MAXRBUF, format, ap);
        return self->client_socket.write(message, strlen(message));
    };
}

INDI::BaseClientQt::BaseClientQt(QObject *parent)
    : QObject(parent)
    , d_ptr(new INDI::BaseClientQtPrivate(this))
{
    D_PTR(BaseClientQt);
    connect(&d->client_socket, SIGNAL(readyRead()), this, SLOT(listenINDI()));
    connect(&d->client_socket, SIGNAL(error(QAbstractSocket::SocketError)), this,
            SLOT(processSocketError(QAbstractSocket::SocketError)));
}

INDI::BaseClientQt::~BaseClientQt()
{
    D_PTR(BaseClientQt);
    d->clear();
}

void INDI::BaseClientQtPrivate::clear()
{
    watchDevice.clearDevices();
    blobModes.clear();
}

void INDI::BaseClientQt::setServer(const char *hostname, unsigned int port)
{
    D_PTR(BaseClientQt);
    d->cServer = hostname;
    d->cPort   = port;
}

const char *INDI::BaseClientQt::getHost() const
{
    D_PTR(const BaseClientQt);
    return d->cServer.c_str();
}

int INDI::BaseClientQt::getPort() const
{
    D_PTR(const BaseClientQt);
    return d->cPort;
}

void INDI::BaseClientQt::watchDevice(const char *deviceName)
{
    D_PTR(BaseClientQt);
    d->watchDevice.watchDevice(deviceName);
}

void INDI::BaseClientQt::watchProperty(const char *deviceName, const char *propertyName)
{
    D_PTR(BaseClientQt);
    watchDevice(deviceName);
    d->watchDevice.watchProperty(deviceName, propertyName);
}

bool INDI::BaseClientQt::connectServer()
{
    D_PTR(BaseClientQt);
    d->client_socket.connectToHost(d->cServer.c_str(), d->cPort);

    if (d->client_socket.waitForConnected(d->timeout_sec * 1000) == false)
    {
        d->sConnected = false;
        return false;
    }

    d->clear();


    d->sConnected = true;

    serverConnected();

    QString getProp;
    if (d->watchDevice.isEmpty())
    {
        IUUserIOGetProperties(&io, this, nullptr, nullptr);
        if (verbose)
            IUUserIOGetProperties(userio_file(), stderr, nullptr, nullptr);
    }
    else
    {
        for (const auto &deviceInfo : d->watchDevice /* first: device name, second: device info */)
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

    return true;
}

bool INDI::BaseClientQt::disconnectServer()
{
    D_PTR(BaseClientQt);

    if (d->sConnected == false)
        return true;

    d->sConnected = false;

    d->client_socket.close();

    d->clear();

    d->watchDevice.unwatchDevices();

    serverDisconnected(0);

    return true;
}

void INDI::BaseClientQt::connectDevice(const char *deviceName)
{
    D_PTR(BaseClientQt);
    d->setDriverConnection(true, deviceName);
}

void INDI::BaseClientQt::disconnectDevice(const char *deviceName)
{
    D_PTR(BaseClientQt);
    d->setDriverConnection(false, deviceName);
}

void INDI::BaseClientQtPrivate::setDriverConnection(bool status, const char *deviceName)
{
    INDI::BaseDevice *drv                 = parent->getDevice(deviceName);
    ISwitchVectorProperty *drv_connection = nullptr;

    if (drv == nullptr)
    {
        IDLog("INDI::BaseClientQt: Error. Unable to find driver %s\n", deviceName);
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

        parent->sendNewSwitch(drv_connection);
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

        parent->sendNewSwitch(drv_connection);
    }
}

INDI::BaseDevice *INDI::BaseClientQt::getDevice(const char *deviceName)
{
    D_PTR(BaseClientQt);
    return d->watchDevice.getDeviceByName(deviceName);
}

std::vector<INDI::BaseDevice *> INDI::BaseClientQt::getDevices() const
{
    D_PTR(const BaseClientQt);
    return d->watchDevice.getDevices();
}

void *INDI::BaseClientQt::listenHelper(void *context)
{
    (static_cast<INDI::BaseClientQt *>(context))->listenINDI();
    return nullptr;
}

void INDI::BaseClientQt::listenINDI()
{
    D_PTR(BaseClientQt);
    char buffer[MAXINDIBUF];
    char msg[MAXRBUF];

    if (d->sConnected == false)
        return;

    INDI::LilXmlParser xmlParser;

    while (d->client_socket.bytesAvailable() > 0)
    {
        // TODO QByteArray
        qint64 readBytes = d->client_socket.read(buffer, MAXINDIBUF - 1);
        if (readBytes > 0)
            buffer[readBytes] = '\0';

        auto documents = xmlParser.parseChunk(buffer, readBytes);

        if (documents.size() == 0)
        {
            if (xmlParser.hasErrorMessage())
            {
                IDLog("Bad XML from %s/%d: %s\n%s\n", d->cServer.c_str(), d->cPort, xmlParser.errorMessage(), buffer);
            }
            break;
        }

        for (const auto &doc: documents)
        {
            INDI::LilXmlElement root = doc.root();

            if (verbose)
                    root.print(stderr, 0);

            int err_code = d->dispatchCommand(root, msg);

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
    }
}

int INDI::BaseClientQtPrivate::dispatchCommand(const INDI::LilXmlElement &root, char *errmsg)
{
    // Ignore echoed newXXX
    if (root.tagName().find("new") == 0)
    {
        return 0;       
    }

    // #PS: copied from BaseClient
#if 0
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
#endif

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


/* delete the property in the given device, including widgets and data structs.
 * when last property is deleted, delete the device too.
 * if no property name attribute at all, delete the whole device regardless.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDI::BaseClientQtPrivate::delPropertyCmd(const INDI::LilXmlElement &root, char *errmsg)
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

int INDI::BaseClientQtPrivate::deleteDevice(const char *devName, char *errmsg)
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

/* a general message command received from the device.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDI::BaseClientQtPrivate::messageCmd(const INDI::LilXmlElement &root, char *errmsg)
{
    INDI_UNUSED(errmsg);

    INDI::BaseDevice *dp = watchDevice.getDeviceByName(root.getAttribute("device"));

    if (dp)
    {
        dp->checkMessage(root.handle());
        return 0;
    }

    // #PS: copied from BaseClient
#if 0
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
#endif
    return 0;
}

void INDI::BaseClientQt::newUniversalMessage(std::string message)
{
    IDLog("%s\n", message.c_str());
}

void INDI::BaseClientQt::sendNewText(ITextVectorProperty *tvp)
{
    AutoCNumeric locale;

    tvp->s = IPS_BUSY;
    IUUserIONewText(&io, this, tvp);
}

void INDI::BaseClientQt::sendNewText(const char *deviceName, const char *propertyName, const char *elementName,
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

void INDI::BaseClientQt::sendNewNumber(INumberVectorProperty *nvp)
{
    AutoCNumeric locale;

    nvp->s = IPS_BUSY;
    IUUserIONewNumber(&io, this, nvp);
}

void INDI::BaseClientQt::sendNewNumber(const char *deviceName, const char *propertyName, const char *elementName,
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

void INDI::BaseClientQt::sendNewSwitch(ISwitchVectorProperty *svp)
{
    svp->s = IPS_BUSY;
    IUUserIONewSwitch(&io, this, svp);
}

void INDI::BaseClientQt::sendNewSwitch(const char *deviceName, const char *propertyName, const char *elementName)
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

void INDI::BaseClientQt::startBlob(const char *devName, const char *propName, const char *timestamp)
{
    IUUserIONewBLOBStart(&io, this, devName, propName, timestamp);
}

void INDI::BaseClientQt::sendOneBlob(IBLOB *bp)
{
    IUUserIOBLOBContextOne(
        &io, this,
        bp->name, bp->size, bp->bloblen, bp->blob, bp->format
    );
}

void INDI::BaseClientQt::sendOneBlob(const char *blobName, unsigned int blobSize, const char *blobFormat,
                                     void *blobBuffer)
{
    IUUserIOBLOBContextOne(
        &io, this,
        blobName, blobSize, blobSize, blobBuffer, blobFormat
    );
}

void INDI::BaseClientQt::finishBlob()
{
    IUUserIONewBLOBFinish(&io, this);
}

void INDI::BaseClientQt::setBLOBMode(BLOBHandling blobH, const char *dev, const char *prop)
{
    D_PTR(BaseClientQt);
    if (!dev[0])
        return;

    auto *bMode = d->findBLOBMode(std::string(dev), prop ? std::string(prop) : std::string());

    if (bMode == nullptr)
    {
        INDI::BLOBMode newMode;
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

    IUUserIOEnableBLOB(&io, this, dev, prop, blobH);
}

BLOBHandling INDI::BaseClientQt::getBLOBMode(const char *dev, const char *prop)
{
    D_PTR(BaseClientQt);
    BLOBHandling bHandle = B_ALSO;

    auto *bMode = d->findBLOBMode(dev, (prop ? std::string(prop) : std::string()));

    if (bMode)
        bHandle = bMode->blobMode;

    return bHandle;
}

INDI::BLOBMode *INDI::BaseClientQtPrivate::findBLOBMode(const std::string &device, const std::string &property)
{
    for (auto &blob : blobModes)
    {
        if (blob.device == device && (property.empty() || blob.property == property))
            return &blob;
    }

    return nullptr;
}

void INDI::BaseClientQt::processSocketError(QAbstractSocket::SocketError socketError)
{
    D_PTR(BaseClientQt);
    if (d->sConnected == false)
        return;

    // TODO Handle what happens on socket failure!
    INDI_UNUSED(socketError);
    IDLog("Socket Error: %s\n", d->client_socket.errorString().toLatin1().constData());
    fprintf(stderr, "INDI server %s/%d disconnected.\n", d->cServer.c_str(), d->cPort);
    d->client_socket.close();
    // Let client handle server disconnection
    serverDisconnected(-1);
}

bool INDI::BaseClientQt::getDevices(std::vector<INDI::BaseDevice *> &deviceList, uint16_t driverInterface )
{
    D_PTR(BaseClientQt);
    for (auto &it: d->watchDevice)
    {
        if (it.second.device->getDriverInterface() & driverInterface)
            deviceList.push_back(it.second.device.get());
    }

    return (deviceList.size() > 0);
}

bool INDI::BaseClientQt::isServerConnected() const
{
    D_PTR(const BaseClientQt);
    return d->sConnected;
}

void INDI::BaseClientQt::setConnectionTimeout(uint32_t seconds, uint32_t microseconds)
{
    D_PTR(BaseClientQt);
    d->timeout_sec = seconds;
    d->timeout_us  = microseconds;
}
void INDI::BaseClientQt::setVerbose(bool enable)
{
    D_PTR(BaseClientQt);
    d->verbose = enable;
}

bool INDI::BaseClientQt::isVerbose() const
{
    D_PTR(const BaseClientQt);
    return d->verbose;
}

#if defined(_MSC_VER)
#undef snprintf
#pragma warning(pop)
#endif
