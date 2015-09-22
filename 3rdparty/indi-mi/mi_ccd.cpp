/*
 QHY INDI Driver

 Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Copyright (C) 2014 Zhirong Li (lzr@qhyccd.com)
 Copyright (C) 2015 Peter Polakovic (peter.polakovic@cloudmakers.eu)

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

#include <libusb-1.0/libusb.h>

#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include "mi_ccd.h"

#include "config.h"

#define POLLMS                  1000        /* Polling time (ms) */
#define TEMP_THRESHOLD          0.2         /* Differential temperature threshold (C)*/
#define MINIMUM_CCD_EXPOSURE    0.001       /* 0.001 seconds minimum exposure */
#define MAX_DEVICES             4           /* Max device cameraCount */

#define MI_VID                      0x1347

//NB Disable for real driver
//#define USE_SIMULATION

static int cameraCount;
static MICCD *cameras[MAX_DEVICES];
static camera_t cameraHandle[MAX_DEVICES];

static struct {
  int pid;
  const char *name;
  } MI_PIDS[] = {
    { 0x400, "G2CCD" },
    { 0x401, "G2CCD-S" },
    { 0x402, "G2CCD2" },
    { 0x403, "G2/G3" },
    { 0x404, "G2/G3/G4" },
    { 0x405, "Gx CCD-I" },
    { 0x406, "Gx CCD-F" },
    { 0x410, "G1-0400" },
    { 0x411, "G1-0800" },
    { 0x412, "G1-0300" },
    { 0x413, "G1-2000" },
    { 0x414, "G1-1400" },
    { 0, NULL }
  };

libusb_context *ctx = NULL;

static void init()
{
  if (ctx == NULL) {
    int rc = libusb_init(&ctx);
    if (rc < 0)
    {
      fprintf(stderr, "init: libusb_init -> %s\n", rc < 0 ? libusb_error_name(rc) : "OK");
    }
  }
}

int miList(const char **names, int *pids, int maxCount)
{
  init();
  int count=0;
  libusb_device **usb_devices;
  struct libusb_device_descriptor descriptor;
  ssize_t total = libusb_get_device_list(ctx, &usb_devices);
  if (total < 0)
  {
    fprintf(stderr, "Can't get device list\n");
    return 0;
  }

  for (int i = 0; i < total && count < maxCount; i++)
  {
    libusb_device *device = usb_devices[i];
    if (!libusb_get_device_descriptor(device, &descriptor))
    {
      if (descriptor.idVendor == MI_VID)
      {
        int pid = descriptor.idProduct;
        for (int i = 0; MI_PIDS[i].pid; i++)
        {
          if (pid == MI_PIDS[i].pid)
          {
            fprintf(stderr, "miList: '%s' [0x%x, 0x%x] found\n", MI_PIDS[i].name, MI_VID, pid);
            pids[count]  = MI_PIDS[i].pid;
            names[count] = MI_PIDS[i].name;
            count++;
            libusb_ref_device(device);
            break;
          }
        }
      }
    }
  }
  libusb_free_device_list(usb_devices, 1);
  return count;
}

static void cleanup()
{
  for (int i = 0; i < cameraCount; i++)
  {
    delete cameras[i];
  }

}

void ISInit()
{
  static bool isInit = false;

  if (!isInit)
  {      

      int ret=0;
      cameraCount = 0;
      const char *cam_name[MAX_DEVICES];
      int cam_pid[MAX_DEVICES];

      #ifndef USE_SIMULATION
      cameraCount = miList(cam_name, &cam_pid[0], MAX_DEVICES);
      if(cameraCount == 0)
      {
          IDLog("Could not find any MI CCDs. Check power.\n");
          return;
      }
     #else
     cameraCount = 1;
     cam_pid[0]   = MI_PIDS[3].pid;
     cam_name[0]  = MI_PIDS[3].name;
     #endif

      for(int i = 0;i < cameraCount;i++)
      {
          // The product ID is unique to each camera it seems
          // But we can't get product ID (from cameraInfo) unless we an open first
          // and we can't do open, unless we have product ID or pass 0 to connect to first
          // camera. But then how can we know the product ID of the 2nd camera? MI has to answer that!
          // For now we are just connecting to first camera.
          //if ( (ret = miccd_open(pid, cameraHandle)) < 0)

          if ( (ret = miccd_open(0, &cameraHandle[i])) < 0)
          {
              fprintf(stderr, "Error connecting to MI camera (%#06x:%#06x): %s. Please unplug the CCD and cycle the power and restart INDI driver before trying again.", MI_VID, cam_pid[i], strerror(ret));
              return;
          }
          else
          {
              fprintf(stderr, "Connected to MI camera (%#06x:%#06x)", MI_VID, cam_pid[i]);
              cameras[i] = new MICCD(&cameraHandle[i]);
          }
      }

      atexit(cleanup);
      isInit = true;
  }
}

