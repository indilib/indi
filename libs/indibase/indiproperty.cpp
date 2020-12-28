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

#include "indiproperty.h"

#include <cstdlib>
#include <cstring>

//#include "indidriver.h"
void IUSaveConfigNumber(FILE *, const INumberVectorProperty *) __attribute__((weak));
static void check_IUSaveConfigNumber(FILE *f, const INumberVectorProperty *v)
{
    if (IUSaveConfigNumber)
        IUSaveConfigNumber(f, v);
    else
        fprintf(stderr, "Cannot save INumberVectorProperty. Please add indidriver to linker.\n");
}

void IUSaveConfigText(FILE *, const ITextVectorProperty *) __attribute__((weak));
static void check_IUSaveConfigText(FILE *f, const ITextVectorProperty *v)
{
    if (IUSaveConfigText)
        IUSaveConfigText(f, v);
    else
        fprintf(stderr, "Cannot save ITextVectorProperty. Please add indidriver to linker.\n");
}

void IUSaveConfigSwitch(FILE *, const ISwitchVectorProperty *) __attribute__((weak));
static void check_IUSaveConfigSwitch(FILE *f, const ISwitchVectorProperty *v)
{
    if (IUSaveConfigSwitch)
        IUSaveConfigSwitch(f, v);
    else
        fprintf(stderr, "Cannot save ISwitchVectorProperty. Please add indidriver to linker.\n");
}

void IUSaveConfigBLOB(FILE *, const IBLOBVectorProperty *) __attribute__((weak));
static void check_IUSaveConfigBLOB(FILE *f, const IBLOBVectorProperty *v)
{
    if (IUSaveConfigBLOB)
        IUSaveConfigBLOB(f, v);
    else
        fprintf(stderr, "Cannot save IBLOBVectorProperty. Please add indidriver to linker.\n");
}


namespace INDI
{

Property::Property()
{ }

Property::Property(void *property, INDI_PROPERTY_TYPE type)
    : pPtr(property)
    , pType(property != nullptr ? type : INDI_UNKNOWN)
    , pRegistered(property != nullptr)
{ }

Property::Property(INumberVectorProperty *property)
    : pPtr(property)
    , pType(property != nullptr ? INDI_NUMBER : INDI_UNKNOWN)
    , pRegistered(property != nullptr)
{ }

Property::Property(ITextVectorProperty   *property)
    : pPtr(property)
    , pType(property != nullptr ? INDI_TEXT : INDI_UNKNOWN)
    , pRegistered(property != nullptr)
{ }

Property::Property(ISwitchVectorProperty *property)
    : pPtr(property)
    , pType(property != nullptr ? INDI_SWITCH : INDI_UNKNOWN)
    , pRegistered(property != nullptr)
{ }

Property::Property(ILightVectorProperty  *property)
    : pPtr(property)
    , pType(property != nullptr ? INDI_LIGHT : INDI_UNKNOWN)
    , pRegistered(property != nullptr)
{ }

Property::Property(IBLOBVectorProperty   *property)
    : pPtr(property)
    , pType(property != nullptr ? INDI_BLOB : INDI_UNKNOWN)
    , pRegistered(property != nullptr)
{ }

Property::~Property()
{
    // Only delete properties if they were created dynamically via the buildSkeleton
    // function. Other drivers are responsible for their own memory allocation.
    if (pDynamic && pPtr != nullptr)
    {
        switch (pType)
        {
            case INDI_NUMBER:
            {
                INumberVectorProperty *p = static_cast<INumberVectorProperty *>(pPtr);
                free(p->np);
                delete p;
                break;
            }

            case INDI_TEXT:
            {
                ITextVectorProperty *p = static_cast<ITextVectorProperty *>(pPtr);
                for (int i = 0; i < p->ntp; ++i)
                {
                    free(p->tp[i].text);
                }
                free(p->tp);
                delete p;
                break;
            }

            case INDI_SWITCH:
            {
                ISwitchVectorProperty *p = static_cast<ISwitchVectorProperty *>(pPtr);
                free(p->sp);
                delete p;
                break;
            }

            case INDI_LIGHT:
            {
                ILightVectorProperty *p = static_cast<ILightVectorProperty *>(pPtr);
                free(p->lp);
                delete p;
                break;
            }

            case INDI_BLOB:
            {
                IBLOBVectorProperty *p = static_cast<IBLOBVectorProperty *>(pPtr);
                for (int i = 0; i < p->nbp; ++i)
                {
                    free(p->bp[i].blob);
                }
                free(p->bp);
                delete p;
                break;
            }

            case INDI_UNKNOWN:
                break;
        }
    }
}

void Property::setProperty(void *p)
{
    pType       = p ? pType : INDI_UNKNOWN;
    pRegistered = p != nullptr;
    pPtr        = p;
}

void Property::setType(INDI_PROPERTY_TYPE t)
{
    pType = t;
}

void Property::setRegistered(bool r)
{
    pRegistered = r;
}

void Property::setDynamic(bool d)
{
    pDynamic = d;
}

void Property::setBaseDevice(BaseDevice *idp)
{
    dp = idp;
}

void *Property::getProperty() const
{
    return pPtr;
}

INDI_PROPERTY_TYPE Property::getType() const
{
    return pType;
}

bool Property::getRegistered() const
{
    return pRegistered;
}

bool Property::isDynamic() const
{
    return pDynamic;
}

BaseDevice *Property::getBaseDevice() const
{
    return dp;
}

void Property::save(FILE *fp)
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: check_IUSaveConfigNumber (fp, getNumber()); break;
        case INDI_TEXT:   check_IUSaveConfigText   (fp, getText());   break;
        case INDI_SWITCH: check_IUSaveConfigSwitch (fp, getSwitch()); break;
        //case INDI_LIGHT:  check_IUSaveConfigLight  (fp, getLight());  break;
        case INDI_BLOB:   check_IUSaveConfigBLOB   (fp, getBLOB());   break;
        default:;;
    }
}

