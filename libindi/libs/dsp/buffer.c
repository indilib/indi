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

void dsp_buffer_shift(dsp_stream_p stream)
{
    if(stream->dims == 0)
        return;
    dsp_t* out = (dsp_t*)calloc(sizeof(dsp_t), stream->len);
    for(int len = 1, dim = 0; dim < stream->dims; dim++, len *= stream->sizes[dim]) {
        for(int y = 0; y < stream->len; y += len) {
            int offset = len / 2;
            memcpy(&out[y], &stream->buf[y + offset], sizeof(dsp_t) * offset);
            memcpy(&out[y + offset], &stream->buf[y], sizeof(dsp_t) * offset);
        }
    }
    memcpy(stream->buf, out, stream->len * sizeof(dsp_t));
    free(out);
}

void dsp_buffer_clear(dsp_stream_p stream)
{
    int k;
    for(k = 0; k < stream->len; k++)
        stream->buf[k] = 0;

}

void dsp_buffer_removemean(dsp_stream_p stream)
{
    int k;

    dsp_t mean = dsp_stats_mean(stream->buf, stream->len);
    for(k = 0; k < stream->len; k++)
        stream->buf[k] = stream->buf[k] - mean;

}

void dsp_buffer_sub(dsp_stream_p stream, dsp_t* in, int inlen)
{
    int len = Min(stream->len, inlen);

	int k;
	for(k = 0; k < len; k++) {
        stream->buf[k] = stream->buf[k] - in[k];
	}

}

void dsp_buffer_sum(dsp_stream_p stream, dsp_t* in, int inlen)
{
    int len = Min(stream->len, inlen);

	int k;
	for(k = 0; k < len; k++) {
        stream->buf[k] = stream->buf[k] + in[k];
	}

}

void dsp_buffer_div(dsp_stream_p stream, double* in, int inlen)
{
    int len = Min(stream->len, inlen);

	int k;
	for(k = 0; k < len; k++) {
        stream->buf[k] = stream->buf[k] / in[k];
	}

}

void dsp_buffer_mul(dsp_stream_p stream, double* in, int inlen)
{
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = stream->buf[k] * in[k];
    }

}

void dsp_buffer_pow(dsp_stream_p stream, double* in, int inlen)
{
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = pow(stream->buf[k], in[k]);
    }

}

void dsp_buffer_log(dsp_stream_p stream, double* in, int inlen)
{
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = Log(stream->buf[k], in[k]);
    }

}

void dsp_buffer_1sub(dsp_stream_p stream, dsp_t val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = val - stream->buf[k];
    }

}

void dsp_buffer_sub1(dsp_stream_p stream, dsp_t val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = stream->buf[k] - val;
    }

}

void dsp_buffer_sum1(dsp_stream_p stream, dsp_t val)
{
	int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] += val;
	}

}

void dsp_buffer_1div(dsp_stream_p stream, double val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = val / stream->buf[k];
    }

}

void dsp_buffer_div1(dsp_stream_p stream, double val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] /= val;
    }

}

void dsp_buffer_mul1(dsp_stream_p stream, double val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = stream->buf[k] * val;
    }

}

void dsp_buffer_pow1(dsp_stream_p stream, double val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = pow(stream->buf[k], val);
    }

}

void dsp_buffer_log1(dsp_stream_p stream, double val)
{
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

void dsp_buffer_median(dsp_stream_p stream, int size, int median)
{

	int k;
    int mid = (size / 2) + (size % 2);
    dsp_t* sorted = (dsp_t*)malloc(size * sizeof(dsp_t));
    for(k = mid; k < stream->len; k++) {
        memcpy (sorted, stream->buf + (k - mid), size * sizeof(dsp_t));
        qsort(sorted, size, sizeof(dsp_t), compare);
        stream->buf[k] = sorted[median];
	}

}

void dsp_buffer_deviate(dsp_stream_p stream, double* deviation, double mindeviation, double maxdeviation)
{
    dsp_stream_p tmp = dsp_stream_copy(stream);
    for(int k = 1; k < stream->len; k++) {
        stream->buf[(int)Max(0, Min(stream->len, ((deviation[k] - mindeviation) * (maxdeviation - mindeviation) + mindeviation) + k))] = tmp->buf[k];
    }
    dsp_stream_free(tmp);
}
