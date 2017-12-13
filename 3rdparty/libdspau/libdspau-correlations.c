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
	int i, x;
	int len = *c;
	int l = (len / 2) - (len % 2);
	double* filtered = (double*)malloc(sizeof(double) * len);
	for(i = skip; i < l; i++)
	{
		dspau_bandpassfilter(in, filtered, len, len, i, Q);
		out[i - skip] = 0;
		for(x = 0; x < len; x++)
		{
			out[i - skip] += filtered[x];
		}
	}
	free (filtered);
	*c = (l - skip);
	return 0;
}

int dspau_autocorrelate(double* in, double* out, int *c)
{
	int i, j;
	int len = *c;
	*c = (len / 2) - (len % 2);
	for(i = 0; i < *c; i++) {
		double x1 = 0;
		double x2 = 0;
		double y = 0;
		for(j = 0; j < *c; j++) {
			y += in[j] * in[j + i];
			x1 += in[j];
			x2 += in[j + i];
		}
		out[i] = y / (x1 * x2);
	}
	return 0;
}

double dspau_crosscorrelate(double* in1, double* in2, double* out, int len1, int len2, int *len)
{
	int i, j;
	int l1 = max(len1, len2);
	int l2 = min(len1, len2);
	*len = l1 - l2;
	for(i = 0; i < *len; i++) {
		double y = 0;
		double x1 = 0;
		double x2 = 0;
		for(j = 0; j < l2; j++) {
			if(len1 > len2) {
				y += in1[i] * in2[j];
				x1 += in1[i];
				x2 += in2[j];
			} else {
				y += in1[j] * in2[i];
				x1 += in1[j];
				x2 += in2[i];
			}
		}
		out[i] = y / (x1 * x2);
	}
	return 0;
}
