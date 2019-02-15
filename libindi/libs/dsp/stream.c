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

void dsp_stream_swap_buffers(dsp_stream_p stream)
{
    double* in = stream->in;
    stream->in = stream->out;
    stream->out = in;
}

double* dsp_stream_set_input_buffer_len(dsp_stream_p stream, int len)
{
    if(stream->in!=NULL) {
        stream->in = (double*)realloc(stream->in, sizeof(double) * len);
    } else {
        stream->in = (double*)malloc(sizeof(double) * len);
    }
    stream->len = len;
    return stream->in;
}

double* dsp_stream_set_output_buffer_len(dsp_stream_p stream, int len)
{
    if(stream->out!=NULL) {
        stream->out = (double*)realloc(stream->out, sizeof(double) * len);
    } else {
        stream->out = (double*)malloc(sizeof(double) * len);
    }
    stream->len = len;
    return stream->out;
}

double* dsp_stream_set_input_buffer(dsp_stream_p stream, void *buffer, int len)
{
    stream->in = (double*)buffer;
    stream->len = len;
    return stream->in;
}

double* dsp_stream_set_output_buffer(dsp_stream_p stream, void *buffer, int len)
{
    stream->out = (double*)buffer;
    stream->len = len;
    return stream->out;
}

double* dsp_stream_get_input_buffer(dsp_stream_p stream)
{
    return stream->in;
}

double* dsp_stream_get_output_buffer(dsp_stream_p stream)
{
    return stream->out;
}

void dsp_stream_free_input_buffer(dsp_stream_p stream)
{
    free(stream->in);
    stream->in = NULL;
}

void dsp_stream_free_output_buffer(dsp_stream_p stream)
{
    free(stream->out);
    stream->out = NULL;
}

dsp_stream_p dsp_stream_new()
{
    dsp_stream_p stream = (dsp_stream_p)calloc(sizeof(dsp_stream), 1);
    stream->out = (double*)calloc(sizeof(double), 1);
    stream->in = (double*)calloc(sizeof(double), 1);
    stream->sizes = (int*)calloc(sizeof(int), 1);
    stream->pos = (int*)calloc(sizeof(int), 1);
    stream->children = calloc(sizeof(dsp_stream_p), 1);
    stream->ROI = (dsp_region*)calloc(sizeof(dsp_region), 1);
    stream->child_count = 0;
    stream->parent = NULL;
    stream->dims = 0;
    stream->len = 1;
    stream->index = 0;
    stream->lambda = 0;
    stream->samplerate = 0;
    return stream;
}

dsp_stream_p dsp_stream_copy(dsp_stream_p stream)
{
    dsp_stream_p dest = dsp_stream_new();
    for(int i = 0; i < stream->dims; i++)
       dsp_stream_add_dim(dest, stream->sizes[i]);
    dest->lambda = stream->lambda;
    dest->samplerate = stream->samplerate;
    memcpy(dest->in, stream->in, sizeof(double) * stream->len);
    memcpy(dest->out, stream->out, sizeof(double) * stream->len);
    return dest;
}

void dsp_stream_add_dim(dsp_stream_p stream, int size)
{
    stream->sizes[stream->dims] = size;
    stream->len *= size;
    stream->in = (double*)realloc(stream->in, sizeof(double) * stream->len);
    stream->out = (double*)realloc(stream->out, sizeof(double) * stream->len);
    stream->dims ++;
    stream->ROI = (dsp_region*)realloc(stream->ROI, sizeof(dsp_region) * (stream->dims + 1));
    stream->sizes = (int*)realloc(stream->sizes, sizeof(int) * (stream->dims + 1));
    stream->pos = (int*)realloc(stream->pos, sizeof(int) * (stream->dims + 1));
}

