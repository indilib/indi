/*******************************************************************************
  Copyright(c) 2017 Ilia Platone, Jasem Mutlaq. All rights reserved.

 DSP Transforms plugin

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

#include "dspinterface.h"
#include "dsp.h"

#include <string>

namespace DSP
{
class FourierTransform : public Interface
{
public:
    FourierTransform(INDI::DefaultDevice *dev);
    virtual bool processBLOB(uint8_t *out, uint32_t dims, int *sizes, int bits_per_sample) override;

protected:
    ~FourierTransform();
};

class InverseFourierTransform : public Interface
{
public:
    InverseFourierTransform(INDI::DefaultDevice *dev);
    bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) override;
    virtual bool processBLOB(uint8_t *out, uint32_t dims, int *sizes, int bits_per_sample) override;

protected:
    ~InverseFourierTransform();
    void Activated() override;
    void Deactivated() override;

private:
    IBLOBVectorProperty DownloadBP;
    IBLOB DownloadB;

    dsp_stream_p phase;
    bool phase_loaded { false };
};

class Spectrum : public Interface
{
public:
    Spectrum(INDI::DefaultDevice *dev);
    virtual bool processBLOB(uint8_t *out, uint32_t dims, int *sizes, int bits_per_sample) override;

protected:
    ~Spectrum();
};

class Histogram : public Interface
{
public:
    Histogram(INDI::DefaultDevice *dev);
    virtual bool processBLOB(uint8_t *out, uint32_t dims, int *sizes, int bits_per_sample) override;

protected:
    ~Histogram();
};
}
