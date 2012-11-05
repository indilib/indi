/*
 Starlight Xpress CCD INDI Driver

 Copyright (c) 2012 Cloudmakers, s. r. o.
 All Rights Reserved.

 Code is based on SX INDI Driver by Gerry Rozema and Jasem Mutlaq
 Copyright(c) 2010 Gerry Rozema.
 Copyright(c) 2012 Jasem Mutlaq.
 All rights reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#include <stdio.h>
#include <memory.h>

#include "sxconfig.h"
#include "sxccd.h"

#define SX_GUIDE_EAST             0x08     /* RA+ */
#define SX_GUIDE_NORTH            0x04     /* DEC+ */
#define SX_GUIDE_SOUTH            0x02     /* DEC- */
#define SX_GUIDE_WEST             0x01     /* RA- */
#define SX_CLEAR_NS               0x09
#define SX_CLEAR_WE               0x06

static int count;
static SXCCD *cameras[20];

#define TIMER 1000

#define TRACE(c) (c)
#define DEBUG(c) (c)

void cleanup() {
  TRACE(fprintf(stderr, "-> cleanup()\n"));
  for (int i = 0; i < count; i++) {
    delete cameras[i];
  }
  TRACE(fprintf(stderr, "<- cleanup\n"));
}

void ISInit() {
  static bool isInit = false;
  if (!isInit) {
    DEVICE devices[20];
    const char* names[20];
    count = sxList(devices, names);
    for (int i = 0; i < count; i++) {
      cameras[i] = new SXCCD(devices[i], names[i]);
    }
    atexit(cleanup);
    isInit = true;
  }
}

void ISGetProperties(const char *dev) {
  TRACE(fprintf(stderr, "-> ISGetProperties(%s)\n", dev));
  ISInit();
  for (int i = 0; i < count; i++) {
    SXCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISGetProperties(camera->name);
      if (dev != NULL)
        break;
    }
  }
  TRACE(fprintf(stderr, "<- ISGetProperties\n"));
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  TRACE(fprintf(stderr, "-> ISNewSwitch(%s, %s...)\n", dev, name));
  ISInit();
  for (int i = 0; i < count; i++) {
    SXCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewSwitch(camera->name, name, states, names, num);
      if (dev != NULL)
        break;
    }
  }
  TRACE(fprintf(stderr, "<- ISNewSwitch\n"));
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) {
  TRACE(fprintf(stderr, "-> ISNewText(%s, %s, ...)\n", dev, name));
  ISInit();
  for (int i = 0; i < count; i++) {
    SXCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewText(camera->name, name, texts, names, num);
      if (dev != NULL)
        break;
    }
  }
  TRACE(fprintf(stderr, "<- ISNewText\n"));
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  TRACE(fprintf(stderr, "-> ISNewNumber(%s, %s, ...)\n", dev, name));
  ISInit();
  for (int i = 0; i < count; i++) {
    SXCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewNumber(camera->name, name, values, names, num);
      if (dev != NULL)
        break;
    }
  }
  TRACE(fprintf(stderr, "<- ISNewNumber\n"));
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

void ExposureTimerCallback(void *p) {
  ((SXCCD *) p)->ExposureTimerHit();
}

void GuideExposureTimerCallback(void *p) {
  ((SXCCD *) p)->GuideExposureTimerHit();
}

void WEGuiderTimerCallback(void *p) {
  ((SXCCD *) p)->WEGuiderTimerHit();
}

void NSGuiderTimerCallback(void *p) {
  ((SXCCD *) p)->NSGuiderTimerHit();
}

