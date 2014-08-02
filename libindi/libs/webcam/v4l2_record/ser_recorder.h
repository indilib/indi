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

#ifndef SER_RECORDER_H
#define SER_RECORDER_H

#include "v4l2_record.h"
#include <linux/videodev2.h>
#include <stdio.h>

typedef struct ser_header {
  char FileID[14];
  unsigned int LuID;
  unsigned int ColorID;
  unsigned int LittleEndian;
  unsigned int ImageWidth;
  unsigned int ImageHeight;
  unsigned int PixelDepth;
  unsigned int FrameCount;
  char Observer[40];
  char Instrume[40];
  char Telescope[40];
  unsigned long int DateTime;
  unsigned long int DateTime_UTC;
} ser_header;

enum ser_color_id {
  SER_MONO = 0,
  SER_BAYER_RGGB = 8,
  SER_BAYER_GRBG = 9,
  SER_BAYER_GBRG = 10,
  SER_BAYER_BGGR = 11,
  SER_BAYER_CYYM = 16,
  SER_BAYER_YCMY = 17,
  SER_BAYER_YMCY = 18,
  SER_BAYER_MYYC = 19,
  SER_RGB = 100,
  SER_BGR = 101
};

#define SER_BIG_ENDIAN 0
#define SER_LITTLE_ENDIAN 1

class SER_Recorder: public V4L2_Recorder
{
 public:


  SER_Recorder();
  virtual ~SER_Recorder();
  
  virtual void init();
  virtual bool setpixelformat(unsigned int f);
  virtual bool setsize(unsigned int width, unsigned int height);
  virtual bool open(const char *filename, char *errmsg);
  virtual bool close();
  virtual bool writeFrame(unsigned char *frame);
  virtual bool writeFrameMono(unsigned char *frame); // default way to write a GREY frame
  virtual bool writeFrameColor(unsigned char *frame); // default way to write a RGB3 frame
  virtual void setDefaultMono(); // prepare to write GREY frame
  virtual void setDefaultColor(); // prepare to write RGB24 frame


 protected:
  bool is_little_endian();
  void write_int_le(unsigned int *i);
  void write_long_int_le(unsigned long *i);
  void write_header(ser_header *s);
  ser_header serh;
  bool streaming_active;
  bool useSER_V3;
  FILE *f;
  unsigned int frame_size;
  unsigned int number_of_planes;
};

#endif // SER_RECORDER_H
