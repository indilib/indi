/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  Simple GPS Simulator

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

#ifndef FLIPFLAT_H
#define FLIPFLAT_H

#include "defaultdevice.h"
#include "indilightboxinterface.h"
#include "indidustcapinterface.h"

class FlipFlat : public INDI::DefaultDevice, public INDI::LightBoxInterface, public INDI::DustCapInterface
{
    public:

    FlipFlat();
    virtual ~FlipFlat();

    virtual bool initProperties();
    virtual void ISGetProperties (const char *dev);
    virtual bool updateProperties();

    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISSnoopDevice (XMLEle *root);

    protected:

    //  Generic indi device entries
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();

    virtual bool saveConfigItems(FILE *fp);
    void TimerHit();

    // From Dust Cap
    virtual IPState ParkCap();
    virtual IPState UnParkCap();

    // From Light Box
    virtual bool SetLightBoxBrightness(uint16_t value);
    virtual bool EnableLightBox(bool enable);

private:

    bool getStartupData();
    bool ping();
    bool getStatus();
    bool getFirmwareVersion();
    bool getBrightness();

    // Device physical port
    ITextVectorProperty PortTP;
    IText PortT[1];

    // Status
    ITextVectorProperty StatusTP;
    IText StatusT[3];

    // Firmware version
    ITextVectorProperty FirmwareTP;
    IText FirmwareT[1];

    int PortFD;
    int productID;
    bool isFlipFlat;
    uint8_t prevCoverStatus, prevLightStatus, prevMotorStatus, prevBrightness;

};

#endif
