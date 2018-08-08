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
    dspau_t* in = stream->in;
    dspau_t* out = stream->out;
    int len = stream->len;
	int i;
    dspau_t RC = 1.0 / (Frequency * 2.0 * PI);
    dspau_t dt = 1.0 / (SamplingFrequency * 2.0 * PI);
    dspau_t alpha = dt / (RC + dt) / Q;
	out [0] = in [0];
	for(i = 1; i < len; ++i) { 
		out [i] = (out [i - 1] + (alpha * (in [i] - out [i - 1])));
	}
    return out;
}

dspau_t* dspau_filter_highpass(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t Frequency, dspau_t Q)
{
    dspau_t* in = stream->in;
    dspau_t* out = stream->out;
    int len = stream->len;
	int i;
    dspau_t RC = 1.0 / (Frequency * 2.0 * PI);
    dspau_t dt = 1.0 / (SamplingFrequency * 2.0 * PI);
    dspau_t alpha = dt / (RC + dt) / Q;
	out [0] = in [0];
	for(i = 1; i < len; ++i) { 
		out [i] = (in[i] - (out [i - 1] + (alpha * (in [i] - out [i - 1]))));
	}
    return out;
}

dspau_t dspau_filter_single(dspau_t yin, Coefficient *coefficient) {
    dspau_t yout = 0.0;
	coefficient->x0 = coefficient->x1; 
	coefficient->x1 = coefficient->x2; 
	coefficient->x2 = yin;

	coefficient->y0 = coefficient->y1; 
	coefficient->y1 = coefficient->y2; 
	coefficient->y2 = coefficient->d0 * coefficient->x2 - coefficient->d1 * coefficient->x1 + coefficient->d0 * coefficient->x0 + coefficient->d1 * coefficient->y1 - coefficient->d2 * coefficient->y0;

	yout = coefficient->y2;
	return yout;
}

dspau_t* dspau_filter_bandreject(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t Frequency, dspau_t Q) {
    dspau_t* in = stream->in;
    dspau_t* out = stream->out;
    int len = stream->len;
	int x;
    dspau_t wo = 2.0 * PI * Frequency / SamplingFrequency;

    Coefficient coefficient;
	coefficient.e = 1 / (1 + tan (wo / (Q * 2)));
	coefficient.p = cos (wo);
	coefficient.d0 = coefficient.e;
	coefficient.d1 = 2 * coefficient.e * coefficient.p;
	coefficient.d2 = (2 * coefficient.e - 1);
	for(x = 0; x < len; x ++) {
        out [x] = dspau_filter_single (in [x], &coefficient);
	}
    return out;
}

dspau_t* dspau_filter_bandpass(dspau_stream_p stream, dspau_t SamplingFrequency, dspau_t Frequency, dspau_t Q) {
    dspau_t* in = stream->in;
    dspau_t* out = stream->out;
    int len = stream->len;
	int x;
    dspau_t wo = 2.0 * PI * Frequency / SamplingFrequency;

    dspau_t mn, mx;
    dspau_stats_minmidmax(in, len, &mn, &mx);
    Coefficient coefficient;
	coefficient.e = 1 / (1 + tan (wo / (Q * 2)));
	coefficient.p = cos (wo);
	coefficient.d0 = coefficient.e;
	coefficient.d1 = 2 * coefficient.e * coefficient.p;
	coefficient.d2 = (2 * coefficient.e - 1);
	for(x = 0; x < len; x ++) {
		out [x] -= mn;
		out [x] *= 2.0 / (mx - mn);
		out [x] -= 1.0;
        out [x] = in [x] - dspau_filter_single (in [x], &coefficient);
		out [x] += 1.0;
		out [x] *= (mx - mn) / 2.0;
		out [x] += mn;
	}
    return out;
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
