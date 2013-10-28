/*
    LX200 Astro-Physics INDI driver

    Copyright (C) 2007 Markus Wildi

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

#include "lx200genericlegacy.h"
/* thisDevice is not defined with the global section property definitions.*/
/* So I use this macro throught the lx200ap.cpp */
/* even one can use thisDevice e.g. in function call like IDMessage( thisDevice,...)*/
/* That may collide with what is defined in LX200GenericLegacy.cpp */
#define myapdev "LX200 Astro-Physics"

#define DOMECONTROL 0
#define NOTDOMECONTROL  1

#define SYNCCM  0
#define SYNCCMR 1

#define NOTESTABLISHED 0 
#define    ESTABLISHED 1 
#define MOUNTNOTINITIALIZED 0 
#define MOUNTINITIALIZED 1 

class LX200AstroPhysics : public LX200GenericLegacy
{
 public:
  LX200AstroPhysics();
  ~LX200AstroPhysics() {}

 void ISGetProperties (const char *dev);
 void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 void ISSnoopDevice (XMLEle *root) ;
 void ISPoll ();
 int  setBasicDataPart0();
 int  setBasicDataPart1();
 void connectTelescope();
 void setupTelescope();
 void handleAltAzSlew();
 void handleAZCoordSet() ;
 void handleEqCoordSet() ;
 void ISInit() ;
 bool isMountInit(void) ;
 private:

 enum LX200_STATUS { LX200_SLEW, LX200_TRACK, LX200_SYNC, LX200_PARK };

private:
 double targetRA, targetDEC;
 double targetAZ, targetALT;

};

void changeLX200AstroPhysicsDeviceName(const char *newName);
#endif
 
