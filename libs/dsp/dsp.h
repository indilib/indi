/*
 *   libDSP - a digital signal processing library
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

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT extern
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>


/**
 * \defgroup DSP Digital Signal Processing API
*
* The DSP API is used for processing monodimensional or multidimensional buffers,<br>
* converting array element types, generate statistics, extract informations from buffers, convolute or<br>
* cross-correlate different single or multi dimensional streams, rotate, scale, crop images.<br>
*
* \author Ilia Platone
*/
/*@{*/

/**
 * \defgroup DSP_Defines DSP API defines
*/
/*@{*/
#define DSP_MAX_STARS 200
#define dsp_t double
#define dsp_t_max pow(2, sizeof(dsp_t)*4)
#define dsp_t_min -dsp_t_max

#define DSP_MAX_THREADS 4

///if min() is not present you can use this one
#ifndef Min
#define Min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (a) _b = (b); \
     _a < _b ? _a : _b; })
#endif
///if max() is not present you can use this one
#ifndef Max
#define Max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (a) _b = (b); \
     _a > _b ? _a : _b; })
#endif
///Logarithm of a with arbitrary base b
#ifndef Log
#define Log(a,b) \
( log(a) / log(b) )
#endif
/*@}*/
/**
 * \defgroup DSP_Types DSP API types
*/
/*@{*/

/**
* \brief Indicates a dot or line inside a dsp_stream
* \sa dsp_stream
*/
typedef struct dsp_point_t
{
/// Center of the point
    double* location;
/// Dimensions limit of the point
    int dims;
} dsp_point;

/**
* \brief Indicates an offset
*/
typedef struct dsp_offset_t
{
/// Center of the point
    double* offset;
/// Dimensions limit of the point
    int dims;
} dsp_offset;

/**
* \brief Alignment informations needed
*/
typedef struct dsp_align_info_t
{
    /// Traslation offset
    double* offset;
    /// Center of rotation coordinates
    double* center;
    /// Rotational offset
    double* radians;
    /// Scaling factor
    double factor;
    /// Dimensions limit
    int dims;
} dsp_align_info;

/**
* \brief Complex number, used in Fourier Transform functions
* \sa dsp_fourier_dft
* \sa dsp_fourier_complex_to_magnitude
*/
typedef struct dsp_complex_t
{
/// Real part of the complex number
    double real;
/// Imaginary part of the complex number
    double imaginary;
} dsp_complex;

/**
* \brief Delimits a region in a single dimension of a buffer
*/
typedef struct dsp_region_t
{
/// Starting point within the buffer
    int start;
/// Length of the region
    int len;
} dsp_region;

/**
* \brief A star or object contained into a buffer
*/
typedef struct dsp_star_t
{
/// The center of the star
    dsp_point center;
/// The diameter of the star
    double diameter;
} dsp_star;

/**
* \brief Multi-dimensional processing delegate function
*/
typedef void *(*dsp_func_t) (void *, ...);

/**
* \brief Contains a set of informations and data relative to a buffer and how to use it
* \sa dsp_stream_new
* \sa dsp_stream_free
* \sa dsp_stream_add_dim
* \sa dsp_stream_del_dim
*/
typedef struct dsp_stream_t
{
/// The buffers length
    int len;
/// Number of dimensions of the buffers
    int dims;
/// Sizes of each dimension
    int* sizes;
/// buffer
    dsp_t* buf;
/// Optional argument for the func() callback
    void *arg;
/// The stream this one is child of
    struct dsp_stream_t* parent;
/// Children streams of the current one
    struct dsp_stream_t** children;
/// Children streams count
    int child_count;
/// Location coordinates
    double* location;
/// Target coordinates
    double* target;
/// Time at the beginning of the stream
    struct timespec starttimeutc;
/// Wavelength observed, used as reference with signal generators or filters
    double wavelength;
/// Focal ratio
    double focal_ratio;
/// Diameter
    double diameter;
/// SNR
    double SNR;
/// Red pixel (Bayer)
    int red;
/// Sensor size
    double *pixel_sizes;
/// Sample rate of the buffers
    double samplerate;
/// Thread type for future usage
    pthread_t thread;
/// Callback function
    dsp_func_t func;
/// Regions of interest for each dimension
    dsp_region *ROI;
/// Stars or objects identified into the buffers - TODO
    dsp_star *stars;
/// Stars or objects quantity
    int stars_count;
/// Align/scale/rotation settings
    dsp_align_info align_info;
/// Frame number (if part of a series)
    int frame_number;
} dsp_stream, *dsp_stream_p;

