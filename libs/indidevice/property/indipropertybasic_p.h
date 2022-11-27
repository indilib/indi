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

#pragma once

#include "indiproperty_p.h"
#include "indipropertyview.h"

#include <vector>
#include <functional>

#define INDI_PROPERTY_RAW_CAST

namespace INDI
{

template <typename T>
struct PropertyContainer
{
#ifndef INDI_PROPERTY_RAW_CAST
        PropertyView<T> typedProperty;
#else
        PropertyView<T> &typedProperty;
#endif
};
template <typename T>
class PropertyBasicPrivateTemplate: public PropertyContainer<T>, public PropertyPrivate
{
    public:
        using RawPropertyType = typename WidgetTraits<T>::PropertyType;
        using BasicPropertyType = PropertyBasicPrivateTemplate<T>;

    public:
        PropertyBasicPrivateTemplate(size_t count);
#ifdef INDI_PROPERTY_RAW_CAST
        PropertyBasicPrivateTemplate(RawPropertyType *rawProperty);
#endif
        virtual ~PropertyBasicPrivateTemplate();

    public:
#ifdef INDI_PROPERTY_RAW_CAST
        bool raw;
#endif
        std::vector<WidgetView<T>>  widgets;
};

}
