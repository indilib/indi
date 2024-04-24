/*
    iOptron iEAF Focuser

    Copyright (C) 2013 Paul de Backer (74.0632@gmail.com)
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

#include "indifocuser.h"

class iEAFFocus : public INDI::Focuser
{
    public:
        iEAFFocus();
        ~iEAFFocus();

        virtual bool Handshake() override;
        const char * getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;

        virtual IPState MoveAbsFocuser(uint32_t ticks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool SetFocuserMaxPosition(uint32_t ticks) override;

    private:
        bool m_isMoving {false};

        void GetFocusParams();
        void setZero();
        bool updateInfo();
        bool Ack();
        bool MoveMyFocuser(uint32_t position);

        INDI::PropertyNumber TemperatureNP {1};
        INDI::PropertySwitch SetZeroSP {1};
};

