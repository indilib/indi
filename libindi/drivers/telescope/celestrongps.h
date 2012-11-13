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

#include "indidevapi.h"
#include "indicom.h"

#define	POLLMS		1000		/* poll period, ms */

class CelestronGPS
{
 public:
 CelestronGPS();
 virtual ~CelestronGPS() {}

 virtual void ISGetProperties (const char *dev);
 virtual void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 virtual void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual void ISPoll ();
 virtual void getBasicData();

 int checkPower(INumberVectorProperty *np);
 int checkPower(ISwitchVectorProperty *sp);
 int checkPower(ITextVectorProperty *tp);
 void connectTelescope();
 void slewError(int slewCode);
 int handleCoordSet();
 int getOnSwitch(ISwitchVectorProperty *sp);

 private:
  int timeFormat;

  double lastRA;
  double lastDEC;

  int lastSet;
  int currentSet;
  double targetRA, targetDEC;
};

#endif

