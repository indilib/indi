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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "libdspau.h"

int dspau_bandpasscorrelate(double* in, double* out, int *c, int skip, double Q)
{
	int len = *c;
	int l = (len / 2) - (len % 2);
	double* filtered = (double*)malloc(sizeof(double) * len);
	for(int i = skip; i < l; i++)
	{
		dspau_bandpassfilter(in, filtered, len, len, i, Q);
		out[i - skip] = 0;
		for(int x = 0; x < len; x++)
		{
			out[i - skip] += filtered[x];
		}
	}
	free (filtered);
	*c = (l - skip);
	return 0;
}

int dspau_autocorrelate(double* in, double* out, int *c, int skip)
{
	int len = *c;
	int l = (len / 2) - (len % 2);
	double* x;
	for(int i = skip; i < l; i++) {
		x = in + i;
		out[i - skip] = 0;
		for(int j = 0; j < l; j++) {
			out[i - skip] += in[j] * x[j];
		}
	}
	*c = (l - skip);
	return 0;
}

double dspau_crosscorrelate(double* x, double* in, int len)
{
	double a = 0.0, ax = 0.0, ay = 0.0;
	double meanx = dspau_mean(x, len);
	double meany = dspau_mean(in, len);
	for (int i = 0; i < len; i++) {
		x[i] -= meanx;
		in[i] -= meany;
	}
	for(int i = 0; i < len; i++) {
		a += x[i] * in[i];
		ax += x[i];
		ay += in[i];
	}
	return a / (ax * ay);
}
