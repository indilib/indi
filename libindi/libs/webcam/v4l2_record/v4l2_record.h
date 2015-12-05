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

#ifndef V4L2_RECORD_H
#define V4L2_RECORD_H

#include <stdio.h>
#include <stdlib.h>
#ifdef OSX_EMBEDED_MODE
#define v4l2_fourcc(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define V4L2_PIX_FMT_GREY    v4l2_fourcc('G', 'R', 'E', 'Y') /*  8  Greyscale     */
#define V4L2_PIX_FMT_SBGGR8  v4l2_fourcc('B', 'A', '8', '1') /*  8  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SBGGR16 v4l2_fourcc('B', 'Y', 'R', '2') /* 16  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG8  v4l2_fourcc('G', 'B', 'R', 'G') /*  8  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_BGR24   v4l2_fourcc('B', 'G', 'R', '3') /* 24  BGR-8-8-8     */
#define V4L2_PIX_FMT_RGB24   v4l2_fourcc('R', 'G', 'B', '3') /* 24  RGB-8-8-8     */
#define V4L2_PIX_FMT_SRGGB8  v4l2_fourcc('R', 'G', 'G', 'B') /*  8  RGRG.. GBGB.. */
#define V4L2_PIX_FMT_SGRBG8  v4l2_fourcc('G', 'R', 'B', 'G') /*  8  GRGR.. BGBG.. */

#else
#include <linux/videodev2.h>
#endif
#include <indidevapi.h>

#include <vector>

class V4L2_Recorder
{
public:
V4L2_Recorder();
virtual ~V4L2_Recorder();

virtual void init()=0;
virtual const char *getName();
virtual bool setpixelformat(unsigned int pixformat)=0; // true when direct encoding of pixel format
virtual bool setsize(unsigned int width, unsigned int height)=0; // set image size in pixels
virtual bool open(const char *filename, char *errmsg)=0;
virtual bool close()=0;
virtual bool writeFrame(unsigned char *frame)=0; // when frame is in known encoding format
virtual bool writeFrameMono(unsigned char *frame)=0; // default way to write a GREY frame
virtual bool writeFrameColor(unsigned char *frame)=0; // default way to write a RGB24 frame
virtual void setDefaultMono()=0; // prepare to write GREY frame
virtual void setDefaultColor()=0; // prepare to write RGB24 frame

protected:
const char *name;
  
};

class V4L2_Record
{
public:
V4L2_Record();
~V4L2_Record();
std::vector<V4L2_Recorder *> getRecorderList();
V4L2_Recorder *getRecorder();
V4L2_Recorder *getDefaultRecorder();
void setRecorder(V4L2_Recorder *recorder);

protected:
std::vector<V4L2_Recorder *> recorder_list;
V4L2_Recorder *current_recorder;
V4L2_Recorder *default_recorder;

};
#endif // V4L2_RECORD_H
