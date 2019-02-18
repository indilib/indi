/*
 *   libDSPAU - a digital signal processing library for astronoms usage
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

void dsp_convolution_convolution(dsp_stream_p dst, dsp_stream_p src) {
    double *out = calloc(sizeof(double), dst->len);
    double mn, mx;
    dsp_stats_minmidmax(dst, &mn, &mx);
    for(int y = 0; y < src->len; y++) {
        for(int x = 0; x < dst->len; x++) {
            out[x] += dst->buf[x] * src->buf[y];
        }
    }
    dsp_stream_free_buffer(dst);
    dsp_stream_set_buffer(dst, out, dst->len);
    dsp_buffer_stretch(dst, mn, mx);
}
