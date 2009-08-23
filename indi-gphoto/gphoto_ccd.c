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

#include "gphoto_driver.h"
#include "gphoto_readimage.h"

/* operational info */
#define MYDEV "GPHOTO DRIVER"			/* Device name we call ourselves */

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"
#define DATA_GROUP      "Data Channel"

#define	MAXEXPERR	10		/* max err in exp time we allow, secs */
#define	OPENDT		5		/* open retry delay, secs */

static int expTID;			/* exposure callback timer id, if any */

/* info when exposure started */
static struct timeval exp0;		/* when exp started */

static gphoto_driver *gphotodrv = NULL;

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
 Property: ISO
*********************************************/
static ISwitch *isoS;
static ISwitchVectorProperty isoSP = {MYDEV, "ISO", "ISO", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, NULL, 0, "", 0};

/********************************************
 Property: Capture Format
*********************************************/
static ISwitch *FormatS;
static ISwitchVectorProperty FormatSP = { MYDEV, "CAPTURE_FORMAT", "Capture Format", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, NULL, 0, "", 0};

/********************************************
 Property: Transfer Format
*********************************************/
enum { FITS_S, NATIVE_S };
static ISwitch TransferS[] = {
	{"FITS", "Fits", ISS_ON, 0, 0},
	{"NATIVE", "Native", ISS_OFF, 0, 0}
};
static ISwitchVectorProperty TransferSP = { MYDEV, "TRANSFER_FORMAT", "Transfer Format", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, TransferS, NARRAY(TransferS), "", 0};

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
static void addFITSKeywords(fitsfile *fptr);
static void uploadFile(const void *fitsData, size_t totalBytes, const char *ext);
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
		IDDefSwitch (&isoSP, NULL);
		IDDefSwitch (&FormatSP, NULL);
		IDDefSwitch (&TransferSP, NULL);

		/* Data */
		IDDefBLOB(&FitsBP, NULL);
	}

}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	int i;

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
				IDSetSwitch(&ConnectSP, "Gphoto driver is online.");
			}
			return;
		}
		else
		{
			reset_all_properties();
			IDSetSwitch(&ConnectSP, "gphoto driver is offline.");
			return;
		}

		return;
	}		
	
	if (ConnectS[ON_S].s != ISS_ON)
	{
		IDMessage(MYDEV, "Gphoto driver is offline. Please connect before issuing any commands.");
		reset_all_properties();
		return;
	}

	if (!strcmp(name, isoSP.name))
	{
		if (IUUpdateSwitch(&isoSP, states, names, n) < 0)
			return;

		for ( i = 0; i < isoSP.nsp; i++) {
			if (isoS[i].s == ISS_ON) {
				gphoto_set_iso(gphotodrv, i);
				isoSP.s = IPS_OK;
				IDSetSwitch(&isoSP, NULL);
				break;
			}
		}
	}

	if (!strcmp(name, FormatSP.name))
	{
		if (IUUpdateSwitch(&FormatSP, states, names, n) < 0)
			return;

		for ( i = 0; i < FormatSP.nsp; i++) {
			if (FormatS[i].s == ISS_ON) {
				gphoto_set_format(gphotodrv, i);
				FormatSP.s = IPS_OK;
				IDSetSwitch(&FormatSP, NULL);
				break;
			}
		}
	}
	if (!strcmp(name, TransferSP.name))
	{
		IUUpdateSwitch(&TransferSP, states, names, n);
		TransferSP.s = IPS_OK;
		IDSetSwitch(&TransferSP, NULL);
	}
}

void ISNewNumber (const char * dev, const char *name, double *doubles, char *names[], int n)
{
	INDI_UNUSED(dev);

	if (ConnectS[ON_S].s != ISS_ON)
	{
		IDMessage(MYDEV, "Gphoto driver is offline. Please connect before issuing any commands.");
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
			IDMessage(MYDEV, "Gphoto driver is already exposing.  Can't abort.");

		} else {
			/* start new exposure with last ExpValues settings.
			 * ExpGo goes busy. set timer to read when done
			 */
			double expsec = ExposureNP.np[0].value;
			int expms = (int)ceil(expsec*1000);

			if (gphoto_start_exposure(gphotodrv, expms) < 0) 
			{
				ExposureNP.s = IPS_ALERT;
				IDSetNumber (&ExposureNP, "Error starting exposure");
				return;
			}

			getStartConditions();

			expTID = IEAddTimer (expms, expTO, NULL);

			ExposureNP.s = IPS_BUSY;
			IDSetNumber (&ExposureNP, "Starting %g sec exposure", expsec);
		}

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
	int zero = 0;
	int type = TUSHORT;
  	long  fpixel = 1, naxis = 2;
  	long naxes[2];
	char ext[16];
	unsigned short *fits;
	int i, fd;
	void *memptr = NULL;
	size_t memsize;
  
	
	/* record we went off */
	expTID = 0;

	/* assert we are doing an exposure */
	if (ExposureNP.s != IPS_BUSY) 
	{
	    fprintf (stderr, "Hmm, expTO but not exposing\n");
	    return;
	}


	if(TransferS[FITS_S].s == ISS_ON) {
		int fd;
		FILE *rawFH;
		char tmpfile[] = "/tmp/indi_XXXXXX";
		char cmd[256], tmpstr[256];
		char *buffer;
		int impixw, impixh;
		// Convert to FITS

		//dcraw can't read from stdin, so we need to write to disk then read it back
		fd = mkstemp(tmpfile);
		gphoto_read_exposure_fd(gphotodrv, fd);
		if (fd == -1) {
			IDLog("gphoto can't write to disk\n");
			unlink(tmpfile);
			// Still need to read exposure, even though we can't send it
			return;
		}
		if(strcasecmp(gphoto_get_file_extension(gphotodrv), "jpg") == 0 ||
		   strcasecmp(gphoto_get_file_extension(gphotodrv), "jpeg") == 0)
		{
			if (read_jpeg(tmpfile, &memptr, &memsize)) {
				IDLog("gphoto failed to read image from dcraw\n");
				unlink(tmpfile);
				return;
			}
		} else {
			if (read_dcraw(tmpfile, &memptr, &memsize)) {
				IDLog("gphoto failed to read image from dcraw\n");
				unlink(tmpfile);
				return;
			}
		}
		unlink(tmpfile);
		strcpy(ext, "fits");
	} else {
		gphoto_read_exposure(gphotodrv);
		gphoto_get_buffer(gphotodrv, (const char **)&memptr, &memsize);
		strcpy(ext, gphoto_get_file_extension(gphotodrv));
	}
	ExposureNP.s = IPS_OK;
	IDSetNumber (&ExposureNP, "Exposure complete, downloading image...");

	printf("size: %d\n", memsize);
	FILE *h = fopen("test.fits", "w+");
	fwrite(memptr, memsize, 1, h);
	fclose(h);
	uploadFile(memptr, memsize, ext);
	if(memptr)
		free(memptr);
}

