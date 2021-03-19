/*******************************************************************************
  Copyright(c) 2020 Jasem Mutlaq. All rights reserved.

  Pegasus Falcon Rotator

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

#include "indirotator.h"
#include <stdint.h>

class PegasusFalcon : public INDI::Rotator
{
    public:
        PegasusFalcon();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;

        // Event loop
        virtual void TimerHit() override;

        // Rotator Overrides
        virtual IPState MoveRotator(double angle) override;
        virtual bool ReverseRotator(bool enabled) override;
        virtual bool SyncRotator(double angle) override;
        virtual bool AbortRotator() override;

    private:
        bool Handshake() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool getFirmware();
        bool getStatusData();

        ///////////////////////////////////////////////////////////////////////////////
        /// Device Control Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool reloadFirmware();
        bool setDerotation(uint32_t ms);

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
                 * @param res_len if -1 and if @a res is not nullptr, the function will read until it detects the default delimeter DRIVER_STOP_CHAR
                 *  up to DRIVER_LEN length. Otherwise, the function will read @a res_len from the device and store it in @a res.
                 * @return True if successful, false otherwise.
        */
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, uint32_t size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        /**
         * @brief cleanupResponse Removes all spaces
         * @param response buffer
         */
        void cleanupResponse(char *response);

        ////////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ////////////////////////////////////////////////////////////////////////////////////
        /// Reboot Device
        ISwitchVectorProperty ReloadFirmwareSP;
        ISwitch ReloadFirmwareS[1];
        /// Derotation
        INumberVectorProperty DerotateNP;
        INumber DerotateN[1];
        /// Firmware
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[1] {};

        std::vector<std::string> lastStatusData;

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const uint8_t DRIVER_STOP_CHAR {0xA};
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        static constexpr const uint8_t DRIVER_LEN {128};
};
