/*
    TeenAstro Focuser
    Copyright (C) 2021 Markus Noga

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
    bool send(const char *const msg);
 
    // Sends a command to the focuser, and waits for a response terminated by '#'. Returns true on success and false on failure.
    bool sendAndReceive(const char *const msg, char *resp);

    // Sends a command to the focuser, and waits for a single character response. Returns true if successful and response is '1', otherwise false.
    bool sendAndReceiveBool(const char *const msg);


    // Updates device version string from device
    bool updateDeviceVersion();

    // Updates current state from device, consisting of position, speed, and temperature. Returns true on success and false on failure.
    bool updateState();

    // Returns true if focuser is moving, else false.   
    bool isMoving();

    // Updates movement configuration from device. Returns true on success and false on failure.
    bool updateConfig();

    // Sets given configuration item on to device to provided value in device native units. Returns true on success and false on failure. 
    bool setConfigItem(char item, uint32_t deviceValue);
    bool setParkPos(uint32_t value);
    bool setMaxPos(uint32_t value);
    bool setGoToSpeed(uint32_t value);
    bool setManualSpeed(uint32_t value);
    bool setGoToAcc(uint32_t value);
    bool setManualAcc(uint32_t value);
    bool setManualDec(uint32_t value);

    // Updates motor configuration from device. Returns true on success and false on failure.
    bool updateMotorConfig();

    // Sends new motor inversion flag value to device. Returns true on success and false on failure. 
    bool setMotorInvert(uint32_t value);
    bool setMotorMicrosteps(uint32_t value);
    bool setMotorResolution(uint32_t value);
    bool setMotorCurrent(uint32_t value);
    bool setMotorStepsPerRevolution(uint32_t value);

    // Starts movement of focuser towards the given absolute position. Returns immediately, true on success else false.
    bool goTo(uint32_t position);

    // Starts movement of focuser towards the parking position. Returns immediately, true on success else false.
    bool goToPark();

    // Stop focuser movement. Returns immediately, true on success else false.
    bool stop();

    // Syncs focuser zero position to current position, true on success else false.
    bool syncZero();


    // UI Elements
    //

    // UI Element: Sync current position as zero position
    ISwitch SyncZeroS[1];
    ISwitchVectorProperty SyncZeroSP;

    // Configuration variables
    //

    IText CfgDeviceVersionT[1];
    ITextVectorProperty CfgDeviceVersionTP;

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
