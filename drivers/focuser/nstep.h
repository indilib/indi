/*
  NStep Focuser

  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

  Thanks to Rigel Systems, especially Gene Nolan and Leon Palmer,
  for their support in writing this driver.

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

class NStep : public INDI::Focuser
{
    public:
        NStep();

        virtual bool Handshake() override;
        const char *getDefaultName() override;

        bool initProperties() override;
        bool updateProperties() override;

        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        void TimerHit() override;

        IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        IPState MoveAbsFocuser(uint32_t targetTicks) override;
        bool SetFocuserSpeed(int speed) override;
        bool SyncFocuser(uint32_t ticks) override;
        bool AbortFocuser() override;

        bool saveConfigItems(FILE *fp) override;

    private:

        ///////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////

        INumber TemperatureN[1];
        INumberVectorProperty TemperatureNP;

        ISwitch CompensationModeS[3];
        ISwitchVectorProperty CompensationModeSP;
        enum
        {
            COMPENSATION_MODE_OFF,
            COMPENSATION_MODE_ONE_SHOT,
            COMPENSATION_MODE_AUTO,
        };

        INumber CompensationSettingsN[4];
        INumberVectorProperty CompensationSettingsNP;
        enum
        {
            COMPENSATION_SETTING_CHANGE,
            COMPENSATION_SETTING_STEP,
            COMPENSATION_SETTING_BACKLASH,
            COMPENSATION_SETTING_TIMER,
        };

        ISwitch PrimeManualS[1];
        ISwitchVectorProperty PrimeManualSP;

        ISwitch SteppingModeS[3];
        ISwitchVectorProperty SteppingModeSP;
        enum
        {
            STEPPING_WAVE,
            STEPPING_HALF,
            STEPPING_FULL,
        };

        ISwitch CoilStatusS[2];
        ISwitchVectorProperty CoilStatusSP;
        enum
        {
            COIL_ENERGIZED_OFF,
            COIL_ENERGIZED_ON,
        };

        INumber SteppingPhaseN[1];
        INumberVectorProperty SteppingPhaseNP;

        INumber MaxSpeedN[1];
        INumberVectorProperty MaxSpeedNP;

        ///////////////////////////////////////////////////////////////////////////////
        /// Read Data From Controller
        ///////////////////////////////////////////////////////////////////////////////
        bool readTemperature();
        bool readPosition();
        bool readCompensationInfo();
        bool readSpeedInfo();
        bool readSteppingInfo();
        bool readCoilStatus();

        ///////////////////////////////////////////////////////////////////////////////
        /// Write Data to Controller
        ///////////////////////////////////////////////////////////////////////////////
        bool setCompensationMode(uint8_t mode);
        bool setCompensationSettings(double change, double move, double backlash, double timer);
        bool setSteppingPhase(uint8_t phase);
        bool setCoilStatus(uint8_t status);
        bool setMaxSpeed(uint8_t maxSpeed);

        ///////////////////////////////////////////////////////////////////////////////
        /// Utility Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        bool getStartupValues();
        void hexDump(char * buf, const char * data, int size);
        bool isMoving();

        /////////////////////////////////////////////////////////////////////////////
        /// Class Variables
        /////////////////////////////////////////////////////////////////////////////
        int32_t m_TargetDiff { 0 };
        uint16_t m_TemperatureCounter { 0 };

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        static constexpr const char * COMPENSATION_TAB = "Compensation";
        static constexpr const char * STEPPING_TAB = "Stepping";
        // '#' is the stop char
        static const char NSTEP_STOP_CHAR { 0x23 };
        // Update temperature every 10x POLLMS. For 500ms, we would
        // update the temperature one every 5 seconds.
        static constexpr const uint8_t NSTEP_TEMPERATURE_FREQ {10};
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t NSTEP_TIMEOUT {3};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t NSTEP_LEN {64};
};
