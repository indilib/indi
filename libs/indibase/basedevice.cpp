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

#include <cerrno>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <zlib.h>
#include <sys/stat.h>
#include <thread>
#include <chrono>

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
    lp = newLilXML();

    if (getenv("INDIDEV") != nullptr)
    {
        deviceName = getenv("INDIDEV");
        putenv(indidev);
    }
}

BaseDevicePrivate::~BaseDevicePrivate()
{
    delLilXML(lp);
    while (!pAll.empty())
    {
        delete pAll.back();
        pAll.pop_back();
    }
}

BaseDevice::BaseDevice()
    : d_ptr(new BaseDevicePrivate)
{ }

BaseDevice::~BaseDevice()
{ }

BaseDevice::BaseDevice(BaseDevicePrivate &dd)
    : d_ptr(&dd)
{ }

INDI::PropertyView<INumber> *BaseDevice::getNumber(const char *name) const
{
    return static_cast<PropertyView<INumber> *>(getRawProperty(name, INDI_NUMBER));
}

INDI::PropertyView<IText> *BaseDevice::getText(const char *name) const
{
    return static_cast<PropertyView<IText> *>(getRawProperty(name, INDI_TEXT));
}

INDI::PropertyView<ISwitch> *BaseDevice::getSwitch(const char *name) const
{
    return static_cast<PropertyView<ISwitch> *>(getRawProperty(name, INDI_SWITCH));
}

INDI::PropertyView<ILight> *BaseDevice::getLight(const char *name) const
{
    return static_cast<PropertyView<ILight> *>(getRawProperty(name, INDI_LIGHT));
}

INDI::PropertyView<IBLOB> *BaseDevice::getBLOB(const char *name) const
{
    return static_cast<PropertyView<IBLOB> *>(getRawProperty(name, INDI_BLOB));
}

IPState BaseDevice::getPropertyState(const char *name) const
{
    for (const auto &oneProp : *getProperties())
        if (!strcmp(name, oneProp->getName()))
            return oneProp->getState();

    return IPS_IDLE;
}

IPerm BaseDevice::getPropertyPermission(const char *name) const
{
    for (const auto &oneProp : *getProperties())
        if (!strcmp(name, oneProp->getName()))
            return oneProp->getPermission();

    return IP_RO;
}

void *BaseDevice::getRawProperty(const char *name, INDI_PROPERTY_TYPE type) const
{
    INDI::Property *prop = getProperty(name, type);
    return prop != nullptr ? prop->getProperty() : nullptr;
}

INDI::Property *BaseDevice::getProperty(const char *name, INDI_PROPERTY_TYPE type) const
{
    D_PTR(const BaseDevice);
    std::lock_guard<std::mutex> lock(d->m_Lock);

    for (const auto &oneProp : *getProperties())
    {
        if (type != oneProp->getType() && type != INDI_UNKNOWN)
            continue;

        if (!oneProp->getRegistered())
            continue;

        if (!strcmp(name, oneProp->getName()))
            return oneProp;
    }

    return nullptr;
}

int BaseDevice::removeProperty(const char *name, char *errmsg)
{
    D_PTR(BaseDevice);
    std::lock_guard<std::mutex> lock(d->m_Lock);

    for (auto orderi = d->pAll.begin(); orderi != d->pAll.end(); ++orderi)
    {
        const auto &oneProp = *orderi;
        if (!strcmp(name, oneProp->getName()))
        {
            // JM 2021-04-28: delete later. We perform the actual delete after 1000ms to give clients a chance to remove the object.
            // This is necessary when rapid define-delete-define sequences are made.
            // This HACK is not ideal. We need to start using std::shared_ptr for this purpose soon, but this will be a major change to the
            // interface. Perhaps for INDI 2.0
            std::thread t([oneProp]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                delete oneProp;
            });
            t.detach();

            orderi = d->pAll.erase(orderi);
            return 0;
        }
    }

    snprintf(errmsg, MAXRBUF, "Error: Property %s not found in device %s.", name, getDeviceName());
    return INDI_PROPERTY_INVALID;
}

