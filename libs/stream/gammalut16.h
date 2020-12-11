/*
    Copyright (C) 2020 by Pawel Soja <kernel32.pl@gmail.com>
    Copyright (C) 2015 by Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2014 by geehalel <geehalel@gmail.com>

    Stream Recorder

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

#include <vector>
#include <cstdint>
#include <cstddef>

class GammaLut16
{
public:
    GammaLut16(double gamma = 2.4, double a = 12.92, double b = 0.055, double Ii = 0.00304);

public:
    void apply(const uint16_t *source, size_t count, uint8_t *destination) const;
    void apply(const uint16_t *first, const uint16_t *last, uint8_t *destination) const;

protected:
    std::vector<uint16_t> mLookUpTable;
};
