/*
*   DSP API - a digital signal processing library for astronomy usage
*   Copyright © 2017-2021  Ilia Platone
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

void dsp_feature_align(dsp_stream_p stream, dsp_stream_p matrix)
{
    int z;
    dsp_fourier_dft(matrix, 1);
    for(z = 0; z < matrix->sizes[stream->dims]; z++) {
        dsp_fourier_dft(stream, 1);
        int x, y, d;
        dsp_t mn = dsp_stats_min(stream->buf, stream->len);
        dsp_t mx = dsp_stats_max(stream->buf, stream->len);
        int* d_pos = (int*)malloc(sizeof(int)*stream->dims);
        for(y = z*stream->len; y < z*stream->len+stream->len; y++) {
            int* pos = dsp_stream_get_position(matrix, y);
            for(d = 0; d < stream->dims; d++) {
                d_pos[d] = stream->sizes[d]/2+pos[d]-matrix->sizes[d]/2;
            }
            x = dsp_stream_set_position(stream, d_pos);
            free(pos);
            stream->magnitude->buf[x] *= sqrt(matrix->magnitude->buf[y]);
        }
        free(d_pos);
        dsp_fourier_idft(stream);
        dsp_buffer_stretch(stream->buf, stream->len, mn, mx);
    }
}