void Property::setName(const char *name)
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: strncpy(static_cast<INumberVectorProperty *>(pPtr)->name, name, MAXINDINAME); break;
        case INDI_TEXT:   strncpy(static_cast<ITextVectorProperty   *>(pPtr)->name, name, MAXINDINAME); break;
        case INDI_SWITCH: strncpy(static_cast<ISwitchVectorProperty *>(pPtr)->name, name, MAXINDINAME); break;
        case INDI_LIGHT:  strncpy(static_cast<ILightVectorProperty  *>(pPtr)->name, name, MAXINDINAME); break;
        case INDI_BLOB:   strncpy(static_cast<IBLOBVectorProperty   *>(pPtr)->name, name, MAXINDINAME); break;
        default:;
    }
}

void Property::setLabel(const char *label)
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: strncpy(static_cast<INumberVectorProperty *>(pPtr)->label, label, MAXINDILABEL); break;
        case INDI_TEXT:   strncpy(static_cast<ITextVectorProperty   *>(pPtr)->label, label, MAXINDILABEL); break;
        case INDI_SWITCH: strncpy(static_cast<ISwitchVectorProperty *>(pPtr)->label, label, MAXINDILABEL); break;
        case INDI_LIGHT:  strncpy(static_cast<ILightVectorProperty  *>(pPtr)->label, label, MAXINDILABEL); break;
        case INDI_BLOB:   strncpy(static_cast<IBLOBVectorProperty   *>(pPtr)->label, label, MAXINDILABEL); break;
        default:;
    }
}

void Property::setGroupName(const char *group)
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: strncpy(static_cast<INumberVectorProperty *>(pPtr)->group, group, MAXINDIGROUP); break;
        case INDI_TEXT:   strncpy(static_cast<ITextVectorProperty   *>(pPtr)->group, group, MAXINDIGROUP); break;
        case INDI_SWITCH: strncpy(static_cast<ISwitchVectorProperty *>(pPtr)->group, group, MAXINDIGROUP); break;
        case INDI_LIGHT:  strncpy(static_cast<ILightVectorProperty  *>(pPtr)->group, group, MAXINDIGROUP); break;
        case INDI_BLOB:   strncpy(static_cast<IBLOBVectorProperty   *>(pPtr)->group, group, MAXINDIGROUP); break;
        default:;
    }
}

void Property::setDeviceName(const char *device)
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: strncpy(static_cast<INumberVectorProperty *>(pPtr)->device, device, MAXINDIDEVICE); break;
        case INDI_TEXT:   strncpy(static_cast<ITextVectorProperty   *>(pPtr)->device, device, MAXINDIDEVICE); break;
        case INDI_SWITCH: strncpy(static_cast<ISwitchVectorProperty *>(pPtr)->device, device, MAXINDIDEVICE); break;
        case INDI_LIGHT:  strncpy(static_cast<ILightVectorProperty  *>(pPtr)->device, device, MAXINDIDEVICE); break;
        case INDI_BLOB:   strncpy(static_cast<IBLOBVectorProperty   *>(pPtr)->device, device, MAXINDIDEVICE); break;
        default:;
    }
}

void Property::setTimestamp(const char *timestamp)
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: strncpy(static_cast<INumberVectorProperty *>(pPtr)->timestamp, timestamp, MAXINDITSTAMP); break;
        case INDI_TEXT:   strncpy(static_cast<ITextVectorProperty   *>(pPtr)->timestamp, timestamp, MAXINDITSTAMP); break;
        case INDI_SWITCH: strncpy(static_cast<ISwitchVectorProperty *>(pPtr)->timestamp, timestamp, MAXINDITSTAMP); break;
        case INDI_LIGHT:  strncpy(static_cast<ILightVectorProperty  *>(pPtr)->timestamp, timestamp, MAXINDITSTAMP); break;
        case INDI_BLOB:   strncpy(static_cast<IBLOBVectorProperty   *>(pPtr)->timestamp, timestamp, MAXINDITSTAMP); break;
        default:;
    }
}

