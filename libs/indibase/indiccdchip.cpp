/*******************************************************************************
 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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
#include "indiccdchip.h"
#include "indidevapi.h"
#include "sharedblob.h"
#include "locale_compat.h"

#include <cstring>
#include <ctime>

namespace INDI
{

CCDChip::CCDChip()
{
    strncpy(ImageExtention, "fits", MAXINDIBLOBFMT);
}

CCDChip::~CCDChip()
{
    IDSharedBlobFree(RawFrame);
    IDSharedBlobFree(BinFrame);
    IDSharedBlobFree(m_FITSMemoryBlock);
}

bool CCDChip::openFITSFile(uint32_t size, int &status)
{
    m_FITSMemorySize = size > 2880 ? 2880 : size;
    m_FITSMemoryBlock = IDSharedBlobAlloc(size);
    if (m_FITSMemoryBlock == nullptr)
    {
        IDLog("Failed to allocate memory for FITS file.");
        status = MEMORY_ALLOCATION;
        return false;
    }

    fits_create_memfile(&m_FITSFilePointer, &m_FITSMemoryBlock, &m_FITSMemorySize, 2880, IDSharedBlobRealloc, &status);
    if (status != 0)
    {
        IDSharedBlobFree(m_FITSMemoryBlock);
        m_FITSMemoryBlock = nullptr;
    }

    return (status == 0);
}

bool CCDChip::finishFITSFile(int &status)
{
    fits_flush_file(m_FITSFilePointer, &status);
    fits_close_file(m_FITSFilePointer, &status);
    if (status == 0)
    {
        m_FITSFilePointer = nullptr;
    }
    return (status == 0);
}

void CCDChip::closeFITSFile()
{
    if (m_FITSFilePointer != nullptr)
    {
        int status = 0;
        // Discard error here, the caller can't expect a valid file anymore at that point
        fits_close_file(m_FITSFilePointer, &status);
        m_FITSFilePointer = nullptr;
    }
    IDSharedBlobFree(m_FITSMemoryBlock);
    m_FITSMemoryBlock = nullptr;
}

void CCDChip::setFrameType(CCD_FRAME type)
{
    FrameType = type;
}

void CCDChip::setResolution(uint32_t x, uint32_t y)
{
    XRes = x;
    YRes = y;

    ImagePixelSizeNP[0].setValue(x);
    ImagePixelSizeNP[1].setValue(y);

    ImagePixelSizeNP.apply();

    ImageFrameNP[FRAME_X].setMin(0);
    ImageFrameNP[FRAME_X].setMax(x - 1);
    ImageFrameNP[FRAME_Y].setMin(0);
    ImageFrameNP[FRAME_Y].setMax(y - 1);

    ImageFrameNP[FRAME_W].setMin(1);
    ImageFrameNP[FRAME_W].setMax(x);
    ImageFrameNP[FRAME_H].setMin(1);
    ImageFrameNP[FRAME_H].setMax(y);
    ImageFrameNP.updateMinMax();
}

void CCDChip::setFrame(uint32_t subx, uint32_t suby, uint32_t subw, uint32_t subh)
{
    SubX = subx;
    SubY = suby;
    SubW = subw;
    SubH = subh;

    ImageFrameNP[FRAME_X].setValue(SubX);
    ImageFrameNP[FRAME_Y].setValue(SubY);
    ImageFrameNP[FRAME_W].setValue(SubW);
    ImageFrameNP[FRAME_H].setValue(SubH);

    ImageFrameNP.apply();
}

void CCDChip::setBin(uint8_t hor, uint8_t ver)
{
    BinX = hor;
    BinY = ver;

    ImageBinNP[BIN_W].setValue(BinX);
    ImageBinNP[BIN_H].setValue(BinY);

    ImageBinNP.apply();
}

void CCDChip::setMinMaxStep(const char *property, const char *element, double min, double max, double step,
                            bool sendToClient)
{
    INumberVectorProperty *nvp = nullptr;

    auto updateMinMaxStep = [element, min, max, sendToClient](INDI::PropertyNumber &oneProperty)
    {
        auto oneElement = oneProperty.findWidgetByName(element);
        if(oneElement)
        {
            oneElement->setMinMax(min, max);
            if(sendToClient)
                oneProperty.updateMinMax();
        }
    };

    if (ImageExposureNP.isNameMatch(property))
        updateMinMaxStep(ImageExposureNP);
    else if (ImageFrameNP.isNameMatch(property))
        updateMinMaxStep(ImageFrameNP);
    else if (ImageBinNP.isNameMatch(property))
        updateMinMaxStep(ImageBinNP);

    else if (ImagePixelSizeNP.isNameMatch(property))
        updateMinMaxStep(ImagePixelSizeNP);
    //    else if (!strcmp(property, RapidGuideDataNP.name))
    //        nvp = &RapidGuideDataNP;
    else
        return;

    INumber *np = IUFindNumber(nvp, element);
    if (np)
    {
        np->min  = min;
        np->max  = max;
        np->step = step;

        if (sendToClient)
            IUUpdateMinMax(nvp);
    }
}

void CCDChip::setPixelSize(double x, double y)
{
    PixelSizeX = x;
    PixelSizeY = y;

    ImagePixelSizeNP[2].setValue(x);
    ImagePixelSizeNP[3].setValue(x);
    ImagePixelSizeNP[4].setValue(y);

    ImagePixelSizeNP.apply();
}

void CCDChip::setBPP(uint8_t bbp)
{
    BitsPerPixel = bbp;

    ImagePixelSizeNP[5].setValue(BitsPerPixel);

    ImagePixelSizeNP.apply();
}

void CCDChip::setFrameBufferSize(uint32_t nbuf, bool allocMem)
{
    if (nbuf == RawFrameSize)
        return;

    RawFrameSize = nbuf;

    if (allocMem == false)
        return;

    RawFrame = static_cast<uint8_t*>(IDSharedBlobRealloc(RawFrame, RawFrameSize));
    if (RawFrame == nullptr)
        RawFrame = static_cast<uint8_t*>(IDSharedBlobAlloc(RawFrameSize));

    if (BinFrame)
    {
        BinFrame = static_cast<uint8_t*>(IDSharedBlobRealloc(BinFrame, RawFrameSize));
        if (BinFrame == nullptr)
            BinFrame = static_cast<uint8_t*>(IDSharedBlobAlloc(RawFrameSize));
    }
}

void CCDChip::setExposureLeft(double duration)
{
    ImageExposureNP.setState(IPS_BUSY);
    ImageExposureNP[0].setValue(duration);
    ImageExposureNP.apply();
}

void CCDChip::setExposureComplete()
{
    ImageExposureNP.setState(IPS_OK);
    ImageExposureNP[0].setValue(0);
    ImageExposureNP.apply();
}

void CCDChip::setExposureDuration(double duration)
{
    ExposureDuration = duration;
    gettimeofday(&StartExposureTime, nullptr);
}

const char *CCDChip::getFrameTypeName(CCD_FRAME fType)
{
    return FrameTypeSP[fType].getName();
}

const char *CCDChip::getExposureStartTime()
{
    static char ts[32];

    char iso8601[32] = {0};
    struct tm *tp = nullptr;

    // Get exposure startup timestamp
    time_t t = static_cast<time_t>(StartExposureTime.tv_sec);

    // Get UTC timestamp
    tp = gmtime(&t);

    // Format it in ISO8601 format
    strftime(iso8601, sizeof(iso8601), "%Y-%m-%dT%H:%M:%S", tp);

    // Add millisecond
    snprintf(ts, 32, "%s.%03d", iso8601, static_cast<int>(StartExposureTime.tv_usec / 1000.0));

    return (ts);
}

//void CCDChip::setInterlaced(bool intr)
//{
//    Interlaced = intr;
//}

void CCDChip::setExposureFailed()
{
    ImageExposureNP.setState(IPS_ALERT);
    ImageExposureNP.apply();
}

int CCDChip::getNAxis() const
{
    return NAxis;
}

void CCDChip::setNAxis(int value)
{
    NAxis = value;
}

void CCDChip::setImageExtension(const char *ext)
{
    strncpy(ImageExtention, ext, MAXINDIBLOBFMT);
}

void CCDChip::binFrame()
{
    if (BinX == 1)
        return;

    // Jasem: Keep full frame shadow in memory to enhance performance and just swap frame pointers after operation is complete
    if (BinFrame == nullptr)
        BinFrame = static_cast<uint8_t*>(IDSharedBlobAlloc(RawFrameSize));
    else
    {
        BinFrame = static_cast<uint8_t*>(IDSharedBlobRealloc(BinFrame, RawFrameSize));
        if (BinFrame == nullptr)
            BinFrame = static_cast<uint8_t*>(IDSharedBlobAlloc(RawFrameSize));
    }

    memset(BinFrame, 0, RawFrameSize);

    switch (getBPP())
    {
        case 8:
        {
            uint8_t *bin_buf = BinFrame;
            // Try to average pixels since in 8bit they get saturated pretty quickly
            double factor      = (BinX * BinX) / 2;
            double accumulator;

            for (uint32_t i = 0; i < SubH; i += BinX)
                for (uint32_t j = 0; j < SubW; j += BinX)
                {
                    accumulator = 0;
                    for (int k = 0; k < BinX; k++)
                    {
                        for (int l = 0; l < BinX; l++)
                        {
                            accumulator += *(RawFrame + j + (i + k) * SubW + l);
                        }
                    }

                    accumulator /= factor;
                    if (accumulator > UINT8_MAX)
                        *bin_buf = UINT8_MAX;
                    else
                        *bin_buf += static_cast<uint8_t>(accumulator);
                    bin_buf++;
                }
        }
        break;

        case 16:
        {
            uint16_t *bin_buf    = reinterpret_cast<uint16_t *>(BinFrame);
            uint16_t *RawFrame16 = reinterpret_cast<uint16_t *>(RawFrame);
            uint16_t val;

            for (uint32_t i = 0; i < SubH; i += BinX)
                for (uint32_t j = 0; j < SubW; j += BinX)
                {
                    for (int k = 0; k < BinX; k++)
                    {
                        for (int l = 0; l < BinX; l++)
                        {
                            val = *(RawFrame16 + j + (i + k) * SubW + l);
                            if (val + *bin_buf > UINT16_MAX)
                                *bin_buf = UINT16_MAX;
                            else
                                *bin_buf += val;
                        }
                    }
                    bin_buf++;
                }
        }
        break;

        default:
            return;
    }

    // Swap frame pointers
    uint8_t *rawFramePointer = RawFrame;
    RawFrame                 = BinFrame;
    // We just memset it next time we use it
    BinFrame = rawFramePointer;
}


// Thx8411:
// Binning Bayer frames
// Each raw frame pixel is mapped and summed onto the binned frame
// The right place of each pixel in the 2x2 Bayer matrix is found by :
// (((i/BinX) & 0xFFFFFFFE) + (i & 0x00000001))
// and
// ((j/BinX) & 0xFFFFFFFE) + (j & 0x00000001)
//
void CCDChip::binBayerFrame()
{
    if (BinX == 1)
        return;

    // Jasem: Keep full frame shadow in memory to enhance performance and just swap frame pointers after operation is complete
    if (BinFrame == nullptr)
        BinFrame = static_cast<uint8_t*>(IDSharedBlobAlloc(RawFrameSize));
    else
    {
        BinFrame = static_cast<uint8_t*>(IDSharedBlobRealloc(BinFrame, RawFrameSize));
        if (BinFrame == nullptr)
            BinFrame = static_cast<uint8_t*>(IDSharedBlobAlloc(RawFrameSize));
    }

    memset(BinFrame, 0, RawFrameSize);

    switch (getBPP())
    {
        // 8 bpp frame
        case 8:
        {
            uint32_t BinFrameOffset;
            uint32_t val;
            uint32_t BinW = SubW / BinX;
            uint8_t BinFactor = BinX * BinY;
            uint32_t RawOffset = 0;
            uint32_t BinOffsetH;

            // for each raw frame row
            for (uint32_t i = 0; i < SubH; i++)
            {
                // find the binned frame row
                BinOffsetH = (((i / BinY) & 0xFFFFFFFE) + (i & 0x00000001)) * BinW;
                // for each raw column
                for (uint32_t j = 0; j < SubW; j++)
                {
                    // find the proper position in the binned frame
                    BinFrameOffset = BinOffsetH + ((j / BinX) & 0xFFFFFFFE) + (j & 0x00000001);
                    // get the existing value in the binned frame
                    val = BinFrame[BinFrameOffset];
                    // add the new value, averaged and caped
                    val += RawFrame[RawOffset] / BinFactor;
                    if(val > UINT8_MAX)
                        val = UINT8_MAX;
                    // write back into the binned frame
                    BinFrame[BinFrameOffset] = static_cast<uint8_t>(val);
                    // next binned frame pixel
                    RawOffset++;
                }
            }
        }
        break;

        // 16 bpp frame
        case 16:
        {
            // works the same as the 8 bits version, without averaging but
            // mapped onto 16 bits pixel
            uint16_t *RawFrame16 = reinterpret_cast<uint16_t *>(RawFrame);
            uint16_t *BinFrame16 = reinterpret_cast<uint16_t *>(BinFrame);

            uint32_t BinFrameOffset;
            uint32_t val;
            uint32_t BinW = SubW / BinX;
            uint32_t RawOffset = 0;

            for (uint32_t i = 0; i < SubH; i++)
            {
                uint32_t BinOffsetH = (((i / BinY) & 0xFFFFFFFE) + (i & 0x00000001)) * BinW;
                for (uint32_t j = 0; j < SubW; j++)
                {
                    BinFrameOffset = BinOffsetH + ((j / BinX) & 0xFFFFFFFE) + (j & 0x00000001);
                    val = BinFrame16[BinFrameOffset];
                    val += RawFrame16[RawOffset];
                    if(val > UINT16_MAX)
                        val = UINT16_MAX;
                    BinFrame16[BinFrameOffset] = (uint16_t)val;
                    RawOffset++;
                }
            }

        }
        break;

        default:
            return;
    }

    // Swap frame pointers
    uint8_t *rawFramePointer = RawFrame;
    RawFrame                 = BinFrame;
    // We just memset it next time we use it
    BinFrame = rawFramePointer;
}

}
