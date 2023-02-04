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

#include "indipropertybasic.h"
#include "indipropertybasic_p.h"
#include <cassert>

namespace INDI
{

template <typename T>
PropertyBasicPrivateTemplate<T>::PropertyBasicPrivateTemplate(size_t count)
#ifdef INDI_PROPERTY_RAW_CAST
    : PropertyContainer<T>{*new PropertyView<T>()}
    , PropertyPrivate(&this->typedProperty)
    , raw{false}
#else
    : PropertyPrivate(&property)
#endif
    , widgets(count)
{
    this->typedProperty.setWidgets(widgets.data(), widgets.size());
}

#ifdef INDI_PROPERTY_RAW_CAST
template <typename T>
PropertyBasicPrivateTemplate<T>::PropertyBasicPrivateTemplate(RawPropertyType *rawProperty)
    : PropertyContainer<T>{*PropertyView<T>::cast(rawProperty)}
    , PropertyPrivate(PropertyView<T>::cast(rawProperty))
    , raw{true}
{ }
#endif

template <typename T>
PropertyBasicPrivateTemplate<T>::~PropertyBasicPrivateTemplate()
{
#ifdef INDI_PROPERTY_RAW_CAST
    if (!raw)
        delete &this->typedProperty;
#endif
}

template <typename T>
PropertyBasic<T>::~PropertyBasic()
{ }

template <typename T>
PropertyBasic<T>::PropertyBasic(PropertyBasicPrivate &dd)
    : Property(dd)
{ }

template <typename T>
PropertyBasic<T>::PropertyBasic(const std::shared_ptr<PropertyBasicPrivate> &dd)
    : Property(std::static_pointer_cast<PropertyPrivate>(dd))
{ }

template <typename T>
void PropertyBasic<T>::setName(const char *name)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setName(name);
}

template <typename T>
void PropertyBasic<T>::setName(const std::string &name)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setName(name);
}

template <typename T>
void PropertyBasic<T>::setLabel(const char *label)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setLabel(label);
}

template <typename T>
void PropertyBasic<T>::setLabel(const std::string &label)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setLabel(label);
}

template <typename T>
void PropertyBasic<T>::setGroupName(const char *name)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setGroupName(name);
}

template <typename T>
void PropertyBasic<T>::setGroupName(const std::string &name)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setGroupName(name);
}

template <typename T>
void PropertyBasic<T>::setPermission(IPerm permission)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setPermission(permission);
}

template <typename T>
void PropertyBasic<T>::setTimeout(double timeout)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setTimeout(timeout);
}

template <typename T>
void PropertyBasic<T>::setState(IPState state)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setState(state);
}

template <typename T>
void PropertyBasic<T>::setTimestamp(const char *timestamp)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setTimestamp(timestamp);
}

template <typename T>
void PropertyBasic<T>::setTimestamp(const std::string &timestamp)
{
    D_PTR(PropertyBasic);
    d->typedProperty.setTimestamp(timestamp);
}

template <typename T>
const char *PropertyBasic<T>::getName() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.getName();
}

template <typename T>
const char *PropertyBasic<T>::getLabel() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.getLabel();
}

template <typename T>
const char *PropertyBasic<T>::getGroupName() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.getGroupName();
}

template <typename T>
IPerm PropertyBasic<T>::getPermission() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.getPermission();
}

template <typename T>
const char *PropertyBasic<T>::getPermissionAsString() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.getPermissionAsString();
}

template <typename T>
double PropertyBasic<T>::getTimeout() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.getTimeout();
}

template <typename T>
IPState PropertyBasic<T>::getState() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.getState();
}

template <typename T>
const char *PropertyBasic<T>::getStateAsString() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.getStateAsString();
}

template <typename T>
const char *PropertyBasic<T>::getTimestamp() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.getTimestamp();
}

template <typename T>
bool PropertyBasic<T>::isEmpty() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.isEmpty();
}

template <typename T>
bool PropertyBasic<T>::isNameMatch(const char *otherName) const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.isNameMatch(otherName);
}

template <typename T>
bool PropertyBasic<T>::isNameMatch(const std::string &otherName) const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.isNameMatch(otherName);
}

template <typename T>
bool PropertyBasic<T>::isLabelMatch(const char *otherLabel) const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.isLabelMatch(otherLabel);
}

template <typename T>
bool PropertyBasic<T>::isLabelMatch(const std::string &otherLabel) const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.isLabelMatch(otherLabel);
}

template <typename T>
void PropertyBasic<T>::save(FILE *f) const
{
    D_PTR(const PropertyBasic);
    d->typedProperty.save(f);
}

template <typename T>
void PropertyBasic<T>::vapply(const char *format, va_list args) const
{
    D_PTR(const PropertyBasic);
    d->typedProperty.vapply(format, args);
}

template <typename T>
void PropertyBasic<T>::vdefine(const char *format, va_list args) const
{
    D_PTR(const PropertyBasic);
    d->typedProperty.vdefine(format, args);
}

