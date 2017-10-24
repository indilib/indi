/*
    Copyright (C) 2017 by Jasem Mutlaq <mutlaqja@ikarustech.com>

    Pixel Color Space Info

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

namespace INDI
{

typedef enum
{
    INDI_MONO       = 0,
    INDI_BAYER_RGGB = 8,
    INDI_BAYER_GRBG = 9,
    INDI_BAYER_GBRG = 10,
    INDI_BAYER_BGGR = 11,
    INDI_BAYER_CYYM = 16,
    INDI_BAYER_YCMY = 17,
    INDI_BAYER_YMCY = 18,
    INDI_BAYER_MYYC = 19,
    INDI_RGB        = 100,
    INDI_BGR        = 101
} PixelFormat;

}
