/*******************************************************************************
  Copyright(c) 2017 Simon Holmbo. All rights reserved.
  Copyright(c) 2018 Jean-Baptiste Butet. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#ifndef SHELYAKSPOX_SPECTROGRAPH_H
#define SHELYAKSPOX_SPECTROGRAPH_H
//On serial connection
//11\n : calib lamp on
//21\n : flat lamp on
//if 11 and 21 : dark on
//(and the opposite, 10, 20)
//00\n : shut off all

//
#include <map>
#include <indiapi.h>
#include "defaultdevice.h"

std::map<ISState, char> COMMANDS = {
  {ISS_ON, 0x31}, {ISS_OFF, 0x30}    //"1" and "0"
};
std::map<std::string, char> PARAMETERS = {
  {"CALIBRATION", 0x31}, {"FLAT", 0x32}, {"DARK", 0x33} //"1", "2", "3"
};

class ShelyakSpox : public INDI::DefaultDevice
{
public:
  ShelyakSpox();
  ~ShelyakSpox();

  void ISGetProperties (const char *dev);
  bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
  bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

protected:
  const char *getDefaultName();

  bool initProperties();
  bool updateProperties();

  bool Connect();
  bool Disconnect();

private:
  int PortFD; // file descriptor for serial port

  // Main Control
  ISwitchVectorProperty LampSP;
  ISwitch LampS[3];

  // Options
  ITextVectorProperty PortTP;
  IText PortT[1];

  // Spectrograph Settings
  INumberVectorProperty SettingsNP;
  INumber SettingsN[2];

  const char* lastLampOn;
  
  
  bool calibrationUnitCommand(char command, char parameter);
  bool resetLamps();
  //bool pollingLamps();
};

#endif // SHELYAKSPOX_SPECTROGRAPH_H
