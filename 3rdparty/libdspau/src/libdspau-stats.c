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

dspau_t dspau_stats_minmidmax(dspau_t* in, int len, dspau_t* min, dspau_t* max)
{
    int i;
    dspau_t mn = DBL_MAX;
    dspau_t mx = DBL_MIN;
    for(i = 0; i < len; i++) {
        mn = (in[i] < mn ? in[i] : mn);
        mx = (in[i] > mx ? in[i] : mx);
    }
    *min = mn;
    *max = mx;
    return (dspau_t)((mx - mn) / 2.0 + mn);
}

dspau_t dspau_stats_mean(dspau_t* in, int len)
{
	int i;
    dspau_t mean = 0.0;
    dspau_t l = (dspau_t)len;
    for(i = 0; i < len; i++) {
        mean += in[i];
	}
	mean /=  l;
	return mean;
}

int dspau_stats_val_count(dspau_t* in, int len, dspau_t val, dspau_t prec)
{
    int i;
    int count = 0;
    int exp = pow(10, prec);
    for(i = 0; i < len; i++) {
        if(floor(exp * in[i]) == floor(exp * val))
            count ++;
    }
    return count;
}
