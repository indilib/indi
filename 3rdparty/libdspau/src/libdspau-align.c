/*
 *   libDSPAU - a digital signal processing library for astronomy usage
 *   Copyright (C) 2017  Ilia Platone <info@iliaplatone.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libdspau.h"
#define ratio (max - min) / (mx - mn + 1)

dspau_star dspau_align_findstar(dspau_t* in, dspau_rectangle tmpRect, int intensity, int width, int height)
{
    dspau_star ret;
    int left = Max(0, Min(tmpRect.x, tmpRect.x + tmpRect.width));
    int top = Max(0, Min(tmpRect.y, tmpRect.y + tmpRect.height));
    int right = Min(tmpRect.width, Max(tmpRect.x, tmpRect.x + tmpRect.width));
    int bottom = Min(tmpRect.height, Max(tmpRect.y, tmpRect.y + tmpRect.height));
    int len = (right - left) * (bottom - top);
    dspau_t* tmpBuf = (dspau_t*)calloc(sizeof(dspau_t), len);
    if (len <= 0)
        goto lost;
    int p = 0;
    for (int y = tmpRect.y; y < tmpRect.y + tmpRect.height && y < height; y++)
    {
        for (int x = tmpRect.x; x < tmpRect.x + tmpRect.width && x < width; x++)
        {
            tmpBuf[p++] = (dspau_t)(in[x + y * width]);
        }
    }
    tmpBuf = dspau_buffer_stretch(tmpBuf, len, 0.0, 100.0);
    width = right - left;
    height = bottom - top;
    right -= left;
    bottom -= top;
    left = 0;
    top = 0;
    int found = 0;
    for (int y = height - 1; y >= 0 && !found; y--)
    {
        for (int x = width - 1; x >= 0 && !found; x --)
        {
            if (tmpBuf[x + y * width] > intensity)
            {
                bottom = y;
                found = 1;
            }
        }
    }
    if (!found)
        goto lost;
    found = 0;
    for (int x = width - 1; x >= 0 && !found; x--)
    {
        for (int y = height - 1; y >= 0 && !found; y--)
        {
            if (tmpBuf[x + y * width] > intensity)
            {
                right = x;
                found = 1;
            }
        }
    }
    if (!found)
        goto lost;
    found = 0;
    for (int y = 0; y < height && !found; y++)
    {
        for (int x = 0; x < width && !found; x += 3)
        {
            if (tmpBuf[x + y * width] > intensity)
            {
                top = y;
                found = 1;
            }
        }
    }
    if (!found)
        goto lost;
    found = 0;
    for (int x = 0; x < right && !found; x += 3)
    {
        for (int y = 0; y < height && !found; y++)
        {
            if (tmpBuf[x + y * width] > intensity)
            {
                left = x;
                found = 1;
            }
        }
    }
    if (!found)
        goto lost;
    ret = (dspau_star) {
        .center = (dspau_point) {
            .x = tmpRect.x + (right - left) / 2 + left,
            .y = tmpRect.y + (bottom - top) / 2 + top,
        },
        .radius = Min(right - left, bottom - top) / 2,
    };
    return ret;
lost:
    ret = (dspau_star) {
        .center = (dspau_point) {
           tmpRect.x,
           tmpRect.y,
        },
        .radius = 0,
    };
    return ret;
}
