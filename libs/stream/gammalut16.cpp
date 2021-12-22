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
#include "gammalut16.h"
#include <cmath>

GammaLut16::GammaLut16(double gamma, double a, double b, double Ii)
{
    mLookUpTable.resize(65536);
    
    unsigned int i = 0;
    for (auto &value: mLookUpTable)
    {
        double I = static_cast<double>(i++) / 65535.0;
        double p;
        if (I <= Ii)
            p = a * I;
        else
            p = (1 + b) * powf(I, 1.0 / gamma) - b;
        value = round(255.0 * p);
    }
}

void GammaLut16::apply(const uint16_t *source, size_t count, uint8_t *destination) const
{
    apply(source, source + count, destination);
}


void GammaLut16::apply(const uint16_t *first, const uint16_t *last, uint8_t *destination) const
{
    const uint16_t *lookUpTable = mLookUpTable.data();

    while (first != last)
        *destination++ = lookUpTable[*first++];
}
