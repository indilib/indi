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
        virtual ~DMFC() override = default;

        virtual bool Handshake() override;
        const char *getDefaultName() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;

        virtual bool SyncFocuser(uint32_t ticks) override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool SetFocuserBacklash(int32_t steps) override;
        virtual bool SetFocuserBacklashEnabled(bool enabled) override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        bool updateFocusParams();
        bool moveAbsolute(uint32_t newPosition);
        bool moveRelative(int relativePosition);
        bool setMaxSpeed(uint16_t speed);
        bool setLedEnabled(bool enable);
        bool setEncodersEnabled(bool enable);
        bool setMotorType(uint8_t type);
        bool ack();
        void ignoreResponse();

        uint32_t currentPosition { 0 };
        uint32_t targetPosition { 0 };
        bool isMoving = false;

        // Temperature probe
        INDI::PropertyNumber TemperatureNP {1};

        // Motor Mode
        INDI::PropertySwitch MotorTypeSP {2};
        enum { MOTOR_DC, MOTOR_STEPPER };

        // Rotator Encoders
        INDI::PropertySwitch EncoderSP {2};
        enum { ENCODERS_ON, ENCODERS_OFF };

        // LED
        INDI::PropertySwitch LEDSP {2};
        enum { LED_OFF, LED_ON };

        // Maximum Speed
        INDI::PropertyNumber MaxSpeedNP {1};

        // Firmware Version
        INDI::PropertyText FirmwareVersionTP {1};
};
