/*
    SestoSenso Focuser
    Copyright (C) 2020 Abdulaziz Bouland (boulandab@ikarustech.com)

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

//#include <cmath>
#include <memory>
#include "indifocuser.h"
//#include <cstring>
//#include <termios.h>
//#include <unistd.h>
//#include <regex>

class RainbowRSF : public INDI::Focuser
{
    public:
        RainbowRSF();
        virtual ~RainbowRSF() override = default;

        virtual bool Handshake() override;
        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        //        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        //        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        //    protected:
        //        virtual IPState MoveAbsFocuser(uint32_t targetTicks);
        //        virtual IPState MoveRelFocuser(FocusDirection dir, unsigned int ticks);
        //        virtual bool FindTemperature();
        //        virtual bool FindHome();
        //        virtual void TimerHit();

    private:

        IText FirmwareT[1] {};
        ITextVectorProperty FirmwareTP;

        // # is the stop char
        static const char DRIVER_STOP_CHAR { '#' };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {16};
};
