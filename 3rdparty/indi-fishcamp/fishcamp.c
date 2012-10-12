/*

	Fishcamp CMOS Camera autoguider http://www.fishcamp.com/

        basic indi driver written by Tanmay Shah & Twinkle Shah (project trainees at PRL, Ahmedabad, - contact shashikiran_ganesh@yahoo.co.in )
	based on test_app.c from the fishcamp linux package
        indi driver based on fliccd driver from indi ..
	
	January-March 2011

	last modified on 28-06-2011 to check with libindi-0.8
	
*/

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

#include "libFcLinux.h"

#include "indidevapi.h"
#include "eventloop.h"
#include "indicom.h"


#define L_tmpnam        500   
#define mydev         	"FishCamp CCD"
#define COMM_GROUP	"Main Control"
#define POLLMS		1000		/* Polling time (ms) */
#define TEMPFILE_LEN    16 
#define currentTemp	TemperatureN[0].value



void connectCCD(void);
void handleExposure(void *);
int  checkPowerN(INumberVectorProperty *np);
int  isCCDConnected(void);
void ISPoll(void *);
//int WriteTIFF(unsigned short * buffer, int cols, int rows, char * filename);
int  writeFITS(const char* filename, char errmsg[]);
void addFITSKeywords(fitsfile *fptr);


/*INDI controls */
/* Connect/Disconnect */
static ISwitch ConnectS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty ConnectSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, ConnectS, NARRAY(ConnectS), "", 0};


