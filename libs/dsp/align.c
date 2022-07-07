
/*
*   DSP API - a digital signal processing library for astronomy usage
*   Copyright © 2017-2021  Ilia Platone
*
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU Lesser General Public
*   License as published by the Free Software Foundation; either
*   version 3 of the License, or (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*   Lesser General Public License for more details.
*
*   You should have received a copy of the GNU Lesser General Public License
*   along with this program; if not, write to the Free Software Foundation,
*   Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "dsp.h"
#include <time.h>

int dsp_qsort_double_desc (const void *arg1, const void *arg2)
{
    double* a1 = (double*)arg1;
    double* a2 = (double*)arg2;
    return ((*a1) < (*a2) ? 1 : -1);
}

int dsp_qsort_double_asc (const void *arg1, const void *arg2)
{
    double* a1 = (double*)arg1;
    double* a2 = (double*)arg2;
    return ((*a1) > (*a2) ? 1 : -1);
}

int dsp_qsort_star_diameter_desc(const void *arg1, const void *arg2)
{
    dsp_star* a = (dsp_star*)arg1;
    dsp_star* b = (dsp_star*)arg2;
    if(a->diameter < b->diameter)
        return 1;
    return -1;
}

int dsp_qsort_star_diameter_asc(const void *arg1, const void *arg2)
{
    dsp_star* a = (dsp_star*)arg1;
    dsp_star* b = (dsp_star*)arg2;
    if(a->diameter > b->diameter)
        return 1;
    return -1;
}

static double calc_match_score(dsp_triangle t1, dsp_triangle t2, dsp_align_info align_info)
{
    int d = 0;
    double err = fabs((t1.index - t2.index)/t1.index);
    double size1 = t1.sizes[0];
    double size2 = t2.sizes[0]/align_info.factor[0];
    err += fabs((size1 - size2)/size1);
    for(d = 1; d < align_info.dims; d ++) {
        double size1 = t1.sizes[d-1];
        double size2 = t2.sizes[d-1]/align_info.factor[d-1];
        double x1 = (t1.stars[d].center.location[d-1]-t1.stars[d-1].center.location[d-1]);
        double y1 = (t1.stars[d].center.location[d]-t1.stars[d-1].center.location[d]);
        double _x2 = (t2.stars[d].center.location[d-1]-align_info.offset[d-1]-align_info.center[d-1]);
        double _y2 = (t2.stars[d].center.location[d]-align_info.offset[d]-align_info.center[d]);
        double x2 = _x2*cos(align_info.radians[d-1])+_y2*sin(align_info.radians[d-1]);
        double y2 = _y2*cos(align_info.radians[d-1])-_x2*sin(align_info.radians[d-1]);
        x2 /= align_info.factor[d-1];
        y2 /= align_info.factor[d];
        err += fabs(((x2-x1)+(y2-y1)+(size2-size1))/size1);
    }
    for(d = 1 ; d < t1.dims; d++) {
        err += fabs((t1.ratios[d] - t2.ratios[d])/t1.ratios[d]);
    }
    err /= 2;
    return err / (align_info.dims + t1.dims);
}

dsp_align_info dsp_align_fill_info(dsp_triangle t1, dsp_triangle t2)
{
    dsp_align_info align_info;
    int dims = t1.dims - 1;
    int d, x;
    align_info.dims = dims;
    align_info.center = (double*)malloc(sizeof(double)*(dims));
    align_info.offset = (double*)malloc(sizeof(double)*(dims));
    align_info.radians = (double*)malloc(sizeof(double)*(dims-1));
    align_info.factor = (double*)malloc(sizeof(double)*(dims));
    for(d = 0; d < dims; d++) {
        align_info.factor[d] = 0;
        align_info.offset[d] = 0;
        align_info.center[d] = 0;
        if(d < dims - 1)
            align_info.radians[d] = 0;
    }
    for(d = 0; d < dims; d++) {
        align_info.center[d] = t2.stars[0].center.location[d];
        align_info.offset[d] = t2.stars[0].center.location[d] - t1.stars[0].center.location[d];
        if(d < dims - 1) {
            align_info.radians[d] = t1.theta[d] - t2.theta[d];
            if(align_info.radians[d] >= M_PI*2.0)
                align_info.radians[d] -= M_PI*2.0;
            if(align_info.radians[d] < 0.0)
                align_info.radians[d] += M_PI*2.0;
        }
        for(x = 0; x < t2.dims; x++) {
            align_info.factor[d] += t2.sizes[x] / t1.sizes[x];
        }
        align_info.factor[d] /= t2.dims;
    }
    align_info.score = calc_match_score(t1, t2, align_info);
    return align_info;
}

dsp_triangle dsp_align_calc_triangle(dsp_star* stars)
{
    int x;
    int d;
    dsp_triangle triangle;
    triangle.dims = stars[0].center.dims+1;
    double *delta = (double*)malloc(sizeof(double)*triangle.dims);
    double **diff = (double**)malloc(sizeof(double*)*triangle.dims);
    triangle.sizes = (double*)malloc(sizeof(double)*triangle.dims);
    triangle.ratios = (double*)malloc(sizeof(double)*triangle.dims);
    triangle.stars = (dsp_star*)malloc(sizeof(dsp_star)*triangle.dims);
    triangle.theta = (double*)malloc(sizeof(double)*(triangle.dims-1));
    triangle.index = 0.0;
    qsort(stars, 3, sizeof(dsp_star), dsp_qsort_star_diameter_desc);
    for(d = 0; d < triangle.dims; d++) {
        diff[d] = (double*)malloc(sizeof(double)*stars[d].center.dims);
        for(x = 0; x < triangle.dims-1; x++) {
            diff[d][x] = stars[(d + 1) < triangle.dims ? d : 0].center.location[x]-stars[(d + 1) < triangle.dims ? (d + 1) : (triangle.dims - 1)].center.location[x];
            delta[d] += pow(diff[d][x], 2);
        }
        delta[d] = sqrt(delta[d]);
    }
    for(d = 0; d < triangle.dims; d++) {
        for(x = 0; x < stars[0].center.dims - 1; x++) {
            if(d == 0) {
                triangle.theta[x] = acos(diff[d][x] / delta[d]);
                if(diff[0][x+1] < 0)
                    triangle.theta[x] = -triangle.theta[x];
                if(triangle.theta[x] >= M_PI*2.0)
                    triangle.theta[x] -= M_PI*2.0;
                if(triangle.theta[x] < 0)
                    triangle.theta[x] += M_PI*2.0;
            } else {
                double index = acos(diff[d][x] / delta[d]);
                if(diff[d][x+1] < 0)
                    index = -index;
                index -= triangle.theta[x];
                triangle.index += index;
            }
        }
    }
    while(triangle.index >= M_PI)
        triangle.index -= M_PI*2.0;
    while(triangle.index < -M_PI)
        triangle.index += M_PI*2.0;
    qsort(delta, triangle.dims, sizeof(double), dsp_qsort_double_desc);
    for(d = 0; d < triangle.dims; d++) {
        triangle.stars[d].diameter = stars[d].diameter;
        triangle.stars[d].center.dims = stars[d].center.dims;
        triangle.stars[d].center.location = (double*)malloc(sizeof(double)*stars[d].center.dims);
        triangle.sizes[d] = delta[d];
        triangle.ratios[d] = delta[d] / delta[0];
        for(x = 0; x < triangle.stars[d].center.dims; x++) {
            triangle.stars[d].center.location[x] = stars[d].center.location[x];
        }
        free(diff[d]);
    }
    free(delta);
    free(diff);
    return triangle;
}

int dsp_align_get_offset(dsp_stream_p stream1, dsp_stream_p stream2, double tolerance, double target_score)
{
    dsp_align_info align_info;
    double decimals = pow(10, tolerance);
    double div = 0.0;
    int d, t1, t2, x, y;
    double phi = 0.0;
    int dims = stream1->dims+1;
    double min_score = 1.0;
    double ratio = decimals*1600.0/div;
    dsp_star *stars = (dsp_star*)malloc(sizeof(dsp_star)*dims);
    for(d = 0; d < stream1->dims; d++) {
        div += pow(stream2->sizes[d], 2);
    }
    div = pow(div, 0.5);
    pwarn("decimals: %lf\n", decimals);
    target_score = 1.0-target_score/100.0;
    stream2->align_info.dims = 2;
    stream2->align_info.score = 1.0;
    stream2->align_info.decimals = decimals;
    pgarb("creating triangles for reference frame...\n");
    stream1->align_info.triangles_count = 0;
    for(x = 0; x < stream1->stars_count; x++) {
        for(y = x+1; y < stream1->stars_count-dims+1; y++) {
            for(d = 0; d < dims; d++) {
                stars[d] = stream1->stars[y+d];
            }
            dsp_triangle t = dsp_align_calc_triangle(stars);
            dsp_stream_add_triangle(stream1, t);
        }
    }
    pgarb("creating triangles for current frame...\n");
    stream2->align_info.triangles_count = 0;
    for(x = 0; x < stream2->stars_count; x++) {
        for(y = x; y < stream2->stars_count-dims+1; y++) {
            for(d = 0; d < dims; d++) {
                stars[d] = stream2->stars[y+d];
            }
            dsp_triangle t = dsp_align_calc_triangle(stars);
            dsp_stream_add_triangle(stream2, t);
        }
    }
    free(stars);
    for(t1 = 0; t1 < stream1->triangles_count; t1++) {
        for(t2 = 0; t2 < stream2->triangles_count; t2++) {
            align_info = dsp_align_fill_info(stream1->triangles[t1], stream2->triangles[t2]);
            if(align_info.score < min_score) {
                stream2->align_info = align_info;
                min_score = align_info.score;
            }
        }
    }
    double radians = stream2->align_info.radians[0];
    if(radians > M_PI)
        radians -= M_PI*2;
    for(d = 0; d < stream1->dims; d++) {
        phi += pow(stream2->align_info.offset[d], 2);
    }
    phi = pow(phi, 0.5);
    stream2->align_info.err = 0xf;
    if(min_score < target_score)
        stream2->align_info.err &= ~DSP_ALIGN_NO_MATCH;
    if(fabs(phi * ratio * 10.0) < 1.0)
        stream2->align_info.err &= ~DSP_ALIGN_TRANSLATED;
    if(fabs((stream2->align_info.factor[0] - 1.0) * 100.0) * decimals < 1)
        stream2->align_info.err &= ~DSP_ALIGN_SCALED;
    if(fabs(radians) * decimals * 10.0 < 1)
        stream2->align_info.err &= ~DSP_ALIGN_ROTATED;
    return stream2->align_info.err;
}
