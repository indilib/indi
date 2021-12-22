/*
    Baader Steeldrive Focuser
    Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

class SteelDrive : public INDI::Focuser
{
  public:
    SteelDrive();
    virtual ~SteelDrive() = default;

    typedef struct
    {
        double maxTrip;
        double gearRatio;
    } FocusCustomSetting;

    typedef enum { FOCUS_HALF_STEP, FOCUS_FULL_STEP } FocusStepMode;
    enum
    {
        FOCUS_MAX_TRIP,
        FOCUS_GEAR_RATIO
    };
    enum
    {
        FOCUS_T_COEFF,
        FOCUS_T_SAMPLES
    };

    virtual bool Handshake() override;
    const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool saveConfigItems(FILE *fp) override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual IPState MoveRelFocuser(FocusDirection dir, unsigned int ticks) override;
    virtual bool SetFocuserSpeed(int speed) override;
    virtual bool AbortFocuser() override;
    virtual void TimerHit() override;

    void debugTriggered(bool enable) override;

  private:
    // Get functions
    bool updateVersion();
    bool updateTemperature();
    bool updateTemperatureSettings();
    bool updatePosition();
    bool updateSpeed();
    bool updateAcceleration();
    bool updateCustomSettings();

    // Set functions
    bool setStepMode(FocusStepMode mode);
    bool setSpeed(unsigned short speed);
    bool setCustomSettings(double maxTrip, double gearRatio);
    bool setAcceleration(unsigned short accel);
    bool setTemperatureSamples(unsigned int targetSamples, unsigned int *finalSample);
    bool setTemperatureCompensation();

    bool Sync(unsigned int position);

    // Motion functions
    bool moveFocuser(unsigned int position);
    bool stop();
    bool startMotion(FocusDirection dir);

    // Misc functions
    bool saveFocuserConfig();
    bool Ack();
    void GetFocusParams();
    float CalcTimeLeft(timeval, float);
    void updateFocusMaxRange(double maxTrip, double gearRatio);

    double targetPos { 0 };
    double lastPos { 0 };
    double lastTemperature { 0 };
    double simPosition { 0 };
    unsigned int currentSpeed { 0 };
    unsigned int temperatureUpdateCounter { 0 };
    bool sim { false };

    struct timeval focusMoveStart { 0, 0 };
    float focusMoveRequest { 0 };

    INumber TemperatureN[1];
    INumberVectorProperty TemperatureNP;

    INumber AccelerationN[1];
    INumberVectorProperty AccelerationNP;

    INumber TemperatureSettingN[2];
    INumberVectorProperty TemperatureSettingNP;

    ISwitch TemperatureCompensateS[2];
    ISwitchVectorProperty TemperatureCompensateSP;

    ISwitch ModelS[5];
    ISwitchVectorProperty ModelSP;

    INumber CustomSettingN[2];
    INumberVectorProperty CustomSettingNP;

    INumber SyncN[1];
    INumberVectorProperty SyncNP;

    IText VersionT[2] {};
    ITextVectorProperty VersionTP;

    FocusCustomSetting fSettings[5];
};
