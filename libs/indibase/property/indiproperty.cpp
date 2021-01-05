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
#include "indiproperty_p.h"

#include <cstdlib>
#include <cstring>

namespace INDI
{

PropertyPrivate::PropertyPrivate(void *property, INDI_PROPERTY_TYPE type)
    : property(property)
    , type(property ? type : INDI_UNKNOWN)
    , registered(property != nullptr)
{ }

PropertyPrivate::PropertyPrivate(ITextVectorProperty *property)
    : property(property)
    , type(property ? INDI_TEXT : INDI_UNKNOWN)
    , registered(property != nullptr)
{ }

PropertyPrivate::PropertyPrivate(INumberVectorProperty *property)
    : property(property)
    , type(property ? INDI_NUMBER : INDI_UNKNOWN)
    , registered(property != nullptr)
{ }

PropertyPrivate::PropertyPrivate(ISwitchVectorProperty *property)
    : property(property)
    , type(property ? INDI_SWITCH : INDI_UNKNOWN)
    , registered(property != nullptr)
{ }

PropertyPrivate::PropertyPrivate(ILightVectorProperty *property)
    : property(property)
    , type(property ? INDI_LIGHT : INDI_UNKNOWN)
    , registered(property != nullptr)
{ }

PropertyPrivate::PropertyPrivate(IBLOBVectorProperty *property)
    : property(property)
    , type(property ? INDI_BLOB : INDI_UNKNOWN)
    , registered(property != nullptr)
{ }

PropertyPrivate::~PropertyPrivate()
{
    // Only delete properties if they were created dynamically via the buildSkeleton
    // function. Other drivers are responsible for their own memory allocation.
    if (property == nullptr || !dynamic)
        return;

    switch (type)
    {
        case INDI_NUMBER:
        {
            INumberVectorProperty *p = static_cast<INumberVectorProperty *>(property);
            free(p->np);
            delete p;
            break;
        }

        case INDI_TEXT:
        {
            ITextVectorProperty *p = static_cast<ITextVectorProperty *>(property);
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
            ISwitchVectorProperty *p = static_cast<ISwitchVectorProperty *>(property);
            free(p->sp);
            delete p;
            break;
        }

        case INDI_LIGHT:
        {
            ILightVectorProperty *p = static_cast<ILightVectorProperty *>(property);
            free(p->lp);
            delete p;
            break;
        }

        case INDI_BLOB:
        {
            IBLOBVectorProperty *p = static_cast<IBLOBVectorProperty *>(property);
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

Property::Property()
    : d_ptr(new PropertyPrivate(nullptr, INDI_UNKNOWN))
{ }

Property::Property(void *property, INDI_PROPERTY_TYPE type)
    : d_ptr(new PropertyPrivate(property, type))
{ }

Property::Property(INumberVectorProperty *property)
    : d_ptr(new PropertyPrivate(property))
{ }

Property::Property(ITextVectorProperty   *property)
    : d_ptr(new PropertyPrivate(property))
{ }

Property::Property(ISwitchVectorProperty *property)
    : d_ptr(new PropertyPrivate(property))
{ }

Property::Property(ILightVectorProperty  *property)
    : d_ptr(new PropertyPrivate(property))
{ }

Property::Property(IBLOBVectorProperty   *property)
    : d_ptr(new PropertyPrivate(property))
{ }

Property::~Property()
{ }

Property::Property(PropertyPrivate &dd)
    : d_ptr(&dd)
{ }

void Property::setProperty(void *p)
{
    D_PTR(Property);
    d->type       = p ? d->type : INDI_UNKNOWN;
    d->registered = p != nullptr;
    d->property   = p;
}

void Property::setType(INDI_PROPERTY_TYPE t)
{
    D_PTR(Property);
    d->type = t;
}

void Property::setRegistered(bool r)
{
    D_PTR(Property);
    d->registered = r;
}

void Property::setDynamic(bool dyn)
{
    D_PTR(Property);
    d->dynamic = dyn;
}

void Property::setBaseDevice(BaseDevice *idp)
{
    D_PTR(Property);
    d->baseDevice = idp;
}

void *Property::getProperty() const
{
    D_PTR(const Property);
    return d->property;
}

INDI_PROPERTY_TYPE Property::getType() const
{
    D_PTR(const Property);
    return d->property != nullptr ? d->type : INDI_UNKNOWN;
}

bool Property::getRegistered() const
{
    D_PTR(const Property);
    return d->registered;
}

bool Property::isDynamic() const
{
    D_PTR(const Property);
    return d->dynamic;
}

BaseDevice *Property::getBaseDevice() const
{
    D_PTR(const Property);
    return d->baseDevice;
}


void Property::setName(const char *name)
{
    D_PTR(Property);
    switch (getType())
    {
        case INDI_NUMBER: strncpy(static_cast<INumberVectorProperty *>(d->property)->name, name, MAXINDINAME); break;
        case INDI_TEXT:   strncpy(static_cast<ITextVectorProperty   *>(d->property)->name, name, MAXINDINAME); break;
        case INDI_SWITCH: strncpy(static_cast<ISwitchVectorProperty *>(d->property)->name, name, MAXINDINAME); break;
        case INDI_LIGHT:  strncpy(static_cast<ILightVectorProperty  *>(d->property)->name, name, MAXINDINAME); break;
        case INDI_BLOB:   strncpy(static_cast<IBLOBVectorProperty   *>(d->property)->name, name, MAXINDINAME); break;
        default:;
    }
}

void Property::setLabel(const char *label)
{
    D_PTR(Property);
    switch (getType())
    {
        case INDI_NUMBER: strncpy(static_cast<INumberVectorProperty *>(d->property)->label, label, MAXINDILABEL); break;
        case INDI_TEXT:   strncpy(static_cast<ITextVectorProperty   *>(d->property)->label, label, MAXINDILABEL); break;
        case INDI_SWITCH: strncpy(static_cast<ISwitchVectorProperty *>(d->property)->label, label, MAXINDILABEL); break;
        case INDI_LIGHT:  strncpy(static_cast<ILightVectorProperty  *>(d->property)->label, label, MAXINDILABEL); break;
        case INDI_BLOB:   strncpy(static_cast<IBLOBVectorProperty   *>(d->property)->label, label, MAXINDILABEL); break;
        default:;
    }
}

void Property::setGroupName(const char *group)
{
    D_PTR(Property);
    switch (getType())
    {
        case INDI_NUMBER: strncpy(static_cast<INumberVectorProperty *>(d->property)->group, group, MAXINDIGROUP); break;
        case INDI_TEXT:   strncpy(static_cast<ITextVectorProperty   *>(d->property)->group, group, MAXINDIGROUP); break;
        case INDI_SWITCH: strncpy(static_cast<ISwitchVectorProperty *>(d->property)->group, group, MAXINDIGROUP); break;
        case INDI_LIGHT:  strncpy(static_cast<ILightVectorProperty  *>(d->property)->group, group, MAXINDIGROUP); break;
        case INDI_BLOB:   strncpy(static_cast<IBLOBVectorProperty   *>(d->property)->group, group, MAXINDIGROUP); break;
        default:;
    }
}

void Property::setDeviceName(const char *device)
{
    D_PTR(Property);
    switch (getType())
    {
        case INDI_NUMBER: strncpy(static_cast<INumberVectorProperty *>(d->property)->device, device, MAXINDIDEVICE); break;
        case INDI_TEXT:   strncpy(static_cast<ITextVectorProperty   *>(d->property)->device, device, MAXINDIDEVICE); break;
        case INDI_SWITCH: strncpy(static_cast<ISwitchVectorProperty *>(d->property)->device, device, MAXINDIDEVICE); break;
        case INDI_LIGHT:  strncpy(static_cast<ILightVectorProperty  *>(d->property)->device, device, MAXINDIDEVICE); break;
        case INDI_BLOB:   strncpy(static_cast<IBLOBVectorProperty   *>(d->property)->device, device, MAXINDIDEVICE); break;
        default:;
    }
}

void Property::setTimestamp(const char *timestamp)
{
    D_PTR(Property);
    switch (getType())
    {
        case INDI_NUMBER: strncpy(static_cast<INumberVectorProperty *>(d->property)->timestamp, timestamp, MAXINDITSTAMP); break;
        case INDI_TEXT:   strncpy(static_cast<ITextVectorProperty   *>(d->property)->timestamp, timestamp, MAXINDITSTAMP); break;
        case INDI_SWITCH: strncpy(static_cast<ISwitchVectorProperty *>(d->property)->timestamp, timestamp, MAXINDITSTAMP); break;
        case INDI_LIGHT:  strncpy(static_cast<ILightVectorProperty  *>(d->property)->timestamp, timestamp, MAXINDITSTAMP); break;
        case INDI_BLOB:   strncpy(static_cast<IBLOBVectorProperty   *>(d->property)->timestamp, timestamp, MAXINDITSTAMP); break;
        default:;
    }
}

void Property::setState(IPState state)
{
    D_PTR(Property);
    switch (getType())
    {
        case INDI_NUMBER: static_cast<INumberVectorProperty *>(d->property)->s = state; break;
        case INDI_TEXT:   static_cast<ITextVectorProperty   *>(d->property)->s = state; break;
        case INDI_SWITCH: static_cast<ISwitchVectorProperty *>(d->property)->s = state; break;
        case INDI_LIGHT:  static_cast<ILightVectorProperty  *>(d->property)->s = state; break;
        case INDI_BLOB:   static_cast<IBLOBVectorProperty   *>(d->property)->s = state; break;
        default:;
    }
}

void Property::setPermission(IPerm permission)
{
    D_PTR(Property);
    switch (getType())
    {
        case INDI_NUMBER: static_cast<INumberVectorProperty *>(d->property)->p = permission; break;
        case INDI_TEXT:   static_cast<ITextVectorProperty   *>(d->property)->p = permission; break;
        case INDI_SWITCH: static_cast<ISwitchVectorProperty *>(d->property)->p = permission; break;
        //case INDI_LIGHT:  static_cast<ILightVectorProperty  *>(d->property)->p = permission; break;
        case INDI_BLOB:   static_cast<IBLOBVectorProperty   *>(d->property)->p = permission; break;
        default:;
    }
}

void Property::setTimeout(double timeout)
{
    D_PTR(Property);
    switch (getType())
    {
        case INDI_NUMBER: static_cast<INumberVectorProperty *>(d->property)->timeout = timeout; break;
        case INDI_TEXT:   static_cast<ITextVectorProperty   *>(d->property)->timeout = timeout; break;
        case INDI_SWITCH: static_cast<ISwitchVectorProperty *>(d->property)->timeout = timeout; break;
        //case INDI_LIGHT:  static_cast<ILightVectorProperty  *>(d->property)->timeout = timeout; break;
        case INDI_BLOB:   static_cast<IBLOBVectorProperty   *>(d->property)->timeout = timeout; break;
        default:;
    }
}

const char *Property::getName() const
{
    D_PTR(const Property);
    switch (getType())
    {
        case INDI_NUMBER: return (static_cast<INumberVectorProperty *>(d->property)->name);
        case INDI_TEXT:   return (static_cast<ITextVectorProperty   *>(d->property)->name);
        case INDI_SWITCH: return (static_cast<ISwitchVectorProperty *>(d->property)->name);
        case INDI_LIGHT:  return (static_cast<ILightVectorProperty  *>(d->property)->name);
        case INDI_BLOB:   return (static_cast<IBLOBVectorProperty   *>(d->property)->name);
        default: return nullptr;
    }
}

const char *Property::getLabel() const
{
    D_PTR(const Property);
    switch (getType())
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(d->property)->label;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(d->property)->label;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(d->property)->label;
        case INDI_LIGHT:  return static_cast <ILightVectorProperty  *>(d->property)->label;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(d->property)->label;
        default: return nullptr;
    }
}


const char *Property::getGroupName() const
{
    D_PTR(const Property);
    switch (getType())
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(d->property)->group;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(d->property)->group;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(d->property)->group;
        case INDI_LIGHT:  return static_cast <ILightVectorProperty  *>(d->property)->group;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(d->property)->group;
        default:          return nullptr;
    }
}

const char *Property::getDeviceName() const
{
    D_PTR(const Property);
    switch (getType())
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(d->property)->device;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(d->property)->device;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(d->property)->device;
        case INDI_LIGHT:  return static_cast <ILightVectorProperty  *>(d->property)->device;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(d->property)->device;
        default:          return nullptr;
    }
}

const char *Property::getTimestamp() const
{
    D_PTR(const Property);
    switch (getType())
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(d->property)->timestamp;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(d->property)->timestamp;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(d->property)->timestamp;
        case INDI_LIGHT:  return static_cast <ILightVectorProperty  *>(d->property)->timestamp;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(d->property)->timestamp;
        default:          return nullptr;
    }
}

