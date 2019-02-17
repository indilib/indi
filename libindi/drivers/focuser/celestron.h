/*
    Celestron Focuser for SCT and EDGEHD

    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2019 Chris Rowland

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

class CelestronSCT : public INDI::Focuser
{
    public:
        CelestronSCT();

        const char * getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n) override;

    protected:
        /**
         * @brief Handshake Try to communicate with Focuser and see if there is a valid response.
         * @return True if communication is successful, false otherwise.
         */
        virtual bool Handshake() override;

        /**
         * @brief MoveAbsFocuser Move to an absolute target position
         * @param targetTicks target position
         * @return IPS_BUSY if motion is in progress. IPS_OK if motion is small and already complete. IPS_ALERT for trouble.
         */
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;

        /**
         * @brief MoveRelFocuser Move focuser for a relative amount of ticks in a specific direction
         * @param dir Directoin of motion
         * @param ticks steps to move
         * @return IPS_BUSY if motion is in progress. IPS_OK if motion is small and already complete. IPS_ALERT for trouble.
         */
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;

        /**
         * @brief SyncFocuser Set the supplied position as the current focuser position
         * @param ticks target position
         * @return IPS_OK if focuser position is now set to ticks. IPS_ALERT for problems.
         */
        virtual bool SyncFocuser(uint32_t ticks) override;

        /**
         * @brief SetFocuserSpeed Set target focuser speed. Speed starts from 1.
         * @param speed target speed
         * @return True if speed is set successfully, false otherwise.
         */
        virtual bool SetFocuserSpeed(int speed) override;

        /**
         * @brief AbortFocuser Abort Focuser motion
         * @return True if successful, false otherwise.
         */
        virtual bool AbortFocuser() override;

        /**
         * @brief TimerHit Primary Loop called every POLLMS milliseconds (set in Options) to check
         * on the focuser status like read position, temperature.. and check if the focuser reached the required position.
         */
        virtual void TimerHit() override;

        virtual bool saveConfigItems(FILE * fp) override;

    private:
        ///////////////////////////////////////////////////////////////////////////////
        /// Utility Functions
        ///////////////////////////////////////////////////////////////////////////////
        /**
         * @brief sendCommand Sends command to device port.
         * @param cmd Command to be sent. Maximum length is CELESTRON_LEN
         * @param res If not nullptr, the function will read the response dependeing on the value of res_len parameter.
         * @param cmd_len If -1 (default), then the function will treat cmd as a null-terminated string and will send it accordingly.
         * if cmd_len is specified, it will write this many bytes to the port.
         * @param res_len if @a res_len is -1 (default), then the function will read until it encounters character @a CELESTRON_DEL ('#').
         * if res_len is specified, it will read up until @a res_len given it is equal or less than @a CELESTRON_LEN.
         * @return True if successful, false otherwise.
         * @example To send command ":ST100#" to the device without requiring a response, simply call
         * @code sendCommand("ST100#")@endcode
         */
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);

        // Format command/response as hex
        void hexDump(char * buf, const char * data, int size);

        // Do we have a response from the focuser?
        bool Ack();

        // Get initial focuser parameters when we first connect
        bool getStartupParameters();

        ///////////////////////////////////////////////////////////////////////////////
        /// Read Data From Controller
        ///////////////////////////////////////////////////////////////////////////////
        // Read and update Position
        bool readPosition();
        // Read and update speed
        bool readSpeed();
        // Read and update backlash
        bool readBacklash();
        // Are we moving?
        bool isMoving();

        ///////////////////////////////////////////////////////////////////////////////
        /// Write Data to Controller
        ///////////////////////////////////////////////////////////////////////////////
        bool sendBacklash(uint32_t steps);

        ///////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////
        INumber BacklashN[1];
        INumberVectorProperty BacklashNP;


        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        // Celestron Buffer
        static const uint8_t CELESTRON_LEN { 32 };
        // Celestorn Delimeter
        static const char CELESTRON_DEL { '#' };
        // Celestron Tiemout in seconds
        static const uint8_t CELESTRON_TIMEOUT { 3 };
};
