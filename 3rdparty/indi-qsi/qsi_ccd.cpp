#if 0
    QSI CCD
    INDI Interface for Quantum Scientific Imaging CCDs
    Based on FLI Indi Driver by Jasem Mutlaq.
    Copyright (C) 2009 Sami Lehti (sami.lehti@helsinki.fi)

    (2011-12-10) Updated by Jasem Mutlaq:
        + Driver completely rewritten to be based on INDI::CCD
        + Fixed compiler warnings.
        + Reduced message traffic.
        + Added filter name property.
        + Added snooping on telescopes.
        + Added Guider support.
        + Added Readout speed option.

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

#include <memory>

#include <fitsio.h>

#include "qsiapi.h"
#include "QSIError.h"
#include "indidevapi.h"
#include "eventloop.h"
#include "indicom.h"
#include "qsi_ccd.h"

void ISInit(void);
void ISPoll(void *);

double min(void);
double max(void);

#define FILTER_WHEEL_TAB	"Filter Wheel"

#define MAX_CCD_TEMP	45		/* Max CCD temperature */
#define MIN_CCD_TEMP	-55		/* Min CCD temperature */
#define MAX_X_BIN	16		/* Max Horizontal binning */
#define MAX_Y_BIN	16		/* Max Vertical binning */
#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define POLLMS		1000		/* Polling time (ms) */
#define TEMP_THRESHOLD  .25		/* Differential temperature threshold (C)*/
#define NFLUSHES	1		/* Number of times a CCD array is flushed before an exposure */

#define LAST_FILTER  5          /* Max slot index */
#define FIRST_FILTER 1          /* Min slot index */

#define currentFilter   FilterN[0].value

std::auto_ptr<QSICCD> qsiCCD(0);

 void ISInit()
 {
    static int isInit =0;

    if (isInit == 1)
        return;

     isInit = 1;
     if(qsiCCD.get() == 0) qsiCCD.reset(new QSICCD());
     //IEAddTimer(POLLMS, ISPoll, NULL);

 }

 void ISGetProperties(const char *dev)
 {
         ISInit();
         qsiCCD->ISGetProperties(dev);
 }

 void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
 {
         ISInit();
         qsiCCD->ISNewSwitch(dev, name, states, names, num);
 }

 void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
 {
         ISInit();
         qsiCCD->ISNewText(dev, name, texts, names, num);
 }

 void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
 {
         ISInit();
         qsiCCD->ISNewNumber(dev, name, values, names, num);
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
     qsiCCD->ISSnoopDevice(root);
 }


 QSICCD::QSICCD()
 {
     targetFilter = 0;

     QSICam.put_UseStructuredExceptions(true);

 }


QSICCD::~QSICCD()
{

}

const char * QSICCD::getDefaultName()
{
    return (char *)"QSI CCD";
}

bool QSICCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    IUFillSwitch(&CoolerS[0], "CONNECT_COOLER", "ON", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "DISCONNECT_COOLER", "OFF", ISS_OFF);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "COOLER_CONNECTION", "Cooler", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&ShutterS[0], "SHUTTER_ON", "Manual open", ISS_OFF);
    IUFillSwitch(&ShutterS[1], "SHUTTER_OFF", "Manual close", ISS_OFF);
    IUFillSwitchVector(&ShutterSP, ShutterS, 2, getDeviceName(), "SHUTTER_CONNECTION", "Shutter", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER", "Cooling Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&ReadOutS[0], "QUALITY_HIGH", "High Quality", ISS_OFF);
    IUFillSwitch(&ReadOutS[1], "QUALITY_LOW", "Fast", ISS_OFF);
    IUFillSwitchVector(&ReadOutSP, ReadOutS, 2, getDeviceName(), "READOUT_QUALITY", "Readout Speed", OPTIONS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&FilterS[0], "FILTER_CW", "+", ISS_OFF);
    IUFillSwitch(&FilterS[1], "FILTER_CCW", "-", ISS_OFF);
    IUFillSwitchVector(&FilterSP, FilterS, 2, getDeviceName(), "FILTER_WHEEL_MOTION", "Turn Wheel", FILTER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    initFilterProperties(getDeviceName(), FILTER_TAB);

    addDebugControl();
}

