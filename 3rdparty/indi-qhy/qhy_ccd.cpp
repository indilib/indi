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
#include "config.h"

#define POLLMS                  1000        /* Polling time (ms) */
#define TEMPERATURE_POLL_MS     1000        /* Temperature Polling time (ms) */
#define TEMP_THRESHOLD          0.2         /* Differential temperature threshold (C)*/
#define MINIMUM_CCD_EXPOSURE    0.1         /* 0.1 seconds minimum exposure */
#define MAX_DEVICES             4           /* Max device cameraCount */

//NB Disable for real driver
//#define USE_SIMULATION

static int cameraCount;
static QHYCCD *cameras[MAX_DEVICES];

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
      char camid[MAXINDIDEVICE];
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
     cameraCount = 2;
     #endif

      for(int i = 0;i < cameraCount;i++)
      {
          memset(camid,'\0', MAXINDIDEVICE);

          #ifndef USE_SIMULATION
          ret = GetQHYCCDId(i,camid);
          #else
          ret = QHYCCD_SUCCESS;
          snprintf(camid, MAXINDIDEVICE, "Model %d", i+1);
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

    for (int i = 0; i < cameraCount; i++)
    {
      QHYCCD *camera = cameras[i];
      camera->ISSnoopDevice(root);
    }
}

