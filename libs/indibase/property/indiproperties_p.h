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

#include "indiproperties.h"

namespace INDI
{

#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
template <typename T>
static inline std::shared_ptr<T> make_shared_weak(T *object)
{
    return std::shared_ptr<T>(object, [](T*) {});
}
#endif

class PropertiesPrivate
{
    public:
        PropertiesPrivate();
        virtual ~PropertiesPrivate();

    public:
        std::deque<INDI::Property> properties;
#ifdef INDI_PROPERTIES_BACKWARD_COMPATIBILE
        mutable std::vector<INDI::Property *> propertiesBC;
        Properties self {make_shared_weak(this)};
#endif
};

}
