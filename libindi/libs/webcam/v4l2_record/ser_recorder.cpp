/*
    Copyright (C) 2014 by geehalel <geehalel@gmail.com>

    SER File Format Recorder (see http://www.grischa-hahn.homepage.t-online.de/astro/ser/index.htm)
    Specifications can be found in
      - for V2: http://www.grischa-hahn.homepage.t-online.de/astro/ser/SER%20Doc%20V2.pdf
      - for V3: http://www.grischa-hahn.homepage.t-online.de/astro/ser/SER%20Doc%20V3b.pdf

    SER Files may be used as input files for Registax 6 or astrostakkert
    (which you can both run under Linux using wine), or also Siril, the linux iris version.

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

#include "ser_recorder.h"

#include <ctime>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

#define ERRMSGSIZ 1024

SER_Recorder::SER_Recorder()
{
    name = "SER File Recorder";
    strncpy(serh.FileID, "LUCAM-RECORDER", 14);
    strncpy(serh.Observer, "                        Unknown Observer", 40);
    strncpy(serh.Instrume, "                      Unknown Instrument", 40);
    strncpy(serh.Telescope, "                       Unknown Telescope", 40);
    serh.LuID = 0;
    if (is_little_endian())
        serh.LittleEndian = SER_LITTLE_ENDIAN;
    else
        serh.LittleEndian = SER_BIG_ENDIAN;
    isRecordingActive = false;
    f                 = nullptr;
}

SER_Recorder::~SER_Recorder()
{
}

bool SER_Recorder::is_little_endian()
{
    unsigned int magic        = 0x00000001;
    unsigned char black_magic = *(unsigned char *)&magic;
    return black_magic == 0x01;
}

void SER_Recorder::write_int_le(uint32_t *i)
{
    if (is_little_endian())
        fwrite((const void *)(i), sizeof(uint32_t), 1, f);
    else
    {
        unsigned char *c = (unsigned char *)i;
        fwrite((const void *)(c + 3), sizeof(char), 1, f);
        fwrite((const void *)(c + 2), sizeof(char), 1, f);
        fwrite((const void *)(c + 1), sizeof(char), 1, f);
        fwrite((const void *)(c), sizeof(char), 1, f);
    }
}

void SER_Recorder::write_long_int_le(uint64_t *i)
{
    if (is_little_endian())
    {
        fwrite((const void *)((uint32_t *)i), sizeof(int), 1, f);
        fwrite((const void *)((uint32_t *)(i) + 1), sizeof(int), 1, f);
    }
    else
    {
        write_int_le((uint32_t *)(i) + 1);
        write_int_le((uint32_t *)(i));
    }
}

void SER_Recorder::write_header(ser_header *s)
{
    fwrite((const void *)(s->FileID), sizeof(char), 14, f);
    write_int_le(&(s->LuID));
    write_int_le(&(s->ColorID));
    write_int_le(&(s->LittleEndian));
    write_int_le(&(s->ImageWidth));
    write_int_le(&(s->ImageHeight));
    write_int_le(&(s->PixelDepth));
    write_int_le(&(s->FrameCount));
    fwrite((const void *)(s->Observer), sizeof(char), 40, f);
    fwrite((const void *)(s->Instrume), sizeof(char), 40, f);
    fwrite((const void *)(s->Telescope), sizeof(char), 40, f);
    write_long_int_le(&(s->DateTime));
    write_long_int_le(&(s->DateTime_UTC));
}

void SER_Recorder::init()
{
}

bool SER_Recorder::setPixelFormat(unsigned int format) // V4L2_PIX_FMT used when encoding
{
    IDLog("recorder: setpixelformat %d\n", format);
    serh.PixelDepth  = 8;
    number_of_planes = 1;
    switch (format)
    {
        case V4L2_PIX_FMT_GREY:
#ifdef V4L2_PIX_FMT_Y10
        case V4L2_PIX_FMT_Y10:
#endif
#ifdef V4L2_PIX_FMT_Y12
        case V4L2_PIX_FMT_Y12:
#endif
#ifdef V4L2_PIX_FMT_Y16
        case V4L2_PIX_FMT_Y16:
#endif
            serh.ColorID = SER_MONO;
#ifdef V4L2_PIX_FMT_Y10
            if (format == V4L2_PIX_FMT_Y10)
                serh.PixelDepth = 10;
#endif
#ifdef V4L2_PIX_FMT_Y12
            if (format == V4L2_PIX_FMT_Y12)
                serh.PixelDepth = 12;
#endif
#ifdef V4L2_PIX_FMT_Y16
            if (format == V4L2_PIX_FMT_Y16)
                serh.PixelDepth = 16;
#endif
            return true;
        case V4L2_PIX_FMT_SBGGR8:
#ifdef V4L2_PIX_FMT_SBGGR10
        case V4L2_PIX_FMT_SBGGR10:
#endif
#ifdef V4L2_PIX_FMT_SBGGR12
        case V4L2_PIX_FMT_SBGGR12:
#endif
        case V4L2_PIX_FMT_SBGGR16:
            serh.ColorID = SER_BAYER_BGGR;
#ifdef V4L2_PIX_FMT_SBGGR10
            if (format == V4L2_PIX_FMT_SBGGR10)
                serh.PixelDepth = 10;
#endif
#ifdef V4L2_PIX_FMT_SBGGR12
            if (format == V4L2_PIX_FMT_SBGGR12)
                serh.PixelDepth = 12;
#endif
            if (format == V4L2_PIX_FMT_SBGGR16)
                serh.PixelDepth = 16;
            return true;
        case V4L2_PIX_FMT_SGBRG8:
#ifdef V4L2_PIX_FMT_SGBRG10
        case V4L2_PIX_FMT_SGBRG10:
#endif
#ifdef V4L2_PIX_FMT_SGBRG12
        case V4L2_PIX_FMT_SGBRG12:
#endif
            serh.ColorID = SER_BAYER_GBRG;
#ifdef V4L2_PIX_FMT_SGBRG10
            if (format == V4L2_PIX_FMT_SGBRG10)
                serh.PixelDepth = 10;
#endif
#ifdef V4L2_PIX_FMT_SGBRG12
            if (format == V4L2_PIX_FMT_SGBRG12)
                serh.PixelDepth = 12;
#endif
            return true;
#if defined(V4L2_PIX_FMT_SGRBG8) || defined(V4L2_PIX_FMT_SGRBG10) || defined(V4L2_PIX_FMT_SGRBG12)
#ifdef V4L2_PIX_FMT_SGRBG8
        case V4L2_PIX_FMT_SGRBG8:
#endif
#ifdef V4L2_PIX_FMT_SGRBG10
        case V4L2_PIX_FMT_SGRBG10:
#endif
#ifdef V4L2_PIX_FMT_SGRBG12
        case V4L2_PIX_FMT_SGRBG12:
#endif
            serh.ColorID = SER_BAYER_GRBG;
#ifdef V4L2_PIX_FMT_SGRBG10
            if (format == V4L2_PIX_FMT_SGRBG10)
                serh.PixelDepth = 10;
#endif
#ifdef V4L2_PIX_FMT_SGRBG12
            if (format == V4L2_PIX_FMT_SGRBG12)
                serh.PixelDepth = 12;
#endif
            return true;
#endif
#if defined(V4L2_PIX_FMT_SRGGB8) || defined(V4L2_PIX_FMT_SRGGB10) || defined(V4L2_PIX_FMT_SRGGB12)
#ifdef V4L2_PIX_FMT_SRGGB8
        case V4L2_PIX_FMT_SRGGB8:
#endif
#ifdef V4L2_PIX_FMT_SRGGB10
        case V4L2_PIX_FMT_SRGGB10:
#endif
#ifdef V4L2_PIX_FMT_SRGGB12
        case V4L2_PIX_FMT_SRGGB12:
#endif
            serh.ColorID = SER_BAYER_RGGB;
#ifdef V4L2_PIX_FMT_SRGGB10
            if (format == V4L2_PIX_FMT_SRGGB10)
                serh.PixelDepth = 10;
#endif
#ifdef V4L2_PIX_FMT_SRGGB12
            if (format == V4L2_PIX_FMT_SRGGB12)
                serh.PixelDepth = 12;
#endif
            return true;
#endif
        case V4L2_PIX_FMT_RGB24:
            number_of_planes = 3;
            serh.ColorID     = SER_RGB;
            return true;
        case V4L2_PIX_FMT_BGR24:
            number_of_planes = 3;
            serh.ColorID     = SER_BGR;
            return true;
        default:
            return false;
    }
}

bool SER_Recorder::setFrame(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    if (isRecordingActive)
        return false;

    offsetX          = x;
    offsetY          = y;
    serh.ImageWidth  = width;
    serh.ImageHeight = height;

    return true;
}

bool SER_Recorder::setSize(uint16_t width, uint16_t height)
{
    if (isRecordingActive)
        return false;

    rawWidth  = width;
    rawHeight = height;
    return true;
}

bool SER_Recorder::open(const char *filename, char *errmsg)
{
    if (isRecordingActive)
        return false;
    serh.FrameCount = 0;
    if ((f = fopen(filename, "w")) == nullptr)
    {
        snprintf(errmsg, ERRMSGSIZ, "recorder open error %d, %s\n", errno, strerror(errno));
        return false;
    }

    serh.DateTime     = getLocalTimeStamp();
    serh.DateTime_UTC = getUTCTimeStamp();
    write_header(&serh);
    frame_size        = serh.ImageWidth * serh.ImageHeight * (serh.PixelDepth <= 8 ? 1 : 2) * number_of_planes;
    isRecordingActive = true;

    frameStamps.clear();

    return true;
}

bool SER_Recorder::close()
{
    if (f)
    {
        // Write all timestamps
        for (auto value : frameStamps)
            write_long_int_le(&value);

        frameStamps.clear();

        fseek(f, 0L, SEEK_SET);
        write_header(&serh);
        fclose(f);
        f = nullptr;
    }

    isRecordingActive = false;
    return true;
}

bool SER_Recorder::writeFrame(unsigned char *frame)
{
    if (!isRecordingActive)
        return false;

    frameStamps.push_back(getUTCTimeStamp());

    fwrite(frame, frame_size, 1, f);
    serh.FrameCount += 1;
    return true;
}

// ajouter une gestion plus fine du mode par defaut
// setMono/setColor appelee par ImageTypeSP
// setPixelDepth si Mono16
bool SER_Recorder::writeFrameMono(unsigned char *frame)
{
    if (isStreamingActive == false &&
        (offsetX > 0 || offsetY > 0 || serh.ImageWidth != rawWidth || serh.ImageHeight != rawHeight))
    {
        int offset = ((rawWidth * offsetY) + offsetX);

        uint8_t *srcBuffer  = frame + offset;
        uint8_t *destBuffer = frame;
        int imageWidth      = serh.ImageWidth;
        int imageHeight     = serh.ImageHeight;

        for (int i = 0; i < imageHeight; i++)
            memcpy(destBuffer + i * imageWidth, srcBuffer + rawWidth * i, imageWidth);
    }

    return writeFrame(frame);
}

bool SER_Recorder::writeFrameColor(unsigned char *frame)
{
    if (isStreamingActive == false &&
        (offsetX > 0 || offsetY > 0 || serh.ImageWidth != rawWidth || serh.ImageHeight != rawHeight))
    {
        int offset = ((rawWidth * offsetY) + offsetX);

        uint8_t *srcBuffer  = frame + offset * 3;
        uint8_t *destBuffer = frame;
        int imageWidth      = serh.ImageWidth;
        int imageHeight     = serh.ImageHeight;

        // RGB
        for (int i = 0; i < imageHeight; i++)
            memcpy(destBuffer + i * imageWidth * 3, srcBuffer + rawWidth * 3 * i, imageWidth * 3);
    }

    return writeFrame(frame);
}

void SER_Recorder::setDefaultMono()
{
    number_of_planes = 1;
    serh.PixelDepth  = 8;
    serh.ColorID     = SER_MONO;
}

void SER_Recorder::setDefaultColor()
{
    number_of_planes = 3;
    serh.PixelDepth  = 8;
    serh.ColorID     = SER_RGB;
}

// Copyright (C) 2015 Chris Garry
//

//
// Calculate if a year is a leap yer
//
bool SER_Recorder::is_leap_year(uint32_t year)
{
    if ((year % 400) == 0)
    {
        // If year is divisible by 400 then is_leap_year
        return true;
    }
    else if ((year % 100) == 0)
    {
        // Else if year is divisible by 100 then not_leap_year
        return false;
    }
    else if ((year % 4) == 0)
    {
        // Else if year is divisible by 4 then is_leap_year
        return true;
    }
    else
    {
        // Else not_leap_year
        return false;
    }
}

uint64_t SER_Recorder::getUTCTimeStamp()
{
    uint64_t utcTS;

    // Get starting time
    timeval currentTime;
    gettimeofday(&currentTime, nullptr);

    struct tm *tp;
    time_t t   = (time_t)currentTime.tv_sec;
    uint32_t u = currentTime.tv_usec;

    // UTC Time
    tp = gmtime(&t);

    dateTo64BitTS(tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec, u, &utcTS);

    return utcTS;
}

uint64_t SER_Recorder::getLocalTimeStamp()
{
    uint64_t localTS;

    // Get starting time
    timeval currentTime;
    gettimeofday(&currentTime, nullptr);

    struct tm *tp;
    time_t t   = (time_t)currentTime.tv_sec;
    uint32_t u = currentTime.tv_usec;

    // Local Time
    tp = localtime(&t);

    dateTo64BitTS(tp->tm_year, tp->tm_mon, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec, u, &localTS);

    return localTS;
}

// Convert real time to timestamp
//
void SER_Recorder::dateTo64BitTS(int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, int32_t second,
                                 int32_t microsec, uint64_t *p_ts)
{
    uint64_t ts = 0;
    int32_t yr;

    // Add 400 year blocks
    for (yr = 1; yr < (year - 400); yr += 400)
    {
        ts += m_septaseconds_per_400_years;
    }

    // Add 1 years
    for (; yr < year; yr++)
    {
        uint32_t days_this_year = 365;
        if (is_leap_year(yr))
        {
            days_this_year = 366;
        }

        ts += (days_this_year * m_septaseconds_per_day);
    }

    // Add months
    for (int mon = 1; mon < month; mon++)
    {
        switch (mon)
        {
            case 4:  // April
            case 6:  // June
            case 9:  // September
            case 11: // Novenber
                ts += (30 * m_septaseconds_per_day);
                break;
            case 2: // Feburary
                if (is_leap_year(year))
                {
                    ts += (29 * m_septaseconds_per_day);
                }
                else
                {
                    ts += (28 * m_septaseconds_per_day);
                }

                break;
            default:
                ts += (31 * m_septaseconds_per_day);
                break;
        }
    }

    // Add days
    ts += ((day - 1) * m_septaseconds_per_day);

    // Add hours
    ts += (hour * m_septaseconds_per_hour);

    // Add minutes
    ts += (minute * m_septaseconds_per_minute);

    // Add seconds
    ts += (second * C_SEPASECONDS_PER_SECOND);

    // Micro seconds
    ts += (microsec * m_sepaseconds_per_microsecond);

    // Output result
    *p_ts = ts;
}
