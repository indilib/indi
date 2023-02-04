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

#include "basedevice.h"
#include "basedevice_p.h"

#include "base64.h"
#include "config.h"
#include "indicom.h"
#include "indistandardproperty.h"
#include "locale_compat.h"

#include "indipropertytext.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertylight.h"
#include "indipropertyblob.h"

#ifdef ENABLE_INDI_SHARED_MEMORY
# include "sharedblob_parse.h"
#endif

#include <cerrno>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <zlib.h>
#include <sys/stat.h>
#include <thread>
#include <chrono>
#include <algorithm>

#if defined(_MSC_VER)
#define snprintf _snprintf
#pragma warning(push)
///@todo Introduce platform independent safe functions as macros to fix this
#pragma warning(disable : 4996)
#endif

namespace INDI
{

BaseDevicePrivate::BaseDevicePrivate()
{
    static char indidev[] = "INDIDEV=";

    if (getenv("INDIDEV") != nullptr)
    {
        deviceName = getenv("INDIDEV");
        putenv(indidev);
    }
}

BaseDevicePrivate::~BaseDevicePrivate()
{
    pAll.clear();
}

BaseDevice::BaseDevice()
    : d_ptr(BaseDevicePrivate::invalid())
{ }

BaseDevice::~BaseDevice()
{ }

BaseDevice::BaseDevice(BaseDevicePrivate &dd)
    : d_ptr(&dd)
{ }

BaseDevice::BaseDevice(const std::shared_ptr<BaseDevicePrivate> &dd)
    : d_ptr(dd)
{ }

INDI::PropertyNumber BaseDevice::getNumber(const char *name) const
{
    return getProperty(name, INDI_NUMBER);
}

INDI::PropertyText BaseDevice::getText(const char *name) const
{
    return getProperty(name, INDI_TEXT);
}

INDI::PropertySwitch BaseDevice::getSwitch(const char *name) const
{
    return getProperty(name, INDI_SWITCH);
}

INDI::PropertyLight BaseDevice::getLight(const char *name) const
{
    return getProperty(name, INDI_LIGHT);
}

INDI::PropertyBlob BaseDevice::getBLOB(const char *name) const
{
    return getProperty(name, INDI_BLOB);
}

IPState BaseDevice::getPropertyState(const char *name) const
{
    for (const auto &oneProp : getProperties())
        if (oneProp.isNameMatch(name))
            return oneProp.getState();

    return IPS_IDLE;
}

IPerm BaseDevice::getPropertyPermission(const char *name) const
{
    for (const auto &oneProp : getProperties())
        if (oneProp.isNameMatch(name))
            return oneProp.getPermission();

    return IP_RO;
}

void *BaseDevice::getRawProperty(const char *name, INDI_PROPERTY_TYPE type) const
{
    INDI::Property prop = getProperty(name, type);
    return prop ? prop.getProperty() : nullptr;
}

INDI::Property BaseDevice::getProperty(const char *name, INDI_PROPERTY_TYPE type) const
{
    D_PTR(const BaseDevice);
    std::lock_guard<std::mutex> lock(d->m_Lock);

    for (const auto &oneProp : getProperties())
    {
        if (type != oneProp.getType() && type != INDI_UNKNOWN)
            continue;

        if (!oneProp.getRegistered())
            continue;

        if (oneProp.isNameMatch(name))
            return oneProp;
    }

    return INDI::Property();
}

BaseDevice::Properties BaseDevice::getProperties()
{
    D_PTR(BaseDevice);
    return d->pAll;
}

const BaseDevice::Properties BaseDevice::getProperties() const
{
    D_PTR(const BaseDevice);
    return d->pAll;
}

int BaseDevice::removeProperty(const char *name, char *errmsg)
{
    D_PTR(BaseDevice);
    int result = INDI_PROPERTY_INVALID;

    std::lock_guard<std::mutex> lock(d->m_Lock);

    d->pAll.erase_if([&name, &result](INDI::Property & prop) -> bool
    {
#if 0
        if (prop.isNameMatch(name))
        {
            // JM 2021-04-28: delete later. We perform the actual delete after 100ms to give clients a chance to remove the object.
            // This is necessary when rapid define-delete-define sequences are made.
            // This HACK is not ideal. We need to start using std::shared_ptr for this purpose soon, but this will be a major change to the
            // interface. Perhaps for INDI 2.0
            std::thread([prop]
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }).detach();
            result = 0;
            return true;
        }
        else
        {
            return false;
        }
#endif
        if (prop.isNameMatch(name))
        {
            result = 0;
            return true;
        }
        else
            return false;
    });

