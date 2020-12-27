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

const char *Property::getName() const
{
    if (pPtr == nullptr)
        return nullptr;

    switch (pType)
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
    if (pPtr == nullptr)
        return nullptr;

    switch (pType)
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
    if (pPtr == nullptr)
        return nullptr;

    switch (pType)
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
    if (pPtr == nullptr)
        return nullptr;

    switch (pType)
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
    if (pPtr == nullptr)
        return nullptr;

    switch (pType)
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
    if (pPtr == nullptr)
        return IPS_ALERT;

    switch (pType)
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
    if (pPtr == nullptr)
        return IP_RO;

    switch (pType)
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
    if (pPtr != nullptr)
        if (pType == INDI_NUMBER)
            return static_cast <INumberVectorProperty * > (pPtr);

    return nullptr;
}

ITextVectorProperty *Property::getText() const
{
    if (pPtr != nullptr)
        if (pType == INDI_TEXT)
            return static_cast <ITextVectorProperty * > (pPtr);

    return nullptr;
}

ILightVectorProperty *Property::getLight() const
{
    if (pPtr != nullptr)
        if (pType == INDI_LIGHT)
            return static_cast <ILightVectorProperty * > (pPtr);

    return nullptr;
}

ISwitchVectorProperty *Property::getSwitch() const
{
    if (pPtr != nullptr)
        if (pType == INDI_SWITCH)
            return static_cast <ISwitchVectorProperty * > (pPtr);

    return nullptr;
}

IBLOBVectorProperty *Property::getBLOB() const
{
    if (pPtr != nullptr)
        if (pType == INDI_BLOB)
            return static_cast <IBLOBVectorProperty * > (pPtr);

    return nullptr;
}

}