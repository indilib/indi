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

Transforms::Transforms(INDI::DefaultDevice *dev) : Interface(dev, DSP_TRANSFORMATIONS, "DSP_TRANSFORMATIONS_PLUGIN", "Spectrum")
{
}

Transforms::~Transforms()
{
}

uint8_t* Transforms::Callback(uint8_t *buf, int dims, int *sizes, int bits_per_sample)
{
    setStream(buf, dims, sizes, bits_per_sample);
    free(buf);
    Spectrum(4096);
    return getStream();
}

void Transforms::Spectrum(int n_elements)
{
    FourierTransform();
    Histogram(n_elements);
}

void Transforms::FourierTransform()
{
    dsp_fourier_dft(stream);
}

void Transforms::Histogram(int histogram_size)
{
    double *histo = dsp_stats_histogram(stream, histogram_size);
    dsp_stream_free_buffer(stream);
    dsp_stream_set_buffer(stream, histo, histogram_size);
    setBufferSizes(1, new long{histogram_size});
}

void Transforms::setStream(void *buf, int dims, int *sizes, int bits_per_sample)
{
    //Create the dsp stream
    stream = dsp_stream_new();
    for(int dim = 0; dim < dims; dim++)
        dsp_stream_add_dim(stream, sizes[dim]);
    dsp_stream_alloc_buffer(stream, stream->len);
    switch (bits_per_sample)
    {
        case 8:
            dsp_buffer_copy((static_cast<uint8_t *>(buf)), stream->buf, stream->len);
            break;
        case 16:
            dsp_buffer_copy((static_cast<uint16_t *>(buf)), stream->buf, stream->len);
            break;
        case 32:
            dsp_buffer_copy((static_cast<uint32_t *>(buf)), stream->buf, stream->len);
            break;
        case 64:
            dsp_buffer_copy((static_cast<unsigned long *>(buf)), stream->buf, stream->len);
            break;
        case -32:
            dsp_buffer_copy((static_cast<float *>(buf)), stream->buf, stream->len);
            break;
        case -64:
            dsp_buffer_copy((static_cast<double *>(buf)), stream->buf, stream->len);
            break;
        default:
            dsp_stream_free_buffer(stream);
            //Destroy the dsp stream
            dsp_stream_free(stream);
    }
}

uint8_t* Transforms::getStream()
{
    void *buffer = malloc(stream->len*getBPS()/8);
    switch (getBPS())
    {
        case 8:
            dsp_buffer_copy(stream->buf, (static_cast<uint8_t *>(buffer)), stream->len);
            break;
        case 16:
            dsp_buffer_copy(stream->buf, (static_cast<uint16_t *>(buffer)), stream->len);
            break;
        case 32:
            dsp_buffer_copy(stream->buf, (static_cast<uint32_t *>(buffer)), stream->len);
            break;
        case 64:
            dsp_buffer_copy(stream->buf, (static_cast<unsigned long *>(buffer)), stream->len);
            break;
        case -32:
            dsp_buffer_copy(stream->buf, (static_cast<float *>(buffer)), stream->len);
            break;
        case -64:
            dsp_buffer_copy(stream->buf, (static_cast<double *>(buffer)), stream->len);
            break;
        default:
            return NULL;
            break;
    }
    //Destroy the dsp stream
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);
    return static_cast<uint8_t *>(buffer);
}
}
