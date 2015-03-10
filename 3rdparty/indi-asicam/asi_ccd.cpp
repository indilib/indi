/*
 ASI CCD Driver

 Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <zlib.h>

#include "indidevapi.h"
#include "eventloop.h"

#include "asi_ccd.h"

#define POLLMS                  250		/* Polling time (ms) */
#define TEMPERATURE_UPDATE_FREQ 4       /* Update every 4 POLLMS ~ 1 second */
#define TEMP_THRESHOLD          .25		/* Differential temperature threshold (C)*/
#define MAX_DEVICES             4       /* Max device cameraCount */
#define MAX_EXP_RETRIES         3

#define CONTROL_TAB     "Controls"

//#define USE_SIMULATION

static int iNumofConnectCameras;
static ASI_CAMERA_INFO *pASICameraInfo;
static ASICCD *cameras[MAX_DEVICES];

pthread_cond_t      cv  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     condMutex     = PTHREAD_MUTEX_INITIALIZER;


static void cleanup()
{
  for (int i = 0; i < iNumofConnectCameras; i++)
  {
    delete cameras[i];
  }

  free(pASICameraInfo);
}

void ISInit()
{
  static bool isInit = false;
  if (!isInit)
  {
      pASICameraInfo = NULL;
      iNumofConnectCameras=0;

      #ifdef USE_SIMULATION
      iNumofConnectCameras = 2;
      ppASICameraInfo = (ASI_CAMERA_INFO *)malloc(sizeof(ASI_CAMERA_INFO *)*iNumofConnectCameras);
      strncpy(ppASICameraInfo[0]->Name, "ASI CCD #1", 64);
      strncpy(ppASICameraInfo[1]->Name, "ASI CCD #2", 64);
      cameras[0] = new ASICCD(ppASICameraInfo[0]);
      cameras[1] = new ASICCD(ppASICameraInfo[1]);
      #else
      iNumofConnectCameras = ASIGetNumOfConnectedCameras();
      if (iNumofConnectCameras > MAX_DEVICES)
          iNumofConnectCameras = MAX_DEVICES;
      pASICameraInfo = (ASI_CAMERA_INFO *)malloc(sizeof(ASI_CAMERA_INFO)*iNumofConnectCameras);
      if (iNumofConnectCameras <= 0)
          //Try sending IDMessage as well?
          IDLog("No ASI Cameras detected. Power on?");
      else
      {
          for(int i = 0; i < iNumofConnectCameras; i++)
          {
              ASIGetCameraProperty(pASICameraInfo+i, i);
              cameras[i] = new ASICCD(pASICameraInfo+i);
          }
      }
    #endif

    atexit(cleanup);
    isInit = true;
  }
}

