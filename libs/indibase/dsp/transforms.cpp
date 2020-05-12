/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

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

#include "transforms.h"
#include "indistandardproperty.h"
#include "indicom.h"
#include "indilogger.h"
#include "dsp.h"

#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>

namespace DSP
{

FourierTransform::FourierTransform(INDI::DefaultDevice *dev) : Interface(dev, DSP_TRANSFORMATIONS, "DFT", "DFT")
{
}

FourierTransform::~FourierTransform()
{
}

uint8_t* FourierTransform::Callback(uint8_t *buf, uint32_t dims, int *sizes, int bits_per_sample)
{
    setStream(buf, dims, sizes, bits_per_sample);
    dsp_fourier_dft_magnitude(stream);
    dsp_buffer_stretch(stream->buf, stream->len, 0.0, (bits_per_sample < 0 ? 1.0 : pow(2, bits_per_sample)-1));
    return getStream();
}

Spectrum::Spectrum(INDI::DefaultDevice *dev) : Interface(dev, DSP_SPECTRUM, "SPECTRUM", "Spectrum")
{
}

Spectrum::~Spectrum()
{
}

uint8_t* Spectrum::Callback(uint8_t *buf, uint32_t dims, int *sizes, int bits_per_sample)
{
    setStream(buf, dims, sizes, bits_per_sample);
    dsp_fourier_dft_magnitude(stream);
    double *histo = dsp_stats_histogram(stream, 4096);
    dsp_stream_free_buffer(stream);
    dsp_stream_set_buffer(stream, histo, 4096);
    setSizes(1, new int{4096});
    return getStream();
}


Histogram::Histogram(INDI::DefaultDevice *dev) : Interface(dev, DSP_SPECTRUM, "HISTOGRAM", "Histogram")
{
}

Histogram::~Histogram()
{
}

uint8_t* Histogram::Callback(uint8_t *buf, uint32_t dims, int *sizes, int bits_per_sample)
{
    setStream(buf, dims, sizes, bits_per_sample);
    double *histo = dsp_stats_histogram(stream, 4096);
    dsp_stream_free_buffer(stream);
    dsp_stream_set_buffer(stream, histo, 4096);
    setSizes(1, new int{4096});
    return getStream();
}
}
