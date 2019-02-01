/*
 *   libDSP - a digital signal processing library for astronomy usage
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

#ifndef _DSP_H
#define _DSP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <fftw3.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>

#ifndef Min
#define Min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })
#endif

#ifndef Max
#define Max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#endif

#ifndef dsp_buffer_reverse
#define dsp_buffer_reverse(a, b, len) \
    ({ \
        int i = len - 1; \
        int j = 0; \
        while(i >= 0) \
        { \
          a[i] = b[j]; \
          i--; \
          j++; \
        } \
    })
#endif

#ifndef dsp_convert
#define dsp_convert(in, out, len) \
    ({ \
        int k; \
        for(k = 0; k < len; k++) { \
        ((__typeof__ (out[0])*)out)[k] = (__typeof__ (out[0]))((__typeof__ (in[0])*)in)[k]; \
        } \
    })
#endif

#define sin2cos(s) cos(asin(s))
#define cos2sin(c) sin(acos(c))

#ifndef ONE_SECOND
#define ONE_SECOND (double)100000000
#endif
#ifndef ONE_MILLISECOND
#define ONE_MILLISECOND (double)100000
#endif
#ifndef ONE_MICROSECOND
#define ONE_MICROSECOND (double)100
#endif
#ifndef EarthRadiusEquatorial
#define EarthRadiusEquatorial (double)6378137.0
#endif
#ifndef EarthRadiusPolar
#define EarthRadiusPolar (double)6356752.0
#endif
#ifndef EarthRadiusMean
#define EarthRadiusMean (double)6372797.0
#endif
#ifndef LightSpeed
#define LightSpeed (double)299792458.0
#endif
#ifndef GammaJ2000
#define GammaJ2000 (double)1.753357767
#endif
#ifndef Euler
#define Euler (double)2.71828182845904523536028747135266249775724709369995
#endif
#ifndef ROOT2
#define ROOT2 (double)1.41421356237309504880168872420969807856967187537694
#endif
#ifndef Airy
#define Airy (double)1.21966
#endif
#ifndef CIRCLE_DEG
#define CIRCLE_DEG (double)360
#endif
#ifndef CIRCLE_AM
#define CIRCLE_AM (double)(CIRCLE_DEG * 60)
#endif
#ifndef CIRCLE_AS
#define CIRCLE_AS (double)(CIRCLE_AM * 60)
#endif
#ifndef RAD_AS
#define RAD_AS (double)(CIRCLE_AS/(PI*2))
#endif
#ifndef AstronomicalUnit
#define AstronomicalUnit (double)1.495978707E+11
#endif
#ifndef Parsec
#define Parsec (double)(AstronomicalUnit*2.06264806247096E+5)
#endif
#ifndef LightYear
#define LightYear (double)(LightSpeed * SIDEREAL_DAY * 365)
#endif
#ifdef  __cplusplus

extern "C" {
#endif
#ifndef DLL_EXPORT
#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT extern "C"
#endif
#endif

typedef void *(*dsp_func_t) (void *);

typedef enum {
    magnitude = 0,
    magnitude_dbv = 1,
    magnitude_root = 2,
    magnitude_square = 3,
    phase_degrees = 4,
    phase_radians = 5,
} dsp_conversiontype;

/**
* @brief Delegate function
*/

typedef struct dsp_stream_s {
    int len;
    int dims;
    int* sizes;
    int* pos;
    int index;
    double* in;
    double* out;
    void *arg;
    struct dsp_stream_s* parent;
    struct dsp_stream_s** children;
    int child_count;
    double* location;
    double* target;
    double lambda;
    double samplerate;
    struct timespec starttimeutc;
    pthread_t thread;
    dsp_func_t func;
} dsp_stream, *dsp_stream_p;

typedef struct {
    dsp_stream_p items;
    int index;
    int count;
} dsp_stream_array, *dsp_stream_p_array;

typedef struct {
    int x;
    int y;
} dsp_point;

typedef struct {
    int start;
    int len;
} dsp_region;

typedef struct {
    dsp_point center;
    int radius;
} dsp_star;

