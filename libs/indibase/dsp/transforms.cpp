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

FourierTransform::FourierTransform(INDI::DefaultDevice *dev) : Interface(dev, DSP_DFT, "DFT", "DFT")
{
}

FourierTransform::~FourierTransform()
{
}

bool FourierTransform::processBLOB(uint8_t *buf, uint32_t dims, int *sizes, int bits_per_sample)
{
    if(!PluginActive) return false;
    setStream(buf, dims, sizes, bits_per_sample);

    dsp_fourier_dft(stream, 1);
    return Interface::processBLOB(getMagnitude(), stream->magnitude->dims, stream->magnitude->sizes, bits_per_sample);
}

InverseFourierTransform::InverseFourierTransform(INDI::DefaultDevice *dev) : Interface(dev, DSP_IDFT, "IDFT", "IDFT")
{
    IUFillBLOB(&DownloadB, "PHASE_DOWNLOAD", "Phase", "");
    IUFillBLOBVector(&DownloadBP, &FitsB, 1, m_Device->getDeviceName(), "PHASE", "Phase Data", DSP_TAB, IP_RW, 60, IPS_IDLE);
}

InverseFourierTransform::~InverseFourierTransform()
{
}

void InverseFourierTransform::Activated()
{
    m_Device->defineProperty(&DownloadBP);
    Interface::Activated();
}

void InverseFourierTransform::Deactivated()
{
    m_Device->deleteProperty(DownloadBP.name);
    Interface::Deactivated();
}

bool InverseFourierTransform::processBLOB(uint8_t *buf, uint32_t dims, int *sizes, int bits_per_sample)
{
    if(!PluginActive) return false;
    if(!phase_loaded) return false;
    setStream(buf, dims, sizes, bits_per_sample);
    if (phase->dims != stream->dims) return false;
    for (int d = 0; d < stream->dims; d++)
        if (phase->sizes[d] != stream->sizes[d])
            return false;
    setMagnitude(buf, dims, sizes, bits_per_sample);
    stream->phase = phase;
    dsp_buffer_set(stream->buf, stream->len, 0);
    dsp_fourier_idft(stream);
    return Interface::processBLOB(getStream(), stream->dims, stream->sizes, bits_per_sample);
}

bool InverseFourierTransform::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                        char *formats[], char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()))
    {
        if(!strcmp(name, DownloadBP.name))
        {
            IUUpdateBLOB(&DownloadBP, sizes, blobsizes, blobs, formats, names, n);
            LOGF_INFO("Received phase BLOB for %s", getDeviceName());
            if(phase != nullptr)
            {
                dsp_stream_free_buffer(phase);
                dsp_stream_free(phase);
            }
            phase = loadFITS(blobs[0], sizes[0]);
            if (phase != nullptr)
            {
                LOGF_INFO("Phase for %s loaded", getDeviceName());
                phase_loaded = true;
                return true;
            }
        }
    }
    return false;
}

Spectrum::Spectrum(INDI::DefaultDevice *dev) : Interface(dev, DSP_SPECTRUM, "SPECTRUM", "Spectrum")
{
}

Spectrum::~Spectrum()
{
}

bool Spectrum::processBLOB(uint8_t *buf, uint32_t dims, int *sizes, int bits_per_sample)
{
    if(!PluginActive) return false;
    setStream(buf, dims, sizes, bits_per_sample);

    dsp_fourier_dft(stream, 1);
    double *histo = dsp_stats_histogram(stream->magnitude, 4096);
    return Interface::processBLOB(static_cast<uint8_t*>(static_cast<void*>(histo)), 1, new int{4096}, -64);
}


Histogram::Histogram(INDI::DefaultDevice *dev) : Interface(dev, DSP_HISTOGRAM, "HISTOGRAM", "Histogram")
{
}

Histogram::~Histogram()
{
}

bool Histogram::processBLOB(uint8_t *buf, uint32_t dims, int *sizes, int bits_per_sample)
{
    if(!PluginActive) return false;
    setStream(buf, dims, sizes, bits_per_sample);

    double *histo = dsp_stats_histogram(stream, 4096);
    return Interface::processBLOB(static_cast<uint8_t*>(static_cast<void*>(histo)), 1, new int{4096}, -64);
}
}
