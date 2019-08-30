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

dsp_stream_p dsp_find_object(dsp_stream_p stream, dsp_stream_p object, int steps) {
    dsp_stream_p tmp = dsp_stream_copy(object);
    dsp_stream_p rotated = dsp_stream_copy(object);
    dsp_buffer_reverse(tmp->buf, tmp->len);
    double *center = (double*)malloc(sizeof(double)*object->dims);
    double *rotation = (double*)malloc(sizeof(double)*object->dims);
    for(int dim = 0; dim < object->dims; dim++) {
        center[dim] = object->sizes[dim]/2;
    }
    for(double x = 0; x < steps; x++) {
        dsp_stream_scale(tmp, x/steps);
        double angle = 0;
        while (angle < pow(M_PI*2, object->dims)) {
            for(int dim = 0; dim < object->dims; dim++) {
                angle += M_PI*2/steps;
                rotation[dim] += M_PI*2/steps;
                rotated = dsp_stream_rotate(object, rotation, center);
                dsp_stream_p tmp0 = dsp_convolution_convolution(tmp, rotated);
                dsp_buffer_sum(tmp, tmp0->buf, tmp0->len);
                dsp_stream_free(tmp0);
            }
        }
    }
    return tmp;
}
