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

double* dspau_buffer_zerofill(double* out, int len)
{
    int k;
    for(k = 0; k < len; k++)
        out[k] = 0;
    return out;
}

double* dspau_buffer_removemean(double* in, int len)
{
    int k;
    double *out = calloc(len, sizeof(double));
    double mean = dspau_stats_mean(in, len);
    for(k = 0; k < len; k++)
        out[k] = in[k] - mean;
    return out;
}

double* dspau_buffer_stretch(double* in, int len, double min, double max)
{
    double *out = calloc(len, sizeof(double));
    int k;
    double mn, mx;
    dspau_stats_minmidmax(in, len, &mn, &mx);
    double oratio = (max - min);
    double iratio = (mx - mn);
    if(iratio == 0) iratio = 1.0;
	for(k = 0; k < len; k++) {
        out[k] = in[k] - mn;
        out[k] = out[k] * oratio / iratio;
        out[k] += min;
    }
    return out;
}

double* dspau_buffer_normalize(double* in, int len, double min, double max)
{
	int k;
    double *out = calloc(len, sizeof(double));
	for(k = 0; k < len; k++) {
        out[k] = (in[k] < min ? min : (in[k] > max ? max : in[k]));
	}
    return out;
}

double* dspau_buffer_sub(double* in1, int len1, double* in2, int len2)
{
    int len = Min(len1, len2);
    double *out = calloc(len, sizeof(double));
	int k;
	for(k = 0; k < len; k++) {
        out[k] = in1[k] - in2[k];
	}
    return out;
}

double* dspau_buffer_sum(double* in1, int len1, double* in2, int len2)
{
    int len = Min(len1, len2);
    double *out = calloc(len, sizeof(double));
	int k;
	for(k = 0; k < len; k++) {
        out[k] = in1[k] + in2[k];
	}
    return out;
}

double* dspau_buffer_div(double* in1, int len1, double* in2, int len2)
{
    int len = Min(len1, len2);
    double *out = calloc(len, sizeof(double));
	int k;
	for(k = 0; k < len; k++) {
        out[k] = in1[k] / in2[k];
	}
    return out;
}

double* dspau_buffer_mul(double* in1, int len1, double* in2, int len2)
{
    int len = Min(len1, len2);
    double *out = calloc(len, sizeof(double));
	int k;
	for(k = 0; k < len; k++) {
        out[k] = in1[k] * in2[k];
	}
    return out;
}

double* dspau_buffer_1sub(double* in, int len, double val)
{
    int k;
    double *out = calloc(len, sizeof(double));
    for(k = 0; k < len; k++) {
        out[k] = val - in[k];
    }
    return out;
}

double* dspau_buffer_sub1(double* in, int len, double val)
{
    int k;
    double *out = calloc(len, sizeof(double));
    for(k = 0; k < len; k++) {
        out[k] = in[k] - val;
    }
    return out;
}

double* dspau_buffer_sum1(double* in, int len, double val)
{
	int k;
    double *out = calloc(len, sizeof(double));
	for(k = 0; k < len; k++) {
        out[k] = in[k] + val;
	}
    return out;
}

double* dspau_buffer_1div(double* in, int len, double val)
{
    int k;
    double *out = calloc(len, sizeof(double));
    for(k = 0; k < len; k++) {
        out[k] = val / in[k];
    }
    return out;
}

double* dspau_buffer_div1(double* in, int len, double val)
{
    int k;
    double *out = calloc(len, sizeof(double));
    for(k = 0; k < len; k++) {
        out[k] = in[k] / val;
    }
    return out;
}

double* dspau_buffer_mul1(double* in, int len, double val)
{
    int k;
    double *out = calloc(len, sizeof(double));
    for(k = 0; k < len; k++) {
        out[k] = in[k] * val;
    }
    return out;
}

double* dspau_buffer_pow(double* in, int len, double val)
{
    int k;
    double *out = calloc(len, sizeof(double));
    for(k = 0; k < len; k++) {
        out[k] = pow(in[k], val);
    }
    return out;
}

double* dspau_buffer_root(double* in, int len, double val)
{
    int k;
    double *out = calloc(len, sizeof(double));
    for(k = 0; k < len; k++) {
        out[k] = 1.0/pow(in[k], val);
    }
    return out;
}

static int compare( const void* a, const void* b)
{
     double int_a = * ( (double*) a );
     double int_b = * ( (double*) b );

     if ( int_a == int_b ) return 0;
     else if ( int_a < int_b ) return -1;
     else return 1;
}

double* dspau_buffer_median(double* in, int len, int size, int median)
{
    double *out = calloc(len, sizeof(double));
	int k;
    int mid = (size / 2) + (size % 2);
    double* sorted = (double*)malloc(size * sizeof(double));
	for(k = mid; k < len; k++) {
        memcpy (sorted, in + (k - mid), size * sizeof(double));
        qsort(sorted, size, sizeof(double), compare);
		out[k] = sorted[median];
	}
    return out;
}

double* dspau_buffer_histogram(double* in, int len, int size)
{
    int k;
    long* i = calloc(sizeof(long), len);
    double *o = dspau_buffer_stretch(in, len, 0.0, size);
    double *out = calloc(sizeof(double), size);
    dspau_convert(o, i, len);
    dspau_convert(i, o, len);
    for(k = 1; k < size; k++) {
        out[k] = dspau_stats_val_count(o, len, k);
    }
    free(i);
    free(o);
    return out;
}

double* dspau_buffer_deviate(double* in1, int len1, double* in2, int len2, double mindeviation, double maxdeviation)
{
    double *out = calloc(len1, sizeof(double));
    int len = Min(len1, len2);
    in2 = dspau_buffer_stretch(in2, len, mindeviation, maxdeviation);
    in2 = dspau_buffer_val_sum(in2, len);
    for(int k = 1; k < len; k++) {
        out[(int)in2[k]] = in1[k];
    }
    return out;
}

double* dspau_buffer_val_sum(double* in, int len)
{
    double* out = calloc(len, sizeof(double));
    out[0] = in[0];
    for(int i = 1; i < len; i++) {
        out[i] += in[i - 1];
    }
    return out;
}

double dspau_buffer_compare(double* in1, int len1, double* in2, int len2)
{
    double out = 0;
    for(int i = 0; i < Min(len1, len2); i++) {
        out += in1[i] - in2[i];
    }
    return out;
}
