/*
    Copyright (C) 2005 by Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2014 by geehalel <geehalel@gmail.com>

    V4L2 Decode  

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

#ifndef V4L2_DECODE_H
#define V4L2_DECODE_H

#include <stdio.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include "indidevapi.h"

#include <vector>

class V4L2_Decoder
{
public:
V4L2_Decoder();
virtual ~V4L2_Decoder();

virtual void init()=0;
virtual const char *getName();
virtual bool setcrop(struct v4l2_crop c)=0;
virtual void resetcrop()=0;
virtual void usesoftcrop(bool c)=0;
virtual void setformat(struct v4l2_format f)=0;
virtual bool issupportedformat(unsigned int format)=0;
virtual const std::vector<unsigned int> &getsupportedformats()=0;
virtual void decode(unsigned char *frame, struct v4l2_buffer *buf)=0;
virtual unsigned char * getY()=0;
virtual unsigned char * getU()=0;
virtual unsigned char * getV()=0;
virtual unsigned char * getColorBuffer()=0;
virtual unsigned char * getRGBBuffer()=0;

protected:
const char *name;

};



class V4L2_Decode
{
public:
V4L2_Decode();
~V4L2_Decode();
std::vector<V4L2_Decoder *> getDecoderList();
V4L2_Decoder *getDecoder();
V4L2_Decoder *getDefaultDecoder();
void setDecoder(V4L2_Decoder *decoder);

protected:
std::vector<V4L2_Decoder *> decoder_list;
V4L2_Decoder *current_decoder;
V4L2_Decoder *default_decoder;

};

#endif
