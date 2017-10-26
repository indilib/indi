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

#pragma once

#include "recorderinterface.h"

#include <cstdint>
#include <stdio.h>

namespace INDI
{

class TheoraRecorder : public RecorderInterface
{
  public:
    TheoraRecorder();
    virtual ~TheoraRecorder() = default;

    virtual void init();
    virtual const char *getExtension() { return ".ogv"; }
    virtual bool setPixelFormat(INDI_PIXEL_FORMAT pixelFormat, uint8_t pixelDepth);
    virtual bool setSize(uint16_t width, uint16_t height);
    virtual bool setFrame(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    virtual bool open(const char *filename, char *errmsg);
    virtual bool close();
    virtual bool writeFrame(uint8_t *frame);
    virtual void setStreamEnabled(bool enable) { isStreamingActive = enable; }

  protected:
    bool isRecordingActive = false, isStreamingActive = false;
    FILE *f;
    uint32_t frame_size;
    uint32_t number_of_planes;
    uint16_t subX = 0, subY = 0, subW = 0, subH = 0, rawWidth = 0, rawHeight = 0;
    std::vector<uint64_t> frameStamps;

  private:    
};
}
