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

void dsp_filter_squarelaw(dsp_stream_p stream)
{
    dsp_t* in = stream->buf;
    dsp_t *out = (dsp_t*)malloc(sizeof(dsp_t) * stream->len);
    int len = stream->len;
    dsp_t mean = dsp_stats_mean(stream->buf, stream->len);
    int val = 0;
    int i;
    for(i = 0; i < len; i++) {
        val = in [i] - mean;
        out [i] = (abs (val) + mean);
    }
    memcpy(stream->buf, out, stream->len * sizeof(dsp_t));
    free(out);
}

void dsp_filter_calc_coefficients(double SamplingFrequency, double LowFrequency, double HighFrequency, double* CF, double* R, double *K)
{
    double BW = (HighFrequency - LowFrequency) / SamplingFrequency;
    *CF = 2.0 * cos((LowFrequency + HighFrequency) * M_PI / SamplingFrequency);
    *R = 1.0 - 3.0 * BW;
    *K = (1.0 - *R * *CF + *R * *R) / (2.0 - *CF);
}

void dsp_filter_lowpass(dsp_stream_p stream, double SamplingFrequency, double Frequency)
{
    double *out = (double*)malloc(sizeof(double) * stream->len);
    double CF = cos(Frequency / 2.0 * M_PI / SamplingFrequency);
    int dim = -1, i;
    out[0] = stream->buf[0];
    while (dim++ < stream->dims - 1) {
        int size = (dim < 0 ? 1 : stream->sizes[dim]);
        for(i = size; i < stream->len; i+=size) {
            out[i] += stream->buf[i] + (out[i - size] - stream->buf[i]) * CF;
        }
    }
    memcpy(stream->buf, out, stream->len * sizeof(double));
    free(out);
}

void dsp_filter_highpass(dsp_stream_p stream, double SamplingFrequency, double Frequency)
{
    dsp_t *out = (dsp_t*)malloc(sizeof(dsp_t) * stream->len);
    double CF = cos(Frequency / 2.0 * M_PI / SamplingFrequency);
    int dim = -1, i;
    out[0] = stream->buf[0];
    while (dim++ < stream->dims - 1) {
        int size = (dim < 0 ? 1 : stream->sizes[dim]);
        for(i = size; i < stream->len; i+=size) {
            out[i] += stream->buf[i] + (out[i - size] - stream->buf[i]) * CF;
        }
    }
    dsp_buffer_sub(stream, out, stream->len);
    free(out);
}

void dsp_filter_bandreject(dsp_stream_p stream, double SamplingFrequency, double LowFrequency, double HighFrequency) {
    dsp_t *out = (dsp_t*)malloc(sizeof(dsp_t) * stream->len);
    double R, K, CF;
    dsp_filter_calc_coefficients(SamplingFrequency, LowFrequency, HighFrequency, &CF, &R, &K);
    double R2 = R*R;

    double a[3] = { K, -K*CF, K };
    double b[2] = { R*CF, -R2 };

    int dim = -1, i, x;
    while (dim++ < stream->dims - 1) {
        for(i = 0; i < stream->len; i++) {
            int size = (dim < 0 ? 1 : stream->sizes[dim]);
            out[i] = 0;
            if(i < stream->len - 2 * size) {
                for(x = 0; x < 3; x++) {
                    out[i] += (double)stream->buf[i + x * size] * a[2 - x];
                }
            }
            if(i > size) {
                for(x = 0; x < 2; x++) {
                    out[i] -= out[i - 2 * size + x * size] * b[x];
                }
            }
        }
    }
    memcpy(stream->buf, out, stream->len * sizeof(dsp_t));
    free(out);
}

void dsp_filter_bandpass(dsp_stream_p stream, double SamplingFrequency, double LowFrequency, double HighFrequency) {
    dsp_t *out = (dsp_t*)malloc(sizeof(dsp_t) * stream->len);
    double R, K, CF;
    dsp_filter_calc_coefficients(SamplingFrequency, LowFrequency, HighFrequency, &CF, &R, &K);
    double R2 = R*R;

    double a[3] = { 1 - K, (K-R)*CF, R2 - K };
    double b[2] = { R*CF, -R2 };

    int dim = -1, i, x;
    while (dim++ < stream->dims - 1) {
        for(i = 0; i < stream->len; i++) {
            int size = (dim < 0 ? 1 : stream->sizes[dim]);
            out[i] = 0;
            if(i < stream->len - 2 * size) {
                for(x = 0; x < 3; x++) {
                    out[i] += (double)stream->buf[i + x * size] * a[2 - x];
                }
            }
            if(i > size) {
                for(x = 0; x < 2; x++) {
                    out[i] -= out[i - 2 * size + x * size] * b[x];
                }
            }
        }
    }
    memcpy(stream->buf, out, stream->len * sizeof(dsp_t));
    free(out);
}
