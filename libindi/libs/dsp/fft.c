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
#include "fftw3.h"

double dsp_fft_complex_to_magnitude(dsp_complex n)
{
    return sqrt (n.real * n.real + n.imaginary * n.imaginary);
}

double dsp_fft_complex_to_phase(dsp_complex n)
{
    double out = 0;
    if (n.real != 0) {
        out = atan (n.imaginary / n.real);
    }
	return out;
}

double* dsp_fft_complex_array_to_magnitude(dsp_complex* in, int len)
{
    int i;
    double* out = (double*)calloc(sizeof(double), len);
    for(i = 0; i < len; i++) {
        out [i] = dsp_fft_complex_to_magnitude(in [i]);
    }
    return out;
}

double* dsp_fft_complex_array_to_phase(dsp_complex* in, int len)
{
	int i;
    double* out = (double*)calloc(sizeof(double), len);
	for(i = 0; i < len; i++) {
        out [i] = dsp_fft_complex_to_phase(in [i]);
	}
    return out;
}

dsp_complex* dsp_fft_dft(dsp_stream_p stream)
{
    dsp_complex* dft = (dsp_complex*)calloc(sizeof(dsp_complex), stream->len);
    dsp_complex* fft_in = (dsp_complex*)calloc(sizeof(dsp_complex), stream->len);
    for(int x = 0; x < stream->len; x++) {
        fft_in[x].real = stream->buf[x];
        fft_in[x].imaginary = stream->buf[x];
    }
    fftw_plan fft = fftw_plan_dft(stream->dims, stream->sizes, (fftw_complex*)fft_in, (fftw_complex*)dft, -1, FFTW_ESTIMATE);
    fftw_execute(fft);
    fftw_destroy_plan(fft);
    return dft;
}