void ISGetProperties(const char *dev)
{
  ISInit();
  for (int i = 0; i < cameraCount; i++)
  {
    MICCD *camera = cameras[i];
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
    MICCD *camera = cameras[i];
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
    MICCD *camera = cameras[i];
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
    MICCD *camera = cameras[i];
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
      MICCD *camera = cameras[i];
      camera->ISSnoopDevice(root);
    }
}

MICCD::MICCD(camera_t *miHandle)
{
    int ret=0;
    cameraHandle = miHandle;

    if ( (ret = miccd_info(cameraHandle, &cameraInfo) < 0) )
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error getting MI camera info: %s.", strerror(ret));
        strncpy(this->name, "MI CCD", MAXINDIDEVICE);
        miccd_close(miHandle);
        cameraHandle->fd = -1;
    }
    else
    {
        snprintf(this->name, MAXINDINAME, "MI CCD %s", cameraInfo.description);
        DEBUGF(INDI::Logger::DBG_SESSION, "Detected %s", this->name);
    }

    HasFilters = false;
    isDark = false;

    setDeviceName(this->name);

    setVersion(INDI_MI_VERSION_MAJOR, INDI_MI_VERSION_MINOR);

    sim = false;
}

MICCD::~MICCD()
{
}

const char * MICCD::getDefaultName()
{
  return name;
}

bool MICCD::initProperties()
{
    INDI::CCD::initProperties();
    initFilterProperties(getDeviceName(), FILTER_TAB);

    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = 5;

    // CCD Regulation power
    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // CCD Fan
    IUFillSwitch(&FanS[0], "FAN_ON", "On", ISS_OFF);
    IUFillSwitch(&FanS[1], "FAN_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&FanSP, FanS, 2, getDeviceName(), "CCD_FAN", "Fan", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // CCD Gain
    IUFillNumber(&GainN[0], "GAIN", "Gain", "%3.0f", 0, 100, 1, 0);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Noise mode
    IUFillSwitch(&NoiseS[0], "NORMAL_NOISE", "Normal", ISS_ON);
    IUFillSwitch(&NoiseS[1], "LOW_NOISE", "Low", ISS_OFF);
    IUFillSwitch(&NoiseS[2], "ULTA_LOW_NOISE", "Ultra low", ISS_OFF);
    IUFillSwitchVector(&NoiseSP, NoiseS, 3, getDeviceName(), "CCD_NOISE", "Noise", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Set minimum exposure speed to 0.001 seconds
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", MINIMUM_CCD_EXPOSURE, 3600, 1, false);

    addAuxControls();

    setDriverInterface(getDriverInterface() | FILTER_INTERFACE);

    return true;
}

void MICCD::ISGetProperties(const char *dev)
{
  INDI::CCD::ISGetProperties(dev);

  if (isConnected())
  {
      if (HasCooler())
      {
          //defineSwitch(&CoolerSP);
          defineNumber(&CoolerNP);          
      }

      defineSwitch(&FanSP);
      defineSwitch(&NoiseSP);
      defineNumber(&GainNP);

      if(HasFilters)
      {
          //Define the Filter Slot and name properties
          defineNumber(&FilterSlotNP);
          if (FilterNameT != NULL)
              defineText(FilterNameTP);
      }

  }

}

bool MICCD::updateProperties()
{
  INDI::CCD::updateProperties();

  if (isConnected())
  {
      if (HasCooler())
      {
          //defineSwitch(&CoolerSP);
          defineNumber(&CoolerNP);

          temperatureID = IEAddTimer(POLLMS, MICCD::updateTemperatureHelper, this);
      }

      defineSwitch(&FanSP);
      defineSwitch(&NoiseSP);
      defineNumber(&GainNP);

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
          //deleteProperty(CoolerSP.name);
          deleteProperty(CoolerNP.name);

          RemoveTimer(temperatureID);
      }      

      deleteProperty(FanSP.name);
      deleteProperty(NoiseSP.name);
      deleteProperty(GainNP.name);

      if(HasFilters)
      {
          deleteProperty(FilterSlotNP.name);
          deleteProperty(FilterNameTP->name);
      }


    RemoveTimer(timerID);
  }

  return true;
}

bool MICCD::Connect()
{

    if (cameraHandle->fd == -1)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error retrieving camera info. Unplug and cycle power and restart INDI driver before you try again.");
        return false;
    }

    sim = isSimulation();

    CCDCapability cap;

    if (sim)
    {
        cap.canSubFrame = true;
        cap.hasGuideHead = false;
        cap.hasShutter = true;
        cap.canAbort = true;
        cap.canBin = true;
        cap.hasCooler = true;
        cap.hasST4Port = false;

        SetCCDCapability(&cap);

        DEBUGF(INDI::Logger::DBG_SESSION, "Connected to %s", name);

        HasFilters = true;

        return true;
    }

   DEBUGF(INDI::Logger::DBG_SESSION, "Connected to %s. Chip: %s. HWRevision: %ld. Model: %d. Description: %s", name, cameraInfo.chip, cameraInfo.hwrevision, cameraHandle->model, cameraInfo.description);

   cap.canSubFrame = true;
   cap.hasGuideHead = false;
   cap.canAbort = true;

   if (cameraHandle->model > G12000)
   {
        cap.canBin = true;
        cap.hasShutter = true;
        cap.hasCooler = true;

        // Hackish way to find if there is filter wheel support
        if (miccd_filter (cameraHandle, 0) == 0)
            HasFilters = true;
        else
            HasFilters = false;
   }
   else
   {
       cap.canBin = false;
       cap.hasShutter = false;
       cap.hasCooler = false;
       HasFilters = false;
   }

   cap.hasST4Port = false;

   SetCCDCapability(&cap);

    return true;
}

