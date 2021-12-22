/*
    Rainbow Astro Focuser
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

#include <memory>
#include "indifocuser.h"

class RainbowRSF : public INDI::Focuser
{
    public:
        RainbowRSF();

        const char *getDefaultName() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void TimerHit() override;
        virtual bool Handshake() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Focuser Command Functions
        ///////////////////////////////////////////////////////////////////////////////
        // Move focuser to absolute position from 0 to 16000 (-8000 to 8000)
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        // Move focuser to a relative position from the current position
        virtual IPState MoveRelFocuser(FocusDirection dir, unsigned int ticks) override;


        // Get temperature in Celcius
        bool updateTemperature();
        // Find home
        bool findHome();
        // Update position
        bool updatePosition();

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, char * res, int res_len);

    private:

        ///////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty GoHomeSP;
        ISwitch GoHomeS[1];

        INumberVectorProperty TemperatureNP;
        INumber TemperatureN[1];

        uint32_t m_TargetPosition { 0 };
        uint32_t m_LastPosition { 0 };
        double m_LastTemperature { 0 };
        uint16_t m_TemperatureCounter { 0 };

        const static uint32_t homePosition { 8000 };


        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        // # is the stop char
        static const char DRIVER_STOP_CHAR { '#' };
        // Update temperature every 10x POLLMS. For 500ms, we would
        // update the temperature one every 5 seconds.
        static constexpr const uint8_t DRIVER_TEMPERATURE_FREQ {10};
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {16};
};
