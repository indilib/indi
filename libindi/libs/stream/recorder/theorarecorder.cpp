/*
    Copyright (C) 2017 by Jasem Mutlaq <mutlaqja@ikarustech.com>

    Theora Recorder

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

#include "theorarecorder.h"

#include <ctime>
#include <cerrno>
#include <cstring>
#include <sys/time.h>

#define ERRMSGSIZ 1024

namespace INDI
{

TheoraRecorder::TheoraRecorder()
{
    name = "Theora";
    isRecordingActive = false;
    f                 = nullptr;
}

void TheoraRecorder::init()
{
}

bool TheoraRecorder::setPixelFormat(INDI_PIXEL_FORMAT pixelFormat, uint8_t pixelDepth)
{   
    return true;
}

bool TheoraRecorder::setFrame(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    if (isRecordingActive)
        return false;

    subX     = x;
    subY     = y;
    subW     = width;
    subH     = height;

    return true;
}

bool TheoraRecorder::setSize(uint16_t width, uint16_t height)
{
    if (isRecordingActive)
        return false;

    rawWidth  = width;
    rawHeight = height;
    return true;
}

bool TheoraRecorder::open(const char *filename, char *errmsg)
{
    if (isRecordingActive)
        return false;

    return true;
}

bool TheoraRecorder::close()
{
    if (f)
    {        
        fclose(f);
        f = nullptr;
    }

    isRecordingActive = false;
    return true;
}

bool TheoraRecorder::writeFrame(uint8_t *frame)
{
    if (!isRecordingActive)
        return false;

    return true;
}

# if 0
bool TheoraRecorder::writeFrameMono(uint8_t *frame)
{
    if (isStreamingActive == false &&
            (subX > 0 || subY > 0 || subW != rawWidth || subH != rawHeight))
    {
        int offset = ((rawWidth * subY) + subX);

        uint8_t *srcBuffer  = frame + offset;
        uint8_t *destBuffer = frame;
        int imageWidth      = subW;
        int imageHeight     = subH;

        for (int i = 0; i < imageHeight; i++)
            memcpy(destBuffer + i * imageWidth, srcBuffer + rawWidth * i, imageWidth);
    }

    return writeFrame(frame);
}

bool TheoraRecorder::writeFrameColor(uint8_t *frame)
{
    if (isStreamingActive == false &&
            (subX > 0 || subY > 0 || subW != rawWidth || subH != rawHeight))
    {
        int offset = ((rawWidth * subY) + subX);

        uint8_t *srcBuffer  = frame + offset * 3;
        uint8_t *destBuffer = frame;
        int imageWidth      = subW;
        int imageHeight     = subH;

        // RGB
        for (int i = 0; i < imageHeight; i++)
            memcpy(destBuffer + i * imageWidth * 3, srcBuffer + rawWidth * 3 * i, imageWidth * 3);
    }

    return writeFrame(frame);
}
#endif

}
