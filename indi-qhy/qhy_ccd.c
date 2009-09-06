/*******************************************************************************
  Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

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

#include "qhy5_driver.h"

/* operational info */
#define MYDEV "QHY5 CCD"				/* Device name we call ourselves */
#define MAX_PIXELS	5000			/* Maximum row of FitsBP */

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"
#define MOTION_GROUP	"Motion Control"
#define DATA_GROUP      "Data Channel"

#define	MAXEXPERR	10		/* max err in exp time we allow, secs */
#define	OPENDT		5		/* open retry delay, secs */

static int impixw, impixh;		/* image size, final binned FitsBP */
static int expTID;			/* exposure callback timer id, if any */
int last_failed = 0;

/* info when exposure started */
static struct timeval exp0;		/* when exp started */

static qhy5_driver *qhydrv = NULL;
fitsfile *fptr = NULL;       /* pointer to the FITS file; defined in fitsio.h */

/**********************************************************************************************/
/************************************ GROUP: Communication ************************************/
/**********************************************************************************************/

/********************************************
 Property: Connection
*********************************************/
enum { ON_S, OFF_S };
static ISwitch ConnectS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
ISwitchVectorProperty ConnectSP		= { MYDEV, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ConnectS, NARRAY(ConnectS), "", 0};

/**********************************************************************************************/
/*********************************** GROUP: Expose ********************************************/
/**********************************************************************************************/

/********************************************
 Property: CCD Exposure [READ]
*********************************************/

