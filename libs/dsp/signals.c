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

void dsp_signals_whitenoise(dsp_stream_p stream)
{
    int k;
    for(k = 0; k < stream->len; k++) {
        stream->buf[k] = (rand() % 255) / 255.0;
    }

}

void dsp_signals_sinewave(dsp_stream_p stream, double samplefreq, double freq)
{
    freq /= samplefreq;
    double rad = 0;
    double x = 0;
    int k;
    for(k = 0; k < stream->len; k++) {
        rad += freq;
        x = rad;
        while (x > 1.0)
            x -= 1.0;
        x *= M_PI * 2;
        stream->buf[k] = sin(x);
    }

}

void dsp_signals_sawtoothwave(dsp_stream_p stream, double samplefreq, double freq)
{
    freq /= samplefreq;
    double rad = 0;
    double x = 0;
    int k;
    for(k = 0; k < stream->len; k++) {
        rad += freq;
        x = rad;
        while (x > 1.0)
            x -= 1.0;
        stream->buf[k] = (double)(32768+32767*x);
    }

}

void dsp_signals_triwave(dsp_stream_p stream, double samplefreq, double freq)
{
    freq /= samplefreq;
    double rad = 0;
    double x = 0;
    int k;
    for(k = 0; k < stream->len; k++) {
        rad += freq;
        x = rad;
        while (x > 2.0)
            x -= 2.0;
        while (x > 1.0)
            x = 2.0 - x;
        stream->buf[k] = (double)(32768+32767*x);
    }

}

void dsp_modulation_frequency(dsp_stream_p stream, double samplefreq, double freq, double bandwidth)
{
    dsp_stream_p carrier = dsp_stream_new();
    dsp_signals_sinewave(carrier, samplefreq, freq);
    double mn = dsp_stats_min(stream->buf, stream->len);
    double mx = dsp_stats_max(stream->buf, stream->len);
    double lo = mn * bandwidth * 1.5 / samplefreq;
    double hi = mx * bandwidth * 0.5 / samplefreq;
    double *deviation = (double*)malloc(sizeof(double) * stream->len);
    dsp_buffer_copy(stream->buf, deviation, stream->len);
    dsp_buffer_deviate(carrier, deviation, hi, lo);
    memcpy(stream->buf, carrier->buf, stream->len * sizeof(double));
    dsp_stream_free(carrier);

}

void dsp_modulation_amplitude(dsp_stream_p stream, double samplefreq, double freq)
{
    dsp_stream_p carrier = dsp_stream_new();
    dsp_signals_sinewave(carrier, samplefreq, freq);
    dsp_buffer_sum(stream, carrier->buf, stream->len);
    dsp_stream_free_buffer(carrier);
    dsp_stream_free(carrier);

}
