/*
    IPX800 Controller
    Copyright (C) 2025 Jasem Mutlaq

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

#include "defaultdevice.h"
#include "indiinputinterface.h"
#include "indioutputinterface.h"

namespace Connection
{
class TCP;
}

class IPX800 : public INDI::DefaultDevice, public INDI::InputInterface, public INDI::OutputInterface
{
    public:
        IPX800();
        virtual ~IPX800() = default;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;

    protected:
        const char *getDefaultName() override;
        void TimerHit() override;
        bool Handshake();

        // Output Interface
        virtual bool UpdateDigitalOutputs() override;
        virtual bool CommandOutput(uint32_t index, OutputState command) override;

    private:
        bool sendCommand(const char *cmd, char *response);
        bool UpdateDigitalInputs() override;
        bool UpdateAnalogInputs() override;


        Connection::TCP *tcpConnection { nullptr };
        int PortFD { -1 };

        INDI::PropertyText ModelVersionTP {1};
        INDI::PropertyText APIKeyTP {1};  // API key for JSON requests

        static constexpr const uint8_t DIGITAL_INPUTS {8};
        static constexpr const uint8_t DIGITAL_OUTPUTS {8};
        static constexpr const uint8_t ANALOG_INPUTS {4};
};
