/*******************************************************************************
  Copyright(c) 2024 Frank Wang. All rights reserved.

  WandererRotator Mini V1/V2

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
class WandererRotatorMini : public INDI::Rotator
{
public:
    WandererRotatorMini();

    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;




protected:
    const char * getDefaultName() override;
    virtual IPState MoveRotator(double angle) override;
    virtual IPState HomeRotator() override;
    virtual bool ReverseRotator(bool enabled) override;

    virtual bool AbortRotator() override;
    virtual void TimerHit() override;
    virtual bool SetRotatorBacklash(int32_t steps) override;
    virtual bool SetRotatorBacklashEnabled(bool enabled) override;




private:
    int firmware=0;
    double M_angleread=0;
    double initangle=0;
    bool Handshake() override;
    INDI::PropertySwitch SetZeroSP{1};
    bool sendCommand(std::string command);
    bool Move(const char *cmd);
    bool haltcommand = false;
    bool ReverseState=false;
    int reversecoefficient=1;
    double backlash=0.5;
    double positionhistory=0;
    int estime=0;
    int nowtime=0;

};