/*@}*/
/**
 * \defgroup dsp_FourierTransform DSP API Fourier transform related functions
*/
/*@{*/

/**
* \brief Perform a discrete Fourier Transform of a dsp_stream
* \param stream the inout stream.
*/
DLL_EXPORT dsp_complex* dsp_fourier_dft(dsp_stream_p stream);

/**
* \brief Perform an inverse discrete Fourier Transform of a dsp_stream
* \param stream the inout stream.
*/
DLL_EXPORT dsp_t* dsp_fourier_idft(dsp_stream_p stream);

/**
* \brief Perform a fast Fourier Transform of a dsp_stream
* \param stream the inout stream.
*/
DLL_EXPORT void dsp_fourier_fft(dsp_stream_p stream);

/**
* \brief Perform an inverse fast Fourier Transform of a dsp_stream
* \param stream the inout stream.
*/
DLL_EXPORT void dsp_fourier_ifft(dsp_stream_p stream);

/**
* \brief Obtain a complex number's magnitude
* \param n the input complex.
* \return the magnitude of the given number
*/
DLL_EXPORT double dsp_fourier_complex_get_magnitude(dsp_complex n);

/**
* \brief Obtain a complex number's phase
* \param n the input complex.
* \return the phase of the given number
*/
DLL_EXPORT double dsp_fourier_complex_get_phase(dsp_complex n);

/**
* \brief Obtain a complex number's array magnitudes
* \param in the input complex number array.
* \param len the input array length.
* \return the array filled with the magnitudes
*/
DLL_EXPORT double* dsp_fourier_complex_array_get_magnitude(dsp_complex* in, int len);

/**
* \brief Obtain a complex number's array phases
* \param in the input complex number array.
* \param len the input array length.
* \return the array filled with the phases
*/
DLL_EXPORT double* dsp_fourier_complex_array_get_phase(dsp_complex* in, int len);

/**
* \brief Perform a discrete Fourier Transform of a dsp_stream and obtain the complex magnitudes
* \param stream the input stream.
*/
DLL_EXPORT void dsp_fourier_dft_magnitude(dsp_stream_p stream);

/**
* \brief Perform a discrete Fourier Transform of a dsp_stream and obtain the complex phases
* \param stream the input stream.
*/
DLL_EXPORT void dsp_fourier_dft_phase(dsp_stream_p stream);

/**
* \brief Perform an inverse discrete Fourier Transform of a dsp_stream and obtain the complex magnitudes
* \param stream the input stream.
*/
DLL_EXPORT void dsp_fourier_idft_magnitude(dsp_stream_p stream);

/**
* \brief Perform an inverse discrete Fourier Transform of a dsp_stream and obtain the complex phases
* \param stream the input stream.
*/
DLL_EXPORT void dsp_fourier_idft_phase(dsp_stream_p stream);

/*@}*/
/**
 * \defgroup dsp_Filters DSP API Linear buffer filtering functions
*/
/*@{*/
/**
* \brief A square law filter
* \param stream the input stream.
*/
DLL_EXPORT void dsp_filter_squarelaw(dsp_stream_p stream);

/**
* \brief A low pass filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param frequency the cutoff frequency of the filter.
*/
DLL_EXPORT void dsp_filter_lowpass(dsp_stream_p stream, double samplingfrequency, double frequency);

/**
* \brief A high pass filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param frequency the cutoff frequency of the filter.
*/
DLL_EXPORT void dsp_filter_highpass(dsp_stream_p stream, double samplingfrequency, double frequency);