void ISGetProperties(const char *dev)
{
  ISInit();
  for (int i = 0; i < iNumofConnectCameras; i++)
  {
    ASICCD *camera = cameras[i];
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
  for (int i = 0; i < iNumofConnectCameras; i++)
  {
    ASICCD *camera = cameras[i];
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
  for (int i = 0; i < iNumofConnectCameras; i++)
  {
    ASICCD *camera = cameras[i];
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
  for (int i = 0; i < iNumofConnectCameras; i++)
  {
    ASICCD *camera = cameras[i];
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

    for (int i = 0; i < iNumofConnectCameras; i++)
    {
      ASICCD *camera = cameras[i];
      camera->ISSnoopDevice(root);
    }
}

ASICCD::ASICCD(ASI_CAMERA_INFO *camInfo)
{
  ControlN = NULL;
  ControlS  = NULL;
  pControlCaps = NULL;
  m_camInfo = camInfo;

  exposureRetries = 0;
  streamPredicate = 0;
  terminateThread = false;

  InWEPulse = InNSPulse = false;
  WEPulseRequest = NSPulseRequest =0;
  WEtimerID = NStimerID = 0;
  WEDir = NSDir = ASI_GUIDE_NORTH;

  TemperatureUpdateCounter = 0;

  snprintf(this->name, MAXINDIDEVICE, "%s", m_camInfo->Name);
  setDeviceName(this->name);
}

ASICCD::~ASICCD()
{

}

const char * ASICCD::getDefaultName()
{
  return name;
}

bool ASICCD::initProperties()
{
  INDI::CCD::initProperties();

  IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 1., .2, 0.0);
  IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

  IUFillNumberVector(&ControlNP, NULL, 0, getDeviceName(), "CCD_CONTROLS", "Controls", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

  IUFillSwitchVector(&ControlSP, NULL, 0, getDeviceName(), "CCD_CONTROLS_MODE", "Set Auto", CONTROL_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);

  IUFillSwitchVector(&VideoFormatSP, NULL, 0, getDeviceName(), "CCD_VIDEO_FORMAT", "Format", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

  IUFillSwitch(&StreamS[0], "STREAM_ON", "Stream On", ISS_OFF);
  IUFillSwitch(&StreamS[1], "STREAM_OFF", "Stream Off", ISS_ON);
  IUFillSwitchVector(&StreamSP, StreamS, NARRAY(StreamS), getDeviceName(), "CCD_VIDEO_STREAM", "Video Stream", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  IUSaveText(&BayerT[2], getBayerString());

  int maxBin=1;
  for (int i=0; i < 16; i++)
  {
      if (m_camInfo->SupportedBins[i] != 0)
          maxBin = m_camInfo->SupportedBins[i];
      else
          break;
  }

  PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, maxBin, 1, false);
  PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, maxBin, 1, false);

  asiCap.canAbort = true;
  asiCap.canBin = (maxBin > 1);
  asiCap.canSubFrame = true;
  asiCap.hasCooler = (m_camInfo->IsCoolerCam);
  asiCap.hasGuideHead = false;
  asiCap.hasShutter = (m_camInfo->MechanicalShutter);
  asiCap.hasST4Port = (m_camInfo->ST4Port);
  asiCap.hasBayer = (m_camInfo->IsColorCam);

  SetCCDCapability(&asiCap);

  addAuxControls();

  return true;
}

void ASICCD::ISGetProperties(const char *dev)
{
  INDI::CCD::ISGetProperties(dev);
}

bool ASICCD::updateProperties()
{
  INDI::CCD::updateProperties();

  if (isConnected())
  {
    imageBP=getBLOB("CCD1");
    imageB=imageBP->bp;

    if (HasCooler())
        defineNumber(&CoolerNP);

    defineSwitch(&StreamSP);

    // Let's get parameters now from CCD
    setupParams();

    if (ControlNP.nnp > 0)
     defineNumber(&ControlNP);

    if (ControlSP.nsp > 0)
        defineSwitch(&ControlSP);

    if (VideoFormatSP.nsp > 0)
        defineSwitch(&VideoFormatSP);

    SetTimer(POLLMS);
  } else
  {

    if (HasCooler())
        deleteProperty(CoolerNP.name);

    deleteProperty(StreamSP.name);

    if (ControlNP.nnp > 0)
        deleteProperty(ControlNP.name);

    if (ControlSP.nsp > 0)
        deleteProperty(ControlSP.name);

    if (VideoFormatSP.nsp > 0)
        deleteProperty(VideoFormatSP.name);
  }

  return true;
}

bool ASICCD::Connect()
{

  DEBUGF(INDI::Logger::DBG_DEBUG,  "Attempting to open %s...", name);

  sim = isSimulation();

  ASI_ERROR_CODE errCode = ASI_SUCCESS;

  if (sim == false)
        errCode = ASIOpenCamera(m_camInfo->CameraID);

  if (errCode != ASI_SUCCESS)
  {
      DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to the CCD (%d)", errCode);
      return false;
  }

  TemperatureUpdateCounter = 0;

  pthread_create( &primary_thread, NULL, &streamVideoHelper, this);

  /* Success! */
  DEBUG(INDI::Logger::DBG_SESSION,  "CCD is online. Retrieving basic data.");

  return true;
}

bool ASICCD::Disconnect()
{
  if (sim == false)
    ASICloseCamera(m_camInfo->CameraID);

  pthread_mutex_lock(&condMutex);
  streamPredicate=1;
  terminateThread=true;
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&condMutex);

  DEBUG(INDI::Logger::DBG_SESSION,  "CCD is offline.");

  return true;
}

bool ASICCD::setupParams()
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;
    int piNumberOfControls = 0;

    errCode = ASIGetNumOfControls(m_camInfo->CameraID, &piNumberOfControls);

    if (errCode != ASI_SUCCESS)
        DEBUGF(INDI::Logger::DBG_DEBUG, "ASIGetNumOfControls error (%d)", errCode);

  if (piNumberOfControls > 0)
  {
      if (ControlNP.nnp > 0)
          free(ControlN);

      if (ControlSP.nsp > 0)
          free(ControlS);

      createControls(piNumberOfControls);
  }

  // Get Image Format
  int w=0, h=0, bin=0;
  ASI_IMG_TYPE imgType;
  ASIGetROIFormat(m_camInfo->CameraID, &w, &h, &bin, &imgType);

  // Get video format and bit depth
  int bit_depth = 8;

  switch (imgType)
  {
    case ASI_IMG_RAW16:
      bit_depth = 16;

    default:
      break;
  }

  if (VideoFormatSP.nsp > 0)
      free(VideoFormatS);

  VideoFormatS = NULL;
  int nVideoFormats = 0;

  for (int i=0; i < 8; i++)
  {
        if (m_camInfo->SupportedVideoFormat[i] == ASI_IMG_END)
            break;

        // 16 bit is not working on arm, skipping it completely for now
        #ifdef LOW_USB_BANDWIDTH
        if (m_camInfo->SupportedVideoFormat[i] == ASI_IMG_RAW16)
            continue;
        #endif

        nVideoFormats++;
        VideoFormatS = VideoFormatS == NULL ? (ISwitch *) malloc(sizeof(ISwitch)) : (ISwitch *) realloc(VideoFormatS, nVideoFormats * sizeof(ISwitch));

        ISwitch *oneVF = VideoFormatS + (nVideoFormats-1);

        switch (m_camInfo->SupportedVideoFormat[i])
        {
            case ASI_IMG_RAW8:
                IUFillSwitch(oneVF, "ASI_IMG_RAW8", "Raw 8 bit", (imgType == ASI_IMG_RAW8) ? ISS_ON : ISS_OFF);
                DEBUG(INDI::Logger::DBG_DEBUG, "Supported Video Format: ASI_IMG_RAW8");
                break;

            case ASI_IMG_RGB24:
                IUFillSwitch(oneVF, "ASI_IMG_RGB24", "RGB 24", (imgType == ASI_IMG_RGB24) ? ISS_ON : ISS_OFF);
                DEBUG(INDI::Logger::DBG_DEBUG, "Supported Video Format: ASI_IMG_RGB24");
                break;

            case ASI_IMG_RAW16:
                IUFillSwitch(oneVF, "ASI_IMG_RAW16", "Raw 16 bit", (imgType == ASI_IMG_RAW16) ? ISS_ON : ISS_OFF);
                DEBUG(INDI::Logger::DBG_DEBUG, "Supported Video Format: ASI_IMG_RAW16");
            break;

            case ASI_IMG_Y8:
                IUFillSwitch(oneVF, "ASI_IMG_Y8", "Luma", (imgType == ASI_IMG_Y8) ? ISS_ON : ISS_OFF);
                DEBUG(INDI::Logger::DBG_DEBUG, "Supported Video Format: ASI_IMG_Y8");
                break;

            default:
                DEBUGF(INDI::Logger::DBG_DEBUG, "Unknown video format (%d)", m_camInfo->SupportedVideoFormat[i]);
                break;
        }

        oneVF->aux = (void *) &m_camInfo->SupportedVideoFormat[i];
  }

  VideoFormatSP.nsp = nVideoFormats;
  VideoFormatSP.sp  = VideoFormatS;

  float x_pixel_size, y_pixel_size; 
  int x_1, y_1, x_2, y_2;

  x_pixel_size = m_camInfo->PixelSize;
  y_pixel_size = m_camInfo->PixelSize;

   x_1 = y_1 = 0;
   x_2 = m_camInfo->MaxWidth;
   y_2 = m_camInfo->MaxHeight;

  SetCCDParams(x_2 - x_1, y_2 - y_1, bit_depth, x_pixel_size, y_pixel_size);

  // Let's calculate required buffer
  int nbuf;
  nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;    //  this is pixel cameraCount
  nbuf += 512;    //  leave a little extra at the end
  PrimaryCCD.setFrameBufferSize(nbuf);

  if (HasCooler())
  {
      long pValue = 0;
      ASI_BOOL isAuto= ASI_FALSE;

      if ( (errCode = ASIGetControlValue(m_camInfo->CameraID, ASI_TEMPERATURE, &pValue, &isAuto)) != ASI_SUCCESS)
          DEBUGF(INDI::Logger::DBG_DEBUG, "ASIGetControlValue temperature error (%d)", errCode);

      TemperatureN[0].value = pValue / 10.0;

      DEBUGF(INDI::Logger::DBG_SESSION,  "The CCD Temperature is %f", TemperatureN[0].value);
      IDSetNumber(&TemperatureNP, NULL);
  }

  ASISetROIFormat(m_camInfo->CameraID, m_camInfo->MaxWidth, m_camInfo->MaxHeight, 1, getImageType());

  return true;

}


bool ASICCD::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

     if(!strcmp(dev,getDeviceName()))
     {
         if (!strcmp(name, ControlNP.name))
         {
            double oldValues[ControlNP.nnp];
            for (int i=0; i < ControlNP.nnp; i++)
                oldValues[i] = ControlN[i].value;

            IUUpdateNumber(&ControlNP, values, names, n);

            for (int i=0; i < ControlNP.nnp; i++)
            {
                ASI_BOOL nAuto = *((ASI_BOOL *) ControlN[i].aux1);
                ASI_CONTROL_TYPE nType = *((ASI_CONTROL_TYPE *) ControlN[i].aux0);

                // If value didn't change or if USB bandwidth control is to change, then only continue if ExposureRequest < 250 ms
                if (ControlN[i].value == oldValues[i] || (nType == ASI_BANDWIDTHOVERLOAD && ExposureRequest > 0.25))
                    continue;

                if ( (errCode = ASISetControlValue(m_camInfo->CameraID, nType, ControlN[i].value, ASI_FALSE)) != ASI_SUCCESS)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "ASISetControlValue (%s=%g) error (%d)", ControlN[i].name, ControlN[i].value, errCode);
                    ControlNP.s = IPS_ALERT;
                    for (int i=0; i < ControlNP.nnp; i++)
                        ControlN[i].value = oldValues[i];
                    IDSetNumber(&ControlNP, NULL);
                    return false;
                }

                // If it was set to nAuto value to turn it off
                if (nAuto)
                {
                    for (int j=0; j < ControlSP.nsp; j++)
                    {
                        ASI_CONTROL_TYPE swType = *((ASI_CONTROL_TYPE *) ControlS[j].aux);

                        if (swType == nType)
                        {
                            ControlS[j].s = ISS_OFF;
                            break;
                        }
                    }

                    IDSetSwitch(&ControlSP, NULL);
                 }
            }

            ControlNP.s = IPS_OK;
            IDSetNumber(&ControlNP, NULL);
            return true;
         }

     }

    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

