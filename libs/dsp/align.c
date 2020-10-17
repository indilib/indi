/*
 *   libDSPAU - a digital signal processing library for astronoms usage
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
#include "sep.h"
#include <time.h>

static int cmp (const void *arg1, const void *arg2)
{
    dsp_star* a1 = (dsp_star*)arg1;
    dsp_star* a2 = (dsp_star*)arg2;
    return (a2->diameter > a1->diameter ? 1 : -1);
}

int dsp_align_find_stars(dsp_stream_p stream, int levels, int min_size, float threshold, dsp_stream_p matrix)
{
    start_gettime;
    dsp_stream_p test = dsp_stream_copy(stream);
    dsp_buffer_stretch(test->buf, test->len, 0, 100);
    unsigned char* buf = (unsigned char*)malloc(test->len);
    dsp_buffer_copy(test->buf, buf, test->len);
    dsp_buffer_stretch(matrix->buf, matrix->len, 0, 100);
    float *fmatrix = (float*)malloc(sizeof(float)*matrix->len);
    dsp_buffer_copy(matrix->buf, fmatrix, matrix->len);
    sep_image image;
    image.data = buf;
    image.noise = NULL;
    image.mask = NULL;
    image.segmap = NULL;
    image.w = test->sizes[0];
    image.h = test->sizes[1];
    image.dtype = SEP_TBYTE;
    image.ndtype = SEP_TBYTE;
    image.sdtype = SEP_TBYTE;
    image.mdtype = SEP_TBYTE;
    image.noiseval = 0;
    image.noise_type = SEP_NOISE_NONE;
    image.gain = 1.0;
    sep_catalog *catalog = (sep_catalog*)malloc(sizeof(sep_catalog));
    sep_extract(&image,
             threshold,         /* detection threshold           [1.5] */
             SEP_THRESH_ABS,      /* threshold units    [SEP_THRESH_REL] */
             min_size,          /* minimum area in pixels          [5] */
             fmatrix,
             matrix->sizes[0], matrix->sizes[1],
             SEP_FILTER_MATCHED,
             levels,
             0.05,
             0,
             0.0,
             &catalog);
    dsp_stream_free_buffer(test);
    dsp_stream_free(test);
    free(buf);
    free(fmatrix);
    if(catalog != NULL) {
        stream->stars = (dsp_star*)realloc(stream->stars, sizeof(dsp_star)*catalog->nobj);
        for(stream->stars_count = 0; stream->stars_count < catalog->nobj; stream->stars_count++) {
            stream->stars[stream->stars_count].center.location = (double*)malloc(sizeof(double)*2);
            stream->stars[stream->stars_count].center.location[0] = catalog->xpeak[stream->stars_count];
            stream->stars[stream->stars_count].center.location[1] = catalog->ypeak[stream->stars_count];
            stream->stars[stream->stars_count].center.dims = 2;
            stream->stars[stream->stars_count].diameter = floor(catalog->flux[stream->stars_count]*1024);
        }
    }
    qsort(stream->stars, stream->stars_count, sizeof(dsp_star), cmp);
    end_gettime;
    fprintf(stdout, "found %d stars\n", stream->stars_count);
    return stream->stars_count;
}

int compar (const void *arg1, const void *arg2)
{
    dsp_offset* a1 = (dsp_offset*)arg1;
    dsp_offset* a2 = (dsp_offset*)arg2;
    return a2->offset[a2->dims-1] - a1->offset[a1->dims-1];
}

void *dsp_align_get_offsets_th(void* arg) {
    struct {
        int s;
        dsp_offset* offsets;
        dsp_stream_p stream;
     } *arguments = arg;
    dsp_offset* offsets = arguments->offsets;
    dsp_stream_p stream = arguments->stream;
    int s = arguments->s;
    int l = 0;
}

