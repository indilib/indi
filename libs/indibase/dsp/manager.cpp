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

#include "manager.h"
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
Manager::Manager(INDI::DefaultDevice *dev)
{
    convolution = new Convolution(dev);
    dft = new FourierTransform(dev);
    spectrum = new Spectrum(dev);
    histogram = new Histogram(dev);
    wavelets = new Wavelets(dev);
}

Manager::~Manager()
{
}

void Manager::ISGetProperties(const char *dev)
{
    convolution->ISGetProperties(dev);
    dft->ISGetProperties(dev);
    spectrum->ISGetProperties(dev);
    histogram->ISGetProperties(dev);
    wavelets->ISGetProperties(dev);
}

bool Manager::updateProperties()
{
    bool r = false;
    r |= convolution->updateProperties();
    r |= dft->updateProperties();
    r |= spectrum->updateProperties();
    r |= histogram->updateProperties();
    r |= wavelets->updateProperties();
    return r;
}

bool Manager::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    bool r = false;
    r |= convolution->ISNewSwitch(dev, name, states, names, num);
    r |= dft->ISNewSwitch(dev, name, states, names, num);
    r |= spectrum->ISNewSwitch(dev, name, states, names, num);
    r |= histogram->ISNewSwitch(dev, name, states, names, num);
    r |= wavelets->ISNewSwitch(dev, name, states, names, num);
    return r;
}

bool Manager::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    bool r = false;
    r |= convolution->ISNewText(dev, name, texts, names, num);
    r |= dft->ISNewText(dev, name, texts, names, num);
    r |= spectrum->ISNewText(dev, name, texts, names, num);
    r |= histogram->ISNewText(dev, name, texts, names, num);
    r |= wavelets->ISNewText(dev, name, texts, names, num);
    return r;
}

bool Manager::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    bool r = false;
    r |= convolution->ISNewNumber(dev, name, values, names, num);
    r |= dft->ISNewNumber(dev, name, values, names, num);
    r |= spectrum->ISNewNumber(dev, name, values, names, num);
    r |= histogram->ISNewNumber(dev, name, values, names, num);
    r |= wavelets->ISNewNumber(dev, name, values, names, num);
    return r;
}

bool Manager::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int num)
{
    bool r = false;
    r |= convolution->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
    r |= dft->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
    r |= spectrum->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
    r |= histogram->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
    r |= wavelets->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
    return r;
}

bool Manager::saveConfigItems(FILE *fp)
{
    bool r = false;
    r |= convolution->saveConfigItems(fp);
    r |= dft->saveConfigItems(fp);
    r |= spectrum->saveConfigItems(fp);
    r |= histogram->saveConfigItems(fp);
    r |= wavelets->saveConfigItems(fp);
    return r;
}

bool Manager::processBLOB(uint8_t* buf, uint32_t ndims, int* dims, int bits_per_sample)
{
    bool r = false;
    r |= convolution->processBLOB(buf, ndims, dims, bits_per_sample);
    r |= dft->processBLOB(buf, ndims, dims, bits_per_sample);
    r |= spectrum->processBLOB(buf, ndims, dims, bits_per_sample);
    r |= histogram->processBLOB(buf, ndims, dims, bits_per_sample);
    r |= wavelets->processBLOB(buf, ndims, dims, bits_per_sample);
    return r;
}

}