void addFITSKeywords(fitsfile *fptr)
{
  int status=0;

  /* TODO add other data later */
  fits_write_date(fptr, &status);
}

void uploadFile(const void *fitsData, size_t totalBytes, const char *ext)
{
   unsigned char *compressedData;
   int r=0;
   unsigned int i =0, nr = 0;
   uLongf compressedBytes=0;
   struct stat stat_p; 

   
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
   
   /* #3 Send it */
     FitsBP.bp[IMG_B].blob = compressedData;
     FitsBP.bp[IMG_B].bloblen = compressedBytes;
     FitsBP.bp[IMG_B].size = totalBytes;
     sprintf(FitsBP.bp[IMG_B].format, ".%s.z", ext);
     FitsBP.s = IPS_OK;
     IDSetBLOB (&FitsBP, NULL);
   
   free (compressedData);
}

ISwitch *create_switch(const char *basestr, const char **options, int max_opts, int setidx)
{
	int i;
	ISwitch *sw = calloc(sizeof(ISwitch), max_opts);
	for(i = 0; i < max_opts; i++) {
		sprintf(sw[i].name, "%s%d", basestr, i);
		strcpy(sw[i].label, options[i]);
		sw[i].s = (i == setidx) ? ISS_ON : ISS_OFF;
	}
	return sw;
}

/* wait forever trying to open camera
 */
static int camconnect()
{
	int setidx;
	const char **options;
	int max_opts;

	if (gphotodrv)
		return 0;

	if (! (gphotodrv = gphoto_open())) {
		IDLog ("Can not open camera: power ok?\n");
		ConnectS[ON_S].s = ISS_OFF;
		ConnectS[OFF_S].s = ISS_ON;
		ConnectSP.s = IPS_ALERT;
		IDSetSwitch(&ConnectSP, "Can not open camera: power ok?");
		return -1;
	}

	if (FormatS)
		free(FormatS);
	setidx = gphoto_get_format_current(gphotodrv);
	options = gphoto_get_formats(gphotodrv, &max_opts);
	IDLog("Setting %d options\n", max_opts);
	FormatS = create_switch("FORMAT", options, max_opts, setidx);
	FormatSP.sp = FormatS;
	FormatSP.nsp = max_opts;

	if (isoS)
		free(isoS);
	setidx = gphoto_get_iso_current(gphotodrv);
	options = gphoto_get_iso(gphotodrv, &max_opts);
	isoS = create_switch("ISO", options, max_opts, setidx);
	isoSP.sp = isoS;
	isoSP.nsp = max_opts;

	/* Expose Group */
	IDDefNumber(&ExposureNP, NULL);

	/* Settings */
	IDDefSwitch (&isoSP, NULL);
	IDDefSwitch (&FormatSP, NULL);
	IDDefSwitch (&TransferSP, NULL);

	/* Data */
	IDDefBLOB(&FitsBP, NULL);
	return 0;

}

void reset_all_properties()
{
	ConnectSP.s		= IPS_IDLE;
	isoSP.s			= IPS_IDLE;
	FormatSP.s		= IPS_IDLE;
	TransferSP.s		= IPS_IDLE;
	ExposureNP.s		= IPS_IDLE;
	FitsBP.s		= IPS_IDLE;

	gphoto_close(gphotodrv);	
	gphotodrv = NULL;

	IDSetSwitch(&ConnectSP, NULL);
	IDSetSwitch(&isoSP, NULL);
	IDSetSwitch(&FormatSP, NULL);
	IDSetSwitch(&TransferSP, NULL);
	IDSetNumber(&ExposureNP, NULL);
	IDSetBLOB(&FitsBP, NULL);
}