/**
* \brief A band pass filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param LowFrequency the high-pass cutoff frequency of the filter.
* \param HighFrequency the low-pass cutoff frequency of the filter.
*/
DLL_EXPORT void dsp_filter_bandpass(dsp_stream_p stream, double samplingfrequency, double LowFrequency, double HighFrequency);

/**
* \brief A band reject filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param LowFrequency the high-pass cutoff frequency of the filter.
* \param HighFrequency the low-pass cutoff frequency of the filter.
*/
DLL_EXPORT void dsp_filter_bandreject(dsp_stream_p stream, double samplingfrequency, double LowFrequency, double HighFrequency);

/*@}*/
/**
 * \defgroup dsp_Convolution DSP API Convolution and cross-correlation functions
*/
/*@{*/
/**
* \brief A cross-convolution processor
* \param stream1 the first input stream.
* \param stream2 the second input stream.
*/
DLL_EXPORT dsp_stream_p dsp_convolution_convolution(dsp_stream_p stream1, dsp_stream_p stream2);

/*@}*/
/**
 * \defgroup dsp_Stats DSP API Buffer statistics functions
*/
/*@{*/

/**
* \brief Gets the minimum value of the input stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the minimum value.
*/
#define dsp_stats_min(buf, len)\
({\
    int i;\
    __typeof__(buf[0]) min = (__typeof__(buf[0]))buf[0];\
    for(i = 0; i < len; i++) {\
        min = Min(buf[i], min);\
    }\
    min;\
    })

/**
* \brief Gets the maximum value of the input stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the maximum value.
*/
#define dsp_stats_max(buf, len)\
({\
    int i;\
    __typeof__(buf[0]) max = (__typeof__(buf[0]))buf[0];\
    for(i = 0; i < len; i++) {\
        max = Max(buf[i], max);\
    }\
    max;\
    })

/**
* \brief Gets the middle value of the input stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the middle value.
*/
#define dsp_stats_mid(buf, len)\
({\
    int i;\
    __typeof__(buf[0]) min = dsp_stats_min(buf, len);\
    (__typeof__(buf[0]))(min - dsp_stats_max(buf, len)) / 2.0 + min;\
})

/**
* \brief Gets minimum value's position into the buffer
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the position index of the minimum value.
*/
#define dsp_stats_minimum_index(buf, len)\
({\
    int i;\
    __typeof__(buf[0]) min = dsp_stats_min(buf, len);\
    for(i = 0; i < len; i++) {\
        if(buf[i] == min) break;\
    }\
    i;\
    })

/**
* \brief Gets maximum value's position into the buffer
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the position index of the maximum value.
*/
#define dsp_stats_maximum_index(buf, len)\
({\
    int i;\
    __typeof__(buf[0]) max = dsp_stats_max(buf, len);\
    for(i = 0; i < len; i++) {\
        if(buf[i] == max) break;\
    }\
    i;\
    })

/**
* \brief A mean calculator
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the mean value of the stream.
*/
#define dsp_stats_mean(buf, len)\
({\
    int i;\
    __typeof__(buf[0]) mean = 0;\
    for(i = 0; i < len; i++) {\
        mean += buf[i];\
    }\
    mean /= len;\
    mean;\
    })

/**
* \brief Counts value occurrences into stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \param val the value to count.
* \return the count of the value of the stream.
*/
#define dsp_stats_val_count(buf, len, val) \
({\
    int x;\
    int count = 0;\
    for(x = 0; x < len; x++) {\
        if(buf[x] == val)\
            count ++;\
    }\
    count;\
    })

/**
* \brief Counts value occurrences into stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \param val the value to count.
* \return the count of the value of the stream.
*/
#define dsp_stats_range_count(buf, len, lo, hi) \
({\
    int x;\
    int count = 0;\
    for(x = 0; x < len; x++) {\
        if(buf[x] < hi && buf[x] >= lo)\
            count ++;\
    }\
    count;\
    })

