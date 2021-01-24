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
    if(stream->buf != NULL) {
        stream->buf = (dsp_t*)realloc(stream->buf, sizeof(dsp_t) * len);
    } else {
        stream->buf = (dsp_t*)malloc(sizeof(dsp_t) * len);
    }
}

void dsp_stream_set_buffer(dsp_stream_p stream, void *buffer, int len)
{
    stream->buf = (dsp_t*)buffer;
    stream->len = len;
}

dsp_t* dsp_stream_get_buffer(dsp_stream_p stream)
{
    return stream->buf;
}

void dsp_stream_free_buffer(dsp_stream_p stream)
{
    if(stream->buf == NULL)
        return;
    free(stream->buf);
}

dsp_stream_p dsp_stream_new()
{
    dsp_stream_p stream = (dsp_stream_p)malloc(sizeof(dsp_stream) * 1);
    stream->buf = (dsp_t*)malloc(sizeof(dsp_t) * 1);
    stream->sizes = (int*)malloc(sizeof(int) * 1);
    stream->pixel_sizes = (double*)malloc(sizeof(double) * 1);
    stream->children = (dsp_stream_p*)malloc(sizeof(dsp_stream_p) * 1);
    stream->ROI = (dsp_region*)malloc(sizeof(dsp_region) * 1);
    stream->location = (double*)malloc(sizeof(double) * 3);
    stream->target = (double*)malloc(sizeof(double) * 3);
    stream->stars = (dsp_star*)malloc(sizeof(dsp_star) * 1);
    stream->align_info.offset = (double*)malloc(sizeof(double)*1);
    stream->align_info.center = (double*)malloc(sizeof(double)*1);
    stream->align_info.radians = (double*)malloc(sizeof(double)*1);
    stream->stars_count = 0;
    stream->child_count = 0;
    stream->frame_number = 0;
    stream->parent = NULL;
    stream->dims = 0;
    stream->red = -1;
    stream->len = 1;
    stream->wavelength = 0;
    stream->diameter = 1;
    stream->focal_ratio = 1;
    stream->samplerate = 0;
    return stream;
}

void dsp_stream_free(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    free(stream->sizes);
    free(stream->pixel_sizes);
    free(stream->children);
    free(stream->ROI);
    free(stream->location);
    free(stream->target);
    free(stream);
}

dsp_stream_p dsp_stream_copy(dsp_stream_p stream)
{
    dsp_stream_p dest = dsp_stream_new();
    int i;
    for(i = 0; i < stream->dims; i++)
       dsp_stream_add_dim(dest, abs(stream->sizes[i]));
    dsp_stream_alloc_buffer(dest, dest->len);
    dest->wavelength = stream->wavelength;
    dest->samplerate = stream->samplerate;
    dest->stars_count = stream->stars_count;
    dest->diameter = stream->diameter;
    dest->focal_ratio = stream->focal_ratio;
    dest->starttimeutc = stream->starttimeutc;
    dest->align_info = stream->align_info;
    memcpy(dest->ROI, stream->ROI, sizeof(dsp_region) * stream->dims);
    memcpy(dest->pixel_sizes, stream->pixel_sizes, sizeof(double) * stream->dims);
    memcpy(dest->target, stream->target, sizeof(double) * 3);
    memcpy(dest->location, stream->location, sizeof(double) * 3);
    memcpy(dest->buf, stream->buf, sizeof(dsp_t) * stream->len);
    return dest;
}

void dsp_stream_add_dim(dsp_stream_p stream, int size)
{
    stream->sizes[stream->dims] = size;
    stream->len *= size;
    stream->dims ++;
    stream->ROI = (dsp_region*)realloc(stream->ROI, sizeof(dsp_region) * (stream->dims + 1));
    stream->sizes = (int*)realloc(stream->sizes, sizeof(int) * (stream->dims + 1));
    stream->pixel_sizes = (double*)realloc(stream->pixel_sizes, sizeof(double) * (stream->dims + 1));
    stream->align_info.dims = stream->dims;
    stream->align_info.offset = (double*)realloc(stream->align_info.offset, sizeof(double)*stream->dims);
    stream->align_info.center = (double*)realloc(stream->align_info.center, sizeof(double)*stream->dims);
    stream->align_info.radians = (double*)realloc(stream->align_info.radians, sizeof(double)*(stream->dims-1));
    stream->align_info.factor = 0.0;
}

