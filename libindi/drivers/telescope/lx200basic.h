/*
    LX200 Basic Driver
    Copyright (C) 2005 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef LX200BASIC_H
#define LX200BASIC_H

#include "inditelescope.h"

class LX200Basic : public INDI::Telescope
{
 public:
 LX200Basic();
 ~LX200Basic();

 virtual const char *getDefaultName();
 virtual bool Connect();
 virtual bool Connect(const char *port, uint32_t baud);
 virtual bool Disconnect();
 virtual bool ReadScopeStatus();
 virtual void ISGetProperties(const char *dev);
 virtual bool initProperties();
 virtual bool updateProperties();
 virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

protected:

 virtual bool Abort();
 virtual bool Goto(double,double);
 virtual bool Sync(double ra, double dec);

 virtual void debugTriggered(bool enable);

 virtual void getBasicData();


private:

 bool isSlewComplete();
 void slewError(int slewCode);
 void mountSim();

  INumber SlewAccuracyN[2];
  INumberVectorProperty SlewAccuracyNP;

  bool   simulation;
  double targetRA, targetDEC;
  double currentRA, currentDEC;
  unsigned int DBG_SCOPE;

};

#endif
