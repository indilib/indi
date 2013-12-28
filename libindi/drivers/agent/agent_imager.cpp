/*******************************************************************************
  Copyright(c) 2013 CloudMakers, s. r. o. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "agent_imager.h"

#define DEVICE_NAME   "Imager"
#define DOWNLOAD_TAB  "Download images"

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
  ActiveDeviceTP = new ITextVectorProperty;  
  GroupCountNP = new INumberVectorProperty;
  ProgressNP = new INumberVectorProperty;
  ExecuteSP = new ISwitchVectorProperty;
  DownloadNP = new INumberVectorProperty;
  FitsBP = new IBLOBVectorProperty;
  for (int i = 0; i < MAX_GROUP_COUNT; i++)
    group[i] = new Group(i);
}

const char *Imager::getDefaultName() {
  return DEVICE_NAME;
}

void Imager::defineProperties() {
  defineText(ActiveDeviceTP);
  defineNumber(GroupCountNP);
  if (isConnected()) {
    defineNumber(ProgressNP);
    defineSwitch(ExecuteSP);
    defineNumber(DownloadNP);
    defineBLOB(FitsBP);
  }
}

void Imager::makeReadOnly() {
  GroupCountNP->p = IP_RO;
  defineProperties();
  for (int i = 0; i < GroupCountN[0].value; i++) {
    group[i]->makeReadOnly();
  }
}

void Imager::makeReadWrite() {
  GroupCountNP->p = IP_RW;
  defineProperties();
  for (int i = 0; i < GroupCountN[0].value; i++) {
    group[i]->makeReadWrite();
  }
}

void Imager::runBatch() {
  ProgressNP->s = IPS_BUSY;
  makeReadOnly();
  
  // TBD
  
  ProgressN[0].value = 1;
  ProgressN[1].value = 1;
  IDSetNumber(ProgressNP, NULL);
}

void Imager::stopBatch() {
  
  // TBD
  
  ProgressNP->s = IPS_OK;
  makeReadWrite();
}

bool Imager::initProperties() {
  INDI::DefaultDevice::initProperties();
  
  IUFillText(&ActiveDeviceT[0], "ACTIVE_CCD", "CCD" ,"Telescope Simulator");
  IUFillText(&ActiveDeviceT[1], "ACTIVE_FOCUSER", "Focuser", "Focuser Simulator");
  IUFillTextVector(ActiveDeviceTP, ActiveDeviceT, 2, DEVICE_NAME, "ACTIVE_DEVICES", "Snoop devices", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
  
  IUFillNumber(&GroupCountN[0], "GROUP_COUNT", "Image group count", "%3.0f", 1, MAX_GROUP_COUNT, 1, 1);
  IUFillNumberVector(GroupCountNP, GroupCountN, 1, DEVICE_NAME, "GROUP_COUNT", "Image groups", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
  
  IUFillNumber(&ProgressN[0], "CURRENT_GROUP", "Current group", "%3.0f", 0, MAX_GROUP_COUNT, 1, 0);
  IUFillNumber(&ProgressN[1], "CURRENT_IMAGE", "Current image", "%3.0f", 0, 100, 1, 0);
  IUFillNumberVector(ProgressNP, ProgressN, 2, DEVICE_NAME, "PROGRESS", "Batch execution progress", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
  
  IUFillSwitch(&ExecuteS[0], "RUN", "Run batch", ISS_OFF);
  IUFillSwitch(&ExecuteS[1], "STOP", "Stop batch", ISS_ON);
  IUFillSwitchVector(ExecuteSP, ExecuteS, 2, DEVICE_NAME, "EXECUTE", "Execute batch", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

  IUFillNumber(&DownloadN[0], "DOWNLOAD_GROUP", "Group", "%3.0f", 1, MAX_GROUP_COUNT, 1, 1);
  IUFillNumber(&DownloadN[1], "DOWNLOAD_IMAGE", "Image", "%3.0f", 1, 100, 1, 1);
  IUFillNumberVector(DownloadNP, DownloadN, 2, DEVICE_NAME, "DOWNLOAD", "Download image", DOWNLOAD_TAB, IP_RW, 60, IPS_IDLE);
  
  IUFillBLOB(&FitsB[0], "IMAGE", "Image", "");
  IUFillBLOBVector(FitsBP, FitsB, 1, DEVICE_NAME, "IMAGE", "Image Data", DOWNLOAD_TAB, IP_RO, 60, IPS_IDLE);

  defineProperties();
  for (int i = 0; i < GroupCountN[0].value; i++) {
    group[i]->defineProperties();
  }
  return true;
}

bool Imager::updateProperties() {
  if (isConnected()) {
    ProgressN[0].value = 0;
    ProgressN[1].value = 0;
    ProgressNP->s = IPS_IDLE;
    defineNumber(ProgressNP);
    ExecuteS[0].s = ISS_OFF;
    ExecuteS[1].s = ISS_ON;
    ExecuteSP->s = IPS_IDLE;
    defineSwitch(ExecuteSP);
    DownloadN[0].value = 0;
    DownloadN[1].value = 0;
    DownloadNP->s = IPS_IDLE;
    defineNumber(DownloadNP);
    FitsBP->s = IPS_IDLE;
    defineBLOB(FitsBP);
  } else {
    stopBatch();
    IDSetNumber(ProgressNP, NULL);
    deleteProperty(ProgressNP->name);
    deleteProperty(ExecuteSP->name);
    deleteProperty(DownloadNP->name);
  }
  return true;
}

void Imager::ISGetProperties(const char *dev) {
  DefaultDevice::ISGetProperties(dev);
}

bool Imager::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
  if (strcmp(dev, getDeviceName()) == 0) {
    if (strcmp(name, "GROUP_COUNT") == 0) {
      float value =  GroupCountN[0].value;
      for (int i = 0; i < value; i++)
        group[i]->deleteProperties();
      GroupCountN[0].value = value =  values[0];
      for (int i = 0; i < value; i++)
        group[i]->defineProperties();
      IDSetNumber(GroupCountNP, NULL);
      return true;
    } else if (strncmp(name, "IMAGE_PROPERTIES_", 17) == 0) {
      for (int i = 0; i < GroupCountN[0].value; i++)
        if (group[i]->ISNewNumber(dev, name, values, names, n))
          return true;
      return false;
    }
  }
  return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool Imager::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {
  if (strcmp(dev, getDeviceName()) == 0) {
    if (strcmp(name, "EXECUTE") == 0) {
      IUUpdateSwitch(ExecuteSP, states, names, n);
      if (ExecuteS[0].s == ISS_ON)
        runBatch();
      else
        stopBatch();
      IDSetSwitch(ExecuteSP, NULL);
      return true;
    }
  }
  return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool Imager::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) {
  return INDI::DefaultDevice::ISNewText(dev,name,texts,names,n);
}

bool Imager::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {

  // TBD
  
  return INDI::DefaultDevice::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool Imager::ISSnoopDevice(XMLEle *root) {

  // TBD
  
  return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool Imager::Connect() {
  IDSnoopDevice(NULL, NULL);
  return true;
}

bool Imager::Disconnect() {
  return true;
}

Imager::~Imager() {
}

Group::Group(int id) {
  this->id = id;
  sprintf(groupName, "Image group %d", id);
  GroupSettingsNP = new INumberVectorProperty;
  sprintf(groupSettingsName, "IMAGE_PROPERTIES_%02d", id);
  IUFillNumber(&GroupSettingsN[0], "IMAGE_COUNT", "Image count", "%3.0f", 1, 100, 1, 1);
  IUFillNumber(&GroupSettingsN[1], "CCD_BINNING", "Binning","%1.0f", 1, 4, 1, 1);
  IUFillNumber(&GroupSettingsN[2], "FILTER_SLOT", "Filter", "%2.f", 1, 12, 1, 1);
  IUFillNumber(&GroupSettingsN[3], "CCD_EXPOSURE_VALUE", "Duration (s)","%5.2f", 0, 36000, 0, 1.0);
  IUFillNumberVector(GroupSettingsNP, GroupSettingsN, 4, DEVICE_NAME, groupSettingsName, "Image group settings", groupName, IP_RW, 60, IPS_IDLE);
}

bool Group::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
  if (strcmp(name, groupSettingsName) == 0) {
    IUUpdateNumber(GroupSettingsNP, values, names, n);
    IDSetNumber(GroupSettingsNP, NULL);
    return true;
  }
  return false;
}


void Group::defineProperties() {
  imager->defineNumber(GroupSettingsNP);
}

void Group::deleteProperties() {
  imager->deleteProperty(GroupSettingsNP->name);
}

void Group::makeReadOnly() {
  GroupSettingsNP->p = IP_RO;
  defineProperties();
}

void Group::makeReadWrite() {
  GroupSettingsNP->p = IP_RW;
  defineProperties();
}
