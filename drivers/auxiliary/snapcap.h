/*******************************************************************************
  Copyright(c) 2017 Jarno Paananen. All right reserved.

  Driver for SnapCap dust cap / flat panel

  based on Flip Flat driver by:

  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

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

#pragma once

#include "defaultdevice.h"
#include "indilightboxinterface.h"
#include "indidustcapinterface.h"

#include <stdint.h>

namespace Connection
{
class Serial;
}

class SnapCap : public INDI::DefaultDevice, public INDI::LightBoxInterface, public INDI::DustCapInterface
{
  public:
    SnapCap();
    virtual ~SnapCap() = default;

    virtual bool initProperties() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool updateProperties() override;

    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISSnoopDevice(XMLEle *root) override;

  protected:
    const char *getDefaultName() override;

    virtual bool saveConfigItems(FILE *fp) override;
    void TimerHit() override;

    // From Dust Cap
    virtual IPState ParkCap() override;
    virtual IPState UnParkCap() override;

    // From Light Box
    virtual bool SetLightBoxBrightness(uint16_t value) override;
    virtual bool EnableLightBox(bool enable) override;

  private:
    bool getStartupData();
    bool ping();
    bool getStatus();
    bool getFirmwareVersion();
    bool getBrightness();

    bool Handshake();

    bool sendCommand(const char *command, char *response);

    IPState Abort();

    // Status
    ITextVectorProperty StatusTP;
    IText StatusT[3]{};

    // Firmware version
    ITextVectorProperty FirmwareTP;
    IText FirmwareT[1]{};

    // Abort
    ISwitch AbortS[1];
    ISwitchVectorProperty AbortSP;

    // Force open & close
    ISwitch ForceS[2];
    ISwitchVectorProperty ForceSP;

    int PortFD{ -1 };
    bool hasLight{ true };
    uint8_t simulationWorkCounter{ 0 };
    uint8_t targetCoverStatus{ 0xFF };
    uint8_t prevCoverStatus{ 0xFF };
    uint8_t prevLightStatus{ 0xFF };
    uint8_t prevMotorStatus{ 0xFF };
    uint8_t prevBrightness{ 0xFF };

    Connection::Serial *serialConnection{ nullptr };
};