bool ASICCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    if(!strcmp(dev,getDeviceName()))
    {
        if (!strcmp(name, StreamSP.name))
        {
            IUUpdateSwitch(&StreamSP, states, names, n);

            if (StreamS[0].s == ISS_ON)
            {
                ASI_IMG_TYPE type = getImageType();

                if (type != ASI_IMG_Y8)
                {
                    IUResetSwitch(&VideoFormatSP);
                    ISwitch *vf = IUFindSwitch(&VideoFormatSP,"ASI_IMG_Y8");
                    if (vf)
                    {
                        vf->s = ISS_ON;
                        DEBUG(INDI::Logger::DBG_DEBUG, "Switching to Luma video format.");
                        PrimaryCCD.setBPP(8);
                        UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
                        IDSetSwitch(&VideoFormatSP, NULL);
                    }
                    else
                    {
                        DEBUG(INDI::Logger::DBG_ERROR, "No Luma video format found, cannot start stream.");
                        IUResetSwitch(&StreamSP);
                        StreamSP.s = IPS_ALERT;
                        IDSetSwitch(&StreamSP, NULL);
                        return true;
                    }
                }
                StreamSP.s = IPS_BUSY;
                ASIStartVideoCapture(m_camInfo->CameraID);
                pthread_mutex_lock(&condMutex);
                streamPredicate = 1;
                pthread_mutex_unlock(&condMutex);
                pthread_cond_signal(&cv);
            }
            else
            {
                StreamSP.s = IPS_IDLE;
                pthread_mutex_lock(&condMutex);
                streamPredicate = 0;
                pthread_mutex_unlock(&condMutex);
                pthread_cond_signal(&cv);
                ASIStopVideoCapture(m_camInfo->CameraID);
            }

            IDSetSwitch(&StreamSP, NULL);
            return true;
        }

        if (!strcmp(name, ControlSP.name))
        {
           IUUpdateSwitch(&ControlSP, states, names, n);

           for (int i=0; i < ControlSP.nsp; i++)
           {
               ASI_CONTROL_TYPE swType = *((ASI_CONTROL_TYPE *) ControlS[i].aux);
               ASI_BOOL swAuto = (ControlS[i].s == ISS_ON) ? ASI_TRUE : ASI_FALSE;

               for (int j=0; j < ControlNP.nnp; j++)
               {
                   ASI_CONTROL_TYPE nType = *((ASI_CONTROL_TYPE *) ControlN[j].aux0);

                   if (swType == nType)
                   {
                       if ( (errCode = ASISetControlValue(m_camInfo->CameraID, nType, ControlN[j].value, swAuto )) != ASI_SUCCESS)
                       {
                           DEBUGF(INDI::Logger::DBG_ERROR, "ASISetControlValue (%s=%g) error (%d)", ControlN[j].name, ControlN[j].value, errCode);
                           ControlNP.s = IPS_ALERT;
                           ControlSP.s = IPS_ALERT;
                           IDSetNumber(&ControlNP, NULL);
                           IDSetSwitch(&ControlSP, NULL);
                           return false;
                       }

                       *((ASI_BOOL *) ControlN[j].aux1) = swAuto;

                   }
               }
           }

           ControlSP.s = IPS_OK;
           IDSetSwitch(&ControlSP, NULL);
           return true;

        }

        if (!strcmp(name, VideoFormatSP.name))
        {
            int prevFormat = IUFindOnSwitchIndex(&VideoFormatSP);

            IUUpdateSwitch(&VideoFormatSP, states, names, n);

            ASI_IMG_TYPE type = getImageType();

            if (StreamSP.s == IPS_BUSY && type != ASI_IMG_Y8)
            {
                IUResetSwitch(&VideoFormatSP);
                VideoFormatS[prevFormat].s = ISS_ON;
                VideoFormatSP.s = IPS_IDLE;
                DEBUG(INDI::Logger::DBG_WARNING, "Only Luma format is supported for video streaming.");
                IDSetSwitch(&VideoFormatSP, NULL);
                return true;
            }

            switch (type)
            {
                case ASI_IMG_RAW16:
                    PrimaryCCD.setBPP(16);
                    break;

                default:
                   PrimaryCCD.setBPP(8);
                    break;
            }

            UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());

            VideoFormatSP.s = IPS_OK;
            IDSetSwitch(&VideoFormatSP, NULL);
            return true;
        }

    }

   return INDI::CCD::ISNewSwitch(dev,name,states,names,n);
}

