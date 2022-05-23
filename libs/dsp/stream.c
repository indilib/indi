/*
*   DSP API - a digital signal processing library for astronomy usage
*   Copyright © 2017-2022  Ilia Platone
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
static int dsp_debug = 0;
static char* dsp_app_name = NULL;

static unsigned long MAX_THREADS = 1;

static FILE *out = NULL;
static FILE *err = NULL;

unsigned long int dsp_max_threads(unsigned long value)
{
    if(value>0) {
        MAX_THREADS = value;
    }
    return MAX_THREADS;
}

void dsp_set_stdout(FILE *f)
{
    out = f;
}

void dsp_set_stderr(FILE *f)
{
    err = f;
}

void dsp_print(int x, char* str)
{
    if(x == DSP_DEBUG_INFO && out != NULL)
        fprintf(out, "%s", str);
    else if(x <= dsp_get_debug_level() && err != NULL)
        fprintf(err, "%s", str);
}

void dsp_set_debug_level(int value)
{
    dsp_debug = value;
}

void dsp_set_app_name(char* name)
{
    dsp_app_name = name;
}

int dsp_get_debug_level()
{
    return dsp_debug;
}

char* dsp_get_app_name()
{
    return dsp_app_name;
}

void dsp_stream_alloc_buffer(dsp_stream_p stream, int len)
{
    if(stream == NULL)
        return;
    if(stream->buf != NULL) {
        stream->buf = (dsp_t*)realloc(stream->buf, sizeof(dsp_t) * len);
    } else {
        stream->buf = (dsp_t*)malloc(sizeof(dsp_t) * len);
    }
    if(stream->dft.buf != NULL) {
        stream->dft.fftw = (fftw_complex*)realloc(stream->dft.buf, sizeof(fftw_complex) * len);
    } else {
        stream->dft.fftw = (fftw_complex*)malloc(sizeof(fftw_complex) * len);
    }
    if(stream->location != NULL) {
        stream->location = (dsp_location*)realloc(stream->location, sizeof(dsp_location) * (stream->len));
    } else {
        stream->location = (dsp_location*)malloc(sizeof(dsp_location) * (stream->len));
    }
    if(stream->magnitude != NULL)
        dsp_stream_alloc_buffer(stream->magnitude, len);
    if(stream->phase != NULL)
        dsp_stream_alloc_buffer(stream->phase, len);
}

void dsp_stream_set_buffer(dsp_stream_p stream, void *buffer, int len)
{
    if(stream == NULL)
        return;
    stream->buf = (dsp_t*)buffer;
    stream->len = len;
}

dsp_t* dsp_stream_get_buffer(dsp_stream_p stream)
{
    if(stream == NULL)
        return NULL;
    return stream->buf;
}

void dsp_stream_free_buffer(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    if(stream->buf != NULL)
        free(stream->buf);
    if(stream->dft.buf != NULL)
        free(stream->dft.buf);
    if(stream->magnitude != NULL)
        dsp_stream_free_buffer(stream->magnitude);
    if(stream->phase != NULL)
        dsp_stream_free_buffer(stream->phase);
}

/**
 * @brief dsp_stream_new
 * @return
 */
