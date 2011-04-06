#if 0
    FLI CCD
    INDI Interface for Finger Lakes Instruments CCDs
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zlib.h>

#include <fitsio.h>

#include <libfli.h>
#include "indidevapi.h"
#include "eventloop.h"
#include "indicom.h"

void ISInit(void);
void getBasicData(void);
void ISPoll(void *);
void handleExposure(void *);
void connectCCD(void);
void getBasicData(void);
void uploadFile(const char* filename);
int  writeFITS(const char* filename, char errmsg[]);
int  findcam(flidomain_t domain);
int  setImageArea(char errmsg[]);
int  manageDefaults(char errmsg[]);
int  grabImage(void);
int  checkPowerS(ISwitchVectorProperty *sp);
int  checkPowerN(INumberVectorProperty *np);
int  checkPowerT(ITextVectorProperty *tp);
int  getOnSwitch(ISwitchVectorProperty *sp);
int  isCCDConnected(void);
void addFITSKeywords(fitsfile *fptr);

double min(void);
double max(void);

extern char* me;
extern int errno;

#define mydev           "FLI CCD"

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"
#define DATA_GROUP      "Data Channel"

#define MAX_CCD_TEMP	45		/* Max CCD temperature */
#define MIN_CCD_TEMP	-55		/* Min CCD temperature */
#define MAX_X_BIN	16.		/* Max Horizontal binning */
#define MAX_Y_BIN	16.		/* Max Vertical binning */
#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define POLLMS		1000		/* Polling time (ms) */
#define TEMP_THRESHOLD  .25		/* Differential temperature threshold (C)*/
#define NFLUSHES	1		/* Number of times a CCD array is flushed before an exposure */

#define FILENAMESIZ	2048
#define LIBVERSIZ 	1024
#define PREFIXSIZ	64
#define PIPEBUFSIZ	8192
#define FRAME_ILEN	64
#define TEMPFILE_LEN	16

enum FLIFrames { LIGHT_FRAME = 0, BIAS_FRAME, DARK_FRAME, FLAT_FRAME };


typedef struct {
  flidomain_t domain;
  char *dname;
  char *name;
  char *model;
  long HWRevision;
  long FWRevision;
  double x_pixel_size;
  double y_pixel_size;
  long Array_Area[4];
  long Visible_Area[4];
  int width, height;
  double temperature;
} cam_t;

typedef struct {
int  width;
int  height;
int  frameType;
int  expose;
unsigned short  *img;
} img_t;

/*static int streamTimerID;		 Stream ID */

static flidev_t fli_dev;
static cam_t *FLICam;
static img_t *FLIImg;
static int portSwitchIndex;

long int Domains[] = { FLIDOMAIN_USB, FLIDOMAIN_SERIAL, FLIDOMAIN_PARALLEL_PORT,  FLIDOMAIN_INET };

/*INDI controls */

/* Connect/Disconnect */
static ISwitch ConnectS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty ConnectSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, ConnectS, NARRAY(ConnectS), "", 0};

