/*
 QHY CCD INDI Driver

 Copyright (c) 2013 Cloudmakers, s. r. o.
 All Rights Reserved.

 Code is based on QHY INDI Driver by Jasem Mutlaq
 Copyright(c) 2012 Jasem Mutlaq. All rights reserved.

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
#include <unistd.h>
#include <memory.h>

#include "qhyconfig.h"
#include "qhyccd.h"

static int count;
static QHYCCD *cameras[20];

#define TIMER 1000

void cleanup() {
  for (int i = 0; i < count; i++) {
    delete cameras[i];
  }
}

void ISInit() {
  static bool isInit = false;
  if (!isInit) {
    QHYDevice *devices[20];
    count = QHYDevice::list(devices, 20);
    for (int i = 0; i < count; i++) {
      cameras[i] = new QHYCCD(devices[i]);
    }
    atexit(cleanup);
    isInit = true;
  }
}

void ISGetProperties(const char *dev) {
  ISInit();
  for (int i = 0; i < count; i++) {
    QHYCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISGetProperties(camera->name);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  ISInit();
  for (int i = 0; i < count; i++) {
    QHYCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewSwitch(camera->name, name, states, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) {
  ISInit();
  for (int i = 0; i < count; i++) {
    QHYCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewText(camera->name, name, texts, names, num);
      if (dev != NULL)
        break;
    }
  }
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  ISInit();
  for (int i = 0; i < count; i++) {
    QHYCCD *camera = cameras[i];
    if (dev == NULL || !strcmp(dev, camera->name)) {
      camera->ISNewNumber(camera->name, name, values, names, num);
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

void ISSnoopDevice(XMLEle *root)
{
    ISInit();
    const char* dev = findXMLAttValu(root, "device");
    for (int i = 0; i < count; i++) {
      QHYCCD *camera = cameras[i];
      if (dev == NULL || !strcmp(dev, camera->name)) {
        camera->ISSnoopDevice(root);
        if (dev != NULL)
          break;
      }
    }

}

void ExposureTimerCallback(void *p) {
  ((QHYCCD *) p)->ExposureTimerHit();
}

QHYCCD::QHYCCD(QHYDevice *device) {
  this->device = device;
  ExposureTimeLeft = 0.0;
  ExposureTimerID = 0;
  snprintf(this->name, 32, "QHY CCD %s", device->getName());
  setDeviceName(this->name);
  setVersion(VERSION_MAJOR, VERSION_MINOR);
}

QHYCCD::~QHYCCD() {
  if (isConnected())
    device->close();
}

void QHYCCD::debugTriggered(bool enable) {
  INDI_UNUSED(enable);
}

void QHYCCD::simulationTriggered(bool enable) {
  INDI_UNUSED(enable);
}

const char *QHYCCD::getDefaultName() {
  return name;
}

bool QHYCCD::initProperties() {
  INDI::CCD::initProperties();
  IUFillNumber(&GainN[0], "GAIN", "Gain", "%0.f", 1., 100, 1., 1.);
  IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

  // JM: Peter please check this. Do we know this here, or after we connected?
  CCDCapability cap;

  cap.canAbort = true;
  cap.canBin = true;
  cap.canSubFrame = true;
  cap.hasCooler = device->hasCooler();
  cap.hasGuideHead = false;
  cap.hasShutter = device->hasShutter();
  cap.hasST4Port = device->hasGuidePort();

  SetCCDCapability(&cap);

  return true;
}

bool QHYCCD::updateProperties() {
  INDI::CCD::updateProperties();
  if (isConnected()) {
    float pixelSizeX, pixelSizeY;
    unsigned pixelCountX, pixelCountY, bitsPerPixel, maxBinX, maxBinY;
    device->getParameters(&pixelCountX, &pixelCountY, &pixelSizeX, &pixelSizeY, &bitsPerPixel, &maxBinX, &maxBinY);
    GainN[0].value = 100;
    defineNumber(&GainNP);
    SetCCDParams(pixelCountX, pixelCountY, bitsPerPixel, pixelSizeX, pixelSizeY);
    PrimaryCCD.setFrameBufferSize(pixelCountX * pixelCountY * bitsPerPixel);
  }
  return true;
}

bool QHYCCD::UpdateCCDBin(int hor, int ver) {
  float pixelSizeX, pixelSizeY;
  unsigned pixelCountX, pixelCountY, bitsPerPixel, maxBinX, maxBinY;
  device->getParameters(&pixelCountX, &pixelCountY, &pixelSizeX, &pixelSizeY, &bitsPerPixel, &maxBinX, &maxBinY);
  if (hor > maxBinY || ver > maxBinX) {
    IDMessage(getDeviceName(), "Binning is not supported.");
    return false;
  }
  PrimaryCCD.setBin(hor, ver);
  return true;
}

bool QHYCCD::UpdateCCDFrame(int x, int y, int w, int h) {
  return device->setParameters(x, y, w, h, 100);
}

bool QHYCCD::Connect() {
  if (!isConnected()) {
    if (device->open()) {
      return device->reset();
    }
  }
  return false;
}

bool QHYCCD::Disconnect() {
  if (isConnected()) {
    device->close();
  }
  return true;
}

int QHYCCD::SetTemperature(double temperature)
{
    // FIXME TODO
    return -1;

}

void QHYCCD::TimerHit() {
  if (InExposure && ExposureTimeLeft >= 0)
    PrimaryCCD.setExposureLeft(ExposureTimeLeft--);
  if (isConnected())
    SetTimer(TIMER);
}

bool QHYCCD::StartExposure(float n) {
  InExposure = true;
  PrimaryCCD.setExposureDuration(n);
  device->startExposure(n);
  int time = (int) (1000 * n);
  if (time < 1)
    time = 1;
  ExposureTimeLeft = n;
  ExposureTimerID = IEAddTimer(time, ExposureTimerCallback, this);
  return true;
}

bool QHYCCD::AbortExposure() {
  if (InExposure) {
    if (ExposureTimerID)
      IERmTimer(ExposureTimerID);
    ExposureTimerID = 0;
    PrimaryCCD.setExposureLeft(ExposureTimeLeft = 0);
    InExposure = false;
    return true;
  }
  return false;
}

void QHYCCD::ExposureTimerHit() {
  if (InExposure) {
    ExposureTimerID = 0;
    char *buf = PrimaryCCD.getFrameBuffer();
    bool done = device->readExposure(buf);
    InExposure = false;
    PrimaryCCD.setExposureLeft(ExposureTimeLeft = 0);
    if (done)
      ExposureComplete (&PrimaryCCD);
  }
}

bool QHYCCD::GuideWest(float time) {
  if (!HasST4Port() || time < 1) {
    return false;
  }
  return device->guidePulse(GUIDE_WEST, time);
}

bool QHYCCD::GuideEast(float time) {
  if (!HasST4Port() || time < 1) {
    return false;
  }
  return device->guidePulse(GUIDE_EAST, time);
}

bool QHYCCD::GuideNorth(float time) {
  if (!HasST4Port() || time < 1) {
    return false;
  }
  return device->guidePulse(GUIDE_NORTH, time);
}

bool QHYCCD::GuideSouth(float time) {
  if (!HasST4Port() || time < 1) {
    return false;
  }
  return device->guidePulse(GUIDE_SOUTH, time);
}

void QHYCCD::ISGetProperties(const char *dev) {
  INDI::CCD::ISGetProperties (name);
  addDebugControl();
}

bool QHYCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {
  return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool QHYCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
  if (!strcmp(dev, getDeviceName())) {
    if (!strcmp(name, "CCD_GAIN")) {
      GainNP.s = IPS_BUSY;
      IDSetNumber(&GainNP, NULL);
      IUUpdateNumber(&GainNP, values, names, n);
      device->setParameters(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), GainN[0].value);
      GainNP.s = IPS_OK;
      IDSetNumber(&GainNP, NULL);
      return true;
    }

  }
  return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

