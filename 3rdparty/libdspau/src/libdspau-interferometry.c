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

static void* dspau_autocorrelate_delegate_mult(void *arg)
{
    dspau_stream_p child = arg;
    dspau_stream_p stream = child->parent;
    int x = 0, y = 0;
    for (child->index = stream->index; x < child->len; x++, child->index++) {
        child->index %= child->len;
        y = child->index;
        dspau_stream_position(child);
        for(int dim = 0; dim < stream->dims; dim++) {
            if(stream->pos[dim] == child->pos[dim]) {
                stream->out[stream->index] += (stream->in[x] * child->in[y]);
            }
        }
    }
    return NULL;
}

static void* dspau_autocorrelate_delegate(void *arg)
{ 
    dspau_stream_p stream = arg;
    dspau_stream_p* children = stream->children;
    int c = 0;
    double_t percent = 0.0;
    for (stream->index = 0; stream->index < stream->len; stream->index ++) {
        dspau_stream_position(stream);
        for(c = 0; c < stream->child_count; c++) {
            dspau_stream_exec(children[c]);
        }
        percent += 100.0 / stream->len;
        fprintf(stderr, "\r%lf", percent);
    }
    dspau_buffer_div1(stream->out, stream->len, stream->child_count);
    return stream->out;
}

dspau_t* dspau_interferometry_autocorrelate(dspau_stream_p stream) {
    dspau_stream_p* children = stream->children;
    stream->func = &dspau_autocorrelate_delegate;
    for(int c = 0; c < stream->child_count; c++) {
        children[c]->func = &dspau_autocorrelate_delegate_mult;
    }
    return dspau_stream_exec(stream);
}

dspau_t* dspau_interferometry_uv_location(dspau_t HA, dspau_t DEC, dspau_t* baseline3) {
    DEC *=  PI / 180.0;
    dspau_t* uv = (dspau_t*)calloc(sizeof(dspau_t), 2);
    uv[0] = (baseline3[0] * sin(HA) + baseline3[1] * cos(HA));
    uv[1] = (-baseline3[0] * sin(DEC) * cos(HA) + baseline3[1] * sin(DEC) * sin(HA) + baseline3[2] * cos(DEC));
    return uv;
}

dspau_t* dspau_interferometry_calc_baselines(dspau_stream_p stream) {
    dspau_stream_p* children = stream->children;
    int num_baselines = ((1 + stream->child_count) * stream->child_count) / 2;
    dspau_t* ret = calloc(sizeof(dspau_t) * 3, num_baselines);
    for(int x = 0; x < stream->child_count; x++) {
        for(int y = x; y < stream->child_count; y++) {
            *ret++ = sqrt(pow(children[x]->location[0], 2) + pow(children[y]->location[0], 2));
            *ret++ = sqrt(pow(children[x]->location[1], 2) + pow(children[y]->location[1], 2));
            *ret++ = sqrt(pow(children[x]->location[2], 2) + pow(children[y]->location[2], 2));
        }
    }
    return ret;
}

dspau_t* dspau_interferometry_uv_coords(dspau_stream_p stream) {
    dspau_t* uv = (dspau_t*)calloc(sizeof(dspau_t), (int)pow(stream->len, 2));
    int num_baselines = ((1 + stream->child_count) * stream->child_count) / 2;
    dspau_t *baselines = dspau_interferometry_calc_baselines(stream);
    dspau_t tao = (1.0 / stream->samplerate);
    tao *= 1000000000.0;
    dspau_t start_time = dspau_time_timespec_to_J2000time(stream->starttimeutc);
    start_time *= 1000000000.0;
    dspau_t current_time = start_time;
    dspau_t end_time = start_time + tao * stream->len;
    for(current_time = start_time; current_time < end_time; current_time+=tao) {
        dspau_t lst = dspau_time_J2000time_to_lst(current_time, 0);
        dspau_t HA = dspau_astro_ra2ha(stream->target[0], lst);
        for(int l = 0; l < num_baselines; l++) {
            dspau_t* uvcoords = dspau_interferometry_uv_location(HA, stream->target[1], (dspau_t*)(baselines + l * 3 * sizeof(dspau_t)));
            for(int d = 0; d < 2; d++) {
                uvcoords[d] /= stream->lambda;
            }
            uv[(int)(uvcoords[0] + uvcoords[1] * stream->len)] = 0;
        }
        current_time += tao;
    }
    return uv;
}
