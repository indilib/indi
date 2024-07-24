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

#include "indirelayinterface.h"
#include "defaultdevice.h"
#include "../../libs/modbus/nanomodbus.h"

class WaveshareRelay : public INDI::DefaultDevice, public INDI::RelayInterface
{
    public:
        WaveshareRelay();
        virtual ~WaveshareRelay() override = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    protected:
        bool Handshake();

        // Relay

        /**
         * \brief Query single relay status
         * \param index Relay index
         * \param status Store relay status in this variable.
         * \return True if operation is successful, false otherwise
         */
        virtual bool QueryRelay(uint32_t index, Status &status);

        /**
         * \brief Send command to relay
         * \return True if operation is successful, false otherwise
         */
        virtual bool CommandRelay(uint32_t index, Command command);

        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        Connection::TCP *tcpConnection {nullptr};
        int PortFD{-1};
        nmbs_t nmbs;
};
