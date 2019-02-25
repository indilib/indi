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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
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
* \sa dsp_fft_dft
* \sa dsp_fft_complex_to_magnitude
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
typedef void *(*dsp_func_t) (void *);

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
/// Parent stream if the current is its child
    struct dsp_stream_t* parent;
/// Children streams of the current one
    struct dsp_stream_t** children;
/// Children streams count
    int child_count;
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
    dsp_star *stars;
} dsp_stream, *dsp_stream_p;

/*@}*/
/**
 * \defgroup DSP_FourierTransform DSP API Fourier transform related functions
*/
/*@{*/

/**
* \brief Discrete Fourier Transform of a dsp_stream
* \param stream the input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern dsp_complex* dsp_fft_dft(dsp_stream_p stream);

/**
* \brief Calculate a complex number's magnitude
* \param n the input complex.
* \return the magnitude of the given number
*/
extern double dsp_fft_complex_to_magnitude(dsp_complex n);

/**
* \brief Calculate a complex number's phase
* \param n the input complex.
* \return the phase of the given number
*/
extern double dsp_fft_complex_to_phase(dsp_complex n);

/**
* \brief Calculate a complex number's array magnitudes
* \param in the input complex number array.
* \param len the input array length.
* \return the array filled with the magnitudes
*/
extern double*  dsp_fft_complex_array_to_magnitude(dsp_complex* in, int len);

/**
* \brief Calculate a complex number's array phases
* \param in the input complex number array.
* \param len the input array length.
* \return the array filled with the phases
*/
extern double*  dsp_fft_complex_array_to_phase(dsp_complex* in, int len);

/*@}*/
/**
 * \defgroup DSP_Filters DSP API Linear buffer filtering functions
*/
/*@{*/
/**
* \brief A square law filter
* \param stream the input stream.
*/
extern void dsp_filter_squarelaw(dsp_stream_p stream);

/**
* \brief A low pass filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param frequency the cutoff frequency of the filter.
* \param q the cutoff slope.
*/
extern void dsp_filter_lowpass(dsp_stream_p stream, double samplingfrequency, double frequency, double q);

/**
* \brief A high pass filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param frequency the cutoff frequency of the filter.
* \param q the cutoff slope.
*/
extern void dsp_filter_highpass(dsp_stream_p stream, double samplingfrequency, double frequency, double q);

/**
* \brief A band pass filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param LowFrequency the high-pass cutoff frequency of the filter.
* \param HighFrequency the low-pass cutoff frequency of the filter.
*/
extern void dsp_filter_bandpass(dsp_stream_p stream, double samplingfrequency, double LowFrequency, double HighFrequency);

/**
* \brief A band reject filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param LowFrequency the high-pass cutoff frequency of the filter.
* \param HighFrequency the low-pass cutoff frequency of the filter.
*/
extern void dsp_filter_bandreject(dsp_stream_p stream, double samplingfrequency, double LowFrequency, double HighFrequency);

/*@}*/
/**
 * \defgroup DSP_Convolution DSP API Convolution and cross-correlation functions
*/
/*@{*/
/**
* \brief A cross-convolution processor
* \param stream1 the first input stream.
* \param stream2 the second input stream.
*/
extern void dsp_convolution_convolution(dsp_stream_p stream1, dsp_stream_p stream2);

/*@}*/
/**
 * \defgroup DSP_Stats DSP API Buffer statistics functions
*/
/*@{*/

/**
* \brief Gets minimum, mid, and maximum values of the input stream
* \param stream the stream on which execute
* \param min the minimum value.
* \param max the maximum value.
* \return the mid value (max - min) / 2 + min.
*/
extern double dsp_stats_minmidmax(dsp_stream_p stream, double* min, double* max);

/**
* \brief A mean calculator
* \param stream the stream on which execute
* \return the mean value of the stream.
*/
extern double dsp_stats_mean(dsp_stream_p stream);

/**
* \brief Counts value occurrences into stream
* \param stream the stream on which execute
* \param val the value to count.
* \return the count of the value of the stream.
*/
extern int dsp_stats_val_count(dsp_stream_p stream, double val);

/**
* \brief Histogram of the inut stream
* \param stream the stream on which execute
* \param size the length of the median.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_stats_histogram(dsp_stream_p stream, int size);

/**
* \brief Sum each buffer's element with its previous in a fibonacci style
* \param stream the stream on which execute
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_stats_val_sum(dsp_stream_p stream);

/**
* \brief Compare two streams
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
* \return the sum of the subtraction of each element of both streams
*/
extern double dsp_stats_compare(dsp_stream_p stream, double* in, int len);

