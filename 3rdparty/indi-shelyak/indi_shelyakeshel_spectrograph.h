/*******************************************************************************
  Copyright(c) 2017 Simon Holmbo. All rights reserved.

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


#ifndef SHELYAKESHEL_SPECTROGRAPH_H
#define SHELYAKESHEL_SPECTROGRAPH_H

#include <map>

#include "defaultdevice.h"

std::map<ISState, char> COMMANDS       = { { ISS_ON, 0x53 }, { ISS_OFF, 0x43 } };
std::map<std::string, char> PARAMETERS = { { "MIRROR", 0x31 },
                                           { "LED", 0x32 },
                                           { "THAR", 0x33 },
                                           { "TUNGSTEN", 0x34 } };

class ShelyakEshel : public INDI::DefaultDevice
{
  public:
    ShelyakEshel();
    ~ShelyakEshel();

    void ISGetProperties(const char *dev);
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);

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
    ISwitchVectorProperty MirrorSP;
    ISwitch MirrorS[2];

    // Options
    ITextVectorProperty PortTP;
    IText PortT[1] {};

    // Spectrograph Settings
    INumberVectorProperty SettingsNP;
    INumber SettingsN[5];

    bool calibrationUnitCommand(char command, char parameter);
};

#endif // SHELYAKESHEL_SPECTROGRAPH_H