bool MICCD::Disconnect()
{
  DEBUG(INDI::Logger::DBG_SESSION, "CCD is offline.");

  if (sim == false)
  {
      miccd_close(cameraHandle);
  }
  return true;
}

bool MICCD::setupParams()
{
    int ret = 0;

    if (sim)
        SetCCDParams(4032,2688,16,9,9);
    else
        SetCCDParams(cameraInfo.w, cameraInfo.h, 16, cameraInfo.pw/1000.0, cameraInfo.ph/1000.0);


    int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    nbuf += 512;
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Get Gain, does it change?
    uint16_t gain=0;
    if ( (ret = miccd_gain(cameraHandle, &gain)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "miccd_gain error: %s", strerror(ret));
        GainNP.s = IPS_ALERT;
        IDSetNumber(&GainNP, NULL);
        return false;
    }
    else
    {
        GainN[0].value = gain;
        GainNP.s = IPS_OK;
        IDSetNumber(&GainNP, NULL);
    }

    return true;
}

int MICCD::SetTemperature(double temperature)
{
    int ret = 0;

    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature- TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    TemperatureRequest = temperature;        


    if (sim == false && (ret = miccd_set_cooltemp(cameraHandle, temperature)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Setting temperature failed: %s.", strerror(ret));
        return -1;
    }

    return 0;
}

bool MICCD::StartExposure(float duration)
{
  int ret = 0;
  isDark  = false;

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
  else if(imageFrameType == CCDChip::DARK_FRAME)
  {
      isDark = true;
  }  

  if (sim == false)
  {
      if ( (ret = miccd_clear (cameraHandle)) < 0)
      {
          DEBUGF(INDI::Logger::DBG_ERROR, "miccd_clear error: %s.", strerror(ret));
          return false;
      }

      // Older G1 models
      if (cameraHandle->model <= G12000)
      {
          uint8_t lowNoiseMode = (IUFindOnSwitchIndex(&NoiseSP) > 0) ? 1 : 0;

          if ( (ret = miccd_g1_mode (cameraHandle, 1, lowNoiseMode)) < 0)
          {
              DEBUGF(INDI::Logger::DBG_ERROR, "miccd_g1_mode error: %s.", strerror(ret));
              return false;
          }

          // No need, we call same function with -1 later once timer is up
          /*if ( (ret = miccd_start_exposure (&camera, PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), duration)) < 0)
          {
              DEBUGF(INDI::Logger::DBG_ERROR, "miccd_start_exposure: %s.", strerror(ret));
              return false;
          }*/
      }
      //G2/G3/G4 models
      else
      {
          if ( (ret = miccd_hclear (cameraHandle)) < 0)
          {
              DEBUGF(INDI::Logger::DBG_ERROR, "miccd_hclear error: %s.", strerror(ret));
              return false;
          }

          if (isDark == false)
          {
              if ( (ret = miccd_open_shutter (cameraHandle)) < 0 )
              {
                  DEBUGF(INDI::Logger::DBG_ERROR, "miccd_open_shutter: %s.", strerror(ret));
                  return false;
              }
          }
      }
  }

  ExposureRequest = duration;
  PrimaryCCD.setExposureDuration(duration);

  gettimeofday(&ExpStart, NULL);
  DEBUGF(INDI::Logger::DBG_DEBUG, "Taking a %g seconds frame...", ExposureRequest);

  InExposure = true;

  return true;
}

bool MICCD::AbortExposure()
{
    int ret=0;

    if (!InExposure || sim)
    {
        InExposure = false;
        DEBUG(INDI::Logger::DBG_SESSION, "Exposure aborted.");
        return true;
    }

    // Older G1 models
    if (cameraHandle->model <= G12000)
    {
        if ( (ret = miccd_abort_exposure (cameraHandle)) < 0 )
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "miccd_abort_exposure: %s.", strerror(ret));
            return false;
        }
    }
    // G2/G3/G4 models
    else
    {
        if (isDark == false)
        {
            if ( (ret = miccd_close_shutter (cameraHandle)) < 0 )
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "miccd_close_shutter: %s.", strerror(ret));
                return false;
            }
        }
    }

    InExposure = false;
    DEBUG(INDI::Logger::DBG_SESSION, "Exposure aborted.");
    return true;
}

