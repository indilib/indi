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
    dspau_t* in = stream->in;
    stream->in = stream->out;
    stream->out = in;
}

dspau_t* dspau_stream_set_input_buffer_len(dspau_stream_p stream, int len)
{
    if(stream->in!=NULL) {
        stream->in = (dspau_t*)realloc(stream->in, sizeof(dspau_t) * len);
    } else {
        stream->in = (dspau_t*)calloc(sizeof(dspau_t), len);
    }
    return stream->in;
}

dspau_t* dspau_stream_set_output_buffer_len(dspau_stream_p stream, int len)
{
    if(stream->out!=NULL) {
        stream->out = (dspau_t*)realloc(stream->out, sizeof(dspau_t) * len);
    } else {
        stream->out = (dspau_t*)calloc(sizeof(dspau_t), len);
    }
    return stream->out;
}

dspau_t* dspau_stream_set_input_buffer(dspau_stream_p stream, void *buffer, int len)
{
    dspau_stream_free_input_buffer(stream);
    stream->in = (dspau_t*)buffer;
    stream->len = len;
    return stream->in;
}

dspau_t* dspau_stream_set_output_buffer(dspau_stream_p stream, void *buffer, int len)
{
    dspau_stream_free_output_buffer(stream);
    stream->out = (dspau_t*)buffer;
    stream->len = len;
    return stream->out;
}

dspau_t* dspau_stream_get_input_buffer(dspau_stream_p stream)
{
    return stream->in;
}

dspau_t* dspau_stream_get_output_buffer(dspau_stream_p stream)
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
    stream->out = (dspau_t*)calloc(sizeof(dspau_t), 1);
    stream->in = (dspau_t*)calloc(sizeof(dspau_t), 1);
    stream->sizes = (int*)calloc(sizeof(int), 1);
    stream->pos = (int*)calloc(sizeof(int), 1);
    stream->children = (dspau_stream_p*)calloc(sizeof(dspau_stream), 1);
    stream->child_count = 0;
    stream->parent = NULL;
    stream->dims = 0;
    stream->len = 1;
    stream->index = 0;
    return stream;
}

dspau_stream_p dspau_stream_copy(dspau_stream_p stream)
{
    dspau_stream_p dest = dspau_stream_new();
    for(int dim = 0; dim < stream->dims; dim++) {
        dspau_stream_add_dim(dest, stream->sizes[dim]);
    }
    memcpy(dest->in, stream->in, stream->len * sizeof(dspau_t));
    memcpy(dest->out, stream->out, stream->len * sizeof(dspau_t));
    return dest;
}

void dspau_stream_add_dim(dspau_stream_p stream, int size)
{
    stream->sizes[stream->dims] = size;
    stream->dims ++;
    stream->sizes = (int*)realloc(stream->sizes, sizeof(int) * (stream->dims + 1));
    stream->pos = (int*)realloc(stream->pos, sizeof(int) * (stream->dims + 1));
    stream->len *= size;
    dspau_stream_set_output_buffer_len(stream, stream->len);
    dspau_stream_set_input_buffer_len(stream, stream->len);
}

void dspau_stream_add_child(dspau_stream_p stream, dspau_stream_p child)
{
    child->parent = stream;
    dspau_stream_p* children = stream->children;
    children[stream->child_count] = child;
    stream->child_count++;
    stream->children = realloc(children, sizeof(dspau_stream) * (stream->child_count + 1));
}

void dspau_stream_free(dspau_stream_p stream)
{
    if(stream->in != NULL) {
        free(stream->sizes);
        stream->sizes = NULL;
    }
    if(stream->in != NULL) {
        free(stream->pos);
        stream->pos = NULL;
    }
    if(stream != NULL) {
        free(stream);
        stream = NULL;
    }
}

int dspau_stream_byte_size(dspau_stream_p stream)
{
    int size = sizeof(*stream);
    return size;
}

dspau_stream_p dspau_stream_position(dspau_stream_p stream) {
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

void *dspau_stream_exec(dspau_stream_p stream) {
    return stream->func(stream);
}