/**
* @brief Create a spectrum from a double array of values
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param dims the number of dimensions of the input stream (input).
* @param sizes array with the lengths of each dimension of the input stream (input).
* @param conversion the output magnitude dsp_conversiontype type.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_fft_spectrum(dsp_stream_p stream, int conversion, int size);

DLL_EXPORT double* dsp_fft_shift(double* in, int dims, int* sizes);

DLL_EXPORT double* dsp_fft_dft(dsp_stream_p stream, int sign, int conversion);

/**
* @brief A square law filter
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_filter_squarelaw(dsp_stream_p stream);

/**
* @brief A low pass filter
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param samplingfrequency the sampling frequency of the input stream.
* @param frequency the cutoff frequency of the filter.
* @param q the cutoff slope.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_filter_lowpass(dsp_stream_p stream, double samplingfrequency, double frequency, double q);

/**
* @brief A high pass filter
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param samplingfrequency the sampling frequency of the input stream.
* @param frequency the cutoff frequency of the filter.
* @param q the cutoff slope.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_filter_highpass(dsp_stream_p stream, double samplingfrequency, double frequency, double q);

/**
* @brief A band pass filter
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param samplingfrequency the sampling frequency of the input stream.
* @param frequency the center frequency of the filter.
* @param q the cutoff slope.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_filter_bandpass(dsp_stream_p stream, double samplingfrequency, double LowFrequency, double HighFrequency);

/**
* @brief A band reject filter
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param samplingfrequency the sampling frequency of the input stream.
* @param frequency the center frequency of the filter.
* @param q the cutoff slope.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_filter_bandreject(dsp_stream_p stream, double samplingfrequency, double LowFrequency, double HighFrequency);

/**
* @brief A cross-convolution processor
* @param in1 the first input stream. (input)
* @param in2 the second input stream. (input)
* @param out the output stream. (output)
* @param len1 the length of the first input stream. (input)
* @param len2 the length of the second input stream. (input)
* @param len the length of the output stream. (output)
* @return the resulting stream.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_convolution_convolution(dsp_stream_p stream1, dsp_stream_p stream2);

DLL_EXPORT void dsp_stream_add_child(dsp_stream_p stream, dsp_stream_p child);

/**
* @brief Gets minimum, mid, and maximum values of the input stream
* @param in the input stream. (input)
* @param len the length of the input stream.
* @param min the minimum value (output).
* @param max the maximum value (output).
* @return the mid value (max - min) / 2 + min.
* Return mid if success.
*/
DLL_EXPORT double dsp_stats_minmidmax(double* in, int len, double* min, double* max);

/**
* @brief A mean calculator
* @param in the input stream. (input)
* @param len the length of the input stream.
* @return the mean value of the stream.
* Return mean if success.
*/
DLL_EXPORT double dsp_stats_mean(double* in, int len);

/**
* @brief Counts value occurrences into stream
* @param in the input stream. (input)
* @param len the length of the input stream.
* @param val the value to count.
* @param prec the decimal precision.
* @return the mean value of the stream.
* Return mean if success.
*/
DLL_EXPORT int dsp_stats_val_count(double* in, int len, double val);

/**
* @brief Subtract mean from stream
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double dsp_buffer_compare(double* in1, int len1, double* in2, int len2);

/**
* @brief Subtract mean from stream
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_removemean(double* in, int len);

/**
* @brief Stretch minimum and maximum values of the input stream
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param min the desired minimum value.
* @param max the desired maximum value.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_stretch(double* in, int len, double min, double max);

/**
* @brief Normalize the input stream to the minimum and maximum values
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.

* @param min the clamping minimum value.
* @param max the clamping maximum value.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.

* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_normalize(double* in, int len, double min, double max);

/**
* @brief Subtract elements of one stream from another's
* @param in1 the input stream to be subtracted. (input)
* @param in2 the input stream with subtraction values. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_sub(double* in1, int len1, double* in2, int len2);

/**
* @brief Sum elements of one stream to another's
* @param in1 the first input stream. (input)
* @param in2 the second input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_sum(double* in1, int len1, double* in2, int len2);

/**
* @brief Divide elements of one stream to another's
* @param in1 the Numerators input stream. (input)
* @param in2 the Denominators input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_div(double* in1, int len1, double* in2, int len2);

/**
* @brief Multiply elements of one stream to another's
* @param in1 the first input stream. (input)
* @param in2 the second input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_mul(double* in1, int len1, double* in2, int len2);

DLL_EXPORT double* dsp_buffer_pow(double* in, int len, double val);

DLL_EXPORT double* dsp_buffer_root(double* in, int len, double val);
/**
* @brief Subtract a value from elements of the input stream
* @param in the Numerators input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param val the value to be subtracted.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_sub1(double* in, int len, double val);
DLL_EXPORT double* dsp_buffer_1sub(double* in, int len, double val);

/**
* @brief Sum elements of the input stream to a value
* @param in the first input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param val the value used for this operation.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_sum1(double* in, int len, double val);

/**
* @brief Divide elements of the input stream to a value
* @param in the Numerators input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param val the denominator.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_div1(double* in, int len, double val);
DLL_EXPORT double* dsp_buffer_1div(double* in, int len, double val);

/**
* @brief Multiply elements of the input stream to a value
* @param in the first input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param val the value used for this operation.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_mul1(double* in, int len, double val);

/**
* @brief Median elements of the inut stream
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param size the length of the median.
* @param median the location of the median value.
* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_median(double* in, int len, int size, int median);

/**
* @brief Histogram of the inut stream
* @param in the input stream. (input)
* @param out the output stream. (output)
* @param len the length of the input stream.
* @param size the length of the median.

* Return 0 if success.
* Return -1 if any error occurs.
*/
DLL_EXPORT double* dsp_buffer_histogram(double* in, int len, int size);