bool QSICCD::updateProperties()
{

    INDI::CCD::updateProperties();

    if (isConnected())
    {
        defineSwitch(&CoolerSP);
        defineSwitch(&ShutterSP);
        defineNumber(&CoolerNP);
        defineNumber(&FilterSlotNP);
        defineSwitch(&FilterSP);
        defineSwitch(&ReadOutSP);

        setupParams();

        if (FilterNameT != NULL)
            defineText(FilterNameTP);

        manageDefaults();

        timerID = SetTimer(POLLMS);
    }
    else
    {
        deleteProperty(CoolerSP.name);
        deleteProperty(ShutterSP.name);
        deleteProperty(CoolerNP.name);
        deleteProperty(FilterSlotNP.name);
        deleteProperty(FilterSP.name);
        deleteProperty(ReadOutSP.name);
        if (FilterNameT != NULL)
            deleteProperty(FilterNameTP->name);

        rmTimer(timerID);
    }

    return true;
}

bool QSICCD::setupParams()
{
    string name,model;
    double temperature;
    double pixel_size_x,pixel_size_y;
    long sub_frame_x,sub_frame_y;
    try
    {
        QSICam.get_Name(name);
        QSICam.get_ModelNumber(model);
        QSICam.get_PixelSizeX(&pixel_size_x);
        QSICam.get_PixelSizeY(&pixel_size_y);
        QSICam.get_NumX(&sub_frame_x);
        QSICam.get_NumY(&sub_frame_y);
        QSICam.get_CCDTemperature(&temperature);
    } catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Setup Params failed. %s.", err.what());
        return false;
    }


    DEBUGF(INDI::Logger::DBG_SESSION, "The CCD Temperature is %f.", temperature);

    TemperatureN[0].value = temperature;			/* CCD chip temperatre (degrees C) */

    SetCCDParams(sub_frame_x, sub_frame_y, 16, pixel_size_x, pixel_size_y);

    imageWidth  = PrimaryCCD.getSubW();
    imageHeight = PrimaryCCD.getSubH();

    IDSetNumber(&TemperatureNP, NULL);

    try
    {
     QSICam.get_Name(name);
    } catch (std::runtime_error& err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "get_Name() failed. %s.", err.what());
        return false;
    }
    DEBUGF(INDI::Logger::DBG_SESSION, "%s", name.c_str());

    int filter_count;
    try
    {
            QSICam.get_FilterCount(filter_count);
    } catch (std::runtime_error err)
    {
            DEBUGF(INDI::Logger::DBG_SESSION, "get_FilterCount() failed. %s.", err.what());
            return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION,"The filter count is %d", filter_count);


    FilterSlotN[0].max = filter_count;
    FilterSlotNP.s = IPS_OK;

    IUUpdateMinMax(&FilterSlotNP);
    IDSetNumber(&FilterSlotNP, NULL);

    FilterSP.s = IPS_OK;
    IDSetSwitch(&FilterSP,NULL);

    GetFilterNames(FILTER_TAB);

    try
    {
        QSICam.get_MinExposureTime(&minDuration);
    } catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "get_MinExposureTime() failed. %s.", err.what());
        return false;
    }

    int nbuf;
    nbuf=PrimaryCCD.getXRes()*PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8;                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

int QSICCD::SetTemperature(double temperature)
{
    bool canSetTemp;
    try
    {
        QSICam.get_CanSetCCDTemperature(&canSetTemp);
    } catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "CanSetCCDTemperature() failed. %s.", err.what());
        return -1;
    }

    if(!canSetTemp)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Cannot set CCD temperature, CanSetCCDTemperature == false\n");
        return -1;
    }

    // If less than 0.1 of a degree, let's just return OK
    if (fabs(temperature - TemperatureN[0].value) < 0.1)
        return 1;

    activateCooler(true);

    try
    {
        QSICam.put_SetCCDTemperature(temperature);
    } catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "put_SetCCDTemperature() failed. %s.", err.what());
        return -1;
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "Setting CCD temperature to %+06.2f C", temperature);
    return 0;
}