bool MICCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    /* Add the X and Y offsets */
    long x_1 = x;
    long y_1 = y;

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

    DEBUGF(INDI::Logger::DBG_DEBUG, "The Final image area is (%ld, %ld), (%ld, %ld)\n", x_1, y_1, x_2, y_2);

    int imageWidth  = x_2 - x_1;
    int imageHeight = y_2 - y_1;

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w,  h);
    int nbuf;
    nbuf=(imageWidth*imageHeight * PrimaryCCD.getBPP()/8);                 //  this is pixel count
    nbuf+=512;                      //  leave a little extra at the end
    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

bool MICCD::UpdateCCDBin(int hor, int ver)
{
    if (hor < 1 || hor > 4 || ver < 1 || ver > 4)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Binning (%dx%d) are out of range. Range from 1x1 to 4x4", hor, ver);
        return false;
    }

   PrimaryCCD.setBin(hor,ver);
   return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());

    return false;
}

float MICCD::calcTimeLeft()
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
int MICCD::grabImage()
{
  unsigned char * image = (unsigned char *) PrimaryCCD.getFrameBuffer();

  if (sim)
  {
      int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();
      int width  = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();

      uint16_t *buffer = (uint16_t *) image;

    for (int i = 0; i < height; i++)
      for (int j = 0; j < width; j++)
        buffer[i * width + j] = rand() % UINT16_MAX;
  } 
  else
  {
    int ret=0;

    // Older G1 models
    if (cameraHandle->model <= G12000)
    {
        if ( (ret = miccd_start_exposure (cameraHandle, PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), -1) ) < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "miccd_start_exposure error: %s", strerror(ret));
            return false;
        }

        if ( (ret = miccd_read_data (cameraHandle, PrimaryCCD.getFrameBufferSize(), PrimaryCCD.getFrameBuffer(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH()) < 0) )
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "miccd_read_data error: %s", strerror(ret));
            return false;
        }
    }
    // G2/G2/G4 models
    else
    {
        if (isDark == false)
        {
            if ( (ret = miccd_close_shutter (cameraHandle)) < 0 )
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "miccd_close_shutter: %s.", strerror(ret));
                return false;
            }
        }

        if ( (ret = miccd_shift_to0 (cameraHandle)) < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "miccd_shift_to0: %s.", strerror(ret));
            return false;
        }

        uint8_t mode = IUFindOnSwitchIndex(&NoiseSP);

        if ( (ret = miccd_mode (cameraHandle, mode)) < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "miccd_mode: %s.", strerror(ret));
            return false;
        }

        if ( (ret = miccd_read_frame (cameraHandle, PrimaryCCD.getBinX(), PrimaryCCD.getBinY(), PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), PrimaryCCD.getFrameBuffer())) < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "miccd_read_frame: %s.", strerror(ret));
            return false;
        }
    }
  }

  DEBUG(INDI::Logger::DBG_DEBUG, "Download complete.");
  if (ExposureRequest > POLLMS * 5)
      DEBUG(INDI::Logger::DBG_SESSION, "Download complete.");

  ExposureComplete(&PrimaryCCD);

  return 0;
}

