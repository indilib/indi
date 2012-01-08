/* Driver for any Apogee USB Alta camera.
 * low level USB code from http://www.randomfactory.com

    Copyright (C) 2007 Elwood Downey (ecdowney@clearskyinstitute.com)

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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <fitsio.h>

#include <indidevapi.h>
#include <eventloop.h>
#include <base64.h>
#include <zlib.h>

#include <libapogee.h>

/* operational info */
#define MYDEV "Apogee CCD"			/* Device name we call ourselves */
#define MAX_PIXELS	5000			/* Maximum row of FitsBP */

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"
#define DATA_GROUP      "Data Channel"

#define	MAXEXPERR	10		/* max err in exp time we allow, secs */
#define	COOLTM		5000		/* ms between reading cooler */
#define	OPENDT		5		/* open retry delay, secs */
#define BPP		2		/* Bytes per pixel */
#define TEMPFILE_LEN	16

//#define SIMULATION      1

static int impixw, impixh;		/* image size, final binned FitsBP */
static int expTID;			/* exposure callback timer id, if any */

/* info when exposure started */
static struct timeval exp0;		/* when exp started */

/* we premanently allocate an image buffer that is surely always large enough.
 * we do this for the following reasons:
 *   1. there is no sure means to limit how much GetImageData() will read
 *   2. this insures lack of memory at runtime will never be a cause for not
 *      being able to read
 */
static unsigned short imbuf[5000*5000];

/**********************************************************************************************/
/************************************ GROUP: Communication ************************************/
/**********************************************************************************************/

/********************************************
 Property: Connection
*********************************************/
enum { ON_S, OFF_S };
static ISwitch ConnectS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
ISwitchVectorProperty ConnectSP		= { MYDEV, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ConnectS, NARRAY(ConnectS), "", 0};


/********************************************
 Property: Port
*********************************************/
static ISwitch PortS[]          	= {{"USB" , "" , ISS_ON, 0, 0},{"ETHERNET", "", ISS_OFF, 0, 0}};
ISwitchVectorProperty PortSP		= { MYDEV, "Port" , "", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, PortS, NARRAY(PortS), "", 0};

/**********************************************************************************************/
/*********************************** GROUP: Expose ********************************************/
/**********************************************************************************************/

/********************************************
 Property: CCD Temperature [REQUEST]
*********************************************/

/* SetTemp parameter */
typedef enum {	/* N.B. order must match array below */
    T_STEMP, N_STEMP
} SetTempIndex;
static INumber  TemperatureWN[N_STEMP] = {{"CCD_TEMPERATURE_VALUE",  "Target temp, C (0 off)",    "%6.1f", -20.0, 20.,1.0, 0}};

static INumberVectorProperty TemperatureWNP = {MYDEV, "CCD_TEMPERATURE_REQUEST", "Set target cooler temperature", EXPOSE_GROUP, IP_WO, 0, IPS_IDLE, TemperatureWN, NARRAY(TemperatureWN), "", 0};

/********************************************
 Property: CCD Temperature [READ]
*********************************************/
/* TempNow parameter */
typedef enum {	/* N.B. order must match array below */
    T_TN, N_TN
} TempNowIndex;
static INumber  TemperatureRN[N_TN] = { {"CCD_TEMPERATURE_VALUE",  "Cooler temp, C",         "%6.1f", -20.0, 20.1, 1,0}};
static INumberVectorProperty TemperatureRNP = {MYDEV, "CCD_TEMPERATURE", "Current cooler temperature", EXPOSE_GROUP, IP_RO, 0, IPS_IDLE, TemperatureRN, NARRAY(TemperatureRN), "", 0};

/********************************************
 Property: CCD Exposure [REQUEST]
*********************************************/

