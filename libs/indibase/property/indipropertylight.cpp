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

#include "indipropertylight.h"
#include "indipropertylight_p.h"

namespace INDI
{

PropertyLightPrivate::PropertyLightPrivate(size_t count)
    : PropertyBasicPrivateTemplate<ILight>(count)
{ }

PropertyLightPrivate::~PropertyLightPrivate()
{ }

PropertyLight::PropertyLight(size_t count)
    : PropertyBasic<ILight>(*new PropertyLightPrivate(count))
{ }

PropertyLight::PropertyLight(INDI::Property property)
    : PropertyBasic<ILight>(property_private_cast<PropertyLightPrivate>(property.d_ptr))
{ }

PropertyLight::~PropertyLight()
{ }

void PropertyLight::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPState state
)
{
    D_PTR(PropertyLight);
    d->typedProperty.setWidgets(d->widgets.data(), d->widgets.size());
    d->typedProperty.fill(device, name, label, group, state);
}

}