    if (result != 0)
        snprintf(errmsg, MAXRBUF, "Error: Property %s not found in device %s.", name, getDeviceName());

    return result;
}

std::string BaseDevice::getSharedFilePath(std::string fileName)
{
    std::string pathName;

    struct stat st;

    // absolute file path
    if (stat(fileName.c_str(), &st) == 0)
    {
        pathName = fileName;
        return pathName;
    }

    // get base name of file
    const size_t lastSlashIdx = fileName.find_last_of("\\/");
    if (std::string::npos != lastSlashIdx)
    {
        fileName.erase(0, lastSlashIdx + 1);
    }

    const char * indiprefix = getenv("INDIPREFIX");
    if (indiprefix)
    {
#if defined(OSX_EMBEDED_MODE)
        pathName  = std::string(indiprefix) + "/Contents/Resources/" + fileName;
#elif defined(__APPLE__)
        pathName  = std::string(indiprefix) + "/Contents/Resources/DriverSupport/" + fileName;
#else
        pathName  = std::string(indiprefix) + "/share/indi/" + fileName;
#endif
    }
    else
    {
        pathName = std::string(DATA_INSTALL_DIR) + "/" + fileName;
    }
    return pathName;
}


static std::string sGetSheletonFilePath(std::string fileName)
{
    std::string pathName;

    struct stat st;

    // ENV file path
    const char *indiskel = getenv("INDISKEL");
    if (indiskel)
    {
        pathName = indiskel;

        IDLog("Using INDISKEL %s\n", pathName.c_str());
        return pathName;
    }

    // absolute file path
    if (stat(fileName.c_str(), &st) == 0)
    {
        pathName = fileName;
        IDLog("Using %s\n", pathName.c_str());
        return pathName;
    }

    // get base name of file
    const size_t lastSlashIdx = fileName.find_last_of("\\/");
    if (std::string::npos != lastSlashIdx)
    {
        fileName.erase(0, lastSlashIdx + 1);
    }

    const char * indiprefix = getenv("INDIPREFIX");
    if (indiprefix)
    {
#if defined(OSX_EMBEDED_MODE)
        pathName  = std::string(indiprefix) + "/Contents/Resources/" + fileName;
#elif defined(__APPLE__)
        pathName  = std::string(indiprefix) + "/Contents/Resources/DriverSupport/" + fileName;
#else
        pathName  = std::string(indiprefix) + "/share/indi/" + fileName;
#endif
    }
    else
    {
        pathName = std::string(DATA_INSTALL_DIR) + "/" + fileName;
    }
    IDLog("Using prefix %s\n", pathName.c_str());
    return pathName;
}

bool BaseDevice::buildSkeleton(const char *filename)
{
    D_PTR(BaseDevice);

    LilXmlDocument document = d->xmlParser.readFromFile(sGetSheletonFilePath(filename));

    if(!document.isValid())
    {
        IDLog("Unable to parse skeleton XML: %s", d->xmlParser.errorMessage());
        return false;
    }

    char errmsg[MAXRBUF];

    for (const auto &element : document.root().getElements())
    {
        buildProp(element, errmsg, true);
    }

    return true;
}

