/*
    Astro-Physics INDI driver

    Copyright (C) 2014 Jasem Mutlaq

    Based on INDI Astrophysics Driver by Markus Wildi

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

#ifndef LX200ASTROPHYSICS_H
#define LX200ASTROPHYSICS_H

#include "lx200generic.h"

#define SYNCCM  0
#define SYNCCMR 1

#define NOTESTABLISHED 0 
#define ESTABLISHED 1
#define MOUNTNOTINITIALIZED 0 
#define MOUNTINITIALIZED 1 

class LX200AstroPhysics : public LX200Generic
{
 public:
  LX200AstroPhysics();
  ~LX200AstroPhysics() {}

 virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual void ISGetProperties(const char *dev);

 void setupTelescope();

 bool isMountInit(void) ;

protected:

 virtual const char *getDefaultName();
 bool initProperties();
 bool updateProperties();

 virtual bool ReadScopeStatus();

 // Parking
 virtual void SetCurrentPark();
 virtual void SetDefaultPark();
 virtual bool Park();
 virtual bool UnPark();

 virtual bool Sync(double ra, double dec);
 virtual bool Goto(double, double);
 virtual bool Connect(const char *port, uint32_t baud);
 virtual bool Disconnect();
 virtual bool updateTime(ln_date * utc, double utc_offset);
 virtual bool updateLocation(double latitude, double longitude, double elevation);
 virtual bool SetSlewRate(int index);

 virtual void debugTriggered(bool enable);
 bool  setBasicDataPart0();
 bool  setBasicDataPart1();

 ISwitch StartUpS[2];
 ISwitchVectorProperty StartUpSP;

 INumber HourangleCoordsN[2];
 INumberVectorProperty HourangleCoordsNP;

 INumber HorizontalCoordsN[2];
 INumberVectorProperty HorizontalCoordsNP;

 ISwitch SlewSpeedS[3];
 ISwitchVectorProperty SlewSpeedSP;

 ISwitch SwapS[2];
 ISwitchVectorProperty SwapSP;

 ISwitch SyncCMRS[2];
 ISwitchVectorProperty SyncCMRSP;

 IText   VersionT[1];
 ITextVectorProperty VersionInfo;

 IText   DeclinationAxisT[1];
 ITextVectorProperty DeclinationAxisTP;

 //INumber APSiderealTimeN[1];
 //INumberVectorProperty APSiderealTimeNP;

 INumber SlewAccuracyN[2];
 INumberVectorProperty SlewAccuracyNP;

private:

 bool timeUpdated, locationUpdated;
 int initStatus;

};

#endif
 
