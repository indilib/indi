/*******************************************************************************
  Copyright(c) 2014 CloudMakers, s. r. o. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "agent_imager.h"

std::auto_ptr<Imager> imager(0);

void ISPoll(void *p);


void ISInit() {
  static int isInit =0;
  if (isInit == 1)
    return;
  isInit = 1;
  if(imager.get() == 0) imager.reset(new Imager());
}

void ISGetProperties(const char *dev) {
  ISInit();
  imager->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
  ISInit();
  imager->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num) {
  ISInit();
  imager->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
  ISInit();
  imager->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {
  ISInit();
  imager->ISNewBLOB (dev, name, sizes, blobsizes, blobs, formats, names, n);
}
void ISSnoopDevice (XMLEle *root) {
  ISInit();
  imager->ISSnoopDevice(root);
}

Imager::Imager() {

  // TBD
  
}

const char *Imager::getDefaultName() {
  return "Imager";
}

bool Imager::initProperties() {
  INDI::DefaultDevice::initProperties();
  
  // TBD
  
  return true;
}

bool Imager::updateProperties() {

  // TBD
  
  return false;
}

void Imager::ISGetProperties (const char *dev) {
  DefaultDevice::ISGetProperties(dev);

  // TBD
  
}

bool Imager::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) {

  // TBD
  
  return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool Imager::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) {

  // TBD
  
  return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool Imager::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) {

  // TBD
  
  return INDI::DefaultDevice::ISNewText(dev,name,texts,names,n);
}

bool Imager::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {

  // TBD
  
  return INDI::DefaultDevice::ISNewBLOB (dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool Imager::ISSnoopDevice (XMLEle *root) {

  // TBD
  
  return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool Imager::Connect() {

  // TBD
  
  return true;
}

bool Imager::Disconnect() {

  // TBD
  
  return true;
}

Imager::~Imager() {
}