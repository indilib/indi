/*
    NFocus DC Relative Focuser

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

/**
 * @brief The NFocus class Handles communication and control with nFocus DC focuser
 *
 * API

        ctrl-F  response 'n' for pc-nFOCUS focuser
        S       response 1 if moving focuser, 0 if not
        :FDSXXX#  Focus in dir D at speed S for XXX counts (S not implemented)
                Counts are increments of (on+off) time, sending 000 halts any focus in progress
                D = 0 Inward
                D = 1 Outward
        :COXXX#   (Configure On) Set focus ON time (# of 0.68ms to wait, default = 73 = 0.05sec)
        :CFXXX#   (Configure oFf) Set focus OFF time (# of 0.68ms to wait, default = 15 = 0.01sec)
        :CFSXX#   (Configure Speed) Set time to wait until second press if high speed requested
                   (# of 0.68ms to wait, default = 73 = 0.05sec)
        :RO       Read focus ON time
        :RF       Read focus off time
        :RS       Read Speed time
        :RT     Read Temperature
 */
class NFocus : public INDI::Focuser
{
    public:
        NFocus();

        virtual bool Handshake() override;
        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual void TimerHit() override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual bool AbortFocuser() override;

        bool saveConfigItems(FILE *fp) override;

    private:
        bool readTemperature();
        bool readMotorSettings();
        bool setMotorSettings(double onTime, double offTime, double fastDelay);

        // Utility
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        bool getStartupValues();
        void hexDump(char * buf, const char * data, int size);
        bool isMoving();

        /////////////////////////////////////////////////////////////////////////////
        /// Properties
        /////////////////////////////////////////////////////////////////////////////
        INumber TemperatureN[1];
        INumberVectorProperty TemperatureNP;

        INumber SettingsN[3];
        INumberVectorProperty SettingsNP;
        enum
        {
            SETTING_ON_TIME,
            SETTING_OFF_TIME,
            SETTING_MODE_DELAY,
        };

        /////////////////////////////////////////////////////////////////////////////
        /// Class Variables
        /////////////////////////////////////////////////////////////////////////////
        uint16_t m_TargetPosition { 0 };
        uint16_t m_TemperatureCounter { 0 };

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * SETTINGS_TAB = "Settings";
        // '#' is the stop char
        static const char NFOCUS_STOP_CHAR { 0x23 };
        // Update temperature every 10x POLLMS. For 500ms, we would
        // update the temperature one every 5 seconds.
        static constexpr const uint8_t NFOCUS_TEMPERATURE_FREQ {10};
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t NFOCUS_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t NFOCUS_LEN {64};
};
