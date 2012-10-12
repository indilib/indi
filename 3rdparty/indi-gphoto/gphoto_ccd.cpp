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

extern "C" {
	#include "gphoto_driver.h"
	#include "gphoto_readimage.h"
}

#include "gphoto_ccd.h"

//auto_ptr<GPhotoCam> gphoto_cam(0);
GPhotoCam *gphoto_cam = NULL;

static void CamInit()
{
	//if(gphoto_cam.get() == 0) gphoto_cam.reset(new GPhotoCam());
	if(! gphoto_cam)
		gphoto_cam = new GPhotoCam();
}

//==========================================================================
/* send client definitions of all properties */
void
ISGetProperties (char const *dev)
{

        if (dev && strcmp (MYDEV, dev))
	    return;
	CamInit();
	gphoto_cam->ISGetProperties();
	/* Communication Group */
}

void
ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
        if (dev && strcmp (MYDEV, dev))
	    return;
	CamInit();
	gphoto_cam->ISNewSwitch(name, states, names, n);
}

void
ISNewNumber (const char * dev, const char *name, double *doubles, char *names[], int n)
{
        if (dev && strcmp (MYDEV, dev))
	    return;
	CamInit();
	gphoto_cam->ISNewNumber(name, doubles, names, n);
}

void
ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
        if (dev && strcmp (MYDEV, dev))
	    return;
	CamInit();
	gphoto_cam->ISNewText(name, texts, names, n);
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

//==========================================================================

GPhotoCam::GPhotoCam()
{
	gphotodrv = NULL;
	on_off[0] = strdup("On");
	on_off[1] = strdup("Off");
	InitVars();
}

GPhotoCam::~GPhotoCam()
{
	free(on_off[0]);
	free(on_off[1]);
	expTID = 0;
}

void GPhotoCam::ISGetProperties(void)
{
	IDDefSwitch(&mConnectSP, NULL);
	IDDefText(&mPortTP, NULL);
	IDDefSwitch(&mExtendedSP, NULL);

	if (mConnectS[ON_S].s == ISS_ON) {
		IDDefNumber(&mExposureNP, NULL);

		/* Settings */
		IDDefSwitch (&mIsoSP, NULL);
		IDDefSwitch (&mFormatSP, NULL);
		IDDefSwitch (&mTransferSP, NULL);

		/* Data */
		IDDefBLOB(&mFitsBP, NULL);
	}

}

