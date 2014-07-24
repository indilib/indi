#if 0
    V4L2 INDI Driver
    INDI Interface for V4L2 devices
    Copyright (C) 2013 Geehalel (geehalel@gmail.com)
    Copyright (C) 2003-2005 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#endif

#ifndef V4L2_DRIVER_H
#define V4L2_DRIVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zlib.h>
#include <asm/types.h>

#include "indidevapi.h"
#include "indicom.h"
#include <fitsio.h>
#include "eventloop.h"


#include <config.h>

#include "webcam/v4l2_base.h"
#include "indiccd.h"

// Long Exposure
#include "lx/Lx.h"

#define IMAGE_CONTROL   "Image Control"
#define IMAGE_GROUP	"V4L2 Control"
#define IMAGE_BOOLEAN	"V4L2 Options"
#define CAPTURE_FORMAT    "Capture Options"

#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define ERRMSGSIZ	1024

#define TEMPFILE_LEN	16

class V4L2_Driver: public INDI::CCD
{
  public:
    V4L2_Driver();
    virtual ~V4L2_Driver();

    /* INDI Functions that must be called from indidrivermain */
    virtual void ISGetProperties (const char *dev);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

    virtual bool initProperties();
    virtual bool updateProperties ();
    virtual void initCamBase();

    static void newFrame(void *p);
    void updateFrame();
    
   protected:

    virtual bool Connect();
    virtual bool Disconnect();

    virtual const char *getDefaultName();
    bool AbortExposure();
    bool UpdateCCDFrame(int x, int y, int w, int h);
    bool UpdateCCDBin(int hor, int ver);

   /* Structs */
   typedef struct {
	int  width;
	int  height;
     //int  expose;
        double  expose;
	unsigned char  *Y;
	unsigned char  *U;
	unsigned char  *V;
	unsigned char  *colorBuffer;
	unsigned char  *compressedFrame;
	unsigned char  *stackedFrame;
	} img_t;


   /* Switches */
    ISwitch StreamS[2];
    ISwitch *CompressS;
    ISwitch ImageTypeS[2];
    ISwitch StackModeS[2];
    
    /* Texts */
    IText PortT[1];
    IText camNameT[1];

    /* Numbers */
    INumber *ExposeTimeN;
    INumber FrameRateN[1];
    INumber *FrameN;
     
    /* BLOBs */
    IBLOBVectorProperty *imageBP;
    IBLOB *imageB;
    
    /* Switch vectors */
    ISwitchVectorProperty StreamSP;				/* Stream switch */
    ISwitchVectorProperty *CompressSP;				/* Compress stream switch */
    ISwitchVectorProperty ImageTypeSP;				/* Color or grey switch */
    ISwitchVectorProperty StackModeSP;				/* StackMode switch */
    ISwitchVectorProperty InputsSP;				/* Select input switch */
    ISwitchVectorProperty CaptureFormatsSP;    			/* Select Capture format switch */
    ISwitchVectorProperty CaptureSizesSP;    			/* Select Capture size switch (Discrete)*/
    ISwitchVectorProperty FrameRatesSP;				/* Select Frame rate (Discrete) */ 
    ISwitchVectorProperty *Options;
    unsigned int v4loptions;
    unsigned int v4ladjustments;
    bool useExtCtrl;

    /* Number vectors */
    INumberVectorProperty *ExposeTimeNP;				/* Exposure */
    INumberVectorProperty CaptureSizesNP;    			/* Select Capture size switch (Step/Continuous)*/
    INumberVectorProperty FrameRateNP;				/* Frame rate (Step/Continuous) */
    INumberVectorProperty *FrameNP;				/* Frame dimenstion */
    INumberVectorProperty ImageAdjustNP;			/* Image controls */

    /* Text vectors */
    ITextVectorProperty PortTP;
    ITextVectorProperty camNameTP;
    
    /* Pointers to optional properties */
    INumber               *AbsExposureN;
    ISwitchVectorProperty *ManualExposureSP;

   /* Initilization functions */
   //virtual void connectCamera(void);
   virtual void getBasicData(void);

   /* Stream/FITS functions */
   void updateStream();
 
   void allocateBuffers();
   void releaseBuffers();

   bool setManualExposure(double duration);
   void binFrame();

   virtual void updateV4L2Controls();
   V4L2_Base *v4l_base;

   char device_name[MAXINDIDEVICE];
   unsigned char *fitsData;		/* Buffer to hold the FITS file */
   int frameCount;			/* For debugging */
   double divider;			/* For limits */
   img_t * V4LFrame;			/* Video frame */

   struct timeval capture_start;		/* To calculate how long a frame take */
   struct timeval capture_end;
   struct timeval exposure_duration;
   unsigned int stackMode;
   ulong frameBytes;
   
   //Long Exposure
   Lx *lx;
   int lxtimer;
   bool startlongexposure(double timeinsec);
   static void lxtimerCallback(void *userpointer);
   short lxstate;
};
   
#endif