void Property::setState(IPState state)
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: static_cast<INumberVectorProperty *>(pPtr)->s = state; break;
        case INDI_TEXT:   static_cast<ITextVectorProperty   *>(pPtr)->s = state; break;
        case INDI_SWITCH: static_cast<ISwitchVectorProperty *>(pPtr)->s = state; break;
        case INDI_LIGHT:  static_cast<ILightVectorProperty  *>(pPtr)->s = state; break;
        case INDI_BLOB:   static_cast<IBLOBVectorProperty   *>(pPtr)->s = state; break;
        default:;
    }
}

void Property::setPermission(IPerm permission)
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: static_cast<INumberVectorProperty *>(pPtr)->p = permission; break;
        case INDI_TEXT:   static_cast<ITextVectorProperty   *>(pPtr)->p = permission; break;
        case INDI_SWITCH: static_cast<ISwitchVectorProperty *>(pPtr)->p = permission; break;
        //case INDI_LIGHT:  static_cast<ILightVectorProperty  *>(pPtr)->p = permission; break;
        case INDI_BLOB:   static_cast<IBLOBVectorProperty   *>(pPtr)->p = permission; break;
        default:;
    }
}

void Property::setTimeout(double timeout)
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: static_cast<INumberVectorProperty *>(pPtr)->timeout = timeout; break;
        case INDI_TEXT:   static_cast<ITextVectorProperty   *>(pPtr)->timeout = timeout; break;
        case INDI_SWITCH: static_cast<ISwitchVectorProperty *>(pPtr)->timeout = timeout; break;
        //case INDI_LIGHT:  static_cast<ILightVectorProperty  *>(pPtr)->timeout = timeout; break;
        case INDI_BLOB:   static_cast<IBLOBVectorProperty   *>(pPtr)->timeout = timeout; break;
        default:;
    }
}

const char *Property::getName() const
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: return (static_cast<INumberVectorProperty *>(pPtr)->name);
        case INDI_TEXT:   return (static_cast<ITextVectorProperty   *>(pPtr)->name);
        case INDI_SWITCH: return (static_cast<ISwitchVectorProperty *>(pPtr)->name);
        case INDI_LIGHT:  return (static_cast<ILightVectorProperty  *>(pPtr)->name);
        case INDI_BLOB:   return (static_cast<IBLOBVectorProperty   *>(pPtr)->name);
        default: return nullptr;
    }
}

const char *Property::getLabel() const
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(pPtr)->label;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(pPtr)->label;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(pPtr)->label;
        case INDI_LIGHT:  return static_cast <ILightVectorProperty  *>(pPtr)->label;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(pPtr)->label;
        default: return nullptr;
    }
}


const char *Property::getGroupName() const
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(pPtr)->group;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(pPtr)->group;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(pPtr)->group;
        case INDI_LIGHT:  return static_cast <ILightVectorProperty  *>(pPtr)->group;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(pPtr)->group;
        default:          return nullptr;
    }
}

const char *Property::getDeviceName() const
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(pPtr)->device;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(pPtr)->device;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(pPtr)->device;
        case INDI_LIGHT:  return static_cast <ILightVectorProperty  *>(pPtr)->device;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(pPtr)->device;
        default:          return nullptr;
    }
}


const char *Property::getTimestamp() const
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(pPtr)->timestamp;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(pPtr)->timestamp;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(pPtr)->timestamp;
        case INDI_LIGHT:  return static_cast <ILightVectorProperty  *>(pPtr)->timestamp;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(pPtr)->timestamp;
        default:          return nullptr;
    }
}

IPState Property::getState() const
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(pPtr)->s;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(pPtr)->s;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(pPtr)->s;
        case INDI_LIGHT:  return static_cast <ILightVectorProperty  *>(pPtr)->s;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(pPtr)->s;
        default:          return IPS_ALERT;
    }
}

IPerm Property::getPermission() const
{
    switch (pPtr != nullptr ? pType : INDI_UNKNOWN)
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(pPtr)->p;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(pPtr)->p;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(pPtr)->p;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(pPtr)->p;
        default:          return IP_RO;
    }
}

INumberVectorProperty *Property::getNumber() const
{
    if (pType == INDI_NUMBER)
        return static_cast <INumberVectorProperty * > (pPtr);

    return nullptr;
}

ITextVectorProperty *Property::getText() const
{
    if (pType == INDI_TEXT)
        return static_cast <ITextVectorProperty * > (pPtr);

    return nullptr;
}

ILightVectorProperty *Property::getLight() const
{
    if (pType == INDI_LIGHT)
        return static_cast <ILightVectorProperty * > (pPtr);

    return nullptr;
}

ISwitchVectorProperty *Property::getSwitch() const
{
    if (pType == INDI_SWITCH)
        return static_cast <ISwitchVectorProperty * > (pPtr);

    return nullptr;
}

IBLOBVectorProperty *Property::getBLOB() const
{
    if (pType == INDI_BLOB)
        return static_cast <IBLOBVectorProperty * > (pPtr);

    return nullptr;
}

}