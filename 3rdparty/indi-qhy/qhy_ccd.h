/*
 QHY INDI Driver

 Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Copyright (C) 2014 Zhirong Li (lzr@qhyccd.com)

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

#ifndef QHY_CCD_H
#define QHY_CCD_H

#include <indiccd.h>
#include <indifilterinterface.h>
#include <iostream>

#include <qhyccd.h>

using namespace std;

#define DEVICE struct usb_device *

class QHYCCD: public INDI::CCD, public INDI::FilterInterface
{
public:

  QHYCCD(const char *name);
  virtual ~QHYCCD();

  const char *getDefaultName();

  bool initProperties();
  void ISGetProperties(const char *dev);
  bool updateProperties();

  bool Connect();
  bool Disconnect();

  int  SetTemperature(double temperature);
  bool StartExposure(float duration);
  bool AbortExposure();

  bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
  bool ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num);
  bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

protected:

  void TimerHit();
  virtual bool UpdateCCDFrame(int x, int y, int w, int h);
  virtual bool UpdateCCDBin(int binx, int biny);

  // Guide Port
  virtual bool GuideNorth(float);
  virtual bool GuideSouth(float);
  virtual bool GuideEast(float);
  virtual bool GuideWest(float);

  // Filter Wheel CFW
  virtual int  QueryFilter();
  virtual bool SelectFilter(int position);
  virtual bool SetFilterNames();
  virtual bool GetFilterNames(const char *groupName);

  ISwitch                CoolerS[2];
  ISwitchVectorProperty CoolerSP;

  INumber                CoolerN[1];
  INumberVectorProperty  CoolerNP;

  INumber                GainN[1];
  INumberVectorProperty  GainNP;

  INumber                OffsetN[1];
  INumberVectorProperty  OffsetNP;

  INumber                SpeedN[1];
  INumberVectorProperty  SpeedNP;

  INumber                USBTRAFFICN[1];
  INumberVectorProperty  USBTRAFFICNP;

private:
  char name[MAXINDIDEVICE];
  char camid[MAXINDIDEVICE];

  int camxbin;
  int camybin;
  int camroix;
  int camroiy;
  int camroiwidth;
  int camroiheight;

  bool IsFocusMode;
  bool HasUSBTraffic;
  bool HasUSBSpeed;
  bool HasGain;
  bool HasOffset;
  bool HasFilters;

  qhyccd_handle *camhandle;

  int temperatureID;
  bool temperatureFlag;
  int timerID;
  bool sim;

  CCDChip::CCD_FRAME imageFrameType;

  float TemperatureRequest;
  float ExposureRequest;
  struct timeval ExpStart;

  float CalcTimeLeft();
  int grabImage();
  bool setupParams();

  void UpdateTemperature();
  static void UpdateTemperatureHelper(void *);

  friend void ::ISGetProperties(const char *dev);
  friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
  friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
  friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
  friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
  friend void ::ISSnoopDevice(XMLEle *root);
};

#endif // QHY_CCD_H