bool QSICCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {

        /* Readout Speed */
        if (!strcmp(name, ReadOutSP.name))
        {
            if (IUUpdateSwitch(&ReadOutSP, states, names, n) < 0) return false;

            if (ReadOutS[0].s == ISS_ON)
            {
                try
                {
                 QSICam.put_ReadoutSpeed(QSICamera::HighImageQuality);
                } catch (std::runtime_error err)
                {
                        IUResetSwitch(&ReadOutSP);
                        ReadOutSP.s = IPS_ALERT;
                        DEBUGF(INDI::Logger::DBG_ERROR, "put_ReadoutSpeed() failed. %s.", err.what());
                        IDSetSwitch(&ReadOutSP, NULL);
                        return false;
                }

            }
            else
            {
                try
                {
                 QSICam.put_ReadoutSpeed(QSICamera::FastReadout);
                } catch (std::runtime_error err)
                {
                        IUResetSwitch(&ReadOutSP);
                        ReadOutSP.s = IPS_ALERT;
                        DEBUGF(INDI::Logger::DBG_ERROR, "put_ReadoutSpeed() failed. %s.", err.what());
                        IDSetSwitch(&ReadOutSP, NULL);
                        return false;
                }

                ReadOutSP.s = IPS_OK;
                IDSetSwitch(&ReadOutSP, NULL);

            }

            ReadOutSP.s = IPS_OK;
            IDSetSwitch(&ReadOutSP, NULL);
            return true;
        }


         /* Cooler */
        if (!strcmp (name, CoolerSP.name))
        {
          if (IUUpdateSwitch(&CoolerSP, states, names, n) < 0) return false;

          if (CoolerS[0].s == ISS_ON)
            activateCooler(true);
          else
              activateCooler(false);

          return true;
        }

        /* Shutter */
        if (!strcmp (name, ShutterSP.name))
        {
            if (IUUpdateSwitch(&ShutterSP, states, names, n) < 0) return false;
            shutterControl();
            return true;
        }

        /* Filter Wheel */
        if (!strcmp (name, FilterSP.name))
        {
            if (IUUpdateSwitch(&FilterSP, states, names, n) < 0) return false;
            turnWheel();
            return true;
        }
    }

        //  Nobody has claimed this, so, ignore it
        return INDI::CCD::ISNewSwitch(dev,name,states,names,n);
}

bool QSICCD::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    int maxFilters = (int) FilterSlotN[0].max;
    //std::string filterDesignation[maxFilters];

    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, FilterNameTP->name))
        {
            if (IUUpdateText(FilterNameTP, texts, names, n) < 0)
            {
                FilterNameTP->s = IPS_ALERT;
                IDSetText(FilterNameTP, "Error updating names. XML corrupted.");
                return false;
            }

            for (int i=0; i < maxFilters; i++)
                filterDesignation[i] = FilterNameT[i].text;

            if (SetFilterNames() == true)
            {
                FilterNameTP->s = IPS_OK;
                IDSetText(FilterNameTP, NULL);
                return true;
            }
            else
            {
                FilterNameTP->s = IPS_ALERT;
                IDSetText(FilterNameTP, "Error updating filter names.");
                return false;
            }

        }
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);

}

bool QSICCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INumber *np;

    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(FilterSlotNP.name, name))
        {

            targetFilter = values[0];

            np = IUFindNumber(&FilterSlotNP, names[0]);

            if (!np)
            {
                FilterSlotNP.s = IPS_ALERT;
                IDSetNumber(&FilterSlotNP, "Unknown error. %s is not a member of %s property.", names[0], name);
                return false;
            }

            int filter_count;
            try
            {
                QSICam.get_FilterCount(filter_count);
            } catch (std::runtime_error err)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "get_FilterCount() failed. %s.", err.what());
                IDSetNumber(&FilterSlotNP, NULL);
            }
            if (targetFilter < FIRST_FILTER || targetFilter > filter_count)
            {
                FilterSlotNP.s = IPS_ALERT;
                DEBUGF(INDI::Logger::DBG_ERROR, "Error: valid range of filter is from %d to %d", FIRST_FILTER, LAST_FILTER);
                IDSetNumber(&FilterSlotNP, NULL);
                return false;
            }

            IUUpdateNumber(&FilterSlotNP, values, names, n);

            FilterSlotNP.s = IPS_BUSY;
            DEBUGF(INDI::Logger::DBG_DEBUG, "Setting current filter to slot %d", targetFilter);
            IDSetNumber(&FilterSlotNP, NULL);


            SelectFilter(targetFilter);

            /* Check current filter position */
            short newFilter = QueryFilter();

            if (newFilter == targetFilter)
            {
                FilterSlotN[0].value = targetFilter;
                FilterSlotNP.s = IPS_OK;
                DEBUGF(INDI::Logger::DBG_DEBUG, "Filter set to slot #%d", targetFilter);
                IDSetNumber(&FilterSlotNP, NULL);
                return true;
            }
            else
                return false;
        }

    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

