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
#include <setjmp.h>
#include <signal.h>

void dsp_buffer_shift(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    if(stream->dims == 0)
        return;
    dsp_t* tmp = (dsp_t*)malloc(sizeof(dsp_t) * stream->len);
    int x, d;
    for(x = 0; x < stream->len/2; x++) {
        int* pos = dsp_stream_get_position(stream, x);
        for(d = 0; d < stream->dims; d++) {
            if(pos[d]<stream->sizes[d] / 2) {
                pos[d] += stream->sizes[d] / 2;
            } else {
                pos[d] -= stream->sizes[d] / 2;
            }
        }
        tmp[x] = stream->buf[dsp_stream_set_position(stream, pos)];
        tmp[dsp_stream_set_position(stream, pos)] = stream->buf[x];
        free(pos);
    }
    memcpy(stream->buf, tmp, stream->len * sizeof(dsp_t));
    free(tmp);
}

void dsp_buffer_removemean(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    int k;

    dsp_t mean = dsp_stats_mean(stream->buf, stream->len);
    for(k = 0; k < stream->len; k++)
        stream->buf[k] = stream->buf[k] - mean;

}

void dsp_buffer_sub(dsp_stream_p stream, dsp_t* in, int inlen)
{
    if(stream == NULL)
        return;
    int len = Min(stream->len, inlen);

	int k;
	for(k = 0; k < len; k++) {
        stream->buf[k] = stream->buf[k] - in[k];
	}

}

void dsp_buffer_sum(dsp_stream_p stream, dsp_t* in, int inlen)
{
    if(stream == NULL)
        return;
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] += in[k];
    }

}

void dsp_buffer_max(dsp_stream_p stream, dsp_t* in, int inlen)
{
    if(stream == NULL)
        return;
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = Max(stream->buf[k], in[k]);
    }

}

void dsp_buffer_min(dsp_stream_p stream, dsp_t* in, int inlen)
{
    if(stream == NULL)
        return;
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = Min(stream->buf[k], in[k]);
    }

}

void dsp_buffer_div(dsp_stream_p stream, dsp_t* in, int inlen)
{
    if(stream == NULL)
        return;
    int len = Min(stream->len, inlen);

	int k;
	for(k = 0; k < len; k++) {
        stream->buf[k] = stream->buf[k] / in[k];
	}

}

void dsp_buffer_mul(dsp_stream_p stream, dsp_t* in, int inlen)
{
    if(stream == NULL)
        return;
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = stream->buf[k] * in[k];
    }

}

void dsp_buffer_pow(dsp_stream_p stream, dsp_t* in, int inlen)
{
    if(stream == NULL)
        return;
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = pow(stream->buf[k], in[k]);
    }

}

void dsp_buffer_log(dsp_stream_p stream, dsp_t* in, int inlen)
{
    if(stream == NULL)
        return;
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = Log(stream->buf[k], in[k]);
    }

}

void dsp_buffer_1sub(dsp_stream_p stream, dsp_t val)
{
    if(stream == NULL)
        return;
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = val - stream->buf[k];
    }

}

void dsp_buffer_sub1(dsp_stream_p stream, dsp_t val)
{
    if(stream == NULL)
        return;
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = stream->buf[k] - val;
    }

}

void dsp_buffer_sum1(dsp_stream_p stream, dsp_t val)
{
    if(stream == NULL)
        return;
	int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] += val;
	}

}

void dsp_buffer_1div(dsp_stream_p stream, double val)
{
    if(stream == NULL)
        return;
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = val / stream->buf[k];
    }

}

void dsp_buffer_div1(dsp_stream_p stream, double val)
{
    if(stream == NULL)
        return;
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] /= val;
    }

}

void dsp_buffer_mul1(dsp_stream_p stream, double val)
{
    if(stream == NULL)
        return;
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = stream->buf[k] * val;
    }

}

void dsp_buffer_pow1(dsp_stream_p stream, double val)
{
    if(stream == NULL)
        return;
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = pow(stream->buf[k], val);
    }

}

void dsp_buffer_log1(dsp_stream_p stream, double val)
{
    if(stream == NULL)
        return;
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = Log(stream->buf[k], val);
    }

}

static int compare( const void* a, const void* b)
{
     dsp_t int_a = * ( (dsp_t*) a );
     dsp_t int_b = * ( (dsp_t*) b );

     if ( int_a == int_b ) return 0;
     else if ( int_a < int_b ) return -1;
     else return 1;
}

static void* dsp_buffer_median_th(void* arg)
{
    struct {
        int cur_th;
        int size;
        int median;
        dsp_stream_p stream;
        dsp_stream_p box;
     } *arguments = arg;
    dsp_stream_p stream = arguments->stream;
    dsp_stream_p box = arguments->box;
    dsp_stream_p in = stream->parent;
    int cur_th = arguments->cur_th;
    int size = arguments->size;
    int median = arguments->median;
    int start = cur_th * stream->len / dsp_max_threads(0);
    int end = start + stream->len / dsp_max_threads(0);
    end = Min(stream->len, end);
    int x, y, dim, idx;
    dsp_t* sorted = (dsp_t*)malloc(pow(size, stream->dims) * sizeof(dsp_t));
    int len = pow(size, in->dims);
    for(x = start; x < end; x++) {
        dsp_t* buf = sorted;
        for(y = 0; y < box->len; y++) {
            int *pos = dsp_stream_get_position(stream, x);
            int *mat = dsp_stream_get_position(box, y);
            for(dim = 0; dim < stream->dims; dim++) {
                pos[dim] += mat[dim] - size / 2;
            }
            idx = dsp_stream_set_position(stream, pos);
            if(idx >= 0 && idx < in->len) {
                *buf++ = in->buf[idx];
            }
            free(pos);
            free(mat);
        }
        qsort(sorted, len, sizeof(dsp_t), compare);
        stream->buf[x] = sorted[median*box->len/size];
    }
    dsp_stream_free_buffer(box);
    dsp_stream_free(box);
    free(sorted);
    return NULL;
}

