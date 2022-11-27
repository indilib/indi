/*******************************************************************************
  Copyright(c) 2011 Jasem Mutlaq. All rights reserved.
               2022 Pawel Soja <kernel32.pl@gmail.com>

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

#include "basedevice.h"

#include "indipropertytext_p.h"
#include "indipropertyswitch_p.h"
#include "indipropertynumber_p.h"
#include "indipropertylight_p.h"
#include "indipropertyblob_p.h"

#include "indipropertytext.h"
#include "indipropertyswitch.h"
#include "indipropertynumber.h"
#include "indipropertylight.h"
#include "indipropertyblob.h"

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

#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
INDI::Property* Property::operator->()
{
    return this;
}

const INDI::Property* Property::operator->() const
{
    return this;
}

Property::operator INDI::Property *()
{
    D_PTR(Property);
    return isValid() ? &d->self : nullptr;
}

Property::operator const INDI::Property *() const
{
    D_PTR(const Property);
    return isValid() ? &d->self : nullptr;
}

Property::operator INDI::PropertyView<INumber> *() const { return this->getNumber(); }
Property::operator INDI::PropertyView<IText>   *() const { return this->getText(); }
Property::operator INDI::PropertyView<ISwitch> *() const { return this->getSwitch(); }
Property::operator INDI::PropertyView<ILight>  *() const { return this->getLight(); }
Property::operator INDI::PropertyView<IBLOB>   *() const { return this->getBLOB(); }

#endif

#define PROPERTY_CASE(CODE) \
    switch (d->property != nullptr ? d->type : INDI_UNKNOWN) \
    { \
        case INDI_NUMBER: { auto property = static_cast<PropertyView<INumber> *>(d->property); CODE } break; \
        case INDI_TEXT:   { auto property = static_cast<PropertyView<IText>   *>(d->property); CODE } break; \
        case INDI_SWITCH: { auto property = static_cast<PropertyView<ISwitch> *>(d->property); CODE } break; \
        case INDI_LIGHT:  { auto property = static_cast<PropertyView<ILight>  *>(d->property); CODE } break; \
        case INDI_BLOB:   { auto property = static_cast<PropertyView<IBLOB>   *>(d->property); CODE } break; \
        default:; \
    }

PropertyPrivate::~PropertyPrivate()
{
    // Only delete properties if they were created dynamically via the buildSkeleton
    // function. Other drivers are responsible for their own memory allocation.
    if (property == nullptr || !dynamic)
        return;

    auto d = this;
    PROPERTY_CASE( delete property; )
}

Property::Property()
    : d_ptr(new PropertyPrivate(nullptr, INDI_UNKNOWN))
{ }

Property::Property(INDI::PropertyNumber property)
    : d_ptr(property.d_ptr)
{ }

Property::Property(INDI::PropertyText   property)
    : d_ptr(property.d_ptr)
{ }

Property::Property(INDI::PropertySwitch property)
    : d_ptr(property.d_ptr)
{ }

Property::Property(INDI::PropertyLight  property)
    : d_ptr(property.d_ptr)
{ }

Property::Property(INDI::PropertyBlob   property)
    : d_ptr(property.d_ptr)
{ }

#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
Property::Property(INumberVectorProperty *property)
    : d_ptr(new PropertyNumberPrivate(property))
{ }

Property::Property(ITextVectorProperty   *property)
    : d_ptr(new PropertyTextPrivate(property))
{ }

Property::Property(ISwitchVectorProperty *property)
    : d_ptr(new PropertySwitchPrivate(property))
{ }

Property::Property(ILightVectorProperty  *property)
    : d_ptr(new PropertyLightPrivate(property))
{ }

Property::Property(IBLOBVectorProperty   *property)
    : d_ptr(new PropertyBlobPrivate(property))
{ }
#endif

Property::~Property()
{ }

Property::Property(PropertyPrivate &dd)
    : d_ptr(&dd)
{ }

Property::Property(const std::shared_ptr<PropertyPrivate> &dd)
    : d_ptr(dd)
{ }

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
    d->baseDevice = (idp == nullptr ? BaseDevice() : *idp);
}

void Property::setBaseDevice(BaseDevice baseDevice)
{
    D_PTR(Property);
    d->baseDevice = baseDevice;
}

#if 0
void *Property::getProperty() const
{
    D_PTR(const Property);
    return d->property;
}
#endif

INDI_PROPERTY_TYPE Property::getType() const
{
    D_PTR(const Property);
    return d->property != nullptr ? d->type : INDI_UNKNOWN;
}

const char *Property::getTypeAsString() const
{
    switch (getType())
    {
        case INDI_NUMBER:
            return "INDI_NUMBER";
        case INDI_SWITCH:
            return "INDI_SWITCH";
        case INDI_TEXT:
            return "INDI_TEXT";
        case INDI_LIGHT:
            return "INDI_LIGHT";
        case INDI_BLOB:
            return "INDI_BLOB";
        case INDI_UNKNOWN:
            return "INDI_UNKNOWN";
    }
    return "INDI_UNKNOWN";
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

BaseDevice Property::getBaseDevice() const
{
    D_PTR(const Property);
    return d->baseDevice;
}

void Property::setName(const char *name)
{
    D_PTR(Property);
    PROPERTY_CASE( property->setName(name); )
}

void Property::setLabel(const char *label)
{
    D_PTR(Property);
    PROPERTY_CASE( property->setLabel(label); )
}

void Property::setGroupName(const char *group)
{
    D_PTR(Property);
    PROPERTY_CASE( property->setGroupName(group); )
}

void Property::setDeviceName(const char *device)
{
    D_PTR(Property);
    PROPERTY_CASE( property->setDeviceName(device); )
}

void Property::setTimestamp(const char *timestamp)
{
    D_PTR(Property);
    PROPERTY_CASE( property->setTimestamp(timestamp); )
}

void Property::setState(IPState state)
{
    D_PTR(Property);
    PROPERTY_CASE( property->setState(state); )
}

void Property::setPermission(IPerm permission)
{
    D_PTR(Property);
    PROPERTY_CASE( property->setPermission(permission); )
}

void Property::setTimeout(double timeout)
{
    D_PTR(Property);
    PROPERTY_CASE( property->setTimeout(timeout); )
}

const char *Property::getName() const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->getName(); )
    return nullptr;
}

const char *Property::getLabel() const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->getLabel(); )
    return nullptr;
}

const char *Property::getGroupName() const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->getGroupName(); )
    return nullptr;
}

const char *Property::getDeviceName() const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->getDeviceName(); )
    return nullptr;
}

const char *Property::getTimestamp() const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->getTimestamp(); )
    return nullptr;
}

IPState Property::getState() const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->getState(); )
    return IPS_ALERT;
}

const char *Property::getStateAsString() const
{
    return pstateStr(getState());
}

IPerm Property::getPermission() const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->getPermission(); )
    return IP_RO;
}

bool Property::isEmpty() const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->isEmpty(); )
    return true;
}

bool Property::isValid() const
{
    D_PTR(const Property);
    return d->type != INDI_UNKNOWN;
}

bool Property::isNameMatch(const char *otherName) const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->isNameMatch(otherName); )
    return false;
}

bool Property::isNameMatch(const std::string &otherName) const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->isNameMatch(otherName); )
    return false;
}

bool Property::isLabelMatch(const char *otherLabel) const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->isLabelMatch(otherLabel); )
    return false;
}

bool Property::isLabelMatch(const std::string &otherLabel) const
{
    D_PTR(const Property);
    PROPERTY_CASE( return property->isLabelMatch(otherLabel); )
    return false;
}

INDI::PropertyNumber Property::getNumber() const
{
    return *this;
}

INDI::PropertyText Property::getText() const
{
    return *this;
}

INDI::PropertyLight Property::getLight() const
{
    return *this;
}

INDI::PropertySwitch Property::getSwitch() const
{
    return *this;
}

INDI::PropertyBlob Property::getBLOB() const
{
    return *this;
}

void Property::save(FILE *fp) const
{
    D_PTR(const Property);
    PROPERTY_CASE( property->save(fp); )
}

void Property::apply(const char *format, ...) const
{
    D_PTR(const Property);
    va_list ap;
    va_start(ap, format);
    PROPERTY_CASE( property->vapply(format, ap); )
    va_end(ap);
}

void Property::define(const char *format, ...) const
{
    D_PTR(const Property);
    va_list ap;
    va_start(ap, format);
    PROPERTY_CASE( property->vdefine(format, ap); )
    va_end(ap);
}

void Property::onUpdate(const std::function<void()> &callback)
{
    D_PTR(Property);
    d->onUpdateCallback = callback;
}

void Property::emitUpdate()
{
    D_PTR(Property);
    if (d->onUpdateCallback)
        d->onUpdateCallback();
}

bool Property::hasUpdateCallback() const
{
    D_PTR(const Property);
    return d->onUpdateCallback != nullptr;
}

}
