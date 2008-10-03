#if 0
    V4L INDI Driver
    INDI Interface for V4L devices
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

#ifndef V4L_DRIVER_H
#define V4L_DRIVER_H

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

#ifdef HAVE_LINUX_VIDEODEV2_H
#include "webcam/v4l2_base.h"
#else
#include "webcam/v4l1_base.h"
#endif 

#define COMM_GROUP	"Main Control"
#define IMAGE_CONTROL   "Image Control"
#define IMAGE_GROUP	"Image Settings"

#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define ERRMSGSIZ	1024

#define TEMPFILE_LEN	16


class V4L_Driver
{
  public:
    V4L_Driver();
    virtual ~V4L_Driver();

    /* INDI Functions that must be called from indidrivermain */
    virtual void ISGetProperties (const char *dev);
    virtual void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

    virtual void initCamBase();
    virtual void initProperties(const char *dev);

    static void newFrame(void *p);
    void updateFrame();

   protected:

   /* Structs */
   typedef struct {
	int  width;
	int  height;
	int  expose;
	unsigned char  *Y;
	unsigned char  *U;
	unsigned char  *V;
	unsigned char  *colorBuffer;
	unsigned char  *compressedFrame;
	} img_t;


   /* Switches */
    ISwitch PowerS[2];
    ISwitch StreamS[2];
    ISwitch CompressS[2];
    ISwitch ImageTypeS[2];
    
    /* Texts */
    IText PortT[1];
    IText camNameT[1];

    /* Numbers */
    INumber ExposeTimeN[1];
    INumber FrameRateN[1];
    INumber FrameN[4];
    #ifndef HAVE_LINUX_VIDEODEV2_H
    INumber ImageAdjustN[5];
    #endif
    
    /* BLOBs */
    IBLOB imageB;
    
    /* Switch vectors */
    ISwitchVectorProperty PowerSP;				/* Connection switch */
    ISwitchVectorProperty StreamSP;				/* Stream switch */
    ISwitchVectorProperty CompressSP;				/* Compress stream switch */
    ISwitchVectorProperty ImageTypeSP;				/* Color or grey switch */

    /* Number vectors */
    INumberVectorProperty ExposeTimeNP;				/* Exposure */
    INumberVectorProperty FrameRateNP;				/* Frame rate */
    INumberVectorProperty FrameNP;				/* Stream dimenstion */
    INumberVectorProperty ImageAdjustNP;			/* Image controls */

    /* Text vectors */
    ITextVectorProperty PortTP;
    ITextVectorProperty camNameTP;
    
    /* BLOB vectors */
    IBLOBVectorProperty imageBP;				/* Data stream */

   /* Initilization functions */
   virtual void connectCamera(void);
   virtual void getBasicData(void);

   /* Stream/FITS functions */
   void updateStream();
   void uploadFile(const char * filename);
   int  writeFITS(const char *filename, char errmsg[]);
   int  grabImage(void);
   void addFITSKeywords(fitsfile *fptr);
   void allocateBuffers();
   void releaseBuffers();

   /* Helper functions */
   int  checkPowerN(INumberVectorProperty *np);
   int  checkPowerS(ISwitchVectorProperty *sp);
   int  checkPowerT(ITextVectorProperty *tp);

   #ifndef HAVE_LINUX_VIDEODEV2_H
   virtual void updateV4L1Controls();
   V4L1_Base *v4l_base;
   #else
   virtual void updateV4L2Controls();
   V4L2_Base *v4l_base;
   #endif

   char device_name[MAXINDIDEVICE];
   unsigned char *fitsData;		/* Buffer to hold the FITS file */
   int frameCount;			/* For debugging */
   double divider;			/* For limits */
   img_t * V4LFrame;			/* Video frame */

   time_t capture_start;		/* To calculate how long a frame take */
   time_t capture_end;

};
   
#endif