/**
* \brief Compare two streams
* \param in1 the first input buffer
* \param in2 the second input buffer
* \param len the length in elements of the buffer.
* \return the sum of the subtraction of each element of both streams
*/
#define dsp_stats_compare(in1, in2, len)\
({\
    __typeof__(in1[0]) out = 0;\
    for(int i = 0; i < len; i++) {\
        out += in1[i] - (__typeof__(in1[0]))in2[i];\
    }\
    out;\
    })

/**
* \brief Histogram of the inut stream
* \param stream the stream on which execute
* \param size the length of the median.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
DLL_EXPORT double* dsp_stats_histogram(dsp_stream_p stream, int size);

/*@}*/
/**
 * \defgroup dsp_Buffers DSP API Buffer editing functions
*/
/*@{*/

/**
* \brief Shift a stream on each dimension
* \param stream the input stream.
*/
DLL_EXPORT void dsp_buffer_shift(dsp_stream_p stream);

/**
* \brief Subtract mean from stream
* \param stream the stream on which execute
*/
DLL_EXPORT void dsp_buffer_removemean(dsp_stream_p stream);

/**
* \brief Stretch minimum and maximum values of the input stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \param min the desired minimum value.
* \param max the desired maximum value.
*/

#define dsp_buffer_stretch(buf, len, _mn, _mx)\
({\
    int k;\
    __typeof__(buf[0]) __mn = dsp_stats_min(buf, len);\
    __typeof__(buf[0]) __mx = dsp_stats_max(buf, len);\
    double oratio = (_mx - _mn);\
    double iratio = (__mx - __mn);\
    if(iratio == 0) iratio = 1;\
    for(k = 0; k < len; k++) {\
        buf[k] -= __mn;\
        buf[k] = (__typeof__(buf[0]))((double)buf[k] * oratio / iratio);\
        buf[k] += _mn;\
    }\
})

/**
* \brief Place the given value on each element of the buffer
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \param val the desired value.
*/

#define dsp_buffer_set(buf, len, _val)\
({\
    int k;\
    for(k = 0; k < len; k++) {\
        buf[k] = (__typeof__(buf[0]))(_val);\
    }\
})

/**
* \brief Normalize the input stream to the minimum and maximum values
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \param min the clamping minimum value.
* \param max the clamping maximum value.
*/
#define dsp_buffer_normalize(buf, len, mn, mx)\
({\
    int k;\
    for(k = 0; k < len; k++) {\
        buf[k] = Max(mn, Min(mx, buf[k]));\
    }\
})

/**
* \brief Subtract elements of one stream from another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_max(dsp_stream_p stream, dsp_t* in, int len);

/**
* \brief Sum elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_min(dsp_stream_p stream, dsp_t* in, int len);

/**
* \brief Subtract elements of one stream from another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_sub(dsp_stream_p stream, dsp_t* in, int len);

/**
* \brief Sum elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_sum(dsp_stream_p stream, dsp_t* in, int len);

/**
* \brief Divide elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_div(dsp_stream_p stream, dsp_t* in, int len);

/**
* \brief Multiply elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_mul(dsp_stream_p stream, dsp_t* in, int len);

/**
* \brief Expose elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_pow(dsp_stream_p stream, dsp_t* in, int len);

/**
* \brief Logarithm elements of one stream using another's as base
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_log(dsp_stream_p stream, dsp_t* in, int len);

/**
* \brief Subtract a value from elements of the input stream
* \param stream the stream on which execute
* \param val the value to be subtracted.
*/
DLL_EXPORT void dsp_buffer_sub1(dsp_stream_p stream, double val);

/**
* \brief Subtract each element of the input stream a value
* \param stream the stream on which execute
* \param val the value to be subtracted.
*/
DLL_EXPORT void dsp_buffer_1sub(dsp_stream_p stream, double val);

/**
* \brief Sum elements of the input stream to a value
* \param stream the stream on which execute
* \param val the value used for this operation.
*/
DLL_EXPORT void dsp_buffer_sum1(dsp_stream_p stream, double val);

