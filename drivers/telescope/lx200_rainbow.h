/*
    LX200 Rainbow Driver
    Copyright (C) 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "lx200generic.h"

class LX200Rainbow : public LX200Generic
{
    public:
        LX200Rainbow();

        const char *getDefaultName() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        // Is slew over?
        virtual bool isSlewComplete() override;
        // Check if mount is responsive
        virtual bool checkConnection() override;
        // Read mount state
        virtual bool ReadScopeStatus() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Tracking Functions
        ///////////////////////////////////////////////////////////////////////////////
        // Toggle Tracking
        virtual bool SetTrackEnabled(bool enabled) override;
        bool getTrackingState();

        ///////////////////////////////////////////////////////////////////////////////
        /// Query Functions
        ///////////////////////////////////////////////////////////////////////////////
        virtual void getBasicData() override;
        bool getFirmwareVersion();

        ///////////////////////////////////////////////////////////////////////////////
        /// Parking, Homing, and Calibration
        ///////////////////////////////////////////////////////////////////////////////
        virtual bool Park() override;

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

    private:

        /// Function
        bool findHome();

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////
        ISwitchVectorProperty HomeSP;
        ISwitch HomeS[1];

        std::string version;

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * INFO_TAB = "Info";
        // '#' is the stop char
        static const char DRIVER_STOP_CHAR { 0x23 };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {64};
};