bool BaseDevice::buildSkeleton(const char *filename)
{
    D_PTR(BaseDevice);
    char errmsg[MAXRBUF];
    FILE *fp     = nullptr;
    XMLEle *root = nullptr, *fproot = nullptr;

    char pathname[MAXRBUF];
    struct stat st;
    const char *indiskel = getenv("INDISKEL");
    if (indiskel)
    {
        strncpy(pathname, indiskel, MAXRBUF - 1);
        pathname[MAXRBUF - 1] = 0;
        IDLog("Using INDISKEL %s\n", pathname);
    }
    else
    {
        if (stat(filename, &st) == 0)
        {
            strncpy(pathname, filename, MAXRBUF - 1);
            pathname[MAXRBUF - 1] = 0;
            IDLog("Using %s\n", pathname);
        }
        else
        {
            const char *slash = strrchr(filename, '/');
            if (slash)
                filename = slash + 1;
            const char *indiprefix = getenv("INDIPREFIX");
            if (indiprefix)
            {
#if defined(OSX_EMBEDED_MODE)
                snprintf(pathname, MAXRBUF - 1, "%s/Contents/Resources/%s", indiprefix, filename);
#elif defined(__APPLE__)
                snprintf(pathname, MAXRBUF - 1, "%s/Contents/Resources/DriverSupport/%s", indiprefix, filename);
#else
                snprintf(pathname, MAXRBUF - 1, "%s/share/indi/%s", indiprefix, filename);
#endif
            }
            else
            {
                snprintf(pathname, MAXRBUF - 1, "%s/%s", DATA_INSTALL_DIR, filename);
            }
            pathname[MAXRBUF - 1] = 0;
            IDLog("Using prefix %s\n", pathname);
        }
    }

    fp = fopen(pathname, "r");

    if (fp == nullptr)
    {
        IDLog("Unable to build skeleton. Error loading file %s: %s\n", pathname, strerror(errno));
        return false;
    }

    fproot = readXMLFile(fp, d->lp, errmsg);
    fclose(fp);

    if (fproot == nullptr)
    {
        IDLog("Unable to parse skeleton XML: %s", errmsg);
        return false;
    }

    //prXMLEle(stderr, fproot, 0);

    for (root = nextXMLEle(fproot, 1); root != nullptr; root = nextXMLEle(fproot, 0))
        buildProp(root, errmsg);

    delXMLEle(fproot);
    return true;
    /**************************************************************************/
}