/**
* \brief Divide elements of the input stream to a value
* \param stream the stream on which execute
* \param val the denominator.
*/
DLL_EXPORT void dsp_buffer_div1(dsp_stream_p stream, double val);

/**
* \brief Divide a value to each element of the input stream
* \param stream the stream on which execute
* \param val the nominator.
*/
DLL_EXPORT void dsp_buffer_1div(dsp_stream_p stream, double val);

/**
* \brief Multiply elements of the input stream to a value
* \param stream the stream on which execute
* \param val the value used for this operation.
*/
DLL_EXPORT void dsp_buffer_mul1(dsp_stream_p stream, double val);

/**
* \brief Expose elements of the input stream to the given power
* \param stream the stream on which execute
* \param val the nth power to expose each element.
*/
DLL_EXPORT void dsp_buffer_pow1(dsp_stream_p stream, double val);

/**
* \brief Logarithm elements of the input stream using the given base
* \param stream the stream on which execute
* \param val the logarithmic base.
*/
DLL_EXPORT void dsp_buffer_log1(dsp_stream_p stream, double val);

/**
* \brief Median elements of the inut stream
* \param stream the stream on which execute
* \param size the length of the median.
* \param median the location of the median value.
*/
DLL_EXPORT void dsp_buffer_median(dsp_stream_p stream, int size, int median);

/**
* \brief Deviate forward the first input stream using the second stream as indexing reference
* \param stream the stream on which execute
* \param deviation the stream containing the deviation buffer
* \param mindeviation the deviation at 0.
* \param maxdeviation the deviation at 1.
*/
DLL_EXPORT void dsp_buffer_deviate(dsp_stream_p stream, dsp_t* deviation, dsp_t mindeviation, dsp_t maxdeviation);

/**
* \brief Reverse the order of the buffer elements
* \param buf the input stream.
* \param len the length of the first input stream.
*/
#ifndef dsp_buffer_reverse
#define dsp_buffer_reverse(buf, len) \
    ({ \
        int i = (len - 1) / 2; \
        int j = i + 1; \
        __typeof__(buf[0]) _x; \
        while(i >= 0) \
        { \
          _x = buf[j]; \
          buf[j] = buf[i]; \
          buf[i] = _x; \
          i--; \
          j++; \
        } \
    })
#endif

#ifndef dsp_buffer_swap
#define dsp_buffer_swap(in, len) \
    ({ \
        int k; \
        switch(sizeof(((__typeof__ (in[0])*)in)[0])) { \
        case 2: \
            for(k = 0; k < len; k++) \
                ((__typeof__ (in[0])*)in)[k] = __bswap_16(((__typeof__ (in[0])*)in)[k]); \
            break; \
        case 3: \
            for(k = 0; k < len; k++) \
            ((__typeof__ (in[0])*)in)[k] = __bswap_32(((__typeof__ (in[0])*)in)[k]); \
            break; \
        case 4: \
            for(k = 0; k < len; k++) \
                ((__typeof__ (in[0])*)in)[k] = __bswap_64(((__typeof__ (in[0])*)in)[k]); \
            break; \
        } \
    })
#endif

/**
* \brief Fill the output buffer with the values of the
* elements of the input stream by casting them to the
* output buffer element type
* \param in the input stream.
* \param out the output stream.
* \param len the length of the first input stream.
*/
#ifndef dsp_buffer_copy
#define dsp_buffer_copy(in, out, len) \
    ({ \
        int k; \
        for(k = 0; k < len; k++) { \
        ((__typeof__ (out[0])*)out)[k] = (__typeof__ (out[0]))((__typeof__ (in[0])*)in)[k]; \
        } \
    })
#endif

