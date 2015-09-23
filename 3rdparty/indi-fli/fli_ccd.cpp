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
#include <unistd.h>
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
   sim = false;
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
    IUFillSwitchVector(&PortSP, PortS, 4, getDeviceName(), "PORTS", "Port", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&CamInfoT[0],"Model","","");
    IUFillText(&CamInfoT[1],"HW Rev","","");
    IUFillText(&CamInfoT[2],"FW Rev","","");
    IUFillTextVector(&CamInfoTP,CamInfoT,3,getDeviceName(),"Model","",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    SetCCDCapability(CCD_CAN_ABORT | CCD_CAN_BIN | CCD_CAN_SUBFRAME | CCD_HAS_COOLER | CCD_HAS_SHUTTER);

}


void FLICCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    defineSwitch(&PortSP);

    addAuxControls();
}

bool FLICCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        defineText(&CamInfoTP);

        setupParams();

        timerID = SetTimer(POLLMS);
    }
    else
    {
        deleteProperty(CamInfoTP.name);

        rmTimer(timerID);
    }

    return true;
}


bool FLICCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    if(strcmp(dev,getDeviceName())==0)
    {
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

bool FLICCD::Connect()
{
    int err=0;

    IDMessage(getDeviceName(), "Attempting to find the FLI CCD...");

    sim = isSimulation();

    if (sim)
        return true;

    if (isDebug())
    {
        IDLog ("Connecting CCD\n");
        IDLog("Attempting to find the camera\n");
    }

    int portSwitchIndex = IUFindOnSwitchIndex(&PortSP);

    if (findFLICCD(Domains[portSwitchIndex]) == false)
    {
        IDMessage(getDeviceName(), "Error: no cameras were detected.");

        if (isDebug())
            IDLog("Error: no cameras were detected.\n");

        return false;
    }

    if ((err = FLIOpen(&fli_dev, FLICam.name, FLIDEVICE_CAMERA | FLICam.domain)))
    {

        IDMessage(getDeviceName(), "Error: FLIOpen() failed. %s.", strerror( (int) -err));

        if (isDebug())
            IDLog("Error: FLIOpen() failed. %s.\n", strerror( (int) -err));

        return false;
    }

    /* Success! */
    IDMessage(getDeviceName(), "CCD is online. Retrieving basic data.");
    if (isDebug())
       IDLog("CCD is online. Retrieving basic data.\n");

        return true;
}


bool FLICCD::Disconnect()
{
    int err;

    if (sim)
        return true;

    if ((err = FLIClose(fli_dev)))
    {
        IDMessage(getDeviceName(), "Error: FLIClose() failed. %s.", strerror( (int) -err));

        if (isDebug())
            IDLog("Error: FLIClose() failed. %s.\n", strerror( (int) -err));

        return false;
    }

    IDMessage(getDeviceName(), "CCD is offline.");
    return true;
}


bool FLICCD::setupParams()
{

    int err=0;

    if (isDebug())
        IDLog("In setupParams\n");

    char hw_rev[16], fw_rev[16];

    //////////////////////
    // 1. Get Camera Model
    //////////////////////
    if (!sim && (err = FLIGetModel (fli_dev, FLICam.model, 32)))
    {
      IDMessage(getDeviceName(), "FLIGetModel() failed. %s.", strerror((int)-err));

      if (isDebug())
        IDLog("FLIGetModel() failed. %s.\n", strerror((int)-err));
      return false;
    }

    if (sim)
        IUSaveText(&CamInfoT[0], getDeviceName());
    else
        IUSaveText(&CamInfoT[0], FLICam.model);

    ///////////////////////////
    // 2. Get Hardware revision
    ///////////////////////////
    if (sim)
        FLICam.HWRevision = 1;
    else if (( err = FLIGetHWRevision(fli_dev, &FLICam.HWRevision)))
    {
      IDMessage(getDeviceName(), "FLIGetHWRevision() failed. %s.", strerror((int)-err));

      if (isDebug())
        IDLog("FLIGetHWRevision() failed. %s.\n", strerror((int)-err));

      return false;
    }

    snprintf(hw_rev, 16, "%ld", FLICam.HWRevision);
    IUSaveText(&CamInfoT[1], hw_rev);

    ///////////////////////////
    // 3. Get Firmware revision
    ///////////////////////////
    if (sim)
        FLICam.FWRevision = 1;
    else if (( err = FLIGetFWRevision(fli_dev, &FLICam.FWRevision)))
    {
      IDMessage(getDeviceName(), "FLIGetFWRevision() failed. %s.", strerror((int)-err));

      if (isDebug())
        IDLog("FLIGetFWRevision() failed. %s.\n", strerror((int)-err));

      return false;
    }

    snprintf(fw_rev, 16, "%ld", FLICam.FWRevision);
    IUSaveText(&CamInfoT[2], fw_rev);


    IDSetText(&CamInfoTP, NULL);
    ///////////////////////////
    // 4. Get Pixel size
    ///////////////////////////
    if (sim)
    {
        FLICam.x_pixel_size = 5.4/1e6;
        FLICam.y_pixel_size = 5.4/1e6;
    }
    else if (( err = FLIGetPixelSize(fli_dev, &FLICam.x_pixel_size, &FLICam.y_pixel_size)))
    {
      IDMessage(getDeviceName(), "FLIGetPixelSize() failed. %s.", strerror((int)-err));
      if (isDebug())
        IDLog("FLIGetPixelSize() failed. %s.\n", strerror((int)-err));
      return false;
    }

    FLICam.x_pixel_size *= 1e6;
    FLICam.y_pixel_size *= 1e6;

    ///////////////////////////
    // 5. Get array area
    ///////////////////////////
    if (sim)
    {
        FLICam.Array_Area[0] = FLICam.Array_Area[1] = 0;
        FLICam.Array_Area[2] = 1280;
        FLICam.Array_Area[3] = 1024;
    }
    else if (( err = FLIGetArrayArea(fli_dev, &FLICam.Array_Area[0], &FLICam.Array_Area[1], &FLICam.Array_Area[2], &FLICam.Array_Area[3])))
    {
      IDMessage(getDeviceName(), "FLIGetArrayArea() failed. %s.", strerror((int)-err));
      if (isDebug())
        IDLog("FLIGetArrayArea() failed. %s.\n", strerror((int)-err));
      return false;
    }

    ///////////////////////////
    // 6. Get visible area
    ///////////////////////////
    if (sim)
    {
         FLICam.Visible_Area[0] = FLICam.Visible_Area[1] = 0;
         FLICam.Visible_Area[2] = 1280;
         FLICam.Visible_Area[3] = 1024;
    }
    else if (( err = FLIGetVisibleArea( fli_dev, &FLICam.Visible_Area[0], &FLICam.Visible_Area[1], &FLICam.Visible_Area[2], &FLICam.Visible_Area[3])))
    {
      IDMessage(getDeviceName(), "FLIGetVisibleArea() failed. %s.", strerror((int)-err));
      if (isDebug())
        IDLog("FLIGetVisibleArea() failed. %s.\n", strerror((int)-err));
      return false;
    }

    ///////////////////////////
    // 7. Get temperature
    ///////////////////////////
    if (sim)
        FLICam.temperature = 25.0;
    else if (( err = FLIGetTemperature(fli_dev, &FLICam.temperature)))
    {
      IDMessage(getDeviceName(), "FLIGetTemperature() failed. %s.", strerror((int)-err));
      if (isDebug())
        IDLog("FLIGetTemperature() failed. %s.\n", strerror((int)-err));
      return false;
    }

    IDMessage(getDeviceName(), "The CCD Temperature is %f.\n", FLICam.temperature);

    if (isDebug())
        IDLog("The CCD Temperature is %f.\n", FLICam.temperature);

    TemperatureN[0].value = FLICam.temperature;			/* CCD chip temperatre (degrees C) */
    TemperatureN[0].min = MIN_CCD_TEMP;
    TemperatureN[0].max = MAX_CCD_TEMP;

    IUUpdateMinMax(&TemperatureNP);
    IDSetNumber(&TemperatureNP, NULL);

    SetCCDParams(FLICam.Visible_Area[2] - FLICam.Visible_Area[0], FLICam.Visible_Area[3] - FLICam.Visible_Area[1], 16, FLICam.x_pixel_size, FLICam.y_pixel_size);

    /* 50 ms */
    minDuration = 0.05;

    /* Default frame type is NORMAL */
    if (!sim && (err = FLISetFrameType(fli_dev, FLI_FRAME_TYPE_NORMAL) ))
    {
      IDMessage(getDeviceName(),"FLISetFrameType() failed. %s.\n", strerror((int)-err));

      if (isDebug())
          IDLog("FLISetFrameType() failed. %s.\n", strerror((int)-err));

      return false;
    }

    /* X horizontal binning */
    if (!sim && (err = FLISetHBin(fli_dev, PrimaryCCD.getBinX()) ))
    {
      IDMessage(getDeviceName(),"FLISetBin() failed. %s.\n", strerror((int)-err));
      if (isDebug())
        IDLog("FLISetBin() failed. %s.\n", strerror((int)-err));
      return false;
    }

    /* Y vertical binning */
    if (!sim && (err = FLISetVBin(fli_dev, PrimaryCCD.getBinY()) ))
    {
      IDMessage(getDeviceName(),"FLISetVBin() failed. %s.\n", strerror((int)-err));
      if (isDebug())
        IDLog("FLISetVBin() failed. %s.\n", strerror((int)-err));

      return false;
    }

    int nbuf;
    nbuf=PrimaryCCD.getXRes()*PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8;                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;

}

int FLICCD::SetTemperature(double temperature)
{
    int err=0;

    if (!sim && (err = FLISetTemperature(fli_dev, temperature)))
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "FLISetTemperature() failed. %s.", strerror((int)-err));
        return -1;
    }

    FLICam.temperature = temperature;


    DEBUGF(INDI::Logger::DBG_ERROR, "Setting CCD temperature to %+06.2f C", temperature);
    return 0;
}