bool QSICCD::StartExposure(float duration)
{
        if(ExposureRequest < minDuration)
        {
            ExposureRequest = minDuration;
            DEBUGF(INDI::Logger::DBG_WARNING, "Exposure shorter than minimum ExposureRequest %g s requested. \n Setting exposure time to %g s.", minDuration,minDuration);
        }

        imageFrameType = PrimaryCCD.getFrameType();

        if(imageFrameType == CCDChip::BIAS_FRAME)
        {
            ExposureRequest = minDuration;
            PrimaryCCD.setExposureDuration(ExposureRequest);
            DEBUGF(INDI::Logger::DBG_SESSION, "Bias Frame (s) : %g\n", ExposureRequest);
        } else
        {
            ExposureRequest = duration;
            PrimaryCCD.setExposureDuration(ExposureRequest);
        }

            /* BIAS frame is the same as DARK but with minimum period. i.e. readout from camera electronics.*/
            if (imageFrameType == CCDChip::BIAS_FRAME)
            {
                try
                {
                    QSICam.put_PreExposureFlush(QSICamera::FlushNormal);
                    QSICam.StartExposure (ExposureRequest,false);
                } catch (std::runtime_error& err)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "StartExposure() failed. %s.", err.what());
                    return false;
                }
            }

            else if (imageFrameType == CCDChip::DARK_FRAME)
            {
                try
                {
                    QSICam.put_PreExposureFlush(QSICamera::FlushNormal);
                    QSICam.StartExposure (ExposureRequest,false);
                } catch (std::runtime_error& err)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "StartExposure() failed. %s.", err.what());
                    return false;
                }
            }

            else if (imageFrameType == CCDChip::LIGHT_FRAME || imageFrameType == CCDChip::FLAT_FRAME)
            {
                try
                {
                    QSICam.put_PreExposureFlush(QSICamera::FlushNormal);
                    QSICam.StartExposure (ExposureRequest,true);
                } catch (std::runtime_error& err)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "StartExposure() failed. %s.", err.what());
                    return false;
                }
            }

            gettimeofday(&ExpStart,NULL);
            DEBUGF(INDI::Logger::DBG_DEBUG, "Taking a %g seconds frame...", ExposureRequest);

            InExposure = true;
            return true;
}

bool QSICCD::AbortExposure()
{
    if (canAbort)
    {
        try
        {
          QSICam.AbortExposure();
        } catch (std::runtime_error err)
        {
          DEBUGF(INDI::Logger::DBG_ERROR, "AbortExposure() failed. %s.", err.what());
          return false;
        }


        InExposure = false;
        return true;
    }

    return false;
}

float QSICCD::CalcTimeLeft(timeval start,float req)
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec/1000);
    timesince=timesince/1000;
    timeleft=req-timesince;
    return timeleft;
}

bool QSICCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    char errmsg[ERRMSG_SIZE];

    /* Add the X and Y offsets */
    long x_1 = x / PrimaryCCD.getBinX();
    long y_1 = y / PrimaryCCD.getBinY();

    long x_2 = x_1 + (w / PrimaryCCD.getBinX());
    long y_2 = y_1 + (h / PrimaryCCD.getBinY());

    if (x_2 > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error: invalid width requested %ld", x_2);
        return false;
    }
    else if (y_2 > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error: invalid height request %ld", y_2);
        return false;
    }


    if (isDebug())
        IDLog("The Final image area is (%ld, %ld), (%ld, %ld)\n", x_1, y_1, x_2, y_2);

    imageWidth  = x_2 - x_1;
    imageHeight = y_2 - y_1;

    try
    {
        QSICam.put_StartX(x_1);
        QSICam.put_StartY(y_1);
        QSICam.put_NumX(imageWidth);
        QSICam.put_NumY(imageHeight);
    } catch (std::runtime_error err)
    {
        snprintf(errmsg, ERRMSG_SIZE, "Setting image area failed. %s.\n",err.what());
        DEBUGF(INDI::Logger::DBG_ERROR, "Setting image area failed. %s.", err.what());
        return false;
    }

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w,  h);
    int nbuf;
    nbuf=(imageWidth*imageHeight * PrimaryCCD.getBPP()/8);                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool QSICCD::UpdateCCDBin(int binx, int biny)
{
    try
    {
        QSICam.put_BinX(binx);
    } catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "put_BinX() failed. %s.", err.what());
        return false;
    }

    try
    {
        QSICam.put_BinY(biny);
    } catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "put_BinY() failed. %s.", err.what());
        return false;
    }

    PrimaryCCD.setBin(binx, biny);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int QSICCD::grabImage()
{
        unsigned short* image = (unsigned short *) PrimaryCCD.getFrameBuffer();

        int x,y,z;
        try {
            bool imageReady = false;
            QSICam.get_ImageReady(&imageReady);
            while(!imageReady)
            {
                usleep(500);
                QSICam.get_ImageReady(&imageReady);
            }

            QSICam.get_ImageArraySize(x,y,z);
            QSICam.get_ImageArray(image);
            imageBuffer = image;
            imageWidth  = x;
            imageHeight = y;
        } catch (std::runtime_error err)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "get_ImageArray() failed. %s.", err.what());
            return -1;
        }

        DEBUG(INDI::Logger::DBG_SESSION, "Download complete.");
	
        ExposureComplete(&PrimaryCCD);

	return 0;
}

