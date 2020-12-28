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
        //        virtual ~RainbowRSF() override = default;

        const char *getDefaultName() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;



    protected:
        //        virtual IPState MoveAbsFocuser(uint32_t targetTicks);
        //        virtual IPState MoveRelFocuser(FocusDirection dir, unsigned int ticks);
        //        virtual bool getTemperature();
        //        virtual bool getHome();
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        //        virtual void TimerHit() override;
        virtual bool Handshake() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Focuser Command Functions
        ///////////////////////////////////////////////////////////////////////////////
        // Move focuser to absolute position from 0 to 16000 (-8000 to 8000)
        //        virtual bool MoveAbsFocuser();
        // Move focuser to a relative position from the current position
        //        virtual bool MoveRelFocuser();
        // Get temperature in Celcius
        virtual bool GetTemperature();
        // Find home
        virtual bool findHome();

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        virtual bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        virtual void hexDump(char * buf, const char * data, int size);

    private:

        ///////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////
        //        IText FirmwareT[1] {};
        //        ITextVectorProperty FirmwareTP;
        INumberVectorProperty CurrentAbsPositionNP;
        INumber CurrentAbsPositionN[1];

        INumberVectorProperty CurrentTempNP;
        INumber CurrentTempN[1];

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        // # is the stop char
        static const char DRIVER_STOP_CHAR { '#' };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {16};
};
