/*
 ASI CCD Driver

 Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef ASI_CCD_H
#define ASI_CCD_H

#include <indiccd.h>
#include <v4l2_record.h>
#include <iostream>

#include "ASICamera2.h"

using namespace std;

class ASICCD: public INDI::CCD
{
public:

  ASICCD(ASI_CAMERA_INFO *camInfo);
  virtual ~ASICCD();

  const char *getDefaultName();

  bool initProperties();
  void ISGetProperties(const char *dev);
  bool updateProperties();

  bool Connect();
  bool Disconnect();

  int  SetTemperature(double temperature);
  bool StartExposure(float duration);
  bool AbortExposure();

  static void * streamVideoHelper(void* context);
  void * streamVideo();

protected:

  bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
  bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
  bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

  void TimerHit();
  virtual bool UpdateCCDFrame(int x, int y, int w, int h);
  virtual bool UpdateCCDBin(int binx, int biny);

  // Guide Port
  virtual bool GuideNorth(float ms);
  virtual bool GuideSouth(float ms);
  virtual bool GuideEast(float ms);
  virtual bool GuideWest(float ms);

private:

  /** Get image from CCD and send it to client */
  int grabImage();
  /** Get initial parameters from camera */
  bool setupParams();
  /** Calculate time left in seconds after start_time */
  float calcTimeLeft(float duration, timeval *start_time);
  /** Create number and switch controls for camera by querying the API */
  void createControls(int piNumberOfControls);
  /** Get the current Bayer string used */
  const char *getBayerString();
  /** Update control values from camera */
  void updateControls();
  /** Return user selected image type */
  ASI_IMG_TYPE getImageType();
  /** Update SER recorder video format */
  void updateRecorderFormat();

  char name[MAXINDIDEVICE];

  /** Additional Properties to INDI::CCD */
  INumber CoolerN[1];
  INumberVectorProperty CoolerNP;

  INumber *ControlN;
  INumberVectorProperty ControlNP;

  ISwitch *ControlS;
  ISwitchVectorProperty ControlSP;

  ISwitch *VideoFormatS;
  ISwitchVectorProperty VideoFormatSP;

  ISwitch StreamS[2];
  ISwitchVectorProperty StreamSP;

  IBLOBVectorProperty *imageBP;
  IBLOB *imageB;

  ISwitch RecordS[2];
  ISwitchVectorProperty RecordSP;				/* Record switch */

  IText RecordFileT[1];
  ITextVectorProperty RecordFileTP;

  struct timeval ExpStart;
  float ExposureRequest;
  float TemperatureRequest;
  int TemperatureUpdateCounter;

  CCDCapability asiCap;

  ASI_CAMERA_INFO *m_camInfo;
  ASI_CONTROL_CAPS *pControlCaps;

  bool sim;
  int streamPredicate;
  pthread_t primary_thread;
  bool terminateThread;
  bool exposureRetries;

  // ST4
  bool InWEPulse;
  float WEPulseRequest;
  struct timeval WEPulseStart;
  int WEtimerID;

  bool InNSPulse;
  float NSPulseRequest;
  struct timeval NSPulseStart;
  int NStimerID;

  ASI_GUIDE_DIRECTION WEDir;
  ASI_GUIDE_DIRECTION NSDir;

  // Record frames
  V4L2_Record *v4l2_record;
  V4L2_Recorder *recorder;
  bool direct_record;
  int frameCount;

  friend void ::ISGetProperties(const char *dev);
  friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
  friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
  friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
  friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
};

#endif // ASI_CCD_H