SXCCD::SXCCD(DEVICE device, const char *name) {
  TRACE(fprintf(stderr, "-> SXCCD::SXCCD\n"));
  this->device = device;
  handle = NULL;
  model = 0;
  oddBuf = NULL;
  evenBuf = NULL;
  GuideStatus = 0;
  TemperatureRequest = 0;
  TemperatureReported = 0;
  ExposureTimeLeft = 0.0;
  GuideExposureTimeLeft = 0.0;
  HasShutter = false;
  HasCooler = false;
  ExposureTimerID = 0;
  DidFlush = false;
  DidLatch = false;
  GuideExposureTimerID = 0;
  InGuideExposure = false;
  DidGuideLatch = false;
  NSGuiderTimerID = 0;
  WEGuiderTimerID = 0;
  snprintf(this->name, 32, "SX CCD %s", name);
  setDeviceName(this->name);
  TRACE(fprintf(stderr, "   %s instance created\n", this->name));
  TRACE(fprintf(stderr, "<- SXCCD::SXCCD\n"));
}

SXCCD::~SXCCD() {
  TRACE(fprintf(stderr, "-> SXCCD::~SXCCD\n"));
  sxClose(handle);
  TRACE(fprintf(stderr, "<- SXCCD::~SXCCD\n"));
}

const char *SXCCD::getDefaultName() {
    return name;
}

