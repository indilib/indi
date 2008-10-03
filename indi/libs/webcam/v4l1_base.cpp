/*
    Copyright (C) 2005 by Jasem Mutlaq
    Email: mutlaqja@ikarustech.com 

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

#include <iostream>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>

#include "ccvt.h"
#include "v4l1_base.h"
#include "eventloop.h"
#include "indidevapi.h"

#define ERRMSGSIZ	1024

using namespace std;

V4L1_Base::V4L1_Base()
{
   frameRate=10;
   fd=-1;
   //usingTimer = false;
   
   //frameUpdate = true;
   //selectCallBackID = -1;
   //timerCallBackID  = -1;

   YBuf       = NULL;
   UBuf       = NULL;
   VBuf       = NULL;
   colorBuffer= NULL;
   buffer_start=NULL;

}

V4L1_Base::~V4L1_Base()
{

  delete (YBuf);
  delete (UBuf);
  delete (VBuf);
  delete (colorBuffer);

}

int V4L1_Base::connectCam(const char * devpath, char *errmsg)
{
   options= (haveBrightness|haveContrast|haveHue|haveColor|haveWhiteness);

   buffer_start=NULL;
   frameRate=10;
   fd=-1;

   //cerr << "In connect Cam with device " << devpath << endl;
   if (-1 == (fd=open(devpath, O_RDWR | O_NONBLOCK, 0)))
   {
        if (-1 == (fd=open(devpath, O_RDONLY | O_NONBLOCK, 0)))
	{
      		strncpy(errmsg, strerror(errno), ERRMSGSIZ);
      		cerr << strerror(errno);
      		return -1;
	}
   }
   
   cerr << "Device opened" << endl;
   
   if (fd != -1) 
   {
      if (-1 == ioctl(fd,VIDIOCGCAP,&capability))
     {
         cerr << "Error: ioctl (VIDIOCGCAP)" << endl;
	 strncpy(errmsg, "ioctl (VIDIOCGCAP)", ERRMSGSIZ);
	 return -1;
      }
      if (-1 == ioctl (fd, VIDIOCGWIN, &window))
      {
         cerr << "Error ioctl (VIDIOCGWIN)" << endl;
	 strncpy(errmsg, "ioctl (VIDIOCGWIN)", ERRMSGSIZ);
	 return -1;
      }
      if (-1 == ioctl (fd, VIDIOCGPICT, &picture_format))
      {
         cerr << "Error: ioctl (VIDIOCGPICT)" << endl;
	 strncpy(errmsg, "ioctl (VIDIOCGPICT)", ERRMSGSIZ);
	 return -1;
      }

      init(0);
   }

   cerr << "initial size w:" << window.width << " -- h: " << window.height << endl;

   /*if (options & ioUseSelect)
   {
      selectCallBackID = addCallback(fd, V4L1_Base::staticUpdateFrame, this);
      cerr << "Using select to wait new frames." << endl;
   } else
   {
      usingTimer = true;
      timerCallBackID = addTimer(1000/frameRate, V4L1_Base::staticCallFrame, this);
      cerr << "Using timer to wait new frames.\n";
   }
    */

   if (mmapInit() < 0)
   {
	 strncpy(errmsg, "mmapInit() failed", ERRMSGSIZ);
	 return -1;
   }
   
   cerr << "All successful, returning\n";
   return fd;
}

void V4L1_Base::disconnectCam()
{
   
   
   delete YBuf;
   delete UBuf;
   delete VBuf;
   YBuf = UBuf = VBuf = NULL;
   
   if (selectCallBackID != -1)
     rmCallback(selectCallBackID);
     
   //if (usingTimer && timerCallBackID != -1)
     //rmTimer(timerCallBackID);
     
   if (munmap (buffer_start, mmap_buffer.size) < 0)
     fprintf(stderr, "munmap: %s\n", strerror(errno));
     
   if (close(fd) < 0)
     fprintf(stderr, "close(fd): %s\n", strerror(errno));
     
   fprintf(stderr, "Disconnect cam\n");
}

void V4L1_Base::newFrame() 
{
      switch (picture_format.palette) 
      {
       case VIDEO_PALETTE_GREY:
         memcpy(YBuf,mmapFrame(),window.width * window.height);
         break;
       case VIDEO_PALETTE_YUV420P:
         memcpy(YBuf,mmapFrame(), window.width * window.height);
         memcpy(UBuf,
                mmapFrame()+ window.width * window.height,
                (window.width/2) * (window.height/2));
         memcpy(VBuf,
                mmapFrame()+ window.width * window.height+(window.width/2) * (window.height/2),
                (window.width/2) * (window.height/2));
         break;
      case VIDEO_PALETTE_YUYV:
         ccvt_yuyv_420p(window.width,window.height,
                           mmapFrame(),
                           YBuf,
                           UBuf,
                           VBuf);
         break;

     case VIDEO_PALETTE_RGB24:
        RGB2YUV(window.width, window.height, mmapFrame(), YBuf, UBuf, VBuf, 0);
        break;

      default: 
         cerr << "invalid palette " <<picture_format.palette << endl;
         exit(1);
      }
   
   
    if (callback)
      (*callback)(uptr);

}

