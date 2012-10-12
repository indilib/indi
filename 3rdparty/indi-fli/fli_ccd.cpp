#if 0
    FLI CCD
    INDI Interface for Finger Lakes Instrument CCDs
    Copyright (C) 2003-2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <memory>
#include <time.h>
#include <math.h>
#include <sys/time.h>

#include "indidevapi.h"
#include "eventloop.h"

#include "fli_ccd.h"

#define MAX_CCD_TEMP	45		/* Max CCD temperature */
#define MIN_CCD_TEMP	-55		/* Min CCD temperature */
#define MAX_X_BIN	16		/* Max Horizontal binning */
#define MAX_Y_BIN	16		/* Max Vertical binning */
#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define POLLMS		1000		/* Polling time (ms) */
#define TEMP_THRESHOLD  .25		/* Differential temperature threshold (C)*/
#define NFLUSHES	1		/* Number of times a CCD array is flushed before an exposure */

std::auto_ptr<FLICCD> fliCCD(0);

const flidomain_t Domains[] = { FLIDOMAIN_USB, FLIDOMAIN_SERIAL, FLIDOMAIN_PARALLEL_PORT,  FLIDOMAIN_INET };

 void ISInit()
 {
    static int isInit =0;

    if (isInit == 1)
        return;

     isInit = 1;
     if(fliCCD.get() == 0) fliCCD.reset(new FLICCD());

 }

 void ISGetProperties(const char *dev)
 {
         ISInit();
         fliCCD->ISGetProperties(dev);
 }

 void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
 {
         ISInit();
         fliCCD->ISNewSwitch(dev, name, states, names, num);
 }

 void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
 {
         ISInit();
         fliCCD->ISNewText(dev, name, texts, names, num);
 }

 void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
 {
         ISInit();
         fliCCD->ISNewNumber(dev, name, values, names, num);
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
     fliCCD->ISSnoopDevice(root);
 }


FLICCD::FLICCD()
{

}


FLICCD::~FLICCD()
{

}

const char * FLICCD::getDefaultName()
{
    return (char *)"FLI CCD";
}

bool FLICCD::initProperties()
{
    // Init parent properties first
    INDI::CCD::initProperties();

    IUFillSwitch(&PortS[0], "USB", "USB", ISS_ON);
    IUFillSwitch(&PortS[1], "SERIAL", "Serial", ISS_OFF);
    IUFillSwitch(&PortS[2], "PARALLEL", "Parallel", ISS_OFF);
    IUFillSwitch(&PortS[3], "INET", "INet", ISS_OFF);
    IUFillSwitchVector(&PortSP, PortS, 1, getDeviceName(), "PORTS", "Port", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&ResetS[0], "RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "FRAME_RESET", "Frame Values", IMAGE_SETTINGS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&TemperatureN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", MIN_CCD_TEMP, MAX_CCD_TEMP, 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "CCD_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    IUFillText(&CamInfoT[0],"Model","","");
    IUFillText(&CamInfoT[1],"HW Rev","","");
    IUFillText(&CamInfoT[1],"SW Rev","","");
    IUFillTextVector(&CamInfoTP,CamInfoT,2,getDeviceName(),"Model","",IMAGE_INFO_TAB,IP_RW,60,IPS_IDLE);

    addAuxControls();

}


void FLICCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    defineSwitch(&PortSP);
}

bool FLICCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        defineNumber(&TemperatureNP);
        defineSwitch(&ResetSP);
        defineText(&CamInfoTP);

        setupParams();

        /*if (manageDefaults(msg))
        {
            IDMessage(getDeviceName(), msg, NULL);
            if (isDebug())
                IDLog("%s\n", msg);
            return false;
        }*/

        timerID = SetTimer(POLLMS);
    }
    else
    {

        deleteProperty(TemperatureNP.name);
        deleteProperty(ResetSP.name);
        deleteProperty(CamInfoTP.name);

        rmTimer(timerID);
    }

    return true;
}


bool FLICCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if(strcmp(dev,getDeviceName())==0)
    {

        /* Reset */
        if (!strcmp (name, ResetSP.name))
        {
          if (IUUpdateSwitch(&ResetSP, states, names, n) < 0) return false;
          resetFrame();
          return true;
        }

        /* Ports */
        if (!strcmp (name, PortSP.name))
        {
          if (IUUpdateSwitch(&PortSP, states, names, n) < 0)
              return false;

          PortSP.s = IPS_OK;
          IDSetSwitch(&PortSP, NULL);
          return true;
        }

    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev,name,states,names,n);
}

