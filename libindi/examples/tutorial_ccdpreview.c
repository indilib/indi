/*
   INDI Developers Manual
   Tutorial CCDPreview
   
   "CCD Preview"
   
   In this tutorial, demostrate the ccdpreview feature, and simulate the readout of a CCD camera. 
   
   Refer to README, which contains instruction on how to build this driver, and use it 
   with an INDI-compatible client.
   
   For use with Kstars: click "Connect"; click "ON"; click "Start" 

*/

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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zlib.h>
#include <math.h>

/* INDI Core headers */
#include "indidevapi.h"
#include "indicom.h"
#include "eventloop.h"
#include "base64.h"

#define mydev         	"Device with Data Transfer"
#define IMAGE_CONTROL   "Image Control"
#define COMM_GROUP	"Main Control"
#define IMAGE_GROUP	"Image Settings"
#define	POLLMS			100
#define WIDTH ((int) CtrlN[0].value)
#define HEIGHT ((int) CtrlN[1].value)
#define BYTESPERPIX ((int) CtrlN[3].value)
#define BYTEORDER ((int) CtrlN[4].value)
#define FWHMPIX (CCDInfoN[0].value)
#define FOCUS (FocusN[0].value)

void uploadStream(uLong maxBytes);
void readoutSim();
void makepic();

/*INDI controls */

/* These properties with these names are required display a video stream*/
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};
static ISwitch StreamS[2]={{ "ON", "", ISS_OFF},{"OFF", "", ISS_ON}};
static ISwitchVectorProperty StreamSP={mydev, "CCDPREVIEW_STREAM", "Video Stream", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE,StreamS, NARRAY(StreamS),"",0};
static INumber CtrlN[] = {{"WIDTH", "Width", "%0.f",16., 512., 16., 256.},
	{"HEIGHT", "Height", "%0.f", 16., 512., 16., 256.},{"MAXGOODDATA", "max. good data value", "%0.f", 1., 256.0*256.0*256.0*256.0, 0., 30000.},
	{"BYTESPERPIXEL", "Bytes/pix", "%0.f", 1., 4., 1., 2.},{"BYTEORDER", "Byte Order", "%0.f", 1., 2., 1., 2.}};
static INumberVectorProperty CtrlNP = {mydev ,  "CCDPREVIEW_CTRL", "Image Size", COMM_GROUP, IP_RW, 60, IPS_IDLE,CtrlN, NARRAY(CtrlN),"",0};
/* These properties with these names are required by the indi client to calculate the FWHM in arcsec*/
/* The FWHM here is in units of pixels. The Pixelsize here is in units of microns. */
static INumber CCDInfoN[] = {{"CCD_FWHM_PIXEL", "FWHM[pix]", "%0.2f",0., 100., 1., 3.},
	{"CCD_PIXEL_SIZE", "Pixelsize[mu]", "%0.1f",0., 100., 1., 15.5}};
static INumberVectorProperty CCDInfoNP = {mydev ,  "CCD_INFO", "CCD Info", COMM_GROUP, IP_RO, 60, IPS_IDLE,CCDInfoN, NARRAY(CCDInfoN),"",0};



/* Blob for sending image (required but the names can be chosen freely)*/
static IBLOB imageB = {"CCD1", "Feed", "", 0, 0, 0, 0, 0, 0, 0};
static IBLOBVectorProperty imageBP = {mydev, "Video", "Video", COMM_GROUP,
  IP_RO, 0, IPS_IDLE, &imageB, 1, "", 0};
  
/* Start / Stop Switch (not required, and only defined in this specific driver)*/
static ISwitch ReadoutS[2]={{ "Start", "", ISS_OFF},{"Stop", "", ISS_ON}};
static ISwitchVectorProperty ReadoutSP={mydev, "readout", "Readout", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE,ReadoutS, NARRAY(ReadoutS),"",0};
/* Adjustable Focus (not required, and only defined in this specific driver)*/
static INumber FocusN[] = {{"Focus", "Focus", "%0.f",0., 100., 1., 40.}};
static INumberVectorProperty FocusNP = {mydev ,  "Telescope", "Telescope", COMM_GROUP, IP_RW, 60, IPS_IDLE,FocusN, NARRAY(FocusN),"",0};


