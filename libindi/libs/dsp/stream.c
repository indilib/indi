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

void dsp_stream_alloc_buffer(dsp_stream_p stream, int len)
{
    if(stream->buf!=NULL) {
        stream->buf = (double*)realloc(stream->buf, sizeof(double) * len);
    } else {
        stream->buf = (double*)malloc(sizeof(double) * len);
    }

}

void dsp_stream_set_buffer(dsp_stream_p stream, void *buffer, int len)
{
    stream->buf = (double*)buffer;
    stream->len = len;

}

double* dsp_stream_get_buffer(dsp_stream_p stream)
{
    return stream->buf;
}

void dsp_stream_free_buffer(dsp_stream_p stream)
{
    if(stream->buf == NULL)
        return;
    free(stream->buf);
    stream->buf = NULL;
}

dsp_stream_p dsp_stream_new()
{
    dsp_stream_p stream = (dsp_stream_p)calloc(sizeof(dsp_stream), 1);
    stream->buf = (double*)calloc(sizeof(double), 1);
    stream->sizes = (int*)calloc(sizeof(int), 1);
    stream->children = calloc(sizeof(dsp_stream_p), 1);
    stream->ROI = (dsp_region*)calloc(sizeof(dsp_region), 1);
    stream->child_count = 0;
    stream->parent = NULL;
    stream->dims = 0;
    stream->len = 1;
    stream->lambda = 0;
    stream->samplerate = 0;
    return stream;
}

void dsp_stream_free(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    free(stream->sizes);
    free(stream->children);
    free(stream);
}

dsp_stream_p dsp_stream_copy(dsp_stream_p stream)
{
    dsp_stream_p dest = dsp_stream_new();
    for(int i = 0; i < stream->dims; i++)
       dsp_stream_add_dim(dest, stream->sizes[i]);
    dsp_stream_alloc_buffer(dest, dest->len);
    dest->lambda = stream->lambda;
    dest->samplerate = stream->samplerate;
    memcpy(dest->buf, stream->buf, sizeof(double) * stream->len);
    return dest;
}

void dsp_stream_add_dim(dsp_stream_p stream, int size)
{
    stream->sizes[stream->dims] = size;
    stream->len *= size;
    stream->dims ++;
    stream->ROI = (dsp_region*)realloc(stream->ROI, sizeof(dsp_region) * (stream->dims + 1));
    stream->sizes = (int*)realloc(stream->sizes, sizeof(int) * (stream->dims + 1));
}

void dsp_stream_del_dim(dsp_stream_p stream, int index)
{
    int* sizes = (int*)calloc(sizeof(int), stream->dims);
    int dims = stream->dims;
    memcpy(sizes, stream->sizes, sizeof(int) * stream->dims);
    free(stream->sizes);
    stream->dims = 0;
    for(int i = 0; i < dims; i++) {
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

void dsp_stream_del_child(dsp_stream_p stream, int index)
{
    dsp_stream_p* children = (dsp_stream_p*)calloc(sizeof(dsp_stream_p), stream->child_count);
    int child_count = stream->child_count;
    memcpy(children, stream->children, sizeof(dsp_stream_p*) * stream->child_count);
    free(stream->children);
    stream->child_count = 0;
    for(int i = 0; i < child_count; i++) {
        if(i != index) {
            dsp_stream_add_child(stream, children[i]);
        }
    }
}

int* dsp_stream_get_position(dsp_stream_p stream, int index) {
    int dim = 0;
    int y = 0;
    int m = 1;
    int* pos = (int*)malloc(sizeof(int) * stream->dims);
    for (dim = 0; dim < stream->dims; dim++) {
        y = index / m;
        y %= stream->sizes[dim];
        m *= stream->sizes[dim];
        pos[dim] = y;
    }
    return pos;
}

int dsp_stream_set_position(dsp_stream_p stream, int* pos) {
    int dim = 0;
    int index = 0;
    int m = 1;
    for (dim = 0; dim < stream->dims; dim++) {
        index += m * pos[dim];
        m *= stream->sizes[dim];
    }
    return index;
}

void *dsp_stream_exec(dsp_stream_p stream) {
    return stream->func(stream);
}

void dsp_stream_exec_multidim(dsp_stream_p stream) {
    if(stream->dims == 0)
        return;
    for(int dim = 0; dim < stream->dims; dim++) {
        stream->arg = (void*)&dim;
        stream->func(stream);
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
    dsp_stream_alloc_buffer(ret, ret->len);
    int x = 0;
    for (int index = 0; index<in->len; index++)
    {
        int* pos = dsp_stream_get_position(in, index);
        for(int dim = 0; dim < in->dims; dim++) {
            if(pos[dim] >= in->ROI[dim].start &&  (pos[dim]-in->ROI[dim].start) < in->ROI[dim].len) {
                ret->buf[x] = in->buf[index];
                x++;
            }
        }
        free(pos);
    }
    return ret;
}
