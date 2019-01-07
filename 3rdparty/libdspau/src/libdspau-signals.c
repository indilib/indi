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

dspau_t* dspau_signals_sinewave(int len, dspau_t samplefreq, dspau_t freq, dspau_t max)
{
    dspau_t* out = (dspau_t*)calloc(sizeof(dspau_t), len);
    int k;
    for(k = 0; k < len; k++)
        out[k] = sin((freq / samplefreq) * PI * 2 * k) * max;
    return out;
}

dspau_t* dspau_signals_sawteethwave(int len, dspau_t samplefreq, dspau_t freq, dspau_t max)
{
    dspau_t* out = (dspau_t*)calloc(sizeof(dspau_t), len);
    int k;
    for(k = 0; k < len; k++) {
        dspau_t value = ((freq / samplefreq) * k);
        while (value > max)
            value -= max;
        out[k] = value;
    }
    return out;
}

dspau_t* dspau_signals_triwave(int len, dspau_t samplefreq, dspau_t freq, dspau_t max)
{
    dspau_t* out = (dspau_t*)calloc(sizeof(dspau_t), len);
    int k;
    for(k = 0; k < len; k++) {
        dspau_t value = ((freq / samplefreq) * k);
        while (value > max * 2)
            value -= max * 2;
        value = value > max ? max - value : value;
        out[k] = value;
    }
    return out;
}

dspau_t* dspau_modulation_frequency(dspau_t* in, int len, dspau_t samplefreq, dspau_t freq, dspau_t bandwidth)
{
    dspau_t* carrying = dspau_signals_sinewave(len, samplefreq, freq, 1.0);
    dspau_t lo = freq / samplefreq;
    dspau_t hi = freq / samplefreq;
    return dspau_buffer_deviate(carrying, len, in, len, lo - bandwidth * 0.5, hi + bandwidth * 1.5);
}

dspau_t* dspau_modulation_amplitude(dspau_t* in, int len, dspau_t samplefreq, dspau_t freq)
{
    dspau_t* modulating = (dspau_t*)calloc(sizeof(dspau_t), len);
    dspau_t* out = (dspau_t*)calloc(sizeof(dspau_t), len);
    in = dspau_signals_sinewave(len, samplefreq, freq, 0.5);
    out = dspau_buffer_sum(modulating, len, in, len);
    return out;
}

dspau_t* dspau_modulation_buffer(dspau_t* in1, int len1, dspau_t* in2, int len2, dspau_t samplefreq, dspau_t freq)
{
    int len = Min(len1, len2);
    int olen = Max(len1, len2);
    dspau_t* buf1 = (len == len1 ? in1 : in2);
    dspau_t* buf2 = (olen == len1 ? in1 : in2);
    dspau_t* out = (dspau_t*)calloc(sizeof(dspau_t), olen);
    freq /= samplefreq;
    for(int i = 0; i < olen; i+= len) {
        dspau_t* tmp = dspau_buffer_sum(&buf2[i], len, buf1, len);
        memcpy(&out[i], tmp, len);
    }
    return out;
}
