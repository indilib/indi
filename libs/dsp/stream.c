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
    stream->buf = NULL;
}

dsp_stream_p dsp_stream_new()
{
    dsp_stream_p stream = (dsp_stream_p)malloc(sizeof(dsp_stream) * 1);
    stream->buf = (dsp_t*)malloc(sizeof(dsp_t) * 1);
    stream->sizes = (int*)malloc(sizeof(int) * 1);
    stream->pixel_sizes = (double*)malloc(sizeof(double) * 1);
    stream->children = malloc(sizeof(dsp_stream_p) * 1);
    stream->ROI = (dsp_region*)malloc(sizeof(dsp_region) * 1);
    stream->location = (double*)malloc(sizeof(double) * 3);
    stream->target = (double*)malloc(sizeof(double) * 3);
    stream->stars = (dsp_star*)malloc(sizeof(dsp_star) * 1);
    stream->stars_count = 0;
    stream->child_count = 0;
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
    free(stream);
}

dsp_stream_p dsp_stream_copy(dsp_stream_p stream)
{
    dsp_stream_p dest = dsp_stream_new();
    int i;
    for(i = 0; i < stream->dims; i++)
       dsp_stream_add_dim(dest, stream->sizes[i]);
    dsp_stream_alloc_buffer(dest, dest->len);
    dest->wavelength = stream->wavelength;
    dest->samplerate = stream->samplerate;
    dest->stars_count = stream->stars_count;
    dest->diameter = stream->diameter;
    dest->focal_ratio = stream->focal_ratio;
    memcpy(&dest->starttimeutc, &stream->starttimeutc, sizeof(struct timespec));
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

dsp_stream_p dsp_stream_crop(dsp_stream_p in)
{
    start_gettime;
    if(in->dims <= 0)
        return NULL;
    dsp_stream_p ret = dsp_stream_new();
    int dim, d;
    for(dim = 0; dim < in->dims; dim++) {
        if(in->ROI[dim].len+in->ROI[dim].start < 1 || in->ROI[dim].start >= in->sizes[dim]-1) {
            dsp_stream_free(ret);
            return NULL;
        }
        dsp_stream_add_dim(ret, in->ROI[dim].len);
    }
    dsp_stream_alloc_buffer(ret, ret->len);
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
        ret->buf[x++] = in->buf[index];
        free(pos);
    }
    end_gettime;
    return ret;
}

static void* dsp_stream_scale_th(void* arg)
{
    struct {
        int cur_th;
        dsp_align_info *info;
        dsp_stream_p stream;
     } *arguments = arg;
    dsp_stream_p stream = arguments->stream;
    dsp_stream_p in = stream->parent;
    int cur_th = arguments->cur_th;
    dsp_align_info *info = arguments->info;
    int start = cur_th * stream->len / MAX_THREADS;
    int end = Min(stream->len, start + stream->len / MAX_THREADS);
    int y, d;
    for(y = start; y < end; y++)
    {
        int* pos = dsp_stream_get_position(stream, y);
        for(d = 0; d < stream->dims; d++) {
            pos[d] -= stream->sizes[d];
            pos[d] /= info->factor;
            pos[d] += stream->sizes[d];
        }
        int x = dsp_stream_set_position(in, pos);
        if(x >= 0 && x < in->len)
            stream->buf[y] = in->buf[x];
        free(pos);
    }
    return NULL;
}

dsp_stream_p dsp_stream_scale(dsp_stream_p in, dsp_align_info *info)
{
    start_gettime;
    int dims = in->dims;
    if(dims == 0)
        return NULL;
    int y;
    dsp_stream_p stream = dsp_stream_copy(in);
    dsp_buffer_set(stream, 0);
    stream->parent = in;
    pthread_t *th = malloc(sizeof(pthread_t)*MAX_THREADS);
    struct {
       int cur_th;
       dsp_align_info *info;
       dsp_stream_p stream;
    } thread_arguments[MAX_THREADS];
    for(y = 0; y < MAX_THREADS; y++)
    {
        thread_arguments[y].cur_th = y;
        thread_arguments[y].info = info;
        thread_arguments[y].stream = stream;
        pthread_create(&th[y], NULL, dsp_stream_scale_th, &thread_arguments[y]);
    }
    for(y = 0; y < MAX_THREADS; y++)
        pthread_join(th[y], NULL);
    free(th);
    end_gettime;
    return stream;
}

static void* dsp_stream_rotate_th(void* arg)
{
    struct {
       int cur_th;
       dsp_align_info *info;
       dsp_stream_p stream;
    } *arguments = arg;
    dsp_stream_p stream = arguments->stream;
    dsp_stream_p in = stream->parent;
    int cur_th = arguments->cur_th;
    dsp_align_info *info = arguments->info;
    int start = cur_th * stream->len / MAX_THREADS;
    int end = Min(stream->len, start + stream->len / MAX_THREADS);
    int y;
    for(y = start; y < end; y++)
    {
        int *pos = dsp_stream_get_position(stream, y);
        int dim;
        for (dim = 1; dim < stream->dims; dim++) {
            pos[dim] -= info->center[dim];
            pos[dim-1] -= info->center[dim-1];
            double r = info->radians[dim-1];
            double x = pos[dim-1];
            double y = pos[dim];
            pos[dim-1] = x*cos(r);
            pos[dim-1] += y*cos(r-M_PI_2);
            pos[dim] = x*-cos(r-M_PI_2);
            pos[dim] += y*cos(r);
            pos[dim-1] += info->center[dim-1];
            pos[dim] += info->center[dim];
        }
        int x = dsp_stream_set_position(in, pos);
        free(pos);
        if(x >= 0 && x < in->len)
            stream->buf[y] = in->buf[x];
    }
    return NULL;
}

dsp_stream_p dsp_stream_rotate(dsp_stream_p in, dsp_align_info* info)
{
    start_gettime;
    dsp_stream_p ret = dsp_stream_copy(in);
    dsp_buffer_set(ret, 0.0);
    ret->parent = in;
    int y;
    pthread_t *th = malloc(sizeof(pthread_t)*MAX_THREADS);
    struct {
       int cur_th;
       dsp_align_info *info;
       dsp_stream_p stream;
    } thread_arguments[MAX_THREADS];
    for(y = 0; y < MAX_THREADS; y++) {
        thread_arguments[y].cur_th = y;
        thread_arguments[y].info = info;
        thread_arguments[y].stream = ret;
        pthread_create(&th[y], NULL, dsp_stream_rotate_th, &thread_arguments[y]);
    }
    for(y = 0; y < MAX_THREADS; y++)
        pthread_join(th[y], NULL);
    free(th);
    end_gettime;
    return ret;
}
