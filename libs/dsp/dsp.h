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
    int* center;
/// Dimensions limit of the point
    int dims;
} dsp_point;

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
/// The radius of the star
    int radius;
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
    double* buf;
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
    double lambda;
/// Sample rate of the buffers
    double samplerate;
/// Thread type for future usage
    pthread_t thread;
/// Callback function
    dsp_func_t func;
/// Regions of interest for each dimension
    dsp_region *ROI;
/// Stars or objects identified into the buffers - TODO
    dsp_star **stars;
/// Stars or objects quantity - TODO
    int star_count;
} dsp_stream, *dsp_stream_p;

/*@}*/
/**
 * \defgroup dsp_FourierTransform DSP API Fourier transform related functions
*/
/*@{*/

/**
* \brief Perform a discrete Fourier Transform of a dsp_stream
* \param stream the input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
DLL_EXPORT dsp_complex* dsp_fourier_dft(dsp_stream_p stream);

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
    (__typeof__(buf[0]))(min - dsp_stats_max(buf, len)) / 2.0 + min);\
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
    if(iratio == 0.0) iratio = 1;\
    for(k = 0; k < len; k++) {\
        buf[k] -= __mn;\
        buf[k] = (__typeof__(buf[0]))((double)buf[k] * (oratio / iratio));\
        buf[k] += (__typeof__(buf[0]))_mn;\
    }\
})

/**
* \brief Normalize the input stream to the minimum and maximum values
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \param min the clamping minimum value.
* \param max the clamping maximum value.
*/
#define dsp_buffer_normalize(buf, len, min, max)\
({\
    int k;\
    for(k = 0; k < len; k++) {\
        buf[k] = (buf[k] < min ? min : (buf[k] > max ? max : buf[k]));\
        }\
})

/**
* \brief Subtract elements of one stream from another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_sub(dsp_stream_p stream, double* in, int len);

/**
* \brief Sum elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_sum(dsp_stream_p stream, double* in, int len);

/**
* \brief Divide elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_div(dsp_stream_p stream, double* in, int len);

/**
* \brief Multiply elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_mul(dsp_stream_p stream, double* in, int len);

/**
* \brief Expose elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_pow(dsp_stream_p stream, double* in, int len);

/**
* \brief Logarithm elements of one stream using another's as base
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
DLL_EXPORT void dsp_buffer_log(dsp_stream_p stream, double* in, int len);

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
* \brief Put zero on each element of the array
* \param stream the stream on which execute
*/
DLL_EXPORT void dsp_buffer_clear(dsp_stream_p stream);

/**
* \brief Deviate forward the first input stream using the second stream as indexing reference
* \param stream the stream on which execute
* \param deviation the stream containing the deviation buffer
* \param mindeviation the deviation at 0.
* \param maxdeviation the deviation at 1.
*/
DLL_EXPORT void dsp_buffer_deviate(dsp_stream_p stream, double* deviation, double mindeviation, double maxdeviation);

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
          buf[i] = buf[j]; \
          buf[j] = _x; \
          i--; \
          j++; \
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
DLL_EXPORT double* dsp_stream_get_buffer(dsp_stream_p stream);

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
DLL_EXPORT void dsp_stream_add_star(dsp_stream_p stream, dsp_star *star);

/**
* \brief Add a child to the DSP Stream passed as argument
* \param stream the target DSP stream.
* \param child the child to add to DSP stream.
* \sa dsp_stream_new
* \sa dsp_stream_del_child
*/
DLL_EXPORT void dsp_stream_add_child(dsp_stream_p stream, dsp_stream_p child);

/**
* \brief Remove the child with index n to a DSP stream
* \param stream the target DSP stream.
* \param n the index of the dimension to remove
* \sa dsp_stream_new
* \sa dsp_stream_add_child
*/
DLL_EXPORT void dsp_stream_del_child(dsp_stream_p stream, int n);

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
DLL_EXPORT void *dsp_stream_exec(dsp_stream_p stream);

/**
* \brief Crop the buffers of the stream passed as argument by reading the ROI field.
* \param stream the target DSP stream.
* \return the cropped DSP stream.
* \sa dsp_stream_new
*/
DLL_EXPORT dsp_stream_p dsp_stream_crop(dsp_stream_p stream);

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

DLL_EXPORT dsp_stream_p dsp_find_object(dsp_stream_p stream, dsp_stream_p object, int steps);

DLL_EXPORT dsp_stream_p dsp_stream_rotate(dsp_stream_p stream, double *degrees, double *pivot);

DLL_EXPORT dsp_stream_p dsp_stream_scale(dsp_stream_p stream, double ratio);

/*@}*/
/*@}*/

#ifdef  __cplusplus
}
#endif

#endif //_DSP_H