void QSICCD::addFITSKeywords(fitsfile *fptr, CCDChip *targetChip)
{

    INDI::CCD::addFITSKeywords(fptr, targetChip);

    int status=0;
    double electronsPerADU;
    short filter = 0;
	char filter_s[32] = "None";

    try
    {
        QSICam.get_ElectronsPerADU(&electronsPerADU);
	    bool hasWheel = false;
	    QSICam.get_HasFilterWheel(&hasWheel);
        if(hasWheel)
        {
	      filter = QueryFilter();
	      string *filterNames = new std::string[LAST_FILTER+1];
	      QSICam.get_Names(filterNames);
          for(unsigned i = 0; i < 18; ++i)
              filter_s[i] = filterNames[filter-1][i];
	    }
    } catch (std::runtime_error& err)
    {
            DEBUGF(INDI::Logger::DBG_ERROR, "get_ElectronsPerADU failed. %s.", err.what());
            return;
    }

        fits_update_key_s(fptr, TDOUBLE, "CCD-TEMP", &(TemperatureN[0].value), "CCD Temperature (Celcius)", &status);
        fits_update_key_s(fptr, TDOUBLE, "EPERADU", &electronsPerADU, "Electrons per ADU", &status);
        fits_update_key_s(fptr, TSHORT,  "FILPOS", &filter, "Filter system position", &status);
        fits_update_key_s(fptr, TSTRING, "FILTER", filter_s, "Filter name", &status);

        fits_write_date(fptr, &status);
}

bool QSICCD::manageDefaults()
{
        /* X horizontal binning */
        try
        {
            QSICam.put_BinX(PrimaryCCD.getBinX());
        } catch (std::runtime_error err) {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error: put_BinX() failed. %s.", err.what());
            return false;
        }

        /* Y vertical binning */
        try {
            QSICam.put_BinY(PrimaryCCD.getBinY());
        } catch (std::runtime_error err)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error: put_BinY() failed. %s.", err.what());
            return false;
        }


        DEBUGF(INDI::Logger::DBG_DEBUG, "Setting default binning %d x %d.\n", PrimaryCCD.getBinX(), PrimaryCCD.getBinY());

        UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getXRes(), PrimaryCCD.getYRes());


        /* Success */
        return true;
}


bool QSICCD::Connect()
{
    bool connected;

    DEBUG(INDI::Logger::DBG_SESSION, "Attempting to find QSI CCD...");

            try
            {
                QSICam.get_Connected(&connected);
            } catch (std::runtime_error err)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "Error: get_Connected() failed. %s.", err.what());
                return false;
            }

            if(!connected)
            {
                try
                {
                    QSICam.put_Connected(true);
                } catch (std::runtime_error err)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "Error: put_Connected(true) failed. %s.", err.what());
                    return false;
                }
            }

            bool hasST4Port=false;
            try
            {
                QSICam.get_CanPulseGuide(&hasST4Port);
            }
            catch (std::runtime_error err)
            {
                  DEBUGF(INDI::Logger::DBG_ERROR, "get_canPulseGuide() failed. %s.", err.what());
                  return false;
            }

            try
            {
                QSICam.get_CanAbortExposure(&canAbort);
            } catch (std::runtime_error err)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "get_CanAbortExposure() failed. %s.", err.what());
                return false;
            }

            CCDCapability cap;

            cap.canAbort = canAbort;
            cap.canBin = true;
            cap.canSubFrame = true;
            cap.hasCooler = true;
            cap.hasGuideHead = false;
            cap.hasShutter = true;
            cap.hasST4Port = hasST4Port;

            SetCCDCapability(&cap);

            /* Success! */
            DEBUG(INDI::Logger::DBG_SESSION, "CCD is online. Retrieving basic data.");
            return true;
}


