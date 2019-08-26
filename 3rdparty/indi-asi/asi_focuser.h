/*
    ZWO EFA Focuser
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

#include <chrono>

class ASIEAF : public INDI::Focuser
{
    public:
        ASIEAF(int id, const char * name, const int maxSteps);
        virtual ~ASIEAF() override = default;

        const char * getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        //virtual bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n) override;
        virtual bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;

        char m_Name[MAXINDINAME];

    protected:
        virtual bool Connect() override;
        virtual bool Disconnect() override;

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

        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool SetFocuserMaxPosition(uint32_t ticks) override;
        virtual bool SetFocuserBacklash(int32_t steps) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;

    private:
        /**
         * @brief sendCommand Send a string command to ASIEAF.
         * @param cmd Command to be sent, must already have the necessary delimeter ('#')
         * @param res If not nullptr, the function will read until it detects the default delimeter ('#') up to ML_RES length.
         *        if nullptr, no read back is done and the function returns true.
         * @return True if successful, false otherwise.
         */
        bool sendCommand(const char * cmd, char * res = nullptr);

        // Get initial focuser parameter when we first connect
        void GetFocusParams();
        // Read and update Temperature
        bool readTemperature();
        // Read and update Position
        bool readPosition();
        // Read max position
        bool readMaxPosition();
        // Read reverse state
        bool readReverse();
        // Read Beep state
        bool readBeep();
        // Are we moving?
        bool isMoving();
        // Read backlash
        bool readBacklash();

        bool gotoAbsolute(uint32_t position);
        //bool setBacklash(uint32_t ticks);

        double targetPos { 0 }, lastPos { 0 }, lastTemperature { 0 };

        // Read Only Temperature Reporting
        INumber TemperatureN[1];
        INumberVectorProperty TemperatureNP;

        // Beep
        ISwitch BeepS[2];
        ISwitchVectorProperty BeepSP;
        enum
        {
            BEEP_ON,
            BEEL_OFF
        };

        // Enable/Disable backlash
        //        ISwitch BacklashCompensationS[2];
        //        ISwitchVectorProperty FocuserBacklashSP;
        //        enum { BACKLASH_ENABLED, BACKLASH_DISABLED };

        //        // Backlash Value
        //        INumber BacklashN[1];
        //        INumberVectorProperty BacklashNP;

        const uint8_t m_ID;
        const int m_MaxSteps;
};
