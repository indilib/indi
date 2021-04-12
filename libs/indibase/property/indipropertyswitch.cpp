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

PropertySwitch::~PropertySwitch()
{ }

void PropertySwitch::reset()
{
    D_PTR(PropertySwitch);
    d->property.reset();
}

int PropertySwitch::findOnSwitchIndex() const
{
    D_PTR(const PropertySwitch);
    return d->property.findOnSwitchIndex();
}

INDI::WidgetView<ISwitch> *PropertySwitch::findOnSwitch() const
{
    D_PTR(const PropertySwitch);
    return d->property.findOnSwitch();
}

bool PropertySwitch::update(const ISState states[], const char * const names[], int n)
{
    D_PTR(PropertySwitch);
    return d->property.update(states, names, n);
}

void PropertySwitch::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPerm permission, ISRule rule, double timeout, IPState state
)
{
    D_PTR(PropertySwitch);
    d->property.setWidgets(d->widgets.data(), d->widgets.size());
    d->property.fill(device, name, label, group, permission, rule, timeout, state);
}

void PropertySwitch::setRule(ISRule rule)
{
    D_PTR(PropertySwitch);
    d->property.setRule(rule);
}

ISRule PropertySwitch::getRule() const
{
    D_PTR(const PropertySwitch);
    return d->property.getRule();
}

const char * PropertySwitch::getRuleAsString() const
{
    D_PTR(const PropertySwitch);
    return d->property.getRuleAsString();
}

}