/* 1 if "readout simulation" is running, 0 otherwise*/
volatile int readout_is_running=0;
/* The number of bytes read from the "virtual CCD" up to now */
volatile uLong readout_bytes_done=0;
/* 1 if readout simulation is aked to stop, 0 otherwise*/
volatile uLong readout_stop=0 ;   
  
/* a buffer to hold a 16 bit image to be displayed*/
unsigned char *image;

/* in this tutorial we simply calculate the FWHM from the focus-position
   in a real driver you will have calculate from the FWHM from the image you
   took with your CCD. For example you can write a fitsfile and use 
   sextractor (sextractor.sourceforge.net) to make a list of all detected 
   objects with their "FWHM_WORLD" parameters and take the median of
   that list. But make your FWHM is in units of pixels.
*/
double fwhm()
{
	return 0.3*sqrt((FOCUS-50.0)*(FOCUS-50.0))+1.0;
}

/* Initlization routine */
static void readout_start()
{
	if (readout_is_running!=0)
	    return;
	
	/* start timer to simulate CCD readout
	   The timer will call function previewSim after POLLMS milliseconds */
	image=malloc(WIDTH*HEIGHT*BYTESPERPIX);
	readout_bytes_done=0;
	readout_stop=0;
	readout_is_running = 1;
	CtrlNP.s=IPS_OK;
	IDSetNumber(&CtrlNP, NULL);
	makepic();
	IEAddTimer (POLLMS, readoutSim, NULL);
}


void ISGetProperties (const char *dev)
{ 

  if (dev && strcmp (mydev, dev))
    return;

  IDDefSwitch(&StreamSP, NULL); 
  IDDefSwitch(&ReadoutSP, NULL);
  IDDefSwitch(&PowerSP, NULL);
  IDDefNumber(&CtrlNP, NULL);
  IDDefNumber(&FocusNP, NULL);
  IDDefNumber(&CCDInfoNP, NULL);
  FocusNP.s=IPS_OK;
  IDSetNumber(&FocusNP, NULL);
  CCDInfoNP.s=IPS_OK;
  IDSetNumber(&CCDInfoNP, NULL);
  CtrlNP.s=IPS_OK;
  IDSetNumber(&CtrlNP, NULL);
  IDDefBLOB(&imageBP, NULL);
  
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], char *blobs[], char *formats[], char *names[], int n)
{
	
       /* ignore if not ours */ 
       if (dev && strcmp (mydev, dev))
         return;

	/* suppress warning */
	n=n; dev=dev; name=name; names=names; 

}
  
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	
	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;
	    
	/* Connection */
	if (!strcmp (name, PowerSP.name))
	{
	  IUUpdateSwitches(&PowerSP, states, names, n);
   	  PowerSP.s = IPS_OK;
          IDSetSwitch(&PowerSP, NULL);
	  return;
	}
	
	if (!strcmp (name, StreamSP.name))
	{
	  IUUpdateSwitches(&StreamSP, states, names, n);
   	  StreamSP.s = IPS_OK;
          IDSetSwitch(&StreamSP, NULL);
	  return;
	}
	
	/* Readout start/stop*/
	if (!strcmp (name, ReadoutSP.name))
	{
	  IUUpdateSwitches(&ReadoutSP, states, names, n);
   	  if (ReadoutS[0].s==ISS_ON) {
		readout_start();  
	  }
	  if (ReadoutS[1].s==ISS_ON) {
		readout_stop=1;  
	  }
	  ReadoutSP.s = IPS_OK;
          IDSetSwitch(&ReadoutSP, NULL);
	  return;
	}
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	
       /* ignore if not ours */ 
       if (dev && strcmp (mydev, dev))
         return;

	/* suppress warning */
	n=n; dev=dev; name=name; names=names; texts=texts;
	
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	/* ignore if not ours */ 
       if (dev && strcmp (mydev, dev))
         return;
       
       /* suppress warning */
       n=n; dev=dev; name=name; names=names; 
       
	if (!strcmp (name, CtrlNP.name)) {
		if (readout_is_running==0) {
			IUUpdateNumbers(&CtrlNP, values, names, n);
		}
		else {
			 IDMessage (mydev, "The Parameters can not be change during readout\n");
		}
		CtrlNP.s=IPS_OK;
		IDSetNumber(&CtrlNP, NULL);
	}
	if (!strcmp (name, FocusNP.name)) {
		if (readout_is_running==0) {
			IUUpdateNumbers(&FocusNP, values, names, n);
		}
		else {
			 IDMessage (mydev, "The Parameters can not be change during readout\n");
		}
		FocusNP.s=IPS_OK;
		IDSetNumber(&FocusNP, NULL);
	}
}


