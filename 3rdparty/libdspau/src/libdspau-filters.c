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

void dspau_filter_calc_coefficients(dspau_t SamplingFrequency, dspau_t LowFrequency, dspau_t HighFrequency, dspau_t* CF, dspau_t* R, dspau_t *K)
{
    dspau_t BW = (HighFrequency - LowFrequency) / SamplingFrequency;
    *CF = 2.0 * cos((LowFrequency + HighFrequency) * PI / SamplingFrequency);
    *R = 1.0 - 3.0 * BW;
    *K = (1.0 - *R * *CF + *R * *R) / (2.0 - *CF);
}

dspau_t* dspau_filter_lowpass(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t Frequency, dspau_t Q)
{
    dspau_t *out = calloc(sizeof(dspau_t), stream->len);
    dspau_t wa = 0.0;
    dspau_t CF = cos(Frequency / 2.0 * PI / SamplingFrequency);
    for(int i = 1; i < stream->len; i++) {
        wa = stream->in[i] + (wa - stream->in[i]) * (CF * Q);
        out[i] = wa;
    }
    return out;
}

dspau_t* dspau_filter_highpass(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t Frequency, dspau_t Q)
{
    dspau_t *out = calloc(sizeof(dspau_t), stream->len);
    dspau_t wa = 0.0;
    dspau_t CF = cos(Frequency / 2.0 * PI / SamplingFrequency);
    for(int i = 1; i < stream->len; i++) {
        wa = stream->in[i] + (wa - stream->in[i]) * (CF * Q);
        out[i] = stream->in[i] - wa;
    }
    return out;
}

dspau_t* dspau_filter_bandreject(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t LowFrequency, dspau_t HighFrequency) {
    dspau_t R, K, CF;
    dspau_filter_calc_coefficients(SamplingFrequency, LowFrequency, HighFrequency, &CF, &R, &K);
    dspau_t R2 = R*R;

    dspau_t a[3] = { K, -K*CF, K };
    dspau_t b[2] = { R*CF, -R2 };

    for(int i = 0; i < stream->len; i++) {
        stream->out[i] = 0;
        for(int x = 0; x < 3; x++) {
            stream->out[i] += stream->in[i + x] * a[2 - x];
        }
        if(i > 1) {
            for(int x = 0; x < 2; x++) {
                stream->out[i] -= stream->out[i - 2 + x] * b[x];
            }
        }
    }
    return stream->out;
}

dspau_t* dspau_filter_bandpass(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t LowFrequency, dspau_t HighFrequency) {
    dspau_t R, K, CF;
    dspau_filter_calc_coefficients(SamplingFrequency, LowFrequency, HighFrequency, &CF, &R, &K);
    dspau_t R2 = R*R;

    dspau_t a[3] = { 1 - K, (K-R)*CF, R2 - K };
    dspau_t b[2] = { R*CF, -R2 };

    for(int i = 0; i < stream->len; i++) {
        stream->out[i] = 0;
        for(int x = 0; x < 3; x++) {
            stream->out[i] += stream->in[i + x] * a[2 - x];
        }
        if(i > 1) {
            for(int x = 0; x < 2; x++) {
                stream->out[i] -= stream->out[i - 2 + x] * b[x];
            }
        }
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