int BaseDevice::buildProp(const INDI::LilXmlElement &root, char *errmsg, bool isDynamic)
{
    D_PTR(BaseDevice);

    // only for check, #PS: remove
    {
        char *rname, *rdev;
        if (crackDN(root.handle(), &rdev, &rname, errmsg) < 0)
            return -1;
    }

    // find type of tag
    static const std::map<INDI_PROPERTY_TYPE, std::string> tagTypeName =
    {
        {INDI_NUMBER, "defNumberVector"},
        {INDI_SWITCH, "defSwitchVector"},
        {INDI_TEXT,   "defTextVector"},
        {INDI_LIGHT,  "defLightVector"},
        {INDI_BLOB,   "defBLOBVector"}
    };

    const auto rootTagName = root.tagName();
    const auto rootTagType = std::find_if(tagTypeName.begin(), tagTypeName.end(), [&rootTagName](const auto & it)
    {
        return rootTagName == it.second;
    });

    if (rootTagType == tagTypeName.end())
    {
        snprintf(errmsg, MAXRBUF, "INDI: <%s> Unable to process tag", rootTagName.c_str());
        return -1;
    }

    //
    const char * propertyName = root.getAttribute("name").toCString();

    if (getProperty(propertyName).isValid())
    {
        return INDI_PROPERTY_DUPLICATED;
    }

    if (d->deviceName.empty())
        d->deviceName = root.getAttribute("device").toString();

    INDI::Property property;
    switch (rootTagType->first)
    {
        case INDI_NUMBER:
        {
            INDI::PropertyNumber typedProperty {0};
            for (const auto &element : root.getElementsByTagName("defNumber"))
            {
                INDI::WidgetViewNumber widget;

                widget.setParent(typedProperty.getNumber());

                widget.setName   (element.getAttribute("name"));
                widget.setLabel  (element.getAttribute("label"));

                widget.setFormat (element.getAttribute("format"));
                widget.setMin    (element.getAttribute("min"));
                widget.setMax    (element.getAttribute("max"));
                widget.setStep   (element.getAttribute("step"));

                widget.setValue  (element.context().toDoubleSexa());

                if (!widget.isNameMatch(""))
                    typedProperty.push(std::move(widget));
            }
            property = typedProperty;
            break;
        }
        case INDI_SWITCH:
        {
            INDI::PropertySwitch typedProperty {0};
            typedProperty.setRule(root.getAttribute("rule"));
            for (const auto &element : root.getElementsByTagName("defSwitch"))
            {
                INDI::WidgetViewSwitch widget;

                widget.setParent(typedProperty.getSwitch());

                widget.setName   (element.getAttribute("name"));
                widget.setLabel  (element.getAttribute("label"));

                widget.setState  (element.context());

                if (!widget.isNameMatch(""))
                    typedProperty.push(std::move(widget));
            }
            property = typedProperty;
            break;
        }

        case INDI_TEXT:
        {
            INDI::PropertyText typedProperty {0};
            for (const auto &element : root.getElementsByTagName("defText"))
            {
                INDI::WidgetViewText widget;

                widget.setParent(typedProperty.getText());

                widget.setName   (element.getAttribute("name"));
                widget.setLabel  (element.getAttribute("label"));

                widget.setText   (element.context());

                if (!widget.isNameMatch(""))
                    typedProperty.push(std::move(widget));
            }
            property = typedProperty;
            break;
        }

        case INDI_LIGHT:
        {
            INDI::PropertyLight typedProperty {0};
            for (const auto &element : root.getElementsByTagName("defLight"))
            {
                INDI::WidgetViewLight widget;

                widget.setParent(typedProperty.getLight());

                widget.setName   (element.getAttribute("name"));
                widget.setLabel  (element.getAttribute("label"));

                widget.setState  (element.context());

                if (!widget.isNameMatch(""))
                    typedProperty.push(std::move(widget));
            }
            property = typedProperty;
            break;
        }

        case INDI_BLOB:
        {
            INDI::PropertyBlob typedProperty {0};
            for (const auto &element : root.getElementsByTagName("defBLOB"))
            {
                INDI::WidgetViewBlob widget;

                widget.setParent(typedProperty.getBLOB());

                widget.setName   (element.getAttribute("name"));
                widget.setLabel  (element.getAttribute("label"));

                widget.setFormat (element.getAttribute("format"));

                if (!widget.isNameMatch(""))
                    typedProperty.push(std::move(widget));
            }
            property = typedProperty;
            break;
        }

        case INDI_UNKNOWN: // it will never happen
            return -1;
    }

    if (!property.isValid())
    {
        IDLog("%s: invalid name '%s'\n", propertyName, rootTagName.c_str());
        return 0;
    }

    if (property.isEmpty())
    {
        IDLog("%s: %s with no valid members\n", propertyName, rootTagName.c_str());
        return 0;
    }

    property.setBaseDevice (*this);
    property.setName       (propertyName);
    property.setDynamic    (isDynamic);
    property.setDeviceName (getDeviceName());

    property.setLabel      (root.getAttribute("label"));
    property.setGroupName  (root.getAttribute("group"));
    property.setState      (root.getAttribute("state"));
    property.setTimeout    (root.getAttribute("timeout"));

    if (rootTagType->first != INDI_LIGHT)
    {
        property.setPermission(root.getAttribute("perm").toIPerm());
    }

    d->addProperty(property);

    // IDLog("Adding number property %s to list.\n", property.getName());
    d->mediateNewProperty(property);

    return (0);
}