dsp_align_info* dsp_align_get_baselines(dsp_stream_p stream1, dsp_stream_p stream2)
{
    int d;
    double dist1 = 0;
    double dist2 = 0;
    if(stream1->stars_count < 2 || stream2->stars_count < 2) {
        fprintf(stdout, "Not enough stars\n");
        return NULL;
    }
    dsp_align_info* info = (dsp_align_info*)malloc(sizeof(dsp_align_info));
    info->dims = Min(stream1->stars[0].center.dims, stream1->stars[0].center.dims);
    if(info->dims < 2) {
        fprintf(stdout, "Not enough dimensions\n");
        free(info);
        return NULL;
    }
    info->offset = (int*)malloc(sizeof(int)*info->dims);
    info->center = (int*)malloc(sizeof(int)*info->dims);
    info->radians = (double*)malloc(sizeof(double)*(info->dims-1));
    for(d = 0; d < info->dims; d++) {
        dist1 += pow((stream1->stars[1].center.location[d] - stream1->stars[0].center.location[d]), 2);
        dist2 += pow((stream2->stars[1].center.location[d] - stream2->stars[0].center.location[d]), 2);
        info->center[d] = stream2->stars[0].center.location[d];
        info->offset[d] = stream1->stars[0].center.location[d] - stream2->stars[0].center.location[d];
    }
    dist1 = sqrt(dist1);
    dist2 = sqrt(dist2);
    info->factor = dist1 / dist2;
    for(d = 0; d < info->dims-1; d++) {
        info->radians[d] = acos((stream1->stars[1].center.location[d] - stream1->stars[0].center.location[d]) / dist1);
        info->radians[d] -= acos((stream2->stars[1].center.location[d] - stream2->stars[0].center.location[d]) / dist2);
    }
    fprintf(stdout, "frame offset: x=%d y=%d cx=%d cy=%d θ=%lf Δ=%lf\n", info->offset[0], info->offset[1], info->center[0], info->center[1], info->radians[0], info->factor);
    return info;
}

dsp_align_info* dsp_align_get_offset(dsp_stream_p stream1, dsp_stream_p stream2)
{
    int d;
    double dist1 = 0;
    double dist2 = 0;
    if(stream1->stars_count < 2 || stream2->stars_count < 2) {
        fprintf(stdout, "Not enough stars\n");
        return NULL;
    }
    dsp_align_info* info = (dsp_align_info*)malloc(sizeof(dsp_align_info));
    info->dims = Min(stream1->stars[0].center.dims, stream1->stars[0].center.dims);
    if(info->dims < 2) {
        fprintf(stdout, "Not enough dimensions\n");
        free(info);
        return NULL;
    }
    info->offset = (int*)malloc(sizeof(int)*info->dims);
    info->center = (int*)malloc(sizeof(int)*info->dims);
    info->radians = (double*)malloc(sizeof(double)*(info->dims-1));
    for(d = 0; d < info->dims; d++) {
        dist1 += pow((stream1->stars[1].center.location[d] - stream1->stars[0].center.location[d]), 2);
        dist2 += pow((stream2->stars[1].center.location[d] - stream2->stars[0].center.location[d]), 2);
        info->center[d] = stream2->stars[0].center.location[d];
        info->offset[d] = stream1->stars[0].center.location[d] - stream2->stars[0].center.location[d];
    }
    dist1 = sqrt(dist1);
    dist2 = sqrt(dist2);
    info->factor = dist1 / dist2;
    for(d = 0; d < info->dims-1; d++) {
        info->radians[d] = acos((stream1->stars[1].center.location[d] - stream1->stars[0].center.location[d]) / dist1);
        info->radians[d] -= acos((stream2->stars[1].center.location[d] - stream2->stars[0].center.location[d]) / dist2);
    }
    fprintf(stdout, "frame offset: x=%d y=%d cx=%d cy=%d θ=%lf Δ=%lf\n", info->offset[0], info->offset[1], info->center[0], info->center[1], info->radians[0], info->factor);
    return info;
}
