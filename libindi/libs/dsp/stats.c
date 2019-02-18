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

double dsp_stats_minmidmax(dsp_stream_p stream, double* min, double* max)
{
    int i;
    *min = DBL_MAX;
    *max = DBL_MIN;
    for(i = 0; i < stream->len; i++) {
        *min = Min(stream->buf[i], *min);
        *max = Max(stream->buf[i], *max);
    }
    return (double)((*max - *min) / 2.0 + *min);
}

double dsp_stats_mean(dsp_stream_p stream)
{
    int i;
    double mean = 0.0;
    double l = (double)stream->len;
    for(i = 0; i < stream->len; i++) {
        mean += stream->buf[i];
    }
    mean /=  l;
    return mean;
}

int dsp_stats_maximum_index(dsp_stream_p stream)
{
    int i;
    double min, max;
    dsp_stats_minmidmax(stream, &min, &max);
    for(i = 0; i < stream->len; i++) {
        if(stream->buf[i] == max) break;
    }
    return i;
}

int dsp_stats_minimum_index(dsp_stream_p stream)
{
    int i;
    double min, max;
    dsp_stats_minmidmax(stream, &min, &max);
    for(i = 0; i < stream->len; i++) {
        if(stream->buf[i] == min) break;
    }
    return i;
}

int dsp_stats_val_count(dsp_stream_p stream, double val)
{
    int x;
    int count = 0;
    for(x = 0; x < stream->len; x++) {
        if(stream->buf[x] == val)
            count ++;
    }
    return count;
}

double* dsp_stats_histogram(dsp_stream_p stream, int size)
{
    int k;
    dsp_stream_p o = dsp_stream_copy(stream);
    double* out = calloc(sizeof(double), size);
    long* i = calloc(sizeof(long), o->len);
    dsp_buffer_normalize(o, 0.0, size);
    dsp_buffer_copy(o->buf, i, o->len);
    dsp_buffer_copy(i, o->buf, o->len);
    for(k = 0; k < size; k++) {
        out[k] = dsp_stats_val_count(o, k);
    }
    free(i);
    dsp_stream_free_buffer(o);
    dsp_stream_free(o);
    dsp_stream_p ret = dsp_stream_new();
    dsp_stream_set_buffer(ret, out, size);
    dsp_buffer_stretch(ret, 0, size);
    dsp_stream_free(ret);
    return out;
}

double* dsp_stats_val_sum(dsp_stream_p stream)
{
    for(int i = 1; i < stream->len; i++) {
        stream->buf[i] += stream->buf[i - 1];
    }
    return stream->buf;
}

double dsp_stats_compare(dsp_stream_p stream, double* in, int inlen)
{
    double out = 0;
    for(int i = 0; i < Min(stream->len, inlen); i++) {
        out += stream->buf[i] - in[i];
    }
    return out;
}
