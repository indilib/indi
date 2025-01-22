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

#include "indipropertytext.h"
#include "indipropertytext_p.h"

namespace INDI
{

PropertyTextPrivate::PropertyTextPrivate(size_t count)
    : PropertyBasicPrivateTemplate<IText>(count)
{ }

PropertyTextPrivate::~PropertyTextPrivate()
{ }

PropertyText::PropertyText(size_t count)
    : PropertyBasic<IText>(*new PropertyTextPrivate(count))
{ }

PropertyText::PropertyText(INDI::Property property)
    : PropertyBasic<IText>(property_private_cast<PropertyTextPrivate>(property.d_ptr))
{ }

PropertyText::~PropertyText()
{ }

bool PropertyText::update(const char * const texts[], const char * const names[], int n)
{
    D_PTR(PropertyText);
    return d->typedProperty.update(texts, names, n) && (emitUpdate(), true);
}

void PropertyText::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPerm permission, double timeout, IPState state
)
{
    D_PTR(PropertyText);
    d->typedProperty.setWidgets(d->widgets.data(), d->widgets.size());
    d->typedProperty.fill(device, name, label, group, permission, timeout, state);
}

bool PropertyText::isUpdated(const char * const texts[], const char * const names[], int n) const
{
    D_PTR(const PropertyText);
    return d->typedProperty.isUpdated(texts, names, n);
}

}