bool FLICCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INumber *np;

    if(strcmp(dev,getDeviceName())==0)
    {

        /* Temperature*/
        if (!strcmp(TemperatureNP.name, name))
        {
            TemperatureNP.s = IPS_IDLE;

            np = IUFindNumber(&TemperatureNP, names[0]);

            if (!np)
            {
                IDSetNumber(&TemperatureNP, "Unknown error. %s is not a member of %s property.", names[0], name);
                return false;
            }

            if (values[0] < MIN_CCD_TEMP || values[0] > MAX_CCD_TEMP)
            {
                IDSetNumber(&TemperatureNP, "Error: valid range of temperature is from %d to %d", MIN_CCD_TEMP, MAX_CCD_TEMP);
                return false;
            }

            /***************************************************
             *
             *
             * FLI SET TEMP CODE HERE
             **************************************************/

            TemperatureNP.s = IPS_BUSY;

            IDSetNumber(&TemperatureNP, "Setting CCD temperature to %+06.2f C", values[0]);
            if (isDebug())
                IDLog("Setting CCD temperature to %+06.2f C\n", values[0]);
            return true;
        }
    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

bool FLICCD::Connect()
{
    bool connected;
    char msg[1024];

    IDMessage(getDeviceName(), "Attempting to find the FLI CCD...");

    if (isDebug())
    {
        IDLog ("Connecting CCD\n");
        IDLog("Attempting to find the camera\n");
    }


        /********************* ADD FLI CONNECT CODE HERE
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

       ****************************************************************/

            /* Success! */
            IDMessage(getDeviceName(), "CCD is online. Retrieving basic data.");
            if (isDebug())
                IDLog("CCD is online. Retrieving basic data.\n");


            return true;
}


bool FLICCD::Disconnect()
{
    char msg[MAXRBUF];
    bool connected;


    /************************************************* Add FLI Disconnect here
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
    *******************************************/

    IDMessage(getDeviceName(), "CCD is offline.");
    return true;
}


bool FLICCD::setupParams()
{

    if (isDebug())
        IDLog("In setupParams\n");

    string name,model;
    double temperature;
    double pixel_size_x,pixel_size_y;
    long sub_frame_x,sub_frame_y;

    /***************** GET FLI INFO HERE
        QSICam.get_Name(name);
        QSICam.get_ModelNumber(model);
        QSICam.get_PixelSizeX(&pixel_size_x);
        QSICam.get_PixelSizeY(&pixel_size_y);
        QSICam.get_NumX(&sub_frame_x);
        QSICam.get_NumY(&sub_frame_y);
        QSICam.get_CCDTemperature(&temperature);

        Add to it Model, HW REV and SW REV
        ***************************************/

    IDMessage(getDeviceName(), "The CCD Temperature is %f.\n", temperature);

    if (isDebug())
        IDLog("The CCD Temperature is %f.\n", temperature);

    TemperatureN[0].value = temperature;			/* CCD chip temperatre (degrees C) */

    SetCCDParams(sub_frame_x, sub_frame_y, 16, pixel_size_x, pixel_size_y);

    imageWidth  = PrimaryCCD.getSubW();
    imageHeight = PrimaryCCD.getSubH();

    IDSetNumber(&TemperatureNP, NULL);

    int nbuf;
    nbuf=PrimaryCCD.getXRes()*PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8;                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

int FLICCD::StartExposure(float duration)
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
            PrimaryCCD.setExposureDuration(minDuration);
            IDMessage(getDeviceName(), "Bias Frame (s) : %g\n", minDuration);
            if (isDebug())
                IDLog("Bias Frame (s) : %g\n", minDuration);
        } else
        {
            PrimaryCCD.setExposureDuration(duration);
            if (isDebug())
                IDLog("Exposure Time (s) is: %g\n", duration);
        }

         ExposureRequest = PrimaryCCD.getExposureDuration();

         /************ PUT FLI EXPOSURE CODE HERE
          *
            // BIAS frame is the same as DARK but with minimum period. i.e. readout from camera electronics.
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
            **********************************************/

            //ImageExposureNP.s = IPS_BUSY;
            gettimeofday(&ExpStart,NULL);
            IDMessage(getDeviceName(), "Taking a %g seconds frame...", ExposureRequest);

            if (isDebug())
                IDLog("Taking a frame...\n");

            InExposure = true;
            return (shortExposure ? 1 : 0);
}

bool FLICCD::AbortExposure()
{

    /* PUT FLI Abort Exposure HERE

        InExposure = false;
        return true;


        */


    return false;

}

float FLICCD::CalcTimeLeft()
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