int ASICCD::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature - TemperatureN[0].value) < TEMP_THRESHOLD)
        return 1;

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    TemperatureRequest = temperature;
    DEBUGF(INDI::Logger::DBG_SESSION, "Setting CCD temperature to %+06.2f C", temperature);
    return 0;
}



bool ASICCD::StartExposure(float duration)
{
  ASI_ERROR_CODE errCode= ASI_SUCCESS;

  if (duration < minDuration)
  {
    DEBUGF(INDI::Logger::DBG_WARNING, "Exposure shorter than minimum duration %g s requested. Setting exposure time to %g s.", duration, minDuration);
    duration = minDuration;
  }

  if (PrimaryCCD.getFrameType() == CCDChip::BIAS_FRAME)
  {
    duration = minDuration;
    DEBUGF(INDI::Logger::DBG_SESSION, "Bias Frame (s) : %g", minDuration);
  }

  PrimaryCCD.setExposureDuration(duration);
  ExposureRequest = duration;

  if ( (errCode = ASIStartExposure(m_camInfo->CameraID, duration * 1000, (PrimaryCCD.getFrameType() == CCDChip::DARK_FRAME) ? ASI_TRUE : ASI_FALSE )) != ASI_SUCCESS)
  {
      DEBUGF(INDI::Logger::DBG_ERROR, "ASIStartExposure error (%d)", errCode);
      return false;
  }

  gettimeofday(&ExpStart, NULL);
  DEBUGF(INDI::Logger::DBG_SESSION, "Taking a %g seconds frame...", ExposureRequest);

  InExposure = true;

  updateControls();

  return true;
}

