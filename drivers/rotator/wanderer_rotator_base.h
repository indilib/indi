/*******************************************************************************
  Copyright(c) 2025 Frank Wang & Jérémie Klein. All rights reserved.

  WandererRotator Base

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
#include "indirotator.h"
#include "indirotatorinterface.h"
#include "indipropertyswitch.h"

class WandererRotatorBase : public INDI::Rotator
{
public:
    WandererRotatorBase();

    bool initProperties() override;
    bool updateProperties() override;
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

protected:
    virtual const char * getDefaultName() override = 0;
    virtual const char * getRotatorHandshakeName() = 0;
    virtual int getMinimumCompatibleFirmwareVersion() = 0;
    virtual int getStepsPerDegree() = 0;

    IPState MoveRotator(double angle) override;
    IPState HomeRotator() override;
    bool ReverseRotator(bool enabled) override;
    bool AbortRotator() override;
    void TimerHit() override;

    bool Handshake() override;
    bool sendCommand(std::string command);
    bool Move(const char *cmd);

    INDI::PropertySwitch SetZeroSP{1};
    INDI::PropertyNumber BacklashNP{1};

    int firmware = 0;
    double M_angleread = 0;
    double M_backlashread = 0;
    double M_reverseread = 0;
    double initangle = 0;
    bool haltcommand = false;
    bool ReverseState = false;
    double backlash = 0.5;
    double positionhistory = 0;
    int estime = 0;
    int nowtime = 0;

    enum
    {
        BACKLASH,
    };
};
