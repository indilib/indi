/*
    Copyright (C) 2017 by Jasem Mutlaq <mutlaqja@ikarustech.com>

    INDI Raw Encoder

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "rawencoder.h"
#include "stream/streammanager.h"

namespace INDI
{

RawEncoder::RawEncoder(StreamManager *sm) : EncoderInterface(sm)
{

}

bool RawEncoder::upload()
{

    int ret                = 0;
    uLongf compressedBytes = 0;
    uLong totalBytes       = currentCCD->PrimaryCCD.getFrameBufferSize();
    uint8_t *buffer        = currentCCD->PrimaryCCD.getFrameBuffer();

    //memcpy(currentCCD->PrimaryCCD.getFrameBuffer(), buffer, currentCCD->PrimaryCCD.getFrameBufferSize());

    // Binning for grayscale frames only for now
    if (currentCCD->PrimaryCCD.getNAxis() == 2)
    {
        currentCCD->PrimaryCCD.binFrame();
        totalBytes /= (currentCCD->PrimaryCCD.getBinX() * currentCCD->PrimaryCCD.getBinY());
    }

    int subX, subY, subW, subH;
    subX = currentCCD->PrimaryCCD.getSubX();
    subY = currentCCD->PrimaryCCD.getSubY();
    subW = currentCCD->PrimaryCCD.getSubW();
    subH = currentCCD->PrimaryCCD.getSubH();

    // If stream frame was not yet initilized, let's do that now
    if (StreamFrameN[CCDChip::FRAME_W].value == 0 || StreamFrameN[CCDChip::FRAME_H].value == 0)
    {
        int binFactor = 1;
        if (currentCCD->PrimaryCCD.getNAxis() == 2)
            binFactor = currentCCD->PrimaryCCD.getBinX();

        StreamFrameN[CCDChip::FRAME_X].value = subX;
        StreamFrameN[CCDChip::FRAME_Y].value = subY;
        StreamFrameN[CCDChip::FRAME_W].value = subW / binFactor;
        StreamFrameN[CCDChip::FRAME_W].value = subH / binFactor;
        StreamFrameNP.s                      = IPS_IDLE;
        IDSetNumber(&StreamFrameNP, nullptr);
    }
    // Check if we need to subframe
    else if ((StreamFrameN[CCDChip::FRAME_W].value > 0 && StreamFrameN[CCDChip::FRAME_H].value > 0) &&
             (StreamFrameN[CCDChip::FRAME_X].value != subX || StreamFrameN[CCDChip::FRAME_Y].value != subY ||
              StreamFrameN[CCDChip::FRAME_W].value != subW || StreamFrameN[CCDChip::FRAME_H].value != subH))
    {
        // For MONO
        if (currentCCD->PrimaryCCD.getNAxis() == 2)
        {
            int binFactor = (currentCCD->PrimaryCCD.getBinX() * currentCCD->PrimaryCCD.getBinY());
            int offset =
                ((subW * StreamFrameN[CCDChip::FRAME_Y].value) + StreamFrameN[CCDChip::FRAME_X].value) / binFactor;

            uint8_t *srcBuffer  = buffer + offset;
            uint8_t *destBuffer = buffer;

            for (int i = 0; i < StreamFrameN[CCDChip::FRAME_H].value; i++)
                memcpy(destBuffer + i * static_cast<int>(StreamFrameN[CCDChip::FRAME_W].value), srcBuffer + subW * i,
                       StreamFrameN[CCDChip::FRAME_W].value);

            totalBytes =
                (StreamFrameN[CCDChip::FRAME_W].value * StreamFrameN[CCDChip::FRAME_H].value) / (binFactor * binFactor);
        }
        // For Color
        else
        {
            // Subframe offset in source frame. i.e. where we start copying data from in the original data frame
            int sourceOffset = (subW * StreamFrameN[CCDChip::FRAME_Y].value) + StreamFrameN[CCDChip::FRAME_X].value;
            // Total bytes
            totalBytes = (StreamFrameN[CCDChip::FRAME_W].value * StreamFrameN[CCDChip::FRAME_H].value) * 3;

            // Copy each color component back into buffer. Since each subframed page is equal or small than source component
            // no need to a new buffer

            uint8_t *srcBuffer  = buffer + sourceOffset * 3;
            uint8_t *destBuffer = buffer;

            // RGB
            for (int i = 0; i < StreamFrameN[CCDChip::FRAME_H].value; i++)
                memcpy(destBuffer + i * static_cast<int>(StreamFrameN[CCDChip::FRAME_W].value * 3),
                       srcBuffer + subW * 3 * i, StreamFrameN[CCDChip::FRAME_W].value * 3);
        }
    }

    // Do we want to compress ?
    if (currentCCD->PrimaryCCD.isCompressed())
    {
        /* Compress frame */
        compressedFrame = (uint8_t *)realloc(compressedFrame, sizeof(uint8_t) * totalBytes + totalBytes / 64 + 16 + 3);
        compressedBytes = sizeof(uint8_t) * totalBytes + totalBytes / 64 + 16 + 3;

        ret = compress2(compressedFrame, &compressedBytes, buffer, totalBytes, 4);
        if (ret != Z_OK)
        {
            /* this should NEVER happen */
            DEBUGF(INDI::Logger::DBG_ERROR, "internal error - compression failed: %d", ret);
            return false;
        }

        // Send it compressed
        imageB->blob    = compressedFrame;
        imageB->bloblen = compressedBytes;
        imageB->size    = totalBytes;
        strcpy(imageB->format, ".stream.z");
    }
    else
    {
        // Send it uncompressed
        imageB->blob    = buffer;
        imageB->bloblen = totalBytes;
        imageB->size    = totalBytes;
        strcpy(imageB->format, ".stream");
    }

    // Upload to client now
    imageBP->s = IPS_OK;
    IDSetBLOB(imageBP, nullptr);
    return true;
}
}
