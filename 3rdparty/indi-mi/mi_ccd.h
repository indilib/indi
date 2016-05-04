/*
 Moravian INDI Driver

 Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Copyright (C) 2016 Jakub Smutny (linux@gxccd.com)

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

#ifndef MI_CCD_H
#define MI_CCD_H

#include <indiccd.h>
#include <indifilterinterface.h>
#include <iostream>

#include "gxccd.h"

using namespace std;

class MICCD: public INDI::CCD, public INDI::FilterInterface
{
public:

  MICCD(int cameraId, bool eth = false);
  virtual ~MICCD();

  const char *getDefaultName();

  bool initProperties();
  void ISGetProperties(const char *dev);
  bool updateProperties();

  bool Connect();
  bool Disconnect();

  int  SetTemperature(double temperature);
  bool StartExposure(float duration);
  bool AbortExposure();

  IPState GuideNorth(float);
  IPState GuideSouth(float);
  IPState GuideEast(float);
  IPState GuideWest(float);

  bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
  bool ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num);
  bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

protected:

  // Misc.
  void TimerHit();
  bool saveConfigItems(FILE *fp);

  // CCD
  virtual bool UpdateCCDFrame(int x, int y, int w, int h);
  virtual bool UpdateCCDBin(int binx, int biny);

  // Filter Wheel CFW
  virtual int  QueryFilter();
  virtual bool SelectFilter(int position);
  virtual bool SetFilterNames();
  virtual bool GetFilterNames(const char *groupName);

  INumber               FanN[1];
  INumberVectorProperty FanNP;

  INumber               WindowHeatingN[1];
  INumberVectorProperty WindowHeatingNP;

  INumber               CoolerN[1];
  INumberVectorProperty CoolerNP;

  INumber               TemperatureRampN[1];
  INumberVectorProperty TemperatureRampNP;

  INumber               GainN[1];
  INumberVectorProperty GainNP;

  ISwitch               NoiseS[3];
  ISwitchVectorProperty NoiseSP;

private:
  char name[MAXINDIDEVICE];

  int cameraId;
  camera_t *cameraHandle;
  bool isEth;

  bool hasGain;
  bool useShutter;

  int numReadModes;
  int numFilters;
  float minExpTime;
  int maxFanValue;
  int maxHeatingValue;
  int maxBinX;
  int maxBinY;

  int temperatureID;
  int timerID;

  bool downloading;
  bool coolerEnabled;

  CCDChip::CCD_FRAME imageFrameType;

  float TemperatureRequest;
  float ExposureRequest;
  struct timeval ExpStart;

  bool setupParams();

  float calcTimeLeft();
  int grabImage();

  void updateTemperature();
  static void updateTemperatureHelper(void *);

  friend void ::ISGetProperties(const char *dev);
  friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
  friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
  friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
  friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
  friend void ::ISSnoopDevice(XMLEle *root);
};

#endif // MI_CCD_H
