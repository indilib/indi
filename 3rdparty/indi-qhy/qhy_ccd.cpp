/*
 QHY INDI Driver

 Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Copyright (C) 2014 Zhirong Li (lzr@qhyccd.com)

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

#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include "qhy_ccd.h"

#define POLLMS                  1000		/* Polling time (ms) */
#define TEMPERATURE_POLL_MS     1000        /* Temperature Polling time (ms) */
#define TEMP_THRESHOLD          0.2         /* Differential temperature threshold (C)*/
#define MINIMUM_CCD_EXPOSURE    0.1         /* 0.1 seconds minimum exposure */
#define MAX_DEVICES             4           /* Max device cameraCount */

//NB Disable for real driver
#define USE_SIMULATION

static int cameraCount;
static QHYCCD *cameras[MAX_DEVICES];

//pthread_mutex_t     qhyMutex = PTHREAD_MUTEX_INITIALIZER;

static void cleanup()
{
  for (int i = 0; i < cameraCount; i++)
  {
    delete cameras[i];
  }

  ReleaseQHYCCDResource();
}

void ISInit()
{
  static bool isInit = false;

  if (!isInit)
  {
      char camid[64];
      bool allCameraInit = true;
      int ret = QHYCCD_ERROR;

      #ifndef USE_SIMULATION
      ret = InitQHYCCDResource();
      if(ret != QHYCCD_SUCCESS)
      {
          IDLog("Init QHYCCD SDK failed (%d)\n", ret);
          return;
      }
      cameraCount = ScanQHYCCD();
     #else
     cameraCount = 1;
     #endif

      for(int i = 0;i < cameraCount;i++)
      {
          memset(camid,'\0', MAXINDIDEVICE);

          #ifndef USE_SIMULATION
          ret = GetQHYCCDId(i,camid);
          #else
          ret = QHYCCD_SUCCESS;
          strncpy(camid, "Simulation", MAXINDIDEVICE);
          #endif
          if(ret == QHYCCD_SUCCESS)
          {
              cameras[i] = new QHYCCD(camid);
          }
          else
          {
              IDLog("#%d GetQHYCCDId error (%d)\n", i, ret);
              allCameraInit = false;
              break;
          }
      }

      if(cameraCount > 0 && allCameraInit)
      {
          atexit(cleanup);
          isInit = true;
      }
  }
}

