/*
    Pegasus DMFC Focuser
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

#include "indifocuser.h"

class DMFC : public INDI::Focuser
{
  public:
    DMFC();
    virtual ~DMFC() = default;

    virtual bool Handshake();
    const char *getDefaultName();
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
    virtual IPState MoveAbsFocuser(uint32_t targetTicks);
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
    virtual bool SetFocuserSpeed(int speed);
    virtual bool AbortFocuser();
    virtual void TimerHit();

  private:
    void GetFocusParams();
    bool sync(uint16_t offset);
    bool updateTemperature();
    bool updatePosition();
    bool updateSpeed();
    bool isMoving();
    bool Ack();

    bool MoveFocuser(unsigned int position);    
    bool setSpeed(unsigned short speed);
    float CalcTimeLeft(timeval, float);

    double targetPos { 0 };
    double lastPos { 0 };
    double lastTemperature { 0 };
    unsigned int currentSpeed { 0 };

    struct timeval focusMoveStart { 0, 0 };
    float focusMoveRequest { 0 };

    // Temperature probe
    INumber TemperatureN[1];
    INumberVectorProperty TemperatureNP;

    // Sync Position
    INumber SyncN[1];
    INumberVectorProperty SyncNP;

    // Motor Mode
    ISwitch MotorModeS[2];
    ISwitchVectorProperty MotorModeSP;
    enum { MOTOR_STEPPER, MOTOR_DC };

    // Rotator Encoders
    ISwitch EncoderS[2];
    ISwitchVectorProperty EncoderSP;

    // Enable/Disable backlash
    ISwitch BacklashCompensationS[2];
    ISwitchVectorProperty BacklashCompensationSP;

    // Backlash Value
    INumber BacklashN[1];
    INumberVectorProperty BacklashNP;

    // Reverse Direction
    ISwitch ReverseS[2];
    ISwitchVectorProperty ReverseSP;

    // LED
    ISwitch LEDS[2];
    ISwitchVectorProperty LEDSP;
};