bool QSICCD::Disconnect()
{
    bool connected;


    try
    {
        QSICam.get_Connected(&connected);
    } catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error: get_Connected() failed. %s.", err.what());
        return false;
    }

    if (connected)
    {
        try
        {
            QSICam.put_Connected(false);
        } catch (std::runtime_error err)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error: put_Connected(false) failed. %s.", err.what());
            return false;
        }
    }

    DEBUG(INDI::Logger::DBG_SESSION, "CCD is offline.");
    return true;
}

void QSICCD::activateCooler(bool enable)
{

        bool coolerOn;

        if (enable)
        {
            try
            {
                QSICam.get_CoolerOn(&coolerOn);
            } catch (std::runtime_error err)
            {
                CoolerSP.s = IPS_IDLE;
                CoolerS[0].s = ISS_OFF;
                CoolerS[1].s = ISS_ON;
                DEBUGF(INDI::Logger::DBG_ERROR, "Error: CoolerOn() failed. %s.", err.what());
                IDSetSwitch(&CoolerSP, NULL);
                return;
            }

            if(!coolerOn)
            {
                try
                {
                    QSICam.put_CoolerOn(true);
                } catch (std::runtime_error err)
                {
                    CoolerSP.s = IPS_IDLE;
                    CoolerS[0].s = ISS_OFF;
                    CoolerS[1].s = ISS_ON;
                    DEBUGF(INDI::Logger::DBG_ERROR, "Error: put_CoolerOn(true) failed. %s.", err.what());
                    return;
                }
            }

            /* Success! */
            CoolerS[0].s = ISS_ON;
            CoolerS[1].s = ISS_OFF;
            CoolerSP.s = IPS_OK;
            DEBUG(INDI::Logger::DBG_SESSION, "Cooler ON");
            IDSetSwitch(&CoolerSP, NULL);
        }
        else
        {
              CoolerS[0].s = ISS_OFF;
              CoolerS[1].s = ISS_ON;
              CoolerSP.s = IPS_IDLE;

              try
              {
                 QSICam.get_CoolerOn(&coolerOn);
                 if(coolerOn) QSICam.put_CoolerOn(false);
              } catch (std::runtime_error err)
              {
                 DEBUGF(INDI::Logger::DBG_ERROR, "Error: CoolerOn() failed. %s.", err.what());
                 IDSetSwitch(&CoolerSP, NULL);
                 return;                                              
              }
              DEBUG(INDI::Logger::DBG_SESSION, "Cooler is OFF.");
              IDSetSwitch(&CoolerSP, NULL);
        }
}

void QSICCD::shutterControl()
{

        bool hasShutter;
        try{
            QSICam.get_HasShutter(&hasShutter);
        }catch (std::runtime_error err)
        {
            ShutterSP.s   = IPS_IDLE;
            ShutterS[0].s = ISS_OFF;
            ShutterS[1].s = ISS_OFF;
            DEBUGF(INDI::Logger::DBG_ERROR, "QSICamera::get_HasShutter() failed. %s.", err.what());
            return;
        }

        if(hasShutter){
            switch (ShutterS[0].s)
            {
              case ISS_ON:

                try
            {
                    QSICam.put_ManualShutterMode(true);
                    QSICam.put_ManualShutterOpen(true);
                }catch (std::runtime_error err)
                {
                    ShutterSP.s = IPS_IDLE;
                    ShutterS[0].s = ISS_OFF;
                    ShutterS[1].s = ISS_ON;
                    DEBUGF(INDI::Logger::DBG_ERROR, "Error: ManualShutterOpen() failed. %s.", err.what());
                    IDSetSwitch(&ShutterSP, NULL);
                    return;
                }

                /* Success! */
                ShutterS[0].s = ISS_ON;
                ShutterS[1].s = ISS_OFF;
                ShutterSP.s = IPS_OK;
                DEBUG(INDI::Logger::DBG_SESSION, "Shutter opened manually.");
                IDSetSwitch(&ShutterSP, NULL);
                break;

              case ISS_OFF:

                try{
                    QSICam.put_ManualShutterOpen(false);
                    QSICam.put_ManualShutterMode(false);
                }catch (std::runtime_error err)
                {
                    ShutterSP.s = IPS_IDLE;
                    ShutterS[0].s = ISS_ON;
                    ShutterS[1].s = ISS_OFF;
                    DEBUGF(INDI::Logger::DBG_ERROR, "Error: ManualShutterOpen() failed. %s.", err.what());
                    IDSetSwitch(&ShutterSP, NULL);
                    return;
                }

                /* Success! */
                ShutterS[0].s = ISS_OFF;
                ShutterS[1].s = ISS_ON;
                ShutterSP.s = IPS_IDLE;
                DEBUG(INDI::Logger::DBG_SESSION, "Shutter closed manually.");
                IDSetSwitch(&ShutterSP, NULL);
                break;
            }
        }
}


