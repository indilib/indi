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

double dspau_stats_minmidmax(double* in, int len, double* min, double* max)
{
    int i;
    *min = DBL_MAX;
    *max = DBL_MIN;
    for(i = 0; i < len; i++) {
        *min = Min(in[i], *min);
        *max = Max(in[i], *max);
    }
    return (double)((*max - *min) / 2.0 + *min);
}

double dspau_stats_mean(double* in, int len)
{
    int i;
    double mean = 0.0;
    double l = (double)len;
    for(i = 0; i < len; i++) {
        mean += in[i];
    }
    mean /=  l;
    return mean;
}

int dspau_stats_maximum_index(double* in, int len)
{
    int i;
    double min, max;
    dspau_stats_minmidmax(in, len, &min, &max);
    for(i = 0; i < len; i++) {
        if(in[i] == max) break;
    }
    return i;
}

int dspau_stats_minimum_index(double* in, int len)
{
    int i;
    double min, max;
    dspau_stats_minmidmax(in, len, &min, &max);
    for(i = 0; i < len; i++) {
        if(in[i] == min) break;
    }
    return i;
}

int dspau_stats_val_count(double* in, int len, double val)
{
    int x;
    int count = 0;
    for(x = 0; x < len; x++) {
        if(in[x] == val)
            count ++;
    }
    return count;
}