bool ASICCD::AbortExposure()
{
  ASIStopExposure(m_camInfo->CameraID);
  InExposure = false;
  return true;
}

bool ASICCD::UpdateCCDFrame(int x, int y, int w, int h)
{
  ASI_ERROR_CODE errCode = ASI_SUCCESS;

  /* Add the X and Y offsets */
  long x_1 = x / PrimaryCCD.getBinX();
  long y_1 = y / PrimaryCCD.getBinY();

  long bin_width = w / PrimaryCCD.getBinX();
  long bin_height = h / PrimaryCCD.getBinY();

  if (bin_width > PrimaryCCD.getXRes() / PrimaryCCD.getBinX())
  {
    DEBUGF(INDI::Logger::DBG_SESSION,  "Error: invalid width requested %d", w);
    return false;
  } else if (bin_height > PrimaryCCD.getYRes() / PrimaryCCD.getBinY())
  {
    DEBUGF(INDI::Logger::DBG_SESSION,  "Error: invalid height request %d", h);
    return false;
  }

  if ( (errCode = ASISetStartPos(m_camInfo->CameraID, x_1, y_1)) != ASI_SUCCESS)
  {
      DEBUGF(INDI::Logger::DBG_ERROR, "ASISetStartPos (%d,%d) error (%d)", x_1, y_1, errCode);
      return false;
  }

  if ( (errCode = ASISetROIFormat(m_camInfo->CameraID, bin_width, bin_height, PrimaryCCD.getBinX(), getImageType())) != ASI_SUCCESS)
  {
      DEBUGF(INDI::Logger::DBG_ERROR, "ASISetROIFormat (%dx%d @ %d) error (%d)", bin_width, bin_height, PrimaryCCD.getBinX(), errCode);
      return false;
  }

  // Set UNBINNED coords
  PrimaryCCD.setFrame(x, y, w, h);

  int nChannels = 1;

  if (getImageType() == ASI_IMG_RGB24)
      nChannels = 3;

  int nbuf;
  nbuf = (bin_width * bin_height * PrimaryCCD.getBPP() / 8) * nChannels;    //  this is pixel count
  nbuf += 512;    //  leave a little extra at the end
  PrimaryCCD.setFrameBufferSize(nbuf);

  DEBUGF(INDI::Logger::DBG_DEBUG, "Setting frame buffer size to %d bytes.", nbuf);

  return true;
}

