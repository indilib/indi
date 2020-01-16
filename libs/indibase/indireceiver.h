/*******************************************************************************
 Copyright(c) 2010, 2017 Ilia Platone, Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indivectorinterface.h"
#include "dsp.h"
#include <fitsio.h>

#ifdef HAVE_WEBSOCKET
#include "indiwsserver.h"
#endif

#include <fitsio.h>

#include <memory>
#include <cstring>
#include <chrono>
#include <stdint.h>
#include <mutex>
#include <thread>

//JM 2019-01-17: Disabled until further notice
//#define WITH_EXPOSURE_LOOPING

extern const char *CAPTURE_SETTINGS_TAB;
extern const char *CAPTURE_INFO_TAB;
extern const char *GUIDE_HEAD_TAB;


/**
 * \class INDI::Detector
 * \brief Class to provide general functionality of Monodimensional Detector.
 *
 * The Detector capabilities must be set to select which features are exposed to the clients.
 * SetDetectorCapability() is typically set in the constructor or initProperties(), but can also be
 * called after connection is established with the Detector, but must be called /em before returning
 * true in Connect().
 *
 * Developers need to subclass INDI::Detector to implement any driver for Detectors within INDI.
 *
 * \example Detector Simulator
 * \author Jasem Mutlaq, Ilia Platone
 *
 */

namespace INDI
{
class StreamManager;

class Receiver : public VectorInterface
{
    public:
        Receiver();
        virtual ~Receiver();

        bool initProperties();
        bool updateProperties();
        void ISGetProperties(const char *dev);
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
        bool ISSnoopDevice(XMLEle *root);

    protected:

        // DSP API Related functions

        /**
         * @brief Create a histogram
         * @param buf the buffer from which extract the histogram
         * @param out the buffer where to copy the histogram
         * @param buf_len the length in bytes of the input buffer
         * @param histogram_size the size of the histogram
         * @param bits_per_sample can be one of 8,16,32,64 for unsigned types, -32,-64 for doubleing single and double types
         */
        void Histogram(void *buf, void *out, int buf_len, int histogram_size, int bits_per_sample);

        /**
         * @brief Create a Fourier transform
         * @param buf the buffer from which extract the Fourier transform
         * @param out the buffer where to copy the Fourier transform
         * @param dims the number of dimensions of the input buffer
         * @param sizes the sizes of each dimension
         * @param bits_per_sample can be one of 8,16,32,64 for unsigned types, -32,-64 for doubleing single and double types
         */
        void FourierTransform(void *buf, void *out, int dims, int *sizes, int bits_per_sample);

        /**
         * @brief Create a Spectrum
         * @param buf the buffer from which extract the spectrum
         * @param out the buffer where to copy the spectrum
         * @param buf_len the length in bytes of the input buffer
         * @param size the size of the spectrum
         * @param bits_per_sample can be one of 8,16,32,64 for unsigned types, -32,-64 for doubleing single and double types
         */
        void Spectrum(void *buf, void *out, int buf_len, int size, int bits_per_sample);

        /**
         * @brief Convolute
         * @param buf the buffer to convolute
         * @param matrix the convolution matrix
         * @param out the buffer where to copy the convoluted buffer
         * @param dims the number of dimensions of the input buffer
         * @param sizes the sizes of each dimension of the input buffer
         * @param matrix_dims the number of dimensions of the matrix
         * @param matrix_sizes the sizes of each dimension of the matrix
         * @param bits_per_sample can be one of 8,16,32,64 for unsigned types, -32,-64 for doubleing single and double types
         */
        void Convolution(void *buf, void *matrix, void *out, int dims, int *sizes, int matrix_dims, int *matrix_sizes, int bits_per_sample);

        /**
         * @brief White noise generator
         * @param buf the buffer to fill
         * @param size the size of the input buffer
         * @param bits_per_sample can be one of 8,16,32,64 for unsigned types, -32,-64 for doubleing single and double types
         */
        void WhiteNoise(void *out, int size, int bits_per_sample);

        };
}
