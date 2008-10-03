/*
    Phlips webcam driver for V4L 1
    Copyright (C) 2005 by Jasem Mutlaq <mutlaqja@ikarustech.com>

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
#include "pwc-ioctl.h"
#include "v4l1_pwc.h"
#include "../eventloop.h"

#define ERRMSG_SIZ	1024

extern int errno;
using namespace std;

V4L1_PWC::V4L1_PWC()
{
   frameRate=15;
   fd=-1;
   streamActive = true;

   YBuf       = NULL;
   UBuf       = NULL;
   VBuf       = NULL;
   colorBuffer= NULL;
   buffer_start=NULL;
}

V4L1_PWC::~V4L1_PWC()
{

} 

int V4L1_PWC::connectCam(const char * devpath, char *errmsg)
{
   options= (ioNoBlock|ioUseSelect|haveBrightness|haveContrast|haveColor);
   struct pwc_probe probe;
   bool IsPhilips = false;
   frameRate=15;
   fd=-1;
   streamActive = true;
   buffer_start=NULL;
   
   if (-1 == (fd=open(devpath,
                           O_RDONLY | ((options & ioNoBlock) ? O_NONBLOCK : 0)))) {
      
      strncpy(errmsg, strerror(errno), 1024);
      cerr << strerror(errno);
      return -1;
   } 
  
   cerr << "Device opened" << endl;
   
   if (fd != -1) {
      if (-1 == ioctl(fd,VIDIOCGCAP,&capability)) {
         cerr << "Error: ioctl (VIDIOCGCAP)" << endl;
	 strncpy(errmsg, "ioctl (VIDIOCGCAP)", 1024);
	 return -1;
      }
      if (-1 == ioctl (fd, VIDIOCGWIN, &window)) {
         cerr << "Error: ioctl (VIDIOCGWIN)" << endl;
	 strncpy(errmsg, "ioctl (VIDIOCGWIN)", 1024);
	 return -1;
      }
      if (-1 == ioctl (fd, VIDIOCGPICT, &picture_format)) {
         cerr << "Error: ioctl (VIDIOCGPICT)" << endl;
	 strncpy(errmsg, "ioctl (VIDIOCGPICT)", 1024);
	 return -1;
      }
      init(0);
   }
   
  // Check to see if it's really a philips webcam       
  if (ioctl(fd, VIDIOCPWCPROBE, &probe) == 0) 
  {
  	    if (!strcmp(capability.name,probe.name))
	    {
	       IsPhilips = true;
	       type_=probe.type;
            }
 }
	 
  if (IsPhilips)
      cerr << "Philips webcam type " << type_ << " detected" << endl;
  else
  {
   strncpy(errmsg, "No Philips webcam detected.", 1024);
   return -1;
  }
    
   cerr << "initial size w:" << window.width << " -- h: " << window.height << endl;
   
   
   mmapInit();
   
   setWhiteBalanceMode(PWC_WB_AUTO, errmsg);
   multiplicateur_=1;
   skippedFrame_=0;
   lastGain_=getGain();

   cerr << "All successful, returning\n";
   return fd;
}

void V4L1_PWC::checkSize(int & x, int & y) 
{
 if (x>=capability.maxwidth && y >= capability.maxheight)
   {
      x=capability.maxwidth;
      y=capability.maxheight;
   } else if (x>=352 && y >=288 && type_<700) {
      x=352;y=288;
   } else if (x>=320 && y >= 240) {
      x=320;y=240;
   } else if (x>=176 && y >=144 && type_<700 ) {
      x=176;y=144;
   } else if (x>=160 && y >=120 ) {
      x=160;y=120;
   } else {
      x=capability.minwidth;
      y=capability.minheight;
   }
}

bool V4L1_PWC::setSize(int x, int y) 
{

   int oldX, oldY;
   char msg[ERRMSG_SIZ];
   checkSize(x,y);
   
   oldX = window.width;
   oldY = window.height;
   
   window.width=x;
   window.height=y;
   
   if (ioctl (fd, VIDIOCSWIN, &window))
   {
       snprintf(msg, ERRMSG_SIZ, "ioctl(VIDIOCSWIN) %s", strerror(errno));
       cerr << msg << endl;
       window.width=oldX;
       window.height=oldY;
       return false;
   }
   ioctl (fd, VIDIOCGWIN, &window);

   cerr << "New size is x=" << window.width << " " << "y=" << window.height <<endl;
   
   allocBuffers();
   
   return true;
}

int V4L1_PWC::saveSettings(char *errmsg)
{
   if (ioctl(fd, VIDIOCPWCSUSER)==-1)
    {
      snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSUSER %s", strerror(errno));
      return -1;
   }
   
   return 0;
}

void V4L1_PWC::restoreSettings()
 {
   ioctl(fd, VIDIOCPWCRUSER);
   getPictureSettings();
}

void V4L1_PWC::restoreFactorySettings()
{
   ioctl(fd, VIDIOCPWCFACTORY);
   getPictureSettings();
}

int V4L1_PWC::setGain(int val, char *errmsg)
 {
   if(-1==ioctl(fd, VIDIOCPWCSAGC, &val))
   {
      snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSAGC %s", strerror(errno));
      return -1;
   }
   else lastGain_=val;
   
   cerr << "setGain "<<val<<endl;
   
   return lastGain_;
}

int V4L1_PWC::getGain()
{
   int gain;
   char msg[ERRMSG_SIZ];
   static int cpt=0;
   if ((cpt%4)==0)
   {
      if (-1==ioctl(fd, VIDIOCPWCGAGC, &gain))
      {
         //perror("VIDIOCPWCGAGC");
	 snprintf(msg, ERRMSG_SIZ, "VIDIOCPWCGAGC %s", strerror(errno));
	 cerr << msg << endl;
         gain=lastGain_;
      } else
      {
         ++cpt;
         lastGain_=gain;
      }
   } else
   {
      ++cpt;
      gain=lastGain_; 
   }
   //cerr << "get gain "<<gain<<endl;
   if (gain < 0) gain*=-1;
   return gain;
}

int V4L1_PWC::setExposure(int val, char *errmsg)
 {
   //cout << "set exposure "<<val<<"\n";
   
   if (-1==ioctl(fd, VIDIOCPWCSSHUTTER, &val))
   {
     snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSSHUTTER %s", strerror(errno));
     return -1;
  }
  
  return 0;
}

void V4L1_PWC::setCompression(int val)
{
   ioctl(fd, VIDIOCPWCSCQUAL, &val); 
}

int V4L1_PWC::getCompression()
{
   int gain;
   ioctl(fd, VIDIOCPWCGCQUAL , &gain);
   if (gain < 0) gain*=-1;
   return gain;
}

int V4L1_PWC::setNoiseRemoval(int val, char *errmsg)
{
   if (-1 == ioctl(fd, VIDIOCPWCSDYNNOISE, &val))
   {
       snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCGDYNNOISE %s", strerror(errno));
       return -1;
   }
   
   return 0;
}

int V4L1_PWC::getNoiseRemoval()
{
   int gain;
   char msg[ERRMSG_SIZ];
   
   if (-1 == ioctl(fd, VIDIOCPWCGDYNNOISE , &gain)) 
   {
   	 snprintf(msg, ERRMSG_SIZ, "VIDIOCPWCGDYNNOISE %s", strerror(errno));
	 cerr << msg << endl;
   }
      
   cout <<"get noise = "<<gain<<endl;
   return gain;
}

int V4L1_PWC::setSharpness(int val, char *errmsg)
 {
   if (-1 == ioctl(fd, VIDIOCPWCSCONTOUR, &val))
   {
       snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSCONTOUR %s", strerror(errno));
       return -1;
  }
  
  return 0;
}

int V4L1_PWC::getSharpness()
{
   int gain;
   char msg[ERRMSG_SIZ];
   
   if (-1 == ioctl(fd, VIDIOCPWCGCONTOUR, &gain)) 
   {
      snprintf(msg, ERRMSG_SIZ, "VIDIOCPWCGCONTOUR %s", strerror(errno));
      cerr << msg << endl;
  }
   
   cout <<"get sharpness = "<<gain<<endl;
   return gain;
}

int V4L1_PWC::setBackLight(bool val, char *errmsg)
{
   static int on=1;
   static int off=0;
   if (-1 == ioctl(fd,VIDIOCPWCSBACKLIGHT, &  val?&on:&off))
   {
        snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSBACKLIGHT %s", strerror(errno));
	return -1;
   }
   
   return 0;
}

bool V4L1_PWC::getBackLight()
{
   int val;
   char msg[ERRMSG_SIZ];
   
   if (-1 == ioctl(fd,VIDIOCPWCGBACKLIGHT, & val)) 
   {
      snprintf(msg, ERRMSG_SIZ, "VIDIOCPWCSBACKLIGHT %s", strerror(errno));
      cerr << msg << endl;
   }

   return val !=0;
}

int V4L1_PWC::setFlicker(bool val, char *errmsg)
{
   static int on=1;
   static int off=0;
   if (-1 == ioctl(fd,VIDIOCPWCSFLICKER, val?&on:&off))
   {
      snprintf(errmsg, ERRMSG_SIZ, "VIDIOCPWCSFLICKER %s", strerror(errno));
      return -1;
   }
   
   return 0;
   
}

bool V4L1_PWC::getFlicker() 
{
   int val;
   char msg[ERRMSG_SIZ];
   
   if (-1 == ioctl(fd,VIDIOCPWCGFLICKER, & val))
   {
         snprintf(msg, ERRMSG_SIZ, "VIDIOCPWCGFLICKER %s", strerror(errno));
	 cerr << msg << endl;
   }
   
   return val !=0;
}

void V4L1_PWC::setGama(int val)
{
   picture_format.whiteness=val;
   setPictureSettings();
}

int V4L1_PWC::getGama() 
{
   return picture_format.whiteness;
}

int V4L1_PWC::setFrameRate(int value, char *errmsg)
 {
   window.flags = (window.flags & ~PWC_FPS_MASK) | ((value << PWC_FPS_SHIFT) & PWC_FPS_MASK);
   if (ioctl(fd, VIDIOCSWIN, &window))
   {
             snprintf(errmsg, ERRMSG_SIZ, "setFrameRate %s", strerror(errno));
	     return -1;
   }
   
   ioctl(fd, VIDIOCGWIN, &window);
   frameRate = value;
   
   return 0;
   //emit exposureTime(multiplicateur_/(double)getFrameRate());
}

int V4L1_PWC::getFrameRate() 
{
   return ((window.flags&PWC_FPS_FRMASK)>>PWC_FPS_SHIFT); 
}

int V4L1_PWC::getWhiteBalance()
{
   char msg[ERRMSG_SIZ];
   struct pwc_whitebalance tmp_whitebalance;
   tmp_whitebalance.mode = tmp_whitebalance.manual_red = tmp_whitebalance.manual_blue = tmp_whitebalance.read_red = tmp_whitebalance.read_blue = PWC_WB_AUTO;
   
   if (ioctl(fd, VIDIOCPWCGAWB, &tmp_whitebalance)) 
   {
           snprintf(msg, ERRMSG_SIZ, "getWhiteBalance %s", strerror(errno));
	   cerr << msg << endl;
   }
   else
   {
#if 0
      cout << "mode="<<tmp_whitebalance.mode
           <<" mr="<<tmp_whitebalance.manual_red
           <<" mb="<<tmp_whitebalance.manual_blue
           <<" ar="<<tmp_whitebalance.read_red
           <<" ab="<<tmp_whitebalance.read_blue
           <<endl;
#endif
      /* manual_red and manual_blue are garbage :-( */
      whiteBalanceMode_=tmp_whitebalance.mode;
   }
            
      return whiteBalanceMode_;
     
      /*switch(whiteBalanceMode_) {
      case PWC_WB_INDOOR:
         setProperty("WhiteBalanceMode","Indor");
         break;
      case PWC_WB_OUTDOOR:
         setProperty("WhiteBalanceMode","Outdoor");
         break;
      case PWC_WB_FL:
         setProperty("WhiteBalanceMode","Neon");
         break;
      case PWC_WB_MANUAL:
          setProperty("WhiteBalanceMode","Manual");
          whiteBalanceRed_=tmp_whitebalance.manual_red;
          whiteBalanceBlue_=tmp_whitebalance.manual_blue;

         break;
      case PWC_WB_AUTO:
         setProperty("WhiteBalanceMode","Auto");
         whiteBalanceRed_=tmp_whitebalance.read_red;
         whiteBalanceBlue_=tmp_whitebalance.read_blue;
         break;
      default:
         setProperty("WhiteBalanceMode","???");
      }
         
      emit whiteBalanceModeChange(whiteBalanceMode_);

      if (((whiteBalanceMode_ == PWC_WB_AUTO) && liveWhiteBalance_)
          || whiteBalanceMode_ != PWC_WB_AUTO) {
         setProperty("WhiteBalanceRed",whiteBalanceRed_);
         emit whiteBalanceRedChange(whiteBalanceRed_);
         setProperty("WhiteBalanceBlue",whiteBalanceBlue_);   
         emit whiteBalanceBlueChange(whiteBalanceBlue_);
         if (guiBuild()) {         
            remoteCTRLWBred_->show();
            remoteCTRLWBblue_->show();
         }
      } else {
         if (guiBuild()) {         
            remoteCTRLWBred_->hide();
            remoteCTRLWBblue_->hide();
         }
      }
   }*/
}

