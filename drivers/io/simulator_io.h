/*
    INDI Simulator IO
    Copyright (C) 2024 Jasem Mutlaq

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

#include "indiinputinterface.h"
#include "indioutputinterface.h"
#include "defaultdevice.h"
#include "inditimer.h"

class SimulatorIO : public INDI::DefaultDevice, public INDI::InputInterface, public INDI::OutputInterface
{
    public:
        SimulatorIO();
        virtual ~SimulatorIO() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual bool Connect() override;
        virtual bool Disconnect() override;

        // Input Interface
        virtual bool UpdateDigitalInputs() override;
        virtual bool UpdateAnalogInputs() override;

        // Output Interface
        virtual bool UpdateDigitalOutputs() override;
        virtual bool CommandOutput(uint32_t index, OutputState command) override;

        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        // Simulation controls
        INDI::PropertySwitch SimulateInputsSP {4};

        // Internal state
        bool m_InputStates[4] {false};
        bool m_OutputStates[4] {false};
};