bool FLICCD::StartExposure(float duration)
{
        int err=0;

        if(duration < minDuration)
        {
            IDMessage(getDeviceName(), "Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.", duration,minDuration);
            duration = minDuration;
        }

        if (imageFrameType == CCDChip::BIAS_FRAME)
        {
            duration = minDuration;
            IDMessage(getDeviceName(), "Bias Frame (s) : %g\n", minDuration);
        }

        if (!sim && (err = FLISetExposureTime(fli_dev, (long)(duration * 1000))))
        {

            IDMessage(getDeviceName(), "FLISetExposureTime() failed. %s.\n", strerror((int)-err));

            if (isDebug())
                IDLog("FLISetExposureTime() failed. %s.\n", strerror((int)-err));

            return false;
        }
        // yes, we need to push the release 
        if (!sim && (err =FLIExposeFrame(fli_dev)))
        {

            IDMessage(getDeviceName(), "FLIxposeFrame() failed. %s.\n", strerror((int)-err));

            if (isDebug())
                IDLog("FLIxposeFrame() failed. %s.\n", strerror((int)-err));

            return false;
        }

        PrimaryCCD.setExposureDuration(duration);
        ExposureRequest = duration;
        if (isDebug())
             IDLog("Exposure Time (s) is: %g\n", duration);

        gettimeofday(&ExpStart,NULL);
        IDMessage(getDeviceName(), "Taking a %g seconds frame...", ExposureRequest);

        InExposure = true;
        return true;
}