/* Exposure time */
static INumber ExposureWN[]    = {{ "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0., 36000., .5, 1., 0, 0, 0}};
static INumberVectorProperty ExposureWNP = { MYDEV, "CCD_EXPOSURE_REQUEST", "Expose", EXPOSE_GROUP, IP_WO, 36000, IPS_IDLE, ExposureWN, NARRAY(ExposureWN), "", 0};

/********************************************
 Property: CCD Exposure [READ]
*********************************************/

static INumber ExposureRN[]    = {{ "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0., 36000., .5, 1., 0, 0, 0}};
static INumberVectorProperty ExposureRNP = { MYDEV, "CCD_EXPOSURE", "Expose", EXPOSE_GROUP, IP_RO, 36000, IPS_IDLE, ExposureRN, NARRAY(ExposureRN), "", 0};

/********************************************
 Property: Some CCD Settings
*********************************************/
/* ExpValues parameter */
typedef enum 
{	/* N.B. order must match array below */
    OSW_EV, OSH_EV, N_EV
} ExpValuesIndex;
static INumber  ExposureSettingsN[N_EV] = 
{
    {"OSW",      "Overscan width",    "%4.0f", 0, 50, 1,   0.0},
    {"OSH",      "Overscan height",   "%4.0f", 0, 50, 1,   0.0},
};
static INumberVectorProperty ExposureSettingsNP = {MYDEV, "ExpValues", "Exposure settings", EXPOSE_GROUP, IP_WO, 0, IPS_IDLE, ExposureSettingsN, NARRAY(ExposureSettingsN), "", 0};

/* Shutter Switch */
static ISwitch  ShutterS[2] = {{"SHUTTER_ON" , "Open" , ISS_ON, 0, 0},{"SHUTTER_OFF", "Closed", ISS_OFF, 0, 0}};

ISwitchVectorProperty ShutterSP		= { MYDEV, "SHUTTER" , "Shutter", EXPOSE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ShutterS, NARRAY(ShutterS), "", 0};

/**********************************************************************************************/
/*********************************** GROUP: Image Settings ************************************/
/**********************************************************************************************/

/********************************************
 Property: CCD Frame
*********************************************/
/* Frame coordinates. Full frame is default */
enum { CCD_X, CCD_Y, CCD_W, CCD_H };
static INumber FrameN[]          	= 
{
 { "X", "X", "%.0f", 0.,     MAX_PIXELS, 1., 0., 0, 0, 0},
 { "Y", "Y", "%.0f", 0.,     MAX_PIXELS, 1., 0., 0, 0, 0},
 { "WIDTH", "Width", "%.0f", 0., MAX_PIXELS, 1., 0., 0, 0, 0},
 { "HEIGHT", "Height", "%.0f",0., MAX_PIXELS, 1., 0., 0, 0, 0}};
 static INumberVectorProperty FrameNP = { MYDEV, "CCD_FRAME", "Frame", IMAGE_GROUP, IP_RW, 60, IPS_IDLE, FrameN, NARRAY(FrameN), "", 0};

/********************************************
 Property: CCD Binning
*********************************************/
enum { CCD_HBIN, CCD_VBIN };
 /* Binning */ 
 static INumber BinningN[]       = {
 { "HOR_BIN", "X", "%0.f", 1., 8, 1., 1., 0, 0, 0},
 { "VER_BIN", "Y", "%0.f", 1., 8, 1., 1., 0, 0, 0}};
 static INumberVectorProperty BinningNP = { MYDEV, "CCD_BINNING", "Binning", IMAGE_GROUP, IP_RW, 60, IPS_IDLE, BinningN, NARRAY(BinningN), "", 0};

/********************************************
 Property: Maximum CCD Values
*********************************************/
/* MaxValues parameter */
typedef enum {	/* N.B. order must match array below */
    EXP_MV, ROIW_MV, ROIH_MV, OSW_MV, OSH_MV, BINW_MV, BINH_MV,
    SHUTTER_MV, MINTEMP_MV, N_MV
} MaxValuesIndex;
static INumber  MaxValuesN[N_MV] = {
    {"ExpTime",  "Exposure time (s)",       "%8.2f", 1,50,1,1,0,0,0},
    {"ROIW",     "Imaging width",     "%4.0f", 1,50,1,1,0,0,0},
    {"ROIH",     "Imaging height",    "%4.0f", 1,50,1,1,0,0,0},
    {"OSW",      "Overscan width",    "%4.0f", 1,50,1,1,0,0,0},
    {"OSH",      "Overscan height",   "%4.0f", 1,50,1,1,0,0,0},
    {"BinW",     "Horizontal binning factor", "%4.0f", 1,8,1,1,0,0,0},
    {"BinH",     "Vertical binnng factor",    "%4.0f", 1,8,1,1,0,0,0},
    {"Shutter",  "1 if have shutter, else 0", "%2.0f", 0,1,1,1,0,0,0},
    {"MinTemp",  "Min cooler temp (C)",        "%5.1f", -20,20,1,1,0,0,0},
};
static INumberVectorProperty MaxValuesNP = {MYDEV, "MaxValues", "Maximum camera settings", IMAGE_GROUP, IP_RO, 0, IPS_IDLE, MaxValuesN, NARRAY(MaxValuesN), "", 0};

/********************************************
 Property: CCD Fan Control
*********************************************/

/* FanSpeed parameter */
typedef enum {	/* N.B. order must match array below */
    OFF_FS, SLOW_FS, MED_FS, FAST_FS, N_FS
} FanSpeedIndex;
static ISwitch FanSpeedS[N_FS] = {
    /* N.B. exactly one must be ISS_ON here to serve as our default */
    {"Off",        "Fans off",     ISS_OFF,0,0},
    {"Slow",       "Fans slow",    ISS_ON,0,0},
    {"Med",        "Fans medium",  ISS_OFF,0,0},
    {"Fast",       "Fans fast",    ISS_OFF,0,0},
};
static ISwitchVectorProperty FanSpeedSP = {MYDEV, "FanSpeed", "Set fans speed", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0., IPS_IDLE, FanSpeedS, NARRAY(FanSpeedS), "", 0};

/**********************************************************************************************/
/*********************************** GROUP: Data **********************************************/
/**********************************************************************************************/

/********************************************
 Property: CCD Image BLOB
*********************************************/

/* Pixels BLOB parameter */
typedef enum {	/* N.B. order must match array below */
    IMG_B, N_B
} PixelsIndex;
static IBLOB FitsB[N_B] = {{"FITS_BLOB", "FITS", ".fits", 0, 0, 0, 0, 0, 0, 0}};
static IBLOBVectorProperty FitsBP = {MYDEV, "CCD_FITS_BLOB", "BLOB", DATA_GROUP, IP_RO, 0, IPS_IDLE, FitsB, NARRAY(FitsB), "", 0};

/* Function prototypes */
static void getStartConditions(void);
static void expTO (void *vp);
static void coolerTO (void *vp);
static void addFITSKeywords(fitsfile *fptr);
static void uploadFile(const char* filename);
/*static void sendFITS (char *fits, int nfits);*/
static int camconnect(void);
static void reset_all_properties();

/* send client definitions of all properties */
void ISGetProperties (char const *dev)
{

        if (dev && strcmp (MYDEV, dev))
	    return;

	/* Communication Group */
	IDDefSwitch(&ConnectSP, NULL);
        IDDefSwitch(&PortSP, NULL);

}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

	if (strcmp (dev, MYDEV))
	    return;

        if (!strcmp(name, PortSP.name))
        {
            if (IUUpdateSwitch(&PortSP, states, names, n) < 0)
                            return;

            PortSP.s = IPS_OK;
            IDSetSwitch(&PortSP, NULL);
            return;
        }

	if (!strcmp(name, ConnectSP.name))
	{

		if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0)
				return;

		if (ConnectS[ON_S].s == ISS_ON)
		{
			if (!camconnect())
			{
				ConnectSP.s = IPS_OK;
				IDSetSwitch(&ConnectSP, "Apogee Alta is online.");
			}
			return;
		}
		else
		{
			reset_all_properties();
			IDSetSwitch(&ConnectSP, "Apogee Alta is offline.");
			return;
		}

		return;
	}		
	
	if (ConnectS[ON_S].s != ISS_ON)
	{
		IDMessage(MYDEV, "Apogee Alta is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

	if (!strcmp (name, FanSpeedSP.name)) 
	{
	    int i;

	   	for (i = 0; i < n; i++)
	    	{
			ISwitch *sp = IUFindSwitch(&FanSpeedSP, names[i]);
			if (sp && states[i] == ISS_ON) {
		    	char *smsg = NULL;
		    	int fs = 0;

		    	/* see which switch */
		    	fs = sp - FanSpeedS;
		    	switch (fs) 
			{
		    		case 0: smsg = "Fans shut off"; break;
		    		case 1: smsg = "Fans speed set to slow"; break;
		    		case 2: smsg = "Fans speed set to medium"; break;
		    		case 3: smsg = "Fans speed set to fast"; break;
		    	}

		    	/* install if reasonable */
		    	if (smsg)
			{
				ApnGlueSetFan (fs);
				IUResetSwitch (&FanSpeedSP);
				FanSpeedSP.sp[fs].s = ISS_ON;
				FanSpeedSP.s = IPS_OK;
                                IDSetSwitch (&FanSpeedSP, "%s", smsg);
		    	}

		    break;
		}
	    }
		return;
	}

	if (!strcmp(name, ShutterSP.name))
	{
		if (IUUpdateSwitch(&ShutterSP, states, names, n) < 0)
				return;
		
		ShutterSP.s = IPS_OK;
		IDSetSwitch(&ShutterSP, NULL);
	}

}

void ISNewNumber (const char * dev, const char *name, double *doubles, char *names[], int n)
{
	INDI_UNUSED(dev);

	if (ConnectS[ON_S].s != ISS_ON)
	{
		IDMessage(MYDEV, "Apogee Alta is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

	if (!strcmp (name, ExposureWNP.name))
	{
		if (IUUpdateNumber(&ExposureWNP, doubles, names, n) < 0)
			return;

	            if (ExposureWNP.s == IPS_BUSY)
		    {
			/* abort current exposure */
			if (expTID) 
			{
			    IERmTimer (expTID);
			    expTID = 0;
			} else
			    fprintf (stderr, "Hmm, BUSY but no expTID\n");

			ApnGlueExpAbort();
			ExposureWNP.s = IPS_IDLE;
			ExposureRNP.s = IPS_IDLE;
			ExposureRNP.np[0].value = 0;
			IDSetNumber (&ExposureWNP, "Exposure aborted");

		    } else 
		    {
			/* start new exposure with last ExpValues settings.
			 * ExpGo goes busy. set timer to read when done
			 */
			double expsec = ExposureWNP.np[0].value;
			int expms = (int)ceil(expsec*1000);
			int wantshutter = (ShutterS[0].s == ISS_ON) ? 1 : 0;

#ifndef SIMULATION
			if (ApnGlueStartExp (&expsec, wantshutter) < 0) 
			{
			    ExposureWNP.s = IPS_ALERT;
			    IDSetNumber (&ExposureWNP, "Error starting exposure");
			    return;
			}
#endif

			getStartConditions();

			ExposureRNP.np[0].value = expsec;

			expTID = IEAddTimer (expms, expTO, NULL);

			ExposureWNP.s = IPS_BUSY;
			IDSetNumber (&ExposureWNP, "Starting %g sec exp, %d x %d, shutter %s", expsec, impixw, impixh, wantshutter ? "open" : "closed");
		    }

		   return;
	    }

	if (!strcmp (name, ExposureSettingsNP.name) || !strcmp (name, FrameNP.name) || !strcmp (name, BinningNP.name))
	{
   	    int roiw, roih, osw, osh, binw, binh, roix, roiy;
	    char whynot[1024];
	    INumberVectorProperty *current_prop = NULL;

		if (!strcmp (name, ExposureSettingsNP.name))
		{
			if (IUUpdateNumber(&ExposureSettingsNP, doubles, names, n) < 0)
				return;
			else
				current_prop = &ExposureSettingsNP;
		}
		else if (!strcmp (name, FrameNP.name))
		{
			if (IUUpdateNumber(&FrameNP, doubles, names, n) < 0)
				return;
			else
				current_prop = &FrameNP;
		}
		else if (!strcmp (name, BinningNP.name))
		{
			if (IUUpdateNumber(&BinningNP, doubles, names, n) < 0)
				return;
			else
				current_prop = &BinningNP;
		}

		osw  = ExposureSettingsNP.np[OSW_EV].value;
		osh  = ExposureSettingsNP.np[OSH_EV].value;

		roix = FrameNP.np[CCD_X].value;
		roiy = FrameNP.np[CCD_Y].value;
		roiw = FrameNP.np[CCD_W].value;
		roih = FrameNP.np[CCD_H].value;

		binw = BinningNP.np[CCD_HBIN].value;
		binh = BinningNP.np[CCD_VBIN].value;

#ifndef SIMULATION
	   if (ApnGlueSetExpGeom (roiw, roih, osw, osh, binw, binh, roix, roiy, &impixw, &impixh, whynot) < 0) 
	   {
		current_prop->s = IPS_ALERT;
		IDSetNumber (current_prop, "Bad values: %s", whynot);
	    } else if (impixw*impixh /* TODO + CFITSIO FITS HEADER SIZE */ > sizeof(imbuf)) 
	    {
		current_prop->s = IPS_ALERT;
		IDSetNumber (current_prop, "No memory for %d x %d",impixw,impixh);
	    } else 
	    {
		current_prop->s = IPS_OK;
		IDSetNumber (current_prop, "New values accepted");
	    }
#else
                current_prop->s = IPS_OK;
                IDSetNumber (current_prop, "New values accepted");
#endif

	   return;
	}

 	if (!strcmp(name, TemperatureWNP.name))
	{

	    if (IUUpdateNumber(&TemperatureWNP, doubles, names, n) < 0)
		return;

	    double newt = TemperatureWNP.np[0].value;

#ifndef SIMULATION
	    ApnGlueSetTemp (newt);
#endif

	    /* let coolerTO loop update TemperatureRNP */

	    TemperatureWNP.s = IPS_BUSY;
	    IDSetNumber (&TemperatureWNP, "Set cooler target to %.1f", newt);

	    return;
	}
}

void
ISNewText (const char *dev, const char *name, char *texts[], char *names[],
int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(texts);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

void
ISNewBLOB (const char *dev, const char *name, int sizes[],
    int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

/* indiserver is sending us a message from a snooped device */
void
ISSnoopDevice (XMLEle *root)
{
INDI_UNUSED(root);
}

/* save conditions at start of exposure */
static void
getStartConditions()
{

	gettimeofday (&exp0, NULL);

}

/* called when exposure is expected to be complete
 * doesn't have to be timed perfectly.
 */
static void expTO (void *vp)
{
	INDI_UNUSED(vp);
	int npix = impixw*impixh;
	char whynot[1024];
	unsigned short *fits;
	int zero = 0;
        int i, fd, status,x,y;
       	char filename[TEMPFILE_LEN] = "/tmp/fitsXXXXXX";
	char filename_rw[TEMPFILE_LEN+1];
	fitsfile *fptr;       /* pointer to the FITS file; defined in fitsio.h */
  	long  fpixel = 1, naxis = 2;
  	long naxes[2] = {impixw,impixh};
  
	
	/* record we went off */
	expTID = 0;

	/* assert we are doing an exposure */
	if (ExposureWNP.s != IPS_BUSY) 
	{
	    fprintf (stderr, "Hmm, expTO but not exposing\n");
	    return;
	}

#ifndef SIMULATION

	/* wait for exp complete, to a point */
	for (i = 0; i < MAXEXPERR && !ApnGlueExpDone(); i++)
	    IEDeferLoop(200, &zero);

	if (i == MAXEXPERR) 
	{
	    /* something's wrong */
	    ApnGlueExpAbort();
	    ExposureWNP.s = IPS_ALERT;
	    ExposureRNP.s = IPS_ALERT;
	    IDSetNumber (&ExposureWNP, "Exposure never completed");
	    IDSetNumber (&ExposureRNP, NULL);
	    return;
	}

#endif

	if ((fd = mkstemp(filename)) < 0)
	{ 
		IDMessage(MYDEV, "Error making temporary filename.");
		IDLog("Error making temporary filename.\n");
		return;
	}
	close(fd);

	/* Append ! to over write any existing files */
	snprintf(filename_rw, TEMPFILE_LEN+1, "!%s", filename);

	/* read FitsBP as a FITS file */
	IDSetNumber (&ExposureWNP, "Reading %d FitsBP", npix);
	fits = imbuf;

#ifdef SIMULATION
        IDLog("Writing random data to fits image in simulation mode\n");
        for (x=0; x < impixh; x++)
            for (y=0; y < impixw; y++)
                fits[x*impixw + y] = rand()%255;
#else
	if (ApnGlueReadPixels (fits, npix, whynot) < 0) 
	{
	    /* can't get FitsBP */
	    ApnGlueExpAbort();
	    ExposureWNP.s = IPS_ALERT;
	    ExposureRNP.s = IPS_ALERT;
	    IDSetNumber (&ExposureWNP, "Error reading FitsBP: %s", whynot);
	    IDSetNumber (&ExposureRNP, NULL);
            return;
        }
#endif
	   /* ok */
	   fits_create_file(&fptr, filename_rw, &status);   /* create new file */

	   if (status)
	   {
		fits_report_error(stderr, status);  /* print out any error messages */
		return;
	   }

           /* Create the primary array image (16-bit unsigned short integer pixels) */
  	   fits_create_img(fptr, USHORT_IMG, naxis, naxes, &status);
	   if (status)
	   {
		fits_report_error(stderr, status);  /* print out any error messages */
		return;
	   }

         addFITSKeywords(fptr);

	  /* Write the array of integers to the image */
  	  fits_write_img(fptr, TUSHORT, fpixel, npix, fits, &status);
	  if (status)
	  {
		fits_report_error(stderr, status);  /* print out any error messages */
		return;
	  }

  	 fits_close_file(fptr, &status);            /* close the file */
	 if (status)
	 {
		fits_report_error(stderr, status);  /* print out any error messages */
		return;
	 }

         fits_report_error(stderr, status);  /* print out any error messages */
	 if (status)
	 {
		fits_report_error(stderr, status);  /* print out any error messages */
		return;
	 }

	ExposureWNP.s = IPS_OK;
	ExposureRNP.s = IPS_OK;
	IDSetNumber (&ExposureWNP, "Exposure complete, downloading FITS...");
	IDSetNumber (&ExposureRNP, NULL);

	uploadFile(filename);
  	unlink(filename);
	

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
     FitsBP.bp[IMG_B].blob = compressedData;
     FitsBP.bp[IMG_B].bloblen = compressedBytes;
     FitsBP.bp[IMG_B].size = totalBytes;
     strcpy(FitsBP.bp[IMG_B].format, ".fits.z");
     FitsBP.s = IPS_OK;
     IDSetBLOB (&FitsBP, NULL);
   
   free (fitsData);   
   free (compressedData);
   
}





/* hack together a FITS header for the current image
 * we use up exactly FHDRSZ bytes.
 */
void addFITSKeywords(fitsfile *fptr)
{

        int status=0;
        int bitpi=16;
        int naxis=2;
        int bscale=1;
        int bzero=32768;
        double expt = ExposureWN[0].value;
        double tempt = TemperatureRN[0].value;
        int binw = ExposureSettingsNP.np[0].value;
        int binh = ExposureSettingsNP.np[1].value;
        char *shtr = ShutterS[0].s == ISS_ON ? "'OPEN    '":"'CLOSED  '";
	double jd = 2440587.5 + exp0.tv_sec/3600./24.;
	char *sensor, *camera;
	char buf[1024];
	struct tm *tmp;

#ifndef SIMULATION
	ApnGlueGetName (&sensor, &camera);
#endif

        //fits_update_key(fptr, TSTRING,  "SIMPLE", simple, "", &status);
        fits_update_key(fptr, TINT,  "BITPI", &bitpi, "bit/pix", &status);
        fits_update_key(fptr, TINT,  "NAXIS",&naxis, "n image axes", &status);
        fits_update_key(fptr, TINT,  "NAXIS1", &impixw, "columns", &status);
        fits_update_key(fptr, TINT,  "NAXIS2", &impixh, "rows", &status);
        fits_update_key(fptr, TINT,  "BSCALE", &bscale, "v=p*BSCALE+BZERO", &status);
        fits_update_key(fptr, TINT,  "BZEROs", &bzero, "v=p*BSCALE+BZERO", &status);
        fits_update_key(fptr, TDOUBLE, "EXPTIME", &expt, "seconds", &status);
#ifndef SIMULATION
        fits_update_key(fptr,  TSTRING, "INSTRUME",camera,"instrument", &status);
        fits_update_key(fptr,  TSTRING, "DETECTOR",sensor," detector", &status);
#endif
        fits_update_key(fptr,  TDOUBLE, "CCDTEMP", &tempt, "deg C", &status);
        fits_update_key(fptr,  TINT, "CCDXBIN", &binw,"column binning", &status);
        fits_update_key(fptr,  TINT, "CCDYBIN", &binh, "row binning", &status);
        fits_update_key(fptr,  TSTRING, "SHUTTER", shtr,"shutter state", &status);

	tmp = gmtime (&exp0.tv_sec);
        fits_update_key(fptr, TSTRING, "TIMESYS", "'UTC     '", "time zone", &status);
        fits_update_key(fptr,  TDOUBLE, "JD", &jd, "JD at start", &status);
	sprintf (buf, "'%4d:%02d:%02d'", tmp->tm_year+1900, tmp->tm_mon+1,
							    tmp->tm_mday);
        fits_update_key(fptr, TSTRING, "DATE-OBS", buf, "Date at start", &status);

	sprintf (buf, "'%02d:%02d:%06.3f'", tmp->tm_hour, tmp->tm_min,
					tmp->tm_sec + exp0.tv_usec/1e6);
        fits_update_key(fptr, TSTRING, "TIME-OBS", buf, "Time at start", &status);

        /* some Telescope info, if sensible */
        /*
	if (ra0 || dec0) {
	    fs_sexa (buf, ra0, 4, 36000);
	    fits += sprintf(fits,"RA2K    = %-20s%-50s",buf," / RA J2K H:M:S");

	    fs_sexa (buf, dec0, 4, 36000);
	    fits += sprintf(fits,"DEC2K   = %-20s%-50s",buf," / Dec J2K D:M:S");

	    fs_sexa (buf, alt0, 4, 3600);
	    fits += sprintf(fits,"ALT     = %-20s%-50s",buf," / Alt D:M:S");

	    fs_sexa (buf, az0, 4, 3600);
	    fits += sprintf(fits,"AZ      = %-20s%-50s",buf," / Azimuth D:M:S");

	    fits += sprintf(fits,"AIRMASS = %20.3f%-50s", am0, " / Airmass");
	}
        */


	/* some env info, if sensible */
        /*
	if (hum0 || windd0) {
            fits_update_key(fptr,  "HUMIDITY= %20.3f%-50s", hum0,
					    " / exterior humidity, percent");
            fits_update_key(fptr,  "AIRTEMP = %20.3f%-50s", extt0,
					    " / exterior temp, deg C");
            fits_update_key(fptr,  "MIRRTEMP= %20.3f%-50s", mirrort0,
					    " / mirror temp, deg C");
            fits_update_key(fptr,  "WINDSPD = %20.3f%-50s", winds0,
					    " / wind speed, kph");
            fits_update_key(fptr,  "WINDDIR = %20.3f%-50s", windd0,
					    " / wind dir, degs E of N");
	}
        */
}

/* timer to read the cooler, repeats forever */
static void coolerTO (void *vp)
{
	INDI_UNUSED(vp);
	static int lasts = 9999;
        double cnow=TemperatureRN[0].value;
	int status;
	char *msg = NULL;

#ifndef SIMULATION
	status = ApnGlueGetTemp(&cnow);
#else
        status = 1;
        if (cnow > TemperatureWN[0].value)
            cnow--;
        else if (cnow < TemperatureWN[0].value)
            cnow++;
        else if (cnow == TemperatureWN[0].value)
            status = 2;
#endif

	switch (status) 
	{
	case 0:
	    TemperatureRNP.s = IPS_IDLE;
	    if (status != lasts)
		msg = "Cooler is now off";
	    break;

	case 1:
	    TemperatureRNP.s = IPS_BUSY;
	    if (status != lasts)
		msg = "Cooler is ramping to target";
	    break;

	case 2:
	    TemperatureRNP.s = IPS_OK;
            TemperatureWNP.s = IPS_OK;
            IDSetNumber(&TemperatureWNP, NULL);
	    if (status != lasts)
		msg = "Cooler is on target";
	    break;
	}

	TemperatureRNP.np[0].value = cnow;
        IDSetNumber (&TemperatureRNP, "%s", msg);

	lasts = status;

	IEAddTimer (COOLTM, coolerTO, NULL);
}

/* wait forever trying to open camera
 */
static int camconnect()
{
	int roiw, roih, osw, osh, binw, binh, shutter;
        double exptime, mintemp;
	char whynot[1024];

#ifndef SIMULATION
        unsigned int portConnection = (PortS[0].s == ISS_ON) ? APOGEE_USB_ONLY : APOGEE_ETH_ONLY;
        if (ApnGlueOpen(portConnection) < 0)
	{
	    IDLog ("Can not open camera: power ok? suid root?\n");
	    ConnectS[ON_S].s = ISS_OFF;
	    ConnectS[OFF_S].s = ISS_ON;
	    ConnectSP.s = IPS_ALERT;
	    IDSetSwitch(&ConnectSP, "Can not open camera: power ok? suid root?");
	    return -1;
	}

	/* get hardware max values */
	  ApnGlueGetMaxValues (&exptime, &roiw, &roih, &osw, &osh, &binw, &binh, &shutter, &mintemp);

	    MaxValuesNP.np[EXP_MV].value = exptime;
	    MaxValuesNP.np[ROIW_MV].value = roiw;
	    MaxValuesNP.np[ROIH_MV].value = roih;
	    MaxValuesNP.np[OSW_MV].value = osw;
	    MaxValuesNP.np[OSH_MV].value = osh;
	    MaxValuesNP.np[BINW_MV].value = binw;
	    MaxValuesNP.np[BINH_MV].value = binh;
	    MaxValuesNP.np[SHUTTER_MV].value = shutter;
	    MaxValuesNP.np[MINTEMP_MV].value = mintemp;

	    /* use max values to set up a default geometry */
	    ExposureRNP.np[0].value = 1.0;

	    FrameNP.np[CCD_X].value = 0;
	    FrameNP.np[CCD_Y].value = 0;
		
	    FrameNP.np[CCD_W].value = roiw;
	    FrameNP.np[CCD_H].value = roih;

	    BinningNP.np[CCD_HBIN].value = 1;
	    BinningNP.np[CCD_VBIN].value = 1;
 
	    ExposureSettingsNP.np[OSW_EV].value = 0;
	    ExposureSettingsNP.np[OSH_EV].value = 0;

	    if (ApnGlueSetExpGeom (roiw, roih, 0, 0, 1, 1, 0, 0, &impixw, &impixh, whynot) < 0)
	    {
 		ConnectS[ON_S].s = ISS_OFF;
	    	ConnectS[OFF_S].s = ISS_ON;
	    	ConnectSP.s = IPS_ALERT;
		IDLog ("Can't even set up %dx%d image geo: %s\n", roiw, roih, whynot);
	    	IDSetSwitch(&ConnectSP, "Can't even set up %dx%d image geo: %s\n", roiw, roih, whynot);
		return -1;
	    }

	    /* start cooler to our TemperatureWNP default */
	    ApnGlueSetTemp (TemperatureWNP.np[T_STEMP].value);

	    /* init and start cooler reading timer */
	    coolerTO(NULL);

	    /* init fans to our FanSpeedSP switch default */
	    ApnGlueSetFan (IUFindOnSwitch(&FanSpeedSP) - FanSpeedS);

#else
        /* init and start cooler reading timer */
        coolerTO(NULL);

        MaxValuesNP.np[ROIW_MV].value = 512;
        MaxValuesNP.np[ROIH_MV].value = 512;

        impixw=512;
        impixh=512;

        ExposureRNP.np[0].value = 1.0;

        FrameNP.np[CCD_X].value = 0;
        FrameNP.np[CCD_Y].value = 0;

        FrameNP.np[CCD_W].value = 512;
        FrameNP.np[CCD_H].value = 512;

        BinningNP.np[CCD_HBIN].value = 1;
        BinningNP.np[CCD_VBIN].value = 1;

        ExposureSettingsNP.np[OSW_EV].value = 0;
        ExposureSettingsNP.np[OSH_EV].value = 0;
#endif

	/* Expose Group */
	IDDefSwitch(&ShutterSP, NULL);
	IDDefNumber(&ExposureWNP, NULL);
	IDDefNumber(&ExposureRNP, NULL);

	IDDefNumber(&TemperatureWNP, NULL);
	IDDefNumber(&TemperatureRNP, NULL);

	/* Settings */
	IDDefNumber (&FrameNP, NULL);
	IDDefNumber (&BinningNP, NULL);
	IDDefNumber (&MaxValuesNP, NULL);
	IDDefNumber (&ExposureSettingsNP, NULL);
	IDDefSwitch (&FanSpeedSP, NULL);

	/* Data */
	IDDefBLOB(&FitsBP, NULL);

	return 0;

}

void reset_all_properties()
{
	ConnectSP.s		= IPS_IDLE;
	TemperatureWNP.s	= IPS_IDLE;
	TemperatureRNP.s	= IPS_IDLE;
	FrameNP.s		= IPS_IDLE;
	BinningNP.s		= IPS_IDLE;
	ExposureWNP.s		= IPS_IDLE;
	ExposureRNP.s		= IPS_IDLE;
	MaxValuesNP.s 		= IPS_IDLE;
	ExposureSettingsNP.s	= IPS_IDLE;
	FanSpeedSP.s		= IPS_IDLE;
	FitsBP.s		= IPS_IDLE;
	ShutterSP.s		= IPS_IDLE;
	
	IDSetSwitch(&ConnectSP, NULL);
	IDSetNumber(&TemperatureWNP, NULL);
	IDSetNumber(&TemperatureRNP, NULL);
	IDSetNumber(&FrameNP, NULL);
	IDSetNumber(&BinningNP, NULL);
	IDSetNumber(&ExposureWNP, NULL);
	IDSetNumber(&ExposureRNP, NULL);
	IDSetNumber(&MaxValuesNP, NULL);
	IDSetNumber(&ExposureSettingsNP, NULL);
	IDSetSwitch(&FanSpeedSP, NULL);
	IDSetBLOB(&FitsBP, NULL);
	IDSetSwitch(&ShutterSP, NULL);
}

