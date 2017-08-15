/*
   INDI Driver for i-Nova PLX series
   Copyright 2013/2014 i-Nova Technologies - Ilia Platone

   written by Ilia Platone.
*/

#include <stdlib.h>
#include <sys/file.h>
#include "inovaplx_ccd.h"

float timerN;
float timerW;
float timerS;
float timerE;
unsigned char DIR		  = 0xF;
unsigned char OLD_DIR	  = 0xF;
const int POLLMS		   = 500;	   /* Polling interval 500 ms */
const int MAX_CCD_GAIN	 = 1023;		/* Max CCD gain */
const int MIN_CCD_GAIN	 = 0;		/* Min CCD gain */
const int MAX_CCD_KLEVEL   = 255;		/* Max CCD black level */
const int MIN_CCD_KLEVEL   = 0;		/* Min CCD black level */

/* Macro shortcut to CCD values */
#define Property( x )   CameraPropertiesN[x]
#define TEMP_FILE "/tmp/inovaInstanceNumber.tmp"
INovaCCD *inova = NULL;
static int isInit = 0;
extern char *__progname;

static void * capture_Thread(void * arg)
{
	((INovaCCD *)arg)->CaptureThread();
	return NULL;
}

static void cleanup()
{
    delete inova;
}

void ISInit()
{
	if (isInit == 1)
		return;
	isInit ++;
	inova = new INovaCCD();
        atexit(cleanup);
}

void ISGetProperties(const char *dev)
{
	ISInit();
	inova->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
	ISInit();
	inova->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
	ISInit();
	inova->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
	ISInit();
	inova->ISNewNumber(dev, name, values, names, num);
}

bool GuideEast(float ms)
{
	ISInit();
	return inova->GuideEast(ms);
}

bool GuideWest(float ms)
{
	ISInit();
	return inova->GuideWest(ms);
}

bool GuideSouth(float ms)
{
	ISInit();
	return inova->GuideSouth(ms);
}