bool FLICCD::AbortExposure()
{
    int err=0;

    if (!sim && (err = FLICancelExposure(fli_dev)))
    {
        IDMessage(getDeviceName(), "FLICancelExposure() failed. %s.", strerror((int)-err));

        if (isDebug())
            IDLog("FLICancelExposure() failed. %s.\n", strerror((int)-err));

      return false;

    }

    InExposure=false;
    return true;
}

bool FLICCD::UpdateCCDFrameType(CCDChip::CCD_FRAME fType)
{
    int err=0;
    CCDChip::CCD_FRAME imageFrameType = PrimaryCCD.getFrameType();
    // in indiccd.cpp imageFrameType is already set
    if (sim)
        return true;

    switch (imageFrameType)
    {
        case CCDChip::BIAS_FRAME:
        case CCDChip::DARK_FRAME:
        if ((err = FLISetFrameType(fli_dev, FLI_FRAME_TYPE_DARK) ))
        {
            if (isDebug())
                IDLog("FLISetFrameType() failed. %s.\n", strerror((int)-err));
            return -1;
        }
        break;

        case CCDChip::LIGHT_FRAME:
        case CCDChip::FLAT_FRAME:
        if ((err = FLISetFrameType(fli_dev, FLI_FRAME_TYPE_NORMAL) ))
        {
            if (isDebug())
                IDLog("FLISetFrameType() failed. %s.\n", strerror((int)-err));
            return -1;
        }
        break;
    }

    return true;

}

