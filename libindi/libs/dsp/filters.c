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

double* dsp_filter_squarelaw(dsp_stream_p stream)
{
    double* in = stream->in;
    double* out = stream->out;
    int len = stream->len;
    double mean = dsp_stats_mean(in, len);
	int val = 0;
	int i;
	for(i = 0; i < len; i++) {
		val = in [i] - mean;
		out [i] = (abs (val) + mean);
	}
    return out;
}

void dsp_filter_calc_coefficients(double SamplingFrequency, double LowFrequency, double HighFrequency, double* CF, double* R, double *K)
{
    double BW = (HighFrequency - LowFrequency) / SamplingFrequency;
    *CF = 2.0 * cos((LowFrequency + HighFrequency) * PI / SamplingFrequency);
    *R = 1.0 - 3.0 * BW;
    *K = (1.0 - *R * *CF + *R * *R) / (2.0 - *CF);
}

double* dsp_filter_lowpass(dsp_stream_p stream, double SamplingFrequency, double Frequency, double Q)
{
    double *out = calloc(sizeof(double), stream->len);
    double wa = 0.0;
    double CF = cos(Frequency / 2.0 * PI / SamplingFrequency);
    for(int i = 1; i < stream->len; i++) {
        wa = stream->in[i] + (wa - stream->in[i]) * (CF * Q);
        out[i] = wa;
    }
    return out;
}

double* dsp_filter_highpass(dsp_stream_p stream, double SamplingFrequency, double Frequency, double Q)
{
    double *out = calloc(sizeof(double), stream->len);
    double wa = 0.0;
    double CF = cos(Frequency / 2.0 * PI / SamplingFrequency);
    for(int i = 1; i < stream->len; i++) {
        wa = stream->in[i] + (wa - stream->in[i]) * (CF * Q);
        out[i] = stream->in[i] - wa;
    }
    return out;
}

double* dsp_filter_bandreject(dsp_stream_p stream, double SamplingFrequency, double LowFrequency, double HighFrequency) {
    double R, K, CF;
    dsp_filter_calc_coefficients(SamplingFrequency, LowFrequency, HighFrequency, &CF, &R, &K);
    double R2 = R*R;

    double a[3] = { K, -K*CF, K };
    double b[2] = { R*CF, -R2 };

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

double* dsp_filter_bandpass(dsp_stream_p stream, double SamplingFrequency, double LowFrequency, double HighFrequency) {
    double R, K, CF;
    dsp_filter_calc_coefficients(SamplingFrequency, LowFrequency, HighFrequency, &CF, &R, &K);
    double R2 = R*R;

    double a[3] = { 1 - K, (K-R)*CF, R2 - K };
    double b[2] = { R*CF, -R2 };

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

double* dsp_filter_deviate(dsp_stream_p stream, dsp_stream_p deviation, double mindeviation, double maxdeviation)
{
    double min, max;
    dsp_stats_minmidmax(stream->in, stream->len, &min, &max);
    int dims = Min(stream->dims, deviation->dims);
    int len = Min(stream->len, deviation->len);
    for(int dim = 0; dim < dims; dim++) {
        int size = len / Min(stream->sizes[dim], deviation->sizes[dim]);
        double * tmp = calloc(size, sizeof(double));
        for(int x = 0; x < len; x += size) {
            tmp = dsp_buffer_deviate(stream->in + x, size, deviation->in + x, size, mindeviation, maxdeviation);
            dsp_buffer_sum(stream->out + x, size, tmp, size);
        }
        free(tmp);
    }
    dsp_buffer_stretch(stream->out, len, min, max);
    return stream->out;
}
