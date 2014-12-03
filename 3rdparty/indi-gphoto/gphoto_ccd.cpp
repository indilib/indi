/*
    Driver type: GPhoto Camera INDI Driver

    Copyright (C) 2009 Geoffrey Hausheer
    Copyright (C) 2013 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

*/

#include <memory>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include <indidevapi.h>
#include <eventloop.h>
#include <indilogger.h>

extern "C" {
    #include "gphoto_driver.h"
    #include "gphoto_readimage.h"
}

#include "gphoto_ccd.h"

#define FOCUS_TAB           "Focus"
#define MAX_DEVICES         5      /* Max device cameraCount */
#define POLLMS              1000
#define STREAMPOLLMS        50
#define FOCUS_TIMER         500
#define MAX_RETRIES         3

static int cameraCount;
static GPhotoCCD *cameras[MAX_DEVICES];

/**********************************************************
 *
 *  IMPORRANT: List supported camera models in initializer of deviceTypes structure
 *
 **********************************************************/

static void cleanup() {
  for (int i = 0; i < cameraCount; i++) {
    delete cameras[i];
  }
}

void ISInit()
{
  static bool isInit = false;
  if (!isInit)
  {

      // Let's just create one camera for now
     cameraCount = 1;
     cameras[0] = new GPhotoCCD();

    atexit(cleanup);
    isInit = true;
  }
}