void dsp_buffer_median(dsp_stream_p in, int size, int median)
{
    if(in == NULL)
        return;
    int y, d;
    dsp_stream_p stream = dsp_stream_copy(in);
    dsp_buffer_set(stream->buf, stream->len, 0);
    stream->parent = in;
    pthread_t *th = (pthread_t *)malloc(sizeof(pthread_t)*dsp_max_threads(0));
    struct {
       int cur_th;
       int size;
       int median;
       dsp_stream_p stream;
       dsp_stream_p box;
    } thread_arguments[dsp_max_threads(0)];
    for(y = 0; y < dsp_max_threads(0); y++)
    {
        thread_arguments[y].cur_th = y;
        thread_arguments[y].size = size;
        thread_arguments[y].median = median;
        thread_arguments[y].stream = stream;
        thread_arguments[y].box = dsp_stream_new();
        for(d = 0; d < stream->dims; d++)
            dsp_stream_add_dim(thread_arguments[y].box, size);
        dsp_stream_alloc_buffer(thread_arguments[y].box, thread_arguments[y].box->len);
        pthread_create(&th[y], NULL, dsp_buffer_median_th, &thread_arguments[y]);
    }
    for(y = 0; y < dsp_max_threads(0); y++)
        pthread_join(th[y], NULL);
    free(th);
    stream->parent = NULL;
    dsp_buffer_copy(stream->buf, in->buf, stream->len);
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);
}

static void* dsp_buffer_sigma_th(void* arg)
{
    struct {
        int cur_th;
        int size;
        dsp_stream_p stream;
        dsp_stream_p box;
     } *arguments = arg;
    dsp_stream_p stream = arguments->stream;
    dsp_stream_p in = stream->parent;
    dsp_stream_p box = arguments->box;
    int cur_th = arguments->cur_th;
    int size = arguments->size;
    int start = cur_th * stream->len / dsp_max_threads(0);
    int end = start + stream->len / dsp_max_threads(0);
    end = Min(stream->len, end);
    int x, y, dim, idx;
    dsp_t* sigma = (dsp_t*)malloc(pow(size, stream->dims) * sizeof(dsp_t));
    int len = pow(size, in->dims);
    for(x = start; x < end; x++) {
        dsp_t* buf = sigma;
        for(y = 0; y < box->len; y++) {
            int *pos = dsp_stream_get_position(stream, x);
            int *mat = dsp_stream_get_position(box, y);
            for(dim = 0; dim < stream->dims; dim++) {
                pos[dim] += mat[dim] - size / 2;
            }
            idx = dsp_stream_set_position(stream, pos);
            if(idx >= 0 && idx < in->len) {
                buf[y] = in->buf[idx];
            }
            free(pos);
            free(mat);
        }
        stream->buf[x] = dsp_stats_stddev(buf, len);
    }
    dsp_stream_free_buffer(box);
    dsp_stream_free(box);
    free(sigma);
    return NULL;
}

void dsp_buffer_sigma(dsp_stream_p in, int size)
{
    if(in == NULL)
        return;
    int y, d;
    dsp_stream_p stream = dsp_stream_copy(in);
    dsp_buffer_set(stream->buf, stream->len, 0);
    stream->parent = in;
    pthread_t *th = (pthread_t *)malloc(sizeof(pthread_t)*dsp_max_threads(0));
    struct {
       int cur_th;
       int size;
       dsp_stream_p stream;
       dsp_stream_p box;
    } thread_arguments[dsp_max_threads(0)];
    for(y = 0; y < dsp_max_threads(0); y++)
    {
        thread_arguments[y].cur_th = y;
        thread_arguments[y].size = size;
        thread_arguments[y].stream = stream;
        thread_arguments[y].box = dsp_stream_new();
        for(d = 0; d < stream->dims; d++)
            dsp_stream_add_dim(thread_arguments[y].box, size);
        pthread_create(&th[y], NULL, dsp_buffer_sigma_th, &thread_arguments[y]);
    }
    for(y = 0; y < dsp_max_threads(0); y++)
        pthread_join(th[y], NULL);
    free(th);
    stream->parent = NULL;
    dsp_buffer_copy(stream->buf, in->buf, stream->len);
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);
}

void dsp_buffer_deviate(dsp_stream_p stream, dsp_t* deviation, dsp_t mindeviation, dsp_t maxdeviation)
{
    if(stream == NULL)
        return;
    dsp_stream_p tmp = dsp_stream_copy(stream);
    int k;
    for(k = 1; k < stream->len; k++) {
        stream->buf[(int)Max(0, Min(stream->len, ((deviation[k] - mindeviation) * (maxdeviation - mindeviation) + mindeviation) + k))] = tmp->buf[k];
    }
    dsp_stream_free(tmp);
}
