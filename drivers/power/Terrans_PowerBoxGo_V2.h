/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  TerransPowerBoxGoV2

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#pragma once

#include "defaultdevice.h"
#include "indipowerinterface.h" // Include the PowerInterface header

#include <vector>
#include <stdint.h>

#include "basedevice.h"

namespace Connection
{
class Serial;
}

class TerransPowerBoxGoV2 : public INDI::DefaultDevice, public INDI::PowerInterface
{
    public:
        TerransPowerBoxGoV2();
    public:
        virtual ~TerransPowerBoxGoV2() = default;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void TimerHit() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[],
                                 int n) override; // Add ISNewNumber

    protected:
        virtual const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;

        // Implement virtual methods from INDI::PowerInterface
        virtual bool SetPowerPort(size_t port, bool enabled) override;
        virtual bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
        virtual bool SetVariablePort(size_t port, bool enabled, double voltage) override;
        virtual bool SetLEDEnabled(bool enabled) override;
        virtual bool SetAutoDewEnabled(size_t port, bool enabled) override;
        virtual bool CyclePower() override;
        virtual bool SetUSBPort(size_t port, bool enabled) override;

    private:
        bool Handshake();
        bool sendCommand(const char * cmd, char * res);
        int PortFD{-1};

        bool setupComplete { false };
        bool processButtonSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        void Get_State();

        Connection::Serial *serialConnection { nullptr };

        // Removed manual property declarations, now handled by INDI::PowerInterface
        // Retained MCUTempNP as it's not power-related
        INDI::PropertyNumber MCUTempNP {1};
        INDI::PropertySwitch StateSaveSP {2};

        static constexpr const char *ADD_SETTING_TAB {"Additional Settings"};

};
