#if 0
    QSI CCD
    INDI Interface for Quantum Scientific Imaging CCDs
    Based on FLI Indi Driver by Jasem Mutlaq.
    Copyright (C) 2009 Sami Lehti (sami.lehti@helsinki.fi)

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

#include "qsiapi.h"
#include "QSIError.h"
#include "indidevapi.h"
#include "eventloop.h"
#include "indicom.h"

#include <iostream>
using namespace std;

void ISInit(void);
void ISPoll(void *);
void handleExposure(void *);
void connectCCD(void);
void activateCooler();
void resetFrame();
void getBasicData(void);
void uploadFile(const char* filename);
int  writeFITS(const char* filename, char errmsg[]);
bool setImageArea(char errmsg[]);
int  manageDefaults(char errmsg[]);
int  grabImage(void);
int  checkPowerS(ISwitchVectorProperty *sp);
int  checkPowerN(INumberVectorProperty *np);
int  checkPowerT(ITextVectorProperty *tp);
int  getOnSwitch(ISwitchVectorProperty *sp);
int  isCCDConnected(void);
void addFITSKeywords(fitsfile *fptr);
void fits_update_key_s(fitsfile*, int, string, void*, string, int*);
void turnWheel();
void shutterControl();

double min(void);
double max(void);

extern char* me;
extern int errno;

#define mydev           "QSI CCD"

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"
#define DATA_GROUP      "Data Channel"
#define FILTER_GROUP      "Filter Wheel"

#define MAX_CCD_TEMP	45		/* Max CCD temperature */
#define MIN_CCD_TEMP	-55		/* Min CCD temperature */
#define MAX_X_BIN	16		/* Max Horizontal binning */
#define MAX_Y_BIN	16		/* Max Vertical binning */
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

#define LAST_FILTER  4          /* Max slot index */
#define FIRST_FILTER 0          /* Min slot index */

#define currentFilter   FilterN[0].value

enum QSIFrames { LIGHT_FRAME = 0, BIAS_FRAME, DARK_FRAME, FLAT_FRAME };

class ImageData {
    public:
        ImageData();
        ~ImageData();

        void setStartTime();
        int  timeLeft();

        int  width;
        int  height;
        int  frameType;
        double  expose;
        unsigned short *img;

    private:
        time_t startTime;
};

ImageData::ImageData() : frameType(LIGHT_FRAME),startTime(time(NULL)){}
ImageData::~ImageData(){}
void ImageData::setStartTime(){ startTime = time(NULL);}
int ImageData::timeLeft(){ return expose - (time(NULL) - startTime);}

static QSICamera QSICam;
static ImageData* QSIImg;
static short targetFilter;

