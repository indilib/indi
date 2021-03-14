/*
    TeenAstro Focuser
    Copyright (C) 2021 Markus Noga
    Derived from the below, and hereby made available under the same license.

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

#ifndef TEENASTRO_FOCUSER_H
#define TEENASTRO_FOCUSER_H

#include "indibase/indifocuser.h"

class TeenAstroFocuser : public INDI::Focuser
{
public:
    TeenAstroFocuser();
    ~TeenAstroFocuser();

    const char * getDefaultName();
    virtual bool Handshake();
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual IPState MoveAbsFocuser(uint32_t ticks);
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
    virtual bool AbortFocuser();
    virtual void TimerHit();

private:

    double targetPos, lastPos;

    // Sends a command to the focuser. Returns true on success and false on failure.
    bool Send(const char *const msg);
 
    // Sends a command to the focuser, and waits for a response terminated by '#'. Returns true on success and false on failure.
    bool SendAndReceive(const char *const msg, char *resp);

    void setZero();
    bool updateMaxPos();

    // Updates current state consisting of position, speed, and temperature. Returns true on success and false on failure.
    bool updateState();   
    bool isMoving();

    // Updates movement configuration. Returns true on success and false on failure.
    bool updateConfig();

    // Updates motor configuration. Returns true on success and false on failure.
    bool updateMotorConfig();

    bool goTo(uint32_t position);
    bool setMaxPos(uint32_t maxPos);

    // UI Elements
    //

    // UI Element: Sync current position as zero position
    ISwitch SetZeroS[1];
    ISwitchVectorProperty SetZeroSP;

    // Configuration variables
    //

    // Focuser park position 
    INumber CfgParkPosN[1];
    INumberVectorProperty CfgParkPosNP;

    // Focuser max position is inherited from superclass as FocusMaxPosN, FocusMaxPosNP

    // Focuser goto speed
    INumber CfgGoToSpeedN[1];
    INumberVectorProperty CfgGoToSpeedNP;

    // Focuser high speed
    INumber CfgManualSpeedN[1];
    INumberVectorProperty CfgManualSpeedNP;

    // Go-to acceleration
    INumber CfgGoToAccN[1];
    INumberVectorProperty CfgGoToAccNP;

    // Manual acceleration
    INumber CfgManualAccN[1];
    INumberVectorProperty CfgManualAccNP;

    // Manual deceleration
    INumber CfgManualDecN[1];
    INumberVectorProperty CfgManualDecNP;


    // Motor configuration variables
    //

    // Configuration element: Invert motor direction
    INumber CfgMotorInvertN[1];
    INumberVectorProperty CfgMotorInvertNP;

    // Configuration element: Motor microsteps
    INumber CfgMotorMicrostepsN[1];
    INumberVectorProperty CfgMotorMicrostepsNP;

    // Configuration element: Impulse resolution (???)
    INumber CfgMotorResolutionN[1];
    INumberVectorProperty CfgMotorResolutionNP;

    // Configuration element: Motor current
    INumber CfgMotorCurrentN[1];
    INumberVectorProperty CfgMotorCurrentNP;

    // Configuration element: Motor steps per revolution
    INumber CfgMotorStepsPerRevolutionN[1];
    INumberVectorProperty CfgMotorStepsPerRevolutionNP;

    // Status variables
    //
    
    // Status variable: Focuser current position is inherited from superclass as FocusAbsPosN, FocusAbsPosNP

    // Status variable: Current focuser speed
    INumber CurSpeedN[1];
    INumberVectorProperty CurSpeedNP;

    // Status variable: Focuser temperature
    INumber TempN[1];
    INumberVectorProperty TempNP;

};

#endif // TEENASTRROFOCUSER_H
