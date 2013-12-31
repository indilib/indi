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

#include <memory>

#include "agent_imager.h"

#define DEVICE_NAME     "Imager"
#define DOWNLOAD_TAB    "Download images"

std::auto_ptr<Imager> imager(0);

// Driver entry points ----------------------------------------------------------------------------

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

void ISSnoopDevice(XMLEle *root) {
  ISInit();
  imager->ISSnoopDevice(root);
}

// Imager ----------------------------------------------------------------------------

Imager::Imager() {
  for (int i = 0; i < MAX_GROUP_COUNT; i++)
    group[i] = new Group(i);
}

const char *Imager::getDefaultName() {
  return DEVICE_NAME;
}

void Imager::defineProperties() {
  defineNumber(&GroupCountNP);
  defineText(&ControlledDeviceTP);
  if (isConnected()) {
    defineLight(&StatusLP);
    defineNumber(&ProgressNP);
    defineSwitch(&ExecuteSP);
    defineNumber(&DownloadNP);
    defineBLOB(&FitsBP);
  } else {
    deleteProperty(ProgressNP.name);
    deleteProperty(ExecuteSP.name);
    deleteProperty(StatusLP.name);
    deleteProperty(DownloadNP.name);
    deleteProperty(FitsBP.name);
  }
}

void Imager::makeReadOnly() {
  GroupCountNP.p = IP_RO;
  defineProperties();
  for (int i = 0; i < GroupCountN[0].value; i++) {
    group[i]->makeReadOnly();
  }
}

void Imager::makeReadWrite() {
  GroupCountNP.p = IP_RW;
  defineProperties();
  for (int i = 0; i < GroupCountN[0].value; i++) {
    group[i]->makeReadWrite();
  }
}

void Imager::initiateNextCapture() {

  IDSetNumber(&ProgressNP, NULL);

  int g = (int)ProgressN[0].value;
  int i = (int)ProgressN[1].value;
  int G = (int)GroupCountN[0].value;
  int I = (int)group[g - 1]->GroupSettingsN[0].value;

  if (g <= G && i <= I) {
    CCDImageExposureN[0].value = group[g - 1]->GroupSettingsN[3].value;
    sendNewNumber(&CCDImageExposureNP);
    IDLog("Group %d of %d, image %d of %d, %5.1fs, capture initiated on %s\n", g, G, i, I, CCDImageExposureN[0].value, CCDImageExposureNP.device);
  }
}

void Imager::runBatch() {
  IDLog("Batch started\n");
  ProgressNP.s = IPS_BUSY;
  makeReadOnly();
  ProgressN[0].value = 1;
  ProgressN[1].value = 1;
  initiateNextCapture();
}

void Imager::stopBatch() {
  
  // TBD
  
  ProgressNP.s = IPS_OK;
  ExecuteS[0].s = ISS_OFF;
  ExecuteS[1].s = ISS_ON;
  IDSetSwitch(&ExecuteSP, NULL);
  makeReadWrite();
}

Imager::~Imager() {
}

// DefaultDevice ----------------------------------------------------------------------------

