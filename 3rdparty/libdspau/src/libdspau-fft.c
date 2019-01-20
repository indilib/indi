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

static double complex_mag(fftw_complex n)
{
	return sqrt (n[0] * n[0] + n[1] * n[1]);
}

static double complex_phi(fftw_complex n)
{
    double out = 0;
	if (n[0] != 0)
		out = atan (n[1] / n[0]);
	return out;
}

static void complex2mag(fftw_complex* in, double* out, int len)
{
	int i;
	for(i = 0; i < len; i++) {
		out [i] = complex_mag(in [i]);
	}
}

static void complex2magpow(fftw_complex* in, double* out, int len)
{
	int i;
	for(i = 0; i < len; i++) {
		out [i] = pow(complex_mag(in [i]), 2);
	}
}

static void complex2magsqrt(fftw_complex* in, double* out, int len)
{
	int i;
	for(i = 0; i < len; i++) {
        out [i] = sqrt (complex_mag(in [i]));
	}
}

static void complex2magdbv(fftw_complex* in, double* out, int len)
{
	int i;
	for(i = 0; i < len; i++) {
        double magVal = complex_mag(in [i]);

		if (magVal <= 0.0)
			magVal = DBL_EPSILON;

		out [i] = 20 * log10 (magVal);
	}
}

static void complex2phideg(fftw_complex* in, double* out, int len)
{
	int i;
    double sf = 180.0 / PI;
	for(i = 0; i < len; i++) {
		out [i] = complex_phi(in [i]) * sf;
	}
}

static void complex2phirad(fftw_complex* in, double* out, int len)
{
	int i;
	for(i = 0; i < len; i++) {
		out [i] = complex_phi(in [i]);
	}
}

double* dspau_fft_spectrum(dspau_stream_p stream, int conversion, int size)
{
    double* ret;
    double* out = dspau_fft_dft(stream, -1, conversion);
    ret = dspau_buffer_histogram(out, stream->len, size);
    free(out);
    return ret;
}

double* dspau_fft_shift(double* in, int dims, int* sizes)
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

double* dspau_fft_dft(dspau_stream_p stream, int sign, int conversion)
{
    fftw_plan p;
    int len = stream->len;
    dspau_stream_add_dim(stream, 1);
    int* sizes = calloc(sizeof(int),stream->dims);
    int dims = stream->dims;
    dspau_buffer_reverse(sizes, stream->sizes, stream->dims);
    double* out = (double*)calloc(sizeof(double), stream->len);
    fftw_complex *fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * stream->len);
    fftw_complex *fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * stream->len);
    for(int i = 0; i < len; i++) {
        fft_in[i][0] = stream->in[i];
        fft_in[i][1] = 0;
    }
    p = fftw_plan_dft(dims, sizes, fft_in, fft_out, sign, FFTW_ESTIMATE);
    fftw_execute(p);
	switch (conversion) {
	case magnitude:
        complex2mag(fft_out, out, len);
		break;
	case magnitude_dbv:
        complex2magdbv(fft_out, out, len);
		break;
    case magnitude_root:
        complex2magsqrt(fft_out, out, len);
		break;
    case magnitude_square:
        complex2magpow(fft_out, out, len);
		break;
	case phase_degrees:
        complex2phideg(fft_out, out, len);
		break;
	case phase_radians:
        complex2phirad(fft_out, out, len);
		break;
    default:
		break;
    }
	fftw_destroy_plan(p);
	fftw_free(fft_in);
    fftw_free(fft_out);
    return out;
}