void uploadStream(uLong maxBytes)
{
	
   unsigned char *Data, *compressedData;
   int r=0,offs=0;
   unsigned int i =0;
   unsigned char j;
   static uLong old_maxBytes, sendBytes;	
   uLongf compressedBytes=0;
   uLong  totalBytes;
   /* #0 Allocate buffer*/
   totalBytes     = WIDTH*HEIGHT*BYTESPERPIX;
   Data       = (unsigned char *) malloc (sizeof(unsigned char) * totalBytes);
   compressedData = (unsigned char *) malloc (sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   if (Data == NULL || compressedData == NULL)
   {
     IDLog("Error! low memory. Unable to initialize fits buffers.\n");
     return;
   }
   if (maxBytes<old_maxBytes) {
     old_maxBytes=0;
   }
   
   /* #1 fill transferbuffer ("Data") with data read from the imagebuffer ("image")*/
   if (maxBytes>totalBytes) {
	maxBytes=totalBytes;
   }
   j=0;
   for (i=old_maxBytes; i < maxBytes; i+= 1) {
     if (BYTEORDER==1) {
       	Data[i-old_maxBytes]=image[i];
     }
     if (BYTEORDER==2) {
       if (i%(2*BYTESPERPIX)==0) {
         offs=i/2;
         }
         if ((i%(2*BYTESPERPIX))<BYTESPERPIX) {
	 Data[i-old_maxBytes]=image[i-offs];
         }
        else {
          Data[i-old_maxBytes]=image[WIDTH*HEIGHT*BYTESPERPIX-(i-offs)];
       }
     }
   }
   sendBytes=maxBytes-old_maxBytes;
   compressedBytes = sizeof(char) * sendBytes + sendBytes / 64 + 16 + 3;
   
   /* #2 Compress it */ 
   r = compress2(compressedData, &compressedBytes, Data, sendBytes, 9);
   if (r != Z_OK)
   {
 	/* this should NEVER happen */
 	IDLog("internal error - compression failed: %d\n", r);
	return;
   }
   /* #3 Send it */
   imageB.blob = compressedData;
   imageB.bloblen = compressedBytes;
   imageB.size = sendBytes;
   strcpy(imageB.format, ".ccdpreview.z");
   imageBP.s = IPS_OK;
   IDSetBLOB (&imageBP, NULL);
   free (Data);   
   free (compressedData);
   old_maxBytes=maxBytes;
   
}


/* readout the "virtual CCD" over time*/
void readoutSim (void *p)
{
	readout_bytes_done+=WIDTH*BYTESPERPIX*8;
	uploadStream(readout_bytes_done);
	/* again */
	if ((readout_bytes_done<WIDTH*HEIGHT*BYTESPERPIX)&&(readout_stop==0)) {
		IEAddTimer (POLLMS, readoutSim, NULL);
	}
	else {
		/* Here you should calculate the fwhm from your image*/
		FWHMPIX=fwhm();
		free(image);
		CCDInfoNP.s=IPS_OK;
		IDSetNumber(&CCDInfoNP, NULL);
		readout_is_running=0;
	}
}

/* Writes a simulated star to the image buffer*/
void makestar(int x, int y, unsigned long f,int boxsize,double fwhm)
{
	int i,j,b;
	unsigned long val;
	float rr;
	for (i=x-boxsize/2;i<x+boxsize/2;i++){
		for (j=y-boxsize/2;j<y+boxsize/2;j++){
			if ((i<0)||(j<0)||(i>WIDTH-1)||(j>HEIGHT-1)) {
				continue;
			}
			rr=exp(-2.0*0.7*((x-i)*(x-i)+(y-j)*(y-j))/(fwhm*fwhm));
			val=0;
			for (b=0;b<BYTESPERPIX;b++) {
				val+=(unsigned long) image[BYTESPERPIX*i+WIDTH*BYTESPERPIX*j+b]*pow(256.0,b);
			}
			val+=f*rr;
			if (BYTESPERPIX==1) {
				val=val/256;
			}
			for (b=0;b<BYTESPERPIX;b++) {
				image[BYTESPERPIX*i+WIDTH*BYTESPERPIX*j+b]=val % 256;
				val=(val-(val % 256))/256;
			}
		}
	}
}

/*Write a simulated bias with readout noise to the image buffer */
void makebias(unsigned long bias,unsigned long noise)
{
	int i,j,b;
	unsigned long val;
	for (i=0;i<WIDTH;i+=1){
		for (j=0;j<HEIGHT;j+=1){
			val=bias;
			val+=(unsigned long) (noise*(rand()/(1.0*RAND_MAX)));
			if (BYTESPERPIX==1) {
				val=val/256;
			}
			for (b=0;b<BYTESPERPIX;b++) {
				image[BYTESPERPIX*i+WIDTH*BYTESPERPIX*j+b]=val % 256;
				val=(val-(val % 256))/256;
			}
		}
	}
}

/*Fill the image buffer with some pseudoastronimical data*/
void makepic()
{
	unsigned long i,x,y,b;
	double rr;
	makebias(200,6);
	/*some faint stars*/
	for (i=1;i<50;i++) {
		x=(int) ((WIDTH-1)*(rand()/(1.0*RAND_MAX)));
		y=(int) ((HEIGHT-1)*(rand()/(1.0*RAND_MAX)));
		rr=pow((rand()/(1.0*RAND_MAX)),2.0)+pow((rand()/(1.0*RAND_MAX)),2.0)
		  +pow((rand()/(1.0*RAND_MAX)),2.0);
		rr*=10;
		b=(int) (10000.0/(rr+1));
		makestar(x,y,b,3*fwhm(),fwhm());
	}
	/*some bright stars*/
	for (i=1;i<50;i++) {
		x=(int) ((WIDTH-1)*(rand()/(1.0*RAND_MAX)));
		y=(int) ((HEIGHT-1)*(rand()/(1.0*RAND_MAX)));
		rr=pow((rand()/(1.0*RAND_MAX)),2.0)+pow((rand()/(1.0*RAND_MAX)),2.0)
		  +pow((rand()/(1.0*RAND_MAX)),2.0);
		rr*=10;
		b=(unsigned long) (10000.0/(rr+1));
		if (BYTESPERPIX>1) {
			b=(unsigned long) b*(pow(256.0,BYTESPERPIX-2.0));
		}
		makestar(x,y,b,3*fwhm(),fwhm() );
	}
	/*some  stars of defined brightness */
	for (i=1;i<3;i++) {
		x=(int) ((WIDTH-1)*(rand()/(1.0*RAND_MAX)));
		y=(int) ((HEIGHT-1)*(rand()/(1.0*RAND_MAX)));
		b=65000;
		makestar(x,y,b,100,fwhm());
	}
	makestar(WIDTH/2,HEIGHT/2,65000,100,fwhm());
}
