/*
    Moonlite DRO Dual Focuser
    Copyright (C) 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

class MoonLiteDRO : public INDI::Focuser
{
  public:
    MoonLiteDRO(int ID);

    typedef enum { FOCUS_HALF_STEP, FOCUS_FULL_STEP } FocusStepMode;

    const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    void remoteDisconnect();
    int getPortFD() { return PortFD; }

protected:
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual bool Handshake() override;
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual bool AbortFocuser() override;
    virtual void TimerHit() override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual bool SyncFocuser(uint32_t ticks) override;

  private:
    void GetFocusParams();
    bool updateStepMode();
    bool updateStepDelay();
    bool updateTemperature();
    bool updatePosition();
    bool isMoving();
    bool Ack();

    bool gotoAbsPosition(uint32_t position);
    bool setStepMode(FocusStepMode mode);
    bool setStepDelay(uint8_t delay);
    bool setTemperatureCalibration(double calibration);
    bool setTemperatureCoefficient(double coefficient);
    bool setTemperatureCompensation(bool enable);

    double targetPos { 0 };
    double lastPos { 0 };
    double lastTemperature { 0 };

    ISwitch StepModeS[2];
    ISwitchVectorProperty StepModeSP;

    INumber StepDelayN[1];
    INumberVectorProperty StepDelayNP;

    INumber TemperatureSettingN[2];
    INumberVectorProperty TemperatureSettingNP;

    INumber TemperatureN[1];
    INumberVectorProperty TemperatureNP;

    ISwitch TemperatureCompensateS[2];
    ISwitchVectorProperty TemperatureCompensateSP;

    const uint8_t m_ID;
    static constexpr const uint8_t DRO_CMD = 16;
    static constexpr const char *SETTINGS_TAB = "Settings";
};
