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

#include "base64.h"
#include "basedevice.h"
#include "locale_compat.h"
#include "indistandardproperty.h"

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

INDI::BaseClientQt::BaseClientQt(QObject *parent) : QObject(parent), cServer("localhost"), cPort(7624)
{
    io.write = [](void *user, const void * ptr, size_t count) -> size_t
    {
        BaseClientQt *self = static_cast<BaseClientQt *>(user);
        return self->client_socket.write(static_cast<const char *>(ptr), count);
    };

    io.vprintf = [](void *user, const char * format, va_list ap) -> int
    {
        BaseClientQt *self = static_cast<BaseClientQt *>(user);
        char message[MAXRBUF];
        vsnprintf(message, MAXRBUF, format, ap);
        return self->client_socket.write(message, strlen(message));
    };

    sConnected = false;
    verbose    = false;
    lillp      = nullptr;

    timeout_sec = 3;
    timeout_us  = 0;

    connect(&client_socket, SIGNAL(readyRead()), this, SLOT(listenINDI()));
    connect(&client_socket, SIGNAL(error(QAbstractSocket::SocketError)), this,
            SLOT(processSocketError(QAbstractSocket::SocketError)));
}

INDI::BaseClientQt::~BaseClientQt()
{
    clear();
}

void INDI::BaseClientQt::clear()
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

void INDI::BaseClientQt::setServer(const char *hostname, unsigned int port)
{
    cServer = hostname;
    cPort   = port;
}

void INDI::BaseClientQt::watchDevice(const char *deviceName)
{
    // Watch for duplicates. Should have used std::set from the beginning but let's
    // avoid changing API now.
    if (std::find(cDeviceNames.begin(), cDeviceNames.end(), deviceName) != cDeviceNames.end())
        return;

    cDeviceNames.push_back(deviceName);
}

void INDI::BaseClientQt::watchProperty(const char *deviceName, const char *propertyName)
{
    watchDevice(deviceName);
    cWatchProperties[deviceName].insert(propertyName);
}

bool INDI::BaseClientQt::connectServer()
{
    client_socket.connectToHost(cServer.c_str(), cPort);

    if (client_socket.waitForConnected(timeout_sec * 1000) == false)
    {
        sConnected = false;
        return false;
    }

    clear();

    lillp = newLilXML();

    sConnected = true;

    serverConnected();

    QString getProp;
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
            IUUserIOGetProperties(&io, this, oneDevice.c_str(), nullptr);
            if (verbose)
                IUUserIOGetProperties(userio_file(), stderr, oneDevice.c_str(), nullptr);

            // #PS: missing code? see INDI::BaseClient::listenINDI()
        }
    }

    return true;
}

bool INDI::BaseClientQt::disconnectServer()
{
    if (sConnected == false)
        return true;

    sConnected = false;

    client_socket.close();
    if (lillp)
    {
        delLilXML(lillp);
        lillp = nullptr;
    }

    clear();

    cDeviceNames.clear();

    serverDisconnected(0);

    return true;
}

void INDI::BaseClientQt::connectDevice(const char *deviceName)
{
    setDriverConnection(true, deviceName);
}

void INDI::BaseClientQt::disconnectDevice(const char *deviceName)
{
    setDriverConnection(false, deviceName);
}