void GPhotoCam::ISNewSwitch (const char *name, ISState *states, char *names[], int n)
{
	int i;

	if (!strcmp(name, mConnectSP.name))
	{

		if (IUUpdateSwitch(&mConnectSP, states, names, n) < 0)
				return;

		if (mConnectS[ON_S].s == ISS_ON)
		{
			if (!Connect())
			{
				mConnectSP.s = IPS_OK;
				IDSetSwitch(&mConnectSP, "Gphoto driver is online.");
			}
			return;
		}
		else
		{
			Reset();
			IDSetSwitch(&mConnectSP, "gphoto driver is offline.");
			return;
		}

		return;
	}		
	if (!strcmp(name, mExtendedSP.name))
	{
		if (IUUpdateSwitch(&mExtendedSP, states, names, n) < 0)
				return;
		if (mConnectS[ON_S].s == ISS_ON)
		{
			if (mExtendedS[ON_S].s == ISS_ON)
			{
				if (! optTID)
				{
					ShowExtendedOptions();
				} else {
					UpdateExtendedOptions(true);
				}
			} else {
				HideExtendedOptions();
			}
		}
		IDSetSwitch(&mExtendedSP, NULL);
		return;
	}
	
	if (mConnectS[ON_S].s != ISS_ON)
	{
		IDMessage(MYDEV, "Gphoto driver is offline. Please connect before issuing any commands.");
		Reset();
		return;
	}

	if (!strcmp(name, mIsoSP.name))
	{
		if (IUUpdateSwitch(&mIsoSP, states, names, n) < 0)
			return;

		for ( i = 0; i < mIsoSP.nsp; i++) {
			if (mIsoS[i].s == ISS_ON) {
				gphoto_set_iso(gphotodrv, i);
				mIsoSP.s = IPS_OK;
				IDSetSwitch(&mIsoSP, NULL);
				break;
			}
		}
	}

	if (!strcmp(name, mFormatSP.name))
	{
		if (IUUpdateSwitch(&mFormatSP, states, names, n) < 0)
			return;

		for ( i = 0; i < mFormatSP.nsp; i++) {
			if (mFormatS[i].s == ISS_ON) {
				gphoto_set_format(gphotodrv, i);
				mFormatSP.s = IPS_OK;
				IDSetSwitch(&mFormatSP, NULL);
				break;
			}
		}
	}
	if (!strcmp(name, mTransferSP.name))
	{
		IUUpdateSwitch(&mTransferSP, states, names, n);
		mTransferSP.s = IPS_OK;
		IDSetSwitch(&mTransferSP, NULL);
	}
	if(CamOptions.find(name) != CamOptions.end())
	{
		cam_opt *opt = CamOptions[name];
		if (opt->widget->type != GP_WIDGET_RADIO
		    && opt->widget->type != GP_WIDGET_MENU
		    && opt->widget->type != GP_WIDGET_TOGGLE)
		{
			IDMessage(MYDEV, "ERROR: Property '%s'is not a switch (%d)", name, opt->widget->type);
			return;
		}
		if (opt->widget->readonly) {
			IDSetSwitch(&opt->prop.sw, "WARNING: Property %s is read-only", name);
			return;
		}
		if (IUUpdateSwitch(&opt->prop.sw, states, names, n) < 0)
			return;
		if (opt->widget->type == GP_WIDGET_TOGGLE) {
			gphoto_set_widget_num(gphotodrv, opt->widget, opt->item.sw[ON_S].s == ISS_ON);
		} else {
			for ( i = 0; i < opt->prop.sw.nsp; i++) {
				if (opt->item.sw[i].s == ISS_ON) {
					gphoto_set_widget_num(gphotodrv, opt->widget, i);
					break;
				}
			}
		}
		opt->prop.sw.s = IPS_OK;
		IDSetSwitch(&opt->prop.sw, NULL);
	}
}

void
GPhotoCam::ISNewNumber (const char *name, double *doubles, char *names[], int n)
{
	if (mConnectS[ON_S].s != ISS_ON)
	{
		IDMessage(MYDEV, "Gphoto driver is offline. Please connect before issuing any commands.");
		Reset();
		return;
	}

	if (!strcmp (name, mExposureNP.name))
	{
		if (IUUpdateNumber(&mExposureNP, doubles, names, n) < 0)
			return;

		if (mExposureNP.s == IPS_BUSY)
		{
			/* Already exposing, what can we do? */
			IDMessage(MYDEV, "Gphoto driver is already exposing.  Can't abort.");

		} else {
			/* start new exposure with last ExpValues settings.
			 * ExpGo goes busy. set timer to read when done
			 */
			double expsec = mExposureNP.np[0].value;
			int expms = (int)ceil(expsec*1000);

			if (gphoto_start_exposure(gphotodrv, expms) < 0) 
			{
				mExposureNP.s = IPS_ALERT;
				IDSetNumber (&mExposureNP, "Error starting exposure");
				return;
			}

			getStartConditions();

			expTID = IEAddTimer (expms, GPhotoCam::ExposureUpdate, this);

			mExposureNP.s = IPS_BUSY;
			IDSetNumber (&mExposureNP, "Starting %g sec exposure", expsec);
		}

		return;
	}
	if(CamOptions.find(name) != CamOptions.end())
	{
		cam_opt *opt = CamOptions[name];
		if (opt->widget->type != GP_WIDGET_RANGE)
		{
			IDMessage(MYDEV, "ERROR: Property '%s'is not a number", name);
			return;
		}
		if (opt->widget->readonly) {
			IDSetNumber(&opt->prop.num, "WARNING: Property %s is read-only", name);
			return;
		}
		if (IUUpdateNumber(&opt->prop.num, doubles, names, n) < 0)
			return;
		gphoto_set_widget_num(gphotodrv, opt->widget, doubles[0]);
		opt->prop.num.s = IPS_OK;
		IDSetNumber(&opt->prop.num, NULL);
	}

}

