/*
 Starlight Xpress Filter Wheel INDI Driver

 Copyright (c) 2012 Cloudmakers, s. r. o.
 All Rights Reserved.

 Code is based on SX Filter Wheel INDI Driver by Gerry Rozema
 Copyright(c) 2010 Gerry Rozema.
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
#include <memory>
#include <unistd.h>

#include "sxconfig.h"
#include "sxwheel.h"

#define TRACE(c) (c)
#define DEBUG(c) (c)

std::auto_ptr<SXWHEEL> sxwheel(0);

void ISInit() {
  static bool isInit = false;

  if (!isInit) {
    isInit = 1;
    if (!sxwheel.get())
      sxwheel.reset(new SXWHEEL());
  }
}

void ISGetProperties(const char *dev) {
  TRACE(fprintf(stderr, "-> ISGetProperties(%s)\n", dev));
  ISInit();
  sxwheel->ISGetProperties(dev);
  TRACE(fprintf(stderr, "<- ISGetProperties\n"));
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  TRACE(fprintf(stderr, "-> ISNewSwitch(%s, %s...)\n", dev, name));
  ISInit();
  sxwheel->ISNewSwitch(dev, name, states, names, num);
  TRACE(fprintf(stderr, "<- ISNewSwitch\n"));
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) {
  TRACE(fprintf(stderr, "-> ISNewText(%s, %s, ...)\n", dev, name));
  ISInit();
  sxwheel->ISNewText(dev, name, texts, names, num);
  TRACE(fprintf(stderr, "<- ISNewText\n"));
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  TRACE(fprintf(stderr, "-> ISNewNumber(%s, %s, ...)\n", dev, name));
  ISInit();
  sxwheel->ISNewNumber(dev, name, values, names, num);
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

SXWHEEL::SXWHEEL() {
  TRACE(fprintf(stderr, "-> SXWHEEL::SXWHEEL()\n"));
  FilterSlotN[0].min = 1;
  FilterSlotN[0].max = -1;
  CurrentFilter = 1;
  handle = 0;
  setDeviceName(getDefaultName());
  setVersion(VERSION_MAJOR, VERSION_MINOR);
  TRACE(fprintf(stderr, "<- SXWHEEL::SXWHEEL\n"));
}

SXWHEEL::~SXWHEEL() {
  TRACE(fprintf(stderr, "-> SXWHEEL::~SXWHEEL()\n"));
  if (handle)
    hid_close(handle);
  hid_exit();
  TRACE(fprintf(stderr, "<- SXWHEEL::~SXWHEEL\n"));
}

const char * SXWHEEL::getDefaultName() {
  return (char *) "SX Wheel";
}

bool SXWHEEL::Connect() {
  TRACE(fprintf(stderr, "-> SXWHEEL::Connect()\n"));
  if (!handle)
    handle = hid_open(0x1278, 0x0920, NULL);
  SelectFilter(CurrentFilter);
  TRACE(fprintf(stderr, "<- SXWHEEL::Connect %d\n", handle != NULL));
  return handle != NULL;
}

bool SXWHEEL::Disconnect() {
  TRACE(fprintf(stderr, "-> SXWHEEL::Disconnect()\n"));
  if (handle)
    hid_close(handle);
  handle = 0;
  TRACE(fprintf(stderr, "<- SXWHEEL::Disconnect 1\n"));
  return true;
}

bool SXWHEEL::initProperties() {
  TRACE(fprintf(stderr, "-> SXWHEEL::initProperties()\n"));
  INDI::FilterWheel::initProperties();
  TRACE(fprintf(stderr, "<- SXWHEEL::initProperties 1"));
  return true;
}

void SXWHEEL::ISGetProperties(const char *dev) {
  TRACE(fprintf(stderr, "-> SXWHEEL::ISGetProperties()\n"));
  INDI::FilterWheel::ISGetProperties(dev);
  TRACE(fprintf(stderr, "<- SXWHEEL::ISGetProperties\n"));
  return;
}

int SXWHEEL::SendWheelMessage(int a, int b) {
  TRACE(fprintf(stderr, "-> SXWHEEL::SendWheelMessage(%d, %d)\n", a, b));

  if (!handle) {
    IDMessage(getDeviceName(), "Filter wheel not connected\n");
    TRACE(fprintf(stderr, "<- SXWHEEL::SendWheelMessage -1\n"));
    return -1;
  }

  unsigned char buf[2] = { a, b };

  int rc = hid_write(handle, buf, 2);
  if (rc != 2) {
    IDMessage(getDeviceName(), "Failed to write to wheel \n");
    TRACE(fprintf(stderr, "<- SXWHEEL::SendWheelMessage (rc=%d) -1\n", rc));
    return -1;
  }

  usleep(100);

  rc = hid_read(handle, buf, 2);
  if (rc != 2) {
    IDMessage(getDeviceName(), "Failed to read from wheel\n");
    TRACE(fprintf(stderr, "<- SXWHEEL::SendWheelMessage (rc=%d) -1\n", rc));
    return -1;
  }

  CurrentFilter = buf[0];
  FilterSlotN[0].max = buf[1];

  TRACE(fprintf(stderr, "<- SXWHEEL::SendWheelMessage 0\n"));

  return 0;
}

int SXWHEEL::QueryFilter() {
  TRACE(fprintf(stderr, "-> SXWHEEL::QueryFilter()\n"));
  SendWheelMessage(0, 0);
  TRACE(fprintf(stderr, "<- SXWHEEL::QueryFilter %d\n", CurrentFilter));
  return CurrentFilter;
}

bool SXWHEEL::SelectFilter(int f) {
  TRACE(fprintf(stderr, "-> SXWHEEL::SelectFilter(%d)\n", f));
  TargetFilter = f;
  SendWheelMessage(f, 0);
  SetTimer(250);
  TRACE(fprintf(stderr, "<- SXWHEEL::SelectFilter 1\n"));
  return true;
}

bool SXWHEEL::GetFilterNames(const char *, const char *) {
  return false;
}

void SXWHEEL::TimerHit() {
  QueryFilter();
  if (CurrentFilter != TargetFilter) {
    SetTimer(250);
  } else {
    SelectFilterDone (CurrentFilter);
  }
}
