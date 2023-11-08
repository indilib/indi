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
        bool updateVoltageIn();
        bool updateMaxLimit();

        void setConnectionParams();
        bool initCommandSet();

        bool getStartupValues();
        void hexDump(char * buf, const char * data, int size);

        uint16_t m_TemperatureCounter { 0 };
        double m_LastTemperature[2] = {-1, -1};
        double m_LastVoltage[2] = {-1, -1};

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

        INDI::PropertyNumber VoltageNP {2};
        enum
        {
            VOLTAGE_12V,
            VOLTAGE_USB
        };

        INDI::PropertySwitch FastMoveSP {3};
        enum
        {
            FASTMOVE_IN,
            FASTMOVE_OUT,
            FASTMOVE_STOP
        };

        std::unique_ptr<PrimalucaLabs::Esatto> m_Esatto;
        static constexpr uint8_t TEMPERATURE_FREQUENCY {10};
        static constexpr double MEASUREMENT_THRESHOLD {0.1};
};
