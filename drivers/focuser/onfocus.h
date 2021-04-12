/*
    OnFocus Focuser
    Copyright (C) 2018 Paul de Backer (74.0632@gmail.com)
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

#ifndef ONFOCUS_H
#define ONFOCUS_H

#include "indibase/indifocuser.h"

class OnFocus : public INDI::Focuser
{
public:
    OnFocus();
    ~OnFocus();

    virtual bool Handshake() override;
    const char * getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual IPState MoveAbsFocuser(uint32_t ticks) override;
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual bool AbortFocuser() override;
    virtual void TimerHit() override;

private:

    double targetPos, lastPos;
 
    void GetFocusParams();
    void setZero();
    bool updateMaxPos();
    bool updateTemperature();
    bool updatePosition();
    bool isMoving();
    bool Ack();

    bool MoveMyFocuser(uint32_t position);
    bool setMaxPos(uint32_t maxPos);

    INumber MaxPosN[1];
    INumberVectorProperty MaxPosNP;
    ISwitch SetZeroS[1];
    ISwitchVectorProperty SetZeroSP;
};

#endif // ONFOCUS_H