template <typename T>
void PropertyBasic<T>::apply(const char *format, ...) const
{
    D_PTR(const PropertyBasic);
    va_list ap;
    va_start(ap, format);
    d->typedProperty.vapply(format, ap);
    va_end(ap);
}

template <typename T>
void PropertyBasic<T>::define(const char *format, ...) const
{
    D_PTR(const PropertyBasic);
    va_list ap;
    va_start(ap, format);
    d->typedProperty.vdefine(format, ap);
    va_end(ap);
}

template <typename T>
void PropertyBasic<T>::apply() const
{
    D_PTR(const PropertyBasic);
    d->typedProperty.apply();
}

template <typename T>
void PropertyBasic<T>::define() const
{
    D_PTR(const PropertyBasic);
    d->typedProperty.define();
}

template <typename T>
WidgetView<T> *PropertyBasic<T>::findWidgetByName(const char *name) const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.findWidgetByName(name);
}

template <typename T>
int PropertyBasic<T>::findWidgetIndexByName(const char *name) const
{
    auto it = findWidgetByName(name);
    return int(it == nullptr ? -1 : it - begin());
}

template <typename T>
size_t PropertyBasic<T>::size() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.count();
}

template <typename T>
void PropertyBasic<T>::resize(size_t size)
{
    D_PTR(PropertyBasic);
#ifdef INDI_PROPERTY_RAW_CAST
    assert(d->raw == false);
#endif
    d->widgets.resize(size);
    d->typedProperty.setWidgets(d->widgets.data(), d->widgets.size());
}

template <typename T>
void PropertyBasic<T>::reserve(size_t size)
{
    D_PTR(PropertyBasic);
#ifdef INDI_PROPERTY_RAW_CAST
    assert(d->raw == false);
#endif
    d->widgets.reserve(size);
    d->typedProperty.setWidgets(d->widgets.data(), d->widgets.size());
}

template <typename T>
void PropertyBasic<T>::shrink_to_fit()
{
    D_PTR(PropertyBasic);
#ifdef INDI_PROPERTY_RAW_CAST
    assert(d->raw == false);
#endif
    d->widgets.shrink_to_fit();
    d->typedProperty.setWidgets(d->widgets.data(), d->widgets.size());
}

template <typename T>
void PropertyBasic<T>::push(WidgetView<T> &&item)
{
    D_PTR(PropertyBasic);
#ifdef INDI_PROPERTY_RAW_CAST
    assert(d->raw == false);
#endif
    item.setParent(&d->typedProperty);
    d->widgets.push_back(std::move(item));
    d->typedProperty.setWidgets(d->widgets.data(), d->widgets.size());
}

template <typename T>
void PropertyBasic<T>::push(const WidgetView<T> &item)
{
    push(std::move(WidgetView<T>(item)));
}

template <typename T>
const WidgetView<T> *PropertyBasic<T>::at(size_t index) const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.at(index);
}

template <typename T>
WidgetView<T> &PropertyBasic<T>::operator[](ssize_t index) const
{
    D_PTR(const PropertyBasic);
    assert(index >= 0);
    return *d->typedProperty.at(index);
}

template <typename T>
WidgetView<T> *PropertyBasic<T>::begin()
{
    D_PTR(PropertyBasic);
    return d->typedProperty.begin();
}

template <typename T>
WidgetView<T> *PropertyBasic<T>::end()
{
    D_PTR(PropertyBasic);
    return d->typedProperty.end();
}

template <typename T>
const WidgetView<T> *PropertyBasic<T>::begin() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.begin();
}

template <typename T>
const WidgetView<T> *PropertyBasic<T>::end() const
{
    D_PTR(const PropertyBasic);
    return d->typedProperty.end();
}

template <typename T>
PropertyView<T> * PropertyBasic<T>::operator &()
{
    D_PTR(PropertyBasic);
    return &d->typedProperty;
}

#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
template <typename T>
PropertyView<T> *PropertyBasic<T>::operator ->()
{
    D_PTR(PropertyBasic);
    return static_cast<PropertyView<T> *>(static_cast<INDI::PropertyPrivate*>(d)->property);
}

template <typename T>
INDI::PropertyView<T> PropertyBasic<T>::operator*()
{
    D_PTR(PropertyBasic);
    return *static_cast<PropertyView<T> *>(static_cast<INDI::PropertyPrivate*>(d)->property);
}
#endif

template class PropertyBasicPrivateTemplate<IText>;
template class PropertyBasicPrivateTemplate<INumber>;
template class PropertyBasicPrivateTemplate<ISwitch>;
template class PropertyBasicPrivateTemplate<ILight>;
template class PropertyBasicPrivateTemplate<IBLOB>;

template class PropertyBasic<IText>;
template class PropertyBasic<INumber>;
template class PropertyBasic<ISwitch>;
template class PropertyBasic<ILight>;
template class PropertyBasic<IBLOB>;

}
