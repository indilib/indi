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

typedef struct {
    double delta;
    double *diff;
} delta_diff;

static int dsp_qsort_delta_diff_desc (const void *arg1, const void *arg2)
{
    delta_diff* a1 = (delta_diff*)arg1;
    delta_diff* a2 = (delta_diff*)arg2;
    return ((*a1).delta < (*a2).delta ? 1 : -1);
}

static int dsp_qsort_delta_diff_asc (const void *arg1, const void *arg2)
{
    delta_diff* a1 = (delta_diff*)arg1;
    delta_diff* a2 = (delta_diff*)arg2;
    return ((*a1).delta > (*a2).delta ? 1 : -1);
}

static int dsp_qsort_triangle_delta_desc(const void *arg1, const void *arg2)
{
    dsp_triangle* a = (dsp_triangle*)arg1;
    dsp_triangle* b = (dsp_triangle*)arg2;
    int ret = 0;
    int s = 0;
    for(s = 0; s < (*a).stars_count; s++)
        ret |= ((*a).sizes[s] < (*b).sizes[s]);
    return (ret ? 1 : -1);
}

static int dsp_qsort_triangle_delta_asc(const void *arg1, const void *arg2)
{
    dsp_triangle* a = (dsp_triangle*)arg1;
    dsp_triangle* b = (dsp_triangle*)arg2;
    int ret = 0;
    int s = 0;
    for(s = 0; s < (*a).stars_count; s++)
        ret |= ((*a).sizes[s] > (*b).sizes[s]);
    return (ret ? 1 : -1);
}

static int dsp_qsort_triangle_diameter_desc(const void *arg1, const void *arg2)
{
    dsp_triangle* a = (dsp_triangle*)arg1;
    dsp_triangle* b = (dsp_triangle*)arg2;
    int ret = 0;
    int s = 0;
    for(s = 0; s < (*a).stars_count; s++) {
        ret |= ((*a).stars[s].diameter < (*b).stars[s].diameter);
        ret |= ((*a).stars[s].peak < (*b).stars[s].peak);
        ret |= ((*a).stars[s].flux < (*b).stars[s].flux);
    }
    return (ret ? 1 : -1);
}

static int dsp_qsort_triangle_diameter_asc(const void *arg1, const void *arg2)
{
    dsp_triangle* a = (dsp_triangle*)arg1;
    dsp_triangle* b = (dsp_triangle*)arg2;
    int ret = 0;
    int s = 0;
    for(s = 0; s < (*a).stars_count; s++) {
        ret |= ((*a).stars[s].diameter > (*b).stars[s].diameter);
        ret |= ((*a).stars[s].peak > (*b).stars[s].peak);
        ret |= ((*a).stars[s].flux > (*b).stars[s].flux);
    }
    return (ret ? 1 : -1);
}

static int dsp_qsort_star_diameter_desc(const void *arg1, const void *arg2)
{
    dsp_star* a = (dsp_star*)arg1;
    dsp_star* b = (dsp_star*)arg2;
    return ((*a).diameter < (*b).diameter ? 1 : -1);
}

static int dsp_qsort_star_diameter_asc(const void *arg1, const void *arg2)
{
    dsp_star* a = (dsp_star*)arg1;
    dsp_star* b = (dsp_star*)arg2;
    return ((*a).diameter > (*b).diameter ? 1 : -1);
}

static int dsp_qsort_double_desc (const void *arg1, const void *arg2)
{
    double* a = (double*)arg1;
    double* b = (double*)arg2;
    return ((*a) < (*b) ? 1 : -1);
}

static int dsp_qsort_double_asc (const void *arg1, const void *arg2)
{
    double* a = (double*)arg1;
    double* b = (double*)arg2;
    return ((*a) > (*b) ? 1 : -1);
}

static double calc_match_score(dsp_triangle t1, dsp_triangle t2, dsp_align_info align_info)
{
    int x = 0;
    int d = 0;
    int div = 0;
    int stars_count = fmin(t1.stars_count, t2.stars_count);
    int num_baselines = stars_count*(stars_count-1)/2;
    double err = 0.0;
    err += fabs((t2.index-t1.index)/t1.index);
    div++;
    for(d = 0; d < align_info.dims; d++) {
        for(x = 0; x < stars_count; x++) {
            err += fabs(t2.stars[x].diameter/align_info.factor[d]-t1.stars[x].diameter)/t1.stars[x].diameter;
            div++;
            err += fabs(t2.stars[x].flux/align_info.factor[d]-t1.stars[x].flux)/t1.stars[x].flux;
            div++;
            err += fabs(t2.stars[x].peak/align_info.factor[d]-t1.stars[x].peak)/t1.stars[x].peak;
            div++;
        }
        for(x = 0; x < num_baselines; x++) {
            err += fabs(t2.sizes[x]/align_info.factor[d]-t1.sizes[x])/t1.sizes[x];
            div++;
        }
    }
    return err / div;
}

