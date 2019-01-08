/*
    Moonlite Focuser
    Copyright (C) 2013 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

class MoonLite : public INDI::Focuser
{
  public:
    MoonLite();
    virtual ~MoonLite() override = default;

    typedef enum { FOCUS_HALF_STEP, FOCUS_FULL_STEP } FocusStepMode;

    const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

protected:
    virtual bool Handshake() override;
    virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual bool SetFocuserSpeed(int speed) override;
    virtual bool AbortFocuser() override;
    virtual void TimerHit() override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual bool SyncFocuser(uint32_t ticks) override;

  private:
    void GetFocusParams();
    //bool sync(uint16_t offset);
    bool updateStepMode();
    bool updateTemperature();
    bool updatePosition();
    bool updateSpeed();
    bool isMoving();
    bool Ack();

    bool MoveFocuser(unsigned int position);
    bool setStepMode(FocusStepMode mode);
    bool setSpeed(unsigned short speed);
    bool setTemperatureCalibration(double calibration);
    bool setTemperatureCoefficient(double coefficient);
    bool setTemperatureCompensation(bool enable);
    float CalcTimeLeft(timeval, float);

    double targetPos { 0 };
    double lastPos { 0 };
    double lastTemperature { 0 };
    unsigned int currentSpeed { 0 };

    struct timeval focusMoveStart { 0, 0 };
    float focusMoveRequest { 0 };

    INumber TemperatureN[1];
    INumberVectorProperty TemperatureNP;

    ISwitch StepModeS[2];
    ISwitchVectorProperty StepModeSP;

//    INumber MaxTravelN[1];
//    INumberVectorProperty MaxTravelNP;

    INumber TemperatureSettingN[2];
    INumberVectorProperty TemperatureSettingNP;

    ISwitch TemperatureCompensateS[2];
    ISwitchVectorProperty TemperatureCompensateSP;

//    INumber SyncN[1];
//    INumberVectorProperty SyncNP;
};
