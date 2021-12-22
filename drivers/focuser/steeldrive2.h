/*
    Baader SteelDriveII Focuser

    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

class SteelDriveII : public INDI::Focuser
{
    public:
        SteelDriveII();

        typedef enum { GOING_UP, GOING_DOWN, STOPPED, ZEROED } State;
        typedef enum { NAME, POSITION, STATE, LIMIT, FOCUS, TEMP0, TEMP1, TEMPAVG, TCOMP, PWM } Summary;

        virtual bool Handshake() override;
        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, unsigned int ticks) override;
        virtual bool SyncFocuser(uint32_t ticks) override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool SetFocuserMaxPosition(uint32_t ticks) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;

        virtual bool saveConfigItems(FILE *fp) override;

    private:
        ///////////////////////////////////////////////////////////////////////////////////
        /// Query functions
        ///////////////////////////////////////////////////////////////////////////////////
        bool getParameter(const std::string &parameter, std::string &value);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Set functions
        ///////////////////////////////////////////////////////////////////////////////////
        bool setParameter(const std::string &parameter, const std::string &value);

        ///////////////////////////////////////////////////////////////////////////////
        /// Communication Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool getSummary();
        bool sendCommandOK(const char * cmd);
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, int size);
        std::vector<std::string> split(const std::string &input, const std::string &regex);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Misc
        ///////////////////////////////////////////////////////////////////////////////////
        void getStartupValues();
        template <typename T> std::string to_string(const T a_value, const int n = 2);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////////

        // Focuser Informatin
        ITextVectorProperty InfoTP;
        IText InfoT[2] {};
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

        // Stepper Drive
        INumberVectorProperty StepperDriveNP;
        INumber StepperDriveN[2];
        enum
        {
            CURRENT_MOVE,
            CURRENT_HOLD
        };

        /////////////////////////////////////////////////////////////////////////////
        /// Private variables
        /////////////////////////////////////////////////////////////////////////////
        State m_State { STOPPED };
        std::map<Summary, std::string> m_Summary;
        bool m_ConfirmFactoryReset { false };

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * SETTINGS_TAB = "Settings";
        static constexpr const char * COMPENSATION_TAB = "Compensation";
        // 0xA is the stop char
        static const char DRIVER_STOP_CHAR { 0x0A };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {192};
};