void dsp_stream_del_dim(dsp_stream_p stream, int index)
{
    int* sizes = (int*)calloc(sizeof(int), stream->dims);
    memcpy(sizes, stream->sizes, sizeof(int) * stream->dims);
    free(stream->sizes);
    stream->dims = 0;
    for(int i = 0; i < stream->dims; i++) {
        if(i != index) {
            dsp_stream_add_dim(stream, sizes[i]);
        }
    }
}

void dsp_stream_add_child(dsp_stream_p stream, dsp_stream_p child)
{
    child->parent = stream;
    stream->children[stream->child_count] = child;
    stream->child_count++;
    stream->children = realloc(stream->children, sizeof(dsp_stream_p) * (stream->child_count + 1));
}

void dsp_stream_free(dsp_stream_p stream)
{
    free(stream->sizes);
    free(stream->pos);
    free(stream->children);
    free(stream);
}

dsp_stream_p dsp_stream_get_position(dsp_stream_p stream) {
    int dim = 0;
    int y = 0;
    int m = 1;
    for (dim = 0; dim < stream->dims; dim++) {
        y = stream->index / m;
        y %= stream->sizes[dim];
        m *= stream->sizes[dim];
        stream->pos[dim] = y;
    }
    return stream;
}

dsp_stream_p dsp_stream_set_position(dsp_stream_p stream) {
    int dim = 0;
    stream->index = 0;
    int m = 1;
    for (dim = 0; dim < stream->dims; dim++) {
        stream->index += m * stream->pos[dim];
        m *= stream->sizes[dim];
    }
    return stream;
}

void *dsp_stream_exec(dsp_stream_p stream) {
    return stream->func(stream);
}

void *dsp_stream_exec_multidim(dsp_stream_p stream) {
    if(stream->dims == 0)
        return NULL;
    for(int dim = 0; dim < stream->dims; dim++) {
        stream->arg = (void*)&dim;
        stream->func(stream);
    }
    return stream;
}

void dsp_stream_mul(dsp_stream_p in1, dsp_stream_p in2)
{
    int dims = Min(in1->dims, in2->dims);
    if(dims == 0)
        return;
    int len1 = 1;
    int len2 = 1;
    for(int dim = 0; dim < dims; dim++) {
        for(int x = 0, y = 0; x < in1->len && y < in2->len; x += len1, y += len2) {
            double i = in1->in[x] * in2->in[y];
            in1->out[x] = i;
            in2->out[y] = i;
        }
        len1 *= in1->sizes[dim];
        len2 *= in2->sizes[dim];
    }
}

void dsp_stream_sum(dsp_stream_p in1, dsp_stream_p in2)
{
    int dims = Min(in1->dims, in2->dims);
    if(dims == 0)
        return;
    int len1 = 1;
    int len2 = 1;
    for(int dim = 0; dim < dims; dim++) {
        for(int x = 0, y = 0; x < in1->sizes[dim] && y < in2->sizes[dim]; x += len1, y += len2) {
            double i = in1->in[x] + in2->in[y];
            in1->out[x] = i;
            in2->out[y] = i;
        }
        len1 = in1->sizes[dim];
        len2 = in2->sizes[dim];
    }
}

dsp_stream_p dsp_stream_crop(dsp_stream_p in)
{
    int dims = in->dims;
    if(dims == 0)
        return NULL;
    dsp_stream_p ret = dsp_stream_new();
    for(int dim = 0; dim < in->dims; dim++) {
        dsp_stream_add_dim(ret, in->ROI[dim].len);
    }
    int x = 0;
    for (in->index = 0; in->index<in->len; in->index++)
    {
        dsp_stream_get_position(in);
        for(int dim = 0; dim < in->dims; dim++) {
            if(in->pos[dim] >= in->ROI[dim].start &&  (in->pos[dim]-in->ROI[dim].start) < in->ROI[dim].len) {
                ret->in[x] = in->in[in->index];
                ret->out[x] = in->out[in->index];
                x++;
            }
        }
    }
    return ret;
}