bool SXCCD::initProperties() {
  TRACE(fprintf(stderr, "-> SXCCD::initProperties()\n"));
  INDI::CCD::initProperties();

  IUFillNumber(&TemperatureN, "CCD_TEMPERATURE_VALUE", "CCD temperature", "%4.1f", -40, 35, 1, TemperatureRequest);
  IUFillNumberVector(&TemperatureNP, &TemperatureN, 1, getDeviceName(), "CCD_TEMPERATURE", "Temperature", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

  IUFillSwitch(&CoolerS[0], "DISCONNECT_COOLER", "Off", ISS_ON);
  IUFillSwitch(&CoolerS[1], "CONNECT_COOLER", "On", ISS_OFF);
  IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "COOLER_CONNECTION", "Cooler", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

  IUFillSwitch(&ShutterS[0], "SHUTTER_ON", "Manual open", ISS_OFF);
  IUFillSwitch(&ShutterS[1], "SHUTTER_OFF", "Manual close", ISS_ON);
  IUFillSwitchVector(&ShutterSP, ShutterS, 2, getDeviceName(), "SHUTTER_CONNECTION", "Shutter", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

  TRACE(fprintf(stderr, "<- SXCCD::initProperties 1\n"));
  return true;
}

bool SXCCD::updateProperties() {
  TRACE(fprintf(stderr, "-> SXCCD::updateProperties()\n"));

  INDI::CCD::updateProperties();

  if (isConnected()) {
    if (HasCooler) {
      defineNumber(&TemperatureNP);
      defineSwitch(&CoolerSP);
    }
    if (HasShutter)
      defineSwitch(&ShutterSP);
  } else {
    if (HasCooler) {
      deleteProperty(TemperatureNP.name);
      deleteProperty(CoolerSP.name);
    }
    if (HasShutter)
      deleteProperty(ShutterSP.name);
  }

  getCameraParams();

  TRACE(fprintf(stderr, "<- SXCCD::updateProperties 1\n"));
  return true;
}

bool SXCCD::updateCCDBin(int hor, int ver) {
  TRACE(fprintf(stderr, "-> SXCCD::updateCCDBin()\n"));
  if (hor == 3 || ver == 3) {
    IDMessage(getDeviceName(), "3x3 binning is not supported.");
    TRACE(fprintf(stderr, "<- SXCCD::updateCCDBin 0\n"));
    return false;
  }
  PrimaryCCD.setBin(hor, ver);
  TRACE(fprintf(stderr, "<- SXCCD::updateCCDBin 1\n"));
  return true;
}

bool SXCCD::Connect() {
  TRACE(fprintf(stderr, "-> SXCCD::Connect()\n"));
  if (handle == NULL) {
    handle = usb_open(device);
    if (handle != NULL) {
      int rc;
      rc = usb_detach_kernel_driver_np(handle, 0);
      TRACE( fprintf(stderr, "   usb_detach_kernel_driver_np() -> %d\n", rc));
#ifdef __APPLE__
      rc=usb_claim_interface(handle,0);
#else
      rc = usb_claim_interface(handle, 1);
#endif
      TRACE(fprintf(stderr, "   usb_claim_interface() -> %d\n", rc));
      if (!rc)
          return true;
    }
  }
  TRACE(fprintf(stderr, "<- SXCCD::Connect 0\n"));
  return false;
}

bool SXCCD::Disconnect() {
  TRACE(fprintf(stderr, "-> SXCCD::Disconnect()\n"));
  if (handle != NULL) {
    int rc = usb_close(handle);
    TRACE(fprintf(stderr, "   usb_close() -> %d\n", rc));
    handle = NULL;
    HasCooler = false;
  }
  TRACE(fprintf(stderr, "<- SXCCD::Disconnect 1\n"));
  return true;
}

void SXCCD::getCameraParams()
{
      struct t_sxccd_params params;
      sxReset(handle);
      usleep(1000);
      model = sxGetCameraModel(handle);
      bool isInterlaced = model & 0x40;
      PrimaryCCD.setInterlaced(isInterlaced);

      sxGetCameraParams(handle, 0, &params);
      if (isInterlaced) {
        params.pix_height /= 2;
        params.height *= 2;
      }

      SetCCDParams(params.width, params.height, params.bits_per_pixel, params.pix_width, params.pix_height);

      int nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes();
      if (params.bits_per_pixel == 16)
        nbuf *= 2;
      nbuf += 512;
      PrimaryCCD.setFrameBufferSize(nbuf);
      if (evenBuf != NULL)
        delete evenBuf;
      if (oddBuf != NULL)
        delete oddBuf;
      evenBuf = new char[nbuf / 2];
      oddBuf = new char[nbuf / 2];

      HasGuideHead = params.extra_caps & SXCCD_CAPS_GUIDER;
      HasCooler = params.extra_caps & SXUSB_CAPS_COOLER;
      HasShutter = params.extra_caps & SXUSB_CAPS_SHUTTER;
      HasSt4Port = params.extra_caps & SXCCD_CAPS_STAR2K;

      if (HasGuideHead) {
        sxGetCameraParams(handle, 1, &params);
        SetGuidHeadParams(params.width, params.height, params.bits_per_pixel, params.pix_width, params.pix_height);
      }
      SetTimer(TIMER);
      TRACE(fprintf(stderr, "<- SXCCD::Connect 1\n"));
}

void SXCCD::TimerHit() {

  if (HasCooler) {
    if (!DidLatch && !DidGuideLatch) {
      unsigned char status;
      unsigned short temperature;
      sxSetCooler(handle, (unsigned char) (CoolerS[1].s == ISS_ON), (unsigned short) (TemperatureRequest * 10 + 2730), &status, &temperature);
      TemperatureN.value = (temperature - 2730) / 10.0;
      if (TemperatureReported != TemperatureN.value) {
        TemperatureReported = TemperatureN.value;
        if (abs(TemperatureRequest - TemperatureReported) < 1)
          TemperatureNP.s = IPS_OK;
        else
          TemperatureNP.s = IPS_BUSY;
        IDSetNumber(&TemperatureNP, NULL);
      }
    }
  }

  if (InExposure && ExposureTimeLeft >= 0) {
    PrimaryCCD.setExposureLeft(ExposureTimeLeft--);
  }

  if (InGuideExposure && GuideExposureTimeLeft >= 0) {
    GuideCCD.setExposureLeft(GuideExposureTimeLeft--);
  }

  if (isConnected())
    SetTimer(TIMER);
}

int SXCCD::StartExposure(float n) {
  TRACE(fprintf(stderr, "-> SXCCD::StartExposure(%f)\n", n));
  InExposure = true;
  PrimaryCCD.setExposureDuration(n);
  if (PrimaryCCD.isInterlaced() && PrimaryCCD.getBinY() == 1) {
    sxClearPixels(handle, CCD_EXP_FLAGS_FIELD_EVEN, 0);
    usleep(100);
    sxClearPixels(handle, CCD_EXP_FLAGS_FIELD_ODD, 0);
  } else
    sxClearPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 0);

  if (HasShutter)
    sxSetShutter(handle, 0);

  int time = (int) (1000 * n);
  if (time < 1)
    time = 1;

  if (time > 3000) {
    DidFlush = false;
    time -= 3000;
  } else
    DidFlush = true;
  DidLatch = false;
  ExposureTimeLeft = n;

  ExposureTimerID = IEAddTimer(time, ExposureTimerCallback, this);

  TRACE(fprintf(stderr, "<- SXCCD::StartExposure 0\n"));
  return 0;
}

bool SXCCD::AbortExposure() {
  TRACE(fprintf(stderr, "-> SXCCD::AbortExposure()\n"));
  if (InExposure) {
    if (ExposureTimerID)
      IERmTimer(ExposureTimerID);
    if (HasShutter)
      sxSetShutter(handle, 1);
    ExposureTimerID = 0;
    PrimaryCCD.setExposureLeft(ExposureTimeLeft = 0);
    DidLatch = false;
    DidFlush = false;

    TRACE(fprintf(stderr, "<- SXCCD::AbortExposure 1\n"));
    return true;
  }
  TRACE(fprintf(stderr, "<- SXCCD::AbortExposure 0\n"));
  return false;
}

void SXCCD::ExposureTimerHit() {
  TRACE(fprintf(stderr, "-> SXCCD::ExposureTimerHit()\n"));
  if (InExposure) {
    if (!DidFlush) {
      ExposureTimerID = IEAddTimer(3000, ExposureTimerCallback, this);
      sxClearPixels(handle, CCD_EXP_FLAGS_NOWIPE_FRAME, 0);
      DidFlush = true;
      TRACE(fprintf(stderr, "   did flush\n"));
    } else {
      int rc;
      ExposureTimerID = 0;
      bool isInterlaced = PrimaryCCD.isInterlaced();
      int subX = PrimaryCCD.getSubX();
      int subY = PrimaryCCD.getSubY();
      int subW = PrimaryCCD.getSubW();
      int subH = PrimaryCCD.getSubH();
      int binX = PrimaryCCD.getBinX();
      int binY = PrimaryCCD.getBinY();
      int subWW = subW * 2;
      char *buf = PrimaryCCD.getFrameBuffer();
      int size;
      ;

      if (isInterlaced && binY > 1)
        size = subW * subH / 2 / binX / (binY / 2);
      else
        size = subW * subH / binX / binY;

      if (HasShutter)
        sxSetShutter(handle, 1);

      DidLatch = true;
      if (isInterlaced) {
        if (binY > 1) {
          rc = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 0, subX, subY, subW, subH / 2, binX, binY / 2);
          if (rc) {
            TRACE(fprintf(stderr, "   did latch\n"));
            rc = sxReadPixels(handle, (unsigned short *) buf, size);
            if (rc) {
              TRACE(fprintf(stderr, "   did read\n"));
            }
          }
        } else {
          rc = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_EVEN | CCD_EXP_FLAGS_SPARE2, 0, subX, subY, subW, subH / 2, binX, 1);
          if (rc) {
            TRACE(fprintf(stderr, "   did latch even\n"));
            rc = sxReadPixels(handle, (unsigned short *) evenBuf, size / 2);
            if (rc) {
              TRACE(fprintf(stderr, "   did read even\n"));
            }
          }
          if (rc)
            rc = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_ODD | CCD_EXP_FLAGS_SPARE2, 0, subX, subY, subW, subH / 2, binX, 1);
          if (rc) {
            TRACE(fprintf(stderr, "   did latch odd\n"));
            rc = sxReadPixels(handle, (unsigned short *) oddBuf, size / 2);
            if (rc) {
              TRACE(fprintf(stderr, "   did read odd\n"));
            }
          }
          if (rc)
            for (int i = 0, j = 0; i < subH; i += 2, j++) {
              memcpy(buf + i * subWW, evenBuf + (j * subWW), subWW);
              memcpy(buf + ((i + 1) * subWW), oddBuf + (j * subWW), subWW);
            }
        }
      } else {
        rc = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 0, subX, subY, subW, subH, binX, binY);
        if (rc) {
          TRACE(fprintf(stderr, "   did latch\n"));
          rc = sxReadPixels(handle, (unsigned short *) buf, size);
          if (rc) {
            TRACE(fprintf(stderr, "   did read\n"));
          }
        }
      }

      DidLatch = false;
      InExposure = false;
      PrimaryCCD.setExposureLeft(ExposureTimeLeft = 0);
      if (rc)
        ExposureComplete(&PrimaryCCD);
    }
  }
  TRACE(fprintf(stderr, "<- SXCCD::ExposureTimerHit\n"));
}

