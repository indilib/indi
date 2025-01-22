/*
    Copyright (C) 2021 by Pawel Soja <kernel32.pl@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "indipropertyswitch.h"
#include "indipropertyswitch_p.h"

namespace INDI
{

PropertySwitchPrivate::PropertySwitchPrivate(size_t count)
    : PropertyBasicPrivateTemplate<ISwitch>(count)
{ }

PropertySwitchPrivate::~PropertySwitchPrivate()
{ }

PropertySwitch::PropertySwitch(size_t count)
    : PropertyBasic<ISwitch>(*new PropertySwitchPrivate(count))
{ }

PropertySwitch::PropertySwitch(INDI::Property property)
    : PropertyBasic<ISwitch>(property_private_cast<PropertySwitchPrivate>(property.d_ptr))
{ }

PropertySwitch::~PropertySwitch()
{ }

void PropertySwitch::reset()
{
    D_PTR(PropertySwitch);
    d->typedProperty.reset();
}

int PropertySwitch::findOnSwitchIndex() const
{
    D_PTR(const PropertySwitch);
    return d->typedProperty.findOnSwitchIndex();
}

std::string PropertySwitch::findOnSwitchName() const
{
    D_PTR(const PropertySwitch);
    return d->typedProperty.findOnSwitchName();
}

INDI::WidgetViewSwitch *PropertySwitch::findOnSwitch() const
{
    D_PTR(const PropertySwitch);
    return d->typedProperty.findOnSwitch();
}

bool PropertySwitch::isSwitchOn(const std::string &name) const
{
    D_PTR(const PropertySwitch);
    return d->typedProperty.isSwitchOn(name);
}

bool PropertySwitch::update(const ISState states[], const char * const names[], int n)
{
    D_PTR(PropertySwitch);
    if (d->onNewValuesCallback)
    {
        NewValues newValues;
        for (int i = 0; i < n; ++i)
        {
            newValues[names[i]] = states[i];
        }

        d->onNewValuesCallback(newValues);
        return true;
    }
    return d->typedProperty.update(states, names, n) && (emitUpdate(), true);
}

bool PropertySwitch::hasUpdateCallback() const
{
    D_PTR(const PropertySwitch);
    return d->onNewValuesCallback != nullptr || d->onUpdateCallback != nullptr;
}

void PropertySwitch::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPerm permission, ISRule rule, double timeout, IPState state
)
{
    D_PTR(PropertySwitch);
    d->typedProperty.setWidgets(d->widgets.data(), d->widgets.size());
    d->typedProperty.fill(device, name, label, group, permission, rule, timeout, state);
}

void PropertySwitch::setRule(ISRule rule)
{
    D_PTR(PropertySwitch);
    d->typedProperty.setRule(rule);
}

ISRule PropertySwitch::getRule() const
{
    D_PTR(const PropertySwitch);
    return d->typedProperty.getRule();
}

const char * PropertySwitch::getRuleAsString() const
{
    D_PTR(const PropertySwitch);
    return d->typedProperty.getRuleAsString();
}

bool PropertySwitch::isUpdated(const ISState states[], const char * const names[], int n) const
{
    D_PTR(const PropertySwitch);
    return d->typedProperty.isUpdated(states, names, n);
}

void PropertySwitch::onNewValues(const std::function<void(const INDI::PropertySwitch::NewValues &)> &callback)
{
    D_PTR(PropertySwitch);
    d->onNewValuesCallback = callback;
}

}
