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
    transforms = new Transforms(dev);
}

Manager::~Manager()
{
    convolution->~Convolution();
    delete convolution;
    convolution = nullptr;
    transforms->~Transforms();
    delete transforms;
    transforms = nullptr;
}

void Manager::ISGetProperties(const char *dev)
{
    convolution->ISGetProperties(dev);
    transforms->ISGetProperties(dev);
}

bool Manager::updateProperties()
{
    bool r = true;
    r &= convolution->updateProperties();
    r &= transforms->updateProperties();
    return r;
}

bool Manager::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    bool r = true;
    r &= convolution->ISNewSwitch(dev, name, states, names, num);
    r &= transforms->ISNewSwitch(dev, name, states, names, num);
    return r;
}

bool Manager::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    bool r = true;
    r &= convolution->ISNewText(dev, name, texts, names, num);
    r &= transforms->ISNewText(dev, name, texts, names, num);
    return r;
}

bool Manager::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    bool r = true;
    r &= convolution->ISNewNumber(dev, name, values, names, num);
    r &= transforms->ISNewNumber(dev, name, values, names, num);
    return r;
}

bool Manager::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int num)
{
    bool r = true;
    r &= convolution->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
    r &= transforms->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
    return r;
}

bool Manager::saveConfigItems(FILE *fp)
{
    bool r = true;
    r &= convolution->saveConfigItems(fp);
    r &= transforms->saveConfigItems(fp);
    return r;
}

bool Manager::processBLOB(uint8_t* buf, int ndims, int* dims, int bits_per_sample)
{
    bool r = true;
    r &= convolution->processBLOB(buf, ndims, dims, bits_per_sample);
    r &= transforms->processBLOB(buf, ndims, dims, bits_per_sample);
    return r;
}

}
