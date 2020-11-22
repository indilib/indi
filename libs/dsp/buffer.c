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
        stream->buf[k] += in[k];
	}

}

void dsp_buffer_div(dsp_stream_p stream, dsp_t* in, int inlen)
{
    int len = Min(stream->len, inlen);

	int k;
	for(k = 0; k < len; k++) {
        stream->buf[k] = stream->buf[k] / in[k];
	}

}

void dsp_buffer_mul(dsp_stream_p stream, dsp_t* in, int inlen)
{
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = stream->buf[k] * in[k];
    }

}

void dsp_buffer_pow(dsp_stream_p stream, dsp_t* in, int inlen)
{
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = pow(stream->buf[k], in[k]);
    }

}

void dsp_buffer_log(dsp_stream_p stream, dsp_t* in, int inlen)
{
    int len = Min(stream->len, inlen);

    int k;
    for(k = 0; k < len; k++) {
        stream->buf[k] = Log(stream->buf[k], in[k]);
    }

}

void dsp_buffer_1sub(dsp_stream_p stream, double val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = val - stream->buf[k];
    }

}

void dsp_buffer_sub1(dsp_stream_p stream, double val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = stream->buf[k] - val;
    }

}

void dsp_buffer_sum1(dsp_stream_p stream, double val)
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
        stream->buf[k] = (dsp_t)(val / (dsp_t)stream->buf[k]);
    }

}

void dsp_buffer_div1(dsp_stream_p stream, double val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = (dsp_t)((dsp_t)stream->buf[k] / val);
    }

}

void dsp_buffer_mul1(dsp_stream_p stream, double val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = (dsp_t)((dsp_t)stream->buf[k] * val);
    }

}

void dsp_buffer_pow1(dsp_stream_p stream, double val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = (dsp_t)pow((dsp_t)stream->buf[k], val);
    }

}

void dsp_buffer_log1(dsp_stream_p stream, double val)
{
    int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = (dsp_t)Log((dsp_t)stream->buf[k], val);
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

void dsp_buffer_deviate(dsp_stream_p stream, dsp_t* deviation, dsp_t mindeviation, dsp_t maxdeviation)
{
    dsp_stream_p tmp = dsp_stream_copy(stream);
    int k;
    for(k = 1; k < stream->len; k++) {
        stream->buf[(int)Max(0, Min(stream->len, ((deviation[k] - mindeviation) * (maxdeviation - mindeviation) + mindeviation) + k))] = tmp->buf[k];
    }
    dsp_stream_free(tmp);
}