static INumber ExposureN[]    = {{ "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0., 36000., .5, 1., 0, 0, 0}};
static INumberVectorProperty ExposureNP = { MYDEV, "CCD_EXPOSURE", "Expose", EXPOSE_GROUP, IP_RW, 36000, IPS_IDLE, ExposureN, NARRAY(ExposureN), "", 0};

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
 Property: CCD Gain
*********************************************/
enum { CCD_GAIN };
 /* Binning */ 
 static INumber GainN[]       = {
 { "GAIN", "Gain", "%0.f", 1., 100, 1., 1., 0, 0, 0}};
 static INumberVectorProperty GainNP = { MYDEV, "CCD_GAIN", "Gain", IMAGE_GROUP, IP_RW, 60, IPS_IDLE, GainN, NARRAY(GainN), "", 0};

/********************************************
 Property: Maximum CCD Values
*********************************************/
/* MaxValues parameter */
typedef enum {	/* N.B. order must match array below */
    EXP_MV, ROIW_MV, ROIH_MV, BINW_MV, BINH_MV, GAIN_MV, N_MV
} MaxValuesIndex;
static INumber  MaxValuesN[N_MV] = {
    {"ExpTime",  "Exposure time (s)",       "%8.2f", 1,50,1,1,0,0,0},
    {"ROIW",     "Imaging width",     "%4.0f", 1,50,1,1,0,0,0},
    {"ROIH",     "Imaging height",    "%4.0f", 1,50,1,1,0,0,0},
    {"BinW",     "Horizontal binning factor", "%4.0f", 1,8,1,1,0,0,0},
    {"BinH",     "Vertical binnng factor",    "%4.0f", 1,8,1,1,0,0,0},
    {"Gain",     "Gain", "%4.0f", 1,100,1,1,0,0,0},
};
static INumberVectorProperty MaxValuesNP = {MYDEV, "MaxValues", "Maximum camera settings", IMAGE_GROUP, IP_RO, 0, IPS_IDLE, MaxValuesN, NARRAY(MaxValuesN), "", 0};


static int GuideNSTID = 0;
static int GuideWETID = 0;
/********************************************
 Property: Timed Guide movement. North/South
*********************************************/
static INumber GuideNSN[]       = {{"TIMED_GUIDE_N", "North (sec)", "%g", 0, 10, 0.001, 0, 0, 0}, {"TIMED_GUIDE_S", "South (sec)", "%g", 0, 10, 0.001, 0, 0, 0}};
INumberVectorProperty GuideNSNP      = { MYDEV, "TELESCOPE_TIMED_GUIDE_NS", "Guide North/South", MOTION_GROUP, IP_RW, 0, IPS_IDLE, GuideNSN, NARRAY(GuideNSN), "", 0};

/********************************************
 Property: Timed Guide movement. West/East
*********************************************/
static INumber GuideWEN[]       = {{"TIMED_GUIDE_W", "West (sec)", "%g", 0, 10, 0.001, 0, 0, 0}, {"TIMED_GUIDE_E", "East (sec)", "%g", 0, 10, 0.001, 0, 0, 0}};
INumberVectorProperty GuideWENP      = { MYDEV, "TELESCOPE_TIMED_GUIDE_WE", "Guide West/East", MOTION_GROUP, IP_RW, 0, IPS_IDLE, GuideWEN, NARRAY(GuideWEN), "", 0};

/********************************************
 Property: Compress FITS
*********************************************/
static ISwitch CompressS[]          	= {{"COMPRESS" , "Compress" , ISS_ON, 0, 0},{"RAW", "Raw", ISS_OFF, 0, 0}};
ISwitchVectorProperty CompressSP		= { MYDEV, "COMPRESSION" , "Compression", DATA_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, CompressS, NARRAY(CompressS), "", 0};
/**********************************************************************************************/

/********************************************
 Property: CCD Image BLOB
*********************************************/

/* Pixels BLOB parameter */
typedef enum {	/* N.B. order must match array below */
    IMG_B, N_B
} PixelsIndex;
static IBLOB FitsB[N_B] = {{"Img", "Image", ".fits", 0, 0, 0, 0, 0, 0, 0}};
static IBLOBVectorProperty FitsBP = {MYDEV, "Pixels", "Image data", DATA_GROUP, IP_RO, 0, IPS_IDLE, FitsB, NARRAY(FitsB), "", 0};

/* Function prototypes */
static void getStartConditions(void);
static void expTO (void *vp);
static void guideTimeout(void *p);
static void addFITSKeywords(fitsfile *fptr);
static void uploadFile(const void *memptr, size_t memsize);
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

	if (ConnectS[ON_S].s == ISS_ON) {
		IDDefNumber(&ExposureNP, NULL);

		/* Settings */
		IDDefNumber (&FrameNP, NULL);
		IDDefNumber (&BinningNP, NULL);
		IDDefNumber (&GainNP, NULL);
		IDDefNumber (&MaxValuesNP, NULL);
 
		/* Motion */ 
		IDDefNumber (&GuideNSNP, NULL );
		IDDefNumber (&GuideWENP, NULL );

		/* Data */
		IDDefSwitch(&CompressSP, NULL);
		IDDefBLOB(&FitsBP, NULL);
	}

}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

	if (strcmp (dev, MYDEV))
	    return;

	if (!strcmp(name, ConnectSP.name))
	{

		if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0)
				return;

		if (ConnectS[ON_S].s == ISS_ON)
		{
			if (!camconnect())
			{
				ConnectSP.s = IPS_OK;
				IDSetSwitch(&ConnectSP, "QHY5 is online.");
			}
			return;
		}
		else
		{
			reset_all_properties();
			IDSetSwitch(&ConnectSP, "QHY5 is offline.");
			return;
		}

		return;
	}		
	
	if (ConnectS[ON_S].s != ISS_ON)
	{
		IDMessage(MYDEV, "QHY5 is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

	if (!strcmp(name, CompressSP.name))
	{

		if (IUUpdateSwitch(&CompressSP, states, names, n) < 0)
				return;

		CompressSP.s = IPS_IDLE;
		IDSetSwitch(&CompressSP, NULL);
		return;
	}		

}

void ISNewNumber (const char * dev, const char *name, double *doubles, char *names[], int n)
{
	INDI_UNUSED(dev);

	if (ConnectS[ON_S].s != ISS_ON)
	{
		IDMessage(MYDEV, "QHY is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

	if (!strcmp (name, ExposureNP.name))
	{
		if (IUUpdateNumber(&ExposureNP, doubles, names, n) < 0)
			return;

	        if (ExposureNP.s == IPS_BUSY)
		{
			/* Already exposing, what can we do? */
			IDMessage(MYDEV, "QHY5 is already exposing.  Can't abort.");

		} else {
			/* start new exposure with last ExpValues settings.
			 * ExpGo goes busy. set timer to read when done
			 */
			double expsec = ExposureNP.np[0].value;
			int expms = (int)ceil(expsec*1000);

			if (qhy5_start_exposure(qhydrv, expms) < 0) 
			{
				ExposureNP.s = IPS_ALERT;
				IDSetNumber (&ExposureNP, "Error starting exposure");
				return;
			}

			getStartConditions();
			last_failed = 0;
			expTID = IEAddTimer (expms, expTO, NULL);

			ExposureNP.s = IPS_BUSY;
			IDSetNumber (&ExposureNP, "Starting %g sec exp, %d x %d", expsec, impixw, impixh);
		}

		return;
	}

	if (!strcmp (name, FrameNP.name) || !strcmp (name, BinningNP.name) || !strcmp(name, GainNP.name))
	{
		int roiw, roih, binw, binh, roix, roiy, gain;
		INumberVectorProperty *current_prop = NULL;

		if (!strcmp (name, FrameNP.name))
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
		else if (!strcmp (name, GainNP.name))
		{
			if (IUUpdateNumber(&GainNP, doubles, names, n) < 0)
				return;
			else
				current_prop = &GainNP;
		}

		roix = FrameNP.np[CCD_X].value;
		roiy = FrameNP.np[CCD_Y].value;
		roiw = FrameNP.np[CCD_W].value;
		roih = FrameNP.np[CCD_H].value;

		binw = BinningNP.np[CCD_HBIN].value;
		binh = BinningNP.np[CCD_VBIN].value;

		gain = GainNP.np[CCD_GAIN].value;

		if (qhy5_set_params(qhydrv, roiw, roih, binw, binh, roix, roiy, gain, &impixw, &impixh)) 
		{
			current_prop->s = IPS_ALERT;
			IDSetNumber (current_prop, "Bad values:");
		} else {
			current_prop->s = IPS_OK;
			IDSetNumber (current_prop, "New values accepted");
		}

		return;
	}
	if (!strcmp(name, GuideNSNP.name))
	{
		long direction;
		int duration_msec;
		int use_pulse_cmd;

		if (GuideNSNP.s == IPS_BUSY)
		{
			// Already guiding so stop before restarting timer
			qhy5_timed_move(qhydrv, QHY_NORTH, 0);
		}
		if (GuideNSTID) {
			IERmTimer(GuideNSTID);
			GuideNSTID = 0;
		}
		IUUpdateNumber(&GuideNSNP, doubles, names, n);

		if (GuideNSNP.np[0].value > 0) {
			duration_msec = GuideNSNP.np[0].value * 1000;
			direction = QHY_NORTH;
		} else {
			duration_msec = GuideNSNP.np[1].value * 1000;
			direction = QHY_SOUTH;
		}
		if (duration_msec <= 0) {
			GuideNSNP.s = IPS_IDLE;
			IDSetNumber (&GuideNSNP, NULL);
			return;
		}
		qhy5_timed_move(qhydrv, direction, duration_msec);
		GuideNSTID = IEAddTimer (duration_msec, guideTimeout, (void *)direction);
		GuideNSNP.s = IPS_BUSY;
		IDSetNumber(&GuideNSNP, NULL);
	}
	if (!strcmp(name, GuideWENP.name))
	{
		long direction;
		int duration_msec;
		int use_pulse_cmd;

		if (GuideWENP.s == IPS_BUSY)
		{
			// Already guiding so stop before restarting timer
			qhy5_timed_move(qhydrv, QHY_WEST, 0);
		}
		if (GuideWETID) {
			IERmTimer(GuideWETID);
			GuideWETID = 0;
		}
		IUUpdateNumber(&GuideWENP, doubles, names, n);

		if (GuideWENP.np[0].value > 0) {
			duration_msec = GuideWENP.np[0].value * 1000;
			direction = QHY_WEST;
		} else {
			duration_msec = GuideWENP.np[1].value * 1000;
			direction = QHY_EAST;
		}
		if (duration_msec <= 0) {
			GuideWENP.s = IPS_IDLE;
			IDSetNumber (&GuideWENP, NULL);
			return;
		}
		qhy5_timed_move(qhydrv, direction, duration_msec);
		GuideWETID = IEAddTimer (duration_msec, guideTimeout, (void *)direction);
		GuideWENP.s = IPS_BUSY;
		IDSetNumber(&GuideWENP, NULL);
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
	int binw = BinningNP.np[CCD_HBIN].value;
	int binh = BinningNP.np[CCD_VBIN].value;
	int zero = 0;
	int type = (binw * binh > 1) ? TUSHORT : TBYTE;
  	long  fpixel = 1, naxis = 2;
  	long naxes[2] = {impixw,impixh};
	unsigned short *fits;
	int i, fd, status = 0;
	void *memptr;
	size_t memsize;
  
	
	/* record we went off */
	expTID = 0;

	/* assert we are doing an exposure */
	if (ExposureNP.s != IPS_BUSY) 
	{
	    fprintf (stderr, "Hmm, expTO but not exposing\n");
	    return;
	}
	memsize = 5760; //impixw * impixh * (type == TBYTE ? 1 : 2) + 4096;
	IDLog("Reading exposure %d x %d (memsize: %lu)\n", impixw, impixh, (unsigned long)memsize);
	if (qhy5_read_exposure(qhydrv)) {
		double expsec = ExposureNP.np[0].value;
		int expms = (int)ceil(expsec*1000);
		int roix = FrameNP.np[CCD_X].value;
		int roiy = FrameNP.np[CCD_Y].value;
		int roiw = FrameNP.np[CCD_W].value;
		int roih = FrameNP.np[CCD_H].value;

		int binw = BinningNP.np[CCD_HBIN].value;
		int binh = BinningNP.np[CCD_VBIN].value;

		int gain = GainNP.np[CCD_GAIN].value;

		if (last_failed) {
		    IDLog("Error: Multiple exposure failures.  Giving up\n");
		    return;
		}
		last_failed = 1;
		IDLog("Error: Failed to read complete image.  Resetting camera and retrying\n");
		if (qhy5_set_params(qhydrv, roiw, roih, binw, binh, roix, roiy, gain, &impixw, &impixh))  {
		    IDLog("Error: Failed to reset camera\n");
		    return;
		}

		if (qhy5_start_exposure(qhydrv, expms) < 0) 
		{
		    ExposureNP.s = IPS_ALERT;
		    IDSetNumber (&ExposureNP, "Error starting exposure");
		    return;
		}


		expTID = IEAddTimer (expms, expTO, NULL);
		return;
	}

	if(! (memptr = malloc(memsize))) {
		IDLog("Error: Failed to allocate memorey: %lu\n", (unsigned long)memsize);
		return;
	}
	fits_create_memfile(&fptr, &memptr, &memsize, 2880, realloc, &status);
	if (status)
	{
		IDLog("Error: Failed to create memfile (memsize: %lu)\n", (unsigned long)memsize);
		fits_report_error(stderr, status);  /* print out any error messages */
		return;
	}
	IDLog("New memsize: %lu\n", (unsigned long)memsize);
	/* Create the primary array image (16-bit unsigned short integer pixels */
  	fits_create_img(fptr, (binw * binh > 1 ? USHORT_IMG : BYTE_IMG), naxis, naxes, &status);
	if (status)
	{
		IDLog("Error: Failed to create FITS image\n");
		fits_report_error(stderr, status);  /* print out any error messages */
		return;
	}
	for(i = 0; i < impixh; i ++)
	{
		void *ptr = qhy5_get_row(qhydrv, i);
		fits_write_img(fptr, type, (i * impixw) + 1, impixw, ptr, &status);
		if (status)
		{
			IDLog("Error: Failed to write FITS image\n");
			fits_report_error(stderr, status);  /* print out any error messages */
			return;
		}
	}
	//Should we use fits_close_file with a fits_mem_file?
  	fits_close_file(fptr, &status);            /* close the file */

	ExposureNP.s = IPS_OK;
	IDSetNumber (&ExposureNP, "Exposure complete, downloading FITS...");

	printf("size: %lu\n", (unsigned long)memsize);
	FILE *h = fopen("test.fits", "w+");
	fwrite(memptr, memsize, 1, h);
	fclose(h);
	uploadFile(memptr, memsize);
	free(memptr);
}

void guideTimeout(void *p)
{
    long direction = (long)p;

    if (direction == -1)
    {
	qhy5_timed_move(qhydrv, QHY_NORTH | QHY_EAST, 0);
	IERmTimer(GuideNSTID);
	IERmTimer(GuideWETID);
	
    }
    if (direction == QHY_NORTH || direction == QHY_SOUTH || direction == -1)
    {
        GuideNSNP.np[0].value = 0;
        GuideNSNP.np[1].value = 0;
	GuideNSNP.s = IPS_IDLE;
	GuideNSTID = 0;
	IDSetNumber(&GuideNSNP, NULL);
    }
    if (direction == QHY_WEST || direction == QHY_EAST || direction == -1)
    {
        GuideWENP.np[0].value = 0;
        GuideWENP.np[1].value = 0;
	GuideWENP.s = IPS_IDLE;
	GuideWETID = 0;
	IDSetNumber(&GuideWENP, NULL);
    }
}

void addFITSKeywords(fitsfile *fptr)
{
  int status=0;

  /* TODO add other data later */
  fits_write_date(fptr, &status);
}

void uploadFile(const void *fitsData, size_t totalBytes)
{
   unsigned char *compressedData;
   int r=0;
   unsigned int i =0, nr = 0;
   uLongf compressedBytes=0;
   struct stat stat_p; 

   if (CompressS[ON_S].s == ISS_ON)
   {
       compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
       compressedData = (unsigned char *) malloc (compressedBytes);
       
       if (fitsData == NULL || compressedData == NULL)
       {
         if (compressedData) free(compressedData);
         IDLog("Error! low memory. Unable to initialize fits buffers.\n");
         return;
       }
       
       /* #2 Compress it */ 
       r = compress2(compressedData, &compressedBytes, fitsData, totalBytes, 9);
       if (r != Z_OK)
       {
     	/* this should NEVER happen */
     	IDLog("internal error - compression failed: %d\n", r);
    	return;
       }
       FitsBP.bp[IMG_B].blob = compressedData;
       FitsBP.bp[IMG_B].bloblen = compressedBytes;
       strcpy(FitsBP.bp[IMG_B].format, ".fits.z");
   } else {
       compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
       FitsBP.bp[IMG_B].blob = realloc((unsigned char *)fitsData, compressedBytes);
       FitsBP.bp[IMG_B].bloblen = totalBytes;
       strcpy(FitsBP.bp[IMG_B].format, ".fits");
   }
   /* #3 Send it */
     FitsBP.bp[IMG_B].size = totalBytes;
     FitsBP.s = IPS_OK;
     IDSetBLOB (&FitsBP, NULL);
   
   free (compressedData);
}


/* wait forever trying to open camera
 */
static int camconnect()
{
	int roiw, roih, binw, binh, gain;
        double exptime;

	if (qhydrv)
		return 0;

	if (! (qhydrv = qhy5_open())) {
		IDLog ("Can not open camera: power ok?\n");
		ConnectS[ON_S].s = ISS_OFF;
		ConnectS[OFF_S].s = ISS_ON;
		ConnectSP.s = IPS_ALERT;
		IDSetSwitch(&ConnectSP, "Can not open camera: power ok?");
		return -1;
	}

	/* get hardware max values */
	qhy5_query_capabilities(qhydrv, &roiw, &roih, &binw, &binh, &gain);

	MaxValuesNP.np[EXP_MV].value = exptime;
	MaxValuesNP.np[ROIW_MV].value = roiw;
	MaxValuesNP.np[ROIH_MV].value = roih;
	MaxValuesNP.np[BINW_MV].value = binw;
	MaxValuesNP.np[BINH_MV].value = binh;
	MaxValuesNP.np[GAIN_MV].value = gain;

	/* use max values to set up a default geometry */

	FrameNP.np[CCD_X].value = 0;
	FrameNP.np[CCD_Y].value = 0;
		
	FrameNP.np[CCD_W].value = roiw;
	FrameNP.np[CCD_H].value = roih;

	BinningNP.np[CCD_HBIN].value = 1;
	BinningNP.np[CCD_VBIN].value = 1;

	GainNP.np[CCD_GAIN].value = gain > 50 ? 50 : gain;

	if (qhy5_set_params(qhydrv, roiw, roih, 1, 1, 0, 0, 50, &impixw, &impixh))
	{
		qhy5_close(qhydrv);
		qhydrv = NULL;
 		ConnectS[ON_S].s = ISS_OFF;
	    	ConnectS[OFF_S].s = ISS_ON;
	    	ConnectSP.s = IPS_ALERT;
		IDLog ("Can't even set up %dx%d image geometry\n", roiw, roih);
	    	IDSetSwitch(&ConnectSP, "Can't even set up %dx%d image geometry", roiw, roih);
		return -1;
	}

	/* Expose Group */
	IDDefNumber(&ExposureNP, NULL);

	/* Settings */
	IDDefNumber (&FrameNP, NULL);
	IDDefNumber (&BinningNP, NULL);
	IDDefNumber (&GainNP, NULL);
	IDDefNumber (&MaxValuesNP, NULL);

	/* Motion */ 
	IDDefNumber (&GuideNSNP, NULL );
	IDDefNumber (&GuideWENP, NULL );

	/* Data */
	IDDefSwitch(&CompressSP, NULL);
	IDDefBLOB(&FitsBP, NULL);

	return 0;

}

void reset_all_properties()
{
	ConnectSP.s		= IPS_IDLE;
	FrameNP.s		= IPS_IDLE;
	BinningNP.s		= IPS_IDLE;
	GainNP.s		= IPS_IDLE;
	ExposureNP.s		= IPS_IDLE;
	MaxValuesNP.s 		= IPS_IDLE;
	FitsBP.s		= IPS_IDLE;
	GuideNSNP.s		= IPS_IDLE;
	GuideWENP.s		= IPS_IDLE;
	
	qhy5_close(qhydrv);
	qhydrv = NULL;

	IDSetSwitch(&ConnectSP, NULL);
	IDSetNumber(&FrameNP, NULL);
	IDSetNumber(&BinningNP, NULL);
	IDSetNumber(&GainNP, NULL);
	IDSetNumber(&ExposureNP, NULL);
	IDSetNumber(&MaxValuesNP, NULL);
	IDSetNumber(&GuideNSNP, NULL);
	IDSetNumber(&GuideWENP, NULL);
	IDSetSwitch(&CompressSP, NULL);
	IDSetBLOB(&FitsBP, NULL);
}