bool BaseDevice::isConnected() const
{
    auto svp = getSwitch(INDI::SP::CONNECTION);
    if (!svp)
        return false;

    auto sp = svp.findWidgetByName("CONNECT");

    return sp && sp->getState() == ISS_ON && svp.getState() == IPS_OK;
}

void BaseDevice::attach()
{
    D_PTR(BaseDevice);
    d->mediateNewDevice(*this);
}

void BaseDevice::detach()
{
    D_PTR(BaseDevice);
    d->mediateRemoveDevice(*this);
}

// helper for BaseDevice::setValue
template <typename TypedProperty>
static void for_property(
    //XMLEle *root,
    const LilXmlElement &root,
    INDI::Property &property,
    const std::function<void(const LilXmlElement &, INDI::WidgetView<typename TypedProperty::ViewType> *)> &function
)
{
    TypedProperty typedProperty = property;

    for (const auto &element : root.getElements())
    {
        auto * item = typedProperty.findWidgetByName(element.getAttribute("name"));
        if (item)
            function(element, item);
    }

    typedProperty.emitUpdate();
}

/*
 * return 0 if ok else -1 with reason in errmsg
 */
int BaseDevice::setValue(const INDI::LilXmlElement &root, char *errmsg)
{
    D_PTR(BaseDevice);

    if (!root.getAttribute("name").isValid())
    {
        snprintf(errmsg, MAXRBUF, "INDI: <%s> unable to find name attribute", root.tagName().c_str());
        return -1;
    }

    // check message
    checkMessage(root.handle());

    // find type of tag
    static const std::map<INDI_PROPERTY_TYPE, std::string> tagTypeName =
    {
        {INDI_NUMBER, "setNumberVector"},
        {INDI_SWITCH, "setSwitchVector"},
        {INDI_TEXT,   "setTextVector"},
        {INDI_LIGHT,  "setLightVector"},
        {INDI_BLOB,   "setBLOBVector"}
    };

    const auto rootTagName = root.tagName();
    const auto rootTagType = std::find_if(tagTypeName.begin(), tagTypeName.end(), [&rootTagName](const auto & it)
    {
        return rootTagName == it.second;
    });

    if (rootTagType == tagTypeName.end())
    {
        snprintf(errmsg, MAXRBUF, "INDI: <%s> Unable to process tag", rootTagName.c_str());
        return -1;
    }

    // update generic values
    const char * propertyName = root.getAttribute("name").toCString();

    INDI::Property property = getProperty(propertyName, rootTagType->first);

    if (!property.isValid())
    {
        snprintf(errmsg, MAXRBUF, "INDI: Could not find property %s in %s", propertyName, getDeviceName());
        return -1;
    }

    // 1. set overall property state, if any
    {
        bool ok = false;
        property.setState(root.getAttribute("state").toIPState(&ok));

        if (!ok)
        {
            snprintf(errmsg, MAXRBUF, "INDI: <%s> bogus state %s for %s", rootTagName.c_str(), root.getAttribute("state").toCString(),
                     propertyName);
            return -1;
        }
    }

    // 2. allow changing the timeout
    {
        AutoCNumeric locale;
        bool ok = false;
        auto timeoutValue = root.getAttribute("timeout").toDouble(&ok);

        if (ok)
            property.setTimeout(timeoutValue);
    }

    // update specific values
    switch (rootTagType->first)
    {
        case INDI_NUMBER:
        {
            AutoCNumeric locale;
            for_property<INDI::PropertyNumber>(root, property, [](const LilXmlElement & element, auto * item)
            {
                item->setValue(element.context());

                // Permit changing of min/max
                if (auto min = element.getAttribute("min")) item->setMin(min);
                if (auto max = element.getAttribute("max")) item->setMax(max);
            });
            locale.Restore();
            break;
        }

        case INDI_SWITCH:
        {
            for_property<INDI::PropertySwitch>(root, property, [](const LilXmlElement & element, auto * item)
            {
                item->setState(element.context());
            });
            break;
        }

        case INDI_TEXT:
        {
            for_property<INDI::PropertyText>(root, property, [](const LilXmlElement & element, auto * item)
            {
                item->setText(element.context());
            });
            break;
        }

        case INDI_LIGHT:
        {
            for_property<INDI::PropertyLight>(root, property, [](const LilXmlElement & element, auto * item)
            {
                item->setState(element.context());
            });
            break;
        }

        case INDI_BLOB:
        {
            if (d->setBLOB(PropertyBlob(property), root, errmsg) < 0)
                return -1;
            break;
        }

        case INDI_UNKNOWN: // it will never happen
            return -1;
    }

    d->mediateUpdateProperty(property);

    return 0;
}

