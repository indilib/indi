/*
    Waveshare ModBUS POE Relay
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

#include "indiinputinterface.h" // Added for InputInterface
#include "indioutputinterface.h"
#include "defaultdevice.h"
#include "inditimer.h"
#include "../../libs/modbus/nanomodbus.h"

class WaveshareRelay : public INDI::DefaultDevice, public INDI::OutputInterface,
    public INDI::InputInterface // Added InputInterface
{
    public:
        WaveshareRelay();
        virtual ~WaveshareRelay() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        bool Handshake();

        // Relay

        /**
         * \brief Update all digital outputs
         * \return True if operation is successful, false otherwise
         * \note UpdateDigitalOutputs should either be called periodically in the child's TimerHit or custom timer function or when an
         * interrupt or trigger warrants updating the digital outputs. Only updated properties that had a change in status since the last
         * time this function was called should be sent to the clients to reduce unnecessary updates.
         * Polling or Event driven implemetation depends on the concrete class hardware capabilities.
         */
        virtual bool UpdateDigitalOutputs() override;

        /**
         * \brief Update all digital inputs
         * \return True if operation is successful, false otherwise
         */
        virtual bool UpdateDigitalInputs() override;

        virtual bool UpdateAnalogInputs() override;

        /**
         * \brief Send command to relay
         * \return True if operation is successful, false otherwise
         */
        virtual bool CommandOutput(uint32_t index, OutputState command) override;

        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        Connection::TCP *tcpConnection {nullptr};
        INDI::PropertyText FirmwareVersionTP {1};
        int PortFD{-1};
        bool m_HaveInput {false};
        nmbs_t nmbs;

};