int SXCCD::StartGuideExposure(float n) {
  TRACE(fprintf(stderr, "-> SXCCD::StartGuideExposure(%f)\n", n));
  InGuideExposure = true;
  GuideCCD.setExposureDuration(n);
  sxClearPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 1);
  int time = (int) (1000 * n);
  if (time < 1)
    time = 1;
  ExposureTimeLeft = n;

  GuideExposureTimerID = IEAddTimer(time, GuideExposureTimerCallback, this);

  TRACE(fprintf(stderr, "<- SXCCD::StartGuideExposure 0\n"));
  return 0;
}

bool SXCCD::AbortGuideExposure() {
  TRACE(fprintf(stderr, "-> SXCCD::AbortGuideExposure()\n"));
  if (InGuideExposure) {
    if (GuideExposureTimerID)
      IERmTimer(GuideExposureTimerID);
    GuideCCD.setExposureLeft(GuideExposureTimeLeft = 0);
    GuideExposureTimerID = 0;
    DidGuideLatch = false;

    TRACE(fprintf(stderr, "<- SXCCD::AbortGuideExposure 1\n"));
    return true;
  }
  TRACE(fprintf(stderr, "<- SXCCD::AbortGuideExposure 0\n"));
  return false;
}

void SXCCD::GuideExposureTimerHit() {
  TRACE(fprintf(stderr, "-> SXCCD::GuideExposureTimerHit()\n"));
  if (InGuideExposure) {
    int rc;
    GuideExposureTimerID = 0;
    int subX = GuideCCD.getSubX();
    int subY = GuideCCD.getSubY();
    int subW = GuideCCD.getSubW();
    int subH = GuideCCD.getSubH();
    int binX = GuideCCD.getBinX();
    int binY = GuideCCD.getBinY();
    int size = subW * subH / binX / binY;
    int subWW = subW * 2;
    char *buf = GuideCCD.getFrameBuffer();

    DidGuideLatch = true;
    rc = sxLatchPixels(handle, CCD_EXP_FLAGS_FIELD_BOTH, 1, subX, subY, subW, subH, binX, binY);
    if (rc) {
      TRACE(fprintf(stderr, "   did guide latch\n"));
      rc = sxReadPixels(handle, (unsigned short *) buf, size);
      if (rc) {
        TRACE(fprintf(stderr, "   did guide read\n"));
      }
    }

    DidGuideLatch = false;
    InGuideExposure = false;
    GuideCCD.setExposureLeft(GuideExposureTimeLeft = 0);
    if (rc)
      ExposureComplete(&GuideCCD);
  }
  TRACE(fprintf(stderr, "<- SXCCD::GuideExposureTimerHit\n"));
}