#ifdef ENABLE_INDI_SHARED_MEMORY
static bool sSharedToBlob(const INDI::LilXmlElement &element, INDI::WidgetViewBlob &widget)
{
    auto attachementId = element.getAttribute("attached-data-id");

    if (!attachementId.isValid())
    {
        return false;
    }

    auto size = element.getAttribute("size");
    // Client mark blob that can be attached directly

    // FIXME: Where is the blob data buffer freed at the end ?
    // FIXME: blobSize is not buffer size here. Must pass it all the way through
    // (while compressing shared buffer is useless)
    if (auto directAttachment = element.getAttribute("attachment-direct"))
    {
        if (widget.getBlob())
        {
            IDSharedBlobFree(widget.getBlob());
            widget.setBlobLen(0);
        }
        widget.setBlob(attachBlobByUid(attachementId.toString(), size));
    }
    else
    {
        // For compatibility, copy to a modifiable memory area
        widget.setBlob(realloc(widget.getBlob(), size));
        void *tmp = attachBlobByUid(attachementId.toString(), size);
        memcpy(widget.getBlob(), tmp, size);
        IDSharedBlobFree(tmp);
    }
    widget.setBlobLen(size);

    return true;
}
#endif

/* Set BLOB vector. Process incoming data stream
 * Return 0 if okay, -1 if error
*/
int BaseDevicePrivate::setBLOB(INDI::PropertyBlob property, const LilXmlElement &root, char *errmsg)
{
    for (const auto &element : root.getElementsByTagName("oneBLOB"))
    {
        auto name   = element.getAttribute("name");
        auto format = element.getAttribute("format");
        auto size   = element.getAttribute("size");

        auto widget = property.findWidgetByName(name);

        if (!name || !format || !size)
        {
            snprintf(errmsg, MAXRBUF, "INDI: %s.%s.%s No valid members.",
                     property.getDeviceName(), property.getName(), name.toCString()
                    );
            return -1;
        }

        if (size.toInt() == 0)
        {
            continue;
        }

        widget->setSize(size);
#ifdef ENABLE_INDI_SHARED_MEMORY
        if (sSharedToBlob(element, *widget) == false)
#endif
        {
            size_t base64_encoded_size = element.context().size();
            size_t base64_decoded_size = 3 * base64_encoded_size / 4;
            widget->setBlob(realloc(widget->getBlob(), base64_decoded_size));
            int blobLen = from64tobits_fast(static_cast<char *>(widget->getBlob()), element.context(), base64_encoded_size);
            widget->setBlobLen(blobLen);
        }

        if (format.endsWith(".z"))
        {
            widget->setFormat(format.toString().substr(0, format.lastIndexOf(".z")));

            uLongf dataSize = widget->getSize() * sizeof(uint8_t);
            Bytef *dataBuffer = static_cast<Bytef *>(malloc(dataSize));

            if (dataBuffer == nullptr)
            {
                strncpy(errmsg, "Unable to allocate memory for data buffer", MAXRBUF);
                return -1;
            }
            int r = uncompress(dataBuffer, &dataSize, static_cast<unsigned char *>(widget->getBlob()),
                               static_cast<uLong>(widget->getBlobLen()));
            if (r != Z_OK)
            {
                snprintf(errmsg, MAXRBUF, "INDI: %s.%s.%s compression error: %d",
                         property.getDeviceName(), property.getName(), widget->getName(), r);
                free(dataBuffer);
                return -1;
            }
            widget->setSize(dataSize);
#ifdef ENABLE_INDI_SHARED_MEMORY
            IDSharedBlobFree(widget->getBlob());
#else
            free(widget->getBlob());
#endif
            widget->setBlob(dataBuffer);

        }
        else
        {
            widget->setFormat(format);
        }

        property.emitUpdate();
    }

    return 0;
}

void BaseDevice::setDeviceName(const char *dev)
{
    D_PTR(BaseDevice);
    d->deviceName = dev;
}

const char *BaseDevice::getDeviceName() const
{
    D_PTR(const BaseDevice);
    return d->deviceName.data();
}

bool BaseDevice::isDeviceNameMatch(const char *otherName) const
{
    D_PTR(const BaseDevice);
    return d->deviceName == otherName;
}