bool FLICCD::updateCCDFrame(int x, int y, int w, int h)
{
    /* Add the X and Y offsets */
    long x_1 = x;
    long y_1 = y;

    long x_2 = x_1 + (w / PrimaryCCD.getBinX());
    long y_2 = y_1 + (h / PrimaryCCD.getBinY());

    if (x_2 > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
    {
        IDMessage(getDeviceName(), "Error: invalid width requested %ld", x_2);
        return false;
    }
    else if (y_2 > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
    {
        IDMessage(getDeviceName(), "Error: invalid height request %ld", y_2);
        return false;
    }


    if (isDebug())
        IDLog("The Final image area is (%ld, %ld), (%ld, %ld)\n", x_1, y_1, x_2, y_2);

    imageWidth  = x_2 - x_1;
    imageHeight = y_2 - y_1;

    /**** PUT FLI SET FRAME CODE HERE
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
    ********************/

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x_1, y_1, w,  h);

    int nbuf;
    nbuf=(imageWidth*imageHeight * PrimaryCCD.getBPP()/8);                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool FLICCD::updateCCDBin(int binx, int biny)
{

    /********** PUT FLI SET BINNING CODE HERE *
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

    ***************/

    PrimaryCCD.setBin(binx, biny);

    return updateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int FLICCD::grabImage()
{
        unsigned short* image = (unsigned short *) PrimaryCCD.getFrameBuffer();

        /*******************
         *
         * PUT FLI grab image code here
         *
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
            IDMessage(getDeviceName(), "get_ImageArray() failed. %s.", err.what());
            IDLog("get_ImageArray() failed. %s.", err.what());
            return -1;
        }

        *******************/

        IDMessage(getDeviceName(), "Download complete.\n");

        ExposureComplete(&PrimaryCCD);

    return 0;
}

void FLICCD::addFITSKeywords(fitsfile *fptr, CCDChip *targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

    int status=0;
    fits_update_key_s(fptr, TDOUBLE, "CCD-TEMP", &(TemperatureN[0].value), "CCD Temperature (Celcius)", &status);
    fits_write_date(fptr, &status);
}


void FLICCD::resetFrame()
{

    /**** RESET TO FLI DEFAULT VALUES ***
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

     */

        IUResetSwitch(&ResetSP);
        ResetSP.s = IPS_IDLE;
        IDSetSwitch(&ResetSP, "Resetting frame and binning.");

        PrimaryCCD.setBin(1,1);
        updateCCDFrame(0,0, imageWidth, imageHeight);

        return;
}

void FLICCD::TimerHit()
{
    int timerID=-1;
    long timeleft;
    double ccdTemp;

    if(isConnected() == false)
        return;  //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft=CalcTimeLeft();

        if(timeleft < 1.0)
        {
            if(timeleft > 0.25)
            {
                //  a quarter of a second or more
                //  just set a tighter timer
                timerID = SetTimer(250);
            } else
            {
                if(timeleft >0.07)
                {
                    //  use an even tighter timer
                    timerID = SetTimer(50);
                } else
                {
                    //  it's real close now, so spin on it
                    while(timeleft > 0)
                    {
                        int slv;
                        slv=100000*timeleft;
                        //IDLog("usleep %d\n",slv);
                        usleep(slv);
                        timeleft=CalcTimeLeft();
                    }

                    /* FLI CODE TO get image
                    QSICam.get_ImageReady(&imageReady);

                    while (!imageReady)
                    {
                        usleep(100);
                        QSICam.get_ImageReady(&imageReady);

                    }

                    **********/

                    /* We're done exposing */
                    IDMessage(getDeviceName(), "Exposure done, downloading image...");
                    IDLog("Exposure done, downloading image...\n");

                    PrimaryCCD.setExposureLeft(0);
                    InExposure = false;
                    /* grab and save image */
                    grabImage();

                }
            }
        }
        else
        {

         if (isDebug())
         {
            IDLog("With time left %ld\n", timeleft);
            IDLog("image not yet ready....\n");
         }

         PrimaryCCD.setExposureLeft(timeleft);

        }

    }

    switch (TemperatureNP.s)
    {
      case IPS_IDLE:
      case IPS_OK:
        /* FLI GET TEMPERATURE CODE
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
        */

        if (fabs(TemperatureN[0].value - ccdTemp) >= TEMP_THRESHOLD)
        {
           TemperatureN[0].value = ccdTemp;
           IDSetNumber(&TemperatureNP, NULL);
        }
        break;

      case IPS_BUSY:
        /* FLI GET TEMPERATURE CODE
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
        */

        if (fabs(TemperatureN[0].value - ccdTemp) <= TEMP_THRESHOLD)
        {
            TemperatureNP.s = IPS_OK;
            TemperatureNP.s = IPS_OK;

            IDSetNumber(&TemperatureNP, NULL);
        }

        TemperatureN[0].value = ccdTemp;
        IDSetNumber(&TemperatureNP, NULL);
        break;

      case IPS_ALERT:
        break;
    }

    if (timerID == -1)
        SetTimer(POLLMS);
    return;
}

