/*
    Pinefeat EF / EF-S Lens Controller – Canon® Lens Compatible
    Copyright (C) 2025 Pinefeat LLP (support@pinefeat.co.uk)

    Based on Moonlite focuser
    Copyright (C) 2013-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include <chrono>

class PinefeatCEF : public INDI::Focuser
{
    public:
        PinefeatCEF();
        ~PinefeatCEF() override = default;

        const char * getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n) override;
        bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;

    protected:
        bool Handshake() override;
        IPState MoveAbsFocuser(uint32_t targetTicks) override;
        IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        bool SetFocuserSpeed(int speed) override;
        void TimerHit() override;

    private:
        bool updateProperties(const int32_t pos, const std::string dist, const std::string aper);
        bool readFocusPosition(int32_t &pos);
        bool readFocusDistance(std::string &result);
        bool readApertureRange(std::string &result);
        bool readFirmwareVersion();
        bool isNotMoving();
        bool moveFocusAbs(uint32_t position);
        bool moveFocusRel(FocusDirection dir, uint32_t offset);
        bool setSpeed(int speed);
        bool setApertureAbs(double value);
        bool setApertureRel(double value);
        bool calibrate();

        /**
         * @brief sendCommand Send a string command to the controller.
         * @param cmd Command to be sent, must already have the necessary delimiter ('\n')
         * @param res If not nullptr, the function will read until it detects the default delimiter ('\n') up to CEF_RES length.
         *        if nullptr, no read back is done and the function returns true.
         * @return True if successful, false otherwise.
         */
        bool sendCommand(const char * cmd, char * res = nullptr);

        INDI::PropertySwitch CalibrateSP {1};

        INDI::PropertyNumber ApertureRelNP {1};

        INDI::PropertyNumber ApertureAbsNP {1};

        INDI::PropertyText ApertureRangeTP {1};

        INDI::PropertyText FocusDistanceTP {1};

        // CEF Buffer Size
        static const uint8_t CEF_BUF { 16 };

        // CEF Command Delimiter
        static const char CEF_DEL { '\n' };

        // CEF Command Timeout
        static const uint8_t CEF_TIMEOUT { 3 };

        std::chrono::steady_clock::time_point lastUpdate;
};
