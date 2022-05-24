/*   libDSP - a digital signal processing library
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

#ifndef _DSP_H
#define _DSP_H

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT extern
#endif

#ifdef __linux__
#include <endian.h>
#else
#define __bswap_16(a) __builtin_bswap16(a)
#define __bswap_32(a) __builtin_bswap32(a)
#define __bswap_64(a) __builtin_bswap64(a)
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
#include <fftw3.h>

/**
 * \defgroup DSP Digital Signal Processing API
*
* The DSP API is a collection of essential routines used in astronomy signal processing<br>
*
* The DSP API is used for processing monodimensional or multidimensional buffers,<br>
* generating, tranforming, stack, processing buffers, aligning, convoluting arrays,<br>
* converting array element types, generate statistics, extract informations from buffers, convolute or<br>
* cross-correlate different single or multi dimensional streams, rotate, scale, crop images.<br>
*
* \author Ilia Platone
* \version 1.0.0
* \date 2017-2022
* \copyright GNU Lesser GPL3 Public License.
*/
/**\{*/
/**
 * \defgroup DSP_Defines DSP API defines
*/
/**\{*/
#define DSP_MAX_STARS 200
#define dsp_t double
#define dsp_t_max 255
#define dsp_t_min -dsp_t_max

/**
* \brief get/set the maximum number of threads allowed
* \param value if greater than 1, set a maximum number of threads allowed
* \return The current or new number of threads allowed during runtime
*/
DLL_EXPORT unsigned long int dsp_max_threads(unsigned long value);

#ifndef DSP_DEBUG
#define DSP_DEBUG
/**
* \brief set the debug level
* \param value the debug level
*/
DLL_EXPORT void dsp_set_debug_level(int value);
/**
* \brief get the debug level
* \return The current debug level
*/
DLL_EXPORT int dsp_get_debug_level();
/**
* \brief set the application name
* \param name the application name to be printed on logs
*/
DLL_EXPORT void dsp_set_app_name(char* name);
/**
* \brief get the application name
* \return The current application name printed on logs
*/
DLL_EXPORT char* dsp_get_app_name();
/**
* \brief set the output log streeam
* \param f The FILE stream pointer to set as standard output
*/
DLL_EXPORT void dsp_set_stdout(FILE *f);
/**
* \brief set the error log streeam
* \param f The FILE stream pointer to set as standard error
*/
DLL_EXPORT void dsp_set_stderr(FILE *f);

/**
* \brief log a message to the error or output streams
* \param x The log level
* \param str The string to print
*/
DLL_EXPORT void dsp_print(int x, char* str);

#define DSP_DEBUG_INFO 0
#define DSP_DEBUG_ERROR 1
#define DSP_DEBUG_WARNING 2
#define DSP_DEBUG_DEBUG 3
#define pdbg(x, ...) ({ \
char str[500]; \
struct timespec ts; \
time_t t = time(NULL); \
struct tm tm = *localtime(&t); \
clock_gettime(CLOCK_REALTIME, &ts); \
sprintf(str, "[%04d-%02d-%02dT%02d:%02d:%02d.%03ld ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec/1000000); \
switch(x) { \
    case DSP_DEBUG_ERROR: \
    sprintf(&str[strlen(str)], "ERRO]"); \
        break; \
    case DSP_DEBUG_WARNING: \
    sprintf(&str[strlen(str)], "WARN]"); \
        break; \
    case DSP_DEBUG_DEBUG: \
    sprintf(&str[strlen(str)], "DEBG]"); \
        break; \
    default: \
    sprintf(&str[strlen(str)], "INFO]"); \
        break; \
} \
if(dsp_get_app_name() != NULL) \
    sprintf(&str[strlen(str)], "[%s]", dsp_get_app_name()); \
sprintf(&str[strlen(str)], " "); \
sprintf(&str[strlen(str)], __VA_ARGS__); \
dsp_print(x, str); \
})
#define pinfo(...) pdbg(DSP_DEBUG_INFO, __VA_ARGS__)
#define perr(...) pdbg(DSP_DEBUG_ERROR, __VA_ARGS__)
#define pwarn(...) pdbg(DSP_DEBUG_WARNING, __VA_ARGS__)
#define pgarb(...) pdbg(DSP_DEBUG_DEBUG, __VA_ARGS__)
#define pfunc pgarb("%s\n", __func__)
#define start_gettime
#define end_gettime
#else
#define pinfo(...)
#define perr(...)
#define pwarn(...)
#define pgarb(...)
#define pfunc(...)
#define start_gettime(...)
#define end_gettime(...)
#endif