/*@}*/
/**
 * \defgroup DSP_Buffers DSP API Buffer editing functions
*/
/*@{*/

/**
* \brief Shift a stream on each dimension
* \param stream the input stream.
*/
extern void dsp_buffer_shift(dsp_stream_p stream);

/**
* \brief Subtract mean from stream
* \param stream the stream on which execute
*/
extern void dsp_buffer_removemean(dsp_stream_p stream);

/**
* \brief Stretch minimum and maximum values of the input stream
* \param stream the stream on which execute
* \param min the desired minimum value.
* \param max the desired maximum value.
*/
extern void dsp_buffer_stretch(dsp_stream_p stream, double min, double max);

/**
* \brief Normalize the input stream to the minimum and maximum values
* \param stream the stream on which execute
* \param min the clamping minimum value.
* \param max the clamping maximum value.
*/
extern void dsp_buffer_normalize(dsp_stream_p stream, double min, double max);

/**
* \brief Subtract elements of one stream from another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
extern void dsp_buffer_sub(dsp_stream_p stream, double* in, int len);

/**
* \brief Sum elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
extern void dsp_buffer_sum(dsp_stream_p stream, double* in, int len);

/**
* \brief Divide elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
extern void dsp_buffer_div(dsp_stream_p stream, double* in, int len);

/**
* \brief Multiply elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
extern void dsp_buffer_mul(dsp_stream_p stream, double* in, int len);

/**
* \brief Expose elements of one stream to another's
* \param stream the stream on which execute
* \param in the buffer operand.
* \param len the length of the buffer
*/
extern void dsp_buffer_pow(dsp_stream_p stream, double* in, int len);

/**
* \brief Subtract a value from elements of the input stream
* \param stream the stream on which execute
* \param val the value to be subtracted.
*/
extern void dsp_buffer_sub1(dsp_stream_p stream, double val);

/**
* \brief Subtract each element of the input stream a value
* \param stream the stream on which execute
* \param val the value to be subtracted.
*/
extern void dsp_buffer_1sub(dsp_stream_p stream, double val);

/**
* \brief Sum elements of the input stream to a value
* \param stream the stream on which execute
* \param val the value used for this operation.
*/
extern void dsp_buffer_sum1(dsp_stream_p stream, double val);

/**
* \brief Divide elements of the input stream to a value
* \param stream the stream on which execute
* \param val the denominator.
*/
extern void dsp_buffer_div1(dsp_stream_p stream, double val);

/**
* \brief Divide a value to each element of the input stream
* \param stream the stream on which execute
* \param val the nominator.
*/
extern void dsp_buffer_1div(dsp_stream_p stream, double val);

/**
* \brief Multiply elements of the input stream to a value
* \param stream the stream on which execute
* \param val the value used for this operation.
*/
extern void dsp_buffer_mul1(dsp_stream_p stream, double val);

/**
* \brief Expose elements of the input stream to the given power
* \param stream the stream on which execute
* \param val the nth power to expose each element.
*/
extern void dsp_buffer_pow1(dsp_stream_p stream, double val);

/**
* \brief Median elements of the inut stream
* \param stream the stream on which execute
* \param size the length of the median.
* \param median the location of the median value.
*/
extern void dsp_buffer_median(dsp_stream_p stream, int size, int median);

/**
* \brief Put zero on each element of the array
* \param stream the stream on which execute
*/
extern void dsp_buffer_zerofill(dsp_stream_p stream);

/**
* \brief Deviate forward the first input stream using the second stream as indexing reference
* \param stream the stream on which execute
* \param deviation the stream containing the deviation buffer
* \param mindeviation the deviation at 0.
* \param maxdeviation the deviation at 1.
*/
extern void dsp_buffer_deviate(dsp_stream_p stream, dsp_stream_p deviation, double mindeviation, double maxdeviation);

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
 * \defgroup DSP_DSPStream DSP API Stream type management functions
*/
/*@{*/

/**
* \brief Allocate a buffer with length len on the stream passed as argument
* \param stream the target DSP stream.
* \param len the new length of the buffer.
*/
extern void dsp_stream_alloc_buffer(dsp_stream_p stream, int len);

/**
* \brief Set the buffer of the stream passed as argument to a specific memory location
* \param stream the target DSP stream.
* \param buffer the new location of the buffer.
* \param len the new length of the buffer.
*/
extern void dsp_stream_set_buffer(dsp_stream_p stream, void *buffer, int len);

/**
* \brief Return the buffer of the stream passed as argument
* \param stream the target DSP stream.
* \return the buffer
*/
extern double* dsp_stream_get_buffer(dsp_stream_p stream);

