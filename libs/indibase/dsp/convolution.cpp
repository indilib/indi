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

#include "convolution.h"
#include "indistandardproperty.h"
#include "indicom.h"
#include "indilogger.h"
#include "dsp.h"
#include "base64.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>

namespace DSP
{

Convolution::Convolution(INDI::DefaultDevice *dev) : Interface(dev, DSP_CONVOLUTION, "CONVOLUTION", "Convolution")
{
    IUFillBLOB(&DownloadB, "CONVOLUTION_DOWNLOAD", "Convolution Matrix", "");
    IUFillBLOBVector(&DownloadBP, &FitsB, 1, m_Device->getDeviceName(), "CONVOLUTION", "Matrix Data", DSP_TAB, IP_RW, 60, IPS_IDLE);
}

Convolution::~Convolution()
{
}

void Convolution::Activated()
{
    m_Device->defineBLOB(&DownloadBP);
}

void Convolution::Deactivated()
{
    m_Device->deleteProperty(DownloadBP.name);
}

bool Convolution::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    INDI_UNUSED(blobsizes);
    if(!strcmp(dev, getDeviceName())) {
        LOGF_INFO("Received new BLOB for %s.", dev);
        if(!strcmp(name, DownloadBP.name)) {
            LOGF_INFO("Received new BLOB for %s.", dev);
            for (int i = 0; i < n; i++) {
                if (!strcmp(names[i], DownloadB.name)) {
                    LOGF_INFO("Received new BLOB for %s.", dev);
                    if(matrix) {
                        dsp_stream_free_buffer(matrix);
                        dsp_stream_free(matrix);
                    }
                    matrix = dsp_stream_new();
                    void* buf = blobs[i];
                    long ndims;
                    int bits_per_sample = 8;
                    int offset = 0;
                    if(!strcmp(formats[i], "fits")) {
                        fitsfile *fptr;
                        int status;
                        size_t size = sizes[i];
                        ffimem(&fptr, &buf, &size, sizes[i], realloc, &status);
                        char value[MAXINDINAME];
                        char comment[MAXINDINAME];
                        char query[MAXINDINAME];
                        ffgkys(fptr, "BITPIX", value, comment, &status);
                        bits_per_sample = (int)atol(value);
                        ffgkys(fptr, "NAXES", value, comment, &status);
                        ndims = (long)atol(value);
                        for (int d = 1; d <= ndims; d++) {
                            sprintf(query, "NAXIS%d", d);
                            ffgkys(fptr, query, value, comment, &status);
                            dsp_stream_add_dim(matrix, (long)atol(value));
                        }
                        offset = 2880;
                        fffree(fptr, &status);
                        buf = static_cast<void*>(static_cast<uint8_t *>(buf)+offset);
                    } else {
                        LOG_ERROR("Only fits decoding at the moment.");
                        continue;
                    }
                    switch (bits_per_sample)
                    {
                        case 8:
                            dsp_buffer_copy((static_cast<uint8_t *>(buf)), matrix->buf, matrix->len);
                            break;
                        case 16:
                            dsp_buffer_copy((static_cast<uint16_t *>(buf)), matrix->buf, matrix->len);
                            break;
                        case 32:
                            dsp_buffer_copy((static_cast<uint32_t *>(buf)), matrix->buf, matrix->len);
                            break;
                        case 64:
                            dsp_buffer_copy((static_cast<unsigned long *>(buf)), matrix->buf, matrix->len);
                            break;
                        case -32:
                            dsp_buffer_copy((static_cast<float *>(buf)), matrix->buf, matrix->len);
                            break;
                        case -64:
                            dsp_buffer_copy((static_cast<double *>(buf)), matrix->buf, matrix->len);
                            break;
                        default:
                            dsp_stream_free_buffer(matrix);
                            //Destroy the dsp stream
                            dsp_stream_free(matrix);
                        break;
                    }
                    LOG_INFO("DSP::Convolution: convolution matrix loaded");
                    matrix_loaded = true;
                }
            }
        }
    }
    return true;
}

uint8_t* Convolution::Callback(uint8_t *buf, uint32_t dims, int *sizes, int bits_per_sample)
{
    setStream(buf, dims, sizes, bits_per_sample);
    Convolute();
    return getStream();
}

void Convolution::Convolute()
{
    if(matrix_loaded)
        dsp_convolution_convolution(stream, matrix);
}

Wavelets::Wavelets(INDI::DefaultDevice *dev) : Interface(dev, DSP_CONVOLUTION, "WAVELETS", "Wavelets")
{
    WaveletsN = (INumber*)malloc(sizeof(INumber)*N_WAVELETS);
    for(int i = 0; i < N_WAVELETS; i++) {
        char strname[MAXINDINAME];
        char strlabel[MAXINDINAME];
        sprintf(strname, "WAVELET%0d", i);
        sprintf(strlabel, "%d pixels Gaussian Wavelet", (i+1)*3);
        IUFillNumber(&WaveletsN[i], strname, strlabel, "%3.3f", -15.0, 255.0, 1.0, 0.0);
    }
    IUFillNumberVector(&WaveletsNP, WaveletsN, N_WAVELETS, m_Device->getDeviceName(), "WAVELET", "Wavelets", DSP_TAB, IP_RW, 60, IPS_IDLE);
}

Wavelets::~Wavelets()
{
}

void Wavelets::Activated()
{
    m_Device->defineNumber(&WaveletsNP);
}

void Wavelets::Deactivated()
{
    m_Device->deleteProperty(WaveletsNP.name);
}

bool Wavelets::ISNewNumber(const char *dev, const char *name, double *values, char *names[], int n)
{
    INDI_UNUSED(values);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
    if (!strcmp(dev, getDeviceName()) && !strcmp(name, WaveletsNP.name)) {
        IDSetNumber(&WaveletsNP, nullptr);
    }
    return true;
}

uint8_t* Wavelets::Callback(uint8_t *buf, uint32_t dims, int *sizes, int bits_per_sample)
{
    setStream(buf, dims, sizes, bits_per_sample);
    double min = dsp_stats_min(stream->buf, stream->len);
    double max = dsp_stats_max(stream->buf, stream->len);
    dsp_stream_p out = dsp_stream_copy(stream);
    for (int i = 0; i < WaveletsNP.nnp; i++) {
        int size = (i+1)*3;
        dsp_stream_p tmp = dsp_stream_copy(stream);
        dsp_stream_p matrix = dsp_stream_new();
        dsp_stream_add_dim(matrix, size);
        dsp_stream_add_dim(matrix, size);
        dsp_stream_alloc_buffer(matrix, matrix->len);
        for(int y = 0; y < size; y++) {
            for(int x = 0; x < size; x++) {
                matrix->buf[x + y * size] = sin(static_cast<double>(x)*M_PI/static_cast<double>(size))*sin(static_cast<double>(y)*M_PI/static_cast<double>(size));
            }
        }
        dsp_convolution_convolution(tmp, matrix);
        dsp_buffer_sub(tmp, matrix->buf, matrix->len);
        dsp_buffer_mul1(tmp, WaveletsNP.np[i].value/8.0);
        dsp_buffer_sum(out, tmp->buf, tmp->len);
        dsp_buffer_normalize(tmp->buf, min, max, tmp->len);
        dsp_stream_free_buffer(matrix);
        dsp_stream_free(matrix);
        dsp_stream_free_buffer(tmp);
        dsp_stream_free(tmp);
    }
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);
    stream = dsp_stream_copy(out);
    return getStream();
}
}