int BaseDevice::buildProp(XMLEle *root, char *errmsg)
{
    D_PTR(BaseDevice);
    IPerm perm    = IP_RO;
    IPState state = IPS_IDLE;
    XMLEle *ep    = nullptr;
    char *rtag, *rname, *rdev;

    INDI::Property *indiProp = nullptr;
    int n = 0;

    rtag = tagXMLEle(root);

    /* pull out device and name */
    if (crackDN(root, &rdev, &rname, errmsg) < 0)
        return -1;

    if (d->deviceName.empty())
        d->deviceName = rdev;

    //if (getProperty(rname, type) != nullptr)
    if (getProperty(rname) != nullptr)
        return INDI_PROPERTY_DUPLICATED;

    if (strcmp(rtag, "defLightVector") && crackIPerm(findXMLAttValu(root, "perm"), &perm) < 0)
    {
        IDLog("Error extracting %s permission (%s)\n", rname, findXMLAttValu(root, "perm"));
        return -1;
    }

    if (crackIPState(findXMLAttValu(root, "state"), &state) < 0)
    {
        IDLog("Error extracting %s state (%s)\n", rname, findXMLAttValu(root, "state"));
        return -1;
    }

    if (!strcmp(rtag, "defNumberVector"))
    {
        AutoCNumeric locale;

        INumberVectorProperty *nvp = new INumberVectorProperty;

        INumber *np = nullptr;

        /* pull out each name/value pair */
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            if (strcmp(tagXMLEle(ep), "defNumber"))
                continue;

            np = static_cast<INumber *>(realloc(np, (n + 1) * sizeof(INumber)));
            INumber *it = &np[n];
            memset(it, 0, sizeof(*it));
            it->nvp = nvp;

            strncpy(it->name, findXMLAttValu(ep, "name"), MAXINDINAME);
            if (*it->name == '\0')
                continue;

            if (f_scansexa(pcdataXMLEle(ep), &(it->value)) < 0)
            {
                IDLog("%s: Bad format %s\n", rname, pcdataXMLEle(ep));
                continue;
            }

            strncpy(it->label,  findXMLAttValu(ep, "label" ), MAXINDILABEL );
            strncpy(it->format, findXMLAttValu(ep, "format"), MAXINDIFORMAT);

            it->min  = atof(findXMLAttValu(ep, "min"));
            it->max  = atof(findXMLAttValu(ep, "max"));
            it->step = atof(findXMLAttValu(ep, "step"));
            ++n;
        }

        if (n > 0)
        {
            nvp->nnp = n;
            nvp->np  = np;

            indiProp = new INDI::Property(nvp);
        }
        else
        {
            IDLog("%s: newNumberVector with no valid members\n", rname);
            delete (nvp);
            free (np);
        }
    }
    else if (!strcmp(rtag, "defSwitchVector"))
    {
        ISwitchVectorProperty *svp = new ISwitchVectorProperty;

        ISwitch *sp = nullptr;

        if (crackISRule(findXMLAttValu(root, "rule"), (&svp->r)) < 0)
            svp->r = ISR_1OFMANY;

        /* pull out each name/value pair */
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            if (strcmp(tagXMLEle(ep), "defSwitch"))
                continue;

            sp = static_cast<ISwitch *>(realloc(sp, (n + 1) * sizeof(ISwitch)));
            ISwitch *it = &sp[n];
            memset(it, 0, sizeof(*it));
            it->svp = svp;

            strncpy(it->name, findXMLAttValu(ep, "name"), MAXINDINAME);
            if (*it->name == '\0')
                continue;

            crackISState(pcdataXMLEle(ep), &(it->s));

            strncpy(it->label, findXMLAttValu(ep, "label"), MAXINDILABEL);
            ++n;
        }

        if (n > 0)
        {
            svp->nsp = n;
            svp->sp  = sp;
            indiProp = new INDI::Property(svp);
        }
        else
        {
            IDLog("%s: newSwitchVector with no valid members\n", rname);
            delete (svp);
            free (sp);
        }
    }

    else if (!strcmp(rtag, "defTextVector"))
    {
        ITextVectorProperty *tvp = new ITextVectorProperty;
        IText *tp                = nullptr;

        // pull out each name/value pair
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            if (strcmp(tagXMLEle(ep), "defText"))
                continue;

            tp = static_cast<IText *>(realloc(tp, (n + 1) * sizeof(IText)));
            memset(&tp[n], 0, sizeof(tp[n]));

            WidgetView<IText> *it = static_cast<WidgetView<IText>*>(&tp[n]);

            it->setParent(tvp);
            it->setName(findXMLAttValu(ep, "name"));

            if (it->getName()[0] == '\0')
                continue;

            it->setText(pcdataXMLEle(ep), pcdatalenXMLEle(ep));
            it->setLabel(findXMLAttValu(ep, "label"));
            ++n;
        }

        if (n > 0)
        {
            tvp->ntp = n;
            tvp->tp  = tp;

            indiProp = new INDI::Property(tvp);
        }
        else
        {
            IDLog("%s: newTextVector with no valid members\n", rname);
            delete (tvp);
            free (tp);
        }
    }
    else if (!strcmp(rtag, "defLightVector"))
    {
        ILightVectorProperty *lvp = new ILightVectorProperty;
        ILight *lp                = nullptr;

        /* pull out each name/value pair */
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            if (strcmp(tagXMLEle(ep), "defLight"))
                continue;

            lp = static_cast<ILight *>(realloc(lp, (n + 1) * sizeof(ILight)));
            ILight *it = &lp[n];
            memset(it, 0, sizeof(*it));
            it->lvp = lvp;

            strncpy(it->name, findXMLAttValu(ep, "name"), MAXINDINAME);
            if (*it->name == '\0')
                continue;

            crackIPState(pcdataXMLEle(ep), &(it->s));

            strncpy(it->label, findXMLAttValu(ep, "label"), MAXINDILABEL);
            ++n;
        }

        if (n > 0)
        {
            lvp->nlp = n;
            lvp->lp  = lp;

            indiProp  = new INDI::Property(lvp);
        }
        else
        {
            IDLog("%s: newLightVector with no valid members\n", rname);
            delete (lvp);
            free (lp);
        }
    }
    else if (!strcmp(rtag, "defBLOBVector"))
    {
        IBLOBVectorProperty *bvp = new IBLOBVectorProperty;
        IBLOB *bp                = nullptr;

        /* pull out each name/value pair */
        for (n = 0, ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            if (strcmp(tagXMLEle(ep), "defBLOB"))
                continue;

            bp = static_cast<IBLOB *>(realloc(bp, (n + 1) * sizeof(IBLOB)));
            IBLOB *it = &bp[n];
            memset(it, 0, sizeof(*it));
            it->bvp = bvp;

            strncpy(it->name, findXMLAttValu(ep, "name"), MAXINDINAME);
            if (*it->name == '\0')
                continue;

            strncpy(it->label,  findXMLAttValu(ep, "label" ), MAXINDILABEL );
            strncpy(it->format, findXMLAttValu(ep, "format"), MAXINDIBLOBFMT);
            ++n;
        }

        if (n > 0)
        {
            bvp->nbp = n;
            bvp->bp  = bp;

            indiProp  = new INDI::Property(bvp);
        }
        else
        {
            IDLog("%s: newBLOBVector with no valid members\n", rname);
            delete (bvp);
            free (bp);
        }
    }

    if (indiProp)
    {
        indiProp->setBaseDevice(this);
        indiProp->setDynamic(true);
        indiProp->setDeviceName(getDeviceName());
        indiProp->setName(rname);
        indiProp->setLabel(findXMLAttValu(root, "label"));
        indiProp->setGroupName(findXMLAttValu(root, "group"));
        indiProp->setPermission(perm);
        indiProp->setState(state);
        indiProp->setTimeout(atoi(findXMLAttValu(root, "timeout")));

        std::unique_lock<std::mutex> lock(d->m_Lock);
        d->pAll.push_back(indiProp);
        lock.unlock();

        //IDLog("Adding number property %s to list.\n", indiProp->getName());
        if (d->mediator)
            d->mediator->newProperty(indiProp);
    }

    return (0);
}