/**
* \brief Fill the output buffer with the values of the
* elements of the input stream by casting them to the
* output buffer element type
* \param in the input stream.
* \param out the output stream.
* \param len the length of the first input stream.
* \param instep copy each instep elements of in into each outstep elements of out.
* \param outstep copy each instep elements of in into each outstep elements of out.
*/
#ifndef dsp_buffer_copy_stepping
#define dsp_buffer_copy_stepping(in, out, inlen, outlen, instep, outstep) \
    ({ \
    int k; \
    int t; \
        for(k = 0, t = 0; k < inlen && t < outlen; k+=instep, t+=outstep) { \
        ((__typeof__ (out[0])*)out)[t] = (__typeof__ (out[0]))((__typeof__ (in[0])*)in)[k]; \
        } \
    })
#endif

/*@}*/
/**
 * \defgroup dsp_DSPStream DSP API Stream type management functions
*/
/*@{*/

/**
* \brief Allocate a buffer with length len on the stream passed as argument
* \param stream the target DSP stream.
* \param len the new length of the buffer.
*/
DLL_EXPORT void dsp_stream_alloc_buffer(dsp_stream_p stream, int len);

/**
* \brief Set the buffer of the stream passed as argument to a specific memory location
* \param stream the target DSP stream.
* \param buffer the new location of the buffer.
* \param len the new length of the buffer.
*/
DLL_EXPORT void dsp_stream_set_buffer(dsp_stream_p stream, void *buffer, int len);

/**
* \brief Return the buffer of the stream passed as argument
* \param stream the target DSP stream.
* \return the buffer
*/
DLL_EXPORT dsp_t* dsp_stream_get_buffer(dsp_stream_p stream);

/**
* \brief Free the buffer of the DSP Stream passed as argument
* \param stream the target DSP stream.
*/
DLL_EXPORT void dsp_stream_free_buffer(dsp_stream_p stream);

/**
* \brief Allocate a new DSP stream type
* \return the newly created DSP stream type
* \sa dsp_stream_free
*/
DLL_EXPORT dsp_stream_p dsp_stream_new();

/**
* \brief Free the DSP stream passed as argument
* \param stream the target DSP stream.
* \sa dsp_stream_new
*/
DLL_EXPORT void dsp_stream_free(dsp_stream_p stream);

/**
* \brief Create a copy of the DSP stream passed as argument
* \param stream the DSP stream to copy.
* \return the copy of the DSP stream
* \sa dsp_stream_new
*/
DLL_EXPORT dsp_stream_p dsp_stream_copy(dsp_stream_p stream);

/**
* \brief Add a star into the stream struct
* \param stream the target DSP stream.
* \param star the star to add to the stream.
*/
DLL_EXPORT void dsp_stream_add_star(dsp_stream_p stream, dsp_star star);

/**
* \brief Add a child to the DSP Stream passed as argument
* \param stream the target DSP stream.
* \param child the child to add to DSP stream.
* \sa dsp_stream_new
* \sa dsp_stream_del_child
*/
DLL_EXPORT void dsp_stream_add_child(dsp_stream_p stream, dsp_stream_p child);

/**
* \brief Add a star to the DSP Stream passed as argument
* \param stream the target DSP stream.
* \param child the star to add to DSP stream.
* \sa dsp_stream_new
* \sa dsp_stream_del_star
*/
DLL_EXPORT void dsp_stream_add_star(dsp_stream_p stream, dsp_star star);

/**
* \brief Remove the child with index n to a DSP stream
* \param stream the target DSP stream.
* \param n the index of the child to remove
* \sa dsp_stream_new
* \sa dsp_stream_add_child
*/
DLL_EXPORT void dsp_stream_del_child(dsp_stream_p stream, int n);

/**
* \brief Remove the star with index n to a DSP stream
* \param stream the target DSP stream.
* \param n the index of the star to remove
* \sa dsp_stream_new
* \sa dsp_stream_add_star
*/
DLL_EXPORT void dsp_stream_del_star(dsp_stream_p stream, int n);

/**
* \brief Add a dimension with length len to a DSP stream
* \param stream the target DSP stream.
* \param len the size of the dimension to add
* \sa dsp_stream_new
* \sa dsp_stream_del_dim
*/
DLL_EXPORT void dsp_stream_add_dim(dsp_stream_p stream, int len);