void QSICCD::TimerHit()
{
    long err;
    long timeleft;
    double ccdTemp;
    double coolerPower;

    if(isConnected() == false)
        return;  //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        bool imageReady;

        timeleft=CalcTimeLeft(ExpStart, ExposureRequest);

        if (timeleft < 1)
        {
            QSICam.get_ImageReady(&imageReady);

            while (!imageReady)
            {
                usleep(100);
                QSICam.get_ImageReady(&imageReady);

            }

            /* We're done exposing */
            DEBUG(INDI::Logger::DBG_SESSION, "Exposure done, downloading image...");
            PrimaryCCD.setExposureLeft(0);
            InExposure = false;
            /* grab and save image */
            grabImage();

        }
        else
        {
            DEBUGF(INDI::Logger::DBG_DEBUG, "Image not ready, time left %ld\n", timeleft);
            PrimaryCCD.setExposureLeft(timeleft);

        }

    }

    switch (TemperatureNP.s)
    {
      case IPS_IDLE:
      case IPS_OK:
        try
       {
            QSICam.get_CCDTemperature(&ccdTemp);
        } catch (std::runtime_error err)
        {
            TemperatureNP.s = IPS_IDLE;
            DEBUGF(INDI::Logger::DBG_ERROR, "get_CCDTemperature() failed. %s.", err.what());
            IDSetNumber(&TemperatureNP, NULL);
            return;
        }

        if (fabs(TemperatureN[0].value - ccdTemp) >= TEMP_THRESHOLD)
        {
           TemperatureN[0].value = ccdTemp;
           IDSetNumber(&TemperatureNP, NULL);
        }
        break;

      case IPS_BUSY:
        try
        {
            QSICam.get_CCDTemperature(&ccdTemp);
        } catch (std::runtime_error err)
        {
            TemperatureNP.s = IPS_ALERT;
            DEBUGF(INDI::Logger::DBG_ERROR, "get_CCDTemperature() failed. %s.", err.what());
            IDSetNumber(&TemperatureNP, NULL);
            return;
        }

        if (fabs(TemperatureN[0].value - ccdTemp) <= TEMP_THRESHOLD)
        {
            TemperatureNP.s = IPS_OK;
            IDSetNumber(&TemperatureNP, NULL);
        }

        TemperatureN[0].value = ccdTemp;
        IDSetNumber(&TemperatureNP, NULL);
        break;

      case IPS_ALERT:
        break;
    }

    switch (CoolerNP.s)
    {
      case IPS_IDLE:
      case IPS_OK:
        try
        {
            QSICam.get_CoolerPower(&coolerPower);
        } catch (std::runtime_error err)
        {
            CoolerNP.s = IPS_IDLE;
            DEBUGF(INDI::Logger::DBG_ERROR, "get_CoolerPower() failed. %s.", err.what());
            IDSetNumber(&CoolerNP, NULL);
            return;
        }

        if (CoolerN[0].value != coolerPower)
        {
            CoolerN[0].value = coolerPower;
            IDSetNumber(&CoolerNP, NULL);
        }

        break;

      case IPS_BUSY:
        try
       {
            QSICam.get_CoolerPower(&coolerPower);
        } catch (std::runtime_error err)
        {
            CoolerNP.s = IPS_ALERT;
            DEBUGF(INDI::Logger::DBG_ERROR, "get_CoolerPower() failed. %s.", err.what());
            IDSetNumber(&CoolerNP, NULL);
            return;
        }
        CoolerNP.s = IPS_OK;

        CoolerN[0].value = coolerPower;
        IDSetNumber(&CoolerNP, NULL);
        break;

      case IPS_ALERT:
         break;
    }

    SetTimer(POLLMS);
    return;
}

