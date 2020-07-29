/*******************************************************************************
  Copyright(c) 2017 Ilia Platone, Jasem Mutlaq. All rights reserved.

 DSP plugin manager

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

#include "indidevapi.h"
#include "convolution.h"
#include "transforms.h"

#include <fitsio.h>
#include <functional>
#include <string>

namespace INDI
{
class DefaultDevice;
}

namespace DSP
{
class Manager
{
    public:
        Manager(INDI::DefaultDevice *dev);
        virtual ~Manager();

        virtual void ISGetProperties(const char *dev);
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
        virtual bool saveConfigItems(FILE *fp);
        virtual bool updateProperties();

        bool processBLOB(uint8_t* buf, uint32_t ndims, int* dims, int bits_per_sample);

        inline void setSizes(uint32_t num, int* sizes) { BufferSizes = sizes; BufferSizesQty = num; }
        inline void getSizes(uint32_t *num, int** sizes) { *sizes = BufferSizes; *num = BufferSizesQty; }

        inline void setBPS(int bps) { BPS = bps; }
        inline int getBPS() { return BPS; }

    private:
        Convolution *convolution;
        FourierTransform *dft;
        Spectrum *spectrum;
        Histogram *histogram;
        Wavelets *wavelets;
        uint32_t BufferSizesQty;
        int *BufferSizes;
        int BPS;
};
}
