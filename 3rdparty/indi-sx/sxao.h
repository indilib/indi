/*
 Starlight Xpress Active Optics INDI Driver

 Copyright (c) 2012 Cloudmakers, s. r. o.
 All Rights Reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#ifndef SXAO_H
#define SXAO_H

#include <libindi/defaultdevice.h>
#include <libindi/indiguiderinterface.h>

extern const char *GUIDE_CONTROL_TAB;

class SXAO: public INDI::DefaultDevice, INDI::GuiderInterface {
  private:
    int PortFD;
    ITextVectorProperty PortTP;
    IText PortT[1];
    INumber AONS[2];
    INumberVectorProperty AONSP;
    INumber AOEW[2];
    INumberVectorProperty AOEWP;
    ISwitch Center[2];
    ISwitchVectorProperty CenterP;

    int aoCommand(const char *request, char *response, int nbytes);

  public:
    SXAO();
    ~SXAO();

    void ISGetProperties (const char *dev);
    bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    bool Connect();
    bool Disconnect();

    bool initProperties();
    bool updateProperties();

    bool GuideNorth(float ms);
    bool GuideSouth(float ms);
    bool GuideEast(float ms);
    bool GuideWest(float ms);

    bool AONorth(int steps);
    bool AOSouth(int steps);
    bool AOEast(int steps);
    bool AOWest(int steps);

    bool AOCenter();
    bool AOUnjam();

    const char *getDefaultName();
};

#endif // SXAO_H