void QSICCD::turnWheel()
{
        short current_filter;

        switch (FilterS[0].s)
        {
          case ISS_ON:
            if (current_filter < LAST_FILTER)
                current_filter++;
            else current_filter = FIRST_FILTER;
            try
            {
                current_filter = QueryFilter();
                if(current_filter < LAST_FILTER)
                    current_filter++;
                else current_filter = FIRST_FILTER;

                SelectFilter(current_filter);
            } catch (std::runtime_error err)
            {
                FilterSP.s = IPS_IDLE;
                FilterS[0].s = ISS_OFF;
                FilterS[1].s = ISS_OFF;
                DEBUGF(INDI::Logger::DBG_ERROR, "QSICamera::get_FilterPos() failed. %s.", err.what());
                return;
            }

            FilterSlotN[0].value = current_filter;
            FilterS[0].s = ISS_OFF;
            FilterS[1].s = ISS_OFF;
            FilterSP.s = IPS_OK;
            DEBUGF(INDI::Logger::DBG_DEBUG, "The current filter is %d", current_filter);
            IDSetSwitch(&FilterSP, NULL);
            break;

          case ISS_OFF:
           try
           {
                current_filter = QueryFilter();
                if(current_filter > FIRST_FILTER)
                    current_filter--;
                else
                    current_filter = LAST_FILTER;
              SelectFilter(current_filter);
            } catch (std::runtime_error err)
            {
                FilterSP.s = IPS_IDLE;
                FilterS[0].s = ISS_OFF;
                FilterS[1].s = ISS_OFF;
                DEBUGF(INDI::Logger::DBG_ERROR, "QSICamera::get_FilterPos() failed. %s.", err.what());
                return;
            }

            FilterSlotN[0].value = current_filter;
            FilterS[0].s = ISS_OFF;
            FilterS[1].s = ISS_OFF;
            FilterSP.s = IPS_OK;
            DEBUGF(INDI::Logger::DBG_DEBUG, "The current filter is %d\n", current_filter);
            IDSetSwitch(&FilterSP, NULL);
            IDSetNumber(&FilterSlotNP, NULL);
            break;
        }
}

bool QSICCD::GuideNorth(float duration)
{
    try
    {
        QSICam.PulseGuide(QSICamera::guideNorth, duration);
    }
    catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "PulseGuide() failed. %s.", err.what());
       return false;
    }

    return true;
}

bool QSICCD::GuideSouth(float duration)
{
    try
    {
        QSICam.PulseGuide(QSICamera::guideSouth, duration);
    }
    catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "PulseGuide() failed. %s.", err.what());
       return false;
    }

    return true;

}

bool QSICCD::GuideEast(float duration)
{
    try
    {
        QSICam.PulseGuide(QSICamera::guideEast, duration);
    }
    catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "PulseGuide() failed. %s.", err.what());
       return false;
    }

    return true;

}

bool QSICCD::GuideWest(float duration)
{
    try
    {
        QSICam.PulseGuide(QSICamera::guideWest, duration);
    }
    catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "PulseGuide() failed. %s.", err.what());
       return false;
    }

    return true;
}


bool QSICCD::GetFilterNames(const char* groupName)
{
    char filterName[MAXINDINAME];
    char filterLabel[MAXINDILABEL];
    int maxFilters = (int) FilterSlotN[0].max;

    if (FilterNameT != NULL)
        delete FilterNameT;

    try
    {
     QSICam.get_Names(filterDesignation);
    }
    catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "QSICamera::get_Names() failed. %s.", err.what());
        return false;
    }

    FilterNameT = new IText[maxFilters];

    for (int i=0; i < maxFilters; i++)
    {
        snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i+1);
        snprintf(filterLabel, MAXINDILABEL, "Filter #%d", i+1);
        IUFillText(&FilterNameT[i], filterName, filterLabel, filterDesignation[i].c_str());
    }

    IUFillTextVector(FilterNameTP, FilterNameT, maxFilters, getDeviceName(), "FILTER_NAME", "Filter", groupName, IP_RW,1, IPS_IDLE);

    return true;
}

bool QSICCD::SetFilterNames()
{
    try
    {
        QSICam.put_Names(filterDesignation);
    }
    catch (std::runtime_error err)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "put_Names() failed. %s.", err.what());
        IDSetText(FilterNameTP, NULL);
        return false;
    }

    return true;
}

bool QSICCD::SelectFilter(int targetFilter)
{
    short filter = targetFilter - 1;
    try
    {
        QSICam.put_Position(filter);
    } catch(std::runtime_error err)
    {
        FilterSlotNP.s = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_ERROR, "put_Position() failed. %s.", err.what());
        IDSetNumber(&FilterSlotNP, NULL);
        return false;
    }

    return true;
}

int QSICCD::QueryFilter()
{
    short newFilter;
    try
    {
        QSICam.get_Position(&newFilter);
    } catch(std::runtime_error err)
    {
        FilterSlotNP.s = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_ERROR, "get_Position() failed. %s.", err.what());
        IDSetNumber(&FilterSlotNP, NULL);
        return -1;
    }

    return newFilter + 1;
}

