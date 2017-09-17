/*
    Copyright (C) 2014 by geehalel <geehalel@gmail.com>

    V4L2 Record

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

#include "v4l2_record.h"

#include <cstdint>
#include <stdio.h>

#ifdef OSX_EMBEDED_MODE
//#include "videodev2.h"
#else
#include <linux/videodev2.h>
#endif

typedef struct ser_header
{
    char FileID[14];
    uint32_t LuID;
    uint32_t ColorID;
    uint32_t LittleEndian;
    uint32_t ImageWidth;
    uint32_t ImageHeight;
    uint32_t PixelDepth;
    uint32_t FrameCount;
    char Observer[40];
    char Instrume[40];
    char Telescope[40];
    uint64_t DateTime;
    uint64_t DateTime_UTC;
} ser_header;

enum ser_color_id
{
    SER_MONO       = 0,
    SER_BAYER_RGGB = 8,
    SER_BAYER_GRBG = 9,
    SER_BAYER_GBRG = 10,
    SER_BAYER_BGGR = 11,
    SER_BAYER_CYYM = 16,
    SER_BAYER_YCMY = 17,
    SER_BAYER_YMCY = 18,
    SER_BAYER_MYYC = 19,
    SER_RGB        = 100,
    SER_BGR        = 101
};

#define SER_BIG_ENDIAN    0
#define SER_LITTLE_ENDIAN 1

class SER_Recorder : public V4L2_Recorder
{
  public:
    SER_Recorder();
    virtual ~SER_Recorder();

    virtual void init();
    virtual bool setPixelFormat(uint32_t f);
    virtual bool setSize(uint16_t width, uint16_t height);
    virtual bool setFrame(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    virtual bool open(const char *filename, char *errmsg);
    virtual bool close();
    virtual bool writeFrame(unsigned char *frame);
    virtual bool writeFrameMono(unsigned char *frame);  // default way to write a GREY frame
    virtual bool writeFrameColor(unsigned char *frame); // default way to write a RGB3 frame
    virtual void setDefaultMono();                      // prepare to write GREY frame
    virtual void setDefaultColor();                     // prepare to write RGB24 frame
    virtual void setStreamEnabled(bool enable) { isStreamingActive = enable; }

    // Public constants
    static const uint64_t C_SEPASECONDS_PER_SECOND = 10000000;

  protected:
    uint64_t utcTo64BitTS();
    bool is_little_endian();
    void write_int_le(uint32_t *i);
    void write_long_int_le(uint64_t *i);
    void write_header(ser_header *s);
    ser_header serh;
    bool isRecordingActive = false, isStreamingActive = false;
    FILE *f;
    uint32_t frame_size;
    uint32_t number_of_planes;
    uint16_t offsetX = 0, offsetY = 0, rawWidth = 0, rawHeight = 0;
    std::vector<uint64_t> frameStamps;

  private:
    // From pipp_timestamp.h
    // Copyright (C) 2015 Chris Garry

    // Date to MS 64bit timestamp format for SER header
    void dateTo64BitTS(int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second,
                       int32_t microsec, uint64_t *p_ts);

    uint64_t getUTCTimeStamp();
    uint64_t getLocalTimeStamp();

    // Calculate if a year is a leap yer
    ///
    static bool is_leap_year(uint32_t year);

    // Constants
    static const uint64_t m_sepaseconds_per_microsecond  = 10;
    static const uint64_t m_septaseconds_per_part_minute = C_SEPASECONDS_PER_SECOND * 6;
    static const uint64_t m_septaseconds_per_minute      = C_SEPASECONDS_PER_SECOND * 60;
    static const uint64_t m_septaseconds_per_hour        = C_SEPASECONDS_PER_SECOND * 60 * 60;
    static const uint64_t m_septaseconds_per_day         = m_septaseconds_per_hour * 24;
    static const uint32_t m_days_in_400_years            = 303 * 365 + 97 * 366;
    static const uint64_t m_septaseconds_per_400_years   = m_days_in_400_years * m_septaseconds_per_day;
};