void
GPhotoCam::ISNewText (const char *name, char *texts[], char *names[], int n)
{
	IText *tp;

	// suppress warning
	n=n;

	if (!strcmp(name, mPortTP.name) )
	{
	  mPortTP.s = IPS_OK;
	  tp = IUFindText( &mPortTP, names[0] );
	  if (!tp)
	   return;

	  IUSaveText(&mPortTP.tp[0], texts[0]);
	  IDSetText (&mPortTP, NULL);
	  return;
	}
	if(CamOptions.find(name) != CamOptions.end())
	{
		cam_opt *opt = CamOptions[name];
		if (opt->widget->type != GP_WIDGET_TEXT)
		{
			IDMessage(MYDEV, "ERROR: Property '%s'is not a string", name);
			return;
		}
		if (opt->widget->readonly) {
			IDSetText(&opt->prop.text, "WARNING: Property %s is read-only", name);
			return;
		}
		if (IUUpdateText(&opt->prop.text, texts, names, n) < 0)
			return;
		gphoto_set_widget_text(gphotodrv, opt->widget, texts[0]);
		opt->prop.num.s = IPS_OK;
		IDSetText(&opt->prop.text, NULL);
	}
}
void    
GPhotoCam::UpdateExtendedOptions (void *p)
{
        GPhotoCam *cam = (GPhotoCam *)p;
        cam->UpdateExtendedOptions();
}               
void
GPhotoCam::UpdateExtendedOptions (bool force)
{
	map<string, cam_opt *>::iterator it;
	if(! expTID) {
		for ( it = CamOptions.begin() ; it != CamOptions.end(); it++ )
		{
			cam_opt *opt = (*it).second;
			if(force || gphoto_widget_changed(opt->widget)) {
				gphoto_read_widget(opt->widget);
				UpdateWidget(opt);
			}
		}
	}
	optTID = IEAddTimer (1000, GPhotoCam::UpdateExtendedOptions, this);
}

/* save conditions at start of exposure */
void
GPhotoCam::getStartConditions()
{

	gettimeofday (&exp0, NULL);

}

/* called when exposure is expected to be complete
 * doesn't have to be timed perfectly.
 */
