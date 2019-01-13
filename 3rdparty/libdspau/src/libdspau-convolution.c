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

#include "libdspau.h"

double* dspau_convolution_convolution(dspau_stream_p stream1, dspau_stream_p stream2) {
    dspau_stream_p dst = dspau_stream_copy(stream1);
    dspau_stream_p src = dspau_stream_copy(stream2);
    for(src->index = 0; src->index < src->len; src->index++) {
        dspau_stream_get_position(src);
        memset(dst->pos, 0, sizeof(int) * dst->dims);
        memcpy(dst->pos, src->pos, sizeof(int) * Min(src->dims, dst->dims));
        dspau_stream_set_position(dst);
        int len = dst->len - dst->index;
        double* mul = dspau_buffer_mul1(dst->in, dst->len, src->in[src->index]);
        double* sum = dspau_buffer_sum(dst->out, len, &mul[dst->index], len);
        memcpy(dst->out, sum, sizeof(double) * len);
        free(sum);
        free(mul);
    }
    return dspau_buffer_div1(dst->out, dst->len, dst->len);
}