bool SXCCD::GuideWest(float time) {
  TRACE(fprintf(stderr, "-> SXCCD::GuideWest(%f)\n", time));
  if (!HasSt4Port || time < 1) {
    TRACE(fprintf(stderr, "<- SXCCD::GuideWest 0\n"));
    return false;
  }
  if (WEGuiderTimerID) {
    IERmTimer(WEGuiderTimerID);
    WEGuiderTimerID = 0;
  }
  GuideStatus &= SX_CLEAR_WE;
  GuideStatus |= SX_GUIDE_WEST;
  sxSetSTAR2000(handle, GuideStatus);
  if (time < 100) {
    usleep(time * 1000);
    GuideStatus &= SX_CLEAR_WE;
    sxSetSTAR2000(handle, GuideStatus);
  } else
    WEGuiderTimerID = IEAddTimer(time, WEGuiderTimerCallback, this);
  TRACE(fprintf(stderr, "<- SXCCD::GuideWest 1\n"));
  return true;
}

bool SXCCD::GuideEast(float time) {
  TRACE(fprintf(stderr, "-> SXCCD::GuideEast(%f)\n", time));
  if (!HasSt4Port || time < 1) {
    TRACE(fprintf(stderr, "<- SXCCD::GuideEast 0\n"));
    return false;
  }
  if (WEGuiderTimerID) {
    IERmTimer(WEGuiderTimerID);
    WEGuiderTimerID = 0;
  }
  GuideStatus &= SX_CLEAR_WE;
  GuideStatus |= SX_GUIDE_EAST;
  sxSetSTAR2000(handle, GuideStatus);
  if (time < 100) {
    usleep(time * 1000);
    GuideStatus &= SX_CLEAR_WE;
    sxSetSTAR2000(handle, GuideStatus);
  } else
    WEGuiderTimerID = IEAddTimer(time, WEGuiderTimerCallback, this);
  TRACE(fprintf(stderr, "<- SXCCD::GuideEast 1\n"));
  return true;
}