/**
* \brief Free the buffer of the DSP Stream passed as argument
* \param stream the target DSP stream.
*/
extern void dsp_stream_free_buffer(dsp_stream_p stream);

/**
* \brief Allocate a new DSP stream type
* \return the newly created DSP stream type
* \sa dsp_stream_free
*/
extern dsp_stream_p dsp_stream_new();

/**
* \brief Free the DSP stream passed as argument
* \param stream the target DSP stream.
* \sa dsp_stream_new
*/
extern void dsp_stream_free(dsp_stream_p stream);

/**
* \brief Create a copy of the DSP stream passed as argument
* \return the copy of the DSP stream
* \sa dsp_stream_new
*/
extern dsp_stream_p dsp_stream_copy(dsp_stream_p stream);

/**
* \brief Add a child to the DSP Stream passed as argument
* \param stream the target DSP stream.
* \param child the child to add to DSP stream.
* \sa dsp_stream_new
* \sa dsp_stream_del_child
*/
extern void dsp_stream_add_child(dsp_stream_p stream, dsp_stream_p child);

/**
* \brief Remove the child with index n to a DSP stream
* \param stream the target DSP stream.
* \param n the index of the dimension to remove
* \sa dsp_stream_new
* \sa dsp_stream_add_child
*/
extern void dsp_stream_del_child(dsp_stream_p stream, int n);

/**
* \brief Add a dimension with length len to a DSP stream
* \param stream the target DSP stream.
* \param len the size of the dimension to add
* \sa dsp_stream_new
* \sa dsp_stream_del_dim
*/
extern void dsp_stream_add_dim(dsp_stream_p stream, int len);

/**
* \brief Remove the dimension with index n to a DSP stream
* \param stream the target DSP stream.
* \param n the index of the dimension to remove
* \sa dsp_stream_new
* \sa dsp_stream_add_dim
*/
extern void dsp_stream_del_dim(dsp_stream_p stream, int n);

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
extern int dsp_stream_set_position(dsp_stream_p stream, int *pos);

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
extern int* dsp_stream_get_position(dsp_stream_p stream, int index);

/**
* \brief Execute the function callback pointed by the func field of the passed stream
* \param stream the target DSP stream.
* \return callback return value
* \sa dsp_stream_new
* \sa dsp_stream_get_position
* \sa dsp_stream_set_position
*/
extern void *dsp_stream_exec(dsp_stream_p stream);

/**
* \brief Execute the function callback pointed by the func field of the passed stream
* by increasing the index value and updating the position each time.
* the function delegate should use the pos* field to obtain the current position on each dimension.
* \param stream the target DSP stream.
* \sa dsp_stream_new
* \sa dsp_stream_get_position
* \sa dsp_stream_set_position
*/
extern void dsp_stream_exec_multidim(dsp_stream_p stream);

/**
* \brief Crop the buffers of the stream passed as argument by reading the ROI field.
* \param stream the target DSP stream.
* \return the cropped DSP stream.
* \sa dsp_stream_new
*/
extern dsp_stream_p dsp_stream_crop(dsp_stream_p stream);

/*@}*/
/**
 * \defgroup DSP_SignalGen DSP API Signal generation functions
*/
/*@{*/

/**
* \brief Generate a sinusoidal wave
* \param stream the target DSP stream.
* \param samplefreq the sampling reference frequency
* \param freq the sine wave frequency
*/
extern void dsp_signals_sinewave(dsp_stream_p stream, double samplefreq, double freq);

/**
* \brief Generate a sawtooth wave
* \param stream the target DSP stream.
* \param samplefreq the sampling reference frequency
* \param freq the sawtooth wave frequency
*/
extern void dsp_signals_sawtoothwave(dsp_stream_p stream, double samplefreq, double freq);

/**
* \brief Generate a triangular wave
* \param stream the target DSP stream.
* \param samplefreq the sampling reference frequency
* \param freq the triangular wave frequency
*/
extern void dsp_signals_triwave(dsp_stream_p stream, double samplefreq, double freq);

/**
* \brief Generate a frequency modulated wave
* \param stream the target DSP stream.
* \param samplefreq the sampling reference frequency
* \param freq the carrier wave frequency
* \param bandwidth the bandwidth of the frequency modulation
*/
extern void dsp_modulation_frequency(dsp_stream_p stream, double samplefreq, double freq, double bandwidth);

/**
* \brief Generate an amplitude modulated wave
* \param stream the target DSP stream.
* \param samplefreq the sampling reference frequency
* \param freq the carrier wave frequency
*/
extern void dsp_modulation_amplitude(dsp_stream_p stream, double samplefreq, double freq);

/*@}*/
/*@}*/

#ifdef  __cplusplus
}
#endif

#endif //_DSP_H