/* Types of Ports */
static ISwitch PortS[]           	= {{"USB", "", ISS_ON, 0, 0}, {"Serial", "", ISS_OFF, 0, 0}, {"Parallel", "", ISS_OFF, 0, 0}, {"INet", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty PortSP	= { mydev, "Port Type", "", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, PortS, NARRAY(PortS), "", 0};

/* Types of Frames */
static ISwitch FrameTypeS[]		= { {"FRAME_LIGHT", "Light", ISS_ON, 0, 0}, {"FRAME_BIAS", "Bias", ISS_OFF, 0, 0}, {"FRAME_DARK", "Dark", ISS_OFF, 0, 0}, {"FRAME_FLAT", "Flat Field", ISS_OFF, 0, 0}};
static ISwitchVectorProperty FrameTypeSP = { mydev, "CCD_FRAME_TYPE", "Frame Type", EXPOSE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, FrameTypeS, NARRAY(FrameTypeS), "", 0};

/* Frame coordinates. Full frame is default */
static INumber FrameN[]          	= {
 { "X", "X", "%.0f", 0.,     MAX_PIXELS, 1., 0., 0, 0, 0},
 { "Y", "Y", "%.0f", 0.,     MAX_PIXELS, 1., 0., 0, 0, 0},
 { "WIDTH", "Width", "%.0f", 0., MAX_PIXELS, 1., 0., 0, 0, 0},
 { "HEIGHT", "Height", "%.0f",0., MAX_PIXELS, 1., 0., 0, 0, 0}};
 static INumberVectorProperty FrameNP = { mydev, "CCD_FRAME", "Frame", IMAGE_GROUP, IP_RW, 60, IPS_IDLE, FrameN, NARRAY(FrameN), "", 0};
 
 /* Binning */ 
 static INumber BinningN[]       = {
 { "HOR_BIN", "X", "%0.f", 1., MAX_X_BIN, 1., 1., 0, 0, 0},
 { "VER_BIN", "Y", "%0.f", 1., MAX_Y_BIN, 1., 1., 0, 0, 0}};
 static INumberVectorProperty BinningNP = { mydev, "CCD_BINNING", "Binning", IMAGE_GROUP, IP_RW, 60, IPS_IDLE, BinningN, NARRAY(BinningN), "", 0};
 
 /* Exposure time */
  static INumber ExposeTimeWN[]    = {{ "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0., 36000., .5, 1., 0, 0, 0}};
  static INumberVectorProperty ExposeTimeWNP = { mydev, "CCD_EXPOSURE_REQUEST", "Expose", EXPOSE_GROUP, IP_WO, 36000, IPS_IDLE, ExposeTimeWN, NARRAY(ExposeTimeWN), "", 0};

  static INumber ExposeTimeRN[]    = {{ "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0., 36000., .5, 1., 0, 0, 0}};
  static INumberVectorProperty ExposeTimeRNP = { mydev, "CCD_EXPOSURE", "Expose", EXPOSE_GROUP, IP_RO, 36000, IPS_IDLE, ExposeTimeRN, NARRAY(ExposeTimeRN), "", 0};
 
  /* Temperature control */
 static INumber TemperatureN[]	  = { {"CCD_TEMPERATURE_VALUE", "Temperature", "%+06.2f", MIN_CCD_TEMP, MAX_CCD_TEMP, .2, 0., 0, 0, 0}};
 static INumberVectorProperty TemperatureNP = { mydev, "CCD_TEMPERATURE", "Temperature (C)", EXPOSE_GROUP, IP_RW, 60, IPS_IDLE, TemperatureN, NARRAY(TemperatureN), "", 0};
 
  /* Pixel size (µm) */
static INumber PixelSizeN[] 	= {
	{ "Width", "", "%.0f", 0. , 0., 0., 0., 0, 0, 0},
	{ "Height", "", "%.0f", 0. , 0., 0., 0., 0, 0, 0}};
static INumberVectorProperty PixelSizeNP = { mydev, "Pixel Size (µm)", "", DATA_GROUP, IP_RO, 0, IPS_IDLE, PixelSizeN, NARRAY(PixelSizeN), "", 0};

/* BLOB for sending image */
static IBLOB imageB = {"FITS_BLOB", "FITS", "", 0, 0, 0, 0, 0, 0, 0};
static IBLOBVectorProperty imageBP = {mydev, "CCD_FITS_BLOB", "BLOB", COMM_GROUP,
  IP_RO, 0, IPS_IDLE, &imageB, 1, "", 0};

/* send client definitions of all properties */
void ISInit()
{
  static int isInit=0;

 if (isInit)
  return;
 
 /* USB by default {USB, SERIAL, PARALLEL, INET} */
 portSwitchIndex = 0;
 
 FLIImg = malloc (sizeof(img_t));
 
 if (FLIImg == NULL)
 {
   IDMessage(mydev, "Error: unable to initialize driver. Low memory.");
   IDLog("Error: unable to initialize driver. Low memory.");
   return;
 }
 
 IEAddTimer (POLLMS, ISPoll, NULL);

 isInit = 1;
 
}

void ISGetProperties (const char *dev)
{ 

   ISInit();
  
  if (dev && strcmp (mydev, dev))
    return;

  /* COMM_GROUP */
  IDDefSwitch(&ConnectSP, NULL);
  IDDefSwitch(&PortSP, NULL);
  IDDefBLOB(&imageBP, NULL);
  
  /* Expose */
  IDDefSwitch(&FrameTypeSP, NULL);  
  IDDefNumber(&ExposeTimeWNP, NULL);
  IDDefNumber(&ExposeTimeRNP, NULL);
  IDDefNumber(&TemperatureNP, NULL);
  
  /* Image Group */
  IDDefNumber(&FrameNP, NULL);
  IDDefNumber(&BinningNP, NULL);
  
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {}
void ISSnoopDevice (XMLEle *root) {}
  
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	long err;
	int i;
	ISwitch *sp;
	
	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;
	    
	ISInit();

	/* Port type */
	if (!strcmp (name, PortSP.name))
	{
   	  PortSP.s = IPS_IDLE; 
	  IUResetSwitch(&PortSP);
	  IUUpdateSwitch(&PortSP, states, names, n);
	  portSwitchIndex = getOnSwitch(&PortSP);
	  
	  PortSP.s = IPS_OK; 
	  IDSetSwitch(&PortSP, NULL);
	  return;
	}
	
	/* Connection */
	if (!strcmp (name, ConnectSP.name))
	{
	  if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0)
		return;
   	  connectCCD();
	  return;
	}
	
     /* Frame Type */
     if (!strcmp(FrameTypeSP.name, name))
     {
       if (checkPowerS(&FrameTypeSP))
         return;
	 
       for (i = 0; i < n ; i++)
       {
         sp = IUFindSwitch(&FrameTypeSP, names[i]);
	 
	 if (!sp)
	 {
	      FrameTypeSP.s = IPS_ALERT;
	      IDSetSwitch(&FrameTypeSP, "Unknown error. %s is not a member of %s property.", names[0], name);
	      return;
	 }
	 
	 /* NORMAL, BIAS, or FLAT */
	 if ( (sp == &FrameTypeS[LIGHT_FRAME] || sp == &FrameTypeS[FLAT_FRAME]) && states[i] == ISS_ON)
	 {
	   if (sp == &FrameTypeS[LIGHT_FRAME])
	     FLIImg->frameType = LIGHT_FRAME;
  	   else
	     FLIImg->frameType = FLAT_FRAME;
	   
	   if ((err = FLISetFrameType(fli_dev, FLI_FRAME_TYPE_NORMAL) ))
  	   {
	    IUResetSwitch(&FrameTypeSP);
	    FrameTypeS[LIGHT_FRAME].s = ISS_ON;
	    FrameTypeSP.s = IPS_ALERT;
            IDSetSwitch(&FrameTypeSP, "FLISetFrameType() failed. %s.\n", strerror((int)-err));
	    IDLog("FLISetFrameType() failed. %s.\n", strerror((int)-err));
	    return;
	   }
	  
	   IUResetSwitch(&FrameTypeSP);
	   sp->s = ISS_ON; 
	   FrameTypeSP.s = IPS_OK;
	   IDSetSwitch(&FrameTypeSP, NULL);
	   break;
	 }
	 /* DARK AND BIAS */
	 else if ( (sp == &FrameTypeS[DARK_FRAME] || sp == &FrameTypeS[BIAS_FRAME]) && states[i] == ISS_ON)
	 {
	   
	  if (sp == &FrameTypeS[DARK_FRAME])
	     FLIImg->frameType = DARK_FRAME;
	  else
	     FLIImg->frameType = BIAS_FRAME;
	   
	   if ((err = FLISetFrameType(fli_dev, FLI_FRAME_TYPE_DARK) ))
  	   {
	    IUResetSwitch(&FrameTypeSP);
	    FrameTypeS[LIGHT_FRAME].s = ISS_ON;
	    FrameTypeSP.s = IPS_ALERT;
            IDSetSwitch(&FrameTypeSP, "FLISetFrameType() failed. %s.\n", strerror((int)-err));
	    IDLog("FLISetFrameType() failed. %s.\n", strerror((int)-err));
	    return;
	   }
	   
	   IUResetSwitch(&FrameTypeSP);
	   sp->s = ISS_ON;
	   FrameTypeSP.s = IPS_OK;
	   IDSetSwitch(&FrameTypeSP, NULL);
	   break;
	 }
	   
  	} /* For loop */
	
	return;
     }
     
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	ISInit();
 
       /* ignore if not ours */ 
       if (dev && strcmp (mydev, dev))
         return;

	/* suppress warning */
	n=n; dev=dev; name=name; names=names; texts=texts;
	
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
      long err;
      int i;
      INumber *np;
      char errmsg[ERRMSG_SIZE];

	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;
	    
	ISInit();
	
    /* Exposure time */
    if (!strcmp (ExposeTimeWNP.name, name))
    {
       if (checkPowerN(&ExposeTimeWNP))
         return;

       if (ExposeTimeWNP.s == IPS_BUSY)
       {
          if ( (err = FLICancelExposure(fli_dev)))
	  {
	    	ExposeTimeWNP.s = IPS_ALERT;
	    	IDSetNumber(&ExposeTimeWNP, "FLICancelExposure() failed. %s.", strerror((int)-err));
	    	IDLog("FLICancelExposure() failed. %s.\n", strerror((int)-err));
	    	return;
	  }
	
	  ExposeTimeWNP.s = IPS_IDLE;
	  ExposeTimeRNP.s = IPS_IDLE;
	  ExposeTimeRN[0].value = 0;

	  IDSetNumber(&ExposeTimeWNP, "Exposure cancelled.");
	  IDSetNumber(&ExposeTimeRNP, NULL);
	  IDLog("Exposure Cancelled.\n");
	  return;
        }
    
       np = IUFindNumber(&ExposeTimeWNP, names[0]);
	 
       if (!np)
       {
	   ExposeTimeWNP.s = IPS_ALERT;
	   IDSetNumber(&ExposeTimeWNP, "Error: %s is not a member of %s property.", names[0], name);
	   return;
       }
	 
        np->value = values[0];
	FLIImg->expose = (int) (values[0] * 1000.);
	
      /* Set duration */  
     if ( (err = FLISetExposureTime(fli_dev, np->value * 1000.) ))
     {
       ExposeTimeWNP.s = IPS_ALERT;
       IDSetNumber(&ExposeTimeWNP, "FLISetExposureTime() failed. %s.\n", strerror((int)-err));
       IDLog("FLISetExposureTime() failed. %s.\n", strerror((int)-err));
       return;
     }
      
      IDLog("Exposure Time (ms) is: %g\n", np->value * 1000.);
      
      handleExposure(NULL);
      return;
    } 
    
    
  if (!strcmp(TemperatureNP.name, name))
  {
    if (checkPowerN(&TemperatureNP))
      return;
      
    TemperatureNP.s = IPS_IDLE;
    
    np = IUFindNumber(&TemperatureNP, names[0]);
    
    if (!np)
    {
    IDSetNumber(&TemperatureNP, "Unknown error. %s is not a member of %s property.", names[0], name);
    return;
    }
    
    if (values[0] < MIN_CCD_TEMP || values[0] > MAX_CCD_TEMP)
    {
      IDSetNumber(&TemperatureNP, "Error: valid range of temperature is from %d to %d", MIN_CCD_TEMP, MAX_CCD_TEMP);
      return;
    }
    
    if ( (err = FLISetTemperature(fli_dev, values[0])))
    {
      IDSetNumber(&TemperatureNP, "FLISetTemperature() failed. %s.", strerror((int)-err));
      IDLog("FLISetTemperature() failed. %s.", strerror((int)-err));
      return;
    }
    
    FLICam->temperature = values[0];
    TemperatureNP.s = IPS_BUSY;
    
    IDSetNumber(&TemperatureNP, "Setting CCD temperature to %+06.2f C", values[0]);
    IDLog("Setting CCD temperature to %+06.2f C\n", values[0]);
    return;
   }
   
   if (!strcmp(FrameNP.name, name))
   {
     int nset=0;
     
     if (checkPowerN(&FrameNP))
      return;
      
     FrameNP.s = IPS_IDLE;
     
     for (i=0; i < n ; i++)
     {
       np = IUFindNumber(&FrameNP, names[i]);
       
      if (!np)
      {
       IDSetNumber(&FrameNP, "Unknown error. %s is not a member of %s property.", names[0], name);
       return;
      }
      
      /* X or Width */
      if (np == &FrameN[0] || np==&FrameN[2])
      {
        if (values[i] < 0 || values[i] > FLICam->width)
	 break;
	 
	nset++;
	np->value = values[i];
      }
      /* Y or height */
      else if (np == &FrameN[1] || np==&FrameN[3])
      {
       if (values[i] < 0 || values[i] > FLICam->height)
	 break;
	 
	nset++;
	np->value = values[i];
      }
     }
      
      if (nset < 4)
      {
        IDSetNumber(&FrameNP, "Invalid range. Valid range is (0,0) - (%0d,%0d)", FLICam->width, FLICam->height);
	IDLog("Invalid range. Valid range is (0,0) - (%0d,%0d)", FLICam->width, FLICam->height);
	return; 
      }
      
      if (setImageArea(errmsg))
      {
        IDSetNumber(&FrameNP, "%s", errmsg);
	return;
      }
      	    
      FrameNP.s = IPS_OK;
      
      /* Adjusting image width and height */ 
      FLIImg->width  = FrameN[2].value;
      FLIImg->height = FrameN[3].value;
      
      IDSetNumber(&FrameNP, NULL);
      
   } /* end FrameNP */
      
    
   if (!strcmp(BinningNP.name, name))
   {
     if (checkPowerN(&BinningNP))
       return;
       
     BinningNP.s = IPS_IDLE;
     
     for (i=0 ; i < n ; i++)
     {
       np = IUFindNumber(&BinningNP, names[i]);
       
      if (!np)
      {
       IDSetNumber(&BinningNP, "Unknown error. %s is not a member of %s property.", names[0], name);
       return;
      }
      
      /* X binning */
      if (np == &BinningN[0])
      {
        if (values[i] < 1 || values[i] > MAX_X_BIN)
	{
	  IDSetNumber(&BinningNP, "Error: Valid X bin values are from 1 to %g", MAX_X_BIN);
	  IDLog("Error: Valid X bin values are from 1 to %g", MAX_X_BIN);
	  return;
	}
	
	if ( (err = FLISetHBin(fli_dev, values[i])))
	{
	  IDSetNumber(&BinningNP, "FLISetHBin() failed. %s.", strerror((int)-err));
	  IDLog("FLISetHBin() failed. %s.", strerror((int)-err));
	  return;
	}
	
	np->value = values[i];
      }
      else if (np == &BinningN[1])
      {
        if (values[i] < 1 || values[i] > MAX_Y_BIN)
	{
	  IDSetNumber(&BinningNP, "Error: Valid Y bin values are from 1 to %g", MAX_Y_BIN);
	  IDLog("Error: Valid X bin values are from 1 to %g", MAX_Y_BIN);
	  return;
	}
	
	if ( (err = FLISetVBin(fli_dev, values[i])))
	{
	  IDSetNumber(&BinningNP, "FLISetVBin() failed. %s.", strerror((int)-err));
	  IDLog("FLISetVBin() failed. %s.", strerror((int)-err));
	  return;
	}
	
	np->value = values[i];
      }
     } /* end for */
     
     if (setImageArea(errmsg))
     {
       IDSetNumber(&BinningNP, errmsg, NULL);
       IDLog("%s", errmsg);
       return;
     }
     
     BinningNP.s = IPS_OK;
     
     IDLog("Binning is: %.0f x %.0f\n", BinningN[0].value, BinningN[1].value);
     
     IDSetNumber(&BinningNP, NULL);
     return;
   }
	
}


void ISPoll(void *p)
{
	long err;
	long timeleft;
	double ccdTemp;
	
	if (!isCCDConnected())
	{
	 IEAddTimer (POLLMS, ISPoll, NULL);
	 return;
	}
	 
        /*IDLog("In Poll.\n");*/
	
	switch (ExposeTimeWNP.s)
	{
	  case IPS_IDLE:
	    break;
	    
	  case IPS_OK:
	    break;
	    
	  case IPS_BUSY:
	    if ( (err = FLIGetExposureStatus(fli_dev, &timeleft)))
	    { 
	      ExposeTimeWNP.s = IPS_ALERT; 
	      ExposeTimeRN[0].value = 0;
	      
	      IDSetNumber(&ExposeTimeWNP, "FLIGetExposureStatus() failed. %s.", strerror((int)-err));
	      IDSetNumber(&ExposeTimeRNP, NULL);
	      IDLog("FLIGetExposureStatus() failed. %s.\n", strerror((int)-err));
	      break;
	    }
	    
	    if (timeleft > 0)
	    {
	      ExposeTimeRN[0].value = timeleft / 1000.;
	      IDSetNumber(&ExposeTimeRNP, NULL); 
	      break;
	    }

	    /* We're done exposing */
	    /*ExposeTimeWNP.s = IPS_OK;*/
	    ExposeTimeRNP.s = IPS_OK;  
	    ExposeTimeRN[0].value = 0;
	    IDSetNumber(&ExposeTimeWNP, "Exposure done, downloading image...");
	    IDSetNumber(&ExposeTimeRNP, NULL);
	    IDLog("Exposure done, downloading image...\n");

	    /* grab and save image */
	     grabImage();

	    break;
	    
	  case IPS_ALERT:
	    break;
	 }
	 
	 switch (TemperatureNP.s)
	 {
	   case IPS_IDLE:
	   case IPS_OK:
	     if ( (err = FLIGetTemperature(fli_dev, &ccdTemp)))
	     {
	       TemperatureNP.s = IPS_IDLE;
	       IDSetNumber(&TemperatureNP, "FLIGetTemperature() failed. %s.", strerror((int)-err));
	       IDLog("FLIGetTemperature() failed. %s.", strerror((int)-err));
	       return;
	     }
	     
	     if (fabs(TemperatureN[0].value - ccdTemp) >= TEMP_THRESHOLD)
	     {
	       TemperatureN[0].value = ccdTemp;
	       IDSetNumber(&TemperatureNP, NULL);
	     }
	     break;
	     
	   case IPS_BUSY:
	   if ((err = FLIGetTemperature(fli_dev, &ccdTemp)))
	     {
	       TemperatureNP.s = IPS_ALERT;
	       IDSetNumber(&TemperatureNP, "FLIGetTemperature() failed. %s.", strerror((int)-err));
	       IDLog("FLIGetTemperature() failed. %s.", strerror((int)-err));
	       return;
	     }
	     
	     if (fabs(FLICam->temperature - ccdTemp) <= TEMP_THRESHOLD)
	       TemperatureNP.s = IPS_OK;
	     	       
              TemperatureN[0].value = ccdTemp;
	      IDSetNumber(&TemperatureNP, NULL);
	      break;
	      
	    case IPS_ALERT:
	     break;
	  }
  	 
	 p=p; 
 	
	 IEAddTimer (POLLMS, ISPoll, NULL);
}

/* Sets the Image area that the CCD will scan and download.
   We compensate for binning. */
int setImageArea(char errmsg[])
{
  
   long x_1, y_1, x_2, y_2;
   long err;
   
   /* Add the X and Y offsets */
   x_1 = FrameN[0].value + FLICam->Visible_Area[0];
   y_1 = FrameN[1].value + FLICam->Visible_Area[1];
   
   x_2 = x_1 + (FrameN[2].value / BinningN[0].value);
   y_2 = y_1 + (FrameN[3].value / BinningN[1].value);
   
   if (x_2 > FLICam->Visible_Area[2])
     x_2 = FLICam->Visible_Area[2];
   
   if (y_2 > FLICam->Visible_Area[3])
     y_2 = FLICam->Visible_Area[3];
     
   IDLog("The Final image area is (%ld, %ld), (%ld, %ld)\n", x_1, y_1, x_2, y_2);
   
   FLIImg->width  = x_2 - x_1;
   FLIImg->height = y_2 - y_1;
   
   if ( (err = FLISetImageArea(fli_dev, x_1, y_1, x_2, y_2) ))
   {
     snprintf(errmsg, ERRMSG_SIZE, "FLISetImageArea() failed. %s.\n", strerror((int)-err));
     IDLog("%s", errmsg);
     return -1;
   }
   
   return 0;
}

/* Downloads the image from the CCD row by row and store them
   in a raw file.
 N.B. No processing is done on the image */
int grabImage()
{
	long err;
	int img_size,i, fd;
	char errmsg[ERRMSG_SIZE];
	char filename[TEMPFILE_LEN] = "/tmp/fitsXXXXXX";
  
	if ((fd = mkstemp(filename)) < 0)
	{ 
		IDMessage(mydev, "Error making temporary filename.");
		IDLog("Error making temporary filename.\n");
		return -1;
	}
	close(fd);
     
	img_size = FLIImg->width * FLIImg->height * sizeof(unsigned short);
	
	FLIImg->img = malloc (img_size);
  
	if (FLIImg->img == NULL)
	{
		IDMessage(mydev, "Not enough memory to store image.");
		IDLog("Not enough memory to store image.\n");
		return -1;
	}
  
	for (i=0; i < FLIImg->height ; i++)
	{
		if ( (err = FLIGrabRow(fli_dev, &FLIImg->img[i * FLIImg->width], FLIImg->width)))
		{
			free(FLIImg->img);
			IDMessage(mydev, "FLIGrabRow() failed at row %d. %s.", i, strerror((int)-err));
			IDLog("FLIGrabRow() failed at row %d. %s.\n", i, strerror((int)-err));
			return -1;
		}
	}
  
	IDMessage(mydev, "Download complete.\n");
	
	/*err = (ImageFormatS[0].s == ISS_ON) ? writeFITS(FileNameT[0].text, errmsg) : writeRAW(FileNameT[0].text, errmsg);*/
	err = writeFITS(filename, errmsg);
   
	if (err)
	{
		free(FLIImg->img);
		IDMessage(mydev, errmsg, NULL);
		return -1;
	}

	free(FLIImg->img);
	return 0;
 
}

int writeFITS(const char* filename, char errmsg[])
{
  fitsfile *fptr;       /* pointer to the FITS file; defined in fitsio.h */
  int status;
  long  fpixel = 1, naxis = 2, nelements;
  long naxes[2];
  char filename_rw[TEMPFILE_LEN+1];

  naxes[0] = FLIImg->width;
  naxes[1] = FLIImg->height;

  /* Append ! to file name to over write it.*/
  snprintf(filename_rw, TEMPFILE_LEN+1, "!%s", filename);

  status = 0;         /* initialize status before calling fitsio routines */
  fits_create_file(&fptr, filename_rw, &status);   /* create new file */

  /* Create the primary array image (16-bit short integer pixels */
  fits_create_img(fptr, USHORT_IMG, naxis, naxes, &status);

  addFITSKeywords(fptr);

  nelements = naxes[0] * naxes[1];          /* number of pixels to write */

  /* Write the array of integers to the image */
  fits_write_img(fptr, TUSHORT, fpixel, nelements, FLIImg->img, &status);

  fits_close_file(fptr, &status);            /* close the file */

  fits_report_error(stderr, status);  /* print out any error messages */

  /* Success */
  ExposeTimeWNP.s = IPS_OK;
  IDSetNumber(&ExposeTimeWNP, NULL);
  uploadFile(filename);
  unlink(filename);
 
  return status;
}

void addFITSKeywords(fitsfile *fptr)
{
  int status=0; 
  char binning_s[32];
  char frame_s[32];
  double min_val, max_val;
  
  /*pixel_size = (float) PixelSizeN[0].value;
  min_val = min();
  max_val = max();
  temp    = (float) TemperatureN[0].value;
  expose  = (float) FLIImg->expose;*/

  snprintf(binning_s, 32, "(%g x %g)", BinningN[0].value, BinningN[1].value);

  switch (FLIImg->frameType)
  {
    case LIGHT_FRAME:
      	strcpy(frame_s, "Light");
	break;
    case BIAS_FRAME:
        strcpy(frame_s, "Bias");
	break;
    case FLAT_FRAME:
        strcpy(frame_s, "Flat Field");
	break;
    case DARK_FRAME:
        strcpy(frame_s, "Dark");
	break;
  }

  fits_update_key(fptr, TDOUBLE, "CCD-TEMP", &(TemperatureN[0].value), "CCD Temperature (Celcius)", &status);
  fits_update_key(fptr, TDOUBLE, "EXPOSURE", &(FLIImg->expose), "Total Exposure Time (ms)", &status); 
  fits_update_key(fptr, TDOUBLE, "PIX-SIZ", &(PixelSizeN[0].value), "Pixel Size (microns)", &status);
  fits_update_key(fptr, TSTRING, "BINNING", binning_s, "Binning HOR x VER", &status);
  fits_update_key(fptr, TSTRING, "FRAME", frame_s, "Frame Type", &status);
  fits_update_key(fptr, TDOUBLE, "DATAMIN", &min_val, "Minimum value", &status);
  fits_update_key(fptr, TDOUBLE, "DATAMAX", &max_val, "Maximum value", &status);
  fits_update_key(fptr, TSTRING, "INSTRUME", "Finger Lakes Instruments", "CCD Name", &status);
  fits_write_date(fptr, &status);

}

void uploadFile(const char* filename)
{
   FILE * fitsFile;
   unsigned char *fitsData, *compressedData;
   int r=0;
   unsigned int i =0, nr = 0;
   uLongf compressedBytes=0;
   uLong  totalBytes;
   struct stat stat_p; 
 
   if ( -1 ==  stat (filename, &stat_p))
   { 
     IDLog(" Error occurred attempting to stat file.\n"); 
     return; 
   }
   
   totalBytes     = stat_p.st_size;
   fitsData       = (unsigned char *) malloc (sizeof(unsigned char) * totalBytes);
   compressedData = (unsigned char *) malloc (sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   
   if (fitsData == NULL || compressedData == NULL)
   {
     if (fitsData) free(fitsData);
     if (compressedData) free(compressedData);
     IDLog("Error! low memory. Unable to initialize fits buffers.\n");
     return;
   }
   
   fitsFile = fopen(filename, "r");
   
   if (fitsFile == NULL)
    return;
   
   /* #1 Read file from disk */ 
   for (i=0; i < totalBytes; i+= nr)
   {
      nr = fread(fitsData + i, 1, totalBytes - i, fitsFile);
     
     if (nr <= 0)
     {
        IDLog("Error reading temporary FITS file.\n");
        return;
     }
   }
   fclose(fitsFile);
   
   compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
    
   /* #2 Compress it */ 
   r = compress2(compressedData, &compressedBytes, fitsData, totalBytes, 9);
   if (r != Z_OK)
   {
 	/* this should NEVER happen */
 	IDLog("internal error - compression failed: %d\n", r);
	return;
   }
   
   /* #3 Send it */
   imageB.blob = compressedData;
   imageB.bloblen = compressedBytes;
   imageB.size = totalBytes;
   strcpy(imageB.format, ".fits.z");
   imageBP.s = IPS_OK;
   IDSetBLOB (&imageBP, NULL);
   
   free (fitsData);   
   free (compressedData);
   
}

/* Initiates the exposure procedure */
void handleExposure(void *p)
{
  long err;
  
  /* no warning */
  p=p;
  
  /* BIAS frame is the same as DARK but with minimum period. i.e. readout from camera electronics.
  */
  if (FLIImg->frameType == BIAS_FRAME)
  {
     if ((err = FLISetExposureTime(fli_dev, 50)))
     {
       ExposeTimeWNP.s = IPS_ALERT;
       IDSetNumber(&ExposeTimeWNP, "FLISetExposureTime() failed. %s.\n", strerror((int)-err));
       IDLog("FLISetExposureTime() failed. %s.\n", strerror((int)-err));
       return;
     }
   }
    
  if ((err = FLIExposeFrame(fli_dev)))
  {
    	ExposeTimeWNP.s = IPS_ALERT;
    	IDSetNumber(&ExposeTimeWNP, "FLIExposeFrame() failed. %s.", strerror((int)-err));
    	IDLog("FLIExposeFrame() failed. %s.\n", strerror((int)-err));
    	return;
  }
 
   ExposeTimeWNP.s = IPS_BUSY;
		  
   IDSetNumber(&ExposeTimeWNP, "Taking a %g seconds frame...", FLIImg->expose / 1000.);
   
   IDLog("Taking a frame...\n");
}

/* Retrieves basic data from the CCD upon connection like temperature, array size, firmware..etc */
void getBasicData()
{

  char buff[2048];
  long err;

  IDLog("In getBasicData()\n");
  
  if ((err = FLIGetModel (fli_dev, buff, 2048)))
  {
    IDMessage(mydev, "FLIGetModel() failed. %s.", strerror((int)-err));
    IDLog("FLIGetModel() failed. %s.\n", strerror((int)-err));
    return;
  }
  else
  {
    if ( (FLICam->model = malloc (sizeof(char) * 2048)) == NULL)
    {
      IDMessage(mydev, "malloc() failed.");
      IDLog("malloc() failed.");
      return;
    }
    
    strcpy(FLICam->model, buff);
  }
  
  if (( err = FLIGetHWRevision(fli_dev, &FLICam->HWRevision)))
  {
    IDMessage(mydev, "FLIGetHWRevision() failed. %s.", strerror((int)-err));
    IDLog("FLIGetHWRevision() failed. %s.\n", strerror((int)-err));
    
    return;
  }
  
  if (( err = FLIGetFWRevision(fli_dev, &FLICam->FWRevision)))
  {
    IDMessage(mydev, "FLIGetFWRevision() failed. %s.", strerror((int)-err));
    IDLog("FLIGetFWRevision() failed. %s.\n", strerror((int)-err));
    return;
  }

  if (( err = FLIGetPixelSize(fli_dev, &FLICam->x_pixel_size, &FLICam->y_pixel_size)))
  {
    IDMessage(mydev, "FLIGetPixelSize() failed. %s.", strerror((int)-err));
    IDLog("FLIGetPixelSize() failed. %s.\n", strerror((int)-err));
    return;
  }
  
  FLICam->x_pixel_size *= 1e6;
  FLICam->y_pixel_size *= 1e6; 
  
  if (( err = FLIGetArrayArea(fli_dev, &FLICam->Array_Area[0], &FLICam->Array_Area[1], &FLICam->Array_Area[2], &FLICam->Array_Area[3])))
  {
    IDMessage(mydev, "FLIGetArrayArea() failed. %s.", strerror((int)-err));
    IDLog("FLIGetArrayArea() failed. %s.\n", strerror((int)-err));
    return;
  }
  
  if (( err = FLIGetVisibleArea( fli_dev, &FLICam->Visible_Area[0], &FLICam->Visible_Area[1], &FLICam->Visible_Area[2], &FLICam->Visible_Area[3])))
  {
    IDMessage(mydev, "FLIGetVisibleArea() failed. %s.", strerror((int)-err));
    IDLog("FLIGetVisibleArea() failed. %s.\n", strerror((int)-err));
  }
  
  if (( err = FLIGetTemperature(fli_dev, &FLICam->temperature)))
  {
    IDMessage(mydev, "FLIGetTemperature() failed. %s.", strerror((int)-err));
    IDLog("FLIGetTemperature() failed. %s.\n", strerror((int)-err));
    return;
  }
  
  IDLog("The CCD Temperature is %f.\n", FLICam->temperature);
  
  PixelSizeN[0].value  = FLICam->x_pixel_size;				/* Pixel width (um) */
  PixelSizeN[1].value  = FLICam->y_pixel_size;				/* Pixel height (um) */
  TemperatureN[0].value = FLICam->temperature;				/* CCD chip temperatre (degrees C) */
  FrameN[0].value = 0;							/* X */
  FrameN[1].value = 0;							/* Y */
  FrameN[2].value = FLICam->Visible_Area[2] - FLICam->Visible_Area[0];	/* Frame Width */
  FrameN[3].value = FLICam->Visible_Area[3] - FLICam->Visible_Area[1];	/* Frame Height */
  
  FLICam->width  = FLIImg->width = FrameN[2].value;
  FLICam->height = FLIImg->width = FrameN[3].value;
  
  BinningN[0].value = BinningN[1].value = 1;
  
  IDLog("The Camera Width is %d ---- %d\n", (int) FLICam->width, (int) FrameN[2].value);
  IDLog("The Camera Height is %d ---- %d\n", (int) FLICam->height, (int) FrameN[3].value);
  
  IDSetNumber(&PixelSizeNP, NULL);
  IDSetNumber(&TemperatureNP, NULL);
  IDSetNumber(&FrameNP, NULL);
  IDSetNumber(&BinningNP, NULL);
  
  IDLog("Exiting getBasicData()\n");
  
}

int manageDefaults(char errmsg[])
{
  long err;
  /*int exposeTimeMS;
  
  exposeTimeMS = (int) (ExposeTimeWN[0].value * 1000.);
  
  IDLog("Setting default exposure time of %d ms.\n", exposeTimeMS);
  if ( (err = FLISetExposureTime(fli_dev, exposeTimeMS) ))
  {
    snprintf(errmsg, ERRMSG_SIZE, "FLISetExposureTime() failed. %s.\n", strerror((int)-err));
    IDLog(errmsg, NULL);
    return -1;
  }*/
  
  /* Default frame type is NORMAL */
  if ( (err = FLISetFrameType(fli_dev, FLI_FRAME_TYPE_NORMAL) ))
  {
    snprintf(errmsg, ERRMSG_SIZE, "FLISetFrameType() failed. %s.\n", strerror((int)-err));
    IDLog(errmsg, NULL);
    return -1;
  }
  
  /* X horizontal binning */
  if ( (err = FLISetHBin(fli_dev, BinningN[0].value) ))
  {
    snprintf(errmsg, ERRMSG_SIZE, "FLISetBin() failed. %s.\n", strerror((int)-err));
    IDLog(errmsg, NULL);
    return -1;
  }
  
  /* Y vertical binning */
  if ( (err = FLISetVBin(fli_dev, BinningN[1].value) ))
  {
    snprintf(errmsg, ERRMSG_SIZE, "FLISetVBin() failed. %s.\n", strerror((int)-err));
    IDLog(errmsg, NULL);
    return -1;
  }
  
  IDLog("Setting default binning %f x %f.\n", BinningN[0].value, BinningN[1].value);
  
  FLISetNFlushes(fli_dev, NFLUSHES);
  
  /* Set image area */
  if (setImageArea(errmsg))
    return -1;
  
  /* Success */
  return 0;
    
}

int getOnSwitch(ISwitchVectorProperty *sp)
{
  int i=0;
 for (i=0; i < sp->nsp ; i++)
 {
   /*IDLog("Switch %s is %s\n", sp->sp[i].name, sp->sp[i].s == ISS_ON ? "On" : "Off");*/
     if (sp->sp[i].s == ISS_ON)
      return i;
 }

 return -1;
}

int checkPowerS(ISwitchVectorProperty *sp)
{
  if (ConnectSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", sp->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", sp->label);
	
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int checkPowerN(INumberVectorProperty *np)
{
  if (ConnectSP.s != IPS_OK)
  {
     if (!strcmp(np->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", np->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", np->label);
    
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;
}

int checkPowerT(ITextVectorProperty *tp)
{

  if (ConnectSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", tp->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", tp->label);
	
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void connectCCD()
{
	long err;
	char errmsg[ERRMSG_SIZE];
	
	IDLog ("In ConnectCCD\n");
   
  /* USB by default {USB, SERIAL, PARALLEL, INET} */
	switch (ConnectS[0].s)
	{
		case ISS_ON:
		IDLog("Current portSwitch is %d\n", portSwitchIndex);
		IDLog("Attempting to find the camera in domain %ld\n", Domains[portSwitchIndex]);
		if (findcam(Domains[portSwitchIndex])) {
			ConnectSP.s = IPS_IDLE;
			ConnectS[0].s = ISS_OFF;
			ConnectS[1].s = ISS_ON;
			IDSetSwitch(&ConnectSP, "Error: no cameras were detected.");
			IDLog("Error: no cameras were detected.\n");
			return;
		}

		if ((err = FLIOpen(&fli_dev, FLICam->name, FLIDEVICE_CAMERA | FLICam->domain)))
		{
			ConnectSP.s = IPS_IDLE;
			ConnectS[0].s = ISS_OFF;
			ConnectS[1].s = ISS_ON;
			IDSetSwitch(&ConnectSP, "Error: FLIOpen() failed. %s.", strerror( (int) -err));
			IDLog("Error: FLIOpen() failed. %s.\n", strerror( (int) -err));
			return;
		}
      
		/* Sucess! */
		ConnectS[0].s = ISS_ON;
		ConnectS[1].s = ISS_OFF;
		ConnectSP.s = IPS_OK;
		IDSetSwitch(&ConnectSP, "CCD is online. Retrieving basic data.");
		IDLog("CCD is online. Retrieving basic data.\n");
		getBasicData();
		if (manageDefaults(errmsg))
		{
			IDMessage(mydev, errmsg, NULL);
			IDLog("%s", errmsg);
			return;
		}

		break;
      
		case ISS_OFF:
			ConnectS[0].s = ISS_OFF;
			ConnectS[1].s = ISS_ON;
			ConnectSP.s = IPS_IDLE;
			if ((err = FLIClose(fli_dev))) {
				ConnectSP.s = IPS_IDLE;
				ConnectS[0].s = ISS_OFF;
				ConnectS[1].s = ISS_ON;
				IDSetSwitch(&ConnectSP, "Error: FLIClose() failed. %s.", strerror( (int) -err));
				IDLog("Error: FLIClose() failed. %s.\n", strerror( (int) -err));
				return;
			}
			IDSetSwitch(&ConnectSP, "CCD is offline.");
			break;
     }
}

/* isCCDConnected: return 1 if we have a connection, 0 otherwise */
int isCCDConnected(void)
{
  return ((ConnectS[0].s == ISS_ON) ? 1 : 0);
}

int findcam(flidomain_t domain)
{
  char **tmplist;
  long err;
  
  IDLog("In find Camera, the domain is %ld\n", domain);

  if (( err = FLIList(domain | FLIDEVICE_CAMERA, &tmplist)))
  {
    IDLog("FLIList() failed. %s\n", strerror((int)-err));
    return -1;
  }
  
  if (tmplist != NULL && tmplist[0] != NULL)
  {
    int i;

    IDLog("Trying to allocate memory to FLICam\n");
    if ((FLICam = malloc (sizeof (cam_t))) == NULL)
    {
        IDLog("malloc() failed.\n");
	return -1;
    }

    for (i = 0; tmplist[i] != NULL; i++)
    {
      int j;

      for (j = 0; tmplist[i][j] != '\0'; j++)
	if (tmplist[i][j] == ';')
	{
	  tmplist[i][j] = '\0';
	  break;
	}
    }
    
     FLICam->domain = domain;
     
	switch (domain)
	{
		case FLIDOMAIN_PARALLEL_PORT:
			FLICam->dname = strdup("parallel port");
			break;

		case FLIDOMAIN_USB:
			FLICam->dname = strdup("USB");
			break;

		case FLIDOMAIN_SERIAL:
			FLICam->dname = strdup("serial");
			break;

		case FLIDOMAIN_INET:
			FLICam->dname = strdup("inet");
			break;

		default:
			FLICam->dname = strdup("Unknown domain");
	}
      
      FLICam->name = strdup(tmplist[0]);
      
     if ((err = FLIFreeList(tmplist)))
     {
       IDLog("FLIFreeList() failed. %s.\n", strerror((int)-err));
       return -1;
     }
      
   } /* end if */
   else
   {
     if ((err = FLIFreeList(tmplist)))
     {
       IDLog("FLIFreeList() failed. %s.\n", strerror((int)-err));
       return -1;
     }
     
     return -1;
   }

  IDLog("Findcam() finished successfully.\n");
  return 0;
}

double min()
{
  double lmin = FLIImg->img[0];
  int ind=0, i, j;
  
  for (i= 0; i < FLIImg->height ; i++)
    for (j= 0; j < FLIImg->width; j++)
    {
       ind = (i * FLIImg->width) + j;
       if (FLIImg->img[ind] < lmin) lmin = FLIImg->img[ind];
    }
    
    return lmin;
}

double max()
{
  double lmax = FLIImg->img[0];
  int ind=0, i, j;
  
   for (i= 0; i < FLIImg->height ; i++)
    for (j= 0; j < FLIImg->width; j++)
    {
      ind = (i * FLIImg->width) + j;
       if (FLIImg->img[ind] > lmax) lmax = FLIImg->img[ind];
    }
    
    return lmax;
}