int V4L1_PWC::setWhiteBalance(char *errmsg) 
{
   struct pwc_whitebalance wb;
   wb.mode=whiteBalanceMode_;
   if (wb.mode == PWC_WB_MANUAL)
   {
      wb.manual_red=whiteBalanceRed_;
      wb.manual_blue=whiteBalanceBlue_;
   }
   
   if (ioctl(fd, VIDIOCPWCSAWB, &wb))
   {
       snprintf(errmsg, ERRMSG_SIZ, "setWhiteBalance %s", strerror(errno)); 
       return -1;
   }
   
   return 0;
}

int V4L1_PWC::setWhiteBalanceMode(int val, char *errmsg)
 {
   if (val == whiteBalanceMode_) 
      return whiteBalanceMode_;
      
   if (val != PWC_WB_AUTO)
   {
      if ( val != PWC_WB_MANUAL)
      {
         whiteBalanceMode_=val;
         if (setWhiteBalance(errmsg) < 0)
	   return -1;
      }
      
      //whiteBalanceMode_=PWC_WB_AUTO;
      whiteBalanceMode_= val;
      if (setWhiteBalance(errmsg) < 0)
       return -1;
      getWhiteBalance();
   }

   /*if (guiBuild()) {
      if (val != PWC_WB_AUTO
          || ( liveWhiteBalance_ && (val ==PWC_WB_AUTO))) {
         remoteCTRLWBred_->show();
         remoteCTRLWBblue_->show();
      } else {
         remoteCTRLWBred_->hide();
         remoteCTRLWBblue_->hide();
      }
   }*/
   
   whiteBalanceMode_=val;
   if (setWhiteBalance(errmsg) < 0)
    return -1;
   getWhiteBalance();
   
   return 0;
}

int V4L1_PWC::setWhiteBalanceRed(int val, char *errmsg) 
{
   whiteBalanceMode_ = PWC_WB_MANUAL;
   whiteBalanceRed_=val;
   if (setWhiteBalance(errmsg) < 0)
    return -1;
    
   return 0;
}

int V4L1_PWC::setWhiteBalanceBlue(int val, char *errmsg)
{
   whiteBalanceMode_ = PWC_WB_MANUAL;
   whiteBalanceBlue_=val;
   if (setWhiteBalance(errmsg) < 0)
     return -1;
     
   return 0;
}

