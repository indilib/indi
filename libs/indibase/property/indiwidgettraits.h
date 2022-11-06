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

#include "indiapi.h"
#include "indiutility.h"

namespace INDI
{

template <typename>
struct WidgetTraits;

template<> struct WidgetTraits<IText>
{
    using PropertyType = ITextVectorProperty;
    struct UpdateArgs
    {
        char **texts;
        char **names;
        int n;
    };
};

template<> struct WidgetTraits<INumber>
{
    using PropertyType = INumberVectorProperty;
    struct UpdateArgs
    {
        double *values;
        char **names;
        int n;
    };
};

template<> struct WidgetTraits<ISwitch>
{
    using PropertyType = ISwitchVectorProperty;
    struct UpdateArgs
    {
        ISState *states;
        char **names;
        int n;
    };
};

template<> struct WidgetTraits<ILight>
{
    using PropertyType = ILightVectorProperty;
    struct UpdateArgs
    {

    };
};

template<> struct WidgetTraits<IBLOB>
{
    using PropertyType = IBLOBVectorProperty;
    struct UpdateArgs
    {
        int *sizes;
        int *blobsizes;
        char **blobs;
        char **formats;
        char **names;
        int n;
    };
};

}
