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
    Convolution = new DSP::Convolution(dev);
    Transforms = new DSP::Transforms(dev);
}

Manager::~Manager()
{
    Convolution->~Convolution();
    delete Convolution;
    Convolution = nullptr;
    Transforms->~Transforms();
    delete Transforms;
    Transforms = nullptr;
}

void Manager::ISGetProperties(const char *dev)
{
    Convolution->ISGetProperties(dev);
    Transforms->ISGetProperties(dev);
}

bool Manager::updateProperties()
{
    bool r = true;
    r &= Convolution->updateProperties();
    r &= Transforms->updateProperties();
    return r;
}

bool Manager::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    bool r = true;
    r &= Convolution->ISNewSwitch(dev, name, states, names, num);
    r &= Transforms->ISNewSwitch(dev, name, states, names, num);
    return r;
}

bool Manager::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    bool r = true;
    r &= Convolution->ISNewText(dev, name, texts, names, num);
    r &= Transforms->ISNewText(dev, name, texts, names, num);
    return r;
}

bool Manager::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    bool r = true;
    r &= Convolution->ISNewNumber(dev, name, values, names, num);
    r &= Transforms->ISNewNumber(dev, name, values, names, num);
    return r;
}

bool Manager::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int num)
{
    bool r = true;
    r &= Convolution->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
    r &= Transforms->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, num);
    return r;
}

bool Manager::ISSnoopDevice(XMLEle *root)
{
    bool r = true;
    r &= Convolution->ISSnoopDevice(root);
    r &= Transforms->ISSnoopDevice(root);
    return r;
}

bool Manager::saveConfigItems(FILE *fp)
{
    bool r = true;
    r &= Convolution->saveConfigItems(fp);
    r &= Transforms->saveConfigItems(fp);
    return r;
}

bool Manager::processBLOB(uint8_t* buf, int ndims, int* dims, int bits_per_sample)
{
    bool r = true;
    r &= Convolution->processBLOB(buf, ndims, dims, bits_per_sample);
    r &= Transforms->processBLOB(buf, ndims, dims, bits_per_sample);
    return r;
}

}