/* Exposure time */
static INumber ExposeTimeN[]    = {{ "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0., 36000., .5, 1., 0, 0, 0}};
static INumberVectorProperty ExposeTimeNP = { mydev, "CCD_EXPOSURE", "Expose", COMM_GROUP, IP_RW, 60, IPS_IDLE, ExposeTimeN, NARRAY(ExposeTimeN), "", 0};
 
/* Temperature control */
 static INumber TemperatureN[]	  = { {"CCD_TEMPERATURE_VALUE", "Temperature", "%+06.2f", -30., 40., 1., 0., 0, 0, 0}};
 static INumberVectorProperty TemperatureNP = { mydev, "CCD_TEMPERATURE", "Temp(C)", COMM_GROUP, IP_RW, 60, IPS_IDLE, TemperatureN, NARRAY(TemperatureN), "", 0};


/* BLOB for sending image */
static IBLOB imageB = {"FITS_BLOB", "CCD1", "", 0, 0, 0, 0, 0, 0, 0};
static IBLOBVectorProperty imageBP = {mydev, "CCD_FITS_BLOB", "CCD1", COMM_GROUP,
  IP_RO, 0, IPS_IDLE, &imageB, 1, "", 0};


int camNum=1;
char filename[]="/tmp/fitsXXXXXX";
void *frameBuffer;
UInt16 rows=1024;
UInt16 cols=1280;
UInt16 top,bottom,left,right;
size_t size=1024*1280*2;

double targetTemp =	0.0;
double exposuretime;

void ISGetProperties (const char *dev)
{ 

  if (dev && strcmp (mydev, dev))
    return;

  /* COMM_GROUP */
  IDDefSwitch(&ConnectSP, NULL);
  IDDefNumber(&ExposeTimeNP, NULL);
  IDDefNumber(&TemperatureNP, NULL);
 // IDDefNumber(&BinningNP, NULL);
  IDDefBLOB(&imageBP, NULL);
	

  IEAddTimer(POLLMS, ISPoll, NULL);
  
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
	    
	//ISInit();

	
	/* Connection */
	if (!strcmp (name, ConnectSP.name))
	{
	  if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0)
		return;
   	  connectCCD();
	  return;
	} 
}


void connectCCD()
{
	int numCamerasFound;
	int camNum=1;
	int cameraSerialNumber;	

	long err;
	char errmsg[ERRMSG_SIZE];
	
	//IDLog ("InConnectCCD\n");
   
  	switch (ConnectS[0].s)
	{
		case ISS_ON:
		{	
			fcUsb_init();

			numCamerasFound=fcUsb_FindCameras();

			IDMessage(mydev,"Found %d fishcamp cameras.\n", numCamerasFound);

			if (numCamerasFound > 0)
			{

				// if we have at least one of our cameras

				IDLog("Opening camera \n");
				// flag camera as being used by us
				fcUsb_OpenCamera(camNum);

				SInt16 temp=fcUsb_cmd_getTemperature(camNum);
				TemperatureN[0].value=(int)temp/100;
				IDSetNumber(&TemperatureNP, NULL);
				fcUsb_cmd_setReadMode(camNum, fc_classicDataXfr, fc_16b_data);

				// setup full image frame
				top = 0;
				left = 0;
				right = cols - 1;
				bottom = rows - 1;
				fcUsb_cmd_setRoi(camNum, left, top, right, bottom);

				// set the camera gain (gain = 4).  Gain can be anything from 1 -> 15.
				fcUsb_cmd_setCameraGain(camNum, 4);
		
				ConnectS[0].s = ISS_ON;
				ConnectS[1].s = ISS_OFF;
				ConnectSP.s = IPS_OK;
				IDSetSwitch(&ConnectSP, "CCD is online. Retrieving basic data.");
				IDLog("CCD is online. Retrieving basic data.\n");

				//cameraSerialNumber = fcUsb_GetCameraSerialNum(camNum);
				//IDMessage("  First camera has serial number = %d\n", cameraSerialNumber);
				

			}			
			else
			{
				IDMessage(mydev,"Inside ConnectCCD : CCD not found");

				ConnectSP.s = IPS_IDLE;
				ConnectS[0].s = ISS_OFF;
				ConnectS[1].s = ISS_ON;
				return;
			}
			
			break;
		}   
      
		case ISS_OFF:
		{
			//IDMessage(mydev,"Inside ConnectCCD IIS_OFF");
		
			ConnectS[0].s = ISS_OFF;
			ConnectS[1].s = ISS_ON;
			ConnectSP.s = IPS_IDLE;
			fcUsb_CloseCamera(camNum);
			free(frameBuffer);
			IDSetSwitch(&ConnectSP, "CCD is offline.");
			break;
		}
     	}
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	//ISInit();
 
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
	    
	//ISInit();
	
   	 /* Exposure time */
    	if (!strcmp (ExposeTimeNP.name, name))
    	{
       		if (checkPowerN(&ExposeTimeNP))
        	 return;

       		if (ExposeTimeNP.s == IPS_BUSY)
       		{
         
	  		ExposeTimeNP.s = IPS_IDLE;
	  		ExposeTimeN[0].value = 0;

	  		IDSetNumber(&ExposeTimeNP, "Exposure cancelled.");
	  		IDLog("Exposure Cancelled.\n");
	  
        	}
    
       		np = IUFindNumber(&ExposeTimeNP, names[0]);
	 
      		if (!np)
       		{
	   		ExposeTimeNP.s = IPS_ALERT;
	   		IDSetNumber(&ExposeTimeNP, "Error: %s is not a member of %s property.", names[0], name);
	   		return;
       		}
	 
        	np->value = values[0];
	
      		/* Set duration */  
   
      		fcUsb_cmd_setIntegrationTime(camNum,np->value*1000 );
		exposuretime=values[0]*1000;
      		IDLog("Exposure Time (ms) is: %g\n", np->value * 1000.);
      		IDLog("isnewNumber::exposuretime:%f",exposuretime);
      		handleExposure(NULL);
      		return;
    	} 
    
    

	/* Temperature */
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
    
 		//set temperature...   
       		TemperatureNP.s = IPS_BUSY;
		fcUsb_cmd_setTemperature(camNum, values[0]*100);
		targetTemp=values[0];
    		IDSetNumber(&TemperatureNP, "Setting CCD temperature to %+06.2f C", values[0]);
    		IDLog("Setting CCD temperature to %+06.2f C\n", values[0]);
    		return;
   	}

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



void ISPoll(void *p)
{
	char * err;
	long timeleft;
	double ccdTemp;
	
	
	
	if (!isCCDConnected())
	{
		IEAddTimer (POLLMS, ISPoll, NULL);
	 	return;
	}
	 
        /*IDLog("In Poll.\n");*/
	
	switch (ExposeTimeNP.s)
	{
		case IPS_IDLE:
		    break;
	    
		case IPS_OK:
		    break;
	    
		case IPS_BUSY:
		{
			int status=fcUsb_cmd_getState(camNum);
			if (status == 0)
        		{
          			/* Let's set the state of OK and report that to the client */
          			ExposeTimeNP.s = IPS_OK;
				ExposeTimeN[0].value = 0;

				IDSetNumber(&ExposeTimeNP, "Exposure done, downloading image...");
				IDSetNumber(&ExposeTimeNP, NULL);
	    			IDLog("Exposure done, downloading image...\n");

				rows = 1024;
				cols = 1280;
				size = rows * cols * 2;
				frameBuffer = malloc(size);

	   			fcUsb_cmd_getRawFrame(camNum, rows, cols, frameBuffer);
				
			  	writeFITS(filename,err);
			}
			else if (ExposeTimeN[0].value > 0)
 				{
				 ExposeTimeN[0].value--;
				 IDSetNumber(&ExposeTimeNP,NULL);//"Exposing. Time left %f",ExposeTimeN[0].value--); 
				IDLog("Exposure status %d\n",status);
				}
			
			break;
		}   
		case IPS_ALERT:
		    break;
	 }
	 
	 switch (TemperatureNP.s)
	 {
	 	case IPS_IDLE:
			currentTemp=fcUsb_cmd_getTemperature(camNum)/100;
			IDSetNumber(&TemperatureNP,NULL); //, "Temperature in IDLE %+6.2f",currentTemp);
	   	case IPS_OK:
			//get temperature	    
			currentTemp=fcUsb_cmd_getTemperature(camNum)/100; 	
			IDSetNumber(&TemperatureNP,NULL);	
			break;
	     
	   	case IPS_BUSY:
			currentTemp=fcUsb_cmd_getTemperature(camNum)/100;
			  
			 if(targetTemp == currentTemp)
			 {
       				TemperatureNP.s = IPS_OK;
       				IDSetNumber(&TemperatureNP, "Target temperature reached.");
				break;
     			 }

     			IDSetNumber(&TemperatureNP, NULL);
     			break;
	      
	    	case IPS_ALERT:
	     		break;
	  }
  
 	
	 IEAddTimer (POLLMS, ISPoll, NULL);
}



void handleExposure(void *p)
{
	int state;
	fcUsb_cmd_startExposure(camNum);
  
	ExposeTimeNP.s = IPS_BUSY;	

	IDSetNumber(&ExposeTimeNP,NULL );
		
	IDLog("Taking a frame...\n");
}


/* isCCDConnected: return 1 if we have a connection, 0 otherwise */
int isCCDConnected(void)
{
  return ((ConnectS[0].s == ISS_ON) ? 1 : 0);
}

void uploadFile(void *fitsdata,int total)
{
    //  lets try sending a ccd preview


    //IDLog("Enter Uploadfile with %d total sending via %s\n",total,FitsBV.name);
    imageB.blob=fitsdata;
    imageB.bloblen=total;
    imageB.size=total;
    strcpy(imageB.format,".fits");
    imageBP.s = IPS_OK;
    IDSetBLOB(&imageBP,NULL);
    //IDLog("Done with SetBlob\n");

    
}

int writeFITS(const char* filename, char errmsg[])
{
  fitsfile *fptr;       /* pointer to the FITS file; defined in fitsio.h */
  int status;
  void *memptr;
  size_t memsize;
  long  fpixel = 1, naxis = 2, nelements;
  long naxes[2];
  char filename_rw[TEMPFILE_LEN+1];

  naxes[0] = cols;
  naxes[1] = rows;

  /* Append ! to file name to over write it.*/
  snprintf(filename_rw, TEMPFILE_LEN+1, "!%s", filename);

  status = 0;         /* initialize status before calling fitsio routines */
  memsize=5760;
  memptr=malloc(memsize);
  if(!memptr) 
  {
            IDLog("Error: failed to allocate memory: %lu\n",(unsigned long)memsize);
  }

  fits_create_memfile(&fptr,&memptr,&memsize,2880,realloc,&status);

  if(status) 
  {
            IDLog("Error: Failed to create FITS image\n");
            fits_report_error(stderr, status);  /* print out any error messages */
            return false;
  }

  /* Create the primary array image (16-bit short integer pixels */
  fits_create_img(fptr, USHORT_IMG, naxis, naxes, &status);

  addFITSKeywords(fptr);

  nelements = naxes[0] * naxes[1];          /* number of pixels to write */

  /* Write the array of integers to the image */
  fits_write_img(fptr, TUSHORT, fpixel, nelements, frameBuffer, &status);

  fits_close_file(fptr, &status);            /* close the file */

  fits_report_error(stderr, status);  /* print out any error messages */

  /* Success */
  ExposeTimeNP.s = IPS_OK;
  IDSetNumber(&ExposeTimeNP, NULL);
  uploadFile(memptr,memsize);
  //unlink(filename);
  free(memptr);
  return status;
}

void addFITSKeywords(fitsfile *fptr)
{
  int status=0; 
  char binning_s[32];
  char frame_s[32];
  double min_val, max_val;
  
  fits_update_key(fptr, TDOUBLE, "CCD-TEMP", &(TemperatureN[0].value), "CCD Temperature (Celcius)", &status);
  //fits_update_key(fptr, TDOUBLE, "EXPOSURE", &(ExposeTimeN[0].value), "Total Exposure Time (ms)", &status); 
  fits_update_key(fptr, TDOUBLE, "EXPOSURE", &exposuretime, "Total Exposure Time (ms)", &status); 
  
  fits_update_key(fptr, TSTRING, "INSTR", "Fishcamp Engineering", "CCD Name", &status);
  fits_write_date(fptr, &status);
  fits_update_key_null(fptr, "END",NULL,&status);

}

