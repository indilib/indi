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

double dsp_fourier_complex_get_magnitude(dsp_complex n)
{
    return sqrt (n.real * n.real + n.imaginary * n.imaginary);
}

double dsp_fourier_complex_get_phase(dsp_complex n)
{
    double out = 0;
    if (n.real != 0) {
        out = atan (n.imaginary / n.real);
    }
    return out;
}

double* dsp_fourier_complex_array_get_magnitude(dsp_complex* in, int len)
{
    int i;
    double* out = (double*)malloc(sizeof(double) * len);
    for(i = 0; i < len; i++) {
        out [i] = (double)dsp_fourier_complex_get_magnitude(in [i]);
    }
    return out;
}

double* dsp_fourier_complex_array_get_phase(dsp_complex* in, int len)
{
    int i;
    double* out = (double*)malloc(sizeof(double) * len);
    for(i = 0; i < len; i++) {
        out [i] = (double)dsp_fourier_complex_get_phase(in [i]);
    }
    return out;
}

dsp_complex* dsp_fourier_dft(dsp_stream_p stream)
{
    dsp_complex* dft = (dsp_complex*)malloc(sizeof(dsp_complex) * stream->len);
    for(int x = 0; x < stream->len; x++) {
        dft[x].real = 0;
        dft[x].imaginary = 0;
    }
    int dim = 0;
    while (dim++ < stream->dims) {
        int size = (dim < 1 ? 1 : stream->sizes[dim-1]);
        for(int i = size; i < stream->len; i+=size) {
            for(int l = size; l < stream->len; l+=size) {
                double k = (double)i / stream->len * (double)l / stream->len * M_PI * 2.0;
                dft[i].real += sin(k) * stream->buf[l];
                dft[i].imaginary += cos(k) * stream->buf[l];
            }
        }
    }
    return dft;
}

void dsp_fourier_dft_magnitude(dsp_stream_p stream)
{
    dsp_t mn = dsp_stats_min(stream->buf, stream->len);
    dsp_t mx = dsp_stats_min(stream->buf, stream->len);
    (void)mn;
    (void)mx;
    dsp_complex* dft = dsp_fourier_dft(stream);
    double* mag = dsp_fourier_complex_array_get_magnitude(dft, stream->len);
    dsp_buffer_stretch(mag, stream->len, mn, mx);
    free(dft);
    dsp_buffer_copy(mag, stream->buf, stream->len);
    free(mag);
}

void dsp_fourier_dft_phase(dsp_stream_p stream)
{
    dsp_t mn = dsp_stats_min(stream->buf, stream->len);
    dsp_t mx = dsp_stats_min(stream->buf, stream->len);
    (void)mn;
    (void)mx;
    dsp_complex* dft = dsp_fourier_dft(stream);
    double* phi = dsp_fourier_complex_array_get_phase(dft, stream->len);
    dsp_buffer_stretch(phi, stream->len, mn, mx);
    free(dft);
    dsp_buffer_copy(phi, stream->buf, stream->len);
    free(phi);
}