void ISGetProperties(const char *dev)
{
  ISInit();
  for (int i = 0; i < cameraCount; i++)
  {
    QHYCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name))
    {
      camera->ISGetProperties(dev);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
  ISInit();
  for (int i = 0; i < cameraCount; i++)
  {
    QHYCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name))
    {
      camera->ISNewSwitch(dev, name, states, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
  ISInit();
  for (int i = 0; i < cameraCount; i++)
  {
    QHYCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name))
    {
      camera->ISNewText(dev, name, texts, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
  ISInit();
  for (int i = 0; i < cameraCount; i++)
  {
    QHYCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name))
    {
      camera->ISNewNumber(dev, name, values, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
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

void ISSnoopDevice(XMLEle *root)
{
    ISInit();

    for (int i = 0; i < cameraCount; i++) {
      QHYCCD *camera = cameras[i];
      if (!strcmp(findXMLAttValu(root, "device"), camera->name)) {
        camera->ISSnoopDevice(root);
        break;
      }
    }
}

QHYCCD::QHYCCD(const char *name)
{
    // Filter Limits, can we call QHY API to find filter maximum?
    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = 5;

    //HasUSBTraffic = false;
    HasUSBSpeed = false;
    HasGain = false;
    HasOffset = false;
    HasFilters = false;
    IsFocusMode = false;

    snprintf(this->name, MAXINDINAME, "QHY CCD %s", name);
    snprintf(this->camid,MAXINDINAME,"%s",name);
    setDeviceName(this->name);

  sim = false;
}

QHYCCD::~QHYCCD()
{
}

const char * QHYCCD::getDefaultName()
{
  return name;
}

bool QHYCCD::initProperties()
{
    INDI::CCD::initProperties();
    initFilterProperties(getDeviceName(), FILTER_TAB);

    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = 5;

    // CCD Cooler Switch
    IUFillSwitch(&CoolerS[0], "ON", "On", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "OFF", "Off", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_REGULATION", "Cooler", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // CCD Regulation power
    IUFillNumber(&CoolerN[0], "COOLER","Regulation (%)", "%.1f", 0, 0, 0, 0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER","Cooler", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // CCD Gain
    IUFillNumber(&GainN[0], "GAIN", "Gain", "%3.0f", 0, 100, 1, 11);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // CCD Offset
    IUFillNumber(&OffsetN[0], "Offset", "Offset", "%3.0f", 0, 0, 1, 0);
    IUFillNumberVector(&OffsetNP, OffsetN, 1, getDeviceName(), "CCD_OFFSET", "Offset", IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

    // USB Speed
    IUFillNumber(&SpeedN[0], "Speed", "Speed", "%3.0f", 0, 0, 1, 0);
    IUFillNumberVector(&SpeedNP, SpeedN, 1, getDeviceName(), "USB_SPEED", "USB Speed", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    addAuxControls();
    return true;
}

void QHYCCD::ISGetProperties(const char *dev)
{
  INDI::CCD::ISGetProperties(dev);

  // JM: On initial ISGetProperties we are disconnected, but if more clients to us, we send the properties right away if we are already connected.
  if (isConnected())
  {
      if (HasCooler())
      {
          defineSwitch(&CoolerSP);
          defineNumber(&CoolerNP);
      }

      if(HasUSBSpeed)
         defineNumber(&SpeedNP);

      if(HasGain)
        defineNumber(&GainNP);

      if(HasOffset)
        defineNumber(&OffsetNP);

      if(HasFilters)
      {
          //Define the Filter Slot and name properties
          defineNumber(&FilterSlotNP);
          if (FilterNameT != NULL)
              defineText(FilterNameTP);
      }
  }

}

bool QHYCCD::updateProperties()
{
  INDI::CCD::updateProperties();
  double min,max,step;

  if (isConnected())
  {

      if (HasCooler())
      {
          defineSwitch(&CoolerSP);
          defineNumber(&CoolerNP);
      }

      if(HasUSBSpeed)
      {
          if (sim)
          {
              SpeedN[0].min = 1;
              SpeedN[0].max = 5;
              SpeedN[0].step = 1;
              SpeedN[0].value = 1;
          }
          else
          {
              int ret = GetQHYCCDParamMinMaxStep(camhandle,CONTROL_SPEED,&min,&max,&step);
              if(ret == QHYCCD_SUCCESS)
              {
                  SpeedN[0].min = min;
                  SpeedN[0].max = max;
                  SpeedN[0].step = step;
              }

              SpeedN[0].value = GetQHYCCDParam(camhandle,CONTROL_SPEED);
          }

          defineNumber(&SpeedNP);
      }

      if(HasGain)
      {
          if (sim)
          {
              GainN[0].min = 0;
              GainN[0].max = 100;
              GainN[0].step = 10;
              GainN[0].value = 50;
          }
          else
          {
              int ret = GetQHYCCDParamMinMaxStep(camhandle,CONTROL_GAIN,&min,&max,&step);
              if(ret == QHYCCD_SUCCESS)
              {
                  GainN[0].min = min;
                  GainN[0].max = max;
                  GainN[0].step = step;
              }
              GainN[0].value = GetQHYCCDParam(camhandle,CONTROL_GAIN);
          }

          defineNumber(&GainNP);
      }

      if(HasOffset)
      {
          if (sim)
          {
              OffsetN[0].min = 1;
              OffsetN[0].max = 10;
              OffsetN[0].step = 1;
              OffsetN[0].value = 1;
          }
          else
          {
              int ret = GetQHYCCDParamMinMaxStep(camhandle,CONTROL_OFFSET,&min,&max,&step);
              if(ret == QHYCCD_SUCCESS)
              {
                  OffsetN[0].min = min;
                  OffsetN[0].max = max;
                  OffsetN[0].step = step;
              }
              OffsetN[0].value = GetQHYCCDParam(camhandle,CONTROL_OFFSET);
          }

          //Define the Offset
          defineNumber(&OffsetNP);
      }

      if(HasFilters)
      {
          //Define the Filter Slot and name properties
          defineNumber(&FilterSlotNP);

          GetFilterNames(FILTER_TAB);
          defineText(FilterNameTP);
      }

    // Let's get parameters now from CCD
    setupParams();

    timerID = SetTimer(POLLMS);
  } else
  {

      if (HasCooler())
      {
          deleteProperty(CoolerSP.name);
          deleteProperty(CoolerNP.name);
      }

      if(HasUSBSpeed)
      {
          deleteProperty(SpeedNP.name);
      }

      if(HasGain)
      {
          deleteProperty(GainNP.name);
      }

      if(HasOffset)
      {
          deleteProperty(OffsetNP.name);
      }

      if(HasFilters)
      {
          deleteProperty(FilterSlotNP.name);
          deleteProperty(FilterNameTP->name);
      }

    RemoveTimer(timerID);
  }

  return true;
}

bool QHYCCD::Connect()
{

    sim = isSimulation();

    int ret;

    CCDCapability cap;

    if (sim)
    {
        cap.canSubFrame = false;
        cap.hasGuideHead = false;
        cap.hasShutter = false;
        cap.canAbort = true;
        cap.hasCooler = true;
        cap.hasST4Port = true;

        SetCCDCapability(&cap);

        HasUSBSpeed = true;
        HasGain = true;
        HasOffset = true;
        HasFilters = true;

        return true;
    }

    camhandle = OpenQHYCCD(camid);

    if(camhandle != NULL)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Connected to %s.",camid);

        cap.canSubFrame = false;
        cap.hasGuideHead = false;
        cap.hasShutter = false;
        cap.canAbort = true;
        cap.hasCooler = false;

        ret = IsQHYCCDControlAvailable(camhandle,CONTROL_COOLER);
        if(ret == QHYCCD_SUCCESS)
        {
            cap.hasCooler = true;
        }

        ret = IsQHYCCDControlAvailable(camhandle,CONTROL_ST4PORT);
        if(ret == QHYCCD_SUCCESS)
        {
            cap.hasST4Port = true;
        }

        ret = IsQHYCCDControlAvailable(camhandle,CONTROL_SPEED);
        if(ret == QHYCCD_SUCCESS)
        {
            HasUSBSpeed = true;
        }

        ret = IsQHYCCDControlAvailable(camhandle,CONTROL_GAIN);
        if(ret == QHYCCD_SUCCESS)
        {
            HasGain = true;
        }

        ret = IsQHYCCDControlAvailable(camhandle,CONTROL_OFFSET);
        if(ret == QHYCCD_SUCCESS)
        {
            HasOffset = true;
        }

        ret = IsQHYCCDControlAvailable(camhandle,CONTROL_CFWPORT);
        if(ret == QHYCCD_SUCCESS)
        {
            HasFilters = true;
        }

        ret = InitQHYCCD(camhandle);
        if(ret != QHYCCD_SUCCESS)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error: Init Camera failed (%d)", ret);
            return false;
        }

        ret = IsQHYCCDControlAvailable(camhandle,CAM_BIN1X1MODE);
        if(ret == QHYCCD_SUCCESS)
        {
            camxbin = 1;
            camybin = 1;
            SetQHYCCDBinMode(camhandle,1,1);
        }

        ret &= IsQHYCCDControlAvailable(camhandle,CAM_BIN2X2MODE);
        ret &= IsQHYCCDControlAvailable(camhandle,CAM_BIN3X3MODE);
        ret &= IsQHYCCDControlAvailable(camhandle,CAM_BIN4X4MODE);
        if(ret == QHYCCD_SUCCESS)
        {
            cap.canBin = true;
        }

        SetCCDCapability(&cap);

        return true;
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Conect Camera failed (%s)",camid);

    return false;

}

bool QHYCCD::Disconnect()
{
  DEBUG(INDI::Logger::DBG_SESSION, "CCD is offline.");

  if (sim == false)
      CloseQHYCCD(camhandle);
  return true;

}

bool QHYCCD::setupParams()
{

    int nbuf,ret,imagew,imageh,bpp;
    double chipw,chiph,pixelw,pixelh;

    if (sim)
    {
        chipw=imagew=1280;
        chiph=imageh=1024;
        pixelh = pixelw = 5.4;
        bpp=8;
    }
    else
    {
        ret = GetQHYCCDChipInfo(camhandle,&chipw,&chiph,&imagew,&imageh,&pixelw,&pixelh,&bpp);

        /* JM: Why do we need this? Use PrimaryCCD buffer */
        /*
        if(ret == QHYCCD_SUCCESS)
        {
            rawdata = new unsigned char[imagew * imageh * 2];
        }*/

        /* JM: We need GetQHYCCDErrorString(ret) to get the string description of the error, please implement this in the SDK */
        if (ret != QHYCCD_SUCCESS)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error: GetQHYCCDChipInfo() (%d)", ret);
            return false;
        }
    }

    SetCCDParams(imagew,imageh,bpp,pixelw,pixelh);

    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    nbuf += 512;
    PrimaryCCD.setFrameBufferSize(nbuf);

    // JM Add must check for initial temperature as well if we have cooler

    if (HasFilters)
        GetFilterNames(FILTER_TAB);

    return true;
}

int QHYCCD::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature- TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    TemperatureRequest = temperature;
    return 0;
}

bool QHYCCD::StartExposure(float duration)
{
  int ret = QHYCCD_ERROR;

  //AbortPrimaryFrame = false;

  if (duration < MINIMUM_CCD_EXPOSURE)
  {
    DEBUGF(INDI::Logger::DBG_WARNING, "Exposure shorter than minimum duration %g s requested. \n Setting exposure time to %g s.", duration, MINIMUM_CCD_EXPOSURE);
    duration = MINIMUM_CCD_EXPOSURE;
  }

  imageFrameType = PrimaryCCD.getFrameType();

  if (imageFrameType == CCDChip::BIAS_FRAME)
  {
    duration = MINIMUM_CCD_EXPOSURE;
    DEBUGF(INDI::Logger::DBG_SESSION, "Bias Frame (s) : %g", duration);
  }

  DEBUGF(INDI::Logger::DBG_DEBUG, "Current exposure time is %f us",duration * 1000 * 1000);
  ExposureRequest = duration;
  PrimaryCCD.setExposureDuration(duration);

  if (sim)
      ret = QHYCCD_SUCCESS;
  else
      ret = SetQHYCCDParam(camhandle,CONTROL_EXPOSURE,ExposureRequest * 1000 * 1000);

  if(ret != QHYCCD_SUCCESS)
  {
      DEBUGF(INDI::Logger::DBG_ERROR, "Set expose time failed (%d).", ret);
  }

  gettimeofday(&ExpStart, NULL);
  DEBUGF(INDI::Logger::DBG_SESSION, "Taking a %g seconds frame...", ExposureRequest);

  InExposure = true;

  return true;
}

bool QHYCCD::AbortExposure()
{
    int ret;

    if (!InExposure || sim)
        return true;

    ret = StopQHYCCDExpSingle(camhandle);
    if(ret == QHYCCD_SUCCESS)
    {
        //AbortPrimaryFrame = true;
        InExposure = false;
        DEBUG(INDI::Logger::DBG_SESSION, "Exposure aborted.");
        return true;
    }
    else
        DEBUGF(INDI::Logger::DBG_ERROR, "Abort exposure failed (%d)", ret);

    return false;
}

bool QHYCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
  // FIXME FIXME
  /* JM: Why isn't this implemented in QHY INDI driver? QHY can't subframe? */

  /* Add the X and Y offsets */
  long x_1 = x;
  long y_1 = y;

  long bin_width = x_1 + (w / PrimaryCCD.getBinX());
  long bin_height = y_1 + (h / PrimaryCCD.getBinY());

  if (bin_width > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
  {
    DEBUGF(INDI::Logger::DBG_ERROR, "Error: invalid width requested %d", w);
    return false;
  } else if (bin_height > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
  {
    DEBUGF(INDI::Logger::DBG_ERROR, "Error: invalid height request %d", h);
    return false;
  }

  DEBUGF(INDI::Logger::DBG_DEBUG, "The Final image area is (%ld, %ld), (%ld, %ld)", x_1, y_1, bin_width, bin_height);

  /**********************************************************
   *
   *
   *
   *  IMPORRANT: Put here your CCD Frame dimension call
   *  The values calculated above are BINNED width and height
   *  which is what most CCD APIs require, but in case your
   *  CCD API implementation is different, don't forget to change
   *  the above calculations.
   *  If there is an error, report it back to client
   *  e.g.
   *  IDMessage(getDeviceName(), "Error, unable to set frame to ...");
   *  return false;
   *
   *
   **********************************************************/

  // Set UNBINNED coords
  PrimaryCCD.setFrame(x_1, y_1, w, h);

  int nbuf;
  nbuf = (bin_width * bin_height * PrimaryCCD.getBPP() / 8);    //  this is pixel count
  nbuf += 512;    //  leave a little extra at the end
  PrimaryCCD.setFrameBufferSize(nbuf);

  DEBUGF(INDI::Logger::DBG_DEBUG, "Setting frame buffer size to %d bytes.", nbuf);

  return true;
}

bool QHYCCD::UpdateCCDBin(int hor, int ver)
{
    int ret = QHYCCD_ERROR;

    if (sim)
    {
        PrimaryCCD.setBin(hor,ver);
        camxbin = hor;
        camybin = ver;
        return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
    }

    if(hor == 1 && ver == 1)
    {
        ret = IsQHYCCDControlAvailable(camhandle,CAM_BIN1X1MODE);
    }
    else if(hor == 2 && ver == 2)
    {
        ret = IsQHYCCDControlAvailable(camhandle,CAM_BIN2X2MODE);
    }
    else if(hor == 3 && ver == 3)
    {
        ret = IsQHYCCDControlAvailable(camhandle,CAM_BIN3X3MODE);
    }
    else if(hor == 4 && ver == 4)
    {
        ret = IsQHYCCDControlAvailable(camhandle,CAM_BIN4X4MODE);
    }

    if(ret == QHYCCD_SUCCESS)
    {
        PrimaryCCD.setBin(hor,ver);
        camxbin = hor;
        camybin = ver;
        return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "SetBin mode failed. Invalid bin requested %dx%d",hor,ver);
    return false;
}

float QHYCCD::CalcTimeLeft()
{
  double timesince;
  double timeleft;
  struct timeval now;
  gettimeofday(&now, NULL);

  timesince = (double) (now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double) (ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
  timesince = timesince / 1000;

  timeleft = ExposureRequest - timesince;
  return timeleft;
}

/* Downloads the image from the CCD. */
int QHYCCD::grabImage()
{
  unsigned char * image = (unsigned char *) PrimaryCCD.getFrameBuffer();
  int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() / 8;
  int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

  if (sim)
  {
    for (int i = 0; i < height; i++)
      for (int j = 0; j < width; j++)
        image[i * width + j] = rand() % 255;
  } else
  {

    int ret;
    int w,h,bpp,channels;

    /* JM: Why do we need this? We have have (char * image) above with the CCD buffer that was already allocated */
    /*
    unsigned short *pt;
    unsigned char *pt8bit;

    pt = (unsigned short int *)targetChip->getFrameBuffer();
    pt8bit = (unsigned char *)malloc(GetQHYCCDMemLength(camhandle));


    ret = GetQHYCCDSingleFrame(camhandle,&w,&h,&bpp,&channels,pt8bit);
    DEBUGF(INDI::Logger::DBG_SESSION, "Current resolution is width:%d height:%d",w,h);
    DEBUGF(INDI::Logger::DBG_SESSION, "Current bin mode is wbin:%d hbin:%d",PrimaryCCD.getBinX(),PrimaryCCD.getBinY());
    */

    // JM: Suggested call
    ret = GetQHYCCDSingleFrame(camhandle,&w,&h,&bpp,&channels,image);

    /* JM: We don't need to do this, the image pointer above already has the image data */
    /*
    if(bpp == 8)
    {
        for(int i = 0;i < w * h;i++)
        {
            *pt = pt8bit[i];
            pt++;
        }
    }
    else
    {
        memcpy(pt,pt8bit,w * h * bpp / 8);
    }
    */

    /* JM: Why are we updating CCD Frame? The width & height are already set and they are not suppose to change after an exposure */
    /*
    if(ret == QHYCCD_SUCCESS)
    {
        PrimaryCCD.setFrame(0,0,w * PrimaryCCD.getBinX(),h * PrimaryCCD.getBinY());
    }


    if(pt8bit)
    {
        free(pt8bit);
    }
    */

  }

  DEBUG(INDI::Logger::DBG_SESSION, "Download complete.");

  ExposureComplete(&PrimaryCCD);

  return 0;
}

void QHYCCD::TimerHit()
{
  int timerID = -1;
  long timeleft;

  if (isConnected() == false)
    return;  //  No need to reset timer if we are not connected anymore

  if (InExposure)
  {
    timeleft = CalcTimeLeft();

    if (timeleft < 1.0)
    {
      if (timeleft > 0.25)
      {
        //  a quarter of a second or more
        //  just set a tighter timer
        timerID = SetTimer(250);
      } else
      {
        if (timeleft > 0.07)
        {
          //  use an even tighter timer
          timerID = SetTimer(50);
        } else
        {
          /* JM: Is there a QHY API call to find if the frame is ready for download?? */

          /* We're done exposing */
          DEBUG(INDI::Logger::DBG_SESSION, "Exposure done, downloading image...");

          PrimaryCCD.setExposureLeft(0);
          InExposure = false;
          /* grab and save image */
          grabImage();

        }
      }
    } else
    {

      DEBUGF(INDI::Logger::DBG_DEBUG, "Exposure in progress: Time left %ld", timeleft);
    }

      PrimaryCCD.setExposureLeft(timeleft);

   }

  if (timerID == -1)
    SetTimer(POLLMS);

}

bool QHYCCD::GuideNorth(float duration)
{
   ControlQHYCCDGuide(camhandle,0,duration);
   return true;
}

bool QHYCCD::GuideSouth(float duration)
{
    ControlQHYCCDGuide(camhandle,2,duration);
    return true;
}

bool QHYCCD::GuideEast(float duration)
{
    ControlQHYCCDGuide(camhandle,1,duration);
    return true;
}

bool QHYCCD::GuideWest(float duration)
{
    ControlQHYCCDGuide(camhandle,3,duration);
    return true;
}

bool QHYCCD::SelectFilter(int position)
{
    int ret = ControlQHYCCDCFW(camhandle,position);
    if(ret == QHYCCD_SUCCESS)
    {
        CurrentFilter = position;
        SelectFilterDone(position);
        return true;
    }

    return false;
}

bool QHYCCD::SetFilterNames()
{
    // Cannot save it in hardware, so let's just save it in the config file to be loaded later
    saveConfig();
    return true;
}

bool QHYCCD::GetFilterNames(const char* groupName)
{
    char filterName[MAXINDINAME];
    char filterLabel[MAXINDILABEL];
    char filterBand[MAXINDILABEL];
    int MaxFilter = FilterSlotN[0].max;

    if (FilterNameT != NULL)
        delete FilterNameT;

    FilterNameT = new IText[MaxFilter];

    for (int i=0; i < MaxFilter; i++)
    {
        snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i+1);
        snprintf(filterLabel, MAXINDILABEL, "Filter#%d", i+1);
        snprintf(filterBand, MAXINDILABEL, "Filter #%d", i+1);
        IUFillText(&FilterNameT[i], filterName, filterLabel, filterBand);
    }

    IUFillTextVector(FilterNameTP, FilterNameT, MaxFilter, getDeviceName(), "FILTER_NAME", "Filter", groupName, IP_RW, 0, IPS_IDLE);

    return true;
}

int QHYCCD::QueryFilter()
{
    return CurrentFilter;
}

bool QHYCCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,CoolerSP.name) == 0)
        {
          IUUpdateSwitch(&CoolerSP, states, names, n);

          if (CoolerS[0].s == ISS_ON)
          {
              temperatureFlag = true;
              CoolerSP.s = IPS_OK;
              temperatureID = IEAddTimer(TEMPERATURE_POLL_MS, QHYCCD::UpdateTemperatureHelper, this);
              TemperatureNP.s = IPS_OK;
              CoolerNP.s = IPS_OK;
          }
          else
          {
              DEBUG(INDI::Logger::DBG_SESSION, "Cooler off.");
              temperatureFlag = false;
              IERmTimer(temperatureID);
              CoolerSP.s = IPS_IDLE;
              if (sim == false)
                  SetQHYCCDParam(camhandle,CONTROL_MANULPWM,0);
              TemperatureNP.s = IPS_IDLE;
              CoolerNP.s = IPS_IDLE;
          }

          IDSetSwitch(&CoolerSP, NULL);

          return true;
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev,name,states,names,n);
}


bool QHYCCD::ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,FilterNameTP->name)==0)
        {
            processFilterName(dev, texts, names, n);
            return true;
        }

    }

    return INDI::CCD::ISNewText(dev,name,texts,names,n);
}

