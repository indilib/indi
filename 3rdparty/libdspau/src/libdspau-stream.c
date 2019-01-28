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

void dspau_stream_swap_buffers(dspau_stream_p stream)
{
    double* in = stream->in;
    stream->in = stream->out;
    stream->out = in;
}

double* dspau_stream_set_input_buffer_len(dspau_stream_p stream, int len)
{
    if(stream->in!=NULL) {
        stream->in = (double*)realloc(stream->in, sizeof(double) * len);
    } else {
        stream->in = (double*)malloc(sizeof(double) * len);
    }
    stream->len = len;
    return stream->in;
}

double* dspau_stream_set_output_buffer_len(dspau_stream_p stream, int len)
{
    if(stream->out!=NULL) {
        stream->out = (double*)realloc(stream->out, sizeof(double) * len);
    } else {
        stream->out = (double*)malloc(sizeof(double) * len);
    }
    stream->len = len;
    return stream->out;
}

double* dspau_stream_set_input_buffer(dspau_stream_p stream, void *buffer, int len)
{
    stream->in = (double*)buffer;
    stream->len = len;
    return stream->in;
}

double* dspau_stream_set_output_buffer(dspau_stream_p stream, void *buffer, int len)
{
    stream->out = (double*)buffer;
    stream->len = len;
    return stream->out;
}

double* dspau_stream_get_input_buffer(dspau_stream_p stream)
{
    return stream->in;
}

double* dspau_stream_get_output_buffer(dspau_stream_p stream)
{
    return stream->out;
}

void dspau_stream_free_input_buffer(dspau_stream_p stream)
{
    free(stream->in);
    stream->in = NULL;
}

void dspau_stream_free_output_buffer(dspau_stream_p stream)
{
    free(stream->out);
    stream->out = NULL;
}

dspau_stream_p dspau_stream_new()
{
    dspau_stream_p stream = (dspau_stream_p)calloc(sizeof(dspau_stream), 1);
    stream->out = (double*)calloc(sizeof(double), 1);
    stream->in = (double*)calloc(sizeof(double), 1);
    stream->location = (double*)calloc(sizeof(double), 3);
    stream->target = (double*)calloc(sizeof(double), 3);
    stream->sizes = (int*)calloc(sizeof(int), 1);
    stream->pos = (int*)calloc(sizeof(int), 1);
    stream->children = calloc(sizeof(dspau_stream_p), 1);
    stream->child_count = 0;
    stream->parent = NULL;
    stream->dims = 0;
    stream->len = 1;
    stream->index = 0;
    stream->lambda = 0;
    stream->samplerate = 0;
    return stream;
}

dspau_stream_p dspau_stream_copy(dspau_stream_p stream)
{
    dspau_stream_p dest = dspau_stream_new();
    for(int i = 0; i < stream->dims; i++)
       dspau_stream_add_dim(dest, stream->sizes[i]);
    dest->lambda = stream->lambda;
    dest->samplerate = stream->samplerate;
    dest->starttimeutc.tv_nsec = stream->starttimeutc.tv_nsec;
    dest->starttimeutc.tv_sec = stream->starttimeutc.tv_sec;
    memcpy(dest->location, stream->location, sizeof(double) * 3);
    memcpy(dest->target, stream->target, sizeof(double) * 3);
    memcpy(dest->in, stream->in, sizeof(double) * stream->len);
    memcpy(dest->out, stream->out, sizeof(double) * stream->len);
    return dest;
}

void dspau_stream_add_dim(dspau_stream_p stream, int size)
{
    stream->sizes[stream->dims] = size;
    stream->dims ++;
    stream->sizes = (int*)realloc(stream->sizes, sizeof(int) * (stream->dims + 1));
    stream->pos = (int*)realloc(stream->pos, sizeof(int) * (stream->dims + 1));
    stream->len *= size;
    stream->in = (double*)realloc(stream->in, sizeof(double) * stream->len);
    stream->out = (double*)realloc(stream->out, sizeof(double) * stream->len);
}

void dspau_stream_add_child(dspau_stream_p stream, dspau_stream_p child)
{
    child->parent = stream;
    stream->children[stream->child_count] = child;
    stream->child_count++;
    stream->children = realloc(stream->children, sizeof(dspau_stream_p) * (stream->child_count + 1));
}

void dspau_stream_free(dspau_stream_p stream)
{/*
    free(stream->out);
    free(stream->in);*/
    free(stream->location);
    free(stream->target);
    free(stream->sizes);
    free(stream->pos);
    free(stream->children);
    free(stream);
}

int dspau_stream_byte_size(dspau_stream_p stream)
{
    int size = sizeof(*stream);
    return size;
}

dspau_stream_p dspau_stream_get_position(dspau_stream_p stream) {
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

dspau_stream_p dspau_stream_set_position(dspau_stream_p stream) {
    int dim = 0;
    stream->index = 0;
    int m = 1;
    for (dim = 0; dim < stream->dims; dim++) {
        stream->index += m * stream->pos[dim];
        m *= stream->sizes[dim];
    }
    return stream;
}

void *dspau_stream_exec(dspau_stream_p stream) {
    return stream->func(stream);
}

void *dspau_stream_exec_multidim(dspau_stream_p stream) {
    if(stream->dims == 0)
        return NULL;
    for(int dim = 0; dim < stream->dims; dim++) {
        stream->arg = (void*)&dim;
        stream->func(stream);
    }
    return stream;
}

void dspau_stream_mul(dspau_stream_p in1, dspau_stream_p in2)
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

void dspau_stream_sum(dspau_stream_p in1, dspau_stream_p in2)
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

dspau_stream_p dspau_stream_crop(dspau_stream_p in, dspau_region* rect)
{
    int dims = in->dims;
    if(dims == 0)
        return NULL;
    int len = 1;
    dspau_stream_p ret = dspau_stream_new();
    for(int dim = 0; dim < dims; dim++) {
        dspau_stream_add_dim(ret, rect[dim].len);
        for(int x = rect[dim].start, y = 0; x < rect[dim].len && x < in->sizes[dim]; x += len, y += rect[dim].len) {
            ret->in[y] = in->in[x];
            ret->out[y] = in->out[x];
        }
        len = in->sizes[dim];
    }
    return ret;
}
