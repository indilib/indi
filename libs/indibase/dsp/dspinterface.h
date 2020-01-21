/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Connection Plugin Interface

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

#include <fitsio.h>
#include <functional>
#include <string>

namespace INDI
{
class DefaultDevice;
}

namespace DSP
{
enum Type {
    DSP_NONE = 0,
    DSP_TRANSFORMATIONS,
    DSP_CONVOLUTION,
};

class Interface
{
    public:
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
        virtual bool ISSnoopDevice(XMLEle *root);
        virtual bool saveConfigItems(FILE *fp);

        bool processBLOB(uint8_t* buf, int ndims, int* dims, int bits_per_sample);

        void setBufferSizes(int num, int* sizes) { BufferSizes = sizes; BufferSizesQty = num; }
        void getBufferSizes(int *num, int** sizes) { *sizes = BufferSizes; *num = BufferSizesQty; }

        void setBPS(int bps) { BPS = bps; }
        int getBPS() { return BPS; }

    protected:
        virtual void Activated();
        virtual void Deactivated();

        IBLOBVectorProperty FitsBP;
        IBLOB FitsB;

        ISwitchVectorProperty ActivateSP;
        ISwitch ActivateS[2];

        //  We are going to snoop these from a telescope
        INumberVectorProperty EqNP;
        INumber EqN[2];

        ITextVectorProperty ActiveDeviceTP;
        IText ActiveDeviceT[4] {};

        Interface(INDI::DefaultDevice *dev, Type type = DSP_NONE, const char *name = "DSP_PLUGIN", const char *label = "DSP Plugin");
        virtual ~Interface();

        const char *getDeviceName();
        std::function<uint8_t*(uint8_t*, int ndims, int*, int)> Callback;
        INDI::DefaultDevice *m_Device {  nullptr };
        const char *m_Name {  nullptr };
        const char *m_Label {  nullptr };
        DSP::Type m_Type {  DSP_NONE };

    private:
                double primaryFocalLength, primaryAperture;
        double RA, Dec;
        double Lat, Lon, El;
        double MPSAS;
        char processedFileName[MAXINDINAME];
        char processedFileExtension[MAXINDIFORMAT];
        int BufferSizesQty;
        int *BufferSizes;
        int BPS;
        bool processBLOBPrivate(uint8_t* buf, int ndims, int* dims, int bits_per_sample);
        void fits_update_key_s(fitsfile *fptr, int type, std::string name, void *p, std::string explanation, int *status);
        void addFITSKeywords(fitsfile *fptr, uint8_t* buf, int len);
        void *sendFITS(uint8_t *buf, int len, int bits_per_sample);
        bool uploadFile(const void *fitsData, size_t totalBytes, bool sendIntegration, bool saveIntegration, const char* format);
        void registerCallback(std::function<uint8_t*(uint8_t*, int ndims, int*, int)> callback);
        int getFileIndex(const char *dir, const char *prefix, const char *ext);
        void getMinMax(double *min, double *max, uint8_t *buf, int len, int bpp);
};
}
