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

#ifndef MOONLITE_H
#define MOONLITE_H

#include "indibase/indifocuser.h"

class MoonLite : public INDI::Focuser
{
public:
    MoonLite();
    ~MoonLite();

    typedef enum { FOCUS_HALF_STEP, FOCUS_FULL_STEP } FocusStepMode;

    virtual bool Connect();
    virtual bool Disconnect();
    const char * getDefaultName();
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool Move(FocusDirection dir, int speed, int duration);
    virtual int MoveAbs(int ticks);
    virtual int MoveRel(FocusDirection dir, unsigned int ticks);
    virtual bool SetSpeed(int speed);
    virtual bool Abort();
    virtual void TimerHit();

protected:
    bool saveConfigItems(FILE *fp);

private:

    int PortFD;
    double targetPos, lastPos, lastTemperature;
    unsigned int currentSpeed;

    struct timeval focusMoveStart;
    float focusMoveRequest;

    void GetFocusParams();
    bool updateStepMode();
    bool updateTemperature();
    bool updatePosition();
    bool updateSpeed();
    bool isMoving();

    bool Move(unsigned int position);
    bool setStepMode(FocusStepMode mode);
    bool setSpeed(unsigned short speed);
    bool setTemperatureCalibration(double calibration);
    bool setTemperatureCoefficient(double coefficient);
    bool setTemperatureCompensation(bool enable);
    float CalcTimeLeft(timeval,float);

    ITextVectorProperty PortTP;
    IText PortT[1];

    INumber TemperatureN[1];
    INumberVectorProperty TemperatureNP;

    ISwitch StepModeS[2];
    ISwitchVectorProperty StepModeSP;

    INumber MaxTravelN[1];
    INumberVectorProperty MaxTravelNP;

    INumber TemperatureSettingN[2];
    INumberVectorProperty TemperatureSettingNP;

    ISwitch TemperatureCompensateS[2];
    ISwitchVectorProperty TemperatureCompensateSP;

    INumber PresetN[3];
    INumberVectorProperty PresetNP;

    ISwitch PresetGotoS[3];
    ISwitchVectorProperty PresetGotoSP;

};

#endif // MOONLITE_H
