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
#include "locale_compat.h"

#include <cstring>
#include <ctime>

namespace INDI
{

CCDChip::CCDChip()
{
    SendCompressed = false;
    Interlaced     = false;

    SubX = SubY = 0;
    SubW = SubH = 1;
    BPP         = 8;
    BinX = BinY = 1;
    NAxis       = 2;

    BinFrame = nullptr;

    strncpy(imageExtention, "fits", MAXINDIBLOBFMT);

    FrameType  = LIGHT_FRAME;
    //lastRapidX = lastRapidY = -1;
}

CCDChip::~CCDChip()
{
    delete [] RawFrame;
    delete[] BinFrame;
}

void CCDChip::setFrameType(CCD_FRAME type)
{
    FrameType = type;
}

void CCDChip::setResolution(int x, int y)
{
    XRes = x;
    YRes = y;

    ImagePixelSizeN[0].value = x;
    ImagePixelSizeN[1].value = y;

    IDSetNumber(&ImagePixelSizeNP, nullptr);

    ImageFrameN[FRAME_X].min = 0;
    ImageFrameN[FRAME_X].max = x - 1;
    ImageFrameN[FRAME_Y].min = 0;
    ImageFrameN[FRAME_Y].max = y - 1;

    ImageFrameN[FRAME_W].min = 1;
    ImageFrameN[FRAME_W].max = x;
    ImageFrameN[FRAME_H].max = 1;
    ImageFrameN[FRAME_H].max = y;
    IUUpdateMinMax(&ImageFrameNP);
}

void CCDChip::setFrame(int subx, int suby, int subw, int subh)
{
    SubX = subx;
    SubY = suby;
    SubW = subw;
    SubH = subh;

    ImageFrameN[FRAME_X].value = SubX;
    ImageFrameN[FRAME_Y].value = SubY;
    ImageFrameN[FRAME_W].value = SubW;
    ImageFrameN[FRAME_H].value = SubH;

    IDSetNumber(&ImageFrameNP, nullptr);
}

void CCDChip::setBin(int hor, int ver)
{
    BinX = hor;
    BinY = ver;

    ImageBinN[BIN_W].value = BinX;
    ImageBinN[BIN_H].value = BinY;

    IDSetNumber(&ImageBinNP, nullptr);
}

void CCDChip::setMinMaxStep(const char *property, const char *element, double min, double max, double step,
                            bool sendToClient)
{
    INumberVectorProperty *nvp = nullptr;

    if (!strcmp(property, ImageExposureNP.name))
        nvp = &ImageExposureNP;
    else if (!strcmp(property, ImageFrameNP.name))
        nvp = &ImageFrameNP;
    else if (!strcmp(property, ImageBinNP.name))
        nvp = &ImageBinNP;
    else if (!strcmp(property, ImagePixelSizeNP.name))
        nvp = &ImagePixelSizeNP;
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

void CCDChip::setPixelSize(float x, float y)
{
    PixelSizex = x;
    PixelSizey = y;

    ImagePixelSizeN[2].value = x;
    ImagePixelSizeN[3].value = x;
    ImagePixelSizeN[4].value = y;

    IDSetNumber(&ImagePixelSizeNP, nullptr);
}

void CCDChip::setBPP(int bbp)
{
    BPP = bbp;

    ImagePixelSizeN[5].value = BPP;

    IDSetNumber(&ImagePixelSizeNP, nullptr);
}

void CCDChip::setFrameBufferSize(int nbuf, bool allocMem)
{
    if (nbuf == RawFrameSize)
        return;

    RawFrameSize = nbuf;

    if (allocMem == false)
        return;

    delete [] RawFrame;
    RawFrame = new uint8_t[nbuf];

    if (BinFrame)
    {
        delete [] BinFrame;
        BinFrame = new uint8_t[nbuf];
    }
}

void CCDChip::setExposureLeft(double duration)
{
    ImageExposureNP.s = IPS_BUSY;
    ImageExposureN[0].value = duration;

    IDSetNumber(&ImageExposureNP, nullptr);
}

void CCDChip::setExposureDuration(double duration)
{
    exposureDuration = duration;
    gettimeofday(&startExposureTime, nullptr);
}

const char *CCDChip::getFrameTypeName(CCD_FRAME fType)
{
    return FrameTypeS[fType].name;
}

const char *CCDChip::getExposureStartTime()
{
    static char ts[32];

    char iso8601[32] = {0};
    struct tm *tp = nullptr;

    // Get exposure startup timestamp
    time_t t = static_cast<time_t>(startExposureTime.tv_sec);

    // Get UTC timestamp
    tp = gmtime(&t);

    // Format it in ISO8601 format
    strftime(iso8601, sizeof(iso8601), "%Y-%m-%dT%H:%M:%S", tp);

    // Add millisecond
    snprintf(ts, 32, "%s.%03d", iso8601, static_cast<int>(startExposureTime.tv_usec / 1000.0));

    return (ts);
}

void CCDChip::setInterlaced(bool intr)
{
    Interlaced = intr;
}

void CCDChip::setExposureFailed()
{
    ImageExposureNP.s = IPS_ALERT;
    IDSetNumber(&ImageExposureNP, nullptr);
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
    strncpy(imageExtention, ext, MAXINDIBLOBFMT);
}

void CCDChip::binFrame()
{
    if (BinX == 1)
        return;

    // Jasem: Keep full frame shadow in memory to enhance performance and just swap frame pointers after operation is complete
    if (BinFrame == nullptr)
        BinFrame = new uint8_t[RawFrameSize];

    memset(BinFrame, 0, RawFrameSize);

    switch (getBPP())
    {
        case 8:
        {
            uint8_t *bin_buf = BinFrame;
            // Try to average pixels since in 8bit they get saturated pretty quickly
            double factor      = (BinX * BinX) / 2;
            double accumulator = 0;

            for (int i = 0; i < SubH; i += BinX)
                for (int j = 0; j < SubW; j += BinX)
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
            for (int i = 0; i < SubH; i += BinX)
                for (int j = 0; j < SubW; j += BinX)
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

}