void SXCCD::WEGuiderTimerHit() {
  TRACE(fprintf(stderr, "-> SXCCD::WEGuiderTimerHit()\n"));
  GuideStatus &= SX_CLEAR_WE;
  sxSetSTAR2000(handle, GuideStatus);
  WEGuiderTimerID = 0;
  TRACE(fprintf(stderr, "<- SXCCD::WEGuiderTimerHit\n"));
}

bool SXCCD::GuideNorth(float time) {
  TRACE(fprintf(stderr, "-> SXCCD::GuideNorth(%f)\n", time));
  if (!HasSt4Port || time < 1) {
    TRACE(fprintf(stderr, "<- SXCCD::GuideNorth 0\n"));
    return false;
  }
  if (NSGuiderTimerID) {
    IERmTimer(NSGuiderTimerID);
    NSGuiderTimerID = 0;
  }
  GuideStatus &= SX_CLEAR_NS;
  GuideStatus |= SX_GUIDE_NORTH;
  sxSetSTAR2000(handle, GuideStatus);
  if (time < 100) {
    usleep(time * 1000);
    GuideStatus &= SX_CLEAR_NS;
    sxSetSTAR2000(handle, GuideStatus);
  } else
    NSGuiderTimerID = IEAddTimer(time, NSGuiderTimerCallback, this);
  TRACE(fprintf(stderr, "<- SXCCD::GuideNorth 1\n"));
  return true;
}

bool SXCCD::GuideSouth(float time) {
  TRACE(fprintf(stderr, "-> SXCCD::GuideSouth(%f)\n", time));
  if (!HasSt4Port || time < 1) {
    TRACE(fprintf(stderr, "<- SXCCD::GuideSouth 0\n"));
    return false;
  }
  if (NSGuiderTimerID) {
    IERmTimer(NSGuiderTimerID);
    NSGuiderTimerID = 0;
  }
  GuideStatus &= SX_CLEAR_NS;
  GuideStatus |= SX_GUIDE_SOUTH;
  sxSetSTAR2000(handle, GuideStatus);
  if (time < 100) {
    usleep(time * 1000);
    GuideStatus &= SX_CLEAR_NS;
    sxSetSTAR2000(handle, GuideStatus);
  } else
    NSGuiderTimerID = IEAddTimer(time, NSGuiderTimerCallback, this);
  TRACE(fprintf(stderr, "<- SXCCD::GuideSouth 1\n"));
  return true;
}