DLL_EXPORT double* dsp_buffer_zerofill(double* out, int len);

DLL_EXPORT double* dsp_buffer_val_sum(double* in, int len);

DLL_EXPORT double* dsp_buffer_convolute(double* in1, int len1, double* in2, int len2);

DLL_EXPORT double* dsp_buffer_deviate(double* in1, int len1, double* in2, int len2, double mindeviation, double maxdeviation);

DLL_EXPORT struct timespec dsp_t_mktimespec(int year, int month, int dom, int hour, int minute, int second, long nanosecond);

DLL_EXPORT double dsp_time_timespec_to_J2000time(struct timespec tp);

DLL_EXPORT double dsp_time_J2000time_to_lst(double secs_since_J2000, double Long);

DLL_EXPORT struct timespec dsp_time_J2000time_to_timespec(double secs);

DLL_EXPORT struct timespec dsp_time_YmdHMSn_to_timespec(int Y, int m, int d, int H, int M, int S, long n);

DLL_EXPORT double dsp_astro_ra2ha(double Ra, double Lst);

DLL_EXPORT void dsp_astro_hadec2altaz(double Ha, double Dec, double Lat, double* Alt, double *Az);

DLL_EXPORT double dsp_astro_elevation(double Lat, double El);

DLL_EXPORT double dsp_astro_ra2ha(double Ra, double Lst);

DLL_EXPORT void dsp_astro_hadec2altaz(double Ha, double Dec, double Lat, double* Alt, double *Az);

DLL_EXPORT double dsp_astro_elevation(double Lat, double El);

DLL_EXPORT double dsp_astro_field_rotation_rate(double Alt, double Az, double Lat);

DLL_EXPORT double dsp_astro_field_rotation(double HA, double rate);

DLL_EXPORT double dsp_astro_parsecmag2absmag(double parsec, double deltamag, int lambda_index, double* ref_specrum, int ref_len, double* spectrum, int len);

DLL_EXPORT double *dsp_stream_set_input_buffer_len(dsp_stream_p stream, int len);

DLL_EXPORT double *dsp_stream_set_output_buffer_len(dsp_stream_p stream, int len);

DLL_EXPORT double *dsp_stream_set_input_buffer(dsp_stream_p stream, void *buffer, int len);

DLL_EXPORT double *dsp_stream_set_output_buffer(dsp_stream_p stream, void *buffer, int len);

DLL_EXPORT double *dsp_stream_get_input_buffer(dsp_stream_p stream);

DLL_EXPORT double *dsp_stream_get_output_buffer(dsp_stream_p stream);

DLL_EXPORT void dsp_stream_free_input_buffer(dsp_stream_p stream);

DLL_EXPORT void dsp_stream_free_output_buffer(dsp_stream_p stream);

DLL_EXPORT dsp_stream_p dsp_stream_new();

DLL_EXPORT dsp_stream_p dsp_stream_copy(dsp_stream_p stream);

DLL_EXPORT void dsp_stream_add_dim(dsp_stream_p stream, int size);

DLL_EXPORT void dsp_stream_free(dsp_stream_p stream);

DLL_EXPORT int dsp_stream_byte_size(dsp_stream_p stream);

DLL_EXPORT dsp_stream_p dsp_stream_set_position(dsp_stream_p stream);

DLL_EXPORT dsp_stream_p dsp_stream_get_position(dsp_stream_p stream);

DLL_EXPORT void *dsp_stream_exec(dsp_stream_p stream);

DLL_EXPORT void *dsp_stream_exec_multidim(dsp_stream_p stream);

DLL_EXPORT dsp_stream_p dsp_stream_crop(dsp_stream_p in, dsp_region* rect);

DLL_EXPORT void dsp_stream_mul(dsp_stream_p in1, dsp_stream_p in2);

DLL_EXPORT void dsp_stream_sum(dsp_stream_p in1, dsp_stream_p in2);

DLL_EXPORT void dsp_stream_swap_buffers(dsp_stream_p stream);

DLL_EXPORT double dsp_astro_field_rotation_rate(double Alt, double Az, double Lat);

DLL_EXPORT double dsp_astro_field_rotation(double HA, double rate);

DLL_EXPORT double* dsp_signals_sinewave(int len, double samplefreq, double freq);

DLL_EXPORT double* dsp_signals_sawteethwave(int len, double samplefreq, double freq);

DLL_EXPORT double* dsp_signals_triwave(int len, double samplefreq, double freq);

DLL_EXPORT double* dsp_modulation_frequency(double* in, int len, double samplefreq, double freq, double bandwidth);

DLL_EXPORT double* dsp_modulation_amplitude(double* in, int len, double samplefreq, double freq);

#ifdef  __cplusplus
}
#endif

#endif //_DSP_H