///if min() is not present you can use this one
#ifndef Min
#define Min(a,b) \
   ({ __typeof (a) _a = (a); \
       __typeof (a) _b = (b); \
     _a < _b ? _a : _b; })
#endif
///if max() is not present you can use this one
#ifndef Max
#define Max(a,b) \
   ({ __typeof (a) _a = (a); \
       __typeof (a) _b = (b); \
     _a > _b ? _a : _b; })
#endif
///Logarithm of a with arbitrary base b
#ifndef Log
#define Log(a,b) \
( log(a) / log(b) )
#endif
#ifndef DSP_ALIGN_TRANSLATED
///The stream is translated by the reference
#define DSP_ALIGN_TRANSLATED 1
#endif
#ifndef DSP_ALIGN_SCALED
///The stream is scaled by the reference
#define DSP_ALIGN_SCALED 2
#endif
#ifndef DSP_ALIGN_ROTATED
///The stream is rotated by the reference
#define DSP_ALIGN_ROTATED 4
#endif
#ifndef DSP_ALIGN_NO_MATCH
///No matches were found during comparison
#define DSP_ALIGN_NO_MATCH 8
#endif
/**\}*/
/**
 * \defgroup DSP_Types DSP API types
*/
/**\{*/

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
* \brief A star or object contained into a buffer
*/
typedef struct dsp_star_t
{
    /// The center of the star
    dsp_point center;
    /// The diameter of the star
    double diameter;
    /// The name of the star
    char name[150];
} dsp_star;

/**
* \brief A star or object contained into a buffer
*/
typedef struct dsp_triangle_t
{
    /// The index of the triangle
    double index;
    /// The dimensions of the triangle
    int dims;
    /// The inclination of the triangle
    double theta;
    /// The sizes of the triangle
    double *sizes;
    /// The sizes of the triangle
    double *ratios;
    /// The stars of the triangle
    dsp_star *stars;
} dsp_triangle;

/**
* \brief Alignment informations needed
*/
typedef struct dsp_align_info_t
{
    /// Translation offset
    double* offset;
    /// Center of rotation coordinates
    double* center;
    /// Rotational offset
    double* radians;
    /// Scaling factor
    double* factor;
    /// Dimensions limit
    int dims;
    /// Reference triangles
    dsp_triangle triangles[2];
    /// Triangles quantity
    int triangles_count;
    /// Match score
    double score;
    /// Decimals
    double decimals;
    /// Errors
    int err;
} dsp_align_info;

