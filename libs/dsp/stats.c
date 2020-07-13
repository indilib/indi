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

double* dsp_stats_histogram(dsp_stream_p stream, int size)
{
    int k;
    double* out = (double*)malloc(sizeof(double)*size);
    double mx = dsp_stats_max(stream->buf, stream->len);
    double mn = dsp_stats_min(stream->buf, stream->len);
    for(k = 1; k < size; k++) {
        out[k] = dsp_stats_range_count(stream->buf, stream->len, mn + (k - 1) * (mx - mn / size), mn + k * (mx - mn / size));
    }
    return out;
}