void V4L1_Base::updateFrame(int /*d*/, void * p)
{

  ( (V4L1_Base *) (p))->newFrame();

}

int V4L1_Base::start_capturing(char * /*errmsg*/)
{
   
   mmapCapture();
   mmapSync();

   selectCallBackID = IEAddCallback(fd, updateFrame, this);
   //newFrame();
   return 0;
}

int V4L1_Base::stop_capturing(char * /*errmsg*/)
{
  
  IERmCallback(selectCallBackID);
  selectCallBackID = -1;
  return 0;
}
 
int V4L1_Base::getWidth()
{
  return window.width;
}

int V4L1_Base::getHeight()
{
 return window.height;
}

void V4L1_Base::setFPS(int fps)
{
  frameRate = fps;
}

int V4L1_Base::getFPS()
{
  return frameRate;
}

char * V4L1_Base::getDeviceName()
{
  return capability.name;
}

void V4L1_Base::init(int preferedPalette)
 {

   if (preferedPalette)
   {
      picture_format.palette=preferedPalette;
      if (0 == ioctl(fd, VIDIOCSPICT, &picture_format))
        cerr <<  "found preferedPalette " << preferedPalette << endl;
      else
      {
         preferedPalette=0;
         cerr << "preferedPalette " << preferedPalette << " invalid, trying to find one." << endl;
      }
   }

   if (preferedPalette == 0)
   {
      do {
      /* trying VIDEO_PALETTE_YUV420P (Planar) */
      picture_format.palette=VIDEO_PALETTE_YUV420P;
      picture_format.depth=12;
      if (0 == ioctl(fd, VIDIOCSPICT, &picture_format)) {
         cerr << "found palette VIDEO_PALETTE_YUV420P" << endl;
	 break;
      }
      cerr << "VIDEO_PALETTE_YUV420P not supported." << endl;
      /* trying VIDEO_PALETTE_YUV420 (interlaced) */
      picture_format.palette=VIDEO_PALETTE_YUV420;
      picture_format.depth=12;
      if ( 0== ioctl(fd, VIDIOCSPICT, &picture_format)) {
         cerr << "found palette VIDEO_PALETTE_YUV420" << endl;
	 break;
      }
      cerr << "VIDEO_PALETTE_YUV420 not supported." << endl;
      /* trying VIDEO_PALETTE_RGB24 */
      picture_format.palette=VIDEO_PALETTE_RGB24;
      picture_format.depth=24;
      if ( 0== ioctl(fd, VIDIOCSPICT, &picture_format)) {
         cerr << "found palette VIDEO_PALETTE_RGB24" << endl;
      break;
      }
      cerr << "VIDEO_PALETTE_RGB24 not supported." << endl;
      /* trying VIDEO_PALETTE_GREY */
      picture_format.palette=VIDEO_PALETTE_GREY;
      picture_format.depth=8;
      if ( 0== ioctl(fd, VIDIOCSPICT, &picture_format)) {
         cerr << "found palette VIDEO_PALETTE_GREY" << endl;
      break;
      }
      cerr << "VIDEO_PALETTE_GREY not supported." << endl;
      cerr << "could not find a supported palette." << endl;
      exit(1);
      } while (false);
   }

   allocBuffers();

}

void V4L1_Base::allocBuffers()
{
   delete YBuf;
   delete UBuf;
   delete VBuf;
   delete colorBuffer;

   YBuf= new unsigned char[window.width * window.height];
   UBuf= new unsigned char[window.width * window.height];
   VBuf= new unsigned char[window.width * window.height];
   colorBuffer = new unsigned char[window.width * window.height * 4];
}

void V4L1_Base::checkSize(int & x, int & y)
{
   if (x >= capability.maxwidth && y >= capability.maxheight)
   {
      x=capability.maxwidth;
      y=capability.maxheight;
   }
   else if (x>=352 && y >=288) {
      x=352;y=288;
   } else if (x>=320 && y >= 240) {
      x=320;y=240;
   } else if (x>=176 && y >=144) {
      x=176;y=144;
   } else if (x>=160 && y >=120 ) {
      x=160;y=120;
   } else
   {
      x=capability.minwidth;
      y=capability.minheight;
   }
}

void V4L1_Base::getMaxMinSize(int & xmax, int & ymax, int & xmin, int & ymin)
{
  xmax = capability.maxwidth;
  ymax = capability.maxheight;
  xmin = capability.minwidth;
  ymin = capability.minheight;
}

