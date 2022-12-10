/*
    Esatto Focuser
    Copyright (C) 2022 Jasem Mutlaq

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
#include "inditimer.h"
#include "primalucacommandset.h"

class Esatto : public INDI::Focuser
{
    public:
        Esatto();
        virtual ~Esatto() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual bool Handshake() override;
        virtual bool Disconnect() override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;
        virtual bool SetFocuserBacklash(int32_t steps) override;

    private:
        bool Ack();

        bool updateTemperature();        
        bool updatePosition();
        bool updateMaxLimit();

        void setConnectionParams();
        bool initCommandSet();
        void checkMotionProgressCallback();        

        bool getStartupValues();
        void hexDump(char * buf, const char * data, int size);

        INDI::PropertyNumber TemperatureNP {2};
        enum
        {
            TEMPERATURE_EXTERNAL,
            TEMPERATURE_MOTOR,
        };

        INDI::PropertyText FirmwareTP {2};
        enum
        {
            FIRMWARE_SN,
            FIRMWARE_VERSION,
        };

        INDI::PropertySwitch FastMoveSP {3};
        enum
        {
            FASTMOVE_IN,
            FASTMOVE_OUT,
            FASTMOVE_STOP
        };

        INDI::Timer m_MotionProgressTimer;
        INDI::Timer m_TemperatureTimer;

        std::unique_ptr<PrimalucaLabs::Focuser> m_Esatto;
};
