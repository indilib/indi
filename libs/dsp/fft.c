/*
*   DSP API - a digital signal processing library for astronomy usage
*   Copyright © 2017-2022  Ilia Platone
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
#include <fftw3.h>

static void dsp_fourier_dft_magnitude(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    if(stream->magnitude)
        stream->magnitude->buf = dsp_fourier_complex_array_get_magnitude(stream->dft, stream->len);
}

static void dsp_fourier_dft_phase(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    if(stream->phase)
        stream->phase->buf = dsp_fourier_complex_array_get_phase(stream->dft, stream->len);
}

void dsp_fourier_2dsp(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    int x, y;
    fftw_complex *dft = (fftw_complex*)malloc(sizeof(fftw_complex) * stream->len);
    memcpy(dft, stream->dft.fftw, sizeof(fftw_complex) * stream->len);
    y = 0;
    for(x = 0; x < stream->len && y < stream->len; x++) {
        int *pos = dsp_stream_get_position(stream, x);
        if(pos[0] <= stream->sizes[0] / 2) {
            stream->dft.fftw[x][0] = dft[y][0];
            stream->dft.fftw[x][1] = dft[y][1];
            stream->dft.fftw[stream->len-1-x][0] = dft[y][0];
            stream->dft.fftw[stream->len-1-x][1] = dft[y][1];
            y++;
        }
        free(pos);
    }
    dsp_fourier_dft_magnitude(stream);
    dsp_buffer_shift(stream->magnitude);
    dsp_fourier_dft_phase(stream);
    dsp_buffer_shift(stream->phase);
}

void dsp_fourier_2fftw(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    int x, y;
    if(!stream->phase || !stream->magnitude) return;
    dsp_buffer_shift(stream->magnitude);
    dsp_buffer_shift(stream->phase);
    stream->dft = dsp_fourier_phase_mag_array_get_complex(stream->magnitude->buf, stream->phase->buf, stream->len);
    fftw_complex *dft = (fftw_complex*)malloc(sizeof(fftw_complex) * stream->len);
    memcpy(dft, stream->dft.fftw, sizeof(fftw_complex) * stream->len);
    dsp_buffer_set(stream->dft.buf, stream->len*2, 0);
    y = 0;
    for(x = 0; x < stream->len; x++) {
        int *pos = dsp_stream_get_position(stream, x);
        if(pos[0] <= stream->sizes[0] / 2) {
            stream->dft.fftw[y][0] = dft[x][0];
            stream->dft.fftw[y][1] = dft[x][1];
            y++;
        }
        free(pos);
    }
}

double* dsp_fourier_complex_array_get_magnitude(dsp_complex in, int len)
{
    int i;
    double* out = (double*)malloc(sizeof(double) * len);
    for(i = 0; i < len; i++) {
        double real = in.complex[i].real;
        double imaginary = in.complex[i].imaginary;
        out [i] = sqrt (pow(real, 2)+pow(imaginary, 2));
    }
    return out;
}

double* dsp_fourier_complex_array_get_phase(dsp_complex in, int len)
{
    int i;
    double* out = (double*)malloc(sizeof(double) * len);
    for(i = 0; i < len; i++) {
        out [i] = 0;
        if (in.complex[i].real != 0) {
            double real = in.complex[i].real;
            double imaginary = in.complex[i].imaginary;
            double mag = sqrt(pow(real, 2)+pow(imaginary, 2));
            double rad = 0.0;
            if(mag > 0.0) {
                rad = acos (imaginary / (mag > 0.0 ? mag : 1.0));
                if(real < 0 && rad != 0)
                    rad = M_PI*2-rad;
            }
            out [i] = rad;
        }
    }
    return out;
}

dsp_complex dsp_fourier_phase_mag_array_get_complex(double* mag, double* phi, int len)
{
    int i;
    dsp_complex out;
    out.fftw = (fftw_complex*)malloc(sizeof(fftw_complex) * len);
    dsp_buffer_set(out.buf, len*2, 0);
    for(i = 0; i < len; i++) {
        double real = sin(phi[i])*mag[i];
        double imaginary = cos(phi[i])*mag[i];
        out.complex[i].real = real;
        out.complex[i].imaginary = imaginary;
    }
    return out;
}

static void* dsp_stream_dft_th(void* arg)
{
    struct {
        int exp;
       dsp_stream_p stream;
    } *arguments = arg;
    dsp_fourier_dft(arguments->stream, arguments->exp);
    return NULL;
}
void dsp_fourier_dft(dsp_stream_p stream, int exp)
{
    if(stream == NULL)
        return;
    if(exp < 1)
        return;
    double* buf = (double*)malloc(sizeof(double) * stream->len);
    if(stream->phase == NULL)
        stream->phase = dsp_stream_copy(stream);
    if(stream->magnitude == NULL)
        stream->magnitude = dsp_stream_copy(stream);
    dsp_buffer_set(stream->dft.buf, stream->len * 2, 0);
    dsp_buffer_copy(stream->buf, buf, stream->len);
    int *sizes = (int*)malloc(sizeof(int)*stream->dims);
    dsp_buffer_reverse(sizes, stream->dims);
    fftw_plan plan = fftw_plan_dft_r2c(stream->dims, stream->sizes, buf, stream->dft.fftw, FFTW_ESTIMATE_PATIENT);
    fftw_execute(plan);
    fftw_free(plan);
    free(sizes);
    free(buf);
    dsp_fourier_2dsp(stream);
    if(exp > 1) {
        exp--;
        pthread_t th[2];
        struct {
           int exp;
           dsp_stream_p stream;
        } thread_arguments[2];
        thread_arguments[0].exp = exp;
        thread_arguments[0].stream = stream->phase;
        pthread_create(&th[0], NULL, dsp_stream_dft_th, &thread_arguments[0]);
        thread_arguments[1].exp = exp;
        thread_arguments[1].stream = stream->magnitude;
        pthread_create(&th[1], NULL, dsp_stream_dft_th, &thread_arguments[1]);
        pthread_join(th[0], NULL);
        pthread_join(th[1], NULL);
    }
}

void dsp_fourier_idft(dsp_stream_p stream)
{
    if(stream == NULL)
        return;
    double *buf = (double*)malloc(sizeof(double)*stream->len);
    dsp_t mn = dsp_stats_min(stream->buf, stream->len);
    dsp_t mx = dsp_stats_max(stream->buf, stream->len);
    dsp_buffer_set(buf, stream->len, 0);
    free(stream->dft.buf);
    dsp_fourier_2fftw(stream);
    int *sizes = (int*)malloc(sizeof(int)*stream->dims);
    dsp_buffer_reverse(sizes, stream->dims);
    fftw_plan plan = fftw_plan_dft_c2r(stream->dims, stream->sizes, stream->dft.fftw, buf, FFTW_ESTIMATE_PATIENT);
    fftw_execute(plan);
    fftw_free(plan);
    free(sizes);
    dsp_buffer_stretch(buf, stream->len, mn, mx);
    dsp_buffer_copy(buf, stream->buf, stream->len);
    dsp_buffer_shift(stream->magnitude);
    dsp_buffer_shift(stream->phase);
    free(buf);
}
