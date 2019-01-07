/*
    SestoSenso Focuser
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

class SestoSenso : public INDI::Focuser
{
  public:
    SestoSenso();
    virtual ~SestoSenso() override = default;

    typedef enum { FOCUS_HALF_STEP, FOCUS_FULL_STEP } FocusStepMode;

    const char *getDefaultName() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

protected:
    virtual bool Handshake() override;
    virtual bool Disconnect() override;
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;

    virtual bool AbortFocuser() override;
    virtual void TimerHit() override;

  private:
    bool Ack();
    void GetFocusParams();
    bool setMinLimit(uint32_t limit);
    bool setMaxLimit(uint32_t limit);
    bool isCommandOK(const char *cmd);
    bool isMotionComplete();

    bool sync(uint32_t newPosition);
    bool updateTemperature();
    bool updatePosition();

    uint32_t targetPos { 0 };
    uint32_t lastPos { 0 };
    double lastTemperature { 0 };

    INumber TemperatureN[1];
    INumberVectorProperty TemperatureNP;

    IText FirmwareT[1] {};
    ITextVectorProperty FirmwareTP;

    INumber SyncN[1];
    INumberVectorProperty SyncNP;

    INumber LimitsN[2];
    INumberVectorProperty LimitsNP;
    enum
    {
        SS_MIN_LIMIT,
        SS_MAX_LIMIT
    };
};