bool ASICCD::UpdateCCDBin(int binx, int biny)
{
  INDI_UNUSED(biny);

  PrimaryCCD.setBin(binx, binx);

  return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

float ASICCD::calcTimeLeft(float duration, timeval *start_time)
{
  double timesince;
  double timeleft;
  struct timeval now;
  gettimeofday(&now, NULL);

  timesince = (double) (now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double) (start_time->tv_sec * 1000.0 + start_time->tv_usec / 1000);
  timesince = timesince / 1000;

  timeleft = duration - timesince;
  return timeleft;
}

/* Downloads the image from the CCD.
 N.B. No processing is done on the image */
int ASICCD::grabImage()
{
  ASI_ERROR_CODE errCode = ASI_SUCCESS;

  ASI_IMG_TYPE type = getImageType();

  unsigned char * image = (unsigned char *) PrimaryCCD.getFrameBuffer();

  unsigned char *buffer = image;

  int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * (PrimaryCCD.getBPP() / 8);
  int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY() * (PrimaryCCD.getBPP() / 8);
  int nChannels = (type == ASI_IMG_RGB24) ? 3 : 1;

  if (type == ASI_IMG_RGB24)
  {
      buffer = (unsigned char *) malloc(width*height*nChannels);
      if (buffer == NULL)
      {
          DEBUG(INDI::Logger::DBG_ERROR, "Not enough memory for RGB 24 buffer, aborting...");
          return -1;
      }
  }

  if ( (errCode = ASIGetDataAfterExp(m_camInfo->CameraID, buffer, width*height*nChannels)) != ASI_SUCCESS)
  {
      DEBUGF(INDI::Logger::DBG_ERROR, "ASIGetDataAfterExp (%dx%d #%d channels) error (%d)", width, height, nChannels, errCode);
      if (type == ASI_IMG_RGB24)
          free(buffer);
      return -1;
  }

  if (type == ASI_IMG_RGB24)
  {
      unsigned char *subR = image;
      unsigned char *subG = image + width * height;
      unsigned char *subB = image + width * height * 2;
      int size = width*height*3-3;

      for (int i=0; i <= size; i+= 3)
      {
          *subR++ = buffer[i];
          *subG++ = buffer[i+1];
          *subB++ = buffer[i+2];
      }

      free(buffer);
  }

  PrimaryCCD.setNAxis(2);

  bool rememberBayer= HasBayer();

  // If we're sending Luma, turn off bayering
  if (type == ASI_IMG_Y8 && HasBayer())
  {
      asiCap.hasBayer = false;
      SetCCDCapability(&asiCap);
  }
  else if (type == ASI_IMG_RGB24)
  {
      PrimaryCCD.setNAxis(3);
  }

  DEBUG(INDI::Logger::DBG_SESSION, "Download complete.");

  ExposureComplete(&PrimaryCCD);

  // Restore bayer cap
  asiCap.hasBayer = rememberBayer;
  SetCCDCapability(&asiCap);

  return 0;
}

void ASICCD::TimerHit()
{
  float timeleft;

  if (isConnected() == false)
    return;  //  No need to reset timer if we are not connected anymore

  if (InExposure)
  {
    timeleft = calcTimeLeft(ExposureRequest, &ExpStart);

    if (timeleft < 0.05)
    {
      //  it's real close now, so spin on it
      int timeoutCounter=0;

      while (timeleft > 0)
      {
              ASI_EXPOSURE_STATUS status = ASI_EXP_IDLE;
              ASI_ERROR_CODE errCode = ASI_SUCCESS;

              timeoutCounter++;

              // Timeout after roughly 2.5 seconds
              if (timeoutCounter > 50)
              {
                  DEBUGF(INDI::Logger::DBG_ERROR, "ASIGetExpStatus timeout out (%d)", errCode);
                  PrimaryCCD.setExposureFailed();
                  InExposure = false;
                  SetTimer(POLLMS);
                  return;
              }

              if ( (errCode = ASIGetExpStatus(m_camInfo->CameraID, &status)) != ASI_SUCCESS)
              {
                  DEBUGF(INDI::Logger::DBG_DEBUG, "ASIGetExpStatus error (%d)", errCode);
                  if (++exposureRetries >= MAX_EXP_RETRIES)
                  {
                      DEBUGF(INDI::Logger::DBG_SESSION, "Exposure failed (%d)", errCode);
                      PrimaryCCD.setExposureFailed();
                      InExposure = false;
                      SetTimer(POLLMS);
                      return;
                  }
                  else
                  {
                      InExposure = false;
                      StartExposure(ExposureRequest);
                      SetTimer(POLLMS);
                      return;
                  }
              }
              else
              {
                  if (status == ASI_EXP_SUCCESS)
                      break;
                  else if (status == ASI_EXP_FAILED)
                  {
                      DEBUGF(INDI::Logger::DBG_ERROR, "ASIGetExpStatus timeout out (%d)", errCode);
                      PrimaryCCD.setExposureFailed();
                      InExposure = false;
                      SetTimer(POLLMS);
                      return;
                  }
                  else
                    usleep(50000);
              }
          }

          exposureRetries=0;

          /* We're done exposing */
          DEBUG(INDI::Logger::DBG_SESSION,  "Exposure done, downloading image...");

          PrimaryCCD.setExposureLeft(0);
          InExposure = false;
          /* grab and save image */
          grabImage();
    }
    else
    {
      //DEBUGF(INDI::Logger::DBG_DEBUG, "With time left %ld", timeleft);
      PrimaryCCD.setExposureLeft(timeleft);
    }
  }

  if (HasCooler() && TemperatureUpdateCounter++ > TEMPERATURE_UPDATE_FREQ)
  {
      TemperatureUpdateCounter = 0;

      long ASIControlValue=0;
      ASI_BOOL ASIControlAuto;

      ASI_ERROR_CODE errCode = ASIGetControlValue(m_camInfo->CameraID, ASI_TEMPERATURE, &ASIControlValue, &ASIControlAuto);
      if (errCode != ASI_SUCCESS)
      {
          DEBUGF(INDI::Logger::DBG_ERROR, "ASIGetControlValue ASI_TEMPERATURE error (%d)", errCode);
          TemperatureNP.s = IPS_ALERT;
      }
      else
      {
          TemperatureN[0].value = ASIControlValue / 10.0;
      }

      switch (TemperatureNP.s)
      {
      case IPS_IDLE:
      case IPS_OK:
      case IPS_ALERT:
            IDSetNumber(&TemperatureNP, NULL);
        break;

      case IPS_BUSY:
        // If we're within threshold, let's make it BUSY ---> OK
        if (fabs(TemperatureRequest - TemperatureN[0].value) <= TEMP_THRESHOLD)
          TemperatureNP.s = IPS_OK;

        IDSetNumber(&TemperatureNP, NULL);
        break;
      }

      errCode = ASIGetControlValue(m_camInfo->CameraID, ASI_COOLER_POWER_PERC, &ASIControlValue, &ASIControlAuto);
      if (errCode != ASI_SUCCESS)
      {
          DEBUGF(INDI::Logger::DBG_ERROR, "ASIGetControlValue ASI_COOLER_POWER_PERC error (%d)", errCode);
          CoolerNP.s = IPS_ALERT;
      }
      else
      {
          CoolerN[0].value = ASIControlValue;
          if (ASIControlValue > 0)
              CoolerNP.s = IPS_BUSY;
          else
              CoolerNP.s = IPS_IDLE;
      }

      IDSetNumber(&CoolerNP, NULL);
  }

  if(InWEPulse)
  {
      timeleft=calcTimeLeft(WEPulseRequest, &WEPulseStart);

     if(timeleft <= (POLLMS+50)/1000.0)
     {
         //  it's real close now, so spin on it
         while(timeleft > 0)
         {
            int slv;
            slv=100000*timeleft;
            usleep(slv);
            timeleft=calcTimeLeft(WEPulseRequest, &WEPulseStart);
         }

         ASIPulseGuideOff(m_camInfo->CameraID, WEDir);
         InWEPulse = false;
     }
  }

  if(InNSPulse)
  {
      timeleft=calcTimeLeft(NSPulseRequest, &NSPulseStart);

     if(timeleft <= (POLLMS+50)/1000.0)
     {
         //  it's real close now, so spin on it
         while(timeleft > 0)
         {
            int slv;
            slv=100000*timeleft;
            usleep(slv);
            timeleft=calcTimeLeft(NSPulseRequest, &NSPulseStart);
         }

         ASIPulseGuideOff(m_camInfo->CameraID, NSDir);
         InNSPulse = false;
     }
  }

  SetTimer(POLLMS);
}

bool ASICCD::GuideNorth(float ms)
{
    RemoveTimer(NStimerID);

    NSDir = ASI_GUIDE_NORTH;

    ASIPulseGuideOn(m_camInfo->CameraID, NSDir);

    DEBUG(INDI::Logger::DBG_DEBUG, "Starting NORTH guide");

    if (ms <= POLLMS)
    {
        usleep(ms*1000);

        ASIPulseGuideOff(m_camInfo->CameraID, NSDir);

        return true;
    }

    NSPulseRequest=ms/1000.0;
    gettimeofday(&NSPulseStart,NULL);
    InNSPulse=true;

    NStimerID = SetTimer(ms-50);

    return true;
}

bool ASICCD::GuideSouth(float ms)
{
    RemoveTimer(NStimerID);

    NSDir = ASI_GUIDE_SOUTH;

    ASIPulseGuideOn(m_camInfo->CameraID, NSDir);

    DEBUG(INDI::Logger::DBG_DEBUG, "Starting SOUTH guide");

    if (ms <= POLLMS)
    {
        usleep(ms*1000);

        ASIPulseGuideOff(m_camInfo->CameraID, NSDir);

        return true;
    }

    NSPulseRequest=ms/1000.0;
    gettimeofday(&NSPulseStart,NULL);
    InNSPulse=true;

    NStimerID = SetTimer(ms-50);

    return true;
}

bool ASICCD::GuideEast(float ms)
{
    RemoveTimer(WEtimerID);

    WEDir = ASI_GUIDE_EAST;

    ASIPulseGuideOn(m_camInfo->CameraID, NSDir);

    DEBUG(INDI::Logger::DBG_DEBUG, "Starting EAST guide");

    if (ms <= POLLMS)
    {
        usleep(ms*1000);

        ASIPulseGuideOff(m_camInfo->CameraID, NSDir);

        return true;
    }

    WEPulseRequest=ms/1000.0;
    gettimeofday(&WEPulseStart,NULL);
    InWEPulse=true;

    WEtimerID = SetTimer(ms-50);

    return true;
}

bool ASICCD::GuideWest(float ms)
{
    RemoveTimer(WEtimerID);

    WEDir = ASI_GUIDE_WEST;

    ASIPulseGuideOn(m_camInfo->CameraID, NSDir);

    DEBUG(INDI::Logger::DBG_DEBUG, "Starting WEST guide");

    if (ms <= POLLMS)
    {
        usleep(ms*1000);

        ASIPulseGuideOff(m_camInfo->CameraID, NSDir);

        return true;
    }

    WEPulseRequest=ms/1000.0;
    gettimeofday(&WEPulseStart,NULL);
    InWEPulse=true;

    WEtimerID = SetTimer(ms-50);

    return true;
}


void ASICCD::createControls(int piNumberOfControls)
{
    ASI_ERROR_CODE errCode = ASI_SUCCESS;

    INumber *control_number = NULL;
    int nWritableControls=0;

    ISwitch *auto_switch = NULL;
    int nAutoSwitches    = 0;

    if (pControlCaps)
        free(pControlCaps);

    pControlCaps = (ASI_CONTROL_CAPS *) malloc(sizeof(ASI_CONTROL_CAPS)*piNumberOfControls);
    ASI_CONTROL_CAPS *oneControlCap = pControlCaps;

    for(int i = 0; i < piNumberOfControls; i++)
    {
        if ( (errCode = ASIGetControlCaps(m_camInfo->CameraID, i, oneControlCap)) != ASI_SUCCESS)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "ASIGetControlCaps error (%d)", errCode);
            return;
        }

        DEBUGF(INDI::Logger::DBG_DEBUG, "Control #%d: name (%s), Descp (%s), Min (%ld), Max (%ld), Default Value (%ld), IsAutoSupported (%s), isWritale (%s) "
                                      , i, oneControlCap->Name, oneControlCap->Description, oneControlCap->MinValue, oneControlCap->MaxValue, oneControlCap->DefaultValue,
                                        oneControlCap->IsAutoSupported ? "True": "False",  oneControlCap->IsWritable ? "True" : "False" );

        if (oneControlCap->IsWritable == ASI_FALSE)
            break;

        if (!strcmp(oneControlCap->Name, "Exposure"))
            continue;

        long pValue=0;
        ASI_BOOL isAuto= ASI_FALSE;        

        ASIGetControlValue(m_camInfo->CameraID, oneControlCap->ControlType, &pValue, &isAuto);

        if (oneControlCap->IsWritable)
        {
            nWritableControls++;

            DEBUGF(INDI::Logger::DBG_DEBUG, "Adding above control as writable control number %d", nWritableControls);

            control_number = (control_number == NULL) ? (INumber *) malloc(sizeof(INumber)) : (INumber*) realloc(control_number, nWritableControls*sizeof(INumber));

            IUFillNumber(control_number + nWritableControls - 1, oneControlCap->Name, oneControlCap->Name, "%g", oneControlCap->MinValue, oneControlCap->MaxValue, (oneControlCap->MaxValue - oneControlCap->MinValue)/10.0,  pValue);

            (control_number + nWritableControls - 1)->aux0 = (void *) &oneControlCap->ControlType;

            (control_number + nWritableControls - 1)->aux1 = (void *) &oneControlCap->IsAutoSupported;

        }

        if (oneControlCap->IsAutoSupported)
        {
            nAutoSwitches++;

            DEBUGF(INDI::Logger::DBG_DEBUG, "Adding above control as auto control number %d", nAutoSwitches);

            auto_switch = (auto_switch == NULL) ? (ISwitch *) malloc(sizeof(ISwitch)) : (ISwitch*) realloc(auto_switch, nAutoSwitches*sizeof(ISwitch));

            char autoName[MAXINDINAME];
            snprintf(autoName, MAXINDINAME, "AUTO_%s", oneControlCap->Name);

            IUFillSwitch(auto_switch + (nAutoSwitches-1), autoName, oneControlCap->Name, isAuto == ASI_TRUE ? ISS_ON : ISS_OFF );

            (auto_switch + (nAutoSwitches-1))->aux = (void *) &oneControlCap->ControlType;
        }

        oneControlCap++;

    }

    ControlNP.nnp = nWritableControls;
    ControlNP.np  = control_number;
    ControlN      = control_number;

    ControlSP.nsp  = nAutoSwitches;
    ControlSP.sp   = auto_switch;
    ControlS       = auto_switch;

}

