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

static void* dsp_convolution_convolution_th(void* arg)
{
    dsp_stream_p matrix = (dsp_stream_p)arg;
    dsp_stream_p field = (dsp_stream_p)matrix->parent;
    dsp_stream_p stream = (dsp_stream_p)field->parent;
    int y = matrix->ROI[0].start;
    int* pos = dsp_stream_get_position(matrix, y);
    int x, d;
    for(x = 0; x < stream->len; x++)
    {
        int* pos1 = dsp_stream_get_position(field, x);
        for(d = 0; d < field->dims; d++)
            pos1[d] += pos[d];
        int z = dsp_stream_set_position(field, pos1);
        if(z >= 0 && z < field->len && x >= 0 && x < stream->len && y >= 0)
            stream->buf[x] += field->buf[z] * matrix->buf[y];
        free(pos1);
    }
    free(pos);
    dsp_stream_free_buffer(matrix);
    dsp_stream_free(matrix);
    return NULL;
}

dsp_stream_p dsp_convolution_convolution(dsp_stream_p stream, dsp_stream_p object) {
    dsp_stream_p field = dsp_stream_copy(stream);
    dsp_stream_p tmp = dsp_stream_copy(stream);
    dsp_buffer_set(tmp->buf, tmp->len, 0);
    field->parent = tmp;
    int n_threads = object->len;
    pthread_t *th = malloc(sizeof(pthread_t)*n_threads);
    int y, xy;
    for(y = -n_threads; y < n_threads; y+=DSP_MAX_THREADS)
    {
        for(xy = 0; xy < DSP_MAX_THREADS; xy++) {
            dsp_stream_p matrix = dsp_stream_copy(object);
            matrix->parent = field;
            matrix->ROI[0].start = xy;
            matrix->ROI[0].len = 0;
            pthread_create(&th[xy], NULL, dsp_convolution_convolution_th, (void*)matrix);
        }
        for(xy = 0; xy < DSP_MAX_THREADS; xy++)
            pthread_join(th[xy], NULL);
    }
    dsp_stream_free_buffer(field);
    dsp_stream_free(field);
    return tmp;
}
