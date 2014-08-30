/*
  QHY CCD INDI Driver

  Copyright (c) 2013 Cloudmakers, s. r. o.
  All Rights Reserved.

  Code is based on QHY INDI Driver by Jasem Mutlaq
  Copyright(c) 2012 Jasem Mutlaq. All rights reserved.

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

#ifndef QHYCCD_H_
#define QHYCCD_H_

#include <indiccd.h>
#include "qhygeneric.h"

void ExposureTimerCallback(void *p);

class QHYCCD : public INDI::CCD
{
  private:
    QHYDevice *device;
    char name[128];
    float ExposureTimeLeft;
    int ExposureTimerID;
    INumberVectorProperty GainNP;
    INumber GainN[1];

  protected:
    const char *getDefaultName();
    bool initProperties();
    bool updateProperties();
    bool UpdateCCDFrame(int x, int y, int w, int h);
    bool UpdateCCDBin(int hor, int ver);
    bool Connect();
    bool Disconnect();
    int  SetTemperature(double temperature);
    bool StartExposure(float n);
    bool AbortExposure();
    void TimerHit();
    void ExposureTimerHit();
    bool GuideWest(float time);
    bool GuideEast(float time);
    bool GuideNorth(float time);
    bool GuideSouth(float time);

  public:
    QHYCCD(QHYDevice *device);
    virtual ~QHYCCD();
    void debugTriggered(bool enable);
    void simulationTriggered(bool enable);
    void ISGetProperties(const char *dev);
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

  friend void ::ExposureTimerCallback(void *p);
  friend void ::ISGetProperties(const char *dev);
  friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
  friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
  friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
  friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
  friend void ::ISSnoopDevice(XMLEle *root);
};

#endif /* QHYCCD_H_ */
