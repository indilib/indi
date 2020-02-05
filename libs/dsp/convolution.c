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

dsp_stream_p dsp_convolution_convolution(dsp_stream_p stream, dsp_stream_p matrix) {
    dsp_stream_p tmp = dsp_stream_copy(stream);
    dsp_buffer_clear(tmp);
    for(int y = 0; y < matrix->len; y++)
    {
        int* pos = dsp_stream_get_position(matrix, y);
        int z = dsp_stream_set_position(stream, pos);
        for(int x = -matrix->len + 1; x < stream->len+matrix->len; x++)
        {
            int i = x+z;
            if(i >= 0 && i < stream->len && x >= 0 && x < stream->len)
                tmp->buf[x] += stream->buf[i] * matrix->buf[y];
        }
    }
    return tmp;
}