const char * ASICCD::getBayerString()
{
    switch (m_camInfo->BayerPattern)
    {
        case ASI_BAYER_BG:
            return "BGGR";
        case ASI_BAYER_GR:
            return "GRBG";
        case ASI_BAYER_GB:
            return "GBRG";
        case ASI_BAYER_RG:
        default:
            return "RGGB";
    }
}

ASI_IMG_TYPE ASICCD::getImageType()
{
    ASI_IMG_TYPE type = ASI_IMG_END;

    if (VideoFormatSP.nsp > 0)
    {
        ISwitch * sp = IUFindOnSwitch(&VideoFormatSP);
        if (sp)
        type = *((ASI_IMG_TYPE*) sp->aux);
    }

    return type;
}

void ASICCD::updateControls()
{
    long pValue=0;
    ASI_BOOL isAuto= ASI_FALSE;

    for (int i=0; i < ControlNP.nnp; i++)
    {
        ASI_CONTROL_TYPE nType = *((ASI_CONTROL_TYPE *) ControlN[i].aux0);

        ASIGetControlValue(m_camInfo->CameraID, nType, &pValue, &isAuto);

        ControlN[i].value = pValue;

        for (int j=0; j < ControlSP.nsp; j++)
        {
            ASI_CONTROL_TYPE sType = *((ASI_CONTROL_TYPE *) ControlS[j].aux);

            if (sType == nType)
            {
                ControlS[j].s = (isAuto == ASI_TRUE) ? ISS_ON : ISS_OFF;
                break;
            }
        }
    }

    IDSetNumber(&ControlNP, NULL);
    IDSetSwitch(&ControlSP, NULL);
}