bool FLICCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    int err=0;

    /* Add the X and Y offsets */
    long x_1 = x;
    long y_1 = y;

    long bin_width  = x_1 + (w / PrimaryCCD.getBinX());
    long bin_height = y_1 + (h / PrimaryCCD.getBinY());

    if (bin_width > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
    {
        IDMessage(getDeviceName(), "Error: invalid width requested %d", w);
        return false;
    }
    else if (bin_height > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
    {
        IDMessage(getDeviceName(), "Error: invalid height request %d", h);
        return false;
    }

    if (isDebug())
        IDLog("The Final image area is (%ld, %ld), (%ld, %ld)\n", x_1, y_1, bin_width, bin_height);

    if (!sim && (err = FLISetImageArea(fli_dev, x_1, y_1, bin_width, bin_height) ))
    {
        IDMessage(getDeviceName(), "FLISetImageArea() failed. %s.\n", strerror((int)-err));
        if (isDebug())
            IDLog("FLISetImageArea() failed. %s.\n", strerror((int)-err));

        return false;
    }

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x_1, y_1, w,  h);

    int nbuf;
    nbuf=(bin_width*bin_height * PrimaryCCD.getBPP()/8);                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool FLICCD::UpdateCCDBin(int binx, int biny)
{

    int err=0;

    /* X horizontal binning */
    if (!sim && (err = FLISetHBin(fli_dev, binx) ))
    {
      IDMessage(getDeviceName(), "FLISetBin() failed. %s.\n", strerror((int)-err));
      if (isDebug())
          IDLog("FLISetHBin() failed. %s.\n", strerror((int)-err));
      return false;
    }

    /* Y vertical binning */
    if (!sim && (err = FLISetVBin(fli_dev, biny) ))
    {
      IDMessage(getDeviceName(), "FLISetVBin() failed. %s.\n", strerror((int)-err));
      if (isDebug())
          IDLog("FLISetVBin() failed. %s.\n", strerror((int)-err));

      return false;
    }

    PrimaryCCD.setBin(binx, biny);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
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

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int FLICCD::grabImage()
{
        int err=0;
        char * image = PrimaryCCD.getFrameBuffer();
        int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP()/8;
        int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

        if (sim)
        {
            for (int i=0; i < height ; i++)
                for (int j=0; j < width; j++)
                    image[i*width+j] = rand() % 255;
        }
        else
        {
            for (int i=0; i < height ; i++)
            {
                if ( (err = FLIGrabRow(fli_dev, image + (i * width), width)))
                {
                    IDMessage(getDeviceName(), "FLIGrabRow() failed at row %d. %s.", i, strerror((int)-err));

                    if (isDebug())
                        IDLog("FLIGrabRow() failed at row %d. %s.\n", i, strerror((int)-err));
                    return -1;
                }
            }
        }

        IDMessage(getDeviceName(), "Download complete.");

        if (isDebug())
            IDLog("Download complete.");

        ExposureComplete(&PrimaryCCD);

    return 0;
}

void FLICCD::TimerHit()
{
    int timerID=-1;
    int err=0;
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
                    while(!sim && timeleft > 0)
                    {

                        if ((err = FLIGetExposureStatus(fli_dev, &timeleft)))
                        {
                          IDMessage(getDeviceName(), "FLIGetExposureStatus() failed. %s.", strerror((int)-err));

                          if (isDebug())
                            IDLog("FLIGetExposureStatus() failed. %s.\n", strerror((int)-err));

                          SetTimer(POLLMS);
                          return;
                        }

                        int slv;
                        slv=100000*timeleft;
                        usleep(slv);
                    }

                    /* We're done exposing */
                    IDMessage(getDeviceName(), "Exposure done, downloading image...");

                    if (isDebug())
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
        if (!sim &&  (err = FLIGetTemperature(fli_dev, &ccdTemp)))
        {
          TemperatureNP.s = IPS_IDLE;
          IDSetNumber(&TemperatureNP, "FLIGetTemperature() failed. %s.", strerror((int)-err));

          if (isDebug())
            IDLog("FLIGetTemperature() failed. %s.", strerror((int)-err));
          break;
        }

        if (fabs(TemperatureN[0].value - ccdTemp) >= TEMP_THRESHOLD)
        {
          TemperatureN[0].value = ccdTemp;
          IDSetNumber(&TemperatureNP, NULL);
        }
        break;

      case IPS_BUSY:
        if (sim)
        {
            ccdTemp = FLICam.temperature;
            TemperatureN[0].value = ccdTemp;
        }
        else if ( (err = FLIGetTemperature(fli_dev, &ccdTemp)))
        {
          TemperatureNP.s = IPS_IDLE;
          IDSetNumber(&TemperatureNP, "FLIGetTemperature() failed. %s.", strerror((int)-err));

          if (isDebug())
            IDLog("FLIGetTemperature() failed. %s.", strerror((int)-err));
          break;
        }

        if (fabs(FLICam.temperature - ccdTemp) <= TEMP_THRESHOLD)
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

    if (timerID == -1)
        SetTimer(POLLMS);
    return;
}

bool FLICCD::findFLICCD(flidomain_t domain)
{
  char **tmplist;
  long err;

  if (isDebug())
    IDLog("In find Camera, the domain is %ld\n", domain);

  if (( err = FLIList(domain | FLIDEVICE_CAMERA, &tmplist)))
  {
      if (isDebug())
        IDLog("FLIList() failed. %s\n", strerror((int)-err));
    return false;
  }

  if (tmplist != NULL && tmplist[0] != NULL)
  {

    for (int i = 0; tmplist[i] != NULL; i++)
    {
      for (int j = 0; tmplist[i][j] != '\0'; j++)
            if (tmplist[i][j] == ';')
            {
                tmplist[i][j] = '\0';
                break;
            }
    }

   FLICam.domain = domain;

    switch (domain)
    {
        case FLIDOMAIN_PARALLEL_PORT:
            FLICam.dname = strdup("parallel port");
            break;

        case FLIDOMAIN_USB:
            FLICam.dname = strdup("USB");
            break;

        case FLIDOMAIN_SERIAL:
            FLICam.dname = strdup("serial");
            break;

        case FLIDOMAIN_INET:
            FLICam.dname = strdup("inet");
            break;

        default:
            FLICam.dname = strdup("Unknown domain");
    }

      FLICam.name = strdup(tmplist[0]);

     if ((err = FLIFreeList(tmplist)))
     {
         if (isDebug())
            IDLog("FLIFreeList() failed. %s.\n", strerror((int)-err));
       return false;
     }

   } /* end if */
   else
   {
     if ((err = FLIFreeList(tmplist)))
     {
         if (isDebug())
            IDLog("FLIFreeList() failed. %s.\n", strerror((int)-err));
       return false;
     }

     return false;
   }

  if (isDebug())
    IDLog("FindFLICCD() finished successfully.\n");

  return true;
}


