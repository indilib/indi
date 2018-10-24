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

typedef struct {
    dspau_t e;
    dspau_t p;
    dspau_t d0;
    dspau_t d1;
    dspau_t d2;
    dspau_t x0;
    dspau_t x1;
    dspau_t x2;
    dspau_t y0;
    dspau_t y1;
    dspau_t y2;
} Coefficient;

dspau_t* dspau_filter_squarelaw(dspau_stream_p stream)
{
    dspau_t* in = stream->in;
    dspau_t* out = stream->out;
    int len = stream->len;
    dspau_t mean = dspau_stats_mean(in, len);
	int val = 0;
	int i;
	for(i = 0; i < len; i++) {
		val = in [i] - mean;
		out [i] = (abs (val) + mean);
	}
    return out;
}

dspau_t* dspau_filter_lowpass(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t Frequency, dspau_t Q)
{
    dspau_t wa = 0.0;
    dspau_t wc = (Frequency * PI / SamplingFrequency) * 2.0;
    for(int i = 0; i < stream->len; i++) {
        wa = pow(sin((dspau_t)i * wc), Q);
        stream->out[i] = stream->in[i] * wa;
    }
    return stream->out;
}

dspau_t* dspau_filter_highpass(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t Frequency, dspau_t Q)
{
    dspau_t wa = 0.0;
    dspau_t wc = (Frequency * PI / SamplingFrequency) * 2.0;
    for(int i = 0; i < stream->len; i++) {
        wa = pow(sin((dspau_t)i * wc), Q);
        stream->out[i] = stream->in[i] -(stream->in[i] * wa);
    }
    return stream->out;
}

dspau_t* dspau_filter_bandreject(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t LowFrequency, dspau_t HighFrequency, dspau_t Q) {
    dspau_t wal = 0.0;
    dspau_t wah = 0.0;
    dspau_t wcl = (LowFrequency * PI / SamplingFrequency) * 2.0;
    dspau_t wch = (HighFrequency * PI / SamplingFrequency) * 2.0;
    for(int i = 0; i < stream->len; i++) {
        wal = pow(sin((dspau_t)i * wcl), Q);
        wah = pow(sin((dspau_t)i * wch), Q);
        stream->out[i] = (stream->in[i] -(stream->in[i] * wah)) * wal;
    }
    return stream->out;
}

dspau_t* dspau_filter_bandpass(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t LowFrequency, dspau_t HighFrequency, dspau_t Q) {
    dspau_t wal = 0.0;
    dspau_t wah = 0.0;
    dspau_t wcl = (LowFrequency * PI / SamplingFrequency) * 2.0;
    dspau_t wch = (HighFrequency * PI / SamplingFrequency) * 2.0;
    for(int i = 0; i < stream->len; i++) {
        wal = pow(sin((dspau_t)i * wcl), Q);
        wah = pow(sin((dspau_t)i * wch), Q);
        stream->out[i] = (stream->in[i] -(stream->in[i] * wal)) * wah;
    }
    return stream->out;
}

dspau_t* dspau_filter_deviate(dspau_stream_p stream, dspau_stream_p deviation, dspau_t mindeviation, dspau_t maxdeviation)
{
    dspau_t min, max;
    dspau_stats_minmidmax(stream->in, stream->len, &min, &max);
    int dims = Min(stream->dims, deviation->dims);
    int len = Min(stream->len, deviation->len);
    for(int dim = 0; dim < dims; dim++) {
        int size = len / Min(stream->sizes[dim], deviation->sizes[dim]);
        dspau_t * tmp = calloc(size, sizeof(dspau_t));
        for(int x = 0; x < len; x += size) {
            tmp = dspau_buffer_deviate(stream->in + x, size, deviation->in + x, size, mindeviation, maxdeviation);
            dspau_buffer_sum(stream->out + x, size, tmp, size);
        }
        free(tmp);
    }
    dspau_buffer_stretch(stream->out, len, min, max);
    return stream->out;
}
