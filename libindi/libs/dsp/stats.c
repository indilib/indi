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

#include "dsp.h"

dsp_t* dsp_stats_histogram(dsp_stream_p stream, int size)
{
    int k;
    dsp_stream_p o = dsp_stream_copy(stream);
    dsp_t* out = (dsp_t*)malloc(sizeof(dsp_t) * size);
    long* i = (long*)malloc(sizeof(long) * o->len);
    dsp_buffer_normalize(o->buf, o->len, 0.0, size);
    dsp_buffer_copy(o->buf, i, o->len);
    dsp_buffer_copy(i, o->buf, o->len);
    for(k = 0; k < size; k++) {
        out[k] = dsp_stats_val_count(o->buf, o->len, k);
    }
    free(i);
    dsp_stream_free_buffer(o);
    dsp_stream_set_buffer(o, out, size);
    dsp_buffer_stretch(o->buf, o->len, 0, size);
    dsp_stream_free(o);
    return out;
}

