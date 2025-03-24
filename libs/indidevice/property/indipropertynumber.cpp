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

#include "indipropertynumber.h"
#include "indipropertynumber_p.h"

namespace INDI
{

PropertyNumberPrivate::PropertyNumberPrivate(size_t count)
    : PropertyBasicPrivateTemplate<INumber>(count)
{ }

PropertyNumberPrivate::~PropertyNumberPrivate()
{ }

PropertyNumber::PropertyNumber(size_t count)
    : PropertyBasic<INumber>(*new PropertyNumberPrivate(count))
{ }

PropertyNumber::PropertyNumber(INDI::Property property)
    : PropertyBasic<INumber>(property_private_cast<PropertyNumberPrivate>(property.d_ptr))
{ }

PropertyNumber::~PropertyNumber()
{ }

bool PropertyNumber::update(const double values[], const char * const names[], int n)
{
    D_PTR(PropertyNumber);
    return d->typedProperty.update(values, names, n) && (emitUpdate(), true);
}

void PropertyNumber::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPerm permission, double timeout, IPState state
)
{
    D_PTR(PropertyNumber);
    d->typedProperty.setWidgets(d->widgets.data(), d->widgets.size());
    d->typedProperty.fill(device, name, label, group, permission, timeout, state);
}

void PropertyNumber::updateMinMax()
{
    D_PTR(PropertyNumber);
    d->typedProperty.updateMinMax();
}

bool PropertyNumber::isUpdated(const double values[], const char * const names[], int n) const
{
    D_PTR(const PropertyNumber);
    return d->typedProperty.isUpdated(values, names, n);
}

}