IPState Property::getState() const
{
    D_PTR(const Property);
    switch (getType())
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(d->property)->s;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(d->property)->s;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(d->property)->s;
        case INDI_LIGHT:  return static_cast <ILightVectorProperty  *>(d->property)->s;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(d->property)->s;
        default:          return IPS_ALERT;
    }
}

IPerm Property::getPermission() const
{
    D_PTR(const Property);
    switch (getType())
    {
        case INDI_NUMBER: return static_cast <INumberVectorProperty *>(d->property)->p;
        case INDI_TEXT:   return static_cast <ITextVectorProperty   *>(d->property)->p;
        case INDI_SWITCH: return static_cast <ISwitchVectorProperty *>(d->property)->p;
        case INDI_BLOB:   return static_cast <IBLOBVectorProperty   *>(d->property)->p;
        default:          return IP_RO;
    }
}

INumberVectorProperty *Property::getNumber() const
{
    D_PTR(const Property);
    if (d->type == INDI_NUMBER)
        return static_cast <INumberVectorProperty * > (d->property);

    return nullptr;
}

ITextVectorProperty *Property::getText() const
{
    D_PTR(const Property);
    if (d->type == INDI_TEXT)
        return static_cast <ITextVectorProperty * > (d->property);

    return nullptr;
}

ILightVectorProperty *Property::getLight() const
{
    D_PTR(const Property);
    if (d->type == INDI_LIGHT)
        return static_cast <ILightVectorProperty * > (d->property);

    return nullptr;
}

ISwitchVectorProperty *Property::getSwitch() const
{
    D_PTR(const Property);
    if (d->type == INDI_SWITCH)
        return static_cast <ISwitchVectorProperty * > (d->property);

    return nullptr;
}

IBLOBVectorProperty *Property::getBLOB() const
{
    D_PTR(const Property);
    if (d->type == INDI_BLOB)
        return static_cast <IBLOBVectorProperty * > (d->property);

    return nullptr;
}

}
