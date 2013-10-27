/*
    LX200 FS2 Driver
	Copyright (C) 2009 Ferran Casarramona (ferran.casarramona@gmail.com)
	Based on LX200 Basic driver from:
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

#ifndef LX200FS2_H
#define LX200FS2_H

//#include "indidevapi.h"
//#include "indicom.h"

#include "lx200generic.h"
// To prevent of using mydev from generic by error
#ifdef mydev
#undef mydevice
#endif

class LX200Fs2 : public LX200Generic
{
 public:
 LX200Fs2();
 ~LX200Fs2();

 virtual void ISGetProperties (const char *dev);
 virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 // Not implemented. LX200 generic used. 
 virtual void handleError(ISwitchVectorProperty *svp, int err, const char *msg);
 virtual void handleError(INumberVectorProperty *nvp, int err, const char *msg);
 virtual void handleError(ITextVectorProperty *tvp, int err, const char *msg);
 int check_fs2_connection(int fd); // Added to support firmware versions below 1.19
 
 protected:
 
  const char *DeviceName;
   /* Numbers */
  INumber FirmwareVerN[1]; // Firmware version
    
   /* Number Vectors */
  INumberVectorProperty FirmwareVerNP;  
  
 //*******************************************************/
 /* Connection Routines
 ********************************************************/
 void init_properties();
 void getBasicData();
 virtual void connectTelescope();
 bool is_connected(void);

};

void changeLX200FS2DeviceName(const char *newName);

#endif
