/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 DSP Convolution plugin

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
#include <fitsio.h>
#define N_WAVELETS 7
#include <string>

namespace DSP
{
class Convolution : public Interface
{
public:
    Convolution(INDI::DefaultDevice *dev);
    ~Convolution();
    bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);

protected:
    void Activated();
    void Deactivated();

    uint8_t *Callback(uint8_t *out, uint32_t dims, int *sizes, int bits_per_sample);

private:
    dsp_stream_p matrix;

    IBLOBVectorProperty DownloadBP;
    IBLOB DownloadB;

    bool matrix_loaded { false };
    void Convolute();
};
class Wavelets : public Interface
{
public:
    Wavelets(INDI::DefaultDevice *dev);
    ~Wavelets();
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

protected:
    void Activated();
    void Deactivated();

    uint8_t *Callback(uint8_t *out, uint32_t dims, int *sizes, int bits_per_sample);

private:
    dsp_stream_p matrix;

    INumberVectorProperty WaveletsNP;
    INumber *WaveletsN;

    bool matrix_loaded { false };
    void Convolute();
};
}
