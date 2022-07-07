/*
*   DSP API - a digital signal processing library for astronomy usage
*   Copyright © 2017-2022  Ilia Platone
*
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU Lesser General Public
*   License as published by the Free Software Foundation; either
*   version 3 of the License, or (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*   Lesser General Public License for more details.
*
*   You should have received a copy of the GNU Lesser General Public License
*   along with this program; if not, write to the Free Software Foundation,
*   Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "dsp.h"

double* dsp_stats_histogram(dsp_stream_p stream, int size)
{
    if(stream == NULL)
        return NULL;
    int k;
    long i = 0;
    double* out = (double*)malloc(sizeof(double)*size);
    dsp_t* tmp = (dsp_t*)malloc(sizeof(dsp_t)*stream->len);
    dsp_buffer_set(out, size, 0.0);
    memcpy(tmp, stream->buf, sizeof(dsp_t)*stream->len);
    dsp_buffer_stretch(tmp, stream->len, 0, size-1);
    for(k = 0; k < stream->len; k++) {
        i = (long)tmp[k];
        if(i > 0 && i < size)
            out[i] ++;
    }
    free(tmp);
    dsp_t mn = dsp_stats_min(out, size);
    dsp_t mx = dsp_stats_max(out, size);
    if(mn < mx)
        dsp_buffer_stretch(out, size, 0, size);
    return out;
}
