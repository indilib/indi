/*
    Copyright (C) 2005 by Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on V4L 2 Example
    http://v4l2spec.bytesex.org/spec-single/v4l2.html#CAPTURE-EXAMPLE

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

#ifndef V4L2_BASE_H
#define V4L2_BASE_H

#include <stdio.h>
#include <stdlib.h>
#include "videodev2.h"
#include "eventloop.h"
#include "indidevapi.h"

#define VIDEO_COMPRESSION_LEVEL		4

class V4L2_Base
{
  public:
   V4L2_Base();
   virtual ~V4L2_Base();

   typedef enum { IO_METHOD_READ, IO_METHOD_MMAP, IO_METHOD_USERPTR } io_method;

   struct buffer
   {
        void *                  start;
        size_t                  length;
   };

  /* Connection */
  virtual int connectCam(const char * devpath, char *errmsg, int pixelFormat = -1 , int width = -1, int height = -1);
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
  void callFrame(void *p);
  void setPictureSettings();
  void getPictureSettings();

  /* Image Size */
  int getWidth();
  int getHeight();
  virtual int setSize(int x, int y);
  virtual void getMaxMinSize(int & x_max, int & y_max, int & x_min, int & y_min);

  /* Frame rate */
  void setFPS(int fps);
  int  getFPS();

  void allocBuffers();
  unsigned char * getY();
  unsigned char * getU();
  unsigned char * getV();
  unsigned char * getColorBuffer();

  void registerCallback(WPF *fp, void *ud);

  int start_capturing(char *errmsg);
  int stop_capturing(char *errmsg);
  static void newFrame(int fd, void *p);
  
  void enumerate_ctrl (void);
  void enumerate_menu (void);
  int  queryINTControls(INumberVectorProperty *nvp);
  int  setINTControl(unsigned int ctrl_id, double new_value, char *errmsg);

  int  query_ctrl(unsigned int ctrl_id, double & ctrl_min, double & ctrl_max, double & ctrl_step, double & ctrl_value, char *errmsg);

  protected:

  int xioctl(int fd, int request, void *arg);
  int read_frame(char *errsg);
  int uninit_device(char *errmsg);
  int open_device(const char *devpath, char *errmsg);
  int init_device(char *errmsg, int pixelFormat , int width, int height); 
  int init_mmap(char *errmsg);
  int errno_exit(const char *s, char *errmsg);
  
  void close_device(void);
  void init_userp(unsigned int buffer_size);
  void init_read(unsigned int buffer_size);

  void findMinMax();
  
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;

  struct v4l2_queryctrl queryctrl;
  struct v4l2_querymenu querymenu;

  WPF *callback;
  void *uptr;
  char          dev_name[64];
  io_method	io;
  int           fd;
  struct buffer *buffers;
  unsigned int  n_buffers;

  bool		dropFrame;

  
  int frameRate;
  int  xmax, xmin, ymax, ymin;
  int  selectCallBackID;
  unsigned char * YBuf,*UBuf,*VBuf, *colorBuffer, *rgb24_buffer;

};
   
#endif