dsp_stream_p dsp_stream_new()
{
    dsp_stream_p stream = (dsp_stream_p)malloc(sizeof(dsp_stream) * 1);
    stream->is_copy = 0;
    stream->buf = (dsp_t*)malloc(sizeof(dsp_t) * 0);
    stream->dft.buf = (double*)malloc(sizeof(dsp_t) * 0);
    stream->magnitude = NULL;
    stream->phase = NULL;
    stream->sizes = (int*)malloc(sizeof(int) * 1);
    stream->pixel_sizes = (double*)malloc(sizeof(double) * 1);
    stream->children = (dsp_stream_p*)malloc(sizeof(dsp_stream_p) * 1);
    stream->ROI = (dsp_region*)malloc(sizeof(dsp_region) * 1);
    stream->location = (dsp_location*)malloc(sizeof(dsp_location) * 1);
    stream->target = (double*)malloc(sizeof(double) * 3);
    stream->stars = (dsp_star*)malloc(sizeof(dsp_star) * 1);
    stream->triangles = (dsp_triangle*)malloc(sizeof(dsp_triangle) * 1);
    stream->align_info.offset = (double*)malloc(sizeof(double)*1);
    stream->align_info.center = (double*)malloc(sizeof(double)*1);
    stream->align_info.radians = (double*)malloc(sizeof(double)*1);
    stream->align_info.factor = (double*)malloc(sizeof(double)*1);
    stream->stars_count = 0;
    stream->triangles_count = 0;
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

/**
 * @brief dsp_stream_free
 * @param stream
 */
void dsp_stream_free(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    if(stream->sizes != NULL)
        free(stream->sizes);
    if(stream->magnitude != NULL)
        dsp_stream_free(stream->magnitude);
    if(stream->phase != NULL)
        dsp_stream_free(stream->phase);
    if(stream->is_copy == 0) {
        if(stream->pixel_sizes != NULL)
            free(stream->pixel_sizes);
        if(stream->children != NULL)
            free(stream->children);
        if(stream->ROI != NULL)
            free(stream->ROI);
        if(stream->location != NULL)
            free(stream->location);
        if(stream->target != NULL)
            free(stream->target);
        if(stream->stars != NULL)
            free(stream->stars);
        if(stream->triangles != NULL)
            free(stream->triangles);
    }
    free(stream);
}

/**
 * @brief dsp_stream_copy
 * @param stream
 * @return
 */
dsp_stream_p dsp_stream_copy(dsp_stream_p stream)
{
    if(stream == NULL)
        return NULL;
    dsp_stream_p dest = dsp_stream_new();
    int i;
    for(i = 0; i < stream->dims; i++)
       dsp_stream_add_dim(dest, abs(stream->sizes[i]));
    for(i = 0; i < stream->stars_count; i++)
        dsp_stream_add_star(dest, stream->stars[i]);
    for(i = 0; i < stream->triangles_count; i++)
        dsp_stream_add_triangle(dest, stream->triangles[i]);
    dest->is_copy = stream->is_copy + 1;
    dsp_stream_alloc_buffer(dest, dest->len);
    dest->wavelength = stream->wavelength;
    dest->samplerate = stream->samplerate;
    dest->diameter = stream->diameter;
    dest->focal_ratio = stream->focal_ratio;
    memcpy(&dest->starttimeutc,  &stream->starttimeutc, sizeof(struct timespec));
    memcpy(&dest->align_info, &stream->align_info, sizeof(dsp_align_info));
    memcpy(dest->ROI, stream->ROI, sizeof(dsp_region) * stream->dims);
    memcpy(dest->pixel_sizes, stream->pixel_sizes, sizeof(double) * stream->dims);
    memcpy(dest->target, stream->target, sizeof(double) * 3);
    if(dest->location != NULL)
        memcpy(dest->location, stream->location, sizeof(dsp_location) * stream->len);
    if(dest->buf != NULL)
        memcpy(dest->buf, stream->buf, sizeof(dsp_t) * stream->len);
    if(dest->dft.buf != NULL)
        memcpy(dest->dft.buf, stream->dft.buf, sizeof(fftw_complex) * stream->len);
    return dest;
}

/**
 * @brief dsp_stream_add_dim
 * @param stream
 * @param size
 */
void dsp_stream_add_dim(dsp_stream_p stream, int size)
{
    if(stream == NULL)
        return;
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
    stream->align_info.factor = (double*)realloc(stream->align_info.factor, sizeof(double)*stream->dims);
    if(stream->magnitude != NULL)
        dsp_stream_add_dim(stream->magnitude, size);
    if(stream->phase != NULL)
        dsp_stream_add_dim(stream->phase, size);
}

/**
 * @brief dsp_stream_set_dim
 * @param stream
 * @param dim
 * @param size
 */
void dsp_stream_set_dim(dsp_stream_p stream, int dim, int size)
{
    if(stream == NULL)
        return;
    int d = 0;
    if(dim < stream->dims) {
        stream->sizes[dim] = size;
        stream->len = 1;
        for(d = 0; d < stream->dims; d++)
            stream->len *= stream->sizes[d];
        if(stream->magnitude != NULL)
            dsp_stream_set_dim(stream->magnitude, dim, size);
        if(stream->phase != NULL)
            dsp_stream_set_dim(stream->phase, dim, size);
    }
}

/**
 * @brief dsp_stream_del_dim
 * @param stream
 * @param index
 */
void dsp_stream_del_dim(dsp_stream_p stream, int index)
{
    if(stream == NULL)
        return;
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
    if(stream->magnitude != NULL)
        dsp_stream_del_dim(stream->magnitude, index);
    if(stream->phase != NULL)
        dsp_stream_del_dim(stream->phase, index);
}

/**
 * @brief dsp_stream_add_child
 * @param stream
 * @param child
 */
void dsp_stream_add_child(dsp_stream_p stream, dsp_stream_p child)
{
    if(stream == NULL)
        return;
    child->parent = stream;
    stream->children[stream->child_count] = child;
    stream->child_count++;
    stream->children = (dsp_stream_p*)realloc(stream->children, sizeof(dsp_stream_p) * (stream->child_count + 1));
}

/**
 * @brief dsp_stream_del_child
 * @param stream
 * @param index
 */
void dsp_stream_del_child(dsp_stream_p stream, int index)
{
    if(stream == NULL)
        return;
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

/**
 * @brief dsp_stream_add_star
 * @param stream
 * @param star
 */
void dsp_stream_add_star(dsp_stream_p stream, dsp_star star)
{
    if(stream == NULL)
        return;
    int d;
    stream->stars = (dsp_star*)realloc(stream->stars, sizeof(dsp_star)*(stream->stars_count+1));
    strcpy(stream->stars[stream->stars_count].name, star.name);
    stream->stars[stream->stars_count].diameter = star.diameter;
    stream->stars[stream->stars_count].center.dims = star.center.dims;
    stream->stars[stream->stars_count].center.location = (double*)malloc(sizeof(double)*star.center.dims);
    for(d = 0; d < star.center.dims; d++)
        stream->stars[stream->stars_count].center.location[d] = star.center.location[d];
    stream->stars_count++;
}

/**
 * @brief dsp_stream_del_star
 * @param stream
 * @param index
 */
void dsp_stream_del_star(dsp_stream_p stream, int index)
{
    if(stream == NULL)
        return;
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

/**
 * @brief dsp_stream_add_triangle
 * @param stream
 * @param triangle
 */
void dsp_stream_add_triangle(dsp_stream_p stream, dsp_triangle triangle)
{
    if(stream == NULL)
        return;
    int s;
    int d;
    stream->triangles = (dsp_triangle*)realloc(stream->triangles, sizeof(dsp_triangle)*(stream->triangles_count+1));
    stream->triangles[stream->triangles_count].dims = triangle.dims;
    stream->triangles[stream->triangles_count].theta = triangle.theta;
    stream->triangles[stream->triangles_count].ratios = (double*)malloc(sizeof(double)*triangle.dims);
    stream->triangles[stream->triangles_count].sizes = (double*)malloc(sizeof(double)*triangle.dims);
    stream->triangles[stream->triangles_count].stars = (dsp_star*)malloc(sizeof(dsp_star)*triangle.dims);
    stream->triangles[stream->triangles_count].ratios[0] = 1.0;
    triangle.index = triangle.index;
    for (s = 0; s < triangle.dims; s++) {
        stream->triangles[stream->triangles_count].sizes[s] = triangle.sizes[s];
        if(s > 0) {
            stream->triangles[stream->triangles_count].ratios[s] = fabs(triangle.sizes[s]/triangle.sizes[0]);
        }
        stream->triangles[stream->triangles_count].stars[s].center.dims = triangle.stars[s].center.dims;
        stream->triangles[stream->triangles_count].stars[s].diameter = triangle.stars[s].diameter;
        stream->triangles[stream->triangles_count].stars[s].center.location = (double*)malloc(sizeof(double)*triangle.stars[s].center.dims);
        for(d = 0; d < triangle.stars[s].center.dims; d++)
            stream->triangles[stream->triangles_count].stars[s].center.location[d] = triangle.stars[s].center.location[d];
    }
    stream->triangles[stream->triangles_count].index = triangle.index;
    stream->triangles_count++;
}

/**
 * @brief dsp_stream_del_triangle
 * @param stream
 * @param index
 */
void dsp_stream_del_triangle(dsp_stream_p stream, int index)
{
    if(stream == NULL)
        return;
    dsp_triangle* triangles = (dsp_triangle*)malloc(sizeof(dsp_triangle) * stream->triangles_count);
    int triangles_count = stream->triangles_count;
    memcpy(triangles, stream->triangles, sizeof(dsp_triangle*) * stream->triangles_count);
    free(stream->triangles);
    stream->triangles_count = 0;
    int i;
    for(i = 0; i < triangles_count; i++) {
        if(i != index) {
            dsp_stream_add_triangle(stream, triangles[i]);
        }
    }
}

/**
 * @brief dsp_stream_get_position
 * @param stream
 * @param index
 * @return
 */
int* dsp_stream_get_position(dsp_stream_p stream, int index) {
    if(stream == NULL)
        return NULL;
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

/**
 * @brief dsp_stream_set_position
 * @param stream
 * @param pos
 * @return
 */
int dsp_stream_set_position(dsp_stream_p stream, int* pos) {
    if(stream == NULL)
        return -1;
    int dim = 0;
    int index = 0;
    int m = 1;
    for (dim = 0; dim < stream->dims; dim++) {
        index += m * pos[dim];
        m *= stream->sizes[dim];
    }
    return index;
}

/**
 * @brief dsp_stream_crop
 * @param in
 */
void dsp_stream_crop(dsp_stream_p in)
{
    if(in == NULL)
        return;
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

void dsp_stream_translate(dsp_stream_p in)
{
    if(in == NULL)
        return;
    dsp_stream_p stream = dsp_stream_copy(in);
    int* offset = (int*)malloc(sizeof(int)*stream->dims);
    dsp_buffer_copy(in->align_info.offset, offset, in->dims);
    int z = dsp_stream_set_position(stream, offset);
    free(offset);
    int k = -z;
    z = Max(0, z);
    k = Max(0, k);
    int len = stream->len-z-k;
    dsp_t *buf = &stream->buf[z];
    dsp_t *data = &in->buf[k];
    memset(in->buf, 0, sizeof(dsp_t)*in->len);
    memcpy(data, buf, sizeof(dsp_t)*len);
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);

}

/**
 * @brief dsp_stream_scale_th
 * @param arg
 * @return
 */
static void* dsp_stream_scale_th(void* arg)
{
    struct {
        int cur_th;
        dsp_stream_p stream;
     } *arguments = arg;
    dsp_stream_p stream = arguments->stream;
    dsp_stream_p in = stream->parent;
    int cur_th = arguments->cur_th;
    int start = cur_th * stream->len / dsp_max_threads(0);
    int end = start + stream->len / dsp_max_threads(0);
    end = Min(stream->len, end);
    int y, d;
    for(y = start; y < end; y++)
    {
        int* pos = dsp_stream_get_position(stream, y);
        double factor = 0.0;
        for(d = 0; d < stream->dims; d++) {
            pos[d] -= stream->align_info.center[d];
            pos[d] /= stream->align_info.factor[d];
            pos[d] += stream->align_info.center[d];
            factor += pow(stream->align_info.factor[d], 2);
        }
        factor = sqrt(factor);
        int x = dsp_stream_set_position(in, pos);
        if(x >= 0 && x < in->len)
            stream->buf[y] += in->buf[x]/(factor*stream->dims);
        free(pos);
    }
    return NULL;
}

void dsp_stream_scale(dsp_stream_p in)
{
    if(in == NULL)
        return;
    int y;
    dsp_stream_p stream = dsp_stream_copy(in);
    dsp_buffer_set(stream->buf, stream->len, 0);
    stream->parent = in;
    pthread_t *th = malloc(sizeof(pthread_t)*dsp_max_threads(0));
    struct {
       int cur_th;
       dsp_stream_p stream;
    } thread_arguments[dsp_max_threads(0)];
    for(y = 0; y < dsp_max_threads(0); y++)
    {
        thread_arguments[y].cur_th = y;
        thread_arguments[y].stream = stream;
        pthread_create(&th[y], NULL, dsp_stream_scale_th, &thread_arguments[y]);
    }
    for(y = 0; y < dsp_max_threads(0); y++)
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
    int start = cur_th * stream->len / dsp_max_threads(0);
    int end = start + stream->len / dsp_max_threads(0);
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
            pos[dim] = x*-sin(r);
            pos[dim-1] = x*cos(r);
            pos[dim] += y*cos(r);
            pos[dim-1] += y*sin(r);
            pos[dim] += stream->align_info.center[dim];
            pos[dim-1] += stream->align_info.center[dim-1];
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
    if(in == NULL)
        return;
    dsp_stream_p stream = dsp_stream_copy(in);
    dsp_buffer_set(stream->buf, stream->len, 0);
    stream->parent = in;
    int y;
    pthread_t *th = malloc(sizeof(pthread_t)*dsp_max_threads(0));
    struct {
       int cur_th;
       dsp_stream_p stream;
    } thread_arguments[dsp_max_threads(0)];
    for(y = 0; y < dsp_max_threads(0); y++) {
        thread_arguments[y].cur_th = y;
        thread_arguments[y].stream = stream;
        pthread_create(&th[y], NULL, dsp_stream_rotate_th, &thread_arguments[y]);
    }
    for(y = 0; y < dsp_max_threads(0); y++)
        pthread_join(th[y], NULL);
    free(th);
    dsp_buffer_copy(stream->buf, in->buf, stream->len);
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);

}
