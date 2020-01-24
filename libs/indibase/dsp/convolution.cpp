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
    IUFillBLOBVector(&DownloadBP, &FitsB, 1, m_Device->getDeviceName(), "CONVOLUTION", "Matrix Data", DSP_TAB, IP_WO, 60, IPS_IDLE);
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
        if(!strcmp(name, DownloadBP.name)) {
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
                        unsigned long size = sizes[i];
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

uint8_t* Convolution::Callback(uint8_t *buf, long dims, long *sizes, int bits_per_sample)
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
}