/*INDI controls */

 /* Connect/Disconnect */
 static ISwitch ConnectS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
 static ISwitchVectorProperty ConnectSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, ConnectS, NARRAY(ConnectS), "", 0};

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
 static INumber SetBinningN[]       = {
//    { "HOR_BIN", "X", "%0.f", 1., MAX_X_BIN, 1., 1., 0, 0, 0},
    { "HOR_BIN", "X", "%.0f", 1., 1600., 1., 1., 0, 0, 0},
    { "VER_BIN", "Y", "%.0f", 1., 1600., 1., 1., 0, 0, 0}};
 static INumberVectorProperty SetBinningNP = { mydev, "SET_BINNING", "Binning", IMAGE_GROUP, IP_RW, 60, IPS_IDLE, SetBinningN, NARRAY(SetBinningN), "", 0};

 static ISwitch ResetS[]             = { {"RESET1", "Reset", ISS_OFF, 0, 0}};
 static ISwitchVectorProperty ResetSP = { mydev, "FRAME_RESET2", "Reset1", IMAGE_GROUP, IP_WO, ISR_1OFMANY, 0, IPS_IDLE, ResetS, NARRAY(ResetS), "", 0};

 /* Exposure time */
  static INumber ExposeTimeWN[]    = {{ "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0., 36000., .1, 1., 0, 0, 0}};
  static INumberVectorProperty ExposeTimeWNP = { mydev, "CCD_EXPOSURE", "Expose", EXPOSE_GROUP, IP_WO, 36000, IPS_IDLE, ExposeTimeWN, NARRAY(ExposeTimeWN), "", 0};

  static INumber ExposeTimeLeftN[]    = {{ "CCD_TIMELEFT_VALUE", "Time left (s)", "%5.2f", 0., 36000., 0., 0., 0, 0, 0}};
  static INumberVectorProperty ExposeTimeLeftNP = { mydev, "CCD_TIMELEFT", "Time left", EXPOSE_GROUP, IP_RO, 36000, IPS_IDLE, ExposeTimeLeftN, NARRAY(ExposeTimeLeftN), "", 0};
 
  /* Temperature control */
 static INumber SetTemperatureN[]	  = { {"SET_CCD_TEMPERATURE_VALUE", "Set Temperature (C)", "%5.2f", MIN_CCD_TEMP, MAX_CCD_TEMP, 0., 0., 0, 0, 0}};
// static INumberVectorProperty SetTemperatureNP = { mydev, "CCD_TEMPERATURE", "Temperature", EXPOSE_GROUP, IP_RW, 60, IPS_IDLE, TemperatureN, NARRAY(TemperatureN), "", 0};
 static INumberVectorProperty SetTemperatureNP = { mydev, "SET_CCD_TEMPERATURE", "Temperature", EXPOSE_GROUP, IP_WO, 60, IPS_IDLE, SetTemperatureN, NARRAY(SetTemperatureN), "", 0};

 static INumber TemperatureN[]    = { {"CCD_TEMPERATURE_VALUE", "Temperature (C)", "%+06.2f", MIN_CCD_TEMP, MAX_CCD_TEMP, .2, 0., 0, 0, 0}};
 static INumberVectorProperty TemperatureNP = { mydev, "CCD_TEMPERATURE", "Temperature", EXPOSE_GROUP, IP_RO, 60, IPS_IDLE, TemperatureN, NARRAY(TemperatureN), "", 0};

 static INumber CoolerN[]    = { {"CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0., 0, 0, 0}};
 static INumberVectorProperty CoolerNP = { mydev, "CCD_COOLER", "Cooling Power", EXPOSE_GROUP, IP_RO, 60, IPS_IDLE, CoolerN, NARRAY(CoolerN), "", 0}; 

 static ISwitch CoolerS[]               = {{"CONNECT_COOLER" , "ON" , ISS_OFF, 0, 0},{"DISCONNECT_COOLER", "OFF", ISS_ON, 0, 0}};
 static ISwitchVectorProperty CoolerSP  = { mydev, "COOLER_CONNECTION" , "Cooler", EXPOSE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, CoolerS, NARRAY(CoolerS), "", 0};

 /* Filter control */
 static INumber FilterN[]               = {{"FILTER_SLOT_VALUE", "Active Filter", "%2.0f", FIRST_FILTER, LAST_FILTER, 1, 0, 0, 0, 0}};
// static INumberVectorProperty FilterNP  = { mydev, "FILTER_SLOT", "Filter", FILTER_GROUP, IP_RO, 0, IPS_IDLE, FilterN, NARRAY(FilterN), "", 0};
 static INumberVectorProperty FilterNP  = { mydev, "FILTER_SLOT", "Filter", FILTER_GROUP, IP_RW, 0, IPS_IDLE, FilterN, NARRAY(FilterN), "", 0};

 static ISwitch FilterS[]               = {{"TURN_WHEEL" , "    +    " , ISS_OFF, 0, 0},{"RTURN_WHEEL", "    -    ", ISS_OFF, 0, 0}};
 static ISwitchVectorProperty FilterSP  = { mydev, "WHEEL_CONNECTION" , "Turn Wheel", FILTER_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, FilterS, NARRAY(FilterS), "", 0};

 /* Shutter */
 static ISwitch ShutterS[]              = {{"SHUTTER_ON" , "Manual open" , ISS_OFF, 0, 0},{"SHUTTER_OFF", "Manual close", ISS_ON, 0, 0}};
 static ISwitchVectorProperty ShutterSP = { mydev, "SHUTTER_CONNECTION" , "Shutter", EXPOSE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, ShutterS, NARRAY(ShutterS), "", 0};

  /* Pixel size (µm) */
 static INumber PixelSizeN[] 	= {
    { "Width", "", "%.0f", 0. , 0., 0., 0., 0, 0, 0},
    { "Height", "", "%.0f", 0. , 0., 0., 0., 0, 0, 0}};
 static INumberVectorProperty PixelSizeNP = { mydev, "Pixel Size (µm)", "", DATA_GROUP, IP_RO, 0, IPS_IDLE, PixelSizeN, NARRAY(PixelSizeN), "", 0};

 /* BLOB for sending image */
 static IBLOB imageB = {"CCD1", "Feed", "", 0, 0, 0, 0, 0, 0, 0};
 static IBLOBVectorProperty imageBP = {mydev, "Video", "Video", COMM_GROUP,IP_RO, 0, IPS_IDLE, &imageB, 1, "", 0};


/* send client definitions of all properties */
void ISInit() {
        static int isInit=0;

        if (isInit) return;

        QSIImg = new ImageData;

        targetFilter = 0;

        IEAddTimer (POLLMS, ISPoll, NULL);

        isInit = 1;
}

void ISGetProperties (const char *dev) { 

        ISInit();
  
        if (dev && strcmp (mydev, dev)) return;

        /* COMM_GROUP */
        IDDefSwitch(&ConnectSP, NULL);
        IDDefBLOB(&imageBP, NULL);
  
        /* Expose */
        IDDefSwitch(&FrameTypeSP, NULL);  
        IDDefNumber(&ExposeTimeWNP, NULL);
        IDDefNumber(&ExposeTimeLeftNP, NULL);
        IDDefNumber(&SetTemperatureNP, NULL);
        IDDefNumber(&TemperatureNP, NULL);
        IDDefNumber(&CoolerNP, NULL);
        IDDefSwitch(&CoolerSP, NULL);
        IDDefSwitch(&ShutterSP, NULL);
  
        /* Image Group */
        IDDefNumber(&FrameNP, NULL);
        IDDefNumber(&SetBinningNP, NULL);
        IDDefSwitch(&ResetSP, NULL);

        /* Filter Wheel */
        IDDefNumber(&FilterNP, NULL);
        IDDefSwitch(&FilterSP, NULL);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {}
void ISSnoopDevice (XMLEle *root) {}
  
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) {

	ISwitch *sp;
	
	/* ignore if not ours */
	if (dev && strcmp (dev, mydev)) return;
	    
	ISInit();

	/* Connection */
	if (!strcmp (name, ConnectSP.name)) {
	  if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0)
		return;
   	  connectCCD();
	  return;
	}
	
        /* Frame Type */
        if (!strcmp(FrameTypeSP.name, name)) {

            if (checkPowerS(&FrameTypeSP)) return;
	 
            for (int i = 0; i < n ; i++) {
                sp = IUFindSwitch(&FrameTypeSP, names[i]);
	 
	        if (!sp) {
	            FrameTypeSP.s = IPS_ALERT;
	            IDSetSwitch(&FrameTypeSP, "Unknown error. %s is not a member of %s property.", names[0], name);
	            return;
	        }
	 
	        /* NORMAL or FLAT */
	        if ( (sp == &FrameTypeS[LIGHT_FRAME] || 
                      sp == &FrameTypeS[FLAT_FRAME]) && states[i] == ISS_ON) {

	            if (sp == &FrameTypeS[LIGHT_FRAME])
	              QSIImg->frameType = LIGHT_FRAME;
  	            else
	              QSIImg->frameType = FLAT_FRAME;

	            IUResetSwitch(&FrameTypeSP);
	            sp->s = ISS_ON; 
	            FrameTypeSP.s = IPS_OK;
	            IDSetSwitch(&FrameTypeSP, NULL);
	            break;
	        }
	        /* DARK AND BIAS */
	        else if ( (sp == &FrameTypeS[DARK_FRAME] || 
                           sp == &FrameTypeS[BIAS_FRAME]) && states[i] == ISS_ON){
	   
	            if (sp == &FrameTypeS[DARK_FRAME])
	              QSIImg->frameType = DARK_FRAME;
	            else
	              QSIImg->frameType = BIAS_FRAME;

    	            IUResetSwitch(&FrameTypeSP);
	            sp->s = ISS_ON;
	            FrameTypeSP.s = IPS_OK;
	            IDSetSwitch(&FrameTypeSP, NULL);
	            break;
	        }
	   
  	    } /* For loop */
	    return;
        }

        /* Cooler */
        if (!strcmp (name, CoolerSP.name)) {
          if (IUUpdateSwitch(&CoolerSP, states, names, n) < 0) return;
          activateCooler();
          return;
        }

        /* Reset */
        if (!strcmp (name, ResetSP.name)) {
          if (IUUpdateSwitch(&ResetSP, states, names, n) < 0) return;
          resetFrame();
          return;
        }

        /* Filter Wheel */
        if (!strcmp (name, FilterSP.name)) {
            if (IUUpdateSwitch(&FilterSP, states, names, n) < 0) return;
            turnWheel();
            return;
        }

        /* Shutter */
        if (!strcmp (name, ShutterSP.name)) {
            if (IUUpdateSwitch(&ShutterSP, states, names, n) < 0) return;
            shutterControl();
            return;
        }
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) {

	ISInit();
 
        /* ignore if not ours */ 
        if (dev && strcmp (mydev, dev))
         return;

        /* suppress warning */
	n=n; dev=dev; name=name; names=names; texts=texts;
	
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) {

        INumber *np;
        char errmsg[ERRMSG_SIZE];

        /* ignore if not ours */
        if (dev && strcmp (dev, mydev)) return;
	    
        ISInit();


        /* Exposure time */
        if (!strcmp (ExposeTimeWNP.name, name)) {

            if (checkPowerN(&ExposeTimeWNP)) return;
            if (checkPowerN(&ExposeTimeLeftNP)) return;

            if (ExposeTimeWNP.s == IPS_BUSY) {

              try {
                QSICam.AbortExposure();
              } catch (std::runtime_error err) {
                ExposeTimeWNP.s = IPS_ALERT;
                IDSetNumber(&ExposeTimeWNP, "AbortExposure() failed. %s.", err.what());
                IDLog("AbortExposure() failed. %s.\n", err.what());
                return;
              }

	      ExposeTimeWNP.s = IPS_IDLE;

              IDSetNumber(&ExposeTimeWNP, "Exposure cancelled.");
	      IDLog("Exposure Cancelled.\n");

	      return;
            }

            np = IUFindNumber(&ExposeTimeWNP, names[0]);

            if (!np) {
    	       ExposeTimeWNP.s = IPS_ALERT;
	       IDSetNumber(&ExposeTimeWNP, "Error: %s is not a member of %s property.", names[0], name);
    	       return;
            }

            double minDuration;
            try {
                QSICam.get_MinExposureTime(&minDuration);
            } catch (std::runtime_error err) {
                IDMessage(mydev, "get_MinExposureTime() failed. %s.", err.what());
                IDLog("get_MinExposureTime() failed. %s.", err.what());
                return;
            }
            if(values[0] < minDuration) {
                values[0] = minDuration;
                IDMessage(mydev, "Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.", minDuration,minDuration);
            }

            if(QSIImg->frameType == BIAS_FRAME){
                np->value = minDuration;
                IDSetNumber(&ExposeTimeWNP, "Bias Frame (s) : %g\n", np->value);
                IDLog("Bias Frame (s) : %g\n", np->value);
            } else {
                np->value = values[0];
                IDLog("Exposure Time (s) is: %g\n", np->value);
            }

            QSIImg->expose = np->value;

            handleExposure(NULL);

            return;
        } 
    

        /* Temperature*/    
        if (!strcmp(SetTemperatureNP.name, name)) {
            if (checkPowerN(&SetTemperatureNP)) return;
      
            SetTemperatureNP.s = IPS_IDLE;
    
            np = IUFindNumber(&SetTemperatureNP, names[0]);
    
            if (!np) {
                IDSetNumber(&SetTemperatureNP, "Unknown error. %s is not a member of %s property.", names[0], name);
                return;
            }
    
            if (values[0] < MIN_CCD_TEMP || values[0] > MAX_CCD_TEMP) {
                IDSetNumber(&SetTemperatureNP, "Error: valid range of temperature is from %d to %d", MIN_CCD_TEMP, MAX_CCD_TEMP);
                return;
            }

            bool canSetTemp;
            try {
                QSICam.get_CanSetCCDTemperature(&canSetTemp);
            } catch (std::runtime_error err) {
                IDSetNumber(&SetTemperatureNP, "CanSetCCDTemperature() failed. %s.", err.what());
                IDLog("CanSetCCDTemperature() failed. %s.", err.what());
                return;
            }
            if(!canSetTemp){
                IDMessage(mydev, "Cannot set CCD temperature, CanSetCCDTemperature == false\n");
                return;
            }

            try {
                QSICam.put_SetCCDTemperature(values[0]);
            } catch (std::runtime_error err) {
                IDSetNumber(&SetTemperatureNP, "put_SetCCDTemperature() failed. %s.", err.what());
                IDLog("put_SetCCDTemperature() failed. %s.", err.what());
                return;
            }
    
            SetTemperatureNP.s = IPS_OK;
    
            IDSetNumber(&SetTemperatureNP, "Setting CCD temperature to %+06.2f C", values[0]);
            IDLog("Setting CCD temperature to %+06.2f C\n", values[0]);
            return;
        }

        /* Frame */
        if (!strcmp(FrameNP.name, name)) {
            int nset=0;
     
            if (checkPowerN(&FrameNP)) return;
      
            FrameNP.s = IPS_IDLE;
     
            for (int i=0; i < n ; i++) {
                np = IUFindNumber(&FrameNP, names[i]);
       
                if (!np) {
                    IDSetNumber(&FrameNP, "Unknown error. %s is not a member of %s property.", names[0], name);
                    return;
                }
      
                /* X or Width */
                if (np == &FrameN[0] || np==&FrameN[2]) {
                    if (values[i] < 0 || values[i] > QSIImg->width) break;
	 
	            nset++;
	            np->value = values[i];
                }

                /* Y or height */
                else if (np == &FrameN[1] || np==&FrameN[3]) {
                    if (values[i] < 0 || values[i] > QSIImg->height) break;
	 
       	            nset++;
	            np->value = values[i];
                }
            }
      
            if (nset < 4) {
                IDSetNumber(&FrameNP, "Invalid range. Valid range is (0,0) - (%0d,%0d)", QSIImg->width, QSIImg->height);
	        IDLog("Invalid range. Valid range is (0,0) - (%0d,%0d)", QSIImg->width, QSIImg->height);
	        return; 
            }
      
            if(!setImageArea(errmsg)) {
                IDSetNumber(&FrameNP, "%s",errmsg);
	        return;
            }

            FrameNP.s = IPS_OK;
      
            /* Adjusting image width and height */ 
            QSIImg->width  = FrameN[2].value;
            QSIImg->height = FrameN[3].value;
      
            IDSetNumber(&FrameNP, NULL);
      
        } /* end FrameNP */
      
    
        if (!strcmp(SetBinningNP.name, name)) {
            if (checkPowerN(&SetBinningNP)) return;
       
            SetBinningNP.s = IPS_IDLE;
     
            for (int i = 0 ; i < n ; i++) {
                np = IUFindNumber(&SetBinningNP, names[i]);
       
                if (!np) {
                    IDSetNumber(&SetBinningNP, "Unknown error. %s is not a member of %s property.", names[0], name);
                    return;
                }
      
                /* X binning */
                if (np == &SetBinningN[0]) {
                    short maxBinX;
                    try {
                        QSICam.get_MaxBinX(&maxBinX);
                    } catch (std::runtime_error err) {
                        IDSetNumber(&SetBinningNP, "get_MaxBinX() failed. %s.", err.what());
                        IDLog("get_MaxBinX() failed. %s.", err.what());
                        return;
                    }

                    if(values[i] > maxBinX){
                        values[i] = maxBinX;
                        IDSetNumber(&SetBinningNP, "Max valid X bin value is %d. Setting X bin to %d", maxBinX,maxBinX);
                        IDLog("Max valid X bin value is %d. Setting X bin to %d", maxBinX,maxBinX);
                    }

                    if (values[i] < 1 || values[i] > MAX_X_BIN) {
        	        IDSetNumber(&SetBinningNP, "Error: Valid X bin values are from 1 to %g", MAX_X_BIN);
    	                IDLog("Error: Valid X bin values are from 1 to %g", MAX_X_BIN);
	                return;
	            }
	
                    try {
                        QSICam.put_BinX(values[i]);
                    } catch (std::runtime_error err) {
        	        IDSetNumber(&SetBinningNP, "put_BinX() failed. %s.", err.what());
	                IDLog("put_BinX() failed. %s.", err.what());
	                return;
	            }
	
    	            np->value = values[i];
                } else if (np == &SetBinningN[1]) {
                    short maxBinY;
                    try {
                        QSICam.get_MaxBinY(&maxBinY);
                    } catch (std::runtime_error err) {
                        IDSetNumber(&SetBinningNP, "get_MaxBinY() failed. %s.", err.what());
                        IDLog("get_MaxBinY() failed. %s.", err.what());
                        return;
                    }

                    if(values[i] > maxBinY){
                        values[i] = maxBinY;
                        IDSetNumber(&SetBinningNP, "Max valid Y bin value is %d. Setting Y bin to %d", maxBinY,maxBinY);
                        IDLog("Max valid Y bin value is %d. Setting Y bin to %d", maxBinY,maxBinY);
                    }

                    if (values[i] < 1 || values[i] > MAX_Y_BIN) {
	                IDSetNumber(&SetBinningNP, "Error: Valid Y bin values are from 1 to %g", MAX_Y_BIN);
	                IDLog("Error: Valid X bin values are from 1 to %g", MAX_Y_BIN);
	                return;
	            }
	
                    try {
                        QSICam.put_BinY(values[i]);
                    } catch (std::runtime_error err) {
                        IDSetNumber(&SetBinningNP, "put_BinY() failed. %s.", err.what());
                        IDLog("put_BinY() failed. %s.", err.what());
                        return;
                    }
	
        	    np->value = values[i];
                }
            } /* end for */
     
            if (!setImageArea(errmsg)) {
                IDSetNumber(&SetBinningNP, errmsg, NULL);
                IDLog("%s", errmsg);
                return;
            }
     
            SetBinningNP.s = IPS_OK;
     
            IDLog("Binning is: %.0f x %.0f\n", SetBinningN[0].value, SetBinningN[1].value);
     
            IDSetNumber(&SetBinningNP, NULL);
            return;
        }


        if (!strcmp(FilterNP.name, name)) {

            targetFilter = values[0];

            np = IUFindNumber(&FilterNP, names[0]);

            if (!np) {
                FilterNP.s = IPS_ALERT;
                IDSetNumber(&FilterNP, "Unknown error. %s is not a member of %s property.", names[0], name);
                return;
            }

            int filter_count;
            try {
                QSICam.get_FilterCount(filter_count);
            } catch (std::runtime_error err) {
                IDSetNumber(&FilterNP, "get_FilterCount() failed. %s.", err.what());
            }
            if (targetFilter < FIRST_FILTER || targetFilter > filter_count - 1) {
                FilterNP.s = IPS_ALERT;
                IDSetNumber(&FilterNP, "Error: valid range of filter is from %d to %d", FIRST_FILTER, LAST_FILTER);
                return;
            }

            IUUpdateNumber(&FilterNP, values, names, n);

            FilterNP.s = IPS_BUSY;
            IDSetNumber(&FilterNP, "Setting current filter to slot %d", targetFilter);
            IDLog("Setting current filter to slot %d\n", targetFilter);

            try {
                QSICam.put_Position(targetFilter);
            } catch(std::runtime_error err) {
                FilterNP.s = IPS_ALERT;
                IDSetNumber(&FilterNP, "put_Position() failed. %s.", err.what());
                IDLog("put_Position() failed. %s.", err.what());
                return;
            }

            /* Check current filter position */
            short newFilter;
            try {
                QSICam.get_Position(&newFilter);
            } catch(std::runtime_error err) {
                FilterNP.s = IPS_ALERT;
                IDSetNumber(&FilterNP, "get_Position() failed. %s.", err.what());
                IDLog("get_Position() failed. %s.\n", err.what());
                return;
            }

            if (newFilter == targetFilter) {
                FilterN[0].value = targetFilter;
                FilterNP.s = IPS_OK;
                IDSetNumber(&FilterNP, "Filter set to slot #%d", targetFilter);
                return;
            }

            return;
        }
}

void ISPoll(void *p) {

	long err;
	long timeleft;
	double ccdTemp;
        double coolerPower;
	
	if (!isCCDConnected()) {
	    IEAddTimer (POLLMS, ISPoll, NULL);
	    return;
	}

        switch (ExposeTimeLeftNP.s) {
          case IPS_IDLE:
            break;

          case IPS_OK:
            break;

          case IPS_BUSY:
            timeleft = QSIImg->timeLeft();
            ExposeTimeLeftN[0].value = timeleft;

            if(timeleft <= 0) {
                ExposeTimeLeftN[0].value = 0;
                ExposeTimeLeftNP.s = IPS_OK;
            }


            IDSetNumber(&ExposeTimeLeftNP, NULL);

            break;

          case IPS_ALERT:
            break;
        }
	 
	switch (ExposeTimeWNP.s) {
	  case IPS_IDLE:
	    break;
	    
	  case IPS_OK:
	    break;
	    
	  case IPS_BUSY:

            bool imageReady;
            try {
                QSICam.get_ImageReady(&imageReady);
            } catch (std::runtime_error err) {
                ExposeTimeWNP.s = IPS_ALERT;

                IDSetNumber(&ExposeTimeWNP, "get_ImageReady() failed. %s.", err.what());
                IDLog("get_ImageReady() failed. %s.", err.what());
                break;
            }

            if(!imageReady) {
                break;
            }

	    /* We're done exposing */
	    ExposeTimeWNP.s = IPS_OK;
	    IDSetNumber(&ExposeTimeWNP, "Exposure done, downloading image...");
	    IDLog("Exposure done, downloading image...\n");

	    /* grab and save image */
	    grabImage();

	    break;
	    
	  case IPS_ALERT:
	    break;
	}
	 
	switch (TemperatureNP.s) {
	  case IPS_IDLE:
	  case IPS_OK:
            try {
                QSICam.get_CCDTemperature(&ccdTemp);
            } catch (std::runtime_error err) {
	        TemperatureNP.s = IPS_IDLE;
	        IDSetNumber(&TemperatureNP, "get_CCDTemperature() failed. %s.", err.what());
	        IDLog("get_CCDTemperature() failed. %s.", err.what());
	        return;
	    }
	     
	    if (fabs(TemperatureN[0].value - ccdTemp) >= TEMP_THRESHOLD) {
	       TemperatureN[0].value = ccdTemp;
	       IDSetNumber(&TemperatureNP, NULL);
	    }
	    break;
	     
	  case IPS_BUSY:
            try {
                QSICam.get_CCDTemperature(&ccdTemp);
            } catch (std::runtime_error err) {
                TemperatureNP.s = IPS_ALERT;
                IDSetNumber(&TemperatureNP, "get_CCDTemperature() failed. %s.", err.what());
                IDLog("get_CCDTemperature() failed. %s.", err.what());
                return;
            }
	     
	    if (fabs(TemperatureN[0].value - ccdTemp) <= TEMP_THRESHOLD)
	        TemperatureNP.s = IPS_OK;
	     	       
            TemperatureN[0].value = ccdTemp;
	    IDSetNumber(&TemperatureNP, NULL);
	    break;
	      
	  case IPS_ALERT:
	    break;
        }

        switch (CoolerNP.s) {
          case IPS_IDLE:
          case IPS_OK:
            try {
                QSICam.get_CoolerPower(&coolerPower);
            } catch (std::runtime_error err) {
                CoolerNP.s = IPS_IDLE;
                IDSetNumber(&CoolerNP, "get_CoolerPower() failed. %s.", err.what());
                IDLog("get_CoolerPower() failed. %s.", err.what());
                return;
            }
            CoolerN[0].value = coolerPower;
            IDSetNumber(&CoolerNP, NULL);

            break;

          case IPS_BUSY:
            try {
                QSICam.get_CoolerPower(&coolerPower);
            } catch (std::runtime_error err) {
                CoolerNP.s = IPS_ALERT;
                IDSetNumber(&CoolerNP, "get_CoolerPower() failed. %s.", err.what());
                IDLog("get_CoolerPower() failed. %s.", err.what());
                return;
            }
            CoolerNP.s = IPS_OK;

            CoolerN[0].value = coolerPower;
            IDSetNumber(&CoolerNP, NULL);
            break;

          case IPS_ALERT:
             break;
        }
  	 
        switch (ResetSP.s) {
          case IPS_IDLE:
             break;
          case IPS_OK:
             break;
          case IPS_BUSY:
             break;
          case IPS_ALERT:
             break;
        }

        switch (FilterNP.s) {

          case IPS_IDLE:
          case IPS_OK:
            short current_filter;
            try {
                QSICam.get_Position(&current_filter);
            } catch (std::runtime_error err) {
                FilterNP.s = IPS_ALERT;
                IDSetNumber(&FilterNP, "QSICamera::get_FilterPos() failed. %s.", err.what());
                IDLog("QSICamera::get_FilterPos() failed. %s.\n", err.what());
                return;
            }

            FilterN[0].value = current_filter;
            IDSetNumber(&FilterNP, NULL);
            break;

          case IPS_BUSY:
            break;

          case IPS_ALERT:
            break;
        }

        p=p; 
 	
        IEAddTimer (POLLMS, ISPoll, NULL);
}

/* Sets the Image area that the CCD will scan and download.
   We compensate for binning. */
bool setImageArea(char errmsg[]) {

        /* Add the X and Y offsets */
        long x_1 = FrameN[0].value;
        long y_1 = FrameN[1].value;

        long x_2 = x_1 + (FrameN[2].value / SetBinningN[0].value);
        long y_2 = y_1 + (FrameN[3].value / SetBinningN[1].value);

        long sensorPixelSize_x,
             sensorPixelSize_y;
        try {
            QSICam.get_CameraXSize(&sensorPixelSize_x);
            QSICam.get_CameraYSize(&sensorPixelSize_y);
        } catch (std::runtime_error err) {
            IDMessage(mydev, "Getting image area size failed. %s.", err.what());
        }
   
        if (x_2 > sensorPixelSize_x / SetBinningN[0].value)
            x_2 = sensorPixelSize_x / SetBinningN[0].value;
   
        if (y_2 > sensorPixelSize_y / SetBinningN[1].value)
            y_2 = sensorPixelSize_y / SetBinningN[1].value;

        IDLog("The Final image area is (%ld, %ld), (%ld, %ld)\n", x_1, y_1, x_2, y_2);
   
        QSIImg->width  = x_2 - x_1;
        QSIImg->height = y_2 - y_1;

        try {
            QSICam.put_StartX(x_1);
            QSICam.put_StartY(y_1);
            QSICam.put_NumX(QSIImg->width);
            QSICam.put_NumY(QSIImg->height);
        } catch (std::runtime_error err) {
            snprintf(errmsg, ERRMSG_SIZE, "Setting image area failed. %s.\n",err.what());
            IDMessage(mydev, "Setting image area failed. %s.", err.what());
            IDLog("Setting image area failed. %s.", err.what());
            return false;
        }

        return true;
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int grabImage() {

	int fd;
	char errmsg[ERRMSG_SIZE];
	char filename[TEMPFILE_LEN] = "/tmp/fitsXXXXXX";
  
	if ((fd = mkstemp(filename)) < 0) { 
		IDMessage(mydev, "Error making temporary filename.");
		IDLog("Error making temporary filename.\n");
		return -1;
	}
	close(fd);

        int x,y,z;
        try {
            bool imageReady = false;
            QSICam.get_ImageReady(&imageReady);
            while(!imageReady){
                usleep(5000);
                QSICam.get_ImageReady(&imageReady);
            }

            QSICam.get_ImageArraySize(x,y,z);
            unsigned short* image = new unsigned short[ x * y ];
            QSICam.get_ImageArray(image);
            QSIImg->img = image;
            QSIImg->width  = x;
            QSIImg->height = y;
        } catch (std::runtime_error err) {
            IDMessage(mydev, "get_ImageArray() failed. %s.", err.what());
            IDLog("get_ImageArray() failed. %s.", err.what());
            return -1;
        }

	IDMessage(mydev, "Download complete.\n");
	
	/*err = (ImageFormatS[0].s == ISS_ON) ? writeFITS(FileNameT[0].text, errmsg) : writeRAW(FileNameT[0].text, errmsg);*/
	int err = writeFITS(filename, errmsg);
   
	if (err) {
		free(QSIImg->img);
		IDMessage(mydev, errmsg, NULL);
		return -1;
	}

	free(QSIImg->img);
	return 0;
}

int writeFITS(const char* filename, char errmsg[]) {
        fitsfile *fptr;       /* pointer to the FITS file; defined in fitsio.h */
        int status;
        long  fpixel = 1, naxis = 2, nelements;
        long naxes[2];
        char filename_rw[TEMPFILE_LEN+1];

        naxes[0] = QSIImg->width;
        naxes[1] = QSIImg->height;



        /* Append ! to file name to over write it.*/
        snprintf(filename_rw, TEMPFILE_LEN+1, "!%s", filename);

        status = 0;         /* initialize status before calling fitsio routines */
        fits_create_file(&fptr, filename_rw, &status);   /* create new file */

        /* Create the primary array image (16-bit short integer pixels */
        fits_create_img(fptr, USHORT_IMG, naxis, naxes, &status);

        addFITSKeywords(fptr);

        nelements = naxes[0] * naxes[1];          /* number of pixels to write */

        /* Write the array of integers to the image */
        fits_write_img(fptr, TUSHORT, fpixel, nelements, QSIImg->img, &status);

        fits_close_file(fptr, &status);            /* close the file */

        fits_report_error(stderr, status);  /* print out any error messages */

        /* Success */
        ExposeTimeWNP.s = IPS_OK;
        IDSetNumber(&ExposeTimeWNP, NULL);
        uploadFile(filename);
        unlink(filename);
 
        return status;
}

void addFITSKeywords(fitsfile *fptr) {

        int status=0; 
        char binning_s[32];
        char frame_s[32];
        double min_val = min();
        double max_val = max();
        double exposure = 1000 * QSIImg->expose; // conversion s - ms  

        snprintf(binning_s, 32, "(%g x %g)", SetBinningN[0].value, SetBinningN[1].value);

        switch (QSIImg->frameType) {
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

        char name_s[32] = "QSI";
        double electronsPerADU;
        short filter;
        try {
            string name;
            QSICam.get_Name(name);
            for(unsigned i = 0; i < 18; ++i) name_s[i] = name[i];
            QSICam.get_ElectronsPerADU(&electronsPerADU);
            QSICam.get_Position(&filter);
        } catch (std::runtime_error& err) {
            IDMessage(mydev, "get_Name() failed. %s.", err.what());
            IDLog("get_Name() failed. %s.\n", err.what());
            return;
        }

        fits_update_key_s(fptr, TDOUBLE, "CCD-TEMP", &(TemperatureN[0].value), "CCD Temperature (Celcius)", &status);
        fits_update_key_s(fptr, TDOUBLE, "EXPTIME", &(QSIImg->expose), "Total Exposure Time (s)", &status);
        if(QSIImg->frameType == DARK_FRAME)
        fits_update_key_s(fptr, TDOUBLE, "DARKTIME", &(QSIImg->expose), "Total Exposure Time (s)", &status);
        fits_update_key_s(fptr, TDOUBLE, "PIX-SIZ", &(PixelSizeN[0].value), "Pixel Size (microns)", &status);
        fits_update_key_s(fptr, TSTRING, "BINNING", binning_s, "Binning HOR x VER", &status);
        fits_update_key_s(fptr, TSTRING, "FRAME", frame_s, "Frame Type", &status);
        fits_update_key_s(fptr, TDOUBLE, "DATAMIN", &min_val, "Minimum value", &status);
        fits_update_key_s(fptr, TDOUBLE, "DATAMAX", &max_val, "Maximum value", &status);
        fits_update_key_s(fptr, TSTRING, "INSTRUME", name_s, "CCD Name", &status);
        fits_update_key_s(fptr, TDOUBLE, "EPERADU", &electronsPerADU, "Electrons per ADU", &status);

        fits_write_date(fptr, &status);
}

void fits_update_key_s(fitsfile* fptr, int type, string name, void* p, string explanation, int* status){
        // this function is for removing warnings about deprecated string conversion to char* (from arg 5)
        fits_update_key(fptr,type,name.c_str(),p, const_cast<char*>(explanation.c_str()), status);
}

void uploadFile(const char* filename) {
        FILE * fitsFile;
        unsigned char *fitsData, *compressedData;
        int r=0;
        unsigned int i =0, nr = 0;
        uLongf compressedBytes=0;
        uLong  totalBytes;
        struct stat stat_p; 
 
        if ( -1 ==  stat (filename, &stat_p)) { 
            IDLog(" Error occurred attempting to stat file.\n"); 
            return; 
        }
   
        totalBytes     = stat_p.st_size;
        fitsData       = (unsigned char *) malloc (sizeof(unsigned char) * totalBytes);
        compressedData = (unsigned char *) malloc (sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   
        if (fitsData == NULL || compressedData == NULL) {
            if (fitsData) free(fitsData);
            if (compressedData) free(compressedData);
            IDLog("Error! low memory. Unable to initialize fits buffers.\n");
            return;
        }
   
        fitsFile = fopen(filename, "r");
   
        if (fitsFile == NULL)
        return;
   
        /* #1 Read file from disk */ 
        for (i=0; i < totalBytes; i+= nr) {
            nr = fread(fitsData + i, 1, totalBytes - i, fitsFile);
     
            if (nr <= 0) {
                IDLog("Error reading temporary FITS file.\n");
                return;
            }
        }
        fclose(fitsFile);
   
        compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
    
        /* #2 Compress it */ 
        r = compress2(compressedData, &compressedBytes, fitsData, totalBytes, 9);
        if (r != Z_OK) {
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
void handleExposure(void *p) {

        /* no warning */
        p=p;

        /* BIAS frame is the same as DARK but with minimum period. i.e. readout from camera electronics.*/

        if (QSIImg->frameType == BIAS_FRAME) {
            try {
                double minDuration;
                QSICam.get_MinExposureTime(&minDuration);
                QSICam.put_PreExposureFlush(QSICamera::FlushNormal);
                QSICam.StartExposure (minDuration,false);
            } catch (std::runtime_error& err) {
                ExposeTimeWNP.s = IPS_ALERT;
                IDMessage(mydev, "StartExposure() failed. %s.", err.what());
                IDLog("StartExposure() failed. %s.\n", err.what());
                return;
            }
        }

        if (QSIImg->frameType == DARK_FRAME) {
            try {
                QSICam.put_PreExposureFlush(QSICamera::FlushNormal);
                QSICam.StartExposure (QSIImg->expose,false);
            } catch (std::runtime_error& err) {
                ExposeTimeWNP.s = IPS_ALERT;
                IDMessage(mydev, "StartExposure() failed. %s.", err.what());
                IDLog("StartExposure() failed. %s.\n", err.what());
                return;
            }
        }

        if (QSIImg->frameType == LIGHT_FRAME || QSIImg->frameType == FLAT_FRAME) {
            try {
                QSICam.put_PreExposureFlush(QSICamera::FlushNormal);
                QSICam.StartExposure (QSIImg->expose,true);
            } catch (std::runtime_error& err) {
                ExposeTimeWNP.s = IPS_ALERT;
                IDMessage(mydev, "StartExposure() failed. %s.", err.what());
                IDLog("StartExposure() failed. %s.\n", err.what());
                return;
            }
        }
        QSIImg->setStartTime();
        ExposeTimeWNP.s = IPS_BUSY;		  
        IDSetNumber(&ExposeTimeWNP, "Taking a %g seconds frame...", QSIImg->expose);

        ExposeTimeLeftN[0].value = QSIImg->timeLeft();
        ExposeTimeLeftNP.s = IPS_BUSY;
        IDSetNumber(&ExposeTimeLeftNP,NULL);

        IDLog("Taking a frame...\n");
}

/* Retrieves basic data from the CCD upon connection like temperature, array size, firmware..etc */
void getBasicData() {

        IDLog("In getBasicData()\n");

        string name,model;
        double temperature;
        double pixel_size_x,pixel_size_y;
        long sub_frame_x,sub_frame_y;
        try {
            QSICam.get_Name(name);
            QSICam.get_ModelNumber(model);
            QSICam.get_PixelSizeX(&pixel_size_x);
            QSICam.get_PixelSizeY(&pixel_size_y);
            QSICam.get_NumX(&sub_frame_x);
            QSICam.get_NumY(&sub_frame_y);
            QSICam.get_CCDTemperature(&temperature);
        } catch (std::runtime_error err) {
            IDMessage(mydev, "getBasicData failed. %s.", err.what());
            IDLog("getBasicData failed. %s.", err.what());
            return;
        }
  

        IDMessage(mydev, "The CCD Temperature is %f.\n", temperature);
        IDLog("The CCD Temperature is %f.\n", temperature);
  
        PixelSizeN[0].value   = pixel_size_x;			/* Pixel width (um) */
        PixelSizeN[1].value   = pixel_size_y;			/* Pixel height (um) */
        TemperatureN[0].value = temperature;			/* CCD chip temperatre (degrees C) */
        FrameN[0].value = 0;					/* X */
        FrameN[1].value = 0;					/* Y */
        FrameN[2].value = sub_frame_x;                              /* Frame Width */
        FrameN[3].value = sub_frame_y;                              /* Frame Height */

        QSIImg->width  = FrameN[2].value;
        QSIImg->height = FrameN[3].value;

        SetBinningN[0].value = SetBinningN[1].value = 1;
  
        IDSetNumber(&PixelSizeNP, NULL);
        IDSetNumber(&TemperatureNP, NULL);
        IDSetNumber(&FrameNP, NULL);
        IDSetNumber(&SetBinningNP, NULL);

        try {
            QSICam.get_Name(name);
        } catch (std::runtime_error& err) {
            IDMessage(mydev, "get_Name() failed. %s.", err.what());
            IDLog("get_Name() failed. %s.\n", err.what());
            return;
        }
        IDMessage(mydev,name.c_str());
        IDLog(name.c_str());

        int filter_count;
        try {
                QSICam.get_FilterCount(filter_count);
        } catch (std::runtime_error err) {
                IDMessage(mydev, "get_FilterCount() failed. %s.", err.what());
                IDLog("get_FilterCount() failed. %s.\n", err.what());
                return;
        }

        IDMessage(mydev,"The filter count is %ld\n", filter_count);
        IDLog("The filter count is %ld\n", filter_count);

        FilterN[0].max = filter_count - 1;
        FilterNP.s = IPS_OK;

        IUUpdateMinMax(&FilterNP);
        IDSetNumber(&FilterNP, "Setting max number of filters.\n");

        FilterSP.s = IPS_OK;
        IDSetSwitch(&FilterSP,NULL);
}

int manageDefaults(char errmsg[]) {

        long err;
  
        /* X horizontal binning */
        try {
            QSICam.put_BinX(SetBinningN[0].value);
        } catch (std::runtime_error err) {
            IDSetSwitch(&ConnectSP, "Error: put_BinX() failed. %s.", err.what());
            IDLog("Error: put_BinX() failed. %s.\n", err.what());
            return -1;
        }

        /* Y vertical binning */
        try {
            QSICam.put_BinY(SetBinningN[1].value);
        } catch (std::runtime_error err) {
            IDSetSwitch(&ConnectSP, "Error: put_BinY() failed. %s.", err.what());
            IDLog("Error: put_BinX() failed. %s.\n", err.what());
            return -1;
        }

        IDLog("Setting default binning %f x %f.\n", SetBinningN[0].value, SetBinningN[1].value);

        /* Set image area */
        if (!setImageArea(errmsg)) return -1;

        short current_filter;
        try {
            QSICam.put_Position(targetFilter);
            QSICam.get_Position(&current_filter);
        } catch (std::runtime_error err) {
            IDMessage(mydev, "QSICamera::get_FilterPos() failed. %s.", err.what());
            IDLog("QSICamera::get_FilterPos() failed. %s.\n", err.what());
            errmsg = const_cast<char*>(err.what());
            return true;
        }

        IDMessage(mydev,"The current filter is %ld\n", current_filter);
        IDLog("The current filter is %ld\n", current_filter);

        FilterN[0].value = current_filter;
        IDSetNumber(&FilterNP, "Storing defaults");

        /* Success */
        return 0;
}

int getOnSwitch(ISwitchVectorProperty *sp) {

        for (int i = 0; i < sp->nsp ; i++) {
            if (sp->sp[i].s == ISS_ON)
            return i;
        }
        return -1;
}

int checkPowerS(ISwitchVectorProperty *sp) {

        if (ConnectSP.s != IPS_OK) {
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

int checkPowerN(INumberVectorProperty *np) {

        if (ConnectSP.s != IPS_OK) {
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

int checkPowerT(ITextVectorProperty *tp) {

        if (ConnectSP.s != IPS_OK) {
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

void connectCCD() {

	IDLog ("Connecting CCD\n");
   
	switch (ConnectS[0].s) {
	  case ISS_ON:
    	    IDLog("Attempting to find the camera\n");

            bool connected;
            try {
                QSICam.get_Connected(&connected);
            } catch (std::runtime_error err) {
                ConnectSP.s = IPS_IDLE;
                ConnectS[0].s = ISS_OFF;
                ConnectS[1].s = ISS_ON;
                IDSetSwitch(&ConnectSP, "Error: get_Connected() failed. %s.", err.what());
                IDLog("Error: get_Connected() failed. %s.\n", err.what());
                return;
            }
            if(!connected){
                try {
                    QSICam.put_Connected(true);
                } catch (std::runtime_error err) {
                    ConnectSP.s = IPS_IDLE;
                    ConnectS[0].s = ISS_OFF;
                    ConnectS[1].s = ISS_ON;
                    IDSetSwitch(&ConnectSP, "Error: put_Connected(true) failed. %s.", err.what());
                    IDLog("Error: put_Connected(true) failed. %s.\n", err.what());
                    return;
                }
            }


            /* Success! */
            ConnectS[0].s = ISS_ON;
            ConnectS[1].s = ISS_OFF;
            ConnectSP.s = IPS_OK;
            IDSetSwitch(&ConnectSP, "CCD is online. Retrieving basic data.");
            IDLog("CCD is online. Retrieving basic data.\n");
            getBasicData();

            char errmsg[ERRMSG_SIZE];
            if (manageDefaults(errmsg)) {
                IDMessage(mydev, errmsg, NULL);
                IDLog("%s", errmsg);
                return;
            }

            break;

          case ISS_OFF:
            ConnectS[0].s = ISS_OFF;
            ConnectS[1].s = ISS_ON;
	    ConnectSP.s = IPS_IDLE;

            try {
                QSICam.get_Connected(&connected);
            } catch (std::runtime_error err) {
                IDSetSwitch(&ConnectSP, "Error: get_Connected() failed. %s.", err.what());
                IDLog("Error: get_Connected() failed. %s.\n", err.what());
                return;
            }
            if(connected){
                try {
                    QSICam.put_Connected(false);
                } catch (std::runtime_error err) {
                    ConnectSP.s = IPS_ALERT;
                    IDSetSwitch(&ConnectSP, "Error: put_Connected(false) failed. %s.", err.what());
                    IDLog("Error: put_Connected(false) failed. %s.\n", err.what());
                    return;
                }
            }

    	    IDSetSwitch(&ConnectSP, "CCD is offline.");
            break;
        }
}

/* isCCDConnected: return 1 if we have a connection, 0 otherwise */
int isCCDConnected(void) {
        return ((ConnectS[0].s == ISS_ON) ? 1 : 0);
}

void activateCooler() {

        bool coolerOn;

        switch (CoolerS[0].s) {
          case ISS_ON:
            try {
                QSICam.get_CoolerOn(&coolerOn);
            } catch (std::runtime_error err) {
                CoolerSP.s = IPS_IDLE;
                CoolerS[0].s = ISS_OFF;
                CoolerS[1].s = ISS_ON;
                IDSetSwitch(&ConnectSP, "Error: CoolerOn() failed. %s.", err.what());
                IDLog("Error: CoolerOn() failed. %s.\n", err.what());
                return;
            }

            if(!coolerOn){
                try {
                    QSICam.put_CoolerOn(true);
                } catch (std::runtime_error err) {
                    CoolerSP.s = IPS_IDLE;
                    CoolerS[0].s = ISS_OFF;
                    CoolerS[1].s = ISS_ON;
                    IDSetSwitch(&ConnectSP, "Error: put_CoolerOn(true) failed. %s.", err.what());
                    IDLog("Error: put_CoolerOn(true) failed. %s.\n", err.what());
                    return;
                }
            }

            /* Success! */
            CoolerS[0].s = ISS_ON;
            CoolerS[1].s = ISS_OFF;
            CoolerSP.s = IPS_OK;
            IDSetSwitch(&CoolerSP, "Cooler ON\n");
            IDLog("Cooler ON\n");

            break;

          case ISS_OFF:
              CoolerS[0].s = ISS_OFF;
              CoolerS[1].s = ISS_ON;
              CoolerSP.s = IPS_IDLE;

              try {
                 QSICam.get_CoolerOn(&coolerOn);
                 if(coolerOn) QSICam.put_CoolerOn(false);
              } catch (std::runtime_error err) {
                 IDSetSwitch(&ConnectSP, "Error: CoolerOn() failed. %s.", err.what());
                 IDLog("Error: CoolerOn() failed. %s.\n", err.what());
                 return;                                              
              }
              IDSetSwitch(&CoolerSP, "Cooler is OFF.");
              break;
        }
}

double min() {

        double lmin = QSIImg->img[0];
        int ind=0, i, j;
  
        for (i= 0; i < QSIImg->height ; i++)
            for (j= 0; j < QSIImg->width; j++) {
                ind = (i * QSIImg->width) + j;
                if (QSIImg->img[ind] < lmin) lmin = QSIImg->img[ind];
        }
    
        return lmin;
}

double max() {

        double lmax = QSIImg->img[0];
        int ind=0, i, j;
  
        for (i= 0; i < QSIImg->height ; i++)
            for (j= 0; j < QSIImg->width; j++) {
                ind = (i * QSIImg->width) + j;
                if (QSIImg->img[ind] > lmax) lmax = QSIImg->img[ind];
        }
    
        return lmax;
}

void resetFrame(){

        long sensorPixelSize_x,
             sensorPixelSize_y;
        try {
            QSICam.get_CameraXSize(&sensorPixelSize_x);
            QSICam.get_CameraYSize(&sensorPixelSize_y);
        } catch (std::runtime_error err) {
            IDMessage(mydev, "Getting image area size failed. %s.", err.what());
        }

        QSIImg->width  = sensorPixelSize_x;
        QSIImg->height = sensorPixelSize_y;

        FrameN[0].value = 0;                                        /* X */
        FrameN[1].value = 0;                                        /* Y */
        FrameN[2].value = sensorPixelSize_x;                        /* Frame Width */
        FrameN[3].value = sensorPixelSize_y;                        /* Frame Height */
        IDSetNumber(&FrameNP, NULL);

        try {
            QSICam.put_BinX(1);
            QSICam.put_BinY(1);
        } catch (std::runtime_error err) {
            IDSetNumber(&SetBinningNP, "Resetting BinX/BinY failed. %s.", err.what());
            IDLog("Resetting BinX/BinY failed. %s.", err.what());
            return;
        }

        SetBinningN[0].value = SetBinningN[1].value = 1;
        IDSetNumber(&SetBinningNP, NULL);

        ResetSP.s = IPS_IDLE;
        IDSetSwitch(&ResetSP, "Resetting frame and binning.");

        char errmsg[ERRMSG_SIZE];
        if(!setImageArea(errmsg)) {
            IDSetNumber(&FrameNP, "%s",errmsg);
            return;
        }

        return;
}

void turnWheel() {

        short current_filter;

        switch (FilterS[0].s) {
          case ISS_ON:
            if(current_filter < LAST_FILTER) current_filter++;
            else current_filter = FIRST_FILTER;
            try {
                QSICam.get_Position(&current_filter);
                if(current_filter < LAST_FILTER) current_filter++;
                else current_filter = FIRST_FILTER;
                QSICam.put_Position(current_filter);
            } catch (std::runtime_error err) {
                FilterSP.s = IPS_IDLE;
                FilterS[0].s = ISS_OFF;
                FilterS[1].s = ISS_OFF;
                IDMessage(mydev, "QSICamera::get_FilterPos() failed. %s.", err.what());
                IDLog("QSICamera::get_FilterPos() failed. %s.\n", err.what());
                return;
            }

            FilterN[0].value = current_filter;
            FilterS[0].s = ISS_OFF;
            FilterS[1].s = ISS_OFF;
            FilterSP.s = IPS_OK;
            IDSetSwitch(&FilterSP,"The current filter is %ld\n", current_filter);
            IDLog("The current filter is %ld\n", current_filter);

            break;

          case ISS_OFF:
            try {
                QSICam.get_Position(&current_filter);
                if(current_filter > FIRST_FILTER) current_filter--;
                else current_filter = LAST_FILTER;
                QSICam.put_Position(current_filter);
            } catch (std::runtime_error err) {
                FilterSP.s = IPS_IDLE;
                FilterS[0].s = ISS_OFF;
                FilterS[1].s = ISS_OFF;
                IDMessage(mydev, "QSICamera::get_FilterPos() failed. %s.", err.what());
                IDLog("QSICamera::get_FilterPos() failed. %s.\n", err.what());
                return;
            }

            FilterN[0].value = current_filter;
            FilterS[0].s = ISS_OFF;
            FilterS[1].s = ISS_OFF;
            FilterSP.s = IPS_OK;
            IDSetSwitch(&FilterSP,"The current filter is %ld\n", current_filter);
            IDLog("The current filter is %ld\n", current_filter);

            break;
        }
}

void shutterControl() {

        bool hasShutter;
        try{
            QSICam.get_HasShutter(&hasShutter);
        }catch (std::runtime_error err) {
            ShutterSP.s   = IPS_IDLE;
            ShutterS[0].s = ISS_OFF;
            ShutterS[1].s = ISS_OFF;
            IDMessage(mydev, "QSICamera::get_HasShutter() failed. %s.", err.what());
            IDLog("QSICamera::get_HasShutter() failed. %s.\n", err.what());
            return;
        }

        if(hasShutter){
            switch (ShutterS[0].s) {
              case ISS_ON:

                try{
                    QSICam.put_ManualShutterMode(true);
                    QSICam.put_ManualShutterOpen(true);
                }catch (std::runtime_error err) {
                    ShutterSP.s = IPS_IDLE;
                    ShutterS[0].s = ISS_OFF;
                    ShutterS[1].s = ISS_ON;
                    IDSetSwitch(&ShutterSP, "Error: ManualShutterOpen() failed. %s.", err.what());
                    IDLog("Error: ManualShutterOpen() failed. %s.\n", err.what());
                    return;
                }

                /* Success! */
                ShutterS[0].s = ISS_ON;
                ShutterS[1].s = ISS_OFF;
                ShutterSP.s = IPS_OK;
                IDSetSwitch(&ShutterSP, "Shutter opened manually.");
                IDLog("Shutter opened manually.\n");

                break;

              case ISS_OFF:

                try{
                    QSICam.put_ManualShutterOpen(false);
                    QSICam.put_ManualShutterMode(false);
                }catch (std::runtime_error err) {
                    ShutterSP.s = IPS_IDLE;
                    ShutterS[0].s = ISS_ON;
                    ShutterS[1].s = ISS_OFF;
                    IDSetSwitch(&ShutterSP, "Error: ManualShutterOpen() failed. %s.", err.what());
                    IDLog("Error: ManualShutterOpen() failed. %s.\n", err.what());
                    return;
                }

                /* Success! */
                ShutterS[0].s = ISS_OFF;
                ShutterS[1].s = ISS_ON;
                ShutterSP.s = IPS_IDLE;
                IDSetSwitch(&ShutterSP, "Shutter closed manually.");
                IDLog("Shutter closed manually.\n");

                break;
            }
        }
}
