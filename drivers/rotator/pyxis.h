/*
    Optec Pyrix Rotator
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

#include "indirotator.h"
#include "string.h"

class Pyxis : public INDI::Rotator
{
  public:

    Pyxis();
    virtual ~Pyxis() = default;

    virtual bool Handshake() override;
    const char * getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

  protected:
    // Rotator Overrides
    virtual IPState HomeRotator() override;
    virtual IPState MoveRotator(double angle) override;
    virtual bool ReverseRotator(bool enabled) override;

    // Misc.
    virtual void TimerHit() override;

  private:    
    // Check if connection is OK
    bool Ack();
    bool isMotionComplete();
    bool getPA(uint16_t & PA);
    int getReverseStatus();
    bool setSteppingMode(uint8_t mode);
    bool setRotationRate(uint8_t rate);
    bool sleepController();
    bool wakeupController();
    std::string getVersion() ;

    void queryParams();

    // Rotation Rate
    INumber RotationRateN[1];
    INumberVectorProperty RotationRateNP;

    // Stepping
    ISwitch SteppingS[2];
    ISwitchVectorProperty SteppingSP;
    enum { FULL_STEP, HALF_STEP};

    // Power
    ISwitch PowerS[2];
    ISwitchVectorProperty PowerSP;
    enum { POWER_SLEEP, POWER_WAKEUP};

    // Firmware version; tells us if 2 inch or 3 inch device
    IText FirmwareT[1] {};
    ITextVectorProperty FirmwareTP;
    IText ModelT[1] {};
    ITextVectorProperty ModelTP;

    uint16_t targetPA = {0};

    // Direction of rotation; 1->angle increasing; -1->angle decreasing
    int direction = 1 ;
};
