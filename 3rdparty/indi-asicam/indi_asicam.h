/*
 Asicam CCD INDI Driver
 
 Copyright (C) 2014 Chrstian Pellegrin <chripell@gmail.com>
 
 Based on CCD Template for INDI Developers
 Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Multiple device support Copyright (C) 2013 Peter Polakovic (peter.polakovic@cloudmakers.eu)

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

#ifndef ASICAM_CCD_H
#define ASICAM_CCD_H

#include <indiccd.h>
#include <iostream>

using namespace std;

#define DEVICE int

class AsicamCCD: public INDI::CCD {
public:

  //AsicamCCD(DEVICE device, const char *camName);
  AsicamCCD(DEVICE device);
  virtual ~AsicamCCD();

  const char *getDefaultName();

  bool initProperties();
  void ISGetProperties(const char *dev);
  bool updateProperties();

  bool Connect();
  bool Disconnect();

  bool StartExposure(float duration);
  bool AbortExposure();

  virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
  virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

protected:

  void TimerHit();
  virtual bool UpdateCCDFrame(int x, int y, int w, int h);
  virtual bool UpdateCCDBin(int binx, int biny);
  virtual bool UpdateCCDFrameType(CCDChip::CCD_FRAME fType);

  // Guide Port
  virtual bool GuideNorth(float);
  virtual bool GuideSouth(float);
  virtual bool GuideEast(float);
  virtual bool GuideWest(float);

private:
  DEVICE device;
  char name[MAXINDINAME];

  INumber GainN[1];
  INumberVectorProperty GainNP;

  INumber USBBWN[1];
  INumberVectorProperty USBBWNP;

  ISwitch ModeS[4];
  ISwitchVectorProperty	ModeSP;

  ISwitch *AvailableCameraS;
  ISwitchVectorProperty AvailableCameraSP;

  double minDuration;
  unsigned short *imageBuffer;

  int timerID;

  CCDChip::CCD_FRAME imageFrameType;

  struct timeval ExpStart;

  float ExposureRequest;

  float CalcTimeLeft();
  int grabImage();
  bool setupParams();

  bool sim;

  bool need_flush;		/* set when internal buffer of the asicam has to be flushed */

  friend void ::ISGetProperties(const char *dev);
  friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
  friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
  friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
  friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
};

#endif // ASICAM_CCD_H