bool QHYCCD::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("INDI::CCD::ISNewNumber %s\n",name);
    if(strcmp(dev,getDeviceName())==0)
    {
        if (strcmp(name,FilterSlotNP.name)==0)
        {
            processFilterSlot(getDeviceName(), values, names);
            return true;
        }

        if(strcmp(name,GainNP.name) == 0)
        {
            IUUpdateNumber(&GainNP, values, names, n);
            SetQHYCCDParam(camhandle,CONTROL_GAIN,GainN[0].value);
            DEBUGF(INDI::Logger::DBG_SESSION, "Current %s value %f",GainNP.name,GainN[0].value);
            GainNP.s = IPS_OK;
            IDSetNumber(&GainNP, NULL);
            //saveConfig();
            return true;
        }


        if(strcmp(name,OffsetNP.name) == 0)
        {
            IUUpdateNumber(&OffsetNP, values, names, n);
            SetQHYCCDParam(camhandle,CONTROL_OFFSET,OffsetN[0].value);
            DEBUGF(INDI::Logger::DBG_SESSION, "Current %s value %f",OffsetNP.name,OffsetN[0].value);
            OffsetNP.s = IPS_OK;
            IDSetNumber(&OffsetNP, NULL);
            saveConfig();
            return true;
        }

        if(strcmp(name,SpeedNP.name) == 0)
        {
            IUUpdateNumber(&SpeedNP, values, names, n);
            SetQHYCCDParam(camhandle,CONTROL_SPEED,SpeedN[0].value);
            DEBUGF(INDI::Logger::DBG_SESSION, "Current %s value %f",SpeedNP.name,SpeedN[0].value);
            SpeedNP.s = IPS_OK;
            IDSetNumber(&SpeedNP, NULL);
            saveConfig();
            return true;
        }

        /*
        if(strcmp(name,USBTRAFFICNP->name) == 0)
        {
            USBTRAFFICNP->s = IPS_BUSY;
            IDSetNumber(USBTRAFFICNP, NULL);
            IUUpdateNumber(USBTRAFFICNP, values, names, n);
            SetQHYCCDParam(camhandle,CONTROL_USBTRAFFIC,USBTRAFFICN[0].value);
            DEBUGF(INDI::Logger::DBG_SESSION, "Current %s value %f",USBTRAFFICNP->name,USBTRAFFICN[0].value);
            USBTRAFFICNP->s = IPS_OK;
            IDSetNumber(USBTRAFFICNP, NULL);
            saveConfig();
            return true;
        }
    */
    }
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

