/*
    Celestron GPS
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

#ifndef CELESTRONGPS_H
#define CELESTRONGPS_H

#include "libs/indibase/inditelescope.h"
#include "indidevapi.h"
#include "indicom.h"

#define	POLLMS		1000		/* poll period, ms */

class CelestronGPS : public INDI::Telescope
{
 public:
 CelestronGPS();
 virtual ~CelestronGPS() {}

 virtual const char *getDefaultName();
 virtual bool Connect();
 virtual bool Connect(char *);
 virtual bool Disconnect();
 virtual bool ReadScopeStatus();
 virtual void ISGetProperties(const char *dev);
 virtual bool initProperties();
 virtual bool updateProperties();
 virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 virtual bool ISSnoopDevice(XMLEle *root);

protected:

 virtual bool MoveNS(TelescopeMotionNS dir);
 virtual bool MoveWE(TelescopeMotionWE dir);
 virtual bool Abort();

 virtual bool saveConfigItems(FILE *fp);

 bool Goto(double ra,double dec);
 bool Sync(double ra, double dec);
 virtual bool canSync();
 virtual bool canPark();

 void slewError(int slewCode);
 void mountSim();

 void enableJoystick();
 void disableJoystick();
 void processNSWE(double mag, double angle);


 /* Slew Speed */
 ISwitchVectorProperty SlewModeSP;
 ISwitch SlewModeS[4];


private:
  int timeFormat;

  double lastRA;
  double lastDEC;

  int lastSet;
  int currentSet;
  double currentRA, currentDEC;
  double targetRA, targetDEC;

  /* Joystick Support */
  ISwitchVectorProperty UseJoystickSP;
  ISwitch UseJoystickS[2];

  ITextVectorProperty JoystickSettingTP;
  IText JoystickSettingT[6];

};

#endif

