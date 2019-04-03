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
    int total = 1;
    for(int dim = 0; dim < stream->dims; dim++)
        total *= stream->sizes[dim];
    double* o = (double*)calloc(sizeof(double), total);
    int len = 1;
    for(int dim = 0; dim < stream->dims; dim++) {
        len *= stream->sizes[dim];
        for(int y = 0; y < total; y += len) {
            memcpy(&o[y], &stream->buf[y + len / 2], sizeof(double) * len / 2);
            memcpy(&o[y + len / 2], &stream->buf[y], sizeof(double) * len / 2);
        }
    }
    dsp_stream_free_buffer(stream);
    dsp_stream_set_buffer(stream, o, stream->len);
}

void dsp_buffer_zerofill(dsp_stream_p stream)
{
    int k;
    for(k = 0; k < stream->len; k++)
        stream->buf[k] = 0;

}

void dsp_buffer_removemean(dsp_stream_p stream)
{
    int k;

    double mean = dsp_stats_mean(stream);
    for(k = 0; k < stream->len; k++)
        stream->buf[k] = stream->buf[k] - mean;

}

void dsp_buffer_stretch(dsp_stream_p stream, double min, double max)
{

    int k;
    double mn, mx;
    dsp_stats_minmidmax(stream, &mn, &mx);
    double oratio = (max - min);
    double iratio = (mx - mn);
    if(iratio == 0) iratio = 1.0;
    for(k = 0; k < stream->len; k++) {
        stream->buf[k] -= mn;
        stream->buf[k] = stream->buf[k] * oratio / iratio;
        stream->buf[k] += min;
    }

}

void dsp_buffer_normalize(dsp_stream_p stream, double min, double max)
{
	int k;

    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = (stream->buf[k] < min ? min : (stream->buf[k] > max ? max : stream->buf[k]));
	}

}

void dsp_buffer_sub(dsp_stream_p stream, double* in, int inlen)
{
    int len = Min(stream->len, inlen);

	int k;
	for(k = 0; k < len; k++) {
        stream->buf[k] = stream->buf[k] - in[k];
	}

}

void dsp_buffer_sum(dsp_stream_p stream, double* in, int inlen)
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

static int compare( const void* a, const void* b)
{
     double int_a = * ( (double*) a );
     double int_b = * ( (double*) b );

     if ( int_a == int_b ) return 0;
     else if ( int_a < int_b ) return -1;
     else return 1;
}

void dsp_buffer_median(dsp_stream_p stream, int size, int median)
{

	int k;
    int mid = (size / 2) + (size % 2);
    double* sorted = (double*)malloc(size * sizeof(double));
    for(k = mid; k < stream->len; k++) {
        memcpy (sorted, stream->buf + (k - mid), size * sizeof(double));
        qsort(sorted, size, sizeof(double), compare);
        stream->buf[k] = sorted[median];
	}

}

void dsp_buffer_deviate(dsp_stream_p stream, dsp_stream_p deviation, double mindeviation, double maxdeviation)
{
    int len = Min(stream->len, deviation->len);
    dsp_stream_p tmp = dsp_stream_copy(deviation);
    dsp_buffer_stretch(tmp, mindeviation, maxdeviation);
    dsp_stats_val_sum(tmp);
    for(int k = 1; k < len; k++) {
        stream->buf[(int)tmp->buf[k]] = stream->buf[k];
    }
    dsp_stream_free(tmp);

}
