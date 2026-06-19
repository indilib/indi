/*
    INDI Camelot Rotator
    Copyright (C) 2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indirotator.h"

#include <memory>
#include <string>
#include <vector>

class Camelot : public INDI::Rotator
{
    public:
        Camelot();
        virtual ~Camelot() = default;

        const char *getDefaultName() override;

        bool initProperties() override;
        bool updateProperties() override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        // Rotator specific functions
        IPState MoveRotator(double degrees) override;
        bool SyncRotator(double degrees) override;
        bool AbortRotator() override;
        bool ReverseRotator(bool enabled) override;

        // Device specific functions
        bool Handshake() override;
        void TimerHit() override;
        bool saveConfigItems(FILE *fp) override;

    private:
        enum RotatorSpeed
        {
            SPEED_FAST,
            SPEED_MEDIUM,
            SPEED_SLOW
        };

        enum RotatorPower
        {
            POWER_NORMAL,
            POWER_HOLD
        };

        bool queryStatus();

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        /**
                 * @brief sendCommand Send a string command to device.
                 * @param cmd Command to be sent. Can be either NULL TERMINATED or just byte buffer.
                 * @param res If not nullptr, the function will wait for a response from the device. If nullptr, it returns true immediately
                 * after the command is successfully sent.
                 * @param cmd_len if -1, it is assumed that the @a cmd is a null-terminated string. Otherwise, it would write @a cmd_len bytes from
                 * the @a cmd buffer.
                 * @param res_len if -1 and if @a res is not nullptr, the function will read until it detects the default delimiter DRIVER_STOP_CHAR
                 *  up to DRIVER_LEN length. Otherwise, the function will read @a res_len from the device and store it in @a res.
                 * @return True if successful, false otherwise.
        */
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, uint32_t size);

        // Properties
        INDI::PropertySwitch RotatorSpeedSP { 3 };
        INDI::PropertyNumber RotatorPowerNP { 2 };

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const uint8_t DRIVER_STOP_CHAR {0x23};
        static constexpr const uint8_t DRIVER_TIMEOUT {2};
        static constexpr const uint8_t DRIVER_LEN {64};
};
