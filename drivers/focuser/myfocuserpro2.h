/*
    MyFocuserPro2 Focuser
    Copyright (C) 2019 Alan Townshend

    Based on Moonlite Focuser
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

class MyFocuserPro2 : public INDI::Focuser
{
    public:
        MyFocuserPro2();
        virtual ~MyFocuserPro2() override = default;

        typedef enum {FOCUS_FULL_STEP,  FOCUS_HALF_STEP, FOCUS_QUARTER_STEP, FOCUS_EIGHTH_STEP, FOCUS_SIXTEENTH_STEP, FOCUS_THIRTYSECOND_STEP } FocusStepMode;

        typedef enum {COIL_POWER_OFF, COIL_POWER_ON } CoilPower;

        typedef enum {DISPLAY_OFF, DISPLAY_ON } DisplayMode;

        typedef enum {TEMP_COMPENSATE_ENABLE, TEMP_COMPENSATE_DISABLE } TemperatureCompensate;

        const char * getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n) override;
        virtual bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;

        static void timedMoveHelper(void * context);

    protected:
        /**
         * @brief Handshake Try to communicate with Focuser and see if there is a valid response.
         * @return True if communication is successful, false otherwise.
         */
        virtual bool Handshake() override;

        /**
         * @brief MoveFocuser Move focuser in a specific direction and speed for period of time.
         * @param dir Direction of motion. Inward or Outward
         * @param speed Speed of motion
         * @param duration Timeout in milliseconds.
         * @return IPS_BUSY if motion is in progress. IPS_OK if motion is small and already complete. IPS_ALERT for trouble.
         */
        virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;

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
        virtual bool SetFocuserMaxPosition(uint32_t ticks) override;
        virtual bool SetFocuserSpeed(int speed) override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE * fp) override;

    private:
        bool Ack();
        /**
         * @brief sendCommand Send a string command to MyFocuserPro2.
         * @param cmd Command to be sent, must already have the necessary delimeter ('#')
         * @param res If not nullptr, the function will read until it detects the default delimeter ('#') up to ML_RES length.
         *        if nullptr, no read back is done and the function returns true.
         * @return True if successful, false otherwise.
         */
        bool sendCommand(const char * cmd, char * res = nullptr);

        // Get initial focuser parameter when we first connect
        void getStartupValues();

        // Read and update Step Mode
        bool readStepMode();

        // Read and update Temperature
        bool readTemperature();

        // Read and Update T.Compensate
        bool readTempCompensateEnable();

        //Read and Update teh Temperature Coefficient
        bool readTempeartureCoefficient();

        // Read and update Position
        bool readPosition();

        // Read and update speed
        bool readSpeed();

        //Read and Update Maximum step position
        bool readMaxPos();

        // Are we moving?
        bool isMoving();

        bool readCoilPowerState();

        bool readDisplayVisible();

        bool readReverseDirection();

        bool readBacklashInEnabled();
        bool readBacklashOutEnabled();

        bool readBacklashInSteps();
        bool readBacklashOutSteps();

        bool setBacklashInSteps(int16_t steps);
        bool setBacklashOutSteps(int16_t steps);

        bool setBacklashInEnabled(bool enabled);
        bool setBacklashOutEnabled(bool enabled);

        bool MoveFocuser(uint32_t position);

        bool setStepMode(FocusStepMode mode);

        bool setSpeed(uint16_t speed);

        bool setDisplayVisible(DisplayMode enable);

        bool setGotoHome();

        bool setCoilPowerState(CoilPower enable);

        bool setTemperatureCelsius();

        bool setTemperatureCalibration(double calibration);

        bool setTemperatureCoefficient(double coefficient);

        bool setTemperatureCompensation(bool enable);

        void timedMoveCallback();

        double targetPos { 0 }, lastPos { 0 }, lastTemperature { 0 };

        // Read Only Temperature Reporting
        INumber TemperatureN[1];
        INumberVectorProperty TemperatureNP;

        // Full/Half...32th Step modes
        ISwitch StepModeS[6];
        ISwitchVectorProperty StepModeSP;

        // Backlash In settings
        INumber BacklashInStepsN[1];
        INumberVectorProperty BacklashInStepsNP;

        // Backlash Out Setting
        INumber BacklashOutStepsN[1];
        INumberVectorProperty BacklashOutStepsNP;

        // Temperature Settings
        INumber TemperatureSettingN[1];
        INumberVectorProperty TemperatureSettingNP;

        // Temperature Compensation Enable/Disable
        ISwitch TemperatureCompensateS[2];
        ISwitchVectorProperty TemperatureCompensateSP;

        //Display On Off
        ISwitch DisplayS[2];
        ISwitchVectorProperty DisplaySP;

        //Goto Home Position
        ISwitch GotoHomeS[1];
        ISwitchVectorProperty GotoHomeSP;

        //CoilPower On Off
        ISwitch CoilPowerS[2];
        ISwitchVectorProperty CoilPowerSP;

        //Backlash In Enable
        ISwitch BacklashInS[2];
        ISwitchVectorProperty BacklashInSP;

        //Backlash Out Enable
        ISwitch BacklashOutS[2];
        ISwitchVectorProperty BacklashOutSP;

        //Focus Speed
        ISwitch FocusSpeedS[3];
        ISwitchVectorProperty FocusSpeedSP;

        // MyFocuserPro2 Buffer
        static const uint8_t ML_RES { 32 };

        // MyFocuserPro2 Delimeter
        static const char ML_DEL { '#' };

        // MyFocuserPro2 Timeout
        static const uint8_t ML_TIMEOUT { 5 };

        // MyFocuserPro2 minimum supported firmware
        static const int32_t MINIMUM_FIRMWARE_VERSION { 291 };
};
