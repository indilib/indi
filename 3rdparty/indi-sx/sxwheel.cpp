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
  ISInit();
  sxwheel->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  ISInit();
  sxwheel->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) {
  ISInit();
  sxwheel->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  ISInit();
  sxwheel->ISNewNumber(dev, name, values, names, num);
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
  ISInit();
  sxwheel->ISSnoopDevice(root);
}

SXWHEEL::SXWHEEL() {
  FilterSlotN[0].min = 1;
  FilterSlotN[0].max = -1;
  CurrentFilter = 1;
  handle = 0;
  setDeviceName(getDefaultName());
  setVersion(VERSION_MAJOR, VERSION_MINOR);
}

SXWHEEL::~SXWHEEL() {
  if (isSimulation())
    IDMessage(getDeviceName(), "simulation: disconnected");
  else { 
    if (handle)
      hid_close(handle);
    hid_exit();
  }
}

void SXWHEEL::debugTriggered(bool enable) {
  INDI_UNUSED(enable);
}

void SXWHEEL::simulationTriggered(bool enable) {
  INDI_UNUSED(enable);
}

const char * SXWHEEL::getDefaultName() {
  return (char *) "SX Wheel";
}

bool SXWHEEL::GetFilterNames(const char* groupName) {
  char filterName[MAXINDINAME];
  char filterLabel[MAXINDILABEL];
  int MaxFilter = FilterSlotN[0].max;
  if (FilterNameT != NULL)
      delete FilterNameT;
  FilterNameT = new IText[MaxFilter];
  for (int i=0; i < MaxFilter; i++) {
    snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i+1);
    snprintf(filterLabel, MAXINDILABEL, "Filter #%d", i+1);
    IUFillText(&FilterNameT[i], filterName, filterLabel, filterLabel);
  }
  IUFillTextVector(FilterNameTP, FilterNameT, MaxFilter, getDeviceName(), "FILTER_NAME", "Filter", groupName, IP_RW, 0, IPS_IDLE);
  return true;
}

bool SXWHEEL::Connect() {
  if (isSimulation()) {
    IDMessage(getDeviceName(), "simulation: connected");
    handle = (hid_device *)1;
  }
  else {
    if (!handle)
      handle = hid_open(0x1278, 0x0920, NULL);
    SelectFilter(CurrentFilter);
  }
  return handle != NULL;
}

bool SXWHEEL::Disconnect() {
  if (isSimulation())
    IDMessage(getDeviceName(), "simulation: disconnected");
  else { 
    if (handle)
      hid_close(handle);
  }
  handle = 0;
  return true;
}

bool SXWHEEL::initProperties() {
  INDI::FilterWheel::initProperties();
  addDebugControl();
  addSimulationControl();
  return true;
}

void SXWHEEL::ISGetProperties(const char *dev) {
  INDI::FilterWheel::ISGetProperties(dev);
  return;
}

int SXWHEEL::SendWheelMessage(int a, int b) {
  if (isSimulation()) {
    IDMessage(getDeviceName(), "simulation: command %d %d", a, b);
    if (a > 0)
      CurrentFilter = a;
    return 0;
  }
  if (!handle) {
    IDMessage(getDeviceName(), "Filter wheel not connected\n");
    return -1;
  }
  unsigned char buf[2] = { a, b };
  int rc = hid_write(handle, buf, 2);
  DEBUGF(INDI::Logger::DBG_DEBUG, "SendWheelMessage: hid_write( { %d, %d } ) -> %d\n", buf[0], buf[1], rc);
  if (rc != 2) {
    IDMessage(getDeviceName(), "Failed to write to wheel \n");
    return -1;
  }
  usleep(100);
  rc = hid_read(handle, buf, 2);
  DEBUGF(INDI::Logger::DBG_DEBUG, "SendWheelMessage: hid_read() -> { %d, %d } %d\n", buf[0], buf[1], rc);
  if (rc != 2) {
    IDMessage(getDeviceName(), "Failed to read from wheel\n");
    return -1;
  }
  CurrentFilter = buf[0];
  FilterSlotN[0].max = buf[1];
  return 0;
}

int SXWHEEL::QueryFilter() {
  SendWheelMessage(0, 0);
  return CurrentFilter;
}

bool SXWHEEL::SelectFilter(int f) {
  TargetFilter = f;
  SendWheelMessage(f, 0);
  SetTimer(250);
  return true;
}

void SXWHEEL::TimerHit() {
  QueryFilter();
  if (CurrentFilter != TargetFilter) {
    SetTimer(250);
  } else {
    SelectFilterDone (CurrentFilter);
  }
}
