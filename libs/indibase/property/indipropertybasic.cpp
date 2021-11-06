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

namespace INDI
{

template <typename T>
PropertyBasicPrivateTemplate<T>::PropertyBasicPrivateTemplate(size_t count)
    : PropertyPrivate(&property)
    , widgets(count)
{
    property.setWidgets(widgets.data(), widgets.size());
}

template <typename T>
PropertyBasicPrivateTemplate<T>::~PropertyBasicPrivateTemplate()
{ }

template <typename T>
PropertyBasic<T>::~PropertyBasic()
{ }

template <typename T>
PropertyBasic<T>::PropertyBasic(PropertyBasicPrivate &dd)
    : Property(dd)
{ }

template <typename T>
void PropertyBasic<T>::setName(const char *name)
{
    D_PTR(PropertyBasic);
    d->property.setName(name);
}

template <typename T>
void PropertyBasic<T>::setName(const std::string &name)
{
    D_PTR(PropertyBasic);
    d->property.setName(name);
}

template <typename T>
void PropertyBasic<T>::setLabel(const char *label)
{
    D_PTR(PropertyBasic);
    d->property.setLabel(label);
}

template <typename T>
void PropertyBasic<T>::setLabel(const std::string &label)
{
    D_PTR(PropertyBasic);
    d->property.setLabel(label);
}

template <typename T>
void PropertyBasic<T>::setGroupName(const char *name)
{
    D_PTR(PropertyBasic);
    d->property.setGroupName(name);
}

template <typename T>
void PropertyBasic<T>::setGroupName(const std::string &name)
{
    D_PTR(PropertyBasic);
    d->property.setGroupName(name);
}

template <typename T>
void PropertyBasic<T>::setPermission(IPerm permission)
{
    D_PTR(PropertyBasic);
    d->property.setPermission(permission);
}

template <typename T>
void PropertyBasic<T>::setTimeout(double timeout)
{
    D_PTR(PropertyBasic);
    d->property.setTimeout(timeout);
}

template <typename T>
void PropertyBasic<T>::setState(IPState state)
{
    D_PTR(PropertyBasic);
    d->property.setState(state);
}

template <typename T>
void PropertyBasic<T>::setTimestamp(const char *timestamp)
{
    D_PTR(PropertyBasic);
    d->property.setTimestamp(timestamp);
}

template <typename T>
void PropertyBasic<T>::setTimestamp(const std::string &timestamp)
{
    D_PTR(PropertyBasic);
    d->property.setTimestamp(timestamp);
}

template <typename T>
const char *PropertyBasic<T>::getName() const
{
    D_PTR(const PropertyBasic);
    return d->property.getName();
}

template <typename T>
const char *PropertyBasic<T>::getLabel() const
{
    D_PTR(const PropertyBasic);
    return d->property.getLabel();
}

template <typename T>
const char *PropertyBasic<T>::getGroupName() const
{
    D_PTR(const PropertyBasic);
    return d->property.getGroupName();
}

template <typename T>
IPerm PropertyBasic<T>::getPermission() const
{
    D_PTR(const PropertyBasic);
    return d->property.getPermission();
}

template <typename T>
const char *PropertyBasic<T>::getPermissionAsString() const
{
    D_PTR(const PropertyBasic);
    return d->property.getPermissionAsString();
}

template <typename T>
double PropertyBasic<T>::getTimeout() const
{
    D_PTR(const PropertyBasic);
    return d->property.getTimeout();
}

template <typename T>
IPState PropertyBasic<T>::getState() const
{
    D_PTR(const PropertyBasic);
    return d->property.getState();
}

template <typename T>
const char *PropertyBasic<T>::getStateAsString() const
{
    D_PTR(const PropertyBasic);
    return d->property.getStateAsString();
}

template <typename T>
const char *PropertyBasic<T>::getTimestamp() const
{
    D_PTR(const PropertyBasic);
    return d->property.getTimestamp();
}

template <typename T>
bool PropertyBasic<T>::isEmpty() const
{
    D_PTR(const PropertyBasic);
    return d->property.isEmpty();
}

template <typename T>
bool PropertyBasic<T>::isNameMatch(const char *otherName) const
{
    D_PTR(const PropertyBasic);
    return d->property.isNameMatch(otherName);
}

template <typename T>
bool PropertyBasic<T>::isNameMatch(const std::string &otherName) const
{
    D_PTR(const PropertyBasic);
    return d->property.isNameMatch(otherName);
}

template <typename T>
bool PropertyBasic<T>::isLabelMatch(const char *otherLabel) const
{
    D_PTR(const PropertyBasic);
    return d->property.isLabelMatch(otherLabel);
}

template <typename T>
bool PropertyBasic<T>::isLabelMatch(const std::string &otherLabel) const
{
    D_PTR(const PropertyBasic);
    return d->property.isLabelMatch(otherLabel);
}

template <typename T>
void PropertyBasic<T>::save(FILE *f) const
{
    D_PTR(const PropertyBasic);
    d->property.save(f);
}

template <typename T>
void PropertyBasic<T>::vapply(const char *format, va_list args) const
{
    D_PTR(const PropertyBasic);
    d->property.vapply(format, args);
}

template <typename T>
void PropertyBasic<T>::vdefine(const char *format, va_list args) const
{
    D_PTR(const PropertyBasic);
    d->property.vdefine(format, args);
}

template <typename T>
void PropertyBasic<T>::apply(const char *format, ...) const
{
    D_PTR(const PropertyBasic);
    va_list ap;
    va_start(ap, format);
    d->property.vapply(format, ap);
    va_end(ap);
}

template <typename T>
void PropertyBasic<T>::define(const char *format, ...) const
{
    D_PTR(const PropertyBasic);
    va_list ap;
    va_start(ap, format);
    d->property.vdefine(format, ap);
    va_end(ap);
}

template <typename T>
void PropertyBasic<T>::apply() const
{
    D_PTR(const PropertyBasic);
    d->property.apply();
}

template <typename T>
void PropertyBasic<T>::define() const
{
    D_PTR(const PropertyBasic);
    d->property.define();
}

template <typename T>
WidgetView<T> *PropertyBasic<T>::findWidgetByName(const char *name) const
{
    D_PTR(const PropertyBasic);
    return d->property.findWidgetByName(name);
}

template <typename T>
int PropertyBasic<T>::findWidgetIndexByName(const char *name) const
{
    auto it = findWidgetByName(name);
    return it == nullptr ? -1 : it - begin();
}

template <typename T>
size_t PropertyBasic<T>::size() const
{
    D_PTR(const PropertyBasic);
    return d->property.count();
}

template <typename T>
void PropertyBasic<T>::resize(size_t size)
{
    D_PTR(PropertyBasic);
    d->widgets.resize(size);
    d->property.setWidgets(d->widgets.data(), d->widgets.size());

}

template <typename T>
void PropertyBasic<T>::reserve(size_t size)
{
    D_PTR(PropertyBasic);
    d->widgets.reserve(size);
    d->property.setWidgets(d->widgets.data(), d->widgets.size());
}

template <typename T>
void PropertyBasic<T>::shrink_to_fit()
{
    D_PTR(PropertyBasic);
    d->widgets.shrink_to_fit();
    d->property.setWidgets(d->widgets.data(), d->widgets.size());    
}

template <typename T>
void PropertyBasic<T>::push(WidgetView<T> &&item)
{
    D_PTR(PropertyBasic);
    item.setParent(&d->property);
    d->widgets.push_back(std::move(item));
    d->property.setWidgets(d->widgets.data(), d->widgets.size());
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
    return d->property.at(index);
}

template <typename T>
WidgetView<T> &PropertyBasic<T>::operator[](size_t index) const
{
    D_PTR(const PropertyBasic);
    return *d->property.at(index);
}

template <typename T>
WidgetView<T> *PropertyBasic<T>::begin()
{
    D_PTR(PropertyBasic);
    return d->property.begin();
}

template <typename T>
WidgetView<T> *PropertyBasic<T>::end()
{
    D_PTR(PropertyBasic);
    return d->property.end();
}

template <typename T>
const WidgetView<T> *PropertyBasic<T>::begin() const
{
    D_PTR(const PropertyBasic);
    return d->property.begin();
}

template <typename T>
const WidgetView<T> *PropertyBasic<T>::end() const
{
    D_PTR(const PropertyBasic);
    return d->property.end();
}

template <typename T>
PropertyView<T> * PropertyBasic<T>::operator &()
{
    D_PTR(PropertyBasic);
    return &d->property;
}

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
