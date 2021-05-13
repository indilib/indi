/*******************************************************************************
  Copyright(c) 2011 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indibase.h"
#include "indiproperty.h"
#include <memory>
namespace INDI
{

#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
template <typename T>
static inline std::shared_ptr<T> make_shared_weak(T *object)
{
    return std::shared_ptr<T>(object, [](T*){});
}
#endif

class BaseDevice;
class PropertyPrivate
{
public:
    void *property = nullptr;
    BaseDevice *baseDevice = nullptr;
    INDI_PROPERTY_TYPE type = INDI_UNKNOWN;
    bool registered = false;
    bool dynamic = false;

    PropertyPrivate(void *property, INDI_PROPERTY_TYPE type);
    PropertyPrivate(ITextVectorProperty *property);
    PropertyPrivate(INumberVectorProperty *property);
    PropertyPrivate(ISwitchVectorProperty *property);
    PropertyPrivate(ILightVectorProperty *property);
    PropertyPrivate(IBLOBVectorProperty *property);

    virtual ~PropertyPrivate();

#ifdef INDI_PROPERTY_BACKWARD_COMPATIBILE
    Property self {make_shared_weak(this)};
#endif
};

}
