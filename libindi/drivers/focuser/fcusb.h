/*******************************************************************************
  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

 Shoestring FCUSB Focuser

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indifocuser.h"
#include "hidapi.h"

#include <map>

class FCUSB : public INDI::Focuser
{
    public:

        // Motor commands
        typedef enum
        {
            MOTOR_OFF = 0x0,
            MOTOR_REV = 0x1,
            MOTOR_FWD = 0x2
        } MotorBits;

        // PWM Freq Commands
        typedef enum
        {
            PWM_1_1  = 0x0,
            PWM_1_4  = 0x1,
            PWM_1_16 = 0x2
        } PWMBits;

        FCUSB();

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;

        static void timedMoveHelper(void * context);

    protected:
        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual void TimerHit() override;

        virtual bool SetFocuserSpeed(int speed) override;
        virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
        virtual bool AbortFocuser() override;
        virtual bool ReverseFocuser(bool enabled) override;

        virtual bool saveConfigItems(FILE * fp) override;

    private:
        // Gets the Motor, PWM, and LED states.
        bool getStatus();

        // Sets the Motor, PWM, and LED states
        bool setStatus();

        void timedMoveCallback();

        hid_device *handle { nullptr };

        uint8_t motorStatus { MOTOR_OFF };
        uint8_t pwmStatus { PWM_1_1 };
        int targetSpeed { 1 };
        struct timeval timedMoveEnd;

        // PWM Scaler
        ISwitchVectorProperty PWMScalerSP;
        ISwitch PWMScalerS[3];

        // Driver Timeout in ms
        static const uint16_t FC_TIMEOUT { 1000 };

        static const uint8_t FC_LED_RED { 0x10 };
        static const uint8_t FC_LED_ON { 0x20 };
};
