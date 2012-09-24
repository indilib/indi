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

    IUFillSwitch(&ResetS[0], "RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "FRAME_RESET", "Frame Values", IMAGE_SETTINGS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&CoolerS[0], "CONNECT_COOLER", "ON", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "DISCONNECT_COOLER", "OFF", ISS_OFF);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "COOLER_CONNECTION", "Cooler", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&ShutterS[0], "SHUTTER_ON", "Manual open", ISS_OFF);
    IUFillSwitch(&ShutterS[1], "SHUTTER_OFF", "Manual close", ISS_OFF);
    IUFillSwitchVector(&ShutterSP, ShutterS, 2, getDeviceName(), "SHUTTER_CONNECTION", "Shutter", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER", "Cooling Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&TemperatureRequestN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", MIN_CCD_TEMP, MAX_CCD_TEMP, 0., 0.);
    IUFillNumberVector(&TemperatureRequestNP, TemperatureN, 1, getDeviceName(), "CCD_TEMPERATURE_REQUEST", "Temperature", MAIN_CONTROL_TAB, IP_WO, 60, IPS_IDLE);

    IUFillNumber(&TemperatureN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", MIN_CCD_TEMP, MAX_CCD_TEMP, 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "CCD_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&ReadOutS[0], "QUALITY_HIGH", "High Quality", ISS_OFF);
    IUFillSwitch(&ReadOutS[1], "QUALITY_LOW", "Fast", ISS_OFF);
    IUFillSwitchVector(&ReadOutSP, ReadOutS, 2, getDeviceName(), "READOUT_QUALITY", "Readout Speed", OPTIONS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    //IUFillNumber(&FilterN[0], "FILTER_SLOT_VALUE", "Active Filter", "%2.0f", FIRST_FILTER, LAST_FILTER, 1, 0);
    //IUFillNumberVector(&FilterSlotNP, FilterN, 1, getDeviceName(), "FILTER_SLOT", "Filter", FILTER_WHEEL_TAB, IP_RW, 60, IPS_IDLE);

    IUFillSwitch(&FilterS[0], "FILTER_CW", "+", ISS_OFF);
    IUFillSwitch(&FilterS[1], "FILTER_CCW", "-", ISS_OFF);
    IUFillSwitchVector(&FilterSP, FilterS, 2, getDeviceName(), "FILTER_WHEEL_MOTION", "Turn Wheel", FILTER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    //initGuiderProperties(getDeviceName(), GUIDER_TAB);
    initFilterProperties(getDeviceName(), FILTER_TAB);


    addDebugControl();
}

bool QSICCD::updateProperties()
{

    INDI::CCD::updateProperties();

    if (isConnected())
    {
        defineNumber(&TemperatureRequestNP);
        defineNumber(&TemperatureNP);
        defineSwitch(&ResetSP);
        defineSwitch(&CoolerSP);
        defineSwitch(&ShutterSP);
        defineNumber(&CoolerNP);
        defineNumber(&FilterSlotNP);
        defineSwitch(&FilterSP);
        defineSwitch(&ReadOutSP);

        setupParams();

        if (FilterNameT != NULL)
            defineText(FilterNameTP);

        char msg[1024];

        if (manageDefaults(msg))
        {
            IDMessage(getDeviceName(), msg, NULL);
            if (isDebug())
                IDLog("%s\n", msg);
            return false;
        }

        timerID = SetTimer(POLLMS);
    }
    else
    {
        deleteProperty(TemperatureRequestNP.name);
        deleteProperty(TemperatureNP.name);
        deleteProperty(ResetSP.name);
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

    if (isDebug())
        IDLog("In setupParams\n");

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
        IDMessage(getDeviceName(), "Setup Params failed. %s.", err.what());
        if (isDebug())
            IDLog("Setup Params failed. %s.", err.what());
        return false;
    }


    IDMessage(getDeviceName(), "The CCD Temperature is %f.\n", temperature);

    if (isDebug())
        IDLog("The CCD Temperature is %f.\n", temperature);

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
        IDMessage(getDeviceName(), "get_Name() failed. %s.", err.what());
        if (isDebug())
            IDLog("get_Name() failed. %s.\n", err.what());
        return false;
    }
    IDMessage(getDeviceName(), "%s", name.c_str());

    if (isDebug())
        IDLog("%s\n", name.c_str());

    int filter_count;
    try
    {
            QSICam.get_FilterCount(filter_count);
    } catch (std::runtime_error err)
    {
            IDMessage(getDeviceName(), "get_FilterCount() failed. %s.", err.what());
            IDLog("get_FilterCount() failed. %s.\n", err.what());
            return false;
    }

    IDMessage(getDeviceName(),"The filter count is %d\n", filter_count);

    if (isDebug())
        IDLog("The filter count is %d\n", filter_count);

    FilterSlotN[0].max = filter_count;
    FilterSlotNP.s = IPS_OK;

    IUUpdateMinMax(&FilterSlotNP);
    IDSetNumber(&FilterSlotNP, "Setting max number of filters.\n");

    FilterSP.s = IPS_OK;
    IDSetSwitch(&FilterSP,NULL);

    try
    {
        QSICam.get_CanPulseGuide(&HasSt4Port);
    }
    catch (std::runtime_error err)
    {
          IDMessage(getDeviceName(), "get_canPulseGuide() failed. %s.", err.what());
          if (isDebug())
            IDLog("get_canPulseGuide() failed. %s.\n", err.what());
          return false;
    }

    GetFilterNames(FILTER_TAB);

    try
    {
        QSICam.get_MinExposureTime(&minDuration);
    } catch (std::runtime_error err)
    {
        IDMessage(getDeviceName(), "get_MinExposureTime() failed. %s.", err.what());
        if (isDebug())
            IDLog("get_MinExposureTime() failed. %s.", err.what());
        return false;
    }

    int nbuf;
    nbuf=PrimaryCCD.getXRes()*PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8;                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
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
                        IDSetSwitch(&ReadOutSP, "put_ReadoutSpeed() failed. %s.", err.what());
                        if (isDebug())
                            IDLog("put_ReadoutSpeed() failed. %s.\n", err.what());
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
                        IDSetSwitch(&ReadOutSP, "put_ReadoutSpeed() failed. %s.", err.what());
                        if (isDebug())
                            IDLog("put_ReadoutSpeed() failed. %s.\n", err.what());
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
          activateCooler();
          return true;
        }

        /* Reset */
        if (!strcmp (name, ResetSP.name))
        {
          if (IUUpdateSwitch(&ResetSP, states, names, n) < 0) return false;
          resetFrame();
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

        /* Temperature*/
        if (!strcmp(TemperatureRequestNP.name, name))
        {
            TemperatureRequestNP.s = IPS_IDLE;

            np = IUFindNumber(&TemperatureRequestNP, names[0]);

            if (!np)
            {
                IDSetNumber(&TemperatureRequestNP, "Unknown error. %s is not a member of %s property.", names[0], name);
                return false;
            }

            if (values[0] < MIN_CCD_TEMP || values[0] > MAX_CCD_TEMP)
            {
                IDSetNumber(&TemperatureRequestNP, "Error: valid range of temperature is from %d to %d", MIN_CCD_TEMP, MAX_CCD_TEMP);
                return false;
            }

            bool canSetTemp;
            try
            {
                QSICam.get_CanSetCCDTemperature(&canSetTemp);
            } catch (std::runtime_error err)
            {
                IDSetNumber(&TemperatureRequestNP, "CanSetCCDTemperature() failed. %s.", err.what());
                if (isDebug())
                    IDLog("CanSetCCDTemperature() failed. %s.", err.what());
                return false;
            }
            if(!canSetTemp)
            {
                IDMessage(getDeviceName(), "Cannot set CCD temperature, CanSetCCDTemperature == false\n");
                return false;
            }

            try
            {
                QSICam.put_SetCCDTemperature(values[0]);
            } catch (std::runtime_error err)
            {
                IDSetNumber(&TemperatureRequestNP, "put_SetCCDTemperature() failed. %s.", err.what());
                if (isDebug())
                    IDLog("put_SetCCDTemperature() failed. %s.", err.what());
                return false;
            }

            TemperatureRequestNP.s = IPS_BUSY;
            TemperatureNP.s = IPS_BUSY;

            IDSetNumber(&TemperatureRequestNP, "Setting CCD temperature to %+06.2f C", values[0]);
            if (isDebug())
                IDLog("Setting CCD temperature to %+06.2f C\n", values[0]);
            return true;
        }

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
                IDSetNumber(&FilterSlotNP, "get_FilterCount() failed. %s.", err.what());
            }
            if (targetFilter < FIRST_FILTER || targetFilter > filter_count)
            {
                FilterSlotNP.s = IPS_ALERT;
                IDSetNumber(&FilterSlotNP, "Error: valid range of filter is from %d to %d", FIRST_FILTER, LAST_FILTER);
                return false;
            }

            IUUpdateNumber(&FilterSlotNP, values, names, n);

            FilterSlotNP.s = IPS_BUSY;
            IDSetNumber(&FilterSlotNP, "Setting current filter to slot %d", targetFilter);
            if (isDebug())
                IDLog("Setting current filter to slot %d\n", targetFilter);

            SelectFilter(targetFilter);

            /* Check current filter position */
            short newFilter = QueryFilter();
	    /*
            try
            {
	        newFilter = QueryFilter();
	        //QSICam.get_Position(&newFilter);
            } catch(std::runtime_error err)
            {
                FilterSlotNP.s = IPS_ALERT;
                IDSetNumber(&FilterSlotNP, "get_Position() failed. %s.", err.what());
                if (isDebug())
                    IDLog("get_Position() failed. %s.\n", err.what());
                return false;
            }
	    */

            if (newFilter == targetFilter)
            {
                FilterSlotN[0].value = targetFilter;
                FilterSlotNP.s = IPS_OK;
                IDSetNumber(&FilterSlotNP, "Filter set to slot #%d", targetFilter);
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

int QSICCD::StartExposure(float duration)
{

        bool shortExposure = false;

        if(duration < minDuration)
        {
            duration = minDuration;
            IDMessage(getDeviceName(), "Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.", minDuration,minDuration);
        }

        imageFrameType = PrimaryCCD.getFrameType();

        if(imageFrameType == CCDChip::BIAS_FRAME)
        {
            PrimaryCCD.setExposure(minDuration);
            IDMessage(getDeviceName(), "Bias Frame (s) : %g\n", minDuration);
            if (isDebug())
                IDLog("Bias Frame (s) : %g\n", minDuration);
        } else
        {
            PrimaryCCD.setExposure(duration);
            if (isDebug())
                IDLog("Exposure Time (s) is: %g\n", duration);
        }

         imageExpose = PrimaryCCD.getExposure();

            /* BIAS frame is the same as DARK but with minimum period. i.e. readout from camera electronics.*/
            if (imageFrameType == CCDChip::BIAS_FRAME)
            {
                try
                {
                    QSICam.put_PreExposureFlush(QSICamera::FlushNormal);
                    QSICam.StartExposure (minDuration,false);
                    shortExposure = true;
                } catch (std::runtime_error& err)
                {
                    //ImageExposureNP.s = IPS_ALERT;
                    IDMessage(getDeviceName(), "StartExposure() failed. %s.", err.what());
                    if (isDebug())
                        IDLog("StartExposure() failed. %s.\n", err.what());
                    return -1;
                }
            }

            else if (imageFrameType == CCDChip::DARK_FRAME)
            {
                try
                {
                    QSICam.put_PreExposureFlush(QSICamera::FlushNormal);
                    QSICam.StartExposure (imageExpose,false);
                } catch (std::runtime_error& err)
                {
                    //ImageExposureNP.s = IPS_ALERT;
                    IDMessage(getDeviceName(), "StartExposure() failed. %s.", err.what());
                    if (isDebug())
                        IDLog("StartExposure() failed. %s.\n", err.what());
                    return -1;
                }
            }

            else if (imageFrameType == CCDChip::LIGHT_FRAME || imageFrameType == CCDChip::FLAT_FRAME)
            {
                try
                {
                    QSICam.put_PreExposureFlush(QSICamera::FlushNormal);
                    QSICam.StartExposure (imageExpose,true);
                } catch (std::runtime_error& err)
                {
                    //ImageExposureNP.s = IPS_ALERT;
                    IDMessage(getDeviceName(), "StartExposure() failed. %s.", err.what());
                    if (isDebug())
                        IDLog("StartExposure() failed. %s.\n", err.what());
                    return -1;
                }
            }

            //ImageExposureNP.s = IPS_BUSY;
            gettimeofday(&ExpStart,NULL);
            IDMessage(getDeviceName(), "Taking a %g seconds frame...", imageExpose);

            if (isDebug())
                IDLog("Taking a frame...\n");

            InExposure = true;
            return (shortExposure ? 1 : 0);
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
          //ImageExposureNP.s = IPS_ALERT;
          IDMessage(getDeviceName(), "AbortExposure() failed. %s.", err.what());
          IDLog("AbortExposure() failed. %s.\n", err.what());
          return false;
        }

        //ImageExposureNP.s = IPS_IDLE;
        //IDSetNumber(&ImageExposureNP, NULL);

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

bool QSICCD::updateCCDFrame(int x, int y, int w, int h)
{
    char errmsg[ERRMSG_SIZE];

    /* Add the X and Y offsets */
    long x_1 = x;
    long y_1 = y;

    long x_2 = x_1 + (w / PrimaryCCD.getBinX());
    long y_2 = y_1 + (h / PrimaryCCD.getBinY());

    long sensorPixelSize_x,
         sensorPixelSize_y;
    try
    {
        QSICam.get_CameraXSize(&sensorPixelSize_x);
        QSICam.get_CameraYSize(&sensorPixelSize_y);
    } catch (std::runtime_error err)
    {
        IDMessage(getDeviceName(), "Getting image area size failed. %s.", err.what());
        return false;
    }

    if (x_2 > sensorPixelSize_x / PrimaryCCD.getBinX())
        x_2 = sensorPixelSize_x / PrimaryCCD.getBinX();

    if (y_2 > sensorPixelSize_y / PrimaryCCD.getBinY())
        y_2 = sensorPixelSize_y / PrimaryCCD.getBinY();

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
        IDMessage(getDeviceName(), "Setting image area failed. %s.", err.what());
        if (isDebug())
            IDLog("Setting image area failed. %s.", err.what());
        return false;
    }

    PrimaryCCD.setFrame(x_1, y_1, x_2, y_2);

    int nbuf;
    nbuf=(imageWidth*imageHeight * PrimaryCCD.getBPP()/8);                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool QSICCD::updateCCDBin(int binx, int biny)
{
    try
    {
        QSICam.put_BinX(binx);
    } catch (std::runtime_error err)
    {
        IDMessage(getDeviceName(), "put_BinX() failed. %s.", err.what());
        IDLog("put_BinX() failed. %s.", err.what());
        return false;
    }

    try
    {
        QSICam.put_BinY(biny);
    } catch (std::runtime_error err)
    {
        IDMessage(getDeviceName(), "put_BinY() failed. %s.", err.what());
        IDLog("put_BinY() failed. %s.", err.what());
        return false;
    }

    PrimaryCCD.setBin(binx, biny);

    return updateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int QSICCD::grabImage()
{
        unsigned short* image = (unsigned short *) PrimaryCCD.getFrameBuffer();

        int x,y,z;
        try {
            int counter=1;
            bool imageReady = false;
            QSICam.get_ImageReady(&imageReady);
            while(!imageReady)
            {
                usleep(500);
                IDLog("Sleeping 500, counter %d\n", counter++);
                QSICam.get_ImageReady(&imageReady);
            }

            QSICam.get_ImageArraySize(x,y,z);
            IDLog("Before grab array\n");
            QSICam.get_ImageArray(image);
            IDLog("After grab array\n");
            imageBuffer = image;
            imageWidth  = x;
            imageHeight = y;
        } catch (std::runtime_error err)
        {
            IDMessage(getDeviceName(), "get_ImageArray() failed. %s.", err.what());
            IDLog("get_ImageArray() failed. %s.", err.what());
            return -1;
        }

        IDMessage(getDeviceName(), "Download complete.\n");
	
        ExposureComplete();

	return 0;
}

void QSICCD::addFITSKeywords(fitsfile *fptr)
{

        int status=0; 
        char binning_s[32];
        char frame_s[32];
        double min_val = min();
        double max_val = max();

        snprintf(binning_s, 32, "(%d x %d)", PrimaryCCD.getBinX(), PrimaryCCD.getBinY());

        switch (imageFrameType)
        {
          case CCDChip::LIGHT_FRAME:
      	    strcpy(frame_s, "Light");
	    break;
          case CCDChip::BIAS_FRAME:
            strcpy(frame_s, "Bias");
	    break;
          case CCDChip::FLAT_FRAME:
            strcpy(frame_s, "Flat Field");
	    break;
          case CCDChip::DARK_FRAME:
            strcpy(frame_s, "Dark");
	    break;
        }

        char name_s[32] = "QSI";
        double electronsPerADU;
	double pixSize1,pixSize2;
        short filter = 0;
	char filter_s[32] = "None";
	char exposureStartTime_s[32] = "";
        try {
            string name;
            QSICam.get_Name(name);
            for(unsigned i = 0; i < 18; ++i) name_s[i] = name[i];
            QSICam.get_ElectronsPerADU(&electronsPerADU);
	    bool hasWheel = false;
	    QSICam.get_HasFilterWheel(&hasWheel);
	    if(hasWheel){
	      filter = QueryFilter();
	      //QSICam.get_Position(&filter);
	      string *filterNames = new std::string[LAST_FILTER+1];
	      QSICam.get_Names(filterNames);
	      for(unsigned i = 0; i < 18; ++i) filter_s[i] = filterNames[filter-1][i];
	    }
	    QSICam.get_PixelSizeX(&pixSize1);
	    QSICam.get_PixelSizeY(&pixSize2);
	    string exposureStartTime;
	    QSICam.get_LastExposureStartTime(exposureStartTime);
	    for(unsigned i = 0; i < 19; ++i) exposureStartTime_s[i] = exposureStartTime[i];
        } catch (std::runtime_error& err) {
            IDMessage(getDeviceName(), "get_Name() failed. %s.", err.what());
            IDLog("get_Name() failed. %s.\n", err.what());
            return;
        }

        //pixSize = PrimaryCCD.getPixelSizeX();
	
        fits_update_key_s(fptr, TDOUBLE, "CCD-TEMP", &(TemperatureN[0].value), "CCD Temperature (Celcius)", &status);
        fits_update_key_s(fptr, TDOUBLE, "EXPTIME", &(imageExpose), "Total Exposure Time (s)", &status);
        if(imageFrameType == CCDChip::DARK_FRAME)
        fits_update_key_s(fptr, TDOUBLE, "DARKTIME", &(imageExpose), "Total Exposure Time (s)", &status);
        fits_update_key_s(fptr, TDOUBLE, "PIXSIZE1", &(pixSize1), "Pixel Size 1 (microns)", &status);
	fits_update_key_s(fptr, TDOUBLE, "PIXSIZE2", &(pixSize2), "Pixel Size 2 (microns)", &status);
        fits_update_key_s(fptr, TSTRING, "BINNING", binning_s, "Binning HOR x VER", &status);
        fits_update_key_s(fptr, TSTRING, "FRAME", frame_s, "Frame Type", &status);
        fits_update_key_s(fptr, TDOUBLE, "DATAMIN", &min_val, "Minimum value", &status);
        fits_update_key_s(fptr, TDOUBLE, "DATAMAX", &max_val, "Maximum value", &status);
        fits_update_key_s(fptr, TSTRING, "INSTRUME", name_s, "CCD Name", &status);
        fits_update_key_s(fptr, TDOUBLE, "EPERADU", &electronsPerADU, "Electrons per ADU", &status);
	fits_update_key_s(fptr, TSHORT,  "FILPOS", &filter, "Filter system position", &status);
	fits_update_key_s(fptr, TSTRING, "FILTER", filter_s, "Filter name", &status);
	fits_update_key_s(fptr, TSTRING, "DATE-OBS", exposureStartTime_s, "UTC start date of observation", &status);

        fits_write_date(fptr, &status);
}

void QSICCD::fits_update_key_s(fitsfile* fptr, int type, string name, void* p, string explanation, int* status)
{
        // this function is for removing warnings about deprecated string conversion to char* (from arg 5)
        fits_update_key(fptr,type,name.c_str(),p, const_cast<char*>(explanation.c_str()), status);
}


int QSICCD::manageDefaults(char errmsg[])
{

        long err;
  
        /* X horizontal binning */
        try
        {
            QSICam.put_BinX(PrimaryCCD.getBinX());
        } catch (std::runtime_error err) {
            IDMessage(getDeviceName(), "Error: put_BinX() failed. %s.", err.what());
            IDLog("Error: put_BinX() failed. %s.\n", err.what());
            return -1;
        }

        /* Y vertical binning */
        try {
            QSICam.put_BinY(PrimaryCCD.getBinY());
        } catch (std::runtime_error err)
        {
            IDMessage(getDeviceName(), "Error: put_BinY() failed. %s.", err.what());
            IDLog("Error: put_BinX() failed. %s.\n", err.what());
            return -1;
        }

        if (isDebug())
            IDLog("Setting default binning %d x %d.\n", PrimaryCCD.getBinX(), PrimaryCCD.getBinY());

        updateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getXRes(), PrimaryCCD.getYRes());



        /* Success */
        return 0;
}


bool QSICCD::Connect()
{
    bool connected;
    char msg[1024];

    IDMessage(getDeviceName(), "Attempting to find the QSI CCD...");

    if (isDebug())
    {
        IDLog ("Connecting CCD\n");
        IDLog("Attempting to find the camera\n");
    }


            try
            {
                QSICam.get_Connected(&connected);
            } catch (std::runtime_error err)
            {
                IDMessage(getDeviceName(), "Error: get_Connected() failed. %s.", err.what());
                if (isDebug())
                    IDLog("Error: get_Connected() failed. %s.", err.what());
                return false;
            }

            if(!connected)
            {
                try
                {
                    QSICam.put_Connected(true);
                } catch (std::runtime_error err)
                {
                    IDMessage(getDeviceName(), "Error: put_Connected(true) failed. %s.", err.what());
                    if (isDebug())
                        IDLog("Error: put_Connected(true) failed. %s.", err.what());
                    return false;
                }
            }

            /* Success! */
            IDMessage(getDeviceName(), "CCD is online. Retrieving basic data.");
            if (isDebug())
                IDLog("CCD is online. Retrieving basic data.\n");


            return true;
}


bool QSICCD::Disconnect()
{
    char msg[MAXRBUF];
    bool connected;


    try
    {
        QSICam.get_Connected(&connected);
    } catch (std::runtime_error err)
    {
        IDMessage(getDeviceName(), "Error: get_Connected() failed. %s.", err.what());
        snprintf(msg, MAXRBUF, "Error: get_Connected() failed. %s.", err.what());

        if (isDebug())
            IDLog("%s\n", msg);
        return false;
    }

    if (connected)
    {
        try
        {
            QSICam.put_Connected(false);
        } catch (std::runtime_error err)
        {
            IDMessage(getDeviceName(), "Error: put_Connected(false) failed. %s.", err.what());
            snprintf(msg, MAXRBUF, "Error: put_Connected(false) failed. %s.\n", err.what());
            if (isDebug())
                IDLog("%s\n", msg);
            return false;
        }
    }

    IDMessage(getDeviceName(), "CCD is offline.");
    return true;
}

void QSICCD::activateCooler()
{

        bool coolerOn;

        switch (CoolerS[0].s)
        {
          case ISS_ON:
            try
            {
                QSICam.get_CoolerOn(&coolerOn);
            } catch (std::runtime_error err)
            {
                CoolerSP.s = IPS_IDLE;
                CoolerS[0].s = ISS_OFF;
                CoolerS[1].s = ISS_ON;
                IDSetSwitch(&CoolerSP, "Error: CoolerOn() failed. %s.", err.what());
                IDLog("Error: CoolerOn() failed. %s.\n", err.what());
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
                    IDSetSwitch(&CoolerSP, "Error: put_CoolerOn(true) failed. %s.", err.what());
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

              try
              {
                 QSICam.get_CoolerOn(&coolerOn);
                 if(coolerOn) QSICam.put_CoolerOn(false);
              } catch (std::runtime_error err)
              {
                 IDSetSwitch(&CoolerSP, "Error: CoolerOn() failed. %s.", err.what());
                 IDLog("Error: CoolerOn() failed. %s.\n", err.what());
                 return;                                              
              }
              IDSetSwitch(&CoolerSP, "Cooler is OFF.");
              break;
        }
}

double QSICCD::min()
{

        double lmin = imageBuffer[0];
        int ind=0, i, j;
  
        for (i= 0; i < imageHeight ; i++)
            for (j= 0; j < imageWidth; j++) {
                ind = (i * imageWidth) + j;
                if (imageBuffer[ind] < lmin) lmin = imageBuffer[ind];
        }
    
        return lmin;
}

double QSICCD::max()
{

        double lmax = imageBuffer[0];
        int ind=0, i, j;
  
        for (i= 0; i < imageHeight ; i++)
            for (j= 0; j < imageWidth; j++) {
                ind = (i * imageWidth) + j;
                if (imageBuffer[ind] > lmax) lmax = imageBuffer[ind];
        }
    
        return lmax;
}

void QSICCD::resetFrame()
{

        long sensorPixelSize_x,
             sensorPixelSize_y;
        try {
            QSICam.get_CameraXSize(&sensorPixelSize_x);
            QSICam.get_CameraYSize(&sensorPixelSize_y);
        } catch (std::runtime_error err)
        {
            IDMessage(getDeviceName(), "Getting image area size failed. %s.", err.what());
        }

        imageWidth  = sensorPixelSize_x;
        imageHeight = sensorPixelSize_y;

        try
        {
            QSICam.put_BinX(1);
            QSICam.put_BinY(1);
        } catch (std::runtime_error err)
        {
            IDMessage(getDeviceName(), "Resetting BinX/BinY failed. %s.", err.what());
            IDLog("Resetting BinX/BinY failed. %s.", err.what());
            return;
        }

        SetCCDParams(imageWidth, imageHeight, 16, 1, 1);

        IUResetSwitch(&ResetSP);
        ResetSP.s = IPS_IDLE;
        IDSetSwitch(&ResetSP, "Resetting frame and binning.");

        PrimaryCCD.setBin(1,1);
        updateCCDFrame(0,0, imageWidth, imageHeight);

        return;
}

void QSICCD::shutterControl()
{

        bool hasShutter;
        try{
            QSICam.get_HasShutter(&hasShutter);
        }catch (std::runtime_error err) {
            ShutterSP.s   = IPS_IDLE;
            ShutterS[0].s = ISS_OFF;
            ShutterS[1].s = ISS_OFF;
            IDMessage(getDeviceName(), "QSICamera::get_HasShutter() failed. %s.", err.what());
            IDLog("QSICamera::get_HasShutter() failed. %s.\n", err.what());
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
                }catch (std::runtime_error err)
                {
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
       /* try
       {
        if (isDebug())
            IDLog("Trying to see if the image is ready.\n");

            QSICam.get_ImageReady(&imageReady);
        } catch (std::runtime_error err)
        {
            IDMessage(getDeviceName(), "get_ImageReady() failed. %s.", err.what());
            IDLog("get_ImageReady() failed. %s.", err.what());
            InExposure = false;
            PrimaryCCD.setExposureFailed();
            SetTimer(POLLMS);
            return;
        }*/

        timeleft=CalcTimeLeft(ExpStart,imageExpose);

        if (timeleft < 1)
        {
            QSICam.get_ImageReady(&imageReady);

            while (!imageReady)
            {
                usleep(100);
                QSICam.get_ImageReady(&imageReady);

            }

            /* We're done exposing */
            IDMessage(getDeviceName(), "Exposure done, downloading image...");
            IDLog("Exposure done, downloading image...\n");

            PrimaryCCD.setExposure(0);
            InExposure = false;
            /* grab and save image */
            grabImage();

        }
        else
        {

         if (isDebug())
         {
            IDLog("With time left %ld\n", timeleft);
            IDLog("image not yet ready....\n");
         }

         PrimaryCCD.setExposure(timeleft);

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
            IDSetNumber(&TemperatureNP, "get_CCDTemperature() failed. %s.", err.what());
            IDLog("get_CCDTemperature() failed. %s.", err.what());
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
            IDSetNumber(&TemperatureNP, "get_CCDTemperature() failed. %s.", err.what());
            IDLog("get_CCDTemperature() failed. %s.", err.what());
            return;
        }

        if (fabs(TemperatureN[0].value - ccdTemp) <= TEMP_THRESHOLD)
        {
            TemperatureNP.s = IPS_OK;
            TemperatureRequestNP.s = IPS_OK;

            IDSetNumber(&TemperatureRequestNP, NULL);
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
            IDSetNumber(&CoolerNP, "get_CoolerPower() failed. %s.", err.what());
            IDLog("get_CoolerPower() failed. %s.", err.what());
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

    switch (ResetSP.s)
    {
      case IPS_IDLE:
         break;
      case IPS_OK:
         break;
      case IPS_BUSY:
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
                //QSICam.get_Position(&current_filter);
                if(current_filter < LAST_FILTER) current_filter++;
                else current_filter = FIRST_FILTER;
		SelectFilter(current_filter);
                //QSICam.put_Position(current_filter);
            } catch (std::runtime_error err)
            {
                FilterSP.s = IPS_IDLE;
                FilterS[0].s = ISS_OFF;
                FilterS[1].s = ISS_OFF;
                IDMessage(getDeviceName(), "QSICamera::get_FilterPos() failed. %s.", err.what());

                if (isDebug())
                    IDLog("QSICamera::get_FilterPos() failed. %s.\n", err.what());
                return;
            }

            FilterSlotN[0].value = current_filter;
            FilterS[0].s = ISS_OFF;
            FilterS[1].s = ISS_OFF;
            FilterSP.s = IPS_OK;
            IDSetSwitch(&FilterSP,"The current filter is %d\n", current_filter);
            IDSetNumber(&FilterSlotNP, NULL);
            if (isDebug())
                IDLog("The current filter is %d\n", current_filter);

            break;

          case ISS_OFF:
           try
           {
	        current_filter = QueryFilter();
	        //QSICam.get_Position(&current_filter);
                if(current_filter > FIRST_FILTER) current_filter--;
                else current_filter = LAST_FILTER;
                //QSICam.put_Position(current_filter);
		SelectFilter(current_filter);
            } catch (std::runtime_error err)
            {
                FilterSP.s = IPS_IDLE;
                FilterS[0].s = ISS_OFF;
                FilterS[1].s = ISS_OFF;
                IDMessage(getDeviceName(), "QSICamera::get_FilterPos() failed. %s.", err.what());

                if (isDebug())
                    IDLog("QSICamera::get_FilterPos() failed. %s.\n", err.what());
                return;
            }

            FilterSlotN[0].value = current_filter;
            FilterS[0].s = ISS_OFF;
            FilterS[1].s = ISS_OFF;
            FilterSP.s = IPS_OK;
            IDSetSwitch(&FilterSP,"The current filter is %d\n", current_filter);
            IDSetNumber(&FilterSlotNP, NULL);

            if (isDebug())
                IDLog("The current filter is %d\n", current_filter);

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
        IDMessage(getDeviceName(), "PulseGuide() failed. %s.", err.what());
      if (isDebug())
         IDLog("PulseGuide failed. %s.", err.what());
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
        IDMessage(getDeviceName(), "PulseGuide() failed. %s.", err.what());
      if (isDebug())
         IDLog("PulseGuide failed. %s.", err.what());
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
        IDMessage(getDeviceName(), "PulseGuide() failed. %s.", err.what());
      if (isDebug())
         IDLog("PulseGuide failed. %s.", err.what());
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
        IDMessage(getDeviceName(), "PulseGuide() failed. %s.", err.what());
      if (isDebug())
         IDLog("PulseGuide failed. %s.", err.what());
      return false;
    }

    return true;
}


bool QSICCD::GetFilterNames(const char* groupName)
{
    char filterName[MAXINDINAME];
    char filterLabel[MAXINDILABEL];
    int maxFilters = (int) FilterSlotN[0].max;
    //std::string filterDesignation[maxFilters];

    if (FilterNameT != NULL)
        delete FilterNameT;

    try
    {
     QSICam.get_Names(filterDesignation);
    }
    catch (std::runtime_error err)
    {
        IDMessage(getDeviceName(), "QSICamera::get_Names() failed. %s.", err.what());
        if (isDebug())
            IDLog("QSICamera::get_Names() failed. %s.", err.what());
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
      IDSetText(FilterNameTP, "put_Names() failed. %s.", err.what());
      if (isDebug())
         IDLog("put_Names() failed. %s.", err.what());
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
        IDSetNumber(&FilterSlotNP, "put_Position() failed. %s.", err.what());
        if (isDebug())
            IDLog("put_Position() failed. %s.", err.what());
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
        IDSetNumber(&FilterSlotNP, "get_Position() failed. %s.", err.what());
        if (isDebug())
            IDLog("get_Position() failed. %s.\n", err.what());
        return -1;
    }

    return newFilter + 1;
}

