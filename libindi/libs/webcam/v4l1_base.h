/*
    Copyright (C) 2005 by Jasem Mutlaq

    Some code based on qastrocam

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

#ifndef V4L1_BASE_H
#define V4L1_BASE_H

#include <stdio.h>
#include <stdlib.h>
#include "videodev.h"
#include "eventloop.h"

class V4L1_Base
{
  public:
   V4L1_Base();
   virtual ~V4L1_Base();

  /* Connection */
  virtual int connectCam(const char * devpath, char *errmsg);
  virtual void disconnectCam();
  char * getDeviceName();

  /* Image settings */
  int  getBrightness();
  int  getContrast();
  int  getColor();
  int  getHue();
  int  getWhiteness();
  void setContrast(int val);
  void setBrightness(int val);
  void setColor(int val);
  void setHue(int val);
  void setWhiteness(int val);

  /* Updates */
  static void updateFrame(int d, void * p);
  void newFrame();
  void setPictureSettings();
  void getPictureSettings();

  /* Image Size */
  int getWidth();
  int getHeight();
  void checkSize(int & x, int & y);
  virtual bool setSize(int x, int y);
  virtual void getMaxMinSize(int & xmax, int & ymax, int & xmin, int & ymin);

  /* Frame rate */
  void setFPS(int fps);
  int  getFPS();

  void init(int preferedPalette);
  void allocBuffers();
  int  mmapInit();
  void mmapCapture();
  void mmapSync();

  unsigned char * mmapFrame();
  unsigned char * getY();
  unsigned char * getU();
  unsigned char * getV();
  unsigned char * getColorBuffer();

  int start_capturing(char *errmsg);
  int stop_capturing(char *errmsg);
  void registerCallback(WPF *fp, void *ud);

  protected:

  int fd;
  WPF *callback;
  void *uptr;
  unsigned long options;

  struct video_capability capability;
  struct video_window window;
  struct video_picture picture_format;
  struct video_mbuf mmap_buffer;

  unsigned char * buffer_start;
  
  long mmap_sync_buffer;
  long mmap_capture_buffer;  
  
  int frameRate;
  bool streamActive;
  int  selectCallBackID;
  unsigned char * YBuf,*UBuf,*VBuf, *colorBuffer;

};
   
#endif
