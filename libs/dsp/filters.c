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

void dsp_filter_squarelaw(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
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

void dsp_filter_lowpass(dsp_stream_p stream, double Frequency)
{
    if(stream == NULL)
        return;
    int d, x;
    double radius = 0.0;
    for(d = 0; d < stream->dims; d++) {
        radius += pow(stream->sizes[d]/2.0, 2);
    }
    radius = sqrt(radius);
    dsp_fourier_dft(stream, 1);
    for(x = 0; x < stream->len; x++) {
        int* pos = dsp_stream_get_position(stream, x);
        double dist = 0.0;
        for(d = 0; d < stream->dims; d++) {
            dist += pow(stream->sizes[d]/2.0-pos[d], 2);
        }
        free(pos);
        dist = sqrt(dist);
        dist *= M_PI/radius;
        if(dist>Frequency)
            stream->magnitude->buf[x] = 0.0;
    }
    dsp_fourier_idft(stream);
}

void dsp_filter_highpass(dsp_stream_p stream, double Frequency)
{
    if(stream == NULL)
        return;
    int d, x;
    double radius = 0.0;
    for(d = 0; d < stream->dims; d++) {
        radius += pow(stream->sizes[d]/2.0, 2);
    }
    radius = sqrt(radius);
    dsp_fourier_dft(stream, 1);
    for(x = 0; x < stream->len; x++) {
        int* pos = dsp_stream_get_position(stream, x);
        double dist = 0.0;
        for(d = 0; d < stream->dims; d++) {
            dist += pow(stream->sizes[d]/2.0-pos[d], 2);
        }
        free(pos);
        dist = sqrt(dist);
        dist *= M_PI/radius;
        if(dist<Frequency)
            stream->magnitude->buf[x] = 0.0;
    }
    dsp_fourier_idft(stream);
}

void dsp_filter_bandreject(dsp_stream_p stream, double LowFrequency, double HighFrequency)
{
    if(stream == NULL)
        return;
    int d, x;
    double radius = 0.0;
    for(d = 0; d < stream->dims; d++) {
        radius += pow(stream->sizes[d]/2.0, 2);
    }
    radius = sqrt(radius);
    dsp_fourier_dft(stream, 1);
    for(x = 0; x < stream->len; x++) {
        int* pos = dsp_stream_get_position(stream, x);
        double dist = 0.0;
        for(d = 0; d < stream->dims; d++) {
            dist += pow(stream->sizes[d]/2.0-pos[d], 2);
        }
        free(pos);
        dist = sqrt(dist);
        dist *= M_PI/radius;
        if(dist<HighFrequency&&dist>LowFrequency)
            stream->magnitude->buf[x] = 0.0;
    }
    dsp_fourier_idft(stream);
}

void dsp_filter_bandpass(dsp_stream_p stream, double LowFrequency, double HighFrequency)
{
    if(stream == NULL)
        return;
    int d, x;
    double radius = 0.0;
    for(d = 0; d < stream->dims; d++) {
        radius += pow(stream->sizes[d]/2.0, 2);
    }
    radius = sqrt(radius);
    dsp_fourier_dft(stream, 1);
    for(x = 0; x < stream->len; x++) {
        int* pos = dsp_stream_get_position(stream, x);
        double dist = 0.0;
        for(d = 0; d < stream->dims; d++) {
            dist += pow(stream->sizes[d]/2.0-pos[d], 2);
        }
        free(pos);
        dist = sqrt(dist);
        dist *= M_PI/radius;
        if(dist>HighFrequency||dist<LowFrequency)
            stream->magnitude->buf[x] = 0.0;
    }
    dsp_fourier_idft(stream);
}
