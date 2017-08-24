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

double dspau_minmidmax(double* in, int len, double *min, double *max)
{
	double mn = DBL_MAX;
	double mx = DBL_MIN;
	for(int i = 0; i < len; i++) {
		mn = (in[i] < mn ? in[i] : mn);
		mx = (in[i] > mx ? in[i] : mx);
	}
	*min = mn;
	*max = mx;
	return (double)((mx - mn) / 2.0 + mn);
}

double dspau_mean(double* in, int len)
{
	double mean = 0.0;
	double l = (double)len;
	for(int i = 0; i < len; i++) {
		mean += in[i];
	}
	mean /=  l;
	return mean;
}

int dspau_removemean(double* in, double* out, int len)
{
	double mean = dspau_mean(out, len);
	for(int k = 0; k < len; k++)
		out[k] = in[k] - mean;
	return 0;
}

int dspau_stretch(double* in, double* out, int len, int min, int max)
{
	double mn, mx;
	dspau_minmidmax(in, len, &mn, &mx);
	for(int k = 0; k < len; k++) {
		out[k] -= mn;
		out[k] *= (max - min) / (mx - mn);
		out[k] += min;
	}
	return 0;
}

int dspau_normalize(double* in, double* out, int len, int min, int max)
{
	for(int k = 0; k < len; k++) {
		out[k] = (in[k] < min ? min : (in[k] > max ? max : in[k]));
	}
	return 0;
}

int dspau_u8todouble(unsigned char* in, double* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (double)in[k];
	}
	return 0;
}

int dspau_u16todouble(unsigned short int* in, double* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (double)in[k];
	}
	return 0;
}

int dspau_u32todouble(unsigned int* in, double* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (double)in[k];
	}
	return 0;
}

int dspau_u64todouble(unsigned long int* in, double* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (double)in[k];
	}
	return 0;
}

int dspau_s8todouble(signed char* in, double* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (double)in[k];
	}
	return 0;
}

int dspau_s16todouble(signed short int* in, double* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (double)in[k];
	}
	return 0;
}

int dspau_s32todouble(signed int* in, double* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (double)in[k];
	}
	return 0;
}

int dspau_s64todouble(signed long int* in, double* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (double)in[k];
	}
	return 0;
}

int dspau_doubletou8(double* in, unsigned char* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (unsigned char)in[k];
	}
	return 0;
}

int dspau_doubletou16(double* in, unsigned short int* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (unsigned short int)in[k];
	}
	return 0;
}

int dspau_doubletou32(double* in, unsigned int* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (unsigned int)in[k];
	}
	return 0;
}

int dspau_doubletou64(double* in, unsigned long int* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (unsigned long int)in[k];
	}
	return 0;
}

int dspau_doubletos8(double* in, signed char* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (signed char)in[k];
	}
	return 0;
}

int dspau_doubletos16(double* in, signed short int* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (signed short int)in[k];
	}
	return 0;
}

int dspau_doubletos32(double* in, signed int* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (signed int)in[k];
	}
	return 0;
}

int dspau_doubletos64(double* in, signed long int* out, int len)
{
	for(int k = 0; k < len; k++) {
		out[k] = (signed long int)in[k];
	}
	return 0;
}

