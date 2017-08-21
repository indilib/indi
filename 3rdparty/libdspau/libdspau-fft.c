#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <fftw3.h>
#include "libdspau.h"

double complex_mag(fftw_complex n)
{
	return sqrt (n[0] * n[0] + n[1] * n[1]);
}

double complex_phi(fftw_complex n)
{
	double ret = 0;
	if (n[0] != 0)
		ret = atan (n[1] / n[0]);
	return ret;
}

double * complex2mag(fftw_complex* rawFFT, int len)
{
	double * mag = (double*)malloc(sizeof(double) * len);
	for (unsigned int i = 0; i < len; i++) {
		mag[i] = complex_mag(rawFFT [i]);
	}

	return mag;
}

double * complex2magpow(fftw_complex* rawFFT, int len)
{
	double * magSquared = (double*)malloc(sizeof(double) * len);
	for (unsigned int i = 0; i < len; i++) {
		double mag = complex_mag(rawFFT [i]);
		magSquared [i] = mag * mag;
	}

	return magSquared;
}

double * complex2magsqrt(fftw_complex* rawFFT, int len)
{
	double * mag = (double*)malloc(sizeof(double) * len);
	for (unsigned int i = 0; i < len; i++) {
		mag [i] = sqrt (complex_mag(rawFFT [i]));
	}

	return mag;
}

double * complex2magdbv(fftw_complex* rawFFT, int len)
{
	double * mag = (double*)malloc(sizeof(double) * len);
	for (unsigned int i = 0; i < len; i++) {
		double magVal = complex_mag(rawFFT [i]);

		if (magVal <= 0.0)
			magVal = DBL_EPSILON;

		mag [i] = 20 * log10 (magVal);
	}

	return mag;
}

double * complex2phideg(fftw_complex* rawFFT, int len)
{
	double sf = 180.0 / M_PI; // Degrees per Radian scale factor

	double * phase = (double*)malloc(sizeof(double) * len);
	for (unsigned int i = 0; i < len; i++) {
		phase [i] = complex_phi(rawFFT [i]) * sf;
	}

	return phase;
}

double * complex2phirad(fftw_complex* rawFFT, int len)
{
	double * phase = (double*)malloc(sizeof(double) * len);
	for (unsigned int i = 0; i < len; i++) {
		phase [i] = complex_phi(rawFFT [i]);
	}

	return phase;
}

double * dspau_spectrum(double * data, double bandwidth, int *l, int conversion)
{
	int i = 0;
	int len = *l;
	fftw_complex *in, *out;
	fftw_plan p;
	in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * len);
	for(i = 0; i < len; i++) {
		in[i][0] = data[i];
		in[i][1] = 0;
	}
	out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * len);
	p = fftw_plan_dft_1d(len, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_execute(p);
	double * spectrum = NULL;
	switch (conversion) {
	case magnitude:
		spectrum = complex2mag(out, len);
		break;
	case magnitude_dbv:
		spectrum = complex2magdbv(out, len);
		break;
	case magnitude_rooted:
		spectrum = complex2magsqrt(out, len);
		break;
	case magnitude_squared:
		spectrum = complex2magpow(out, len);
		break;
	case phase_degrees:
		spectrum = complex2phideg(out, len);
		break;
	case phase_radians:
		spectrum = complex2phirad(out, len);
		break;
	default:
		return NULL;
	}
	fftw_destroy_plan(p);
	fftw_free(in);
	fftw_free(out);
	*l /= 2;
	return spectrum;
}