/**
* \brief Complex number array struct, used in Fourier Transform functions
* \sa dsp_fourier_dft
*/
typedef union
{
    /// Complex struct type array
    struct
    {
        /// Real part of the complex number
        double real;
        /// Imaginary part of the complex number
        double imaginary;
    } *complex;
    /// Complex number type array used with libFFTW
    fftw_complex *fftw;
    /// Linear double array containing complex numbers
    double *buf;
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
* \brief The location type
*/
typedef union dsp_location_t
{
    /// The location in xyz coordinates
    struct
    {
        double x;
        double y;
        double z;
    } xyz;
    /// The location in geographic coordinates
    struct
    {
        double lon;
        double lat;
        double el;
    } geographic;
    /// A 3d double array containing the location
    double coordinates[3];
} dsp_location;

/**
* \brief Multi-dimensional processing delegate function
*/
typedef void *(*dsp_func_t) (void *, ...);

/**
* \brief Contains a set of informations and data relative to a buffer and how to use it
* \sa dsp_stream_new
* \sa dsp_stream_add_dim
* \sa dsp_stream_del_dim
* \sa dsp_stream_alloc_buffer
* \sa dsp_stream_copy
* \sa dsp_stream_free_buffer
* \sa dsp_stream_free
*/
typedef struct dsp_stream_t
{
    /// Friendly name of the stream
    char name[128];
    /// Increments by one on the copied stream
    int is_copy;
    /// The buffers length
    int len;
    /// Number of dimensions of the buffers
    int dims;
    /// Sizes of each dimension
    int* sizes;
    /// buffer
    dsp_t* buf;
    /// Fourier transform
    dsp_complex dft;
    /// Optional argument for the func() callback
    void *arg;
    /// The parent stream
    struct dsp_stream_t* parent;
    /// Children streams
    struct dsp_stream_t** children;
    /// Children streams count
    int child_count;
    /// Location coordinates pointer, can be extended to the main buffer size as location companion
    dsp_location* location;
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
    /// Fourier transform magnitude
    struct dsp_stream_t *magnitude;
    /// Fourier transform phase
    struct dsp_stream_t *phase;
    /// Regions of interest for each dimension
    dsp_region *ROI;
    /// Stars or objects identified into the buffers - TODO
    dsp_star *stars;
    /// Stars or objects quantity
    int stars_count;
    /// Triangles of stars or objects
    dsp_triangle *triangles;
    /// Triangles of stars or objects quantity
    int triangles_count;
    /// Align/scale/rotation settings
    dsp_align_info align_info;
    /// Frame number (if part of a series)
    int frame_number;
} dsp_stream, *dsp_stream_p;

/**\}*/
/**
 * \defgroup dsp_FourierTransform DSP API Fourier transform related functions
*/
/**\{*/

/**
* \brief Perform a discrete Fourier Transform of a dsp_stream
* \param stream the inout stream.
* \param exp the exponent (recursivity) of the fourier transform.
*/
DLL_EXPORT void dsp_fourier_dft(dsp_stream_p stream, int exp);

/**
* \brief Perform an inverse discrete Fourier Transform of a dsp_stream
* \param stream the inout stream.
*/
DLL_EXPORT void dsp_fourier_idft(dsp_stream_p stream);

/**
* \brief Fill the magnitude and phase buffers with the current data in stream->dft
* \param stream the inout stream.
*/
DLL_EXPORT void dsp_fourier_2dsp(dsp_stream_p stream);

/**
* \brief Obtain the complex fourier tranform from the current magnitude and phase buffers
* \param stream the inout stream.
*/
DLL_EXPORT void dsp_fourier_2fftw(dsp_stream_p stream);

/**
* \brief Obtain a complex array from phase and magnitude arrays
* \param mag the input magnitude array.
* \param phi the input phase array.
* \param len the input arrays length.
* \return the array filled with the complex numbers
*/
DLL_EXPORT dsp_complex dsp_fourier_phase_mag_array_get_complex(double* mag, double* phi, int len);

/**
* \brief Obtain a complex number's array magnitudes
* \param in the input complex number array.
* \param len the input array length.
* \return the array filled with the magnitudes
*/
DLL_EXPORT double* dsp_fourier_complex_array_get_magnitude(dsp_complex in, int len);

/**
* \brief Obtain a complex number's array phases
* \param in the input complex number array.
* \param len the input array length.
* \return the array filled with the phases
*/
DLL_EXPORT double* dsp_fourier_complex_array_get_phase(dsp_complex in, int len);

/**\}*/
/**
 * \defgroup dsp_Filters DSP API Linear buffer filtering functions
*/
/**\{*/
/**
* \brief A square law filter
* \param stream the input stream.
*/
DLL_EXPORT void dsp_filter_squarelaw(dsp_stream_p stream);

/**
* \brief A low pass filter
* \param stream the input stream.
* \param frequency the cutoff frequency of the filter in radians.
*/
DLL_EXPORT void dsp_filter_lowpass(dsp_stream_p stream, double frequency);

/**
* \brief A high pass filter
* \param stream the input stream.
* \param frequency the cutoff frequency of the filter in radians.
*/
DLL_EXPORT void dsp_filter_highpass(dsp_stream_p stream, double frequency);

/**
* \brief A band pass filter
* \param stream the input stream.
* \param LowFrequency the high-pass cutoff frequency of the filter in radians.
* \param HighFrequency the low-pass cutoff frequency of the filter in radians.
*/
DLL_EXPORT void dsp_filter_bandpass(dsp_stream_p stream, double LowFrequency,
                                    double HighFrequency);

/**
* \brief A band reject filter
* \param stream the input stream.
* \param LowFrequency the high-pass cutoff frequency of the filter in radians.
* \param HighFrequency the low-pass cutoff frequency of the filter.
*/
DLL_EXPORT void dsp_filter_bandreject(dsp_stream_p stream, double LowFrequency,
                                      double HighFrequency);

/**\}*/
/**
 * \defgroup dsp_Convolution DSP API Convolution and cross-correlation functions
*/
/**\{*/
/**
* \brief A cross-convolution processor
* \param stream the input stream.
* \param matrix the convolution matrix stream.
*/
DLL_EXPORT void dsp_convolution_convolution(dsp_stream_p stream, dsp_stream_p matrix);

/**
* \brief A cross-correlation processor
* \param stream the input stream.
* \param matrix the correlation matrix stream.
*/
DLL_EXPORT void dsp_convolution_correlation(dsp_stream_p stream, dsp_stream_p matrix);

/**\}*/
/**
 * \defgroup dsp_Stats DSP API Buffer statistics functions
*/
/**\{*/

#ifndef dsp_stats_min
/**
* \brief Gets the minimum value of the input stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the minimum value.
*/
#define dsp_stats_min(buf, len)\
({\
    int i;\
    dsp_t min = (dsp_t)buf[0];\
    for(i = 0; i < len; i++) {\
        min = Min(buf[i], min);\
    }\
    min;\
    })
#endif

#ifndef dsp_stats_max
/**
* \brief Gets the maximum value of the input stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the maximum value.
*/
#define dsp_stats_max(buf, len)\
({\
    int i;\
    dsp_t max = (dsp_t)buf[0];\
    for(i = 0; i < len; i++) {\
        max = Max(buf[i], max);\
    }\
    max;\
    })
#endif

#ifndef dsp_stats_mid
/**
* \brief Gets the middle value of the input stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the middle value.
*/
#define dsp_stats_mid(buf, len)\
({\
    int i;\
    dsp_t min = dsp_stats_min(buf, len);\
    (dsp_t)(min - dsp_stats_max(buf, len)) / 2.0 + min;\
})
#endif

#ifndef dsp_stats_maximum_index
/**
* \brief Gets minimum value's position into the buffer
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the position index of the minimum value.
*/
#define dsp_stats_minimum_index(buf, len)\
({\
    int i;\
    dsp_t min = dsp_stats_min(buf, len);\
    for(i = 0; i < len; i++) {\
        if(buf[i] == min) break;\
    }\
    i;\
    })
#endif

#ifndef dsp_stats_maximum_index
/**
* \brief Gets maximum value's position into the buffer
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the position index of the maximum value.
*/
#define dsp_stats_maximum_index(buf, len)\
({\
    int i;\
    dsp_t max = dsp_stats_max(buf, len);\
    for(i = 0; i < len; i++) {\
        if(buf[i] == max) break;\
    }\
    i;\
    })
#endif

#ifndef dsp_stats_stddev
/**
* \brief A mean calculator
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the mean value of the stream.
*/
#define dsp_stats_mean(buf, len)\
({\
    int __dsp__i;\
    double __dsp__mean = 0;\
    for(__dsp__i = 0; __dsp__i < len; __dsp__i++) {\
        __dsp__mean += buf[__dsp__i];\
    }\
    __dsp__mean /= len;\
    __dsp__mean;\
    })
#endif

#ifndef dsp_stats_stddev
/**
* \brief Standard deviation of the inut stream
* \param buf the inout buffer
* \param len the length of the buffer
*/
#define dsp_stats_stddev(buf, len)\
({\
    double __dsp__mean = dsp_stats_mean(buf, len);\
    int __dsp__x;\
    double __dsp__stddev = 0;\
    for(__dsp__x = 0; __dsp__x < len; __dsp__x++) {\
        __dsp__stddev += fabs(buf[__dsp__x] - __dsp__mean);\
    }\
    __dsp__stddev /= len;\
    __dsp__stddev;\
    })
#endif

#ifndef dsp_stats_val_count
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
#endif

#ifndef dsp_stats_val_sum
/**
* \brief Cumulative sum of all the values on the stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \return the count of the value of the stream.
*/
#define dsp_stats_val_sum(buf, len) \
({\
    int x;\
    double sum = 0;\
    for(x = 0; x < len; x++) {\
        sum += buf[x];\
    }\
    sum;\
    })
#endif

#ifndef dsp_stats_range_count
/**
* \brief Counts value ranging occurrences into stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \param lo the bottom margin value.
* \param hi the upper margin value.
* \return the count of the values ranging between the margins of the stream.
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
#endif

#ifndef dsp_stats_compare
/**
* \brief Compare two buffers
* \param in1 the first input buffer
* \param in2 the second input buffer
* \param len the length in elements of the buffer.
* \return the sum of the subtraction of each element of both streams
*/
#define dsp_stats_compare(in1, in2, len)\
({\
    __typeof(in1[0]) out = 0;\
    for(int i = 0; i < len; i++) {\
        out += in1[i] - (__typeof(in1[0]))in2[i];\
    }\
    out;\
    })
#endif

/**
* \brief Histogram of the inut stream
* \param stream the stream on which execute
* \param size the length of the median.
* \return the output stream if successfull elaboration. NULL if an
* error is encountered.
*/
DLL_EXPORT double* dsp_stats_histogram(dsp_stream_p stream, int size);

/**\}*/
/**
 * \defgroup dsp_Buffers DSP API Buffer editing functions
*/
/**\{*/

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

#ifndef dsp_buffer_stretch
/**
* \brief Stretch minimum and maximum values of the input stream
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \param _mn the desired minimum value.
* \param _mx the desired maximum value.
*/
#define dsp_buffer_stretch(buf, len, _mn, _mx)\
({\
    int k;\
    dsp_t __mn = dsp_stats_min(buf, len);\
    dsp_t __mx = dsp_stats_max(buf, len);\
    double oratio = (_mx - _mn);\
    double iratio = (__mx - __mn);\
    if(iratio == 0) iratio = 1;\
    for(k = 0; k < len; k++) {\
        buf[k] -= __mn;\
        buf[k] = (dsp_t)((double)buf[k] * oratio / iratio);\
        buf[k] += _mn;\
    }\
})
#endif

#ifndef dsp_buffer_set
/**
* \brief Fill the buffer with the passed value
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \param _val the desired value.
*/
#define dsp_buffer_set(buf, len, _val)\
({\
    int k;\
    for(k = 0; k < len; k++) {\
        buf[k] = (dsp_t)(_val);\
    }\
})
#endif

#ifndef dsp_buffer_normalize
/**
* \brief Normalize the input stream to the minimum and maximum values
* \param buf the input buffer
* \param len the length in elements of the buffer.
* \param mn the clamping bottom value.
* \param mx the clamping upper value.
*/
#define dsp_buffer_normalize(buf, len, mn, mx)\
({\
    int k;\
    for(k = 0; k < len; k++) {\
        buf[k] = Max(mn, Min(mx, buf[k]));\
    }\
})
#endif

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
DLL_EXPORT void dsp_buffer_sub1(dsp_stream_p stream, dsp_t val);

/**
* \brief Subtract each element of the input stream a value
* \param stream the stream on which execute
* \param val the value to be subtracted.
*/
DLL_EXPORT void dsp_buffer_1sub(dsp_stream_p stream, dsp_t val);

/**
* \brief Sum elements of the input stream to a value
* \param stream the stream on which execute
* \param val the value used for this operation.
*/
DLL_EXPORT void dsp_buffer_sum1(dsp_stream_p stream, dsp_t val);

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
* \brief Median elements of the input stream
* \param stream the stream on which execute
* \param size the length of the median.
* \param median the location of the median value.
*/
DLL_EXPORT void dsp_buffer_median(dsp_stream_p stream, int size, int median);

/**
* \brief Standard deviation of each element of the input stream within the given size
* \param stream the stream on which execute
* \param size the reference size.
*/
DLL_EXPORT void dsp_buffer_sigma(dsp_stream_p stream, int size);

/**
* \brief Deviate forward the first input stream using the second stream as indexing reference
* \param stream the stream on which execute
* \param deviation the stream containing the deviation buffer
* \param mindeviation the deviation at 0.
* \param maxdeviation the deviation at 1.
*/
DLL_EXPORT void dsp_buffer_deviate(dsp_stream_p stream, dsp_t* deviation, dsp_t mindeviation, dsp_t maxdeviation);

#ifndef dsp_buffer_reverse
/**
* \brief Reverse the order of the buffer elements
* \param buf the inout stream.
* \param len the length of the input stream.
*/
#define dsp_buffer_reverse(buf, len) \
    ({ \
        int i = (len - 1) / 2; \
        int j = i + 1; \
        dsp_t _x; \
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
/**
* \brief Change the in buffer elements endianness
* \param in the inout stream.
* \param len the length of the input stream.
*/
#define dsp_buffer_swap(in, len) \
    ({ \
        int k; \
        switch(sizeof(((__typeof (in[0])*)in)[0])) { \
        case 2: \
            for(k = 0; k < len; k++) \
                ((__typeof (in[0])*)in)[k] = __bswap_16(((__typeof (in[0])*)in)[k]); \
            break; \
        case 3: \
            for(k = 0; k < len; k++) \
            ((__typeof (in[0])*)in)[k] = __bswap_32(((__typeof (in[0])*)in)[k]); \
            break; \
        } \
    })
#endif

#ifndef dsp_buffer_copy
/**
* \brief Fill the output buffer with the values of the
* elements of the input stream by casting them to the
* output buffer element type
* \param in the input stream.
* \param out the output stream.
* \param len the length of the first input stream.
*/
#define dsp_buffer_copy(in, out, len) \
    ({ \
        int k; \
        for(k = 0; k < len; k++) { \
        ((__typeof (out[0])*)out)[k] = (__typeof (out[0]))((__typeof (in[0])*)in)[k]; \
        } \
    })
#endif

#ifndef dsp_buffer_copy_stepping
/**
* \brief Fill the output buffer with the values of the
* elements of the input stream by casting them to the
* output buffer element type
* \param in the input stream.
* \param out the output stream.
* \param inlen the length of the input stream.
* \param outlen the length of the output stream.
* \param instep copy each instep elements of in into each outstep elements of out.
* \param outstep copy each instep elements of in into each outstep elements of out.
*/
#define dsp_buffer_copy_stepping(in, out, inlen, outlen, instep, outstep) \
    ({ \
    int k; \
    int t; \
        for(k = 0, t = 0; k < inlen && t < outlen; k+=instep, t+=outstep) { \
        ((__typeof (out[0])*)out)[t] = (__typeof (out[0]))((__typeof (in[0])*)in)[k]; \
        } \
    })
#endif

/**\}*/
/**
 * \defgroup dsp_DSPStream DSP API Stream type management functions
*/
/**\{*/

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
DLL_EXPORT dsp_stream_p dsp_stream_new(void);

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
* \param star the star to add to DSP stream.
* \sa dsp_stream_new
* \sa dsp_stream_del_star
*/
DLL_EXPORT void dsp_stream_add_star(dsp_stream_p stream, dsp_star star);

/**
* \brief Remove the star with index n to a DSP stream
* \param stream the target DSP stream.
* \param n the index of the star to remove
* \sa dsp_stream_new
* \sa dsp_stream_add_star
*/
DLL_EXPORT void dsp_stream_del_star(dsp_stream_p stream, int n);

/**
* \brief Add a triangle to the DSP Stream passed as argument
* \param stream the target DSP stream.
* \param triangle the triangle to add to DSP stream.
* \sa dsp_stream_new
* \sa dsp_stream_del_triangle
*/
DLL_EXPORT void dsp_stream_add_triangle(dsp_stream_p stream, dsp_triangle triangle);

/**
* \brief Remove the triangle with index n to a DSP stream
* \param stream the target DSP stream.
* \param index the index of the triangle to remove
* \sa dsp_stream_new
* \sa dsp_stream_add_triangle
*/
DLL_EXPORT void dsp_stream_del_triangle(dsp_stream_p stream, int index);

/**
* \brief Calculate the triangles in the stream struct
* \param stream the target DSP stream.
* \sa dsp_stream_new
* \sa dsp_stream_add_triangle
*/
DLL_EXPORT void dsp_stream_calc_triangles(dsp_stream_p stream);

/**
* \brief Remove the child with index n to a DSP stream
* \param stream the target DSP stream.
* \param n the index of the child to remove
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
* \brief Set a dimension size to a DSP stream
* \param stream the target DSP stream.
* \param dim the index of the dimension to modify
* \param size the size of the dimension to modify
* \sa dsp_stream_new
* \sa dsp_stream_add_dim
*/
DLL_EXPORT void dsp_stream_set_dim(dsp_stream_p stream, int dim, int size);

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
* \param args The arguments list to be passed to the callback
* \return callback return value
* \sa dsp_stream_new
* \sa dsp_stream_get_position
* \sa dsp_stream_set_position
*/
DLL_EXPORT void *dsp_stream_exec(dsp_stream_p stream, void *args, ...);

/**
* \brief Crop the buffers of the stream passed as argument by reading the ROI field.
* \param stream The stream that will be cropped in-place
*/
DLL_EXPORT void dsp_stream_crop(dsp_stream_p stream);

/**
* \brief Rotate a stream around an axis and offset
* \param stream The stream that will be rotated in-place
*/
DLL_EXPORT void dsp_stream_rotate(dsp_stream_p stream);

/**
* \brief Translate a stream
* \param stream The stream that will be translated in-place
*/
DLL_EXPORT void dsp_stream_translate(dsp_stream_p stream);

/**
* \brief Scale a stream
* \param stream The stream that will be scaled in-place
*/
DLL_EXPORT void dsp_stream_scale(dsp_stream_p stream);

/**\}*/
/**
 * \defgroup dsp_SignalGen DSP API Signal generation functions
*/
/**\{*/

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

/**\}*/
/**
 * \defgroup dsp_FileManagement DSP API File read/write functions
*/
/**\{*/

/**
* \brief Read a FITS file and fill a dsp_stream_p with its content
* \param filename the file name.
* \param channels will be filled with the number of components
* \param stretch 1 if the buffer intensities have to be stretched
* \return The new dsp_stream_p structure pointer
*/
DLL_EXPORT dsp_stream_p* dsp_file_read_fits(const char* filename, int *channels, int stretch);

/**
* \brief Write the dsp_stream_p into a FITS file,
* \param filename the file name.
* \param bpp the bit depth of the output FITS file.
* \param stream the input stream to be saved
*/
DLL_EXPORT void dsp_file_write_fits(const char* filename, int bpp, dsp_stream_p stream);

/**
* \brief Write the components dsp_stream_p array into a JPEG file,
* \param filename the file name.
* \param components the number of streams in the array to be used as components 1 or 3.
* \param bpp the bit depth of the output JPEG file [8,16,32,64,-32,-64].
* \param stream the input stream to be saved
*/
DLL_EXPORT void dsp_file_write_fits_composite(const char* filename, int components, int bpp, dsp_stream_p* stream);

/**
* \brief Read a JPEG file and fill a array of dsp_stream_p with its content,
* each color channel has its own stream in this array and an additional grayscale at end will be added
* \param filename the file name.
* \param channels this value will be updated with the channel quantity into the picture.
* \param stretch 1 if the buffer intensities have to be stretched
* \return The new dsp_stream_p structure pointers array
*/
DLL_EXPORT dsp_stream_p* dsp_file_read_jpeg(const char* filename, int *channels, int stretch);

/**
* \brief Write the stream into a JPEG file,
* \param filename the file name.
* \param quality the quality of the output JPEG file 0-100.
* \param stream the input stream to be saved
*/
DLL_EXPORT void dsp_file_write_jpeg(const char* filename, int quality, dsp_stream_p stream);

/**
* \brief Write the components dsp_stream_p array into a JPEG file,
* \param filename the file name.
* \param components the number of streams in the array to be used as components 1 or 3.
* \param quality the quality of the output JPEG file 0-100.
* \param stream the input stream to be saved
*/
DLL_EXPORT void dsp_file_write_jpeg_composite(const char* filename, int components, int quality, dsp_stream_p* stream);

/**
* \brief Convert a bayer pattern dsp_t array into a grayscale array
* \param src the input buffer
* \param width the picture width
* \param height the picture height
* \return The new dsp_t array
*/
DLL_EXPORT dsp_t* dsp_file_bayer_2_gray(dsp_t* src, int width, int height);

/**
* \brief Convert a bayer pattern dsp_t array into a ordered 3 RGB array
* \param src the input buffer
* \param red the location of the red pixel within the bayer pattern
* \param width the picture width
* \param height the picture height
* \return The new dsp_t array
*/
DLL_EXPORT dsp_t* dsp_file_bayer_2_rgb(dsp_t *src, int red, int width, int height);

/**
* \brief Convert a color component dsp_t array into a dsp_stream_p array each element containing the single components
* \param buf the input buffer
* \param dims the number of dimension
* \param sizes the sizes of each dimension
* \param components the number of the color components
* \return The new dsp_stream_p array
*/
DLL_EXPORT dsp_stream_p *dsp_stream_from_components(dsp_t* buf, int dims, int *sizes, int components);

/**
* \brief Convert an RGB color dsp_t array into a dsp_stream_p array each element containing the single components
* \param buf the input buffer
* \param dims the number of dimension
* \param sizes the sizes of each dimension
* \param components the number of the color components
* \param bpp the color depth of the color components
* \param stretch whether to stretch within 0 and dsp_t_max
* \return The new dsp_stream_p array
*/
DLL_EXPORT dsp_stream_p *dsp_buffer_rgb_to_components(void* buf, int dims, int *sizes, int components, int bpp, int stretch);

/**
* \brief Convert a component dsp_stream_p array into an RGB dsp_t array
* \param stream the component dsp_stream_p array
* \param rgb the output buffer
* \param components the number of the color components
* \param bpp the color depth of the color components
*/
DLL_EXPORT void dsp_buffer_components_to_rgb(dsp_stream_p *stream, void* rgb, int components, int bpp);

/**
* \brief Convert a component dsp_stream_p array into a bayer dsp_t array
* \param src the component dsp_stream_p array
* \param red the red offset within the bayer quad
* \param width the width of the output array
* \param height the height of the output array
* \return The new dsp_stream_p array
*/
DLL_EXPORT dsp_t* dsp_file_composite_2_bayer(dsp_stream_p *src, int red, int width, int height);

/**
* \brief Write a FITS file from a dsp_stream_p array
* \param filename the output file name
* \param components the number of the color components
* \param bpp the color depth of the color components
* \param stream the component dsp_stream_p array
*/
DLL_EXPORT void dsp_file_write_fits_bayer(const char* filename, int components, int bpp, dsp_stream_p* stream);

/**
* \brief Convert a bayer pattern dsp_t array into a contiguos component array
* \param src the input buffer
* \param red the location of the red pixel within the bayer pattern
* \param width the picture width
* \param height the picture height
* \return The new dsp_t array
*/
DLL_EXPORT dsp_t* dsp_file_bayer_2_composite(dsp_t *src, int red, int width, int height);

/**
* \brief Calculate offsets, rotation and scaling of two streams giving reference alignment point
* \param ref the reference stream
* \param to_align the stream to be aligned
* \param tolerance number of decimals allowed
* \param target_score the minimum matching score to reach
* \return The alignment mask (bit1: translated, bit2: scaled, bit3: rotated)
*/
DLL_EXPORT int dsp_align_get_offset(dsp_stream_p ref, dsp_stream_p to_align, double tolerance, double target_score);

/**
* \brief Callback function for qsort for double type ascending ordering
* \param arg1 the first comparison element
* \param arg2 the second comparison element
* \return 1 if arg1 is greater than arg2, -1 if arg2 is greater than arg1
*/
DLL_EXPORT int dsp_qsort_double_asc (const void *arg1, const void *arg2);

/**
* \brief Callback function for qsort for double type descending ordering
* \param arg1 the first comparison element
* \param arg2 the second comparison element
* \return 1 if arg2 is greater than arg1, -1 if arg1 is greater than arg2
*/
DLL_EXPORT int dsp_qsort_double_desc (const void *arg1, const void *arg2);

/**
* \brief Callback function for qsort for dsp_star ascending ordering by their diameters
* \param arg1 the first comparison element
* \param arg2 the second comparison element
* \return 1 if arg1 diameter is greater than arg2, -1 if arg2 diameter is greater than arg1
*/
DLL_EXPORT int dsp_qsort_star_diameter_asc (const void *arg1, const void *arg2);

/**
* \brief Callback function for qsort for dsp_star descending ordering by their diameters
* \param arg1 the first comparison element
* \param arg2 the second comparison element
* \return 1 if arg2 diameter is greater than arg1, -1 if arg1 diameter is greater than arg2
*/
DLL_EXPORT int dsp_qsort_star_diameter_desc (const void *arg1, const void *arg2);

/**\}*/
/// \defgroup dsp_FitsExtensions
#include <fits_extensions.h>

#ifdef  __cplusplus
}
#endif

#endif //_DSP_H