dsp_align_info *dsp_align_fill_info(dsp_triangle t1, dsp_triangle t2)
{
    dsp_align_info *align_info = (dsp_align_info*)malloc(sizeof(dsp_align_info));
    int dims = t1.dims;
    int d, x;
    int num_baselines = t1.stars_count*(t1.stars_count-1)/2;
    align_info->dims = dims;
    align_info->center = (double*)malloc(sizeof(double)*(dims));
    align_info->offset = (double*)malloc(sizeof(double)*(dims));
    align_info->radians = (double*)malloc(sizeof(double)*(dims-1));
    align_info->factor = (double*)malloc(sizeof(double)*(dims));
    for(d = 0; d < dims; d++) {
        align_info->factor[d] = 0;
        align_info->offset[d] = 0;
        align_info->center[d] = 0;
        if(d < dims - 1)
            align_info->radians[d] = 0;
    }
    for(d = 0; d < dims; d++) {
        align_info->center[d] = t2.stars[0].center.location[d];
        align_info->offset[d] = t2.stars[0].center.location[d] - t1.stars[0].center.location[d];
        if(d < dims - 1) {
            align_info->radians[d] = t1.theta[d] - t2.theta[d];
            if(align_info->radians[d] >= M_PI*2.0)
                align_info->radians[d] -= M_PI*2.0;
            if(align_info->radians[d] < 0.0)
                align_info->radians[d] += M_PI*2.0;
        }
        for(x = 0; x < num_baselines; x++) {
            align_info->factor[d] += t2.sizes[x] / t1.sizes[x];
        }
        align_info->factor[d] /= num_baselines;
    }
    align_info->score = calc_match_score(t1, t2, *align_info);
    return align_info;
}

void dsp_align_free_triangle(dsp_triangle *t)
{
    int d;
    free(t->sizes);
    free(t->theta);
    free(t->ratios);
    for(d = 0; d < t->dims; d++) {
        free(t->stars[d].center.location);
    }
    free(t->stars);
    free(t);
}

dsp_triangle *dsp_align_calc_triangle(dsp_star* stars, int num_stars)
{
    int x;
    int y;
    int d;
    dsp_triangle *triangle = (dsp_triangle*)malloc(sizeof(dsp_triangle));
    triangle->dims = stars[0].center.dims;
    triangle->stars_count = num_stars;
    int num_baselines = triangle->stars_count*(triangle->stars_count-1)/2;
    delta_diff *deltadiff = (delta_diff*)malloc(sizeof(delta_diff)*num_baselines);
    triangle->sizes = (double*)malloc(sizeof(double)*num_baselines);
    triangle->ratios = (double*)malloc(sizeof(double)*num_baselines);
    triangle->stars = (dsp_star*)malloc(sizeof(dsp_star)*triangle->stars_count);
    triangle->theta = (double*)malloc(sizeof(double)*(triangle->dims-1));
    triangle->index = 0.0;
    qsort(stars, triangle->stars_count, sizeof(dsp_star), dsp_qsort_star_diameter_desc);
    int idx = 0;
    for(x = 0; x < triangle->stars_count; x++) {
        for(y = x+1; y < triangle->stars_count; y++) {
            deltadiff[idx].diff = (double*)malloc(sizeof(double)*triangle->dims);
            for(d = 0; d < triangle->dims; d++) {
                deltadiff[idx].diff[d] = stars[x].center.location[d]-stars[y].center.location[d];
                deltadiff[idx].delta += pow(deltadiff[idx].diff[d], 2);
            }
            deltadiff[idx].delta = sqrt(deltadiff[idx].delta);
            idx++;
        }
    }
    qsort(deltadiff, num_baselines, sizeof(delta_diff), dsp_qsort_delta_diff_desc);
    for(d = 0; d < triangle->dims-1; d++) {
        triangle->theta[d] = acos(deltadiff[0].diff[d] / deltadiff[0].delta);
        if(deltadiff[0].diff[d+1] < 0)
            triangle->theta[d] = M_PI*2.0-triangle->theta[d];
        for(x = 1; x < num_baselines; x++) {
            double index = acos(deltadiff[x].diff[d] / deltadiff[x].delta);
            if(deltadiff[x].diff[d+1] < 0)
                index = M_PI*2.0-index;
            index -= triangle->theta[d];
            triangle->index += index;
        }
    }
    triangle->index /= (num_baselines+triangle->dims-2)*M_PI*2;
    for(d = 0; d < triangle->stars_count; d++) {
        triangle->stars[d].diameter = stars[d].diameter;
        triangle->stars[d].peak = stars[d].peak;
        triangle->stars[d].flux = stars[d].flux;
        triangle->stars[d].theta = stars[d].theta;
        triangle->stars[d].center.dims = stars[d].center.dims;
        triangle->stars[d].center.location = (double*)malloc(sizeof(double)*stars[d].center.dims);
        for(x = 0; x < triangle->stars[d].center.dims; x++) {
            triangle->stars[d].center.location[x] = stars[d].center.location[x];
        }
    }
    for(d = 0; d < num_baselines; d++) {
        triangle->sizes[d] = deltadiff[d].delta;
        triangle->ratios[d] = deltadiff[d].delta / deltadiff[0].delta;
        free(deltadiff[d].diff);
    }
    free(deltadiff);
    return triangle;
}