bool BaseDevice::isConnected() const
{
    ISwitchVectorProperty *svp = getSwitch(INDI::SP::CONNECTION);
    if (!svp)
        return false;

    ISwitch *sp = IUFindSwitch(svp, "CONNECT");

    return sp && sp->s == ISS_ON && svp->s == IPS_OK;
}

/*
 * return 0 if ok else -1 with reason in errmsg
 */
int BaseDevice::setValue(XMLEle *root, char *errmsg)
{
    D_PTR(BaseDevice);
    XMLEle *ep = nullptr;
    char *name = nullptr;
    double timeout = 0;
    IPState state = IPS_IDLE;
    bool stateSet = false, timeoutSet = false;

    char *rtag = tagXMLEle(root);

    XMLAtt *ap = findXMLAtt(root, "name");
    if (!ap)
    {
        snprintf(errmsg, MAXRBUF, "INDI: <%s> unable to find name attribute", tagXMLEle(root));
        return (-1);
    }

    name = valuXMLAtt(ap);

    /* set overall property state, if any */
    ap = findXMLAtt(root, "state");
    if (ap)
    {
        if (crackIPState(valuXMLAtt(ap), &state) != 0)
        {
            snprintf(errmsg, MAXRBUF, "INDI: <%s> bogus state %s for %s", tagXMLEle(root), valuXMLAtt(ap), name);
            return (-1);
        }

        stateSet = true;
    }

    /* allow changing the timeout */
    ap = findXMLAtt(root, "timeout");
    if (ap)
    {
        AutoCNumeric locale;
        timeout    = atof(valuXMLAtt(ap));
        timeoutSet = true;
    }

    checkMessage(root);

    if (!strcmp(rtag, "setNumberVector"))
    {
        INumberVectorProperty *nvp = getNumber(name);
        if (nvp == nullptr)
        {
            snprintf(errmsg, MAXRBUF, "INDI: Could not find property %s in %s", name, getDeviceName());
            return -1;
        }

        if (stateSet)
            nvp->s = state;

        if (timeoutSet)
            nvp->timeout = timeout;

        AutoCNumeric locale;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            INumber *np = IUFindNumber(nvp, findXMLAttValu(ep, "name"));
            if (!np)
                continue;

            np->value = atof(pcdataXMLEle(ep));

            // Permit changing of min/max
            if (findXMLAtt(ep, "min"))
                np->min = atof(findXMLAttValu(ep, "min"));
            if (findXMLAtt(ep, "max"))
                np->max = atof(findXMLAttValu(ep, "max"));
        }

        locale.Restore();

        if (d->mediator)
            d->mediator->newNumber(nvp);

        return 0;
    }
    else if (!strcmp(rtag, "setTextVector"))
    {
        ITextVectorProperty *tvp = getText(name);
        if (tvp == nullptr)
            return -1;

        if (stateSet)
            tvp->s = state;

        if (timeoutSet)
            tvp->timeout = timeout;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            IText *tp = IUFindText(tvp, findXMLAttValu(ep, "name"));
            if (!tp)
                continue;

            IUSaveText(tp, pcdataXMLEle(ep));
        }

        if (d->mediator)
            d->mediator->newText(tvp);

        return 0;
    }
    else if (!strcmp(rtag, "setSwitchVector"))
    {
        ISState swState;
        ISwitchVectorProperty *svp = getSwitch(name);
        if (svp == nullptr)
            return -1;

        if (stateSet)
            svp->s = state;

        if (timeoutSet)
            svp->timeout = timeout;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            ISwitch *sp = IUFindSwitch(svp, findXMLAttValu(ep, "name"));
            if (!sp)
                continue;

            if (crackISState(pcdataXMLEle(ep), &swState) == 0)
                sp->s = swState;
        }

        if (d->mediator)
            d->mediator->newSwitch(svp);

        return 0;
    }
    else if (!strcmp(rtag, "setLightVector"))
    {
        IPState lState;
        ILightVectorProperty *lvp = getLight(name);

        if (lvp == nullptr)
            return -1;

        if (stateSet)
            lvp->s = state;

        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            ILight *lp = IUFindLight(lvp, findXMLAttValu(ep, "name"));
            if (!lp)
                continue;

            if (crackIPState(pcdataXMLEle(ep), &lState) == 0)
                lp->s = lState;
        }

        if (d->mediator)
            d->mediator->newLight(lvp);

        return 0;
    }
    else if (!strcmp(rtag, "setBLOBVector"))
    {
        IBLOBVectorProperty *bvp = getBLOB(name);

        if (bvp == nullptr)
            return -1;

        if (stateSet)
            bvp->s = state;

        if (timeoutSet)
            bvp->timeout = timeout;

        return setBLOB(bvp, root, errmsg);
    }

    snprintf(errmsg, MAXRBUF, "INDI: <%s> Unable to process tag", tagXMLEle(root));
    return -1;
}