void
GPhotoCam::ExposureUpdate (void *p)
{
	GPhotoCam *cam = (GPhotoCam *)p;
	cam->ExposureUpdate();
}
void GPhotoCam::ExposureUpdate (void)
{
	char ext[16];
	void *memptr = NULL;
	size_t memsize;
  
	
	/* record we went off */
	expTID = 0;

	/* assert we are doing an exposure */
	if (mExposureNP.s != IPS_BUSY) 
	{
	    fprintf (stderr, "Hmm, expTO but not exposing\n");
	    return;
	}

	if(mTransferS[FITS_S].s == ISS_ON || mTransferS[ZFITS_S].s == ISS_ON) {
		int fd;
		char tmpfile[] = "/tmp/indi_XXXXXX";
		// Convert to FITS

		//dcraw can't read from stdin, so we need to write to disk then read it back
		fd = mkstemp(tmpfile);
		gphoto_read_exposure_fd(gphotodrv, fd);
		if (fd == -1) {
			mExposureNP.s = IPS_ALERT;
			IDSetNumber (&mExposureNP, "Exposure failed to save image...");
			IDLog("gphoto can't write to disk\n");
			unlink(tmpfile);
			// Still need to read exposure, even though we can't send it
			return;
		}
		if(strcasecmp(gphoto_get_file_extension(gphotodrv), "jpg") == 0 ||
		   strcasecmp(gphoto_get_file_extension(gphotodrv), "jpeg") == 0)
		{
			if (read_jpeg(tmpfile, &memptr, &memsize)) {
				mExposureNP.s = IPS_ALERT;
				IDSetNumber (&mExposureNP, "Exposure failed to parse jpeg.");
				IDLog("gphoto failed to read jpeg image\n");
				unlink(tmpfile);
				return;
			}
		} else {
			if (read_dcraw(tmpfile, &memptr, &memsize)) {
				mExposureNP.s = IPS_ALERT;
				IDSetNumber (&mExposureNP, "Exposure failed to parse raw image.");
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
	IDSetNumber (&mExposureNP, "Exposure complete, downloading image...");

	printf("size: %lu\n", (unsigned long)memsize);
#ifdef DEBUG_FITS
	FILE *h = fopen("test.fits", "w+");
	fwrite(memptr, memsize, 1, h);
	fclose(h);
#endif
	uploadFile(memptr, memsize, ext);
	mExposureNP.s = IPS_OK;
	IDSetNumber (&mExposureNP, NULL);
	if(memptr) {
		if(mTransferS[FITS_S].s == ISS_ON || mTransferS[ZFITS_S].s == ISS_ON)
			free(memptr);
		else
			gphoto_free_buffer(gphotodrv);
	}
}

void
GPhotoCam::uploadFile(const void *fitsData, size_t totalBytes, const char *ext)
{
	unsigned char *compressedData = NULL;
	int r=0;
	uLongf compressedBytes=0;

	if (mTransferS[ZFITS_S].s == ISS_ON)
	{
		compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
		compressedData = (unsigned char *) malloc (compressedBytes);
 
		if (fitsData == NULL || compressedData == NULL)
		{
			if (compressedData) free(compressedData);
			IDMessage(MYDEV, "Error: Ran out of memory compressing image\n");
			IDLog("Error! low memory. Unable to initialize fits buffers.\n");
			return;
		}
       
		/* #2 Compress it */ 
		r = compress2(compressedData, &compressedBytes, (const Bytef*)fitsData, totalBytes, 9);
		if (r != Z_OK)
		{
			/* this should NEVER happen */
			IDMessage(MYDEV, "Error: Failed to compress image\n");
			IDLog("internal error - compression failed: %d\n", r);
			return;
		}
		mFitsBP.bp[IMG_B].blob = compressedData;
		mFitsBP.bp[IMG_B].bloblen = compressedBytes;
		sprintf(mFitsBP.bp[IMG_B].format, ".%s.z", ext);
	} else {
		mFitsBP.bp[IMG_B].blob = (unsigned char *)fitsData;
		mFitsBP.bp[IMG_B].bloblen = totalBytes;
		sprintf(mFitsBP.bp[IMG_B].format, ".%s", ext);
	}
	/* #3 Send it */
	mFitsBP.bp[IMG_B].size = totalBytes;
	mFitsBP.s = IPS_OK;
	IDSetBLOB (&mFitsBP, NULL);

	if (compressedData)
		free (compressedData);
}

ISwitch *
GPhotoCam::create_switch(const char *basestr, const char **options, int max_opts, int setidx)
{
	int i;
	ISwitch *sw = (ISwitch *)calloc(sizeof(ISwitch), max_opts);
	for(i = 0; i < max_opts; i++) {
		sprintf(sw[i].name, "%s%d", basestr, i);
		strcpy(sw[i].label, options[i]);
		sw[i].s = (i == setidx) ? ISS_ON : ISS_OFF;
	}
	return sw;
}

void
GPhotoCam::UpdateWidget(cam_opt *opt)
{
	struct tm *tm;

	switch(opt->widget->type) {
	case GP_WIDGET_RADIO:
	case GP_WIDGET_MENU:
		for (int i = 0; i < opt->widget->choice_cnt; i++)
			opt->item.sw[i].s = opt->widget->value.index == i ? ISS_ON : ISS_OFF;
		IDSetSwitch(&opt->prop.sw, NULL);
		break;
	case GP_WIDGET_TEXT:
		free(opt->item.text.text);
		opt->item.text.text = strdup(opt->widget->value.text);
		IDSetText(&opt->prop.text, NULL);
		break;
	case GP_WIDGET_TOGGLE:
		if(opt->widget->value.toggle) {
			opt->item.sw[0].s = ISS_ON;
			opt->item.sw[0].s = ISS_OFF;
		} else {
			opt->item.sw[0].s = ISS_OFF;
			opt->item.sw[0].s = ISS_ON;
		}
		IDSetSwitch(&opt->prop.sw, NULL);
		break;
	case GP_WIDGET_RANGE:
		opt->item.num.value = opt->widget->value.num;
		IDSetNumber(&opt->prop.num, NULL);
		break;
	case GP_WIDGET_DATE:
		free(opt->item.text.text);
		tm = gmtime((time_t *)&opt->widget->value.date);
		opt->item.text.text = strdup(asctime(tm));
		IDSetText(&opt->prop.text, NULL);
		break;
	default:
		delete opt;
		return;
	}
}

void
GPhotoCam::AddWidget(gphoto_widget *widget)
{
	IPerm perm;
	struct tm *tm;

	if(! widget)
		return;

	perm = widget->readonly ? IP_RO : IP_RW;

	cam_opt *opt = new cam_opt();
	opt->widget = widget;

	switch(widget->type) {
	case GP_WIDGET_RADIO:
	case GP_WIDGET_MENU:
		opt->item.sw = create_switch(widget->name, widget->choices, widget->choice_cnt, widget->value.index);
		IUFillSwitchVector(&opt->prop.sw, opt->item.sw, widget->choice_cnt, MYDEV,
			widget->name, widget->name, widget->parent, perm, ISR_1OFMANY, 60, IPS_IDLE);
		IDDefSwitch(&opt->prop.sw, NULL);
		break;
	case GP_WIDGET_TEXT:
		IUFillText(&opt->item.text, widget->name, widget->name, widget->value.text);
		IUFillTextVector(&opt->prop.text, &opt->item.text, 1, MYDEV,
			widget->name, widget->name, widget->parent, perm, 60, IPS_IDLE);
		IDDefText(&opt->prop.text, NULL);
		break;
	case GP_WIDGET_TOGGLE:
		opt->item.sw = create_switch(widget->name, (const char **)on_off, 2, widget->value.toggle ? 0 : 1);
		IUFillSwitchVector(&opt->prop.sw, opt->item.sw, 2, MYDEV,
			widget->name, widget->name, widget->parent, perm, ISR_1OFMANY, 60, IPS_IDLE);
		IDDefSwitch(&opt->prop.sw, NULL);
		break;
	case GP_WIDGET_RANGE:
		IUFillNumber(&opt->item.num, widget->name, widget->name, "%5.2f",
			widget->min, widget->max, widget->step, widget->value.num);
		IUFillNumberVector(&opt->prop.num, &opt->item.num, 1, MYDEV,
			widget->name, widget->name, widget->parent, perm, 60, IPS_IDLE);
		break;
	case GP_WIDGET_DATE:
		tm = gmtime((time_t *)&widget->value.date);
		IUFillText(&opt->item.text, widget->name, widget->name, asctime(tm));
		IUFillTextVector(&opt->prop.text, &opt->item.text, 1, MYDEV,
			widget->name, widget->name, widget->parent, perm, 60, IPS_IDLE);
		IDDefText(&opt->prop.text, NULL);
		break;
	default:
		delete opt;
		return;
	}
	
	CamOptions[widget->name] = opt;
	
}

void
GPhotoCam::ShowExtendedOptions(void)
{
	gphoto_widget_list *iter;

	iter = gphoto_find_all_widgets(gphotodrv);
	while(iter) {
		gphoto_widget *widget = gphoto_get_widget_info(gphotodrv, &iter);
		AddWidget(widget);
	}
	optTID = IEAddTimer (1000, GPhotoCam::UpdateExtendedOptions, this);
}

void
GPhotoCam::HideExtendedOptions(void)
{
	if (optTID) {
		IERmTimer(optTID);
		optTID = 0;
	}

	while (CamOptions.begin() != CamOptions.end()) {
		cam_opt *opt = (*CamOptions.begin()).second;
		IDDelete(MYDEV, (*CamOptions.begin()).first.c_str(), NULL);

		switch(opt->widget->type) {
		case GP_WIDGET_RADIO:
		case GP_WIDGET_MENU:
		case GP_WIDGET_TOGGLE:
			free(opt->item.sw);
			break;
		case GP_WIDGET_TEXT:
		case GP_WIDGET_DATE:
			free(opt->item.text.text);
			break;
		default:
			break;
		}

		delete opt;
		CamOptions.erase(CamOptions.begin());
	}
}
/* wait forever trying to open camera
 */
int
GPhotoCam::Connect()
{
	int setidx;
	const char **options;
	int max_opts;
	const char *port = NULL;

	if (gphotodrv)
		return 0;

	if(mPortTP.tp[0].text && strlen(mPortTP.tp[0].text)) {
		port = mPortTP.tp[0].text;
	}
	if (! (gphotodrv = gphoto_open(port))) {
		IDLog ("Can not open camera: power ok?\n");
		mConnectS[ON_S].s = ISS_OFF;
		mConnectS[OFF_S].s = ISS_ON;
		mConnectSP.s = IPS_ALERT;
		IDSetSwitch(&mConnectSP, "Can not open camera: power ok?");
		return -1;
	}

	// This is just for debugging.  It should be removed in the future
	//gphoto_show_options(gphotodrv);

	if (mFormatS)
		free(mFormatS);
	setidx = gphoto_get_format_current(gphotodrv);
	options = gphoto_get_formats(gphotodrv, &max_opts);
	mFormatS = create_switch("FORMAT", options, max_opts, setidx);
	mFormatSP.sp = mFormatS;
	mFormatSP.nsp = max_opts;

	if (mIsoS)
		free(mIsoS);
	setidx = gphoto_get_iso_current(gphotodrv);
	options = gphoto_get_iso(gphotodrv, &max_opts);
	mIsoS = create_switch("ISO", options, max_opts, setidx);
	mIsoSP.sp = mIsoS;
	mIsoSP.nsp = max_opts;

	/* Expose Group */
	IDDefNumber(&mExposureNP, NULL);

	/* Settings */
	IDDefSwitch (&mIsoSP, NULL);
	IDDefSwitch (&mFormatSP, NULL);
	IDDefSwitch (&mTransferSP, NULL);

	/* Data */
	IDDefBLOB(&mFitsBP, NULL);

	if(mExtendedS[0].s == ISS_ON)
		ShowExtendedOptions();

	return 0;

}

void
GPhotoCam::Reset()
{
	HideExtendedOptions();

	mConnectSP.s		= IPS_IDLE;
	mIsoSP.s			= IPS_IDLE;
	mFormatSP.s		= IPS_IDLE;
	mTransferSP.s		= IPS_IDLE;
	mExposureNP.s		= IPS_IDLE;
	mFitsBP.s		= IPS_IDLE;

	gphoto_close(gphotodrv);	
	gphotodrv = NULL;

	IDDelete(MYDEV, mIsoSP.name, NULL);
	IDDelete(MYDEV, mFormatSP.name, NULL);
	IDDelete(MYDEV, mTransferSP.name, NULL);
	IDDelete(MYDEV, mExposureNP.name, NULL);
	IDDelete(MYDEV, mFitsBP.name, NULL);
	//IDSetSwitch(&mConnectSP, NULL);
	//IDSetSwitch(&mIsoSP, NULL);
	//IDSetSwitch(&mFormatSP, NULL);
	//IDSetSwitch(&mTransferSP, NULL);
	//IDSetNumber(&mExposureNP, NULL);
	//IDSetBLOB(&mFitsBP, NULL);
}

void GPhotoCam::InitVars(void)
{

	/**********************************************************************************************/
	/************************************ GROUP: Communication ************************************/
	/**********************************************************************************************/
  	IUFillSwitch(&mConnectS[ON_S], "CONNECT" , "Connect" , ISS_OFF);
  	IUFillSwitch(&mConnectS[OFF_S], "DISCONNECT", "Disconnect", ISS_ON);
  	IUFillSwitchVector(&mConnectSP, mConnectS, NARRAY(mConnectS), MYDEV,
		"CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY,
		0, IPS_IDLE);
	IUFillText(&mPortT[0], "PORT" , "Port", "");
	IUFillTextVector(&mPortTP, mPortT, NARRAY(mPortT), MYDEV,
		"SHUTTER_PORT" , "Shutter Release", COMM_GROUP, IP_RW,
		0, IPS_IDLE);
  	IUFillSwitch(&mExtendedS[ON_S], "SHOW" , "Show" , ISS_OFF);
  	IUFillSwitch(&mExtendedS[OFF_S], "HIDE" , "Hide" , ISS_OFF);
  	IUFillSwitchVector(&mExtendedSP, mExtendedS, NARRAY(mExtendedS), MYDEV,
		"CAM_PROPS" , "Cam Props", COMM_GROUP, IP_RW, ISR_1OFMANY,
		0, IPS_IDLE);
	/**********************************************************************************************/
	/*********************************** GROUP: Expose ********************************************/
	/**********************************************************************************************/
	IUFillNumber(&mExposureN[0], "CCD_EXPOSURE_VALUE", "Duration (s)",
		"%5.2f", 0., 36000., .5, 1.);
	IUFillNumberVector(&mExposureNP, mExposureN, NARRAY(mExposureN), MYDEV,
                "CCD_EXPOSURE", "Expose", EXPOSE_GROUP, IP_RW, 36000, IPS_IDLE);

	/**********************************************************************************************/
	/*********************************** GROUP: Image Settings ************************************/
	/**********************************************************************************************/
	//We don't know how many items will be in the switch yet
	IUFillSwitchVector(&mIsoSP, NULL, 0, MYDEV,
		"ISO", "ISO", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
	IUFillSwitchVector(&mFormatSP, NULL, 0, MYDEV,
		"CAPTURE_FORMAT", "Capture Format", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
  	IUFillSwitch(&mTransferS[0], "FITS" , "Fits" , ISS_ON);
  	IUFillSwitch(&mTransferS[1], "ZFITS", "Compressed Fits", ISS_OFF);
  	IUFillSwitch(&mTransferS[2], "NATIVE", "Native", ISS_OFF);
	IUFillSwitchVector(&mTransferSP, mTransferS, NARRAY(mTransferS), MYDEV,
		"TRANSFER_FORMAT", "Transfer Format", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

	/**********************************************************************************************/
	/*********************************** GROUP: Data Settings *************************************/
	/**********************************************************************************************/
	IUFillBLOB(&mFitsB[0], "Img", "Image", ".fits");
	IUFillBLOBVector(&mFitsBP, mFitsB, NARRAY(mFitsB), MYDEV,
		"Pixels", "Image data", DATA_GROUP, IP_RO, 0, IPS_IDLE);
}
