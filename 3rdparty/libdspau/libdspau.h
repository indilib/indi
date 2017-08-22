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

#ifndef _DSPAU_H
#define _DSPAU_H


#ifdef  __cplusplus
extern "C" {
#endif
enum dspau_conversiontype {
	magnitude = 0,
	magnitude_dbv = 1,
	magnitude_rooted = 2,
	magnitude_squared = 3,
	phase_degrees = 4,
	phase_radians = 5,
};

/**
* @brief Create a spectrum from a double array of values
* @param data the input stream.
* @param bandwidth the bandwidth of the spectrum.
* @param len the length of the input stream.
* @param conversion the output magnitude dspau_conversiontype type.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return out if success.
* Return NULL if any error occurs.
*/
double* dspau_spectrum(double* data, int *len, int conversion);

/**
* @brief A square law filter
* @param data the input stream.
* @param len the length of the input stream.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return out if success.
* Return NULL if any error occurs.
*/
double* dspau_squarelawfilter(double* data, int len);

/**
* @brief A low pass filter
* @param data the input stream.
* @param len the length of the input stream.
* @param samplingfrequency the sampling frequency of the input stream.
* @param frequency the cutoff frequency of the filter.
* @param q the cutoff slope.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return out if success.
* Return NULL if any error occurs.
*/
double* dspau_lowpassfilter(double* data, int len, double samplingfrequency, double frequency, double q);

/**
* @brief A high pass filter
* @param data the input stream.
* @param len the length of the input stream.
* @param samplingfrequency the sampling frequency of the input stream.
* @param frequency the cutoff frequency of the filter.
* @param q the cutoff slope.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return out if success.
* Return NULL if any error occurs.
*/
double* dspau_highpassfilter(double* data, int len, double samplingfrequency, double frequency, double q);

/**
* @brief A band pass filter
* @param data the input stream.
* @param len the length of the input stream.
* @param samplingfrequency the sampling frequency of the input stream.
* @param frequency the center frequency of the filter.
* @param q the cutoff slope.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return out if success.
* Return NULL if any error occurs.
*/
double* dspau_bandpassfilter(double* data, int len, double samplingfrequency, double frequency, double q);

/**
* @brief A band reject filter
* @param data the input stream.
* @param len the length of the input stream.
* @param samplingfrequency the sampling frequency of the input stream.
* @param frequency the center frequency of the filter.
* @param q the cutoff slope.
* @return the output stream if successfull elaboration. NULL if an
* error is encountered.
* Return out if success.
* Return NULL if any error occurs.
*/
double* dspau_bandrejectfilter(double* data, int len, double samplingfrequency, double frequency, double q);

#ifdef  __cplusplus
}
#endif

#endif //_DSPAU_H