/**
* \brief Remove the dimension with index n to a DSP stream
* \param stream the target DSP stream.
* \param n the index of the dimension to remove
* \sa dsp_stream_new
* \sa dsp_stream_add_dim
*/
DLL_EXPORT void dsp_stream_del_dim(dsp_stream_p stream, int n);

/**
* \brief Obtain the position the DSP stream by parsing multidimensional indexes
* \param stream the target DSP stream.
* \param pos the position of the index on each dimension.
* \return the position of the index on a single dimension.
* \sa dsp_stream_new
* \sa dsp_stream_get_position
* \sa dsp_stream_exec
* \sa dsp_stream_exec_multidim
*/
DLL_EXPORT int dsp_stream_set_position(dsp_stream_p stream, int *pos);

/**
* \brief Return the multidimensional positional indexes of a DSP stream by specify a linear index
* \param stream the target DSP stream.
* \param index the position of the index on a single dimension.
* \return the the position of the index on each dimension.
* \sa dsp_stream_new
* \sa dsp_stream_set_position
* \sa dsp_stream_exec
* \sa dsp_stream_exec_multidim
*/
DLL_EXPORT int* dsp_stream_get_position(dsp_stream_p stream, int index);

/**
* \brief Execute the function callback pointed by the func field of the passed stream
* \param stream the target DSP stream.
* \return callback return value
* \sa dsp_stream_new
* \sa dsp_stream_get_position
* \sa dsp_stream_set_position
*/
DLL_EXPORT void *dsp_stream_exec(dsp_stream_p stream, void *args, ...);

/**
* \brief Crop the buffers of the stream passed as argument by reading the ROI field.
* \param stream the target DSP stream.
* \return the cropped DSP stream.
* \sa dsp_stream_new
*/
DLL_EXPORT void dsp_stream_crop(dsp_stream_p stream);

/*@}*/
/**
 * \defgroup dsp_SignalGen DSP API Signal generation functions
*/
/*@{*/

/**
* \brief Generate white noise
* \param stream the target DSP stream.
*/
DLL_EXPORT void dsp_signals_whitenoise(dsp_stream_p stream);

/**
* \brief Generate a sinusoidal wave
* \param stream the target DSP stream.
* \param samplefreq the sampling reference frequency
* \param freq the sine wave frequency
*/
DLL_EXPORT void dsp_signals_sinewave(dsp_stream_p stream, double samplefreq, double freq);

/**
* \brief Generate a sawtooth wave
* \param stream the target DSP stream.
* \param samplefreq the sampling reference frequency
* \param freq the sawtooth wave frequency
*/
DLL_EXPORT void dsp_signals_sawtoothwave(dsp_stream_p stream, double samplefreq, double freq);

/**
* \brief Generate a triangular wave
* \param stream the target DSP stream.
* \param samplefreq the sampling reference frequency
* \param freq the triangular wave frequency
*/
DLL_EXPORT void dsp_signals_triwave(dsp_stream_p stream, double samplefreq, double freq);

/**
* \brief Generate a frequency modulated wave
* \param stream the target DSP stream.
* \param samplefreq the sampling reference frequency
* \param freq the carrier wave frequency
* \param bandwidth the bandwidth of the frequency modulation
*/
DLL_EXPORT void dsp_modulation_frequency(dsp_stream_p stream, double samplefreq, double freq, double bandwidth);

/**
* \brief Generate an amplitude modulated wave
* \param stream the target DSP stream.
* \param samplefreq the sampling reference frequency
* \param freq the carrier wave frequency
*/
DLL_EXPORT void dsp_modulation_amplitude(dsp_stream_p stream, double samplefreq, double freq);

/**
* \brief Find stars into the stream
* \param stream The stream containing stars
* \param levels The level of thresholding
* \param min_size Minimum stellar size
* \param threshold Intensity treshold
* \param matrix The star shape
* \return The new dsp_stream_p structure pointer
*/
DLL_EXPORT int dsp_align_find_stars(dsp_stream_p stream, int levels, int min_size, float threshold, dsp_stream_p matrix);

