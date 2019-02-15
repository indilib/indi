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

double* dsp_fft_spectrum(dsp_stream_p stream, int size)
{
    dsp_complex* dft = dsp_fft_dft(stream);
    double* tmp = dsp_fft_complex_array_to_magnitude(dft, stream->len);
    free(dft);
    double* ret = dsp_buffer_histogram(tmp, stream->len, size);
    free(tmp);
    return ret;
}

double* dsp_fft_shift(double* in, int dims, int* sizes)
{
    if(dims == 0)
        return NULL;
    int total = 1;
    for(int dim = 0; dim < dims; dim++)
        total *= sizes[dim];
    double* o = (double*)calloc(sizeof(double), total);
    int len = 1;
    for(int dim = 0; dim < dims; dim++) {
        len *= sizes[dim];
        for(int y = 0; y < total; y += len) {
            memcpy(&o[y], &in[y + len / 2], sizeof(double) * len / 2);
            memcpy(&o[y + len / 2], &in[y], sizeof(double) * len / 2);
        }
    }
    return o;
}

dsp_complex* dsp_fft_dft(dsp_stream_p stream)
{
    dsp_complex* dft = (dsp_complex*)calloc(sizeof(dsp_complex), stream->len);
    for (stream->index = 0 ; stream->index<stream->len; stream->index++)
    {
        dsp_stream_get_position(stream);
        for(int dim = 0; dim < stream->dims; dim++) {
            for (int n=0 ; n<stream->sizes[dim] ; ++n) dft[stream->index].real += stream->in[stream->index] * cos(n * stream->pos[dim] * M_PI * 2 / stream->sizes[dim]);
            for (int n=0 ; n<stream->sizes[dim] ; ++n) dft[stream->index].imaginary -= stream->in[stream->index] * sin(n * stream->pos[dim] * M_PI * 2 / stream->sizes[dim]);
        }
    }
    return dft;
}