bool V4L1_Base::setSize(int x, int y)
{
   int oldX, oldY;
   checkSize(x,y);
   
   oldX = window.width;
   oldY = window.height;
   
   window.width=x;
   window.height=y;

   cerr << "New size is x=" << window.width << " " << "y=" << window.height <<endl;
   
   if (ioctl (fd, VIDIOCSWIN, &window))
   {
       cerr << "ioctl(VIDIOCSWIN)" << endl;
       window.width=oldX;
       window.height=oldY;
       return false;
   }
   ioctl (fd, VIDIOCGWIN, &window);

   allocBuffers();
   
   return true;
}

void V4L1_Base::setContrast(int val)
{
   picture_format.contrast=val;
   setPictureSettings();
}

int V4L1_Base::getContrast()
{
   return picture_format.contrast;
}

void V4L1_Base::setBrightness(int val)
{
   picture_format.brightness=val;
   setPictureSettings();
}

int V4L1_Base::getBrightness()
{
   return picture_format.brightness;
}

void V4L1_Base::setColor(int val)
{
   picture_format.colour=val;
   setPictureSettings();
}

int V4L1_Base::getColor()
{
   return picture_format.colour;
}

void V4L1_Base::setHue(int val)
{
   picture_format.hue=val;
   setPictureSettings();
}

int V4L1_Base::getHue()
{
   return picture_format.hue;
}

void V4L1_Base::setWhiteness(int val)
{
   picture_format.whiteness=val;
   setPictureSettings();
}

int V4L1_Base::getWhiteness() 
{
   return picture_format.whiteness;
}

void V4L1_Base::setPictureSettings()
{
   if (ioctl(fd, VIDIOCSPICT, &picture_format) ) {
      cerr << "setPictureSettings" << endl;
   }
   ioctl(fd, VIDIOCGPICT, &picture_format);
}

void V4L1_Base::getPictureSettings()
{
   if (ioctl(fd, VIDIOCGPICT, &picture_format) )
   {
      cerr << "getPictureSettings" << endl;
   }
}

int V4L1_Base::mmapInit()
{
   mmap_buffer.size = 0;
   mmap_buffer.frames = 0;

   mmap_sync_buffer=-1;
   mmap_capture_buffer=-1;
   buffer_start=NULL;

   if (ioctl(fd, VIDIOCGMBUF, &mmap_buffer)) {
      // mmap not supported
      return -1;
   }

   buffer_start = (unsigned char *) mmap (NULL, mmap_buffer.size, PROT_READ, MAP_SHARED, fd, 0);

   if (buffer_start == MAP_FAILED)
    {
      cerr << "mmap" << endl;
      mmap_buffer.size = 0;
      mmap_buffer.frames = 0;
      buffer_start=NULL;
      return -1;
   }

   return 0;
}

void V4L1_Base::mmapCapture() 
{
   
   struct video_mmap vm;
   mmap_capture_buffer = (mmap_capture_buffer + 1) % mmap_buffer.frames;

   vm.frame = mmap_capture_buffer;
   vm.format = picture_format.palette;
   vm.width = window.width;
   vm.height = window.height;

   if (ioctl(fd, VIDIOCMCAPTURE, &vm) < 0)
        cerr << "Error V4L1_Base::mmapCapture" << endl;
}

void V4L1_Base::mmapSync()
{
   mmap_sync_buffer= (mmap_sync_buffer + 1) % mmap_buffer.frames;

   if (ioctl(fd, VIDIOCSYNC, &mmap_sync_buffer) < 0)
      cerr << "Error V4L1_Base::mmapSync()" << endl;
}

unsigned char * V4L1_Base::mmapFrame()
{
   return (buffer_start + mmap_buffer.offsets[mmap_sync_buffer]);
}

unsigned char * V4L1_Base::getY()
{
  return YBuf;
}

unsigned char * V4L1_Base::getU()
{
 return UBuf;
}

unsigned char * V4L1_Base::getV()
{
 return VBuf;
}

unsigned char * V4L1_Base::getColorBuffer()
{
  //cerr << "in get color buffer " << endl;
  
  switch (picture_format.palette) 
  {
      case VIDEO_PALETTE_YUV420P:
        ccvt_420p_bgr32(window.width, window.height,
                      mmapFrame(), (void*)colorBuffer);
      break;

    case VIDEO_PALETTE_YUYV:
         ccvt_yuyv_bgr32(window.width, window.height,
                      mmapFrame(), (void*)colorBuffer);
         break;
	 
    case VIDEO_PALETTE_RGB24:
         ccvt_rgb24_bgr32(window.width, window.height,
                      mmapFrame(), (void*)colorBuffer);
         break;
	 
   default:
    break;
  }
  

  return colorBuffer;

}

void V4L1_Base::registerCallback(WPF *fp, void *ud)
{
  callback = fp;
  uptr = ud;
}