void dsp_stream_del_dim(dsp_stream_p stream, int index)
{
    int* sizes = (int*)malloc(sizeof(int) * stream->dims);
    int dims = stream->dims;
    memcpy(sizes, stream->sizes, sizeof(int) * stream->dims);
    free(stream->sizes);
    stream->dims = 0;
    int i;
    for(i = 0; i < dims; i++) {
        if(i != index) {
            dsp_stream_add_dim(stream, abs(sizes[i]));
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
    dsp_stream_p* children = (dsp_stream_p*)malloc(sizeof(dsp_stream_p) * stream->child_count);
    int child_count = stream->child_count;
    memcpy(children, stream->children, sizeof(dsp_stream_p*) * stream->child_count);
    free(stream->children);
    stream->child_count = 0;
    int i;
    for(i = 0; i < child_count; i++) {
        if(i != index) {
            dsp_stream_add_child(stream, children[i]);
        }
    }
}

void dsp_stream_add_star(dsp_stream_p stream, dsp_star star)
{
    stream->stars = (dsp_star*)realloc(stream->stars, sizeof(dsp_star)*(stream->stars_count+1));
    stream->stars[stream->stars_count].center.dims = star.center.dims;
    stream->stars[stream->stars_count].center.location = star.center.location;
    stream->stars[stream->stars_count].diameter = star.diameter;
    stream->stars_count++;
}

void dsp_stream_del_star(dsp_stream_p stream, int index)
{
    dsp_star* stars = (dsp_star*)malloc(sizeof(dsp_star) * stream->stars_count);
    int stars_count = stream->stars_count;
    memcpy(stars, stream->stars, sizeof(dsp_star*) * stream->stars_count);
    free(stream->stars);
    stream->stars_count = 0;
    int i;
    for(i = 0; i < stars_count; i++) {
        if(i != index) {
            dsp_stream_add_star(stream, stars[i]);
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

void dsp_stream_crop(dsp_stream_p in)
{
    dsp_stream_p stream = dsp_stream_new();
    int dim, d;
    for(dim = 0; dim < in->dims; dim++) {
        dsp_stream_add_dim(stream, abs(in->ROI[dim].len));
    }
    dsp_stream_alloc_buffer(stream, stream->len);
    int *init = dsp_stream_get_position(in, 0);
    int *stop = dsp_stream_get_position(in, 0);
    for(d = 0; d < in->dims; d++) {
        init[d] = in->ROI[d].start;
        stop[d] = in->ROI[d].start+in->ROI[d].len;
    }
    int index = dsp_stream_set_position(in, init);
    int end = dsp_stream_set_position(in, stop);
    free(init);
    free(stop);
    int x = 0;
    for (; index<end; index++)
    {
        int breakout = 0;
        int *pos = dsp_stream_get_position(in, index);
        for(dim = 0; dim < in->dims; dim++) {
            if(pos[dim]>in->ROI[dim].start&&pos[dim]<in->ROI[dim].start+in->ROI[dim].len)break;
            breakout++;
        }
        if(breakout > 0)continue;
        stream->buf[x++] = in->buf[index];
        free(pos);
    }
    dsp_buffer_copy(stream->sizes, in->sizes, stream->dims);
    in->len = stream->len;
    dsp_stream_alloc_buffer(in, in->len);
    dsp_buffer_copy(stream->buf, in->buf, stream->len);
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);
}

void dsp_stream_traslate(dsp_stream_p in)
{
    dsp_stream_p stream = dsp_stream_copy(in);
    int* offset = (int*)malloc(sizeof(int)*stream->dims);
    dsp_buffer_copy(stream->align_info.offset, offset, stream->dims);
    int z = dsp_stream_set_position(stream, offset);
    free(offset);
    int k = -z;
    z = Max(0, z);
    k = Max(0, k);
    int len = stream->len-z-k;
    dsp_t *buf = &stream->buf[k];
    dsp_t *data = &in->buf[z];
    memset(in->buf, 0, sizeof(dsp_t)*in->len);
    memcpy(data, buf, sizeof(dsp_t)*len);
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);
}

static void* dsp_stream_scale_th(void* arg)
{
    struct {
        int cur_th;
        dsp_stream_p stream;
     } *arguments = arg;
    dsp_stream_p stream = arguments->stream;
    dsp_stream_p in = stream->parent;
    int cur_th = arguments->cur_th;
    int start = cur_th * stream->len / DSP_MAX_THREADS;
    int end = start + stream->len / DSP_MAX_THREADS;
    end = Min(stream->len, end);
    int y, d;
    for(y = start; y < end; y++)
    {
        int* pos = dsp_stream_get_position(stream, y);
        for(d = 0; d < stream->dims; d++) {
            pos[d] -= stream->sizes[d];
            pos[d] /= stream->align_info.factor;
            pos[d] += stream->sizes[d];
        }
        int x = dsp_stream_set_position(in, pos);
        if(x >= 0 && x < in->len)
            stream->buf[y] += in->buf[x]/(stream->align_info.factor*stream->dims);
        free(pos);
    }
    return NULL;
}

void dsp_stream_scale(dsp_stream_p in)
{
    int y;
    dsp_stream_p stream = dsp_stream_copy(in);
    dsp_buffer_set(stream->buf, stream->len, 0);
    stream->parent = in;
    pthread_t *th = malloc(sizeof(pthread_t)*DSP_MAX_THREADS);
    struct {
       int cur_th;
       dsp_stream_p stream;
    } thread_arguments[DSP_MAX_THREADS];
    for(y = 0; y < DSP_MAX_THREADS; y++)
    {
        thread_arguments[y].cur_th = y;
        thread_arguments[y].stream = stream;
        pthread_create(&th[y], NULL, dsp_stream_scale_th, &thread_arguments[y]);
    }
    for(y = 0; y < DSP_MAX_THREADS; y++)
        pthread_join(th[y], NULL);
    free(th);
    stream->parent = NULL;
    dsp_buffer_copy(stream->buf, in->buf, stream->len);
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);
}

static void* dsp_stream_rotate_th(void* arg)
{
    struct {
       int cur_th;
       dsp_stream_p stream;
    } *arguments = arg;
    dsp_stream_p stream = arguments->stream;
    dsp_stream_p in = stream->parent;
    int cur_th = arguments->cur_th;
    int start = cur_th * stream->len / DSP_MAX_THREADS;
    int end = start + stream->len / DSP_MAX_THREADS;
    end = Min(stream->len, end);
    int y;
    for(y = start; y < end; y++)
    {
        int *pos = dsp_stream_get_position(stream, y);
        int dim;
        for (dim = 1; dim < stream->dims; dim++) {
            pos[dim] -= stream->align_info.center[dim];
            pos[dim-1] -= stream->align_info.center[dim-1];
            double r = stream->align_info.radians[dim-1];
            double x = pos[dim-1];
            double y = pos[dim];
            pos[dim] = x*-cos(r-M_PI_2);
            pos[dim-1] = x*cos(r);
            pos[dim] += y*cos(r);
            pos[dim] += stream->align_info.center[dim];
            pos[dim-1] += stream->align_info.center[dim-1];
            pos[dim-1] += y*cos(r-M_PI_2);
        }
        int x = dsp_stream_set_position(in, pos);
        free(pos);
        if(x >= 0 && x < in->len)
            stream->buf[y] = in->buf[x];
    }
    return NULL;
}

void dsp_stream_rotate(dsp_stream_p in)
{
    dsp_stream_p stream = dsp_stream_copy(in);
    dsp_buffer_set(stream->buf, stream->len, 0);
    stream->parent = in;
    int y;
    pthread_t *th = malloc(sizeof(pthread_t)*DSP_MAX_THREADS);
    struct {
       int cur_th;
       dsp_stream_p stream;
    } thread_arguments[DSP_MAX_THREADS];
    for(y = 0; y < DSP_MAX_THREADS; y++) {
        thread_arguments[y].cur_th = y;
        thread_arguments[y].stream = stream;
        pthread_create(&th[y], NULL, dsp_stream_rotate_th, &thread_arguments[y]);
    }
    for(y = 0; y < DSP_MAX_THREADS; y++)
        pthread_join(th[y], NULL);
    free(th);
    dsp_buffer_copy(stream->buf, in->buf, stream->len);
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);
}
