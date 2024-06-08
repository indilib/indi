/*******************************************************************************
  Copyright(c) 2024 Frank Wang. All rights reserved.

  WandererCover V4-EC

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
#include "indidustcapinterface.h"
#include "indilightboxinterface.h"

namespace Connection
{
class Serial;
}

class WandererCoverV4EC : public INDI::DefaultDevice, public INDI::DustCapInterface, public INDI::LightBoxInterface
{
public:
    WandererCoverV4EC();
    virtual ~WandererCoverV4EC() = default;

    virtual bool initProperties() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool updateProperties() override;

protected:

    // From Dust Cap
    virtual IPState ParkCap() override;
    virtual IPState UnParkCap() override;

    // From Light Box
    virtual bool SetLightBoxBrightness(uint16_t value) override;
    virtual bool EnableLightBox(bool enable) override;

    const char *getDefaultName() override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual void TimerHit() override;


private:

    int firmware=0;
    bool toggleCover(bool open);
    bool sendCommand(std::string command);
    //Current Calibrate
    bool getData();
    double closesetread=0;
    double opensetread=0;
    double positionread=0;
    double voltageread=0;
    bool Ismoving=false;
    bool setDewPWM(int id, int value);
    bool setClose(double value);
    bool setOpen(double value);
    void updateData(double closesetread,double opensetread,double positionread,double voltageread);

    INDI::PropertyNumber DataNP{4};
    enum
    {
        closeset_read,
        openset_read,
        position_read,
        voltage_read,
    };

    //Dew heater///////////////////////////////////////////////////////////////
    INDI::PropertyNumber SetHeaterNP{1};
    enum
    {
        Heat,
    };
    //Close Set///////////////////////////////////////////////////////////////
    INDI::PropertyNumber CloseSetNP{1};
    enum
    {
        CloseSet,
    };
    //Open Set///////////////////////////////////////////////////////////////
    INDI::PropertyNumber OpenSetNP{1};
    enum
    {
        OpenSet,
    };

    int PortFD{ -1 };

    Connection::Serial *serialConnection{ nullptr };
};