/**
* \brief Limit search area to the radius around the first n stars and store those streams as children of the stream to be aligned
* \param reference The reference solved stream
* \param to_align The stream to be aligned
* \param n Stars count limit
* \param radius The search area
* \return The number of streams cropped and added
*/
DLL_EXPORT int dsp_align_crop_limit(dsp_stream_p reference, dsp_stream_p to_align, int n, int radius);

/**
* \brief Find offsets between 2 streams and extract align informations
* \param stream1 The first stream
* \param stream2 The second stream
* \param max_stars The maximum stars count allowed
* \param precision The precision used for comparison
* \param start_star Start compare from the start_star brigher star
* \return The new dsp_align_info structure pointer
*/
DLL_EXPORT int dsp_align_get_offset(dsp_stream_p stream1, dsp_stream_p stream, int max_stars, int decimals);

/**
* \brief Rotate a stream around an axis and offset
* \param stream The stream that need rotation
* \param info The dsp_align_info structure pointer containing the rotation informations
* \return The new dsp_stream_p structure pointer
*/
DLL_EXPORT void dsp_stream_rotate(dsp_stream_p stream);

/**
* \brief Traslate a stream
* \param stream The stream that need traslation
* \param info The dsp_align_info structure pointer containing the traslation informations
* \return The new dsp_stream_p structure pointer
*/
DLL_EXPORT void dsp_stream_traslate(dsp_stream_p stream);

/**
* \brief Scale a stream
* \param stream The stream that need scaling
* \param info The dsp_align_info structure pointer containing the scaling informations
* \return The new dsp_stream_p structure pointer
*/
DLL_EXPORT void dsp_stream_scale(dsp_stream_p stream);

/**
* \brief Read a FITS file and fill a dsp_stream_p with its content
* \param filename the file name.
* \param stretch 1 if the buffer intensities have to be stretched
* \return The new dsp_stream_p structure pointer
*/
DLL_EXPORT dsp_stream_p* dsp_file_read_fits(char *filename, int *channels, int stretch);

/**
* \brief Write the components dsp_stream_p array into a FITS file,
* \param filename the file name.
* \param components the number of streams in the array to be used as components 1 or 3.
* \param bpp the bit depth of the output JPEG file [8,16,32,64,-32,-64].
* \param stream the input stream to be saved
*/
DLL_EXPORT void* dsp_file_write_fits(int bpp, size_t* memsize, dsp_stream_p stream);

/**
* \brief Read a JPEG file and fill a array of dsp_stream_p with its content,
* each color channel has its own stream in this array and an additional grayscale at end will be added
* \param filename the file name.
* \param channels this value will be updated with the channel quantity into the picture.
* \param stretch 1 if the buffer intensities have to be stretched
* \return The new dsp_stream_p structure pointers array
*/
DLL_EXPORT dsp_stream_p* dsp_file_read_jpeg(char *filename, int *channels, int stretch);

/**
* \brief Write the components dsp_stream_p array into a JPEG file,
* \param filename the file name.
* \param components the number of streams in the array to be used as components 1 or 3.
* \param quality the quality of the output JPEG file 0-100.
* \param stream the input stream to be saved
*/
DLL_EXPORT void dsp_file_write_jpeg_composite(char *filename, int components, int quality, dsp_stream_p* stream);

DLL_EXPORT dsp_stream_p *dsp_stream_from_components(dsp_t* buf, int dims, int *sizes, int components);
DLL_EXPORT dsp_stream_p *dsp_buffer_rgb_to_components(void* buf, int dims, int *sizes, int components, int bpp, int stretch);
DLL_EXPORT void dsp_buffer_components_to_rgb(dsp_stream_p *stream, void* rgb, int components, int bpp);

/*@}*/
/*@}*/

#ifdef  __cplusplus
}
#endif

#endif //_DSP_H
