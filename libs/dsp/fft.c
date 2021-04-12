/*
 *   libDSPAU - a digital signal processing library for astronoms usage
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
#include <fftw3.h>

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
    double* dft = (double*)malloc(sizeof(double) * stream->len);
    dsp_complex* out = (dsp_complex*)malloc(sizeof(dsp_complex) * stream->len);
    dsp_buffer_copy(stream->buf, dft, stream->len);
    int *sizes = (int*)malloc(sizeof(int)*stream->dims);
    memcpy (sizes, stream->sizes, sizeof(int)*stream->dims);
    dsp_buffer_reverse(sizes, stream->dims);
    fftw_plan plan = fftw_plan_dft_r2c(stream->dims, sizes, dft, (fftw_complex*)out, FFTW_ESTIMATE);
    fftw_execute(plan);
    fftw_free(plan);
    fftw_free(dft);
    return out;
}

dsp_t* dsp_fourier_idft(dsp_stream_p stream)
{
    dsp_complex* dft = (dsp_complex*)malloc(sizeof(dsp_complex) * stream->len);
    double* out = (double*)malloc(sizeof(double) * stream->len);
    int x;
    for (x=0; x<stream->len; x++) {
        dft[x].real = stream->buf[x];
        dft[x].imaginary = stream->buf[x];
    }
    int *sizes = (int*)malloc(sizeof(int)*stream->dims);
    memcpy (sizes, stream->sizes, sizeof(int)*stream->dims);
    dsp_buffer_reverse(sizes, stream->dims);
    fftw_plan plan = fftw_plan_dft_c2r(stream->dims, sizes, (fftw_complex*)dft, out, FFTW_ESTIMATE);
    fftw_execute(plan);
    dsp_buffer_copy(out, stream->buf, stream->len);
    fftw_free(plan);
    free(dft);
    free(out);
    return stream->buf;
}
