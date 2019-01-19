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

double* dspau_signals_sinewave(int len, double samplefreq, double freq)
{
    double* out = (double*)calloc(sizeof(double), len);
    freq /= samplefreq;
    double rad = 0;
    double x = 0;
    for(int k = 0; k < len; k++) {
        rad += freq;
        x = rad;
        while (x > 1.0)
            x -= 1.0;
        x *= PI * 2;
        out[k] = sin(x);
    }
    return out;
}

double* dspau_signals_sawteethwave(int len, double samplefreq, double freq)
{
    double* out = (double*)calloc(sizeof(double), len);
    freq /= samplefreq;
    double rad = 0;
    double x = 0;
    for(int k = 0; k < len; k++) {
        rad += freq;
        x = rad;
        while (x > 1.0)
            x -= 1.0;
        out[k] = x;
    }
    return out;
}

double* dspau_signals_triwave(int len, double samplefreq, double freq)
{
    double* out = (double*)calloc(sizeof(double), len);
    freq /= samplefreq;
    double rad = 0;
    double x = 0;
    for(int k = 0; k < len; k++) {
        rad += freq;
        x = rad;
        while (x > 2.0)
            x -= 2.0;
        while (x > 1.0)
            x = 2.0 - x;
        out[k] = x;
    }
    return out;
}

double* dspau_modulation_frequency(double* in, int len, double samplefreq, double freq, double bandwidth)
{
    double* carrying = dspau_signals_sinewave(len, samplefreq, freq);
    double lo = freq / samplefreq;
    double hi = freq / samplefreq;
    return dspau_buffer_deviate(carrying, len, in, len, lo - bandwidth * 0.5, hi + bandwidth * 1.5);
}

double* dspau_modulation_amplitude(double* in, int len, double samplefreq, double freq)
{
    double* modulating = (double*)calloc(sizeof(double), len);
    double* out = (double*)calloc(sizeof(double), len);
    in = dspau_signals_sinewave(len, samplefreq, freq);
    out = dspau_buffer_sum(modulating, len, in, len);
    return out;
}

double* dspau_modulation_buffer(double* in1, int len1, double* in2, int len2, double samplefreq, double freq)
{
    int len = Min(len1, len2);
    int olen = Max(len1, len2);
    double* buf1 = (len == len1 ? in1 : in2);
    double* buf2 = (olen == len1 ? in1 : in2);
    double* out = (double*)calloc(sizeof(double), olen);
    freq /= samplefreq;
    for(int i = 0; i < olen; i+= len) {
        double* tmp = dspau_buffer_sum(&buf2[i], len, buf1, len);
        memcpy(&out[i], tmp, len);
    }
    return out;
}
