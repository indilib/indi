/*
    PlaneWave EFA Protocol

    Hendrick Focuser

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

#include "indifocuser.h"

#include <map>

class EFA : public INDI::Focuser
{
    public:
        EFA();

        virtual bool Handshake();
        const char *getDefaultName();
        virtual bool initProperties();
        virtual bool updateProperties();

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

    protected:
        virtual IPState MoveAbsFocuser(uint32_t targetTicks);
        virtual IPState MoveRelFocuser(FocusDirection dir, unsigned int ticks);
        virtual bool SyncFocuser(uint32_t ticks);
        virtual bool ReverseFocuser(bool enabled);
        virtual bool SetFocuserMaxPosition(uint32_t ticks);
        virtual bool AbortFocuser();
        virtual void TimerHit();

        virtual bool saveConfigItems(FILE *fp);

    private:
        ///////////////////////////////////////////////////////////////////////////////////
        /// Query functions
        ///////////////////////////////////////////////////////////////////////////////////


        ///////////////////////////////////////////////////////////////////////////////////
        /// Set functions
        ///////////////////////////////////////////////////////////////////////////////////


        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommandOk(const char * cmd, int cmd_len);
        bool sendCommand(const char * cmd, char * res, int cmd_len, int res_len);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Misc
        ///////////////////////////////////////////////////////////////////////////////////
        void getStartupValues();        
        uint8_t calculateCheckSum(const char *cmd);
        template <typename T> std::string to_string(const T a_value, const int n = 2);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////

        // Focuser Informatin
        ITextVectorProperty InfoTP;
        IText InfoT[2];
        enum
        {
            INFO_NAME,
            INFO_VERSION
        };

        // Focuser Operations
        ISwitchVectorProperty OperationSP;
        ISwitch OperationS[3];
        enum
        {
            OPERATION_REBOOT,
            OPERATION_RESET,
            OPERATION_ZEROING,
        };

        // Temperature Compensation
        ISwitchVectorProperty TemperatureCompensationSP;
        ISwitch TemperatureCompensationS[2];
        enum
        {
            TC_ENABLED,
            TC_DISABLED
        };

        // TC State
        ISwitchVectorProperty TemperatureStateSP;
        ISwitch TemperatureStateS[2];
        enum
        {
            TC_ACTIVE,
            TC_PAUSED
        };

        // Temperature Compensation Settings
        INumberVectorProperty TemperatureSettingsNP;
        INumber TemperatureSettingsN[3];
        enum
        {
            TC_FACTOR,
            TC_PERIOD,
            TC_DELTA
        };

        // Temperature Sensors
        INumberVectorProperty TemperatureSensorNP;
        INumber TemperatureSensorN[3];
        enum
        {
            TEMP_0,
            TEMP_1,
            TEMP_AVG
        };

        /////////////////////////////////////////////////////////////////////////////
        /// Private variables
        /////////////////////////////////////////////////////////////////////////////

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////        
        // Start of Message
        static const char DRIVER_SOM { 0x3B };

        static const char DEVICE_PC { 0x20 };
        static const char DEVICE_HC { 0x0D };
        static const char DEVICE_FOC { 0x12 };
        static const char DEVICE_FAN { 0x13 };
        static const char DEVICE_TEMP { 0x12 };

        static constexpr const uint8_t DRIVER_LEN {9};
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
};