void MICCD::TimerHit()
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
          /* We're done exposing */
          DEBUG(INDI::Logger::DBG_DEBUG, "Exposure done, downloading image...");
          // Don't spam the session log unless it is a long exposure > 5 seconds
          if (ExposureRequest > POLLMS * 5)
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

bool MICCD::SelectFilter(int position)
{
    int ret = 0;

    if (sim == false && (ret = miccd_filter(cameraHandle, position)) < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "miccd_filter error: %s.", strerror(ret));
        return false;
    }

    CurrentFilter = position;
    SelectFilterDone(position);
    DEBUGF(INDI::Logger::DBG_DEBUG, "Filter changed to %d", position);
    return true;
}

bool MICCD::SetFilterNames()
{
    // Cannot save it in hardware, so let's just save it in the config file to be loaded later
    saveConfig();
    return true;
}

bool MICCD::GetFilterNames(const char* groupName)
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

int MICCD::QueryFilter()
{
    return CurrentFilter;
}

bool MICCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(!strcmp(NoiseSP.name, name))
        {
          IUUpdateSwitch(&NoiseSP, states, names, n);

          if (cameraHandle->model <= G12000 && NoiseS[2].s == ISS_ON)
          {
              IUResetSwitch(&NoiseSP);
              NoiseS[0].s = ISS_ON;
              NoiseSP.s = IPS_ALERT;
              DEBUG(INDI::Logger::DBG_ERROR, "Ultra low noise mode not supported in this camera model.");
          }
          else
              NoiseSP.s = IPS_OK;

          IDSetSwitch(&NoiseSP, NULL);

          return true;
        }


        if(!strcmp(FanSP.name, name))
        {
            int ret = 0;

            IUUpdateSwitch(&FanSP, states, names, n);

            if (sim == false && (ret = miccd_fan(cameraHandle, (FanS[0].s == ISS_ON) ? 1 : 0)) < 0)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "miccd_fan error: %s.", strerror(ret));
                IUResetSwitch(&FanSP);
                FanSP.s = IPS_ALERT;
            }
            else
            {
                IUResetSwitch(&FanSP);
                FanSP.s = IPS_OK;
            }

            IDSetSwitch(&FanSP, NULL);
            return true;
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev,name,states,names,n);
}


bool MICCD::ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(!strcmp(name,FilterNameTP->name))
        {
            processFilterName(dev, texts, names, n);
            return true;
        }

    }

    return INDI::CCD::ISNewText(dev,name,texts,names,n);
}

bool MICCD::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{    
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name,FilterSlotNP.name))
        {
            processFilterSlot(getDeviceName(), values, names);
            return true;
        }       
    }

    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

void MICCD::updateTemperatureHelper(void *p)
{
    if (static_cast<MICCD*>(p)->isConnected())
        static_cast<MICCD*>(p)->updateTemperature();
}

void MICCD::updateTemperature()
{
    float ccdtemp=0;
    uint16_t ccdpower=0;
    int ret=0;

    if (sim)
    {
        ccdtemp = TemperatureN[0].value;
        if (TemperatureN[0].value < TemperatureRequest)
            ccdtemp += TEMP_THRESHOLD;
        else if (TemperatureN[0].value > TemperatureRequest)
            ccdtemp -= TEMP_THRESHOLD;

        ccdpower = 30;
    }
    else
    {
        if ( (ret = miccd_chip_temperature(cameraHandle, &ccdtemp) ) < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "miccd_chip_temperature error: %s.", strerror(ret));
            return;
        }

        if ( (ret = miccd_power_voltage(cameraHandle, &ccdpower)) < 0)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "miccd_chip_temperature error: %s.", strerror(ret));
            return;

        }
    }    

    TemperatureN[0].value = ccdtemp;
    CoolerN[0].value = ccdpower/1000.0;

    if (TemperatureNP.s == IPS_BUSY && fabs(TemperatureN[0].value - TemperatureRequest) <= TEMP_THRESHOLD)
    {
        TemperatureN[0].value = TemperatureRequest;
        TemperatureNP.s = IPS_OK;
    }

    IDSetNumber(&TemperatureNP, NULL);
    IDSetNumber(&CoolerNP, NULL);
    temperatureID = IEAddTimer(POLLMS,MICCD::updateTemperatureHelper, this);

}

bool MICCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    if (HasFilters)
    {
        IUSaveConfigNumber(fp, &FilterSlotNP);
        IUSaveConfigText(fp, FilterNameTP);
    }

    IUSaveConfigSwitch(fp, &NoiseSP);

    return true;
}