void ISGetProperties(const char *dev) {
  ISInit();
  for (int i = 0; i < cameraCount; i++) {
    GPhotoCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISGetProperties(dev);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  ISInit();
  for (int i = 0; i < cameraCount; i++) {
    GPhotoCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewSwitch(dev, name, states, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) {
  ISInit();
  for (int i = 0; i < cameraCount; i++) {
    GPhotoCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewText(dev, name, texts, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  ISInit();
  for (int i = 0; i < cameraCount; i++) {
    GPhotoCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewNumber(dev, name, values, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}
void ISSnoopDevice(XMLEle *root) {
  INDI_UNUSED(root);
}


//==========================================================================
GPhotoCCD::GPhotoCCD()
{
    // For now let's set name to default name. In the future, we need to to support multiple devices per one driver
    if (*getDeviceName() == '\0')
        strncpy(name, getDefaultName(), MAXINDINAME);
    else
        strncpy(name, getDeviceName(), MAXINDINAME);

    gphotodrv = NULL;
    on_off[0] = strdup("On");
    on_off[1] = strdup("Off");

    setVersion(1, 2);
}
//==========================================================================
GPhotoCCD::~GPhotoCCD()
{
    free(on_off[0]);
    free(on_off[1]);
    expTID = 0;

}

const char * GPhotoCCD::getDefaultName()
{
    return (const char *)"GPhoto CCD";
}

void GPhotoCCD::debugTriggered(bool enable)
{
    gphoto_set_debug(enable ? 1 : 0);
    gphoto_read_set_debug(enable ? 1 : 0);
}

bool GPhotoCCD::initProperties()
{
  // Init parent properties first
  INDI::CCD::initProperties();

  initFocuserProperties(getDeviceName(), FOCUS_TAB);

  IUFillText(&mPortT[0], "PORT" , "Port", "");
  IUFillTextVector(&PortTP, mPortT, NARRAY(mPortT), getDeviceName(),	"SHUTTER_PORT" , "Shutter Release", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

  //We don't know how many items will be in the switch yet
  IUFillSwitchVector(&mIsoSP, NULL, 0, getDeviceName(), "CCD_ISO", "ISO", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
  IUFillSwitchVector(&mFormatSP, NULL, 0, getDeviceName(), "CAPTURE_FORMAT", "Capture Format", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

  IUFillSwitch(&autoFocusS[0], "Set", "", ISS_OFF);
  IUFillSwitchVector(&autoFocusSP, autoFocusS, 1, getDeviceName(), "Auto Focus", "", FOCUS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  IUFillSwitch(&transferFormatS[0], "FITS", "", ISS_ON);
  IUFillSwitch(&transferFormatS[1], "Native", "", ISS_OFF);
  IUFillSwitchVector(&transferFormatSP, transferFormatS, 2, getDeviceName(), "Transfer Format", "", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  IUFillSwitch(&livePreviewS[0], "Enable", "", ISS_OFF);
  IUFillSwitch(&livePreviewS[1], "Disable", "", ISS_ON);
  IUFillSwitchVector(&livePreviewSP, livePreviewS, 2, getDeviceName(), "VIDEO_STREAM", "Preview", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  CCDCapability cap;

  cap.canAbort = false;
  cap.canBin = false;
  //cap.canSubFrame = false;
  cap.canSubFrame = true;
  cap.hasCooler = false;
  cap.hasGuideHead = false;
  cap.hasShutter = false;
  cap.hasST4Port = false;

  SetCCDCapability(&cap);

  FocuserCapability focusCap;
  focusCap.canAbort=false;
  focusCap.canAbsMove=false;
  focusCap.canRelMove=false;
  focusCap.variableSpeed=true;

  SetFocuserCapability(&focusCap);

  FocusSpeedN[0].min=0;
  FocusSpeedN[0].max=3;
  FocusSpeedN[0].step=1;
  FocusSpeedN[0].value=0;

  /* JM 2014-05-20 Make PrimaryCCD.ImagePixelSizeNP writable since we can't know for now the pixel size and bit depth from gphoto */
  PrimaryCCD.setCCDInfoWritable();

  return true;
}

void GPhotoCCD::ISGetProperties(const char *dev)
{
  INDI::CCD::ISGetProperties(dev);

  defineText(&PortTP);

  if (isConnected())
  {
      if (mIsoSP.nsp > 0)
            defineSwitch(&mIsoSP);
      if (mFormatSP.nsp > 0)
        defineSwitch(&mFormatSP);

      defineSwitch(&transferFormatSP);
      defineSwitch(&livePreviewSP);
      defineSwitch(&autoFocusSP);

      defineSwitch(&FocusMotionSP);
      defineNumber(&FocusSpeedNP);
      defineNumber(&FocusTimerNP);

      ShowExtendedOptions();
  }

  // Add Debug, Simulator, and Configuration controls
  addAuxControls();
}

bool GPhotoCCD::updateProperties()
{
  INDI::CCD::updateProperties();

  if (isConnected())
  {
      if (mIsoSP.nsp > 0)
        defineSwitch(&mIsoSP);
      if (mFormatSP.nsp > 0)
        defineSwitch(&mFormatSP);

      defineSwitch(&transferFormatSP);
      defineSwitch(&livePreviewSP);
      defineSwitch(&autoFocusSP);

      defineSwitch(&FocusMotionSP);
      defineNumber(&FocusSpeedNP);
      defineNumber(&FocusTimerNP);

      imageBP=getBLOB("CCD1");
      imageB=imageBP->bp;

      // Dummy values until first capture is done
      SetCCDParams(1280, 1024, 8, 5.4, 5.4);

    if (sim == false)
    {
        ShowExtendedOptions();
        DEBUG(INDI::Logger::DBG_SESSION, "Please update the camera pixel size in the Image Info section. The camera resolution will be updated after the first exposure is complete.");
    }

    timerID = SetTimer(POLLMS);
  } else
  {
    if (mIsoSP.nsp > 0)
       deleteProperty(mIsoSP.name);
    if (mFormatSP.nsp > 0)
       deleteProperty(mFormatSP.name);

    deleteProperty(livePreviewSP.name);
    deleteProperty(autoFocusSP.name);    
    deleteProperty(transferFormatSP.name);
    deleteProperty(FocusMotionSP.name);
    deleteProperty(FocusSpeedNP.name);
    deleteProperty(FocusTimerNP.name);

    HideExtendedOptions();
    rmTimer(timerID);
  }

  return true;
}

bool GPhotoCCD::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(strcmp(name,PortTP.name)==0)
        {
            PortTP.s=IPS_OK;
            IUUpdateText(&PortTP,texts,names,n);
            IDSetText(&PortTP, NULL);
            return true;
        }

        if(CamOptions.find(name) != CamOptions.end())
        {
            cam_opt *opt = CamOptions[name];
            if (opt->widget->type != GP_WIDGET_TEXT)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "ERROR: Property '%s'is not a string", name);
                return false;
            }
            if (opt->widget->readonly)
            {
                DEBUGF(INDI::Logger::DBG_WARNING, "WARNING: Property %s is read-only", name);
                IDSetText(&opt->prop.text, NULL);
                return false;
            }

            if (IUUpdateText(&opt->prop.text, texts, names, n) < 0)
                return false;
            gphoto_set_widget_text(gphotodrv, opt->widget, texts[0]);
            opt->prop.num.s = IPS_OK;
            IDSetText(&opt->prop.text, NULL);
            return true;
        }
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool GPhotoCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{

  if (strcmp(dev, getDeviceName()) == 0)
  {
      if (!strcmp(name, mIsoSP.name))
      {
          if (IUUpdateSwitch(&mIsoSP, states, names, n) < 0)
              return false;

          for (int i = 0; i < mIsoSP.nsp; i++)
          {
              if (mIsoS[i].s == ISS_ON)
              {
                  if (sim == false)
                      gphoto_set_iso(gphotodrv, i);
                  mIsoSP.s = IPS_OK;
                  IDSetSwitch(&mIsoSP, NULL);
                  break;
              }
          }
      }

      if (!strcmp(name, mFormatSP.name))
      {
          if (IUUpdateSwitch(&mFormatSP, states, names, n) < 0)
              return false;

          for (int i = 0; i < mFormatSP.nsp; i++)
          {
              if (mFormatS[i].s == ISS_ON)
              {
                  if (sim == false)
                    gphoto_set_format(gphotodrv, i);
                  mFormatSP.s = IPS_OK;
                  IDSetSwitch(&mFormatSP, NULL);
                  break;
              }
          }
      }

      if (!strcmp(name, transferFormatSP.name))
      {
          IUUpdateSwitch(&transferFormatSP, states, names, n);
          transferFormatSP.s = IPS_OK;
          IDSetSwitch(&transferFormatSP, NULL);
          return true;
      }

      if (!strcmp(name, autoFocusSP.name))
      {
          IUResetSwitch(&autoFocusSP);
          if (gphoto_auto_focus == GP_OK)
              autoFocusSP.s = IPS_OK;
          else
              autoFocusSP.s = IPS_ALERT;

          IDSetSwitch(&autoFocusSP, NULL);
          return true;
      }

      if (!strcmp(name, UploadSP.name))
      {
          IUUpdateSwitch(&UploadSP, states, names, n);
          UploadSP.s = IPS_OK;
          IDSetSwitch(&UploadSP, NULL);

          if (!sim)
              gphoto_set_upload_settings(gphotodrv, IUFindOnSwitchIndex(&UploadSP));
          return true;
      }

      if (!strcmp(name, livePreviewSP.name))
      {
          IUUpdateSwitch(&livePreviewSP, states, names, n);
          if (livePreviewS[0].s == ISS_ON)
          {
              livePreviewSP.s = IPS_BUSY;
              SetTimer(STREAMPOLLMS);
          }
          else
              livePreviewSP.s = IPS_IDLE;
          IDSetSwitch(&livePreviewSP, NULL);
          return true;
      }

      if (strstr(name, "FOCUS"))
      {
          return processFocuserSwitch(dev, name, states, names, n);
      }

      if(CamOptions.find(name) != CamOptions.end())
      {
          cam_opt *opt = CamOptions[name];
          if (opt->widget->type != GP_WIDGET_RADIO
              && opt->widget->type != GP_WIDGET_MENU
              && opt->widget->type != GP_WIDGET_TOGGLE)
          {
              DEBUGF(INDI::Logger::DBG_ERROR, "ERROR: Property '%s'is not a switch (%d)", name, opt->widget->type);
              return false;
          }

          if (opt->widget->readonly)
          {
              DEBUGF(INDI::Logger::DBG_WARNING, "WARNING: Property %s is read-only", name);
              IDSetSwitch(&opt->prop.sw, NULL);
              return false;
          }

          if (IUUpdateSwitch(&opt->prop.sw, states, names, n) < 0)
              return false;

          if (opt->widget->type == GP_WIDGET_TOGGLE)
          {
              gphoto_set_widget_num(gphotodrv, opt->widget, opt->item.sw[ON_S].s == ISS_ON);
          } else
          {
              for (int i = 0; i < opt->prop.sw.nsp; i++)
              {
                  if (opt->item.sw[i].s == ISS_ON)
                  {
                      gphoto_set_widget_num(gphotodrv, opt->widget, i);
                      break;
                  }
              }
          }
          opt->prop.sw.s = IPS_OK;
          IDSetSwitch(&opt->prop.sw, NULL);
          return true;
      }            
  }

  return INDI::CCD::ISNewSwitch(dev, name, states, names, n);

}

bool GPhotoCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{

  if (strcmp(dev, getDeviceName()) == 0)
  {
      if (strstr(name, "FOCUS_"))
          return processFocuserNumber(dev, name, values, names, n);

      if(CamOptions.find(name) != CamOptions.end())
      {
          cam_opt *opt = CamOptions[name];
          if (opt->widget->type != GP_WIDGET_RANGE)
          {
              DEBUGF(INDI::Logger::DBG_ERROR, "ERROR: Property '%s'is not a string", name);
              return false;
          }
          if (opt->widget->readonly)
          {
              DEBUGF(INDI::Logger::DBG_WARNING, "WARNING: Property %s is read-only", name);
              return false;
          }
          if (IUUpdateNumber(&opt->prop.num, values, names, n) < 0)
              return false;
          gphoto_set_widget_num(gphotodrv, opt->widget, values[0]);
          opt->prop.num.s = IPS_OK;
          IDSetNumber(&opt->prop.num, NULL);
          return true;
      }
  }

  return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool GPhotoCCD::Connect()
{

  sim = isSimulation();

  int setidx;
  const char **options;
  int max_opts;
  const char *port = NULL;

  if(PortTP.tp[0].text && strlen(PortTP.tp[0].text))
      port = PortTP.tp[0].text;

  if (sim == false && ! (gphotodrv = gphoto_open(port)))
  {
      DEBUG(INDI::Logger::DBG_ERROR, "Can not open camera: Power OK?");
      return false;
  }

  if (mFormatS)
      free(mFormatS);

  if (sim)
  {
    setidx =0;
    max_opts=1;
    const char *fmts[] = { "Custom"};
    options = fmts;
  }
  else
  {
      setidx = gphoto_get_format_current(gphotodrv);
      options = gphoto_get_formats(gphotodrv, &max_opts);
  }
  mFormatS = create_switch("FORMAT", options, max_opts, setidx);
  mFormatSP.sp = mFormatS;
  mFormatSP.nsp = max_opts;

  if (mIsoS)
      free(mIsoS);

  if (sim)
  {
      setidx=0;
      max_opts=4;
      const char *isos[] = { "100", "200", "400", "800" };
      options = isos;
  }
  else
  {
    setidx = gphoto_get_iso_current(gphotodrv);
    options = gphoto_get_iso(gphotodrv, &max_opts);
  }

  mIsoS = create_switch("ISO", options, max_opts, setidx);    
  mIsoSP.sp = mIsoS;
  mIsoSP.nsp = max_opts;

  DEBUGF(INDI::Logger::DBG_SESSION, "%s is online.", getDeviceName());

  return true;

}

bool GPhotoCCD::Disconnect()
{
    if (sim)
        return true;
   gphoto_close(gphotodrv);
   gphotodrv = NULL;
   DEBUGF(INDI::Logger::DBG_SESSION, "%s is offline.", getDeviceName());
  return true;

}

bool GPhotoCCD::StartExposure(float duration)
{
    if (PrimaryCCD.isExposing())
    {
        DEBUG(INDI::Logger::DBG_ERROR, "GPhoto driver is already exposing. Can not abort.");
        return false;
    }

    /* start new exposure with last ExpValues settings.
     * ExpGo goes busy. set timer to read when done
     */
    int expms = (int)ceil(duration*1000);

    PrimaryCCD.setExposureDuration(duration);


    if (sim == false && gphoto_start_exposure(gphotodrv, expms) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error starting exposure");
        return false;
    }

    ExposureRequest = duration;
    gettimeofday(&ExpStart, NULL);
    InExposure = true;

    DEBUGF(INDI::Logger::DBG_SESSION, "Starting %g sec exposure", duration);

    return true;
}

bool GPhotoCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    if (transferFormatS[0].s != ISS_ON)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Subframing is only supported in FITS transport mode.");
        return false;
    }

    PrimaryCCD.setFrame(x,y,w,h);

    return true;
}

float GPhotoCCD::CalcTimeLeft()
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

void GPhotoCCD::TimerHit()
{
  int timerID = -1;
  long timeleft=1e6;

  if (isConnected() == false)
    return;

  if (livePreviewSP.s == IPS_BUSY)
  {
      bool rc = capturePreview();

      if (rc)          
          SetTimer(STREAMPOLLMS);
      else
      {
          livePreviewSP.s = IPS_ALERT;
          DEBUG(INDI::Logger::DBG_ERROR, "Error capturing preview.");
          livePreviewS[0].s = ISS_OFF;
          livePreviewS[1].s = ISS_ON;
          IDSetSwitch(&livePreviewSP, NULL);
      }

      return;
  }

  if (InExposure)
  {
    timeleft = CalcTimeLeft();

    if (timeleft < 1.0)
    {
      if (timeleft > 0.25) {
        //  a quarter of a second or more
        //  just set a tighter timer
        timerID = SetTimer(250);
      } else {
        if (timeleft > 0.07) {
          //  use an even tighter timer
          timerID = SetTimer(50);
        } else {
          //  it's real close now, so spin on it
          while (!sim && timeleft > 0)
          {

            int slv;
            slv = 100000 * timeleft;
            usleep(slv);
          }

          PrimaryCCD.setExposureLeft(0);
          InExposure = false;
          /* grab and save image */
          bool rc = grabImage();
          if (rc == false)
          {
              PrimaryCCD.setExposureFailed();
          }
        }
      }
    }
    else
        DEBUGF(INDI::Logger::DBG_DEBUG, "Image not ready. Time left %ld", timeleft);


      PrimaryCCD.setExposureLeft(timeleft);

    }

  if (FocusTimerNP.s == IPS_BUSY)
  {
      char errMsg[MAXRBUF];
      if ( gphoto_manual_focus(gphotodrv, focusSpeed, errMsg) != GP_OK)
      {
          DEBUGF(INDI::Logger::DBG_ERROR, "Focusing failed: %s", errMsg);
          FocusTimerNP.s = IPS_ALERT;
          FocusTimerN[0].value = 0 ;
      }
      else
      {
          FocusTimerN[0].value -= FOCUS_TIMER;
          if (FocusTimerN[0].value <= 0)
          {
              FocusTimerN[0].value = 0;
              FocusTimerNP.s = IPS_OK;
          }
          else
              timerID = SetTimer(FOCUS_TIMER);
      }

      IDSetNumber(&FocusTimerNP, NULL);
  }

  if (timerID == -1)
    SetTimer(POLLMS);
}

void GPhotoCCD::UpdateExtendedOptions (void *p)
{
    GPhotoCCD *cam = (GPhotoCCD *)p;
    cam->UpdateExtendedOptions();
}

void GPhotoCCD::UpdateExtendedOptions (bool force)
{
	map<string, cam_opt *>::iterator it;
    if(! expTID)
    {
		for ( it = CamOptions.begin() ; it != CamOptions.end(); it++ )
		{
			cam_opt *opt = (*it).second;
            if(force || gphoto_widget_changed(opt->widget))
            {
				gphoto_read_widget(opt->widget);
				UpdateWidget(opt);
			}
		}
	}

    optTID = IEAddTimer (1000, GPhotoCCD::UpdateExtendedOptions, this);
}

bool GPhotoCCD::grabImage()
{
    //char ext[16];
    char *memptr = PrimaryCCD.getFrameBuffer();
    char *ccdptr = memptr;
	size_t memsize;
    int fd, naxis=2, w, h, bpp=8;
    static bool firstGrab=true;

    if (sim)
    {

      w= PrimaryCCD.getXRes();
      h= PrimaryCCD.getYRes();
      size_t fullbuf_size = w*h + 512;
      char * fullbuf = (char *) malloc(fullbuf_size);
      for (int i = 0; i < h; i++)
        for (int j = 0; j < w; j++)
          fullbuf[i * w + j] = rand() % 255;

      // Starting address if subframing
      memptr = fullbuf + (PrimaryCCD.getSubY() * PrimaryCCD.getXRes()) + PrimaryCCD.getSubX();
      memsize = PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * PrimaryCCD.getBPP()/8;

      PrimaryCCD.setFrameBuffer(memptr);
      PrimaryCCD.setFrameBufferSize(memsize, false);
      //PrimaryCCD.setResolution(w, h);
      //PrimaryCCD.setFrame(0, 0, w, h);
      PrimaryCCD.setNAxis(naxis);
      PrimaryCCD.setBPP(bpp);

      ExposureComplete(&PrimaryCCD);
      return true;

    }

    // If only save to SD Card, let's not upload back to client
    /*if (UploadS[GP_UPLOAD_SDCARD].s == ISS_ON)
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Exposure complete. Image saved to SD Card.");
        ExposureComplete(&PrimaryCCD);
        return true;
    }*/


    if (transferFormatS[0].s == ISS_ON)
    {
	    
        char tmpfile[] = "/tmp/indi_XXXXXX";

        //dcraw can't read from stdin, so we need to write to disk then read it back
        fd = mkstemp(tmpfile);

        int ret = gphoto_read_exposure_fd(gphotodrv, fd);

        if (ret != GP_OK || fd == -1)
        {
            if (fd == -1)
                DEBUGF(INDI::Logger::DBG_ERROR, "Exposure failed to save image. Cannot create temp file %s", tmpfile);
            else
                DEBUGF(INDI::Logger::DBG_ERROR, "Exposure failed to save image... %s", gp_result_as_string(ret));
            unlink(tmpfile);
            return false;
        }

        if (!strcmp(gphoto_get_file_extension(gphotodrv), "unknown"))
        {
                DEBUG(INDI::Logger::DBG_ERROR, "Exposure failed.");
                return false;
        }

        /* We're done exposing */
        DEBUG(INDI::Logger::DBG_SESSION, "Exposure done, downloading image...");
    
    
        if(strcasecmp(gphoto_get_file_extension(gphotodrv), "jpg") == 0 ||
           strcasecmp(gphoto_get_file_extension(gphotodrv), "jpeg") == 0)
        {
                if (read_jpeg(tmpfile, &memptr, &memsize, &naxis, &w, &h))
                {
                    DEBUG(INDI::Logger::DBG_ERROR, "Exposure failed to parse jpeg.");
                    unlink(tmpfile);
                    return false;
                }
        }
        else
        {
                if (read_dcraw(tmpfile, &memptr, &memsize, &naxis, &w, &h, &bpp))
                {
                    DEBUG(INDI::Logger::DBG_ERROR, "Exposure failed to parse raw image.");
                    unlink(tmpfile);
                    return false;
                }
		
                unlink(tmpfile);
        }

        PrimaryCCD.setImageExtension("fits");

        ccdptr = memptr;
        memptr += ((PrimaryCCD.getSubY() * PrimaryCCD.getXRes()) + PrimaryCCD.getSubX()) *bpp/8 ;
        memsize = PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * bpp/8;
        if (firstGrab)
        {
            PrimaryCCD.setFrame(0, 0, w, h);
            firstGrab = false;
        }
    }
    else
    {
        gphoto_get_dimensions(gphotodrv, &w, &h);
        int rc = gphoto_read_exposure(gphotodrv);

        if (rc != 0)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Failed to expose.");
            return rc;
        }
	
        /* We're done exposing */
         DEBUG(INDI::Logger::DBG_DEBUG, "Exposure done, downloading image...");
         char *newMemptr = NULL;
         gphoto_get_buffer(gphotodrv, (const char **)&newMemptr, &memsize);
         memptr = (char *)realloc(memptr, memsize); // We copy the obtained memory pointer to avoid freeing some gphoto memory
         ccdptr=memptr;
         memcpy(memptr, newMemptr, memsize); //

        PrimaryCCD.setImageExtension(gphoto_get_file_extension(gphotodrv));
        PrimaryCCD.setFrame(0, 0, w, h);
    }

    PrimaryCCD.setFrameBuffer(memptr);
    PrimaryCCD.setFrameBufferSize(memsize, false);
    PrimaryCCD.setResolution(w, h);    
    PrimaryCCD.setNAxis(naxis);
    PrimaryCCD.setBPP(bpp);

    ExposureComplete(&PrimaryCCD);

    // Restore old pointer
    PrimaryCCD.setFrameBuffer(ccdptr);

    return true;
}

ISwitch *GPhotoCCD::create_switch(const char *basestr, const char **options, int max_opts, int setidx)
{
	int i;
	ISwitch *sw = (ISwitch *)calloc(sizeof(ISwitch), max_opts);
    ISwitch *one_sw = sw;

    char sw_name[MAXINDINAME];
    char sw_label[MAXINDILABEL];
    ISState sw_state;

    for(i = 0; i < max_opts; i++)
    {
        snprintf(sw_name, MAXINDINAME, "%s%d", basestr, i);
        strncpy(sw_label, options[i], MAXINDILABEL);
        sw_state = (i == setidx) ? ISS_ON : ISS_OFF;

        IUFillSwitch(one_sw++, sw_name, sw_label, sw_state);
	}
	return sw;
}

void GPhotoCCD::UpdateWidget(cam_opt *opt)
{
	struct tm *tm;

    switch(opt->widget->type)
    {
	case GP_WIDGET_RADIO:
	case GP_WIDGET_MENU:
		for (int i = 0; i < opt->widget->choice_cnt; i++)
			opt->item.sw[i].s = opt->widget->value.index == i ? ISS_ON : ISS_OFF;
		IDSetSwitch(&opt->prop.sw, NULL);
		break;
	case GP_WIDGET_TEXT:
		free(opt->item.text.text);
		opt->item.text.text = strdup(opt->widget->value.text);
		IDSetText(&opt->prop.text, NULL);
		break;
	case GP_WIDGET_TOGGLE:
		if(opt->widget->value.toggle) {
			opt->item.sw[0].s = ISS_ON;
			opt->item.sw[0].s = ISS_OFF;
        } else
        {
			opt->item.sw[0].s = ISS_OFF;
			opt->item.sw[0].s = ISS_ON;
		}
		IDSetSwitch(&opt->prop.sw, NULL);
		break;
	case GP_WIDGET_RANGE:
		opt->item.num.value = opt->widget->value.num;
		IDSetNumber(&opt->prop.num, NULL);
		break;
	case GP_WIDGET_DATE:
		free(opt->item.text.text);
		tm = gmtime((time_t *)&opt->widget->value.date);
		opt->item.text.text = strdup(asctime(tm));
		IDSetText(&opt->prop.text, NULL);
		break;
	default:
		delete opt;
		return;
	}
}

void GPhotoCCD::AddWidget(gphoto_widget *widget)
{
	IPerm perm;
	struct tm *tm;

	if(! widget)
		return;

	perm = widget->readonly ? IP_RO : IP_RW;

	cam_opt *opt = new cam_opt();
	opt->widget = widget;

    switch(widget->type)
    {
	case GP_WIDGET_RADIO:
	case GP_WIDGET_MENU:
		opt->item.sw = create_switch(widget->name, widget->choices, widget->choice_cnt, widget->value.index);
        IUFillSwitchVector(&opt->prop.sw, opt->item.sw, widget->choice_cnt, getDeviceName(),
			widget->name, widget->name, widget->parent, perm, ISR_1OFMANY, 60, IPS_IDLE);
		IDDefSwitch(&opt->prop.sw, NULL);
		break;
	case GP_WIDGET_TEXT:
		IUFillText(&opt->item.text, widget->name, widget->name, widget->value.text);
        IUFillTextVector(&opt->prop.text, &opt->item.text, 1, getDeviceName(),
			widget->name, widget->name, widget->parent, perm, 60, IPS_IDLE);
		IDDefText(&opt->prop.text, NULL);
		break;
	case GP_WIDGET_TOGGLE:
		opt->item.sw = create_switch(widget->name, (const char **)on_off, 2, widget->value.toggle ? 0 : 1);
        IUFillSwitchVector(&opt->prop.sw, opt->item.sw, 2, getDeviceName(),
			widget->name, widget->name, widget->parent, perm, ISR_1OFMANY, 60, IPS_IDLE);
		IDDefSwitch(&opt->prop.sw, NULL);
		break;
	case GP_WIDGET_RANGE:
		IUFillNumber(&opt->item.num, widget->name, widget->name, "%5.2f",
			widget->min, widget->max, widget->step, widget->value.num);
        IUFillNumberVector(&opt->prop.num, &opt->item.num, 1, getDeviceName(),
			widget->name, widget->name, widget->parent, perm, 60, IPS_IDLE);
		break;
	case GP_WIDGET_DATE:
		tm = gmtime((time_t *)&widget->value.date);
		IUFillText(&opt->item.text, widget->name, widget->name, asctime(tm));
        IUFillTextVector(&opt->prop.text, &opt->item.text, 1, getDeviceName(),
			widget->name, widget->name, widget->parent, perm, 60, IPS_IDLE);
		IDDefText(&opt->prop.text, NULL);
		break;
	default:
		delete opt;
		return;
	}
	
	CamOptions[widget->name] = opt;
	
}

void
GPhotoCCD::ShowExtendedOptions(void)
{
	gphoto_widget_list *iter;

	iter = gphoto_find_all_widgets(gphotodrv);
    while(iter)
    {
		gphoto_widget *widget = gphoto_get_widget_info(gphotodrv, &iter);
		AddWidget(widget);
	}

    if (isDebug())
        gphoto_show_options(gphotodrv);

    optTID = IEAddTimer (1000, GPhotoCCD::UpdateExtendedOptions, this);
}

void
GPhotoCCD::HideExtendedOptions(void)
{
    if (optTID)
    {
		IERmTimer(optTID);
		optTID = 0;
	}

    while (CamOptions.begin() != CamOptions.end())
    {
		cam_opt *opt = (*CamOptions.begin()).second;
        IDDelete(getDeviceName(), (*CamOptions.begin()).first.c_str(), NULL);

        switch(opt->widget->type)
        {
		case GP_WIDGET_RADIO:
		case GP_WIDGET_MENU:
		case GP_WIDGET_TOGGLE:
			free(opt->item.sw);
			break;
		case GP_WIDGET_TEXT:
		case GP_WIDGET_DATE:
			free(opt->item.text.text);
			break;
		default:
			break;
		}

		delete opt;
		CamOptions.erase(CamOptions.begin());
	}
}

int GPhotoCCD::MoveFocuser(FocusDirection dir, int speed, int duration)
{
 
   /* gphoto works with steps */ 
   
    if (sim)
        return 0;

   char errMsg[MAXRBUF];
   if (dir == FOCUS_INWARD)
       focusSpeed = speed * -1;
   else
       focusSpeed = speed;

   DEBUGF(INDI::Logger::DBG_DEBUG, "Setting focuser speed to %d", focusSpeed);

   while (duration-->0)
   {
       if ( gphoto_manual_focus(gphotodrv, focusSpeed, errMsg) != GP_OK)
       {
           DEBUGF(INDI::Logger::DBG_ERROR, "Focusing failed: %s", errMsg);
           return -1;
       }
   }

   return 0;
}

bool GPhotoCCD::SetSpeed(int speed)
{
    if (speed >= FocusSpeedN[0].min && speed <= FocusSpeedN[0].max)
        return true;

    return false;
}

bool GPhotoCCD::capturePreview()
{

    if (sim)
        return false;

    int rc = GP_OK;
    char errMsg[MAXRBUF];
    const char* previewData;
    unsigned long int previewSize;

    CameraFile* previewFile = NULL;

    rc = gp_file_new(&previewFile);
    if (rc != GP_OK)
    {
       DEBUGF(INDI::Logger::DBG_ERROR, "Error creating gphoto file: %s", gp_result_as_string(rc));
      return false;
    }

    for (int i=0; i < MAX_RETRIES; i++)
    {
        rc = gphoto_capture_preview(gphotodrv, previewFile, errMsg);
        if (rc == true)
            break;
    }

    if (rc != GP_OK)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errMsg);
        return false;
    }

    if (rc >= GP_OK)
    {
       rc = gp_file_get_data_and_size(previewFile, &previewData, &previewSize);
       if (rc != GP_OK)
       {
           DEBUGF(INDI::Logger::DBG_ERROR, "Error getting preview image data and size: %s", gp_result_as_string(rc));
           return false;
       }
    }

    //DEBUGF(INDI::Logger::DBG_DEBUG, "Preview capture size %d bytes.", previewSize);

   char *previewBlob = (char *) previewData;

   imageB->blob = previewBlob;
   imageB->bloblen = previewSize;
   imageB->size = previewSize;
   strncpy(imageB->format, "stream_jpeg", MAXINDIBLOBFMT);

   IDSetBLOB (imageBP, NULL);

    if (previewFile)
    {
       gp_file_unref(previewFile);
       previewFile = NULL;
    }

    return true;
}

bool GPhotoCCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    if (mIsoSP.nsp > 0)
          IUSaveConfigSwitch(fp, &mIsoSP);

    return true;
}