bool GuideNorth(float ms)
{
	ISInit();
	return inova->GuideNorth(ms);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
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

void ISSnoopDevice (XMLEle *root)
{
	ISInit();
	inova->ISSnoopDevice(root);
}


INovaCCD::INovaCCD()
{
	ExposureRequest = 0.0;
	InExposure = false;
}
bool INovaCCD::HasST4Port ()
{
	return iNovaSDK_HasST4();
}
bool INovaCCD::HasBayer ()
{
	return iNovaSDK_HasColorSensor();
}
bool INovaCCD::CanSubFrame ()
{
	return true;
}
bool INovaCCD::CanBin ()
{
	return true;
}
bool INovaCCD::Connect()
{
	int step = 0;
	const char *Sn;
	if(iNovaSDK_MaxCamera() > 0) {
		Sn = iNovaSDK_OpenCamera(1);
		IDMessage(getDefaultName(), "SN: %s\n", Sn);
		if(Sn[0] >= '0' && Sn[0] < '3')
		 {
		iNovaSDK_InitST4();
		IDMessage(getDefaultName(), "Camera model is %s\n", iNovaSDK_GetName());
		iNovaSDK_InitCamera(RESOLUTION_FULL);
		step++;
		maxW = iNovaSDK_GetImageWidth();
		maxH = iNovaSDK_GetImageHeight();
		iNovaSDK_SetFrameSpeed(FRAME_SPEED_LOW);
		iNovaSDK_CancelLongExpTime();
		iNovaSDK_OpenVideo();
		threadsRunning = true;
		RawData = (unsigned char *)malloc(iNovaSDK_GetArraySize() * (iNovaSDK_GetDataWide() > 0 ? 2 : 1));
		pthread_create(&captureThread, NULL, capture_Thread, (void*)this);
		CameraPropertiesNP.s = IPS_IDLE;
		return true;
		}
		iNovaSDK_CloseCamera();
	}
	IDMessage(getDefaultName(), "ERROR.");
	return false;
}

bool INovaCCD::Disconnect()
{
	threadsRunning = false;
	iNovaSDK_SensorPowerDown();
	iNovaSDK_CloseVideo();
	iNovaSDK_CloseCamera();
	IDMessage(getDefaultName(), "disconnected\n");
	return true;
}

const char * INovaCCD::getDefaultName()
{
	return "i.Nova Camera";
}

const char * INovaCCD::getDeviceName()
{
	return getDefaultName();
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool INovaCCD::initProperties()
{
	// Must init parent properties first!
	INDI::CCD::initProperties();

	CameraPropertiesN = new INumber[NUM_PROPERTIES];

	// We init the property details. This is a stanard property of the INDI Library.
	IUFillText(&iNovaInformationT[0], "INOVA_NAME", "Camera Name", iNovaSDK_GetName());
	IUFillText(&iNovaInformationT[1], "INOVA_SENSOR_NAME", "Sensor Name", iNovaSDK_SensorName());
	IUFillText(&iNovaInformationT[2], "INOVA_SN", "Serial Number", iNovaSDK_SerialNumber());
	IUFillText(&iNovaInformationT[3], "INOVA_ST4", "Can Guide", iNovaSDK_HasST4() ? "Yes" : "No");
	IUFillText(&iNovaInformationT[4], "INOVA_COLOR", "Color Sensor", iNovaSDK_HasColorSensor() ? "Yes" : "No");
	IUFillTextVector(&iNovaInformationTP, iNovaInformationT, 5, getDeviceName(), "INOVA_INFO", "i.Nova Camera Informations", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
	IUFillNumber(&CameraPropertiesN[CCD_GAIN_N], "CCD_GAIN_VALUE", "Gain", "%4.0f", 0, 1023, 1, 255);
	IUFillNumber(&CameraPropertiesN[CCD_BLACKLEVEL_N], "CCD_BLACKLEVEL_VALUE", "Black Level", "%3.0f", 0, 255, 1, 0);
	IUFillNumberVector(&CameraPropertiesNP, CameraPropertiesN, NUM_PROPERTIES, getDeviceName(), "CCD_PROPERTIES", "Camera properties", IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

	uint32_t cap = CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | (iNovaSDK_HasColorSensor() ? CCD_HAS_BAYER : 0) | (iNovaSDK_HasST4() ? CCD_HAS_ST4_PORT : 0);
	SetCCDCapability(cap);
	return true;

}

/**************************************************************************************
** INDI is asking us to submit list of properties for the device
***************************************************************************************/
void INovaCCD::ISGetProperties(const char *dev)
{
	INDI::CCD::ISGetProperties(dev);

	// If we are _already_ connected, let's define our temperature property to the client now
	if (isConnected())
	{
		// Define our properties
	iNovaInformationT[0].text = (char*)iNovaSDK_GetName();
	iNovaInformationT[1].text = (char*)iNovaSDK_SensorName();
	iNovaInformationT[2].text = (char*)iNovaSDK_SerialNumber();
	iNovaInformationT[3].text = (char*)(iNovaSDK_HasST4() ? "Yes" : "No");
	iNovaInformationT[4].text = (char*)(iNovaSDK_HasColorSensor() ? "Yes" : "No");
		defineText(&iNovaInformationTP);
		defineNumber(&CameraPropertiesNP);
		defineNumber(&GuideNSV);
		defineNumber(&GuideEWV);
	}

}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool INovaCCD::updateProperties()
{

	if (isConnected())
	{
		// Define our properties
		iNovaInformationT[0].text = (char*)iNovaSDK_GetName();
		iNovaInformationT[1].text = (char*)iNovaSDK_SensorName();
		iNovaInformationT[2].text = (char*)iNovaSDK_SerialNumber();
		iNovaInformationT[3].text = (char*)(iNovaSDK_HasST4() ? "Yes" : "No");
		iNovaInformationT[4].text = (char*)(iNovaSDK_HasColorSensor() ? "Yes" : "No");
		defineText(&iNovaInformationTP);
		defineNumber(&CameraPropertiesNP);
		defineNumber(&GuideNSV);
		defineNumber(&GuideEWV);

		// Let's get parameters now from CCD
		setupParams();

		// Start the timer
		SetTimer(POLLMS);
	}
	else
		// We're disconnected
	{
		deleteProperty(iNovaInformationTP.name);
		deleteProperty(CameraPropertiesNP.name);
		deleteProperty(GuideNSV.name);
		deleteProperty(GuideEWV.name);
	}

	// Call parent update properties
	INDI::CCD::updateProperties();

	return true;
}

/**************************************************************************************
** Setting up CCD parameters
***************************************************************************************/
void INovaCCD::setupParams()
{
	int bpp = iNovaSDK_GetDataWide() > 0 ? 16 : 8;
	SetCCDParams(iNovaSDK_GetImageWidth(), iNovaSDK_GetImageHeight(), bpp, 5.4d, 5.4d);

	// Let's calculate how much memory we need for the primary CCD buffer
	int nbuf;
	nbuf=PrimaryCCD.getXRes()*PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8;
	nbuf+=512;	//  leave a little extra at the end
	PrimaryCCD.setFrameBufferSize(nbuf);
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool INovaCCD::StartExposure(float duration)
{

	// Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
	double expTime = 1000.0 * duration;
	iNovaSDK_SetExpTime(expTime);

	ExposureRequest = duration;
	PrimaryCCD.setExposureDuration(ExposureRequest);
	gettimeofday(&ExpStart,NULL);

	InExposure=true;

	// We're done
	return true;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool INovaCCD::AbortExposure()
{
	iNovaSDK_CancelLongExpTime();
	InExposure = false;
	return true;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float INovaCCD::CalcTimeLeft()
{
	double timesince;
	double timeleft;
	struct timeval now;
	gettimeofday(&now,NULL);

	timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec/1000);
	timesince=timesince/1000;

	timeleft=ExposureRequest-timesince;
	return timeleft;
}

/**************************************************************************************
** Client is asking us to set a new number
***************************************************************************************/
bool INovaCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
	if (strcmp (dev, getDeviceName()))
		return false;

	if (isConnected() == false)
	{
		IDMessage(getDefaultName(), "Cannot change property while device is disconnected.");
		return false;
	}

	if (!strcmp(name, CameraPropertiesNP.name))
	{
		CameraPropertiesNP.s = IPS_BUSY;

		IUUpdateNumber(&CameraPropertiesNP, values, names, n);

		iNovaSDK_SetAnalogGain((int16_t)Property( CCD_GAIN_N ).value);
		iNovaSDK_SetBlackLevel((int16_t)Property( CCD_BLACKLEVEL_N ).value);

		IDSetNumber(&CameraPropertiesNP, NULL);
		CameraPropertiesNP.s = IPS_IDLE;
		return true;
	}

	if(INDI::CCD::ISNewNumber(dev,name,values,names,n))
	{
		binX = PrimaryCCD.getBinX();
		binY = PrimaryCCD.getBinY();
		startX = PrimaryCCD.getSubX();
		startY = PrimaryCCD.getSubY();
		endX = startX + PrimaryCCD.getSubW();
		endY = startY + PrimaryCCD.getSubH();
		endX = (endX > maxW ? maxW : endX);
		endY = (endY > maxH ? maxH : endY);

		PrimaryCCD.setFrame (startX, startY, endX-startX, endY-startY);

		return true;
	}

	return false;
}

/**************************************************************************************
** INDI is asking us to add any FITS keywords to the FITS header
***************************************************************************************/
void INovaCCD::addFITSKeywords(fitsfile *fptr, CCDChip *targetChip)
{
	// Let's first add parent keywords
	INDI::CCD::addFITSKeywords(fptr, targetChip);

	// Add temperature to FITS header
	int status=0;
	//fits_update_key_s(fptr, TDOUBLE, "GAIN", &(Property( CCD_GAIN_N ).value), "CCD Gain", &status);
	//fits_update_key_s(fptr, TDOUBLE, "BLACKLEVEL", &(Property( CCD_BLACKLEVEL_N ).value), "CCD Black Level", &status);
	fits_write_date(fptr, &status);

}

/**************************************************************************************
** Main device loop. We check for exposure and temperature progress here
***************************************************************************************/
void INovaCCD::TimerHit()
{
	long timeleft;

	if(isConnected() == false)
		return;  //  No need to reset timer if we are not connected anymore

	if (InExposure)
	{
		timeleft=CalcTimeLeft();

		// Less than a 0.1 second away from exposure completion
		// This is an over simplified timing method, check CCDSimulator and inova for better timing checks
		if(timeleft < 0.1)
		{
			/* We're done exposing */
			IDMessage(getDefaultName(), getDeviceName(), "Exposure done, downloading image...");
		}
		else
			// Just update time left in client
			PrimaryCCD.setExposureLeft(timeleft);

	}

	SetTimer(POLLMS);
	return;
}

IPState INovaCCD::GuideEast(float ms)
{
	DIR = 0x0E;
	iNovaSDK_SendST4(DIR);
	usleep(ms * 1000000);
	DIR |= 0x09;
	iNovaSDK_SendST4(DIR);
	//timerE = IEAddTimer(ms, timerEast, (void *)this);
	return IPS_IDLE;
}

IPState INovaCCD::GuideWest(float ms)
{
	DIR &= 0x07;
	iNovaSDK_SendST4(DIR);
	usleep(ms * 1000000);
	DIR |= 0x09;
	iNovaSDK_SendST4(DIR);
	//timerW = IEAddTimer(ms, timerWest, (void *)this);
	return IPS_IDLE;
}

IPState INovaCCD::GuideNorth(float ms)
{
	DIR &= 0x0D;
	iNovaSDK_SendST4(DIR);
	usleep(ms * 1000000);
	DIR |= 0x06;
	iNovaSDK_SendST4(DIR);
	//timerN = IEAddTimer(ms, timerNorth, (void *)this);
	return IPS_IDLE;
}

IPState INovaCCD::GuideSouth(float ms)
{
	DIR &= 0x0B;
	iNovaSDK_SendST4(DIR);
	usleep(ms * 1000000);
	DIR |= 0x06;
	iNovaSDK_SendST4(DIR);
	//timerS = IEAddTimer(ms, timerSouth, (void *)this);
	return IPS_IDLE;
}

void INovaCCD::CaptureThread()
{
	while(threadsRunning)
	{
		RawData = (unsigned char*)iNovaSDK_GrabFrame();
		if(RawData != NULL && InExposure)
		{
			// We're no longer exposing...
			InExposure = false;
			
			grabImage();
		}
	}
}

void INovaCCD::grabImage()
{
	// Let's get a pointer to the frame buffer
	unsigned char * image = PrimaryCCD.getFrameBuffer();
	if(image != NULL)
	{
		int Bpp = iNovaSDK_GetDataWide() > 0 ? 2 : 1;
		int p = 0;

		for(int y=startY; y<endY; y+=binY)
		{
			if(endY-y<binY)
				break;
			for(int x=startX*Bpp; x<endX*Bpp; x+=Bpp*binX)
			{
				if(endX*Bpp-x<binX*Bpp)
					break;
				int t = 0;
				for(int yy = y; yy < y+binY; yy++)
				{
					for(int xx = x; xx < x+Bpp*binX; xx+=Bpp)
					{
						if(Bpp>1)
						{
							t += RawData[1+xx+yy*maxW*Bpp] + (RawData[xx+yy*maxW*Bpp] << 8);
							t = (t < 0xffff ? t : 0xffff);
						}
						else
						{
							t += RawData[xx+yy*maxW*Bpp];
							t = (t < 0xff ? t : 0xff);
						}
					}
				}
				image[p++] = (unsigned char)(t & 0xff);
				if(Bpp>1)
				{
					image[p++] = (unsigned char)((t >> 8) & 0xff);
				}
			}
		}
		// Let INDI::CCD know we're done filling the image buffer
		IDMessage(getDefaultName(), getDeviceName(), "Download complete.");
		ExposureComplete(&PrimaryCCD);
	}
}