int dsp_align_get_offset(dsp_stream_p stream1, dsp_stream_p stream2, double tolerance, double target_score, int num_stars)
{
    dsp_align_info *align_info;
    double decimals = pow(10, tolerance);
    double div = 0.0;
    int d, t1, t2, x, y;
    double phi = 0.0;
    double ratio = decimals*1600.0/div;
    dsp_star *stars = (dsp_star*)malloc(sizeof(dsp_star)*num_stars);
    for(d = 0; d < stream1->dims; d++) {
        div += pow(stream2->sizes[d], 2);
    }
    div = pow(div, 0.5);
    pwarn("decimals: %lf\n", decimals);
    target_score = (1.0-target_score / 100.0);
    stream2->align_info.dims = stream2->dims;
    stream2->align_info.triangles_count = 2;
    stream2->align_info.score = 1.0;
    stream2->align_info.decimals = decimals;
    pgarb("creating triangles for reference frame...\n");
    while(stream1->triangles_count > 0)
        dsp_stream_del_triangle(stream1, stream1->triangles_count-1);
    for(x = 0; x < stream1->stars_count * (stream1->stars_count-num_stars+1) / 2; x++) {
        for(y = 0; y < num_stars; y++) {
            stars[y] = stream1->stars[(x + y * (x / stream1->stars_count + 1)) % stream1->stars_count];
        }
        dsp_triangle *t = dsp_align_calc_triangle(stars, num_stars);
        dsp_stream_add_triangle(stream1, *t);
        dsp_align_free_triangle(t);
    }
    pgarb("creating triangles for current frame...\n");
    while(stream2->triangles_count > 0)
        dsp_stream_del_triangle(stream2, stream2->triangles_count-1);
    for(x = 0; x < stream2->stars_count * (stream2->stars_count-num_stars+1) / 2; x++) {
        for(y = 0; y < num_stars; y++) {
            stars[y] = stream2->stars[(x + y * (x / stream2->stars_count + 1)) % stream2->stars_count];
        }
        dsp_triangle *t = dsp_align_calc_triangle(stars, num_stars);
        dsp_stream_add_triangle(stream2, *t);
        dsp_align_free_triangle(t);
    }
    free(stars);
    stream2->align_info.center = (double*)malloc(sizeof(double)*stream2->align_info.dims);
    stream2->align_info.factor = (double*)malloc(sizeof(double)*stream2->align_info.dims);
    stream2->align_info.offset = (double*)malloc(sizeof(double)*stream2->align_info.dims);
    stream2->align_info.radians = (double*)malloc(sizeof(double)*(stream2->align_info.dims-1));
    for(t1 = 0; t1 < stream1->triangles_count; t1++) {
        for(t2 = 0; t2 < stream2->triangles_count; t2++) {
            align_info = dsp_align_fill_info(stream1->triangles[t1], stream2->triangles[t2]);
            if(align_info->score < stream2->align_info.score) {
                memcpy(stream2->align_info.center, align_info->center, sizeof(double)*stream2->align_info.dims);
                memcpy(stream2->align_info.factor, align_info->factor, sizeof(double)*stream2->align_info.dims);
                memcpy(stream2->align_info.offset, align_info->offset, sizeof(double)*stream2->align_info.dims);
                memcpy(stream2->align_info.radians, align_info->radians, sizeof(double)*(stream2->align_info.dims-1));
                memcpy(stream2->align_info.triangles, align_info->triangles, sizeof(dsp_triangle)*2);
                stream2->align_info.score = align_info->score;
                stream2->align_info.decimals = decimals;
            }
            free(align_info->center);
            free(align_info->factor);
            free(align_info->offset);
            free(align_info->radians);
            free(align_info);
        }
    }
    double radians = stream2->align_info.radians[0];
    for(d = 0; d < stream1->dims; d++) {
        phi += pow(stream2->align_info.offset[d], 2);
    }
    phi = pow(phi, 0.5);
    stream2->align_info.err = 0xf;
    if(floor(stream2->align_info.score * decimals)  < floor(target_score * decimals))
        stream2->align_info.err &= ~DSP_ALIGN_NO_MATCH;
    if(fabs(phi * ratio * decimals) < 1.0)
        stream2->align_info.err &= ~DSP_ALIGN_TRANSLATED;
    if(floor(fabs(stream2->align_info.factor[0] - 1.0) * decimals) < 1)
        stream2->align_info.err &= ~DSP_ALIGN_SCALED;
    if(floor(fabs(radians) * decimals) < 1)
        stream2->align_info.err &= ~DSP_ALIGN_ROTATED;
    return stream2->align_info.err;
}
