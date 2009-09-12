/*
   INDI Developers Manual
   Tutorial #3
   
   "Simple CCD Simulator"
   
   In this tutorial, we employ a BLOB property to send a sample FITS file to the client.
   
   Refer to README, which contains instruction on how to build this driver, and use it 
   with an INDI-compatible client.

*/

/** \file tutorial_three.c
    \brief Simulate a CCD camera by using INDI BLOBs to send FITS data to the client.
    \author Jasem Mutlaq
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

/* INDI Core headers */
#include "indidevapi.h"
#include "indicom.h"
#include "eventloop.h"
#include "base64.h"

#define mydev         	"CCD Simulator"
#define COMM_GROUP	"Main Control"

/* Handy macro */
#define currentTemp	TemperatureN[0].value

void uploadFile(const char* filename);
void ISPoll(void * p);

static double targetTemp	= 0;

/*INDI controls */

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};

/* Exposure time */
static INumber ExposeTimeN[]    = {{ "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0., 36000., .5, 1., 0, 0, 0}};
static INumberVectorProperty ExposeTimeNP = { mydev, "CCD_EXPOSURE", "Expose", COMM_GROUP, IP_RW, 60, IPS_IDLE, ExposeTimeN, NARRAY(ExposeTimeN), "", 0};
 
/* Temperature control */
 static INumber TemperatureN[]	  = { {"TEMPERATURE", "Temperature", "%+06.2f", -30., 40., 1., 0., 0, 0, 0}};
 static INumberVectorProperty TemperatureNP = { mydev, "CCD_TEMPERATURE", "Temperature (C)", COMM_GROUP, IP_RW, 60, IPS_IDLE, TemperatureN, NARRAY(TemperatureN), "", 0};

/* BLOB for sending image */
static IBLOB imageB = {"CCD1", "Feed", "", 0, 0, 0, 0, 0, 0, 0};
static IBLOBVectorProperty imageBP = {mydev, "Video", "Video", COMM_GROUP,
  IP_RO, 0, IPS_IDLE, &imageB, 1, "", 0};

void ISGetProperties (const char *dev)
{ 

  if (dev && strcmp (mydev, dev))
    return;

  /* COMM_GROUP */
  IDDefSwitch(&PowerSP, NULL);
  IDDefNumber(&ExposeTimeNP, NULL);
  IDDefNumber(&TemperatureNP, NULL);
  IDDefBLOB(&imageBP, NULL);

  IEAddTimer(1000, ISPoll, NULL);
  
}

/* Note that we must define ISNewBLOB and ISSnoopDevice even if we don't use them, otherwise, the driver will NOT compile */
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {}
void ISSnoopDevice (XMLEle *root) {}

  
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;
	    
	/* Connection */
	if (!strcmp (name, PowerSP.name))
	{
	  IUUpdateSwitch(&PowerSP, states, names, n);
   	  PowerSP.s = IPS_OK;
          if (PowerS[0].s == ISS_ON)
          	IDSetSwitch(&PowerSP, "CCD Simulator is online.");
          else
		IDSetSwitch(&PowerSP, "CCD Simulator is offline.");
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

    /* Exposure time */
    if (!strcmp (ExposeTimeNP.name, name))
    {

     /* Let's make sure that our simulator is online */
     if (PowerS[0].s != ISS_ON)
     {
       ExposeTimeNP.s = IPS_IDLE;
       IDSetNumber(&ExposeTimeNP, "CCD Simulator is offline.");
       return;
     }

      IDLog("Sending BLOB FITS...\n");

      ExposeTimeNP.s = IPS_BUSY;
      ExposeTimeN[0].value = values[0];
 
      return;
    } 

    /* Temperature */
    if (!strcmp (TemperatureNP.name, name))
    {

     /* Let's make sure that our simulator is online */
     if (PowerS[0].s != ISS_ON)
     {
       TemperatureNP.s = IPS_IDLE;
       IDSetNumber(&TemperatureNP, "CCD Simulator is offline");
       return;
     }

      targetTemp = values[0];

      /* If the requested temperature is the current CCD chip temperature, then return */
      if (targetTemp == TemperatureN[0].value)
      {
        TemperatureNP.s = IPS_OK;
        IDSetNumber(&TemperatureNP, NULL);
        return;
      }
      
      /* Otherwise, set property state to busy */
      TemperatureNP.s = IPS_BUSY;
      IDSetNumber(&TemperatureNP, "Setting CCD temperature to %g", targetTemp);

      return;
    }

}

/* Open a data file, compress its content, and send it via our BLOB property */
void uploadFile(const char* filename)
{
   FILE * fitsFile;
   unsigned char *fitsData, *compressedData;
   int r=0;
   unsigned int i =0, nr = 0;
   uLongf compressedBytes=0;
   uLong  totalBytes;
   struct stat stat_p; 
 
   /* #1 Let's get the file size */
   if ( -1 ==  stat (filename, &stat_p))
   { 
     IDLog(" Error occoured attempting to stat file.\n"); 
     return; 
   }
   
   /* #2 Allocate memory for raw and compressed data */
   totalBytes     = stat_p.st_size;
   fitsData       = (unsigned char *) malloc (sizeof(unsigned char) * totalBytes);
   compressedData = (unsigned char *) malloc (sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   
   if (fitsData == NULL || compressedData == NULL)
   {
     IDLog("Error! low memory. Unable to initialize fits buffers.\n");
     return;
   }
   
   /* #3 Open the FITS file */
   fitsFile = fopen(filename, "r");
   
   if (fitsFile == NULL)
    return;
   
   /* #4 Read file from disk */ 
   for (i=0; i < totalBytes; i+= nr)
   {
      nr = fread(fitsData + i, 1, totalBytes - i, fitsFile);
     
     if (nr <= 0)
     {
        IDLog("Error reading temporary FITS file.\n");
        return;
     }
   }
   
   compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
    
   /* #5 Compress raw data */ 
   r = compress2(compressedData, &compressedBytes, fitsData, totalBytes, 9);
   if (r != Z_OK)
   {
 	/* this should NEVER happen */
 	IDLog("internal error - compression failed: %d\n", r);
	return;
   }
   
   /* #6 Send it */
   imageB.blob = compressedData;
   imageB.bloblen = compressedBytes;
   imageB.size = totalBytes;
   strcpy(imageB.format, ".fits.z");

   /* #7 Set BLOB state to Ok */
   imageBP.s = IPS_OK;
   IDSetBLOB (&imageBP, NULL);

   /* #8 Set Exposure status to Ok */
   ExposeTimeNP.s = IPS_OK;
   IDSetNumber(&ExposeTimeNP, "Sending FITS...");
   
   /* #9 Let's never forget to free our memory */
   free (fitsData);   
   free (compressedData);
   
}

/* Poll device status and update accordingly */
void ISPoll(void * p)
{

   /* We need to check the status of properties that we want to watch. */

   /* #1 CCD Exposure */
   switch (ExposeTimeNP.s)
   {
     case IPS_IDLE:
     case IPS_OK:
     case IPS_ALERT:
	break;

     /* If an exposure state is busy, decrement the exposure value until it's zero (done exposing). */
     case IPS_BUSY:
	ExposeTimeN[0].value--;

        /* Are we done yet? */
        if (ExposeTimeN[0].value == 0)
        {
          /* Let's set the state of OK and report that to the client */
          ExposeTimeNP.s = IPS_OK;
 
          /* Upload a sample FITS file */
          uploadFile("ngc1316o.fits");

          IDSetNumber(&ExposeTimeNP, NULL);
	  break;
        }

	IDSetNumber(&ExposeTimeNP, NULL);
        break;
   } 

   /* #2 CCD Temperature */
   switch (TemperatureNP.s)
   {
     case IPS_IDLE:
     case IPS_OK:
     case IPS_ALERT:
	break;

     case IPS_BUSY:
     /* If target temperature is higher, then increase current CCD temperature */  
     if (currentTemp < targetTemp)
        currentTemp++;
     /* If target temperature is lower, then decrese current CCD temperature */
     else if (currentTemp > targetTemp)
       currentTemp--;
     /* If they're equal, stop updating */
     else
     {
       TemperatureNP.s = IPS_OK;
       IDSetNumber(&TemperatureNP, "Target temperature reached.");

       break;
     }

     IDSetNumber(&TemperatureNP, NULL);
     break;
   }

   /* Keep polling alive */
   IEAddTimer (1000, ISPoll, NULL);

}
