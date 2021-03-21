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

#include "indipropertybasic.h"

namespace INDI
{

class PropertyLightPrivate;
class PropertyLight: public INDI::PropertyBasic<ILight>
{
    DECLARE_PRIVATE(PropertyLight)
public:
    PropertyLight(size_t count);
    ~PropertyLight();

public:
    // bool update(..., const char * const names[], int n);

    void fill(
        const char *device, const char *name, const char *label, const char *group,
        IPState state
    );
};

}