/* Set BLOB vector. Process incoming data stream
 * Return 0 if okay, -1 if error
*/
int BaseDevice::setBLOB(IBLOBVectorProperty *bvp, XMLEle *root, char *errmsg)
{
    D_PTR(BaseDevice);
    /* pull out each name/BLOB pair, decode */
    for (XMLEle *ep = nextXMLEle(root, 1); ep; ep = nextXMLEle(root, 0))
    {
        if (strcmp(tagXMLEle(ep), "oneBLOB") == 0)
        {
            XMLAtt *na = findXMLAtt(ep, "name");

            IBLOB *blobEL = IUFindBLOB(bvp, findXMLAttValu(ep, "name"));

            XMLAtt *fa = findXMLAtt(ep, "format");
            XMLAtt *sa = findXMLAtt(ep, "size");
            if (na && fa && sa)
            {
                int blobSize = atoi(valuXMLAtt(sa));

                /* Blob size = 0 when only state changes */
                if (blobSize == 0)
                {
                    if (d->mediator)
                        d->mediator->newBLOB(blobEL);
                    continue;
                }

                blobEL->size    = blobSize;
                uint32_t base64_encoded_size = pcdatalenXMLEle(ep);
                uint32_t base64_decoded_size = 3 * base64_encoded_size / 4;
                blobEL->blob    = static_cast<unsigned char *>(realloc(blobEL->blob, base64_decoded_size));
                blobEL->bloblen = from64tobits_fast(static_cast<char *>(blobEL->blob), pcdataXMLEle(ep), base64_encoded_size);

                strncpy(blobEL->format, valuXMLAtt(fa), MAXINDIFORMAT);

                if (strstr(blobEL->format, ".z"))
                {
                    blobEL->format[strlen(blobEL->format) - 2] = '\0';
                    uLongf dataSize = blobEL->size * sizeof(uint8_t);
                    uint8_t *dataBuffer = static_cast<uint8_t *>(malloc(dataSize));

                    if (dataBuffer == nullptr)
                    {
                        strncpy(errmsg, "Unable to allocate memory for data buffer", MAXRBUF);
                        return (-1);
                    }

                    int r = uncompress(dataBuffer, &dataSize, static_cast<unsigned char *>(blobEL->blob),
                                       static_cast<uLong>(blobEL->bloblen));
                    if (r != Z_OK)
                    {
                        snprintf(errmsg, MAXRBUF, "INDI: %s.%s.%s compression error: %d", blobEL->bvp->device,
                                 blobEL->bvp->name, blobEL->name, r);
                        free(dataBuffer);
                        return -1;
                    }
                    blobEL->size = dataSize;
                    free(blobEL->blob);
                    blobEL->blob = dataBuffer;
                }

                if (d->mediator)
                    d->mediator->newBLOB(blobEL);
            }
            else
            {
                snprintf(errmsg, MAXRBUF, "INDI: %s.%s.%s No valid members.", blobEL->bvp->device, blobEL->bvp->name,
                         blobEL->name);
                return -1;
            }
        }
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

    if (d->mediator)
        d->mediator->newMessage(this, d->messageLog.size() - 1);
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

void BaseDevice::registerProperty(void *p, INDI_PROPERTY_TYPE type)
{
    D_PTR(BaseDevice);
    if (p == nullptr || type == INDI_UNKNOWN)
        return;

    const char *name = INDI::Property(p, type).getName();

    INDI::Property *pContainer = getProperty(name, type);

    if (pContainer != nullptr)
        pContainer->setRegistered(true);
    else
    {
        std::lock_guard<std::mutex> lock(d->m_Lock);
        d->pAll.push_back(new INDI::Property(p, type));
    }
}

void BaseDevice::registerProperty(INDI::Property &property)
{
    D_PTR(BaseDevice);

    if (property.getType() == INDI_UNKNOWN)
        return;

    INDI::Property *pContainer = getProperty(property.getName(), property.getType());

    if (pContainer != nullptr)
        pContainer->setRegistered(true);
    else
    {
        std::lock_guard<std::mutex> lock(d->m_Lock);
        d->pAll.push_back(new INDI::Property(property));
    }
}

const char *BaseDevice::getDriverName() const
{
    ITextVectorProperty *driverInfo = getText("DRIVER_INFO");

    if (driverInfo == nullptr)
        return nullptr;

    IText *driverName = IUFindText(driverInfo, "DRIVER_NAME");
    if (driverName)
        return driverName->text;

    return nullptr;
}


void BaseDevice::registerProperty(ITextVectorProperty *property)
{
    registerProperty(property, INDI_TEXT);
}

void BaseDevice::registerProperty(INumberVectorProperty *property)
{
    registerProperty(property, INDI_NUMBER);
}

void BaseDevice::registerProperty(ISwitchVectorProperty *property)
{
    registerProperty(property, INDI_SWITCH);
}

void BaseDevice::registerProperty(ILightVectorProperty *property)
{
    registerProperty(property, INDI_LIGHT);
}

void BaseDevice::registerProperty(IBLOBVectorProperty *property)
{
    registerProperty(property, INDI_BLOB);
}

void BaseDevice::registerProperty(PropertyView<IText> *property)
{
    registerProperty(static_cast<ITextVectorProperty*>(property));
}

void BaseDevice::registerProperty(PropertyView<INumber> *property)
{
    registerProperty(static_cast<INumberVectorProperty*>(property));
}

void BaseDevice::registerProperty(PropertyView<ISwitch> *property)
{
    registerProperty(static_cast<ISwitchVectorProperty*>(property));
}

void BaseDevice::registerProperty(PropertyView<ILight> *property)
{
    BaseDevice::registerProperty(static_cast<ILightVectorProperty*>(property));
}

void BaseDevice::registerProperty(PropertyView<IBLOB> *property)
{
    BaseDevice::registerProperty(static_cast<IBLOBVectorProperty*>(property));
}

const char *BaseDevice::getDriverExec() const
{
    ITextVectorProperty *driverInfo = getText("DRIVER_INFO");

    if (driverInfo == nullptr)
        return nullptr;

    IText *driverExec = IUFindText(driverInfo, "DRIVER_EXEC");
    if (driverExec)
        return driverExec->text;

    return nullptr;
}

const char *BaseDevice::getDriverVersion() const
{
    ITextVectorProperty *driverInfo = getText("DRIVER_INFO");

    if (driverInfo == nullptr)
        return nullptr;

    IText *driverVersion = IUFindText(driverInfo, "DRIVER_VERSION");
    if (driverVersion)
        return driverVersion->text;

    return nullptr;
}

uint16_t BaseDevice::getDriverInterface()
{
    ITextVectorProperty *driverInfo = getText("DRIVER_INFO");

    if (driverInfo == nullptr)
        return 0;

    IText *driverInterface = IUFindText(driverInfo, "DRIVER_INTERFACE");
    if (driverInterface)
        return atoi(driverInterface->text);

    return 0;
}

const BaseDevice::Properties *BaseDevice::getProperties() const
{
    D_PTR(const BaseDevice);
    return &d->pAll;
}

BaseDevice::Properties *BaseDevice::getProperties()
{
    D_PTR(BaseDevice);
    return &d->pAll;
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

}

#if defined(_MSC_VER)
#undef snprintf
#pragma warning(pop)
#endif