QHYCCD::QHYCCD(const char *name)
{
    // Filter Limits, can we call QHY API to find filter maximum?
    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = 5;

    HasUSBSpeed = false;
    HasGain = false;
    HasOffset = false;
    HasFilters = false;
    IsFocusMode = false;

    snprintf(this->name, MAXINDINAME, "QHY CCD %s", name);
    snprintf(this->camid,MAXINDINAME,"%s",name);
    setDeviceName(this->name);

    setVersion(INDI_QHY_VERSION_MAJOR, INDI_QHY_VERSION_MINOR);

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
    IUFillSwitch(&CoolerS[0], "COOLER_ON", "On", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "Off", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // CCD Regulation power
    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // CCD Gain
    IUFillNumber(&GainN[0], "GAIN", "Gain", "%3.0f", 0, 100, 1, 11);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // CCD Offset
    IUFillNumber(&OffsetN[0], "Offset", "Offset", "%3.0f", 0, 0, 1, 0);
    IUFillNumberVector(&OffsetNP, OffsetN, 1, getDeviceName(), "CCD_OFFSET", "Offset", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // USB Speed
    IUFillNumber(&SpeedN[0], "Speed", "Speed", "%3.0f", 0, 0, 1, 0);
    IUFillNumberVector(&SpeedNP, SpeedN, 1, getDeviceName(), "USB_SPEED", "USB Speed", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    addAuxControls();

    setInterfaceDescriptor(getInterfaceDescriptor() | FILTER_INTERFACE);

    return true;
}

void QHYCCD::ISGetProperties(const char *dev)
{
  INDI::CCD::ISGetProperties(dev);

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
        cap.canSubFrame = true;
        cap.hasGuideHead = false;
        cap.hasShutter = false;
        cap.canAbort = true;
        cap.canBin = true;
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

        cap.canSubFrame = true;
        cap.hasGuideHead = false;
        cap.hasShutter = false;
        cap.canAbort = true;
        cap.hasCooler = false;

        ret = InitQHYCCD(camhandle);
        if(ret != QHYCCD_SUCCESS)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error: Init Camera failed (%d)", ret);
            return false;
        }

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

        ret = IsQHYCCDControlAvailable(camhandle,CAM_BIN1X1MODE);
        if(ret == QHYCCD_SUCCESS)
        {
            camxbin = 1;
            camybin = 1;
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

    DEBUGF(INDI::Logger::DBG_ERROR, "Connecting to CCD failed (%s)",camid);

    return false;

}

bool QHYCCD::Disconnect()
{
  DEBUG(INDI::Logger::DBG_SESSION, "CCD is offline.");

  if (sim == false)
  {
      CloseQHYCCD(camhandle);
  }
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

        /* JM: We need GetQHYCCDErrorString(ret) to get the string description of the error, please implement this in the SDK */
        if (ret != QHYCCD_SUCCESS)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Error: GetQHYCCDChipInfo() (%d)", ret);
            return false;
        }
        camroix = 0;
        camroiy = 0;
        camroiwidth = imagew;
        camroiheight = imageh;
    }

    SetCCDParams(imagew,imageh,bpp,pixelw,pixelh);

    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    nbuf += 512;
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

int QHYCCD::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature- TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    TemperatureRequest = temperature;

    // Enable cooler
    setCooler(true);

    return 0;
}

bool QHYCCD::StartExposure(float duration)
{
  int ret = QHYCCD_ERROR;

  //AbortPrimaryFrame = false;

  if (duration < MINIMUM_CCD_EXPOSURE)
  {
    DEBUGF(INDI::Logger::DBG_WARNING, "Exposure shorter than minimum duration %g s requested. Setting exposure time to %g s.", duration, MINIMUM_CCD_EXPOSURE);
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
      return false;
  }

  // lzr: we need to call the following apis every single exposure,the usleep(200000) is important
  if (sim)
      ret = QHYCCD_SUCCESS;
  else
      ret = SetQHYCCDBinMode(camhandle,camxbin,camybin);
  if(ret != QHYCCD_SUCCESS)
  {
      DEBUGF(INDI::Logger::DBG_SESSION, "Set QHYCCD Bin mode failed (%d)", ret);
      return false;
  }

  if (sim)
      ret = QHYCCD_SUCCESS;
  else
     ret = SetQHYCCDResolution(camhandle,camroix,camroiy,camroiwidth,camroiheight);
  if(ret != QHYCCD_SUCCESS)
  {
      DEBUGF(INDI::Logger::DBG_SESSION, "Set QHYCCD ROI resolution failed (%d)", ret);
      return false;
  }
  
  usleep(300000); 

  if (sim)
      ret = QHYCCD_SUCCESS;
  else
      ret = ExpQHYCCDSingleFrame(camhandle);
  if(ret != QHYCCD_SUCCESS)
  {
      DEBUGF(INDI::Logger::DBG_SESSION, "Begin QHYCCD expose failed (%d)", ret);
      return false;
  }

  gettimeofday(&ExpStart, NULL);
  DEBUGF(INDI::Logger::DBG_DEBUG, "Taking a %g seconds frame...", ExposureRequest);

  InExposure = true;

  return true;
}

bool QHYCCD::AbortExposure()
{
    int ret;

    if (!InExposure || sim)
    {
        InExposure = false;
        return true;
    }

    ret = CancelQHYCCDExposingAndReadout(camhandle);
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

  camroix = x_1;
  camroiy = y_1;
  camroiwidth = x_2 - x_1;
  camroiheight = y_2 - y_1;

  DEBUGF(INDI::Logger::DBG_DEBUG, "The Final image area is (%d, %d), (%d, %d)\n", camroix, camroiy, camroiwidth, camroiheight);


  // Set UNBINNED coords
  PrimaryCCD.setFrame(x, y, w,  h);
  int nbuf;
  nbuf=(camroiwidth * camroiheight * PrimaryCCD.getBPP()/8);                 //  this is pixel count
  nbuf+=512;                      //  leave a little extra at the end
  PrimaryCCD.setFrameBufferSize(nbuf);

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

float QHYCCD::calcTimeLeft()
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
  } 
  else
  {

    int ret;
    int w,h,bpp,channels;

    ret = GetQHYCCDSingleFrame(camhandle,&w,&h,&bpp,&channels,image);
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
    timeleft = calcTimeLeft();

    if (timeleft < 1.0)
    {
      if (timeleft > 0.25)
      {
        //  a quarter of a second or more
        //  just set a tighter timer
        timerID = SetTimer(250);
      } 
      else
      {
        if (timeleft > 0.07)
        {
          //  use an even tighter timer
          timerID = SetTimer(50);
        } 
        else
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
    } 
    else
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
    int ret;
    if (sim)
        ret = QHYCCD_SUCCESS;
    else
        ret = ControlQHYCCDCFW(camhandle,position-1);
    if(ret == QHYCCD_SUCCESS)
    {
        CurrentFilter = position;
        SelectFilterDone(position);
        DEBUGF(INDI::Logger::DBG_DEBUG, "%s: Filter changed to %1", camid, position);
        return true;
    }
    else
        DEBUGF(INDI::Logger::DBG_ERROR, "Changing filter failed (%d)", ret);

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

          setCooler(CoolerS[0].s == ISS_ON);

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
    }
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

void QHYCCD::setCooler(bool enable)
{
    if (enable && temperatureFlag == false)
    {
        CoolerS[0].s = ISS_ON;
        CoolerS[1].s = ISS_OFF;
        CoolerSP.s   = IPS_OK;
        IDSetSwitch(&CoolerSP, NULL);

        CoolerNP.s = IPS_BUSY;
        IDSetNumber(&CoolerNP, NULL);
        DEBUG(INDI::Logger::DBG_SESSION, "Cooler on.");

        temperatureFlag = true;
        temperatureID = IEAddTimer(TEMPERATURE_POLL_MS, QHYCCD::updateTemperatureHelper, this);
    }
    else if (!enable && temperatureFlag == true)
    {
        temperatureFlag = false;
        IERmTimer(temperatureID);
        if (sim == false)
            SetQHYCCDParam(camhandle,CONTROL_MANULPWM,0);

        CoolerSP.s = IPS_IDLE;
        CoolerS[0].s = ISS_OFF;
        CoolerS[1].s = ISS_ON;
        IDSetSwitch(&CoolerSP, NULL);

        CoolerNP.s = IPS_IDLE;
        IDSetNumber(&CoolerNP, NULL);

        TemperatureNP.s = IPS_IDLE;
        IDSetNumber(&TemperatureNP, NULL);
        DEBUG(INDI::Logger::DBG_SESSION, "Cooler off.");
    }
}

void QHYCCD::updateTemperatureHelper(void *p)
{
    if (static_cast<QHYCCD*>(p)->isConnected())
        static_cast<QHYCCD*>(p)->updateTemperature();
}

void QHYCCD::updateTemperature()
{
    double ccdtemp,coolpower;

    if(temperatureFlag)
    {

        if (sim)
        {
            ccdtemp = TemperatureN[0].value;
            if (TemperatureN[0].value < TemperatureRequest)
                ccdtemp += TEMP_THRESHOLD;
            else if (TemperatureN[0].value > TemperatureRequest)
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
        {
            TemperatureN[0].value = TemperatureRequest;
            TemperatureNP.s = IPS_OK;
        }

        IDSetNumber(&TemperatureNP, NULL);
        IDSetNumber(&CoolerNP, NULL);

        temperatureID = IEAddTimer(TEMPERATURE_POLL_MS,QHYCCD::updateTemperatureHelper, this);
    }
}

bool QHYCCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    if (HasFilters)
    {
        IUSaveConfigNumber(fp, &FilterSlotNP);
        IUSaveConfigText(fp, FilterNameTP);
    }

    return true;
}

