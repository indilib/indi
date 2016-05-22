/*
  SSAG CCD INDI Driver

  Copyright (c) 2016 Peter Polakovic, CloudMakers, s. r. o.
  All Rights Reserved.

  Code is based on OpenSSAG modified for libusb 1.0
  Copyright (c) 2011 Eric J. Holmes, Orion Telescopes & Binoculars
  All rights reserved.
 
  https://github.com/ejholmes/openssag

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
#include <stdint.h>
#include <math.h>

#include "ssagccd.h"

std::unique_ptr<SSAGCCD> camera(new SSAGCCD());

void ISGetProperties(const char *dev) {
  camera->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  camera->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num) {
  camera->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  camera->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root) {
  camera->ISSnoopDevice(root);
}

SSAGCCD::SSAGCCD() {
  ssag = new OpenSSAG::SSAG();
}

const char *SSAGCCD::getDefaultName() {
  return "SSAG CCD";
}

bool SSAGCCD::initProperties() {
  INDI::CCD::initProperties();
  addDebugControl();
}

bool SSAGCCD::updateProperties() {
  INDI::CCD::updateProperties();
}

bool SSAGCCD::Connect() {
  if (isConnected())
    return true;
  if (!ssag->Connect()) {
    IDMessage(getDeviceName(), "Failed to connect to SSAG");
    return false;
  }
  setConnected(true);
  SetCCDCapability(CCD_HAS_ST4_PORT);
  PrimaryCCD.setInterlaced(false);
  SetCCDParams(1280, 1024, 8, 5.2, 5.2);
  PrimaryCCD.setFrameBufferSize(1280*1024+512);
  return true;
}

bool SSAGCCD::StartExposure(float duration) {
  ExposureTime = duration;
  InExposure = true;
  PrimaryCCD.setExposureDuration(duration);
  struct OpenSSAG::raw_image *image = ssag->Expose(duration*1000);
  PrimaryCCD.setExposureLeft(0);
  if (image) {
    PrimaryCCD.setFrame(0, 0, image->width, image->height);
    unsigned char* imgBuf =  (unsigned char *)PrimaryCCD.getFrameBuffer();
    memcpy(imgBuf, image->data, image->width*image->height);
    ssag->FreeRawImage(image);
    InExposure = false;
    ExposureComplete(&PrimaryCCD);
    return true;
  }
  return false;
}

bool SSAGCCD::AbortExposure() {
  return true;
}

IPState SSAGCCD::GuideWest(float time) {
  ssag->Guide(OpenSSAG::guide_west, (int)time);
  return IPS_OK;
}

IPState SSAGCCD::GuideEast(float time) {
  ssag->Guide(OpenSSAG::guide_east, (int)time);
  return IPS_OK;
}

IPState SSAGCCD::GuideNorth(float time) {
  ssag->Guide(OpenSSAG::guide_north, (int)time);
  return IPS_OK;
}

IPState SSAGCCD::GuideSouth(float time) {
  ssag->Guide(OpenSSAG::guide_south, (int)time);
  return IPS_OK;
}

bool SSAGCCD::Disconnect() {
  if (isConnected()) {
    ssag->Disconnect();
  }
  setConnected(false);
  return true;
}