void INDI::BaseClientQt::setDriverConnection(bool status, const char *deviceName)
{
    INDI::BaseDevice *drv                 = getDevice(deviceName);
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

INDI::BaseDevice *INDI::BaseClientQt::getDevice(const char *deviceName)
{
    for (auto &dev : cDevices)
    {
        if (!strcmp(deviceName, dev->getDeviceName()))
            return dev;
    }
    return nullptr;
}

void *INDI::BaseClientQt::listenHelper(void *context)
{
    (static_cast<INDI::BaseClientQt *>(context))->listenINDI();
    return nullptr;
}

void INDI::BaseClientQt::listenINDI()
{
    char buffer[MAXINDIBUF];
    char errorMsg[MAXRBUF];
    int err_code = 0;

    XMLEle **nodes;
    int inode = 0;

    if (sConnected == false)
        return;

    while (client_socket.bytesAvailable() > 0)
    {
        qint64 readBytes = client_socket.read(buffer, MAXINDIBUF - 1);
        if (readBytes > 0)
            buffer[readBytes] = '\0';

        nodes = parseXMLChunk(lillp, buffer, readBytes, errorMsg);
        if (!nodes)
        {
            if (errorMsg[0])
            {
                fprintf(stderr, "Bad XML from %s/%ud: %s\n%s\n", cServer.c_str(), cPort, errorMsg, buffer);
                return;
            }
            return;
        }
        XMLEle *root = nodes[inode];
        while (root)
        {
            if (verbose)
                prXMLEle(stderr, root, 0);

            if ((err_code = dispatchCommand(root, errorMsg)) < 0)
            {
                // Silenty ignore property duplication errors
                if (err_code != INDI_PROPERTY_DUPLICATED)
                {
                    IDLog("Dispatch command error(%d): %s\n", err_code, errorMsg);
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

int INDI::BaseClientQt::dispatchCommand(XMLEle *root, char *errmsg)
{
    if (!strcmp(tagXMLEle(root), "message"))
        return messageCmd(root, errmsg);
    else if (!strcmp(tagXMLEle(root), "delProperty"))
        return delPropertyCmd(root, errmsg);
    // Just ignore any getProperties we might get
    else if (!strcmp(tagXMLEle(root), "getProperties"))
        return INDI_PROPERTY_DUPLICATED;

    /* Get the device, if not available, create it */
    INDI::BaseDevice *dp = findDev(root, 1, errmsg);
    if (dp == nullptr)
    {
        strcpy(errmsg, "No device available and none was created");
        return INDI_DEVICE_NOT_FOUND;
    }

    // Ignore echoed newXXX
    if (strstr(tagXMLEle(root), "new"))
        return 0;

    // If device is set to BLOB_ONLY, we ignore everything else
    // not related to blobs
    if (getBLOBMode(dp->getDeviceName()) == B_ONLY)
    {
        if (!strcmp(tagXMLEle(root), "defBLOBVector"))
            return dp->buildProp(root, errmsg);
        else if (!strcmp(tagXMLEle(root), "setBLOBVector"))
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

    if ((!strcmp(tagXMLEle(root), "defTextVector")) || (!strcmp(tagXMLEle(root), "defNumberVector")) ||
            (!strcmp(tagXMLEle(root), "defSwitchVector")) || (!strcmp(tagXMLEle(root), "defLightVector")) ||
            (!strcmp(tagXMLEle(root), "defBLOBVector")))
        return dp->buildProp(root, errmsg);
    else if (!strcmp(tagXMLEle(root), "setTextVector") || !strcmp(tagXMLEle(root), "setNumberVector") ||
             !strcmp(tagXMLEle(root), "setSwitchVector") || !strcmp(tagXMLEle(root), "setLightVector") ||
             !strcmp(tagXMLEle(root), "setBLOBVector"))
        return dp->setValue(root, errmsg);

    return INDI_DISPATCH_ERROR;
}

/* delete the property in the given device, including widgets and data structs.
 * when last property is deleted, delete the device too.
 * if no property name attribute at all, delete the whole device regardless.
 * return 0 if ok, else -1 with reason in errmsg[].
 */
int INDI::BaseClientQt::delPropertyCmd(XMLEle *root, char *errmsg)
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
            snprintf(errmsg, MAXRBUF, "Cannot delete property %s as it is not defined yet. Check driver.", valuXMLAtt(ap));
            return -1;
        }

        removeProperty(rProp);
        int errCode = dp->removeProperty(valuXMLAtt(ap), errmsg);

        return errCode;
    }
    // delete the whole device
    else
        return deleteDevice(dp->getDeviceName(), errmsg);
}

int INDI::BaseClientQt::deleteDevice(const char *devName, char *errmsg)
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

INDI::BaseDevice *INDI::BaseClientQt::findDev(const char *devName, char *errmsg)
{
    std::vector<INDI::BaseDevice *>::const_iterator devicei;

    for (devicei = cDevices.begin(); devicei != cDevices.end(); devicei++)
    {
        if (!strcmp(devName, (*devicei)->getDeviceName()))
            return (*devicei);
    }

    snprintf(errmsg, MAXRBUF, "Device %s not found", devName);
    return nullptr;
}

/* add new device */
INDI::BaseDevice *INDI::BaseClientQt::addDevice(XMLEle *dep, char *errmsg)
{
    //devicePtr dp(new INDI::BaseDriver());
    INDI::BaseDevice *dp = new INDI::BaseDevice();
    XMLAtt *ap;
    char *device_name;

    /* allocate new INDI::BaseDriver */
    ap = findXMLAtt(dep, "device");
    if (!ap)
    {
        strncpy(errmsg, "Unable to find device attribute in XML element. Cannot add device.", MAXRBUF);
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

INDI::BaseDevice *INDI::BaseClientQt::findDev(XMLEle *root, int create, char *errmsg)
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
int INDI::BaseClientQt::messageCmd(XMLEle *root, char *errmsg)
{
    INDI::BaseDevice *dp = findDev(root, 0, errmsg);

    if (dp)
        dp->checkMessage(root);

    return (0);
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
    if (!dev[0])
        return;

    BLOBMode *bMode = findBLOBMode(std::string(dev), prop ? std::string(prop) : std::string());

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

BLOBHandling INDI::BaseClientQt::getBLOBMode(const char *dev, const char *prop)
{
    BLOBHandling bHandle = B_ALSO;

    BLOBMode *bMode = findBLOBMode(dev, (prop ? std::string(prop) : std::string()));

    if (bMode)
        bHandle = bMode->blobMode;

    return bHandle;
}

INDI::BaseClientQt::BLOBMode *INDI::BaseClientQt::findBLOBMode(const std::string &device, const std::string &property)
{
    for (auto &blob : blobModes)
    {
        if (blob->device == device && blob->property == property)
            return blob;
    }

    return nullptr;
}

void INDI::BaseClientQt::processSocketError(QAbstractSocket::SocketError socketError)
{
    if (sConnected == false)
        return;

    // TODO Handle what happens on socket failure!
    INDI_UNUSED(socketError);
    IDLog("Socket Error: %s\n", client_socket.errorString().toLatin1().constData());
    fprintf(stderr, "INDI server %s/%d disconnected.\n", cServer.c_str(), cPort);
    delLilXML(lillp);
    client_socket.close();
    // Let client handle server disconnection
    serverDisconnected(-1);
}

bool INDI::BaseClientQt::getDevices(std::vector<INDI::BaseDevice *> &deviceList, uint16_t driverInterface )
{
    for (INDI::BaseDevice *device : cDevices)
    {
        if (device->getDriverInterface() & driverInterface)
            deviceList.push_back(device);
    }

    return (deviceList.size() > 0);
}

bool INDI::BaseClientQt::isServerConnected() const
{
    return sConnected;
}

#if defined(_MSC_VER)
#undef snprintf
#pragma warning(pop)
#endif
