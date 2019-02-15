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

#ifdef  __cplusplus
extern "C" {
#endif

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

/**
 * \defgroup DSP Digital Signal Processing API
*
* The DSP API is used for processing monodimensional or multidimensional buffers,<br>
* converting array element types, generate statistics, extract informations from buffers, convolute or<br>
* cross-correlate different single or multi dimensional streams, rotate, scale, crop images.<br>
* The DSP API can also be used for astrometrical purposes, by locating the ALT/AZ location of celestial<br>
* objects or finding or recognizing a star into the field of view.<br>
*
* \author Ilia Platone
*/
/*@{*/


/**
 * \defgroup DSP_Types DSP API Types
*/
/*@{*/

/**
* \brief DFT Conversion type
*/
typedef enum _dsp_conversiontype
{
    magnitude = 0,
    magnitude_dbv = 1,
    magnitude_root = 2,
    magnitude_square = 3,
    phase_degrees = 4,
    phase_radians = 5,
} dsp_conversiontype;

/**
* \brief DSP Point Type
*/
typedef struct _dsp_point
{
    int x;
    int y;
} dsp_point;

/**
* \brief DSP Region Type
*/
typedef struct _dsp_region
{
    int start;
    int len;
} dsp_region;

/**
* \brief DSP Star Type
*/
typedef struct _dsp_star
{
    dsp_point center;
    int radius;
} dsp_star;

/**
* \brief Multi-dimensional processing delegate function
*/
typedef void *(*dsp_func_t) (void *);

/**
* \brief DSP Stream Type
*/
typedef struct dsp_stream_s
{
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
    dsp_region *ROI;
    dsp_star *stars;
} dsp_stream, *dsp_stream_p;

/*@}*/
/**
 * \defgroup DSP_FourierTransform DSP API Fourier transform related functions
*/
/*@{*/

/**
* \brief Create a spectrum from a double array of values
* \param in the input stream.
* \param conversion the output magnitude conversion type.
* \param size The dimension of the spectrum.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_fft_spectrum(dsp_stream_p stream, dsp_conversiontype conversion, int size);

/**
* \brief Shift a stream on each dimension
* \param in the input buffer.
* \param dims the number of dimensions of the input buffer.
* \param sizes array with the lengths of each dimension of the input buffer.
* \return the output buffer if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_fft_shift(double* in, int dims, int* sizes);

/**
* \brief Discrete Fourier Transform of a dsp_stream
* \param stream the input stream.
* \param sign the direction of the Fourier transform.
* \param conversion the output magnitude conversion type.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_fft_dft(dsp_stream_p stream, int sign, dsp_conversiontype conversion);

/*@}*/
/**
 * \defgroup DSP_Filters DSP API Linear buffer filtering functions
*/
/*@{*/
/**
* \brief A square law filter
* \param stream the input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_filter_squarelaw(dsp_stream_p stream);

/**
* \brief A low pass filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param frequency the cutoff frequency of the filter.
* \param q the cutoff slope.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_filter_lowpass(dsp_stream_p stream, double samplingfrequency, double frequency, double q);

/**
* \brief A high pass filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param frequency the cutoff frequency of the filter.
* \param q the cutoff slope.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_filter_highpass(dsp_stream_p stream, double samplingfrequency, double frequency, double q);

/**
* \brief A band pass filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param LowFrequency the high-pass cutoff frequency of the filter.
* \param HighFrequency the low-pass cutoff frequency of the filter.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_filter_bandpass(dsp_stream_p stream, double samplingfrequency, double LowFrequency, double HighFrequency);

/**
* \brief A band reject filter
* \param stream the input stream.
* \param samplingfrequency the sampling frequency of the input stream.
* \param LowFrequency the high-pass cutoff frequency of the filter.
* \param HighFrequency the low-pass cutoff frequency of the filter.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_filter_bandreject(dsp_stream_p stream, double samplingfrequency, double LowFrequency, double HighFrequency);

/*@}*/
/**
 * \defgroup DSP_Convolution DSP API Convolution and cross-correlation functions
*/
/*@{*/
/**
* \brief A cross-convolution processor
* \param in1 the first input stream.
* \param in2 the second input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_convolution_convolution(dsp_stream_p stream1, dsp_stream_p stream2);

/*@}*/
/**
 * \defgroup DSP_Stats DSP API Buffer statistics functions
*/
/*@{*/

/**
* \brief Gets minimum, mid, and maximum values of the input stream
* \param in the input stream.
* \param len the length of the input stream.
* \param min the minimum value.
* \param max the maximum value.
* \return the mid value (max - min) / 2 + min.
* Return mid if success.
*/
extern double dsp_stats_minmidmax(double* in, int len, double* min, double* max);

/**
* \brief A mean calculator
* \param in the input stream.
* \param len the length of the input stream.
* \return the mean value of the stream.
* Return mean if success.
*/
extern double dsp_stats_mean(double* in, int len);

/**
* \brief Counts value occurrences into stream
* \param in the input stream.
* \param len the length of the input stream.
* \param val the value to count.
* \param prec the decimal precision.
* \return the mean value of the stream.
* Return mean if success.
*/
extern int dsp_stats_val_count(double* in, int len, double val);

/*@}*/
/**
 * \defgroup DSP_Buffers DSP API Buffer editing functions
*/
/*@{*/

/**
* \brief Compare two streams
* \param in1 the first input stream.
* \param len1 the length of the first input stream.
* \param in2 the second input stream.
* \param len2 the length of the second input stream.
* \return the sum of the subtraction of each element of both streams
*/
extern double dsp_buffer_compare(double* in1, int len1, double* in2, int len2);

/**
* \brief Subtract mean from stream
* \param in the input stream.
* \param out the output stream.
* \param len the length of the input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_removemean(double* in, int len);

/**
* \brief Stretch minimum and maximum values of the input stream
* \param in the input stream.
* \param out the output stream.
* \param len the length of the input stream.
* \param min the desired minimum value.
* \param max the desired maximum value.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_stretch(double* in, int len, double min, double max);

/**
* \brief Normalize the input stream to the minimum and maximum values
* \param in the input stream.
* \param len the length of the input stream.
* \param min the clamping minimum value.
* \param max the clamping maximum value.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_normalize(double* in, int len, double min, double max);

/**
* \brief Subtract elements of one stream from another's
* \param in1 the first input stream.
* \param len1 the length of the first input stream.
* \param in2 the second input stream.
* \param len2 the length of the second input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_sub(double* in1, int len1, double* in2, int len2);

/**
* \brief Sum elements of one stream to another's
* \param in1 the first input stream.
* \param len1 the length of the first input stream.
* \param in2 the second input stream.
* \param len2 the length of the second input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_sum(double* in1, int len1, double* in2, int len2);

/**
* \brief Divide elements of one stream to another's
* \param in1 the first input stream.
* \param len1 the length of the first input stream.
* \param in2 the second input stream.
* \param len2 the length of the second input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_div(double* in1, int len1, double* in2, int len2);

/**
* \brief Multiply elements of one stream to another's
* \param in1 the first input stream.
* \param len1 the length of the first input stream.
* \param in2 the second input stream.
* \param len2 the length of the second input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_mul(double* in1, int len1, double* in2, int len2);

/**
* \brief Expose elements of the input stream to the given power
* \param in the input stream.
* \param len the length of the input stream.
* \param val the nth power to expose each element.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_pow(double* in, int len, double val);

/**
* \brief Root elements of the input stream to the given power
* \param in the input stream.
* \param len the length of the input stream.
* \param val the nth power to root each element.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_root(double* in, int len, double val);

/**
* \brief Subtract a value from elements of the input stream
* \param in the Numerators input stream.
* \param out the output stream.
* \param len the length of the input stream.
* \param val the value to be subtracted.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_sub1(double* in, int len, double val);

/**
* \brief Subtract each element of the input stream a value
* \param in the Numerators input stream.
* \param out the output stream.
* \param len the length of the input stream.
* \param val the value to be subtracted.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_1sub(double* in, int len, double val);

/**
* \brief Sum elements of the input stream to a value
* \param in the first input stream.
* \param out the output stream.
* \param len the length of the input stream.
* \param val the value used for this operation.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_sum1(double* in, int len, double val);

/**
* \brief Divide elements of the input stream to a value
* \param in the Numerators input stream.
* \param out the output stream.
* \param len the length of the input stream.
* \param val the denominator.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_div1(double* in, int len, double val);

/**
* \brief Divide a value to each element of the input stream
* \param in the Numerators input stream.
* \param out the output stream.
* \param len the length of the input stream.
* \param val the nominator.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_1div(double* in, int len, double val);

/**
* \brief Multiply elements of the input stream to a value
* \param in the first input stream.
* \param out the output stream.
* \param len the length of the input stream.
* \param val the value used for this operation.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_mul1(double* in, int len, double val);

/**
* \brief Median elements of the inut stream
* \param in the input stream.
* \param out the output stream.
* \param len the length of the input stream.
* \param size the length of the median.
* \param median the location of the median value.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_median(double* in, int len, int size, int median);

/**
* \brief Histogram of the inut stream
* \param in the input stream.
* \param out the output stream.
* \param len the length of the input stream.
* \param size the length of the median.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_histogram(double* in, int len, int size);

/**
* \brief Put zero on each element of the array
* \param out the input stream.
* \param len the length of the input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_zerofill(double* out, int len);

/**
* \brief Sum each buffer's element with its previous in a fibonacci style
* \param in the input stream.
* \param len the length of the input stream.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_val_sum(double* in, int len);

/**
* \brief Deviate forward the first input stream using the second stream as indexing reference
* \param in1 the first input stream.
* \param len1 the length of the first input stream.
* \param in2 the second input stream.
* \param len2 the length of the second input stream.
* \param mindeviation the deviation at 0.
* \param maxdeviation the deviation at 1.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_buffer_deviate(double* in1, int len1, double* in2, int len2, double mindeviation, double maxdeviation);

/**
* \brief Reverse the order of the input buffer elements
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
#ifndef dsp_convert
#define dsp_convert(in, out, len) \
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
* \brief Set the input buffer length on the stream passed as argument
* \param stream the target DSP stream.
* \param len the new length of the input buffer.
* \return the input buffer
*/
extern double *dsp_stream_set_input_buffer_len(dsp_stream_p stream, int len);

/**
* \brief Set the output buffer length on the stream passed as argument
* \param stream the target DSP stream.
* \param len the new length of the output buffer.
* \return the output buffer
*/
extern double *dsp_stream_set_output_buffer_len(dsp_stream_p stream, int len);

/**
* \brief Set the input buffer of the stream passed as argument to a specific memory location
* \param stream the target DSP stream.
* \param buffer the new location of the input buffer.
* \param len the new length of the input buffer.
* \return the input buffer
*/
extern double *dsp_stream_set_input_buffer(dsp_stream_p stream, void *buffer, int len);

/**
* \brief Set the output buffer of the stream passed as argument to a specific memory location
* \param stream the target DSP stream.
* \param buffer the new location of the output buffer.
* \param len the new length of the output buffer.
* \return the output buffer
*/
extern double *dsp_stream_set_output_buffer(dsp_stream_p stream, void *buffer, int len);

/**
* \brief Return the input buffer of the stream passed as argument
* \param stream the target DSP stream.
* \return the input buffer
*/
extern double *dsp_stream_get_input_buffer(dsp_stream_p stream);

/**
* \brief Return the output buffer of the stream passed as argument
* \param stream the target DSP stream.
* \return the output buffer
*/
extern double *dsp_stream_get_output_buffer(dsp_stream_p stream);

/**
* \brief Swap input and output buffers of the passed stream
* \param stream the target DSP stream.
*/
extern void dsp_stream_free_input_buffer(dsp_stream_p stream);

/**
* \brief Swap input and output buffers of the passed stream
* \param stream the target DSP stream.
*/
extern void dsp_stream_free_output_buffer(dsp_stream_p stream);

/**
* \brief Allocate a new DSP stream type
* \return the newly created DSP stream type
*/
extern dsp_stream_p dsp_stream_new();

/**
* \brief Create a copy of the DSP stream passed as argument
* \return the copy of the DSP stream
*/
extern dsp_stream_p dsp_stream_copy(dsp_stream_p stream);

/**
* \brief Swap input and output buffers of the passed stream
* \param stream the target DSP stream.
*/
extern void dsp_stream_add_child(dsp_stream_p stream, dsp_stream_p child);

/**
* \brief Swap input and output buffers of the passed stream
* \param stream the target DSP stream.
*/
extern void dsp_stream_add_dim(dsp_stream_p stream, int size);

/**
* \brief Swap input and output buffers of the passed stream
* \param stream the target DSP stream.
*/
extern void dsp_stream_free(dsp_stream_p stream);

/**
* \brief Update the pos field of the DSP stream passed as argument by reading the index field
* \param stream the target DSP stream.
* \return the updated DSP stream.
*/
extern dsp_stream_p dsp_stream_set_position(dsp_stream_p stream);

/**
* \brief Update the index field of the DSP stream passed as argument by reading the pos field
* \param stream the target DSP stream.
* \return the updated DSP stream.
*/
extern dsp_stream_p dsp_stream_get_position(dsp_stream_p stream);

/**
* \brief Execute the function callback pointed by the func field of the passed stream
* \param stream the target DSP stream.
* \return the return value of the function delegate.
*/
extern void *dsp_stream_exec(dsp_stream_p stream);

/**
* \brief Execute the function callback pointed by the func field of the passed stream
* by increasing the index value and updating the position each time.
* the function delegate should use the pos* field to obtain the current position on each dimension.
* \param stream the target DSP stream.
* \return the return value of the function delegate.
*/
extern void *dsp_stream_exec_multidim(dsp_stream_p stream);

/**
* \brief Crop the buffers of the stream passed as argument by reading the ROI field.
* \param stream the target DSP stream.
* \return the cropped DSP stream.
*/
extern dsp_stream_p dsp_stream_crop(dsp_stream_p stream);

/**
* \brief Swap input and output buffers of the passed stream
* \param stream the target DSP stream.
*/
extern void dsp_stream_mul(dsp_stream_p in1, dsp_stream_p in2);

/**
* \brief Swap input and output buffers of the passed stream
* \param stream the target DSP stream.
*/
extern void dsp_stream_sum(dsp_stream_p in1, dsp_stream_p in2);

/**
* \brief Swap input and output buffers of the passed stream
* \param stream the target DSP stream.
*/
extern void dsp_stream_swap_buffers(dsp_stream_p stream);

/*@}*/
/**
 * \defgroup DSP_SignalGen DSP API Signal generation functions
*/
/*@{*/

/**
* \brief Generate a sinusoidal wave
* \param len the length of the output buffer
* \param samplefreq the sampling reference frequency
* \param freq the sine wave frequency
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_signals_sinewave(int len, double samplefreq, double freq);

/**
* \brief Generate a sawtooth wave
* \param len the length of the output buffer
* \param samplefreq the sampling reference frequency
* \param freq the sawtooth wave frequency
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_signals_sawteethwave(int len, double samplefreq, double freq);

/**
* \brief Generate a triangular wave
* \param len the length of the output buffer
* \param samplefreq the sampling reference frequency
* \param freq the triangular wave frequency
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_signals_triwave(int len, double samplefreq, double freq);

/**
* \brief Generate a frequency modulated wave
* \param in the input stream.
* \param len the length of the output buffer
* \param samplefreq the sampling reference frequency
* \param freq the carrier wave frequency
* \param bandwidth the bandwidth of the frequency modulation
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_modulation_frequency(double* in, int len, double samplefreq, double freq, double bandwidth);

/**
* \brief Generate an amplitude modulated wave
* \param in the input stream.
* \param len the length of the output buffer
* \param samplefreq the sampling reference frequency
* \param freq the carrier wave frequency
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
extern double* dsp_modulation_amplitude(double* in, int len, double samplefreq, double freq);

/*@}*/
/*@}*/

#ifdef  __cplusplus
}
#endif

#endif //_DSP_H
