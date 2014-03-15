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


 virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

 virtual const char *getDefaultName();
 bool initProperties();
 bool updateProperties();

 virtual bool ReadScopeStatus();

 virtual bool Park();
 virtual bool Connect(char *);

 virtual void debugTriggered(bool enable);

 int  setBasicDataPart0();
 int  setBasicDataPart1();

 void setupTelescope();
 //void handleAltAzSlew();
 //void handleAZCoordSet() ;
 //void handleEqCoordSet() ;
 //void ISInit() ;
 bool isMountInit(void) ;

protected:

 ISwitch StartUpS[2];
 ISwitchVectorProperty StartUpSP;

 INumber HourangleCoordsN[2];
 INumberVectorProperty HourangleCoordsNP;

 INumber HorizontalCoordsN[2];
 INumberVectorProperty HorizontalCoordsNP;

 ISwitch MoveToRateS[4];
 ISwitchVectorProperty MoveToRateSP;

 ISwitch SlewRateS[3];
 ISwitchVectorProperty SlewRateSP;

 ISwitch SwapS[2];
 ISwitchVectorProperty SwapSP;

 ISwitch SyncCMRS[2];
 ISwitchVectorProperty SyncCMRSP;

 IText   VersionT[1];
 ITextVectorProperty VersionInfo;

 IText   DeclinationAxisT[1];
 ITextVectorProperty DeclinationAxisTP;

 INumber APSiderealTimeN[1];
 INumberVectorProperty APSiderealTimeNP;

};

#endif
 
