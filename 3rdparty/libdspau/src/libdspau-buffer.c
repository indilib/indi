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

dspau_t* dspau_buffer_zerofill(dspau_t* out, int len)
{
    dspau_t *in = calloc(len, sizeof(dspau_t));
    memcpy(out, in, len * sizeof(dspau_t));
    free(in);
    return out;
}

dspau_t* dspau_buffer_removemean(dspau_t* in, int len)
{
    int k;
    dspau_t mean = dspau_stats_mean(in, len);
    for(k = 0; k < len; k++)
        in[k] = in[k] - mean;
    return in;
}

dspau_t* dspau_buffer_stretch(dspau_t* in, int len, dspau_t min, dspau_t max)
{
    int k;
    dspau_t mn, mx;
    dspau_stats_minmidmax(in, len, &mn, &mx);
	for(k = 0; k < len; k++) {
        in[k] = in[k] - mn;
        in[k] = in[k] * ((max - min) / (mx - mn + 1));
        in[k] += min;
    }
    return in;
}

dspau_t* dspau_buffer_normalize(dspau_t* in, int len, dspau_t min, dspau_t max)
{
	int k;
	for(k = 0; k < len; k++) {
        in[k] = (in[k] < min ? min : (in[k] > max ? max : in[k]));
	}
    return in;
}

dspau_t* dspau_buffer_sub(dspau_t* in1, int len1, dspau_t* in2, int len2)
{
    int len = Min(len1, len2);
	int k;
	for(k = 0; k < len; k++) {
        in1[k] = in1[k] - in2[k];
	}
    return in1;
}

dspau_t* dspau_buffer_sum(dspau_t* in1, int len1, dspau_t* in2, int len2)
{
    int len = Min(len1, len2);
	int k;
	for(k = 0; k < len; k++) {
        in1[k] = in1[k] + in2[k];
	}
    return in1;
}

dspau_t* dspau_buffer_div(dspau_t* in1, int len1, dspau_t* in2, int len2)
{
    int len = Min(len1, len2);
	int k;
	for(k = 0; k < len; k++) {
        in1[k] = in1[k] / in2[k];
	}
    return in1;
}

dspau_t* dspau_buffer_mul(dspau_t* in1, int len1, dspau_t* in2, int len2)
{
    int len = Min(len1, len2);
	int k;
	for(k = 0; k < len; k++) {
        in1[k] = in1[k] * in2[k];
	}
    return in1;
}

dspau_t* dspau_buffer_1sub(dspau_t* in, int len, dspau_t val)
{
    int k;
    for(k = 0; k < len; k++) {
        in[k] = val - in[k];
    }
    return in;
}

dspau_t* dspau_buffer_sub1(dspau_t* in, int len, dspau_t val)
{
    int k;
    for(k = 0; k < len; k++) {
        in[k] = in[k] - val;
    }
    return in;
}

dspau_t* dspau_buffer_sum1(dspau_t* in, int len, dspau_t val)
{
	int k;
	for(k = 0; k < len; k++) {
        in[k] = in[k] + val;
	}
    return in;
}

dspau_t* dspau_buffer_1div(dspau_t* in, int len, dspau_t val)
{
    int k;
    for(k = 0; k < len; k++) {
        in[k] = val / in[k];
    }
    return in;
}

dspau_t* dspau_buffer_div1(dspau_t* in, int len, dspau_t val)
{
    int k;
    for(k = 0; k < len; k++) {
        in[k] = in[k] / val;
    }
    return in;
}

dspau_t* dspau_buffer_mul1(dspau_t* in, int len, dspau_t val)
{
    int k;
    for(k = 0; k < len; k++) {
        in[k] = in[k] * val;
    }
    return in;
}

dspau_t* dspau_buffer_pow(dspau_t* in, int len, dspau_t val)
{
    int k;
    for(k = 0; k < len; k++) {
        in[k] = pow(in[k], val);
    }
    return in;
}

dspau_t* dspau_buffer_root(dspau_t* in, int len, dspau_t val)
{
    int k;
    for(k = 0; k < len; k++) {
        in[k] = sqrt(in[k]);
    }
    return in;
}

static int compare( const void* a, const void* b)
{
     dspau_t int_a = * ( (dspau_t*) a );
     dspau_t int_b = * ( (dspau_t*) b );

     if ( int_a == int_b ) return 0;
     else if ( int_a < int_b ) return -1;
     else return 1;
}

dspau_t* dspau_buffer_median(dspau_t* in, int len, int size, int median)
{
    dspau_t *out = calloc(len, sizeof(dspau_t));
	int k;
    int mid = (size / 2) + (size % 2);
    dspau_t* sorted = (dspau_t*)malloc(size * sizeof(dspau_t));
	for(k = mid; k < len; k++) {
        memcpy (sorted, in + (k - mid), size * sizeof(dspau_t));
        qsort(sorted, size, sizeof(dspau_t), compare);
		out[k] = sorted[median];
	}
    return out;
}

dspau_t* dspau_buffer_histogram(dspau_t* in, int len, int size)
{
    dspau_t *out = calloc(size, sizeof(dspau_t));
    int k;
    dspau_t *tmp = dspau_buffer_stretch(in, len, 0, size);
    for(k = 0; k < size; k++) {
        out[k] = dspau_stats_val_count(tmp, len, k, 0);
    }
    free (tmp);
    return out;
}

dspau_t* dspau_buffer_deviate(dspau_t* in1, int len1, dspau_t* in2, int len2, dspau_t mindeviation, dspau_t maxdeviation)
{
    dspau_t *out = calloc(len1, sizeof(dspau_t));
    int len = Min(len1, len2);
    in2 = dspau_buffer_stretch(in2, len, mindeviation, maxdeviation);
    in2 = dspau_buffer_val_sum(in2, len);
    for(int k = 1; k < len; k++) {
        out[(int)in2[k]] = in1[k];
    }
    return out;
}

dspau_t* dspau_buffer_convolute(dspau_t* in1, int len1, dspau_t* in2, int len2)
{
    dspau_t *out = calloc(len1, sizeof(dspau_t));
    in2 = dspau_buffer_val_sum(in2, len2);
    in2 = dspau_buffer_stretch(in2, len2, 0, 1.0 / in2[len2 - 1]);
    for(int k = 0; k < len1; k++) {
        for(int l = 0; l < len2; l++) {
            if(l - l / 2 >= 0 && l - l / 2 < len1) {
                out[k] += in1[k] * in2[k + l - l / 2];
            }
        }
    }
    return out;
}

dspau_t* dspau_buffer_val_sum(dspau_t* in, int len)
{
    dspau_t* out = calloc(len, sizeof(dspau_t));
    out[0] = in[0];
    for(int i = 1; i < len; i++) {
        out[i] += in[i - 1];
    }
    return out;
}