void * ASICCD::streamVideoHelper(void* context)
{
    return ((ASICCD*)context)->streamVideo();
}

void* ASICCD::streamVideo()
{
  pthread_mutex_lock(&condMutex);
  unsigned char *compressedFrame = (unsigned char *) malloc(1);

  while (true)
  {
      while (streamPredicate == 0)
                  pthread_cond_wait(&cv, &condMutex);

      if (terminateThread)
          break;

      // release condMutex
      pthread_mutex_unlock(&condMutex);

      unsigned char *targetFrame = (unsigned char *) PrimaryCCD.getFrameBuffer();
      // Remove the 512 offset
      uLong totalBytes     = PrimaryCCD.getFrameBufferSize() - 512;
      int waitMS           = ExposureRequest*2000 + 500;

      ASIGetVideoData(m_camInfo->CameraID, targetFrame, totalBytes, waitMS);

      uLongf compressedBytes = 0;

      /* Do we want to compress ? */
       if (PrimaryCCD.isCompressed())
       {
           /* Compress frame */
           compressedFrame = (unsigned char *) realloc (compressedFrame, sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);

           compressedBytes = sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3;

           int r = compress2(compressedFrame, &compressedBytes, targetFrame, totalBytes, 4);
           if (r != Z_OK)
           {
               /* this should NEVER happen */
               IDLog("internal error - compression failed: %d\n", r);
               return 0;
           }

           /* #3.A Send it compressed */
           imageB->blob = compressedFrame;
           imageB->bloblen = compressedBytes;
           imageB->size = totalBytes;
           strcpy(imageB->format, ".stream.z");
       }
       else
       {
          /* #3.B Send it uncompressed */
           imageB->blob = targetFrame;
           imageB->bloblen = totalBytes;
           imageB->size = totalBytes;
           strcpy(imageB->format, ".stream");
       }

      imageBP->s = IPS_OK;
      IDSetBLOB (imageBP, NULL);

     // lock cond before wait again
     pthread_mutex_lock(&condMutex);

  }

  free(compressedFrame);
  pthread_mutex_unlock(&condMutex);
  return 0;
}
