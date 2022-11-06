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

#include "indipropertyblob.h"
#include "indipropertyblob_p.h"

namespace INDI
{

PropertyBlobPrivate::PropertyBlobPrivate(size_t count)
    : PropertyBasicPrivateTemplate<IBLOB>(count)
{ }

PropertyBlobPrivate::~PropertyBlobPrivate()
{ }

PropertyBlob::PropertyBlob(size_t count)
    : PropertyBasic<IBLOB>(*new PropertyBlobPrivate(count))
{ }

PropertyBlob::PropertyBlob(INDI::Property property)
    : PropertyBasic<IBLOB>(property_private_cast<PropertyBlobPrivate>(property.d_ptr))
{ }

PropertyBlob::~PropertyBlob()
{ }

bool PropertyBlob::update(
    const int sizes[], const int blobsizes[], const char * const blobs[], const char * const formats[],
    const char * const names[], int n
)
{
    D_PTR(PropertyBlob);
    return d->typedProperty.update(sizes, blobsizes, blobs, formats, names, n) && (emitUpdate(), true);
}

void PropertyBlob::fill(
    const char *device, const char *name, const char *label, const char *group,
    IPerm permission, double timeout, IPState state
)
{
    D_PTR(PropertyBlob);
    d->typedProperty.setWidgets(d->widgets.data(), d->widgets.size());
    d->typedProperty.fill(device, name, label, group, permission, timeout, state);
}

}