bool BaseDevice::isDeviceNameMatch(const std::string &otherName) const
{
    D_PTR(const BaseDevice);
    return d->deviceName == otherName;
}

/* add message to queue
 * N.B. don't put carriage control in msg, we take care of that.
 */
void BaseDevice::checkMessage(XMLEle *root)
{
    XMLAtt *ap;
    ap = findXMLAtt(root, "message");

    if (ap)
        doMessage(root);
}

/* Store msg in queue */
void BaseDevice::doMessage(XMLEle *msg)
{
    XMLAtt *message;
    XMLAtt *time_stamp;

    char msgBuffer[MAXRBUF];

    /* prefix our timestamp if not with msg */
    time_stamp = findXMLAtt(msg, "timestamp");

    /* finally! the msg */
    message = findXMLAtt(msg, "message");
    if (!message)
        return;

    if (time_stamp)
        snprintf(msgBuffer, MAXRBUF, "%s: %s ", valuXMLAtt(time_stamp), valuXMLAtt(message));
    else
        snprintf(msgBuffer, MAXRBUF, "%s: %s ", timestamp(), valuXMLAtt(message));

    std::string finalMsg = msgBuffer;

    // Prepend to the log
    addMessage(finalMsg);
}

void BaseDevice::addMessage(const std::string &msg)
{
    D_PTR(BaseDevice);
    std::unique_lock<std::mutex> guard(d->m_Lock);
    d->messageLog.push_back(msg);
    guard.unlock();

    d->mediateNewMessage(*this, int(d->messageLog.size() - 1));
}

const std::string &BaseDevice::messageQueue(size_t index) const
{
    D_PTR(const BaseDevice);
    std::lock_guard<std::mutex> lock(d->m_Lock);
    assert(index < d->messageLog.size());
    return d->messageLog.at(index);
}

const std::string &BaseDevice::lastMessage() const
{
    D_PTR(const BaseDevice);
    std::lock_guard<std::mutex> lock(d->m_Lock);
    assert(d->messageLog.size() != 0);
    return d->messageLog.back();
}

bool BaseDevice::isValid() const
{
    D_PTR(const BaseDevice);
    return d->valid;
}

void BaseDevice::watchProperty(const char *name, const std::function<void(INDI::Property)> &callback)
{
    D_PTR(BaseDevice);
    d->watchPropertyMap[name] = callback;
}

void BaseDevice::registerProperty(const INDI::Property &property)
{
    D_PTR(BaseDevice);

    if (property.getType() == INDI_UNKNOWN)
        return;

    auto pContainer = getProperty(property.getName(), property.getType());

    if (pContainer.isValid())
        pContainer.setRegistered(true);
    else
        d->addProperty(property);
}

// #PS: TODO remove
void BaseDevice::registerProperty(const INDI::Property &property, INDI_PROPERTY_TYPE)
{
    registerProperty(property);
}

const char *BaseDevice::getDriverName() const
{
    auto driverName = getText("DRIVER_INFO").findWidgetByName("DRIVER_NAME");

    return driverName ? driverName->getText() : nullptr;
}

const char *BaseDevice::getDriverExec() const
{
    auto driverExec = getText("DRIVER_INFO").findWidgetByName("DRIVER_EXEC");
    return driverExec ? driverExec->getText() : nullptr;
}

const char *BaseDevice::getDriverVersion() const
{
    auto driverVersion = getText("DRIVER_INFO").findWidgetByName("DRIVER_VERSION");
    return driverVersion ? driverVersion->getText() : nullptr;
}

uint16_t BaseDevice::getDriverInterface() const
{
    auto driverInterface = getText("DRIVER_INFO").findWidgetByName("DRIVER_INTERFACE");
    return driverInterface ? atoi(driverInterface->getText()) : 0;
}

void BaseDevice::setMediator(INDI::BaseMediator *mediator)
{
    D_PTR(BaseDevice);
    d->mediator = mediator;
}

INDI::BaseMediator *BaseDevice::getMediator() const
{
    D_PTR(const BaseDevice);
    return d->mediator;
}

BaseDevice *BaseDevice::operator->()
{
    D_PTR(BaseDevice);
    return &d->self;
}

BaseDevice::operator BaseDevice*()
{
    D_PTR(BaseDevice);
    return isValid() ? &d->self : nullptr;
}

}

#if defined(_MSC_VER)
#undef snprintf
#pragma warning(pop)
#endif
