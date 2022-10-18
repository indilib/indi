/*******************************************************************************
  Copyright(c) 2016 Jasem Mutlaq. All rights reserved.
  Copyright(c) 2022 Pawel Soja <kernel32.pl@gmail.com>

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

#include "abstractbaseclient.h"
#include "abstractbaseclient_p.h"

#include "indiuserio.h"
#include "locale_compat.h"
#include "indistandardproperty.h"

#if defined(_MSC_VER)
#define snprintf _snprintf
#pragma warning(push)
///@todo Introduce plattform indipendent safe functions as macros to fix this
#pragma warning(disable : 4996)
#endif

namespace INDI
{

userio AbstractBaseClientPrivate::io;

// AbstractBaseClientPrivate

AbstractBaseClientPrivate::AbstractBaseClientPrivate(AbstractBaseClient *parent)
    : parent(parent)
{ }

void AbstractBaseClientPrivate::clear()
{
    watchDevice.clearDevices();
    blobModes.clear();
}


int AbstractBaseClientPrivate::dispatchCommand(const LilXmlElement &root, char *errmsg)
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
        BaseDevice *device = new BaseDevice();
        device->setMediator(parent);
        return device;
    });
}

int AbstractBaseClientPrivate::deleteDevice(const char *devName, char *errmsg)
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
int AbstractBaseClientPrivate::delPropertyCmd(const LilXmlElement &root, char *errmsg)
{
    /* dig out device and optional property name */
    BaseDevice *dp = watchDevice.getDeviceByName(root.getAttribute("device"));

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
int AbstractBaseClientPrivate::messageCmd(const LilXmlElement &root, char *errmsg)
{
    INDI_UNUSED(errmsg);

    BaseDevice *dp = watchDevice.getDeviceByName(root.getAttribute("device"));

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

void AbstractBaseClientPrivate::userIoGetProperties()
{
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
}


void AbstractBaseClientPrivate::setDriverConnection(bool status, const char *deviceName)
{
    BaseDevice *drv                 = parent->getDevice(deviceName);
    ISwitchVectorProperty *drv_connection = nullptr;

    if (drv == nullptr)
    {
        IDLog("BaseClientQt: Error. Unable to find driver %s\n", deviceName);
        return;
    }

    drv_connection = drv->getSwitch(SP::CONNECTION);

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

BLOBMode *AbstractBaseClientPrivate::findBLOBMode(const std::string &device, const std::string &property)
{
    for (auto &blob : blobModes)
    {
        if (blob.device == device && (property.empty() || blob.property == property))
            return &blob;
    }

    return nullptr;
}

// AbstractBaseClient

AbstractBaseClient::AbstractBaseClient(std::unique_ptr<AbstractBaseClientPrivate> &&d)
    : d_ptr_indi(std::move(d))
{ }

AbstractBaseClient::~AbstractBaseClient()
{

}

void AbstractBaseClient::setServer(const char *hostname, unsigned int port)
{
    D_PTR(AbstractBaseClient);
    d->cServer = hostname;
    d->cPort   = port;
}

const char *AbstractBaseClient::getHost() const
{
    D_PTR(const AbstractBaseClient);
    return d->cServer.c_str();
}

int AbstractBaseClient::getPort() const
{
    D_PTR(const AbstractBaseClient);
    return d->cPort;
}

bool AbstractBaseClient::connectServer()
{
    return false;
}

bool AbstractBaseClient::disconnectServer()
{
    return false;
}

bool AbstractBaseClient::isServerConnected() const
{
    D_PTR(const AbstractBaseClient);
    return d->sConnected;
}

void AbstractBaseClient::setConnectionTimeout(uint32_t seconds, uint32_t microseconds)
{
    D_PTR(AbstractBaseClient);
    d->timeout_sec = seconds;
    d->timeout_us  = microseconds;
}

void AbstractBaseClient::setVerbose(bool enable)
{
    D_PTR(AbstractBaseClient);
    d->verbose = enable;
}

bool AbstractBaseClient::isVerbose() const
{
    D_PTR(const AbstractBaseClient);
    return d->verbose;
}

void AbstractBaseClient::watchDevice(const char *deviceName)
{
    D_PTR(AbstractBaseClient);
    d->watchDevice.watchDevice(deviceName);
}

void AbstractBaseClient::watchProperty(const char *deviceName, const char *propertyName)
{
    D_PTR(AbstractBaseClient);
    watchDevice(deviceName);
    d->watchDevice.watchProperty(deviceName, propertyName);
}


void AbstractBaseClient::connectDevice(const char *deviceName)
{
    D_PTR(AbstractBaseClient);
    d->setDriverConnection(true, deviceName);
}

void AbstractBaseClient::disconnectDevice(const char *deviceName)
{
    D_PTR(AbstractBaseClient);
    d->setDriverConnection(false, deviceName);
}

BaseDevice *AbstractBaseClient::getDevice(const char *deviceName)
{
    D_PTR(AbstractBaseClient);
    return d->watchDevice.getDeviceByName(deviceName);
}

std::vector<BaseDevice *> AbstractBaseClient::getDevices() const
{
    D_PTR(const AbstractBaseClient);
    return d->watchDevice.getDevices();
}

bool AbstractBaseClient::getDevices(std::vector<BaseDevice *> &deviceList, uint16_t driverInterface )
{
    D_PTR(AbstractBaseClient);
    for (auto &it: d->watchDevice)
    {
        if (it.second.device->getDriverInterface() & driverInterface)
            deviceList.push_back(it.second.device.get());
    }

    return (deviceList.size() > 0);
}


void AbstractBaseClient::setBLOBMode(BLOBHandling blobH, const char *dev, const char *prop)
{
    D_PTR(AbstractBaseClient);
    if (!dev[0])
        return;

    auto *bMode = d->findBLOBMode(std::string(dev), prop ? std::string(prop) : std::string());

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

    IUUserIOEnableBLOB(&d->io, this, dev, prop, blobH);
}

BLOBHandling AbstractBaseClient::getBLOBMode(const char *dev, const char *prop)
{
    D_PTR(AbstractBaseClient);
    BLOBHandling bHandle = B_ALSO;

    auto *bMode = d->findBLOBMode(dev, (prop ? std::string(prop) : std::string()));

    if (bMode)
        bHandle = bMode->blobMode;

    return bHandle;
}


void AbstractBaseClient::sendNewText(ITextVectorProperty *tvp)
{
    D_PTR(AbstractBaseClient);
    AutoCNumeric locale;

    tvp->s = IPS_BUSY;
    IUUserIONewText(&d->io, this, tvp);
}

void AbstractBaseClient::sendNewText(const char *deviceName, const char *propertyName, const char *elementName,
                                     const char *text)
{
    BaseDevice *drv = getDevice(deviceName);

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

void AbstractBaseClient::sendNewNumber(INumberVectorProperty *nvp)
{
    D_PTR(AbstractBaseClient);
    AutoCNumeric locale;

    nvp->s = IPS_BUSY;
    IUUserIONewNumber(&d->io, this, nvp);
}

void AbstractBaseClient::sendNewNumber(const char *deviceName, const char *propertyName, const char *elementName,
                                       double value)
{
    BaseDevice *drv = getDevice(deviceName);

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

void AbstractBaseClient::sendNewSwitch(ISwitchVectorProperty *svp)
{
    D_PTR(AbstractBaseClient);
    svp->s = IPS_BUSY;
    IUUserIONewSwitch(&d->io, this, svp);
}

void AbstractBaseClient::sendNewSwitch(const char *deviceName, const char *propertyName, const char *elementName)
{
    BaseDevice *drv = getDevice(deviceName);

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

void AbstractBaseClient::startBlob(const char *devName, const char *propName, const char *timestamp)
{
    D_PTR(AbstractBaseClient);
    IUUserIONewBLOBStart(&d->io, this, devName, propName, timestamp);
}

void AbstractBaseClient::sendOneBlob(IBLOB *bp)
{
    D_PTR(AbstractBaseClient);
    IUUserIOBLOBContextOne(
        &d->io, this,
        bp->name, bp->size, bp->bloblen, bp->blob, bp->format
    );
}

void AbstractBaseClient::sendOneBlob(const char *blobName, unsigned int blobSize, const char *blobFormat,
                                     void *blobBuffer)
{
    D_PTR(AbstractBaseClient);
    IUUserIOBLOBContextOne(
        &d->io, this,
        blobName, blobSize, blobSize, blobBuffer, blobFormat
    );
}

void AbstractBaseClient::finishBlob()
{
    D_PTR(AbstractBaseClient);
    IUUserIONewBLOBFinish(&d->io, this);
}

void AbstractBaseClient::newUniversalMessage(std::string message)
{
    IDLog("%s\n", message.c_str());
}

}

#if defined(_MSC_VER)
#undef snprintf
#pragma warning(pop)
#endif