void SXCCD::NSGuiderTimerHit() {
  TRACE(fprintf(stderr, "-> SXCCD::NSGuiderTimerHit()\n"));
  GuideStatus &= SX_CLEAR_NS;
  sxSetSTAR2000(handle, GuideStatus);
  NSGuiderTimerID = 0;
  TRACE(fprintf(stderr, "<- SXCCD::NSGuiderTimerHit\n"));
}

void SXCCD::ISGetProperties(const char *dev) {
  TRACE(fprintf(stderr, "-> SXCCD::ISGetProperties(%s)\n", name));
  INDI::CCD::ISGetProperties(name);
  addDebugControl();
  TRACE(fprintf(stderr, "<- SXCCD::ISGetProperties\n"));
}

bool SXCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {
  TRACE(fprintf(stderr, "-> SXCCD::ISNewSwitch(%s, %s, ...)\n", dev, name));
  bool result = false;
  if (strcmp(name, ShutterSP.name) == 0) {
    IUUpdateSwitch(&ShutterSP, states, names, n);
    ShutterSP.s = IPS_OK;
    IDSetSwitch(&ShutterSP, NULL);
    sxSetShutter(handle, ShutterS[0].s != ISS_ON);
    result = true;
  } else if (strcmp(name, CoolerSP.name) == 0) {
    IUUpdateSwitch(&CoolerSP, states, names, n);
    CoolerSP.s = IPS_OK;
    IDSetSwitch(&CoolerSP, NULL);
    unsigned char status;
    unsigned short temperature;
    sxSetCooler(handle, (unsigned char) (CoolerS[1].s == ISS_ON), (unsigned short) (TemperatureRequest * 10 + 2730), &status, &temperature);
    TemperatureReported = TemperatureN.value = (temperature - 2730) / 10.0;
    TemperatureNP.s = IPS_OK;
    IDSetNumber(&TemperatureNP, NULL);
    result = true;
  } else
    result = INDI::CCD::ISNewSwitch(dev, name, states, names, n);

  TRACE(fprintf(stderr, "<- SXCCD::ISNewSwitch %d\n", result));
  return result;
}

bool SXCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
  TRACE(fprintf(stderr, "-> SXCCD::ISNewNumber(%s, %s, ...)\n", dev, name));
  bool result = false;

  if (strcmp(name, TemperatureNP.name) == 0) {
    IUUpdateNumber(&TemperatureNP, values, names, n);
    TemperatureRequest = TemperatureN.value;
    unsigned char status;
    unsigned short temperature;
    sxSetCooler(handle, (unsigned char) (CoolerS[1].s == ISS_ON), (unsigned short) (TemperatureRequest * 10 + 2730), &status, &temperature);
    TemperatureReported = TemperatureN.value = (temperature - 2730) / 10.0;
    if (abs(TemperatureRequest - TemperatureReported) < 1)
      TemperatureNP.s = IPS_OK;
    else
      TemperatureNP.s = IPS_BUSY;
    IDSetNumber(&TemperatureNP, NULL);
    CoolerSP.s = IPS_OK;
    CoolerS[0].s = ISS_OFF;
    CoolerS[1].s = ISS_ON;
    IDSetSwitch(&CoolerSP, NULL);
    result = true;
  } else
    result = INDI::CCD::ISNewNumber(dev, name, values, names, n);

  TRACE(fprintf(stderr, "<- SXCCD::ISNewNumber %d\n", result));
  return result;
}

