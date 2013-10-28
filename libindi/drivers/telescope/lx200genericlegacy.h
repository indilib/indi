/*
    LX200 Generic
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef LX200GenericLegacy_H
#define LX200GenericLegacy_H

#include "indidevapi.h"
#include "indicom.h"

#define	POLLMS		1000		/* poll period, ms */
#define mydev		"LX200 Generic" /* The device name */

class LX200GenericLegacy
{
 public:
 LX200GenericLegacy();
 virtual ~LX200GenericLegacy();

 virtual void ISGetProperties (const char *dev);
 virtual void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 virtual void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual void ISSnoopDevice (XMLEle *root);
 virtual void ISPoll ();
 virtual void getBasicData();

 int checkPower(INumberVectorProperty *np);
 int checkPower(ISwitchVectorProperty *sp);
 int checkPower(ITextVectorProperty *tp);
 virtual void handleError(ISwitchVectorProperty *svp, int err, const char *msg);
 virtual void handleError(INumberVectorProperty *nvp, int err, const char *msg);
 virtual void handleError(ITextVectorProperty *tvp, int err, const char *msg);
 bool isTelescopeOn(void);
 virtual void connectTelescope();
 void slewError(int slewCode);
 void getAlignment();
 int handleCoordSet();
 int getOnSwitch(ISwitchVectorProperty *sp);
 void setCurrentDeviceName(const char * devName);
 void correctFault();
 void enableSimulation(bool enable);
 void updateTime();
 void updateLocation();
 void mountSim();

 static void updateFocusTimer(void *p);
 static void guideTimeout(void *p);
 int    fd;
 int    GuideNSTID;
 int    GuideWETID;

 protected:
  int timeFormat;
  int currentSiteNum;
  int trackingMode;

  double JD;
  double lastRA;
  double lastDEC;
  bool   fault;
  bool   simulation;
  char   thisDevice[64];
  int    currentSet;
  int    lastSet;
  double targetRA, targetDEC;

};

void changeLX200GenericLegacyDeviceName(const char * newName);
void changeAllDeviceNames(const char *newName);

#endif