void QHYCCD::UpdateTemperatureHelper(void *p)
{
    if (static_cast<QHYCCD*>(p)->isConnected())
        static_cast<QHYCCD*>(p)->UpdateTemperature();
}

void QHYCCD::UpdateTemperature()
{
    double ccdtemp,coolpower;

    if(temperatureFlag)
    {
        //JM Why mutex lock required? It's all running in the same thread!
        //pthread_mutex_lock(&qhyMutex);
        if (sim)
        {
            ccdtemp = TemperatureN[0].value;
            if (TemperatureN[0].value < TemperatureRequest)
                ccdtemp += TEMP_THRESHOLD;
            else
                ccdtemp -= TEMP_THRESHOLD;

            coolpower = 128;
        }
        else
        {
            ccdtemp = GetQHYCCDParam(camhandle,CONTROL_CURTEMP);
            coolpower = GetQHYCCDParam(camhandle,CONTROL_CURPWM);
            ControlQHYCCDTemp(camhandle,TemperatureRequest);
        }

        TemperatureN[0].value = ccdtemp;
        CoolerN[0].value = coolpower / 255.0 * 100;

        if (fabs(TemperatureN[0].value - TemperatureRequest) <= TEMP_THRESHOLD)
            TemperatureNP.s = IPS_OK;

        IDSetNumber(&TemperatureNP, NULL);
        IDSetNumber(&CoolerNP, NULL);

        //pthread_mutex_unlock(&qhyMutex);
        temperatureID = IEAddTimer(TEMPERATURE_POLL_MS,QHYCCD::UpdateTemperatureHelper, this);
    }
}