bool Imager::initProperties() {
  INDI::DefaultDevice::initProperties();
  
  IUFillNumber(&GroupCountN[0], "GROUP_COUNT", "Image group count", "%3.0f", 1, MAX_GROUP_COUNT, 1, 1);
  IUFillNumberVector(&GroupCountNP, GroupCountN, 1, DEVICE_NAME, "GROUP_COUNT", "Image groups", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
  
  IUFillText(&ControlledDeviceT[0], "CONTROLLED_CCD", "CCD" ,"CCD Simulator");
  IUFillText(&ControlledDeviceT[1], "CONTROLLED_FILTER", "Filter wheel", "Filter Simulator");
  IUFillTextVector(&ControlledDeviceTP, ControlledDeviceT, 2, DEVICE_NAME, "CONTROLLED_DEVICES", "Controlled devices", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
  controlledCCD = ControlledDeviceT[0].text;
  controlledFilterWheel = ControlledDeviceT[1].text;
  
  IUFillLight(&StatusL[0], "CCD_STATUS", controlledCCD, IPS_IDLE);
  IUFillLight(&StatusL[1], "FILTER_STATUS", controlledFilterWheel, IPS_IDLE);
  IUFillLightVector(&StatusLP, StatusL, 2, DEVICE_NAME, "STATUS", "Controlled devices", MAIN_CONTROL_TAB, IPS_IDLE);
  
  IUFillNumber(&ProgressN[0], "CURRENT_GROUP", "Current group", "%3.0f", 0, MAX_GROUP_COUNT, 1, 0);
  IUFillNumber(&ProgressN[1], "CURRENT_IMAGE", "Current image", "%3.0f", 0, 100, 1, 0);
  IUFillNumberVector(&ProgressNP, ProgressN, 2, DEVICE_NAME, "PROGRESS", "Batch execution progress", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
  
  IUFillSwitch(&ExecuteS[0], "RUN", "Run batch", ISS_OFF);
  IUFillSwitch(&ExecuteS[1], "STOP", "Stop batch", ISS_ON);
  IUFillSwitchVector(&ExecuteSP, ExecuteS, 2, DEVICE_NAME, "EXECUTE", "Execute batch", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
  
  IUFillNumber(&DownloadN[0], "DOWNLOAD_GROUP", "Group", "%3.0f", 1, MAX_GROUP_COUNT, 1, 1);
  IUFillNumber(&DownloadN[1], "DOWNLOAD_IMAGE", "Image", "%3.0f", 1, 100, 1, 1);
  IUFillNumberVector(&DownloadNP, DownloadN, 2, DEVICE_NAME, "DOWNLOAD", "Download image", DOWNLOAD_TAB, IP_RW, 60, IPS_IDLE);
  
  IUFillBLOB(&FitsB[0], "IMAGE", "Image", "");
  IUFillBLOBVector(&FitsBP, FitsB, 1, DEVICE_NAME, "IMAGE", "Image Data", DOWNLOAD_TAB, IP_RO, 60, IPS_IDLE);
  
  defineProperties();
  for (int i = 0; i < GroupCountN[0].value; i++) {
    group[i]->defineProperties();
  }
  
  IUFillNumber(&CCDImageExposureN[0], "CCD_EXPOSURE_VALUE", "Duration (s)", "%5.2f", 0,36000, 0, 1.0);
  IUFillNumberVector(&CCDImageExposureNP, CCDImageExposureN, 1, ControlledDeviceT[0].text, "CCD_EXPOSURE", "Expose", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

  return true;
}

bool Imager::updateProperties() {
  if (isConnected()) {
    ProgressN[0].value = 0;
    ProgressN[1].value = 0;
    ProgressNP.s = IPS_IDLE;
    ExecuteS[0].s = ISS_OFF;
    ExecuteS[1].s = ISS_ON;
    ExecuteSP.s = IPS_IDLE;
    DownloadN[0].value = 0;
    DownloadN[1].value = 0;
    DownloadNP.s = IPS_IDLE;
    FitsBP.s = IPS_IDLE;
    ControlledDeviceTP.p = IP_RO;
  } else {
    ControlledDeviceTP.p = IP_RW;
  }
  defineProperties();
  return true;
}

void Imager::ISGetProperties(const char *dev) {
  DefaultDevice::ISGetProperties(dev);
}

bool Imager::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
  if (!strcmp(dev, DEVICE_NAME)) {
    if (!strcmp(name, GroupCountNP.name)) {
      float value =  GroupCountN[0].value;
      for (int i = 0; i < value; i++)
        group[i]->deleteProperties();
      GroupCountN[0].value = value =  values[0];
      for (int i = 0; i < value; i++)
        group[i]->defineProperties();
      IDSetNumber(&GroupCountNP, NULL);
      return true;
    } else if (strncmp(name, "IMAGE_PROPERTIES_", 17) == 0) {
      for (int i = 0; i < GroupCountN[0].value; i++)
        if (group[i]->ISNewNumber(dev, name, values, names, n)) {
          return true;
        }
      return false;
    }
  }
  return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool Imager::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) {
  if (!strcmp(dev, DEVICE_NAME)) {
    if (!strcmp(name, ExecuteSP.name)) {
      IUUpdateSwitch(&ExecuteSP, states, names, n);
      if (ExecuteS[0].s == ISS_ON)
        runBatch();
      else
        stopBatch();
      IDSetSwitch(&ExecuteSP, NULL);
      return true;
    }
  }
  return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool Imager::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) {
  if (!strcmp(dev, DEVICE_NAME)) {
    if (!strcmp(name, ControlledDeviceTP.name)) {
      IUUpdateText(&ControlledDeviceTP, texts, names, n);
      IDSetText(&ControlledDeviceTP, NULL);
      strcpy(StatusL[0].label, ControlledDeviceT[0].text);
      strcpy(CCDImageExposureNP.device, ControlledDeviceT[0].text);
      strcpy(StatusL[1].label, ControlledDeviceT[1].text);
    }
  }
  return INDI::DefaultDevice::ISNewText(dev,name,texts,names,n);
}

bool Imager::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {
  
  // TBD
  
  return INDI::DefaultDevice::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool Imager::ISSnoopDevice(XMLEle *root) {
  return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool Imager::Connect() {
  setServer("localhost", 7624); // TODO configuration options
  watchDevice(controlledCCD);
  watchDevice(controlledFilterWheel);
  connectServer();
  setBLOBMode(B_ALSO, controlledCCD, NULL);

  return true;
}

bool Imager::Disconnect() {
  stopBatch();
  disconnectServer();
  
  // TODO image files cleanup
  
  return true;
}

// BaseClient ----------------------------------------------------------------------------

void Imager::serverConnected() {
  IDLog("Server connected\n");
  StatusL[0].s = IPS_ALERT;
  StatusL[1].s = IPS_ALERT;
  IDSetLight(&StatusLP, NULL);
}

void Imager::newDevice(INDI::BaseDevice *dp) {
  const char *deviceName = dp->getDeviceName();
  IDLog("Device %s detected\n", deviceName);
  if (!strcmp(deviceName, controlledCCD))
    StatusL[0].s = IPS_BUSY;
  else if (!strcmp(deviceName, controlledFilterWheel))
    StatusL[1].s = IPS_BUSY;
  IDSetLight(&StatusLP, NULL);
}

void Imager::newProperty(INDI::Property *property) {
  const char *deviceName = property->getDeviceName();
  if (!strcmp(property->getName(), "CONNECTION")) {
    bool state = ((ISwitchVectorProperty *)property->getProperty())->sp[0].s;
    if (!strcmp(deviceName, controlledCCD)) {
      if (state)
        StatusL[0].s = IPS_OK;
      else {
        connectDevice(controlledCCD);
        IDLog("Connecting %s\n", controlledCCD);
      }
    } else if (!strcmp(deviceName, controlledFilterWheel)) {
      if (state)
        StatusL[1].s = IPS_OK;
      else {
        connectDevice(controlledFilterWheel);
        IDLog("Connecting %s\n", controlledFilterWheel);
      }
    }
    IDSetLight(&StatusLP, NULL);
  }
  return;
}

void Imager::removeProperty(INDI::Property *property) {
}

void Imager::newBLOB(IBLOB *bp) {
  int g = (int)ProgressN[0].value;
  int i = (int)ProgressN[1].value;
  int G = (int)GroupCountN[0].value;
  int I = (int)group[g - 1]->GroupSettingsN[0].value;
  
  IDLog("Group %d of %d, image %d of %d, image received\n", g, G, i, I);

  // TBD save image
  
  if (i == I) {
    if (g == G) {
      stopBatch();
    } else {
      ProgressN[0].value = g + 1;
      ProgressN[1].value = 1;
      initiateNextCapture();
    }
  } else {
    ProgressN[1].value = ((int)ProgressN[1].value) + 1;
    initiateNextCapture();
  }
}

void Imager::newSwitch(ISwitchVectorProperty *svp) {
  const char *deviceName = svp->device;
  bool state = svp->sp[0].s;
  if (!strcmp(svp->name, "CONNECTION")) {
    if (!strcmp(deviceName, controlledCCD)) {
      if (state) {
        StatusL[0].s = IPS_OK;
        IDLog("%s connected\n", controlledCCD);
      } else {
        StatusL[0].s = IPS_BUSY;
        IDLog("%s disconnected\n", controlledCCD);
      }
    } else if (!strcmp(deviceName, controlledFilterWheel)) {
      if (state) {
        StatusL[1].s = IPS_OK;
        IDLog("%s connected\n", controlledFilterWheel);
      } else {
        StatusL[1].s = IPS_BUSY;
        IDLog("%s disconnected\n", controlledFilterWheel);
      }
    }
    IDSetLight(&StatusLP, NULL);
  }
}

void Imager::newNumber(INumberVectorProperty *nvp) {
  IDLog("%s %s %5.1f\n", nvp->device, nvp->name, nvp->np[0].value);
}

void Imager::newText(ITextVectorProperty *tvp) {
}

void Imager::newLight(ILightVectorProperty *lvp) {
}

void Imager::newMessage(INDI::BaseDevice *dp, int messageID) {
}

void Imager::serverDisconnected(int exit_code) {
  IDLog("Server disconnected\n");
  StatusL[0].s = IPS_ALERT;
  StatusL[1].s = IPS_ALERT;
}

// Group ----------------------------------------------------------------------------

Group::Group(int id) {
  this->id = id;
  sprintf(groupName, "Image group %d", id);
  sprintf(groupSettingsName, "IMAGE_PROPERTIES_%02d", id);
  IUFillNumber(&GroupSettingsN[0], "IMAGE_COUNT", "Image count", "%3.0f", 1, 100, 1, 1);
  IUFillNumber(&GroupSettingsN[1], "CCD_BINNING", "Binning","%1.0f", 1, 4, 1, 1);
  IUFillNumber(&GroupSettingsN[2], "FILTER_SLOT", "Filter", "%2.f", 1, 12, 1, 1);
  IUFillNumber(&GroupSettingsN[3], "CCD_EXPOSURE_VALUE", "Duration (s)","%5.2f", 0, 36000, 0, 1.0);
  IUFillNumberVector(&GroupSettingsNP, GroupSettingsN, 4, DEVICE_NAME, groupSettingsName, "Image group settings", groupName, IP_RW, 60, IPS_IDLE);
}

bool Group::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) {
  if (strcmp(name, groupSettingsName) == 0) {
    IUUpdateNumber(&GroupSettingsNP, values, names, n);
    IDSetNumber(&GroupSettingsNP, NULL);
    return true;
  }
  return false;
}

void Group::defineProperties() {
  imager->defineNumber(&GroupSettingsNP);
}

void Group::deleteProperties() {
  imager->deleteProperty(GroupSettingsNP.name);
}

void Group::makeReadOnly() {
  GroupSettingsNP.p = IP_RO;
  defineProperties();
}

void Group::makeReadWrite() {
  GroupSettingsNP.p = IP_RW;
  defineProperties();
}
