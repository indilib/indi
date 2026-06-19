/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Televue FocusMaster Driver.

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

#ifdef _USE_SYSTEM_HIDAPILIB
#include <hidapi/hidapi.h>
#else
#include <indi_hidapi.h>
#endif


class FocusMaster : public INDI::Focuser
{
    public:

        FocusMaster();
        virtual ~FocusMaster() = default;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        const char *getDefaultName();
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
        virtual IPState MoveAbsFocuser(uint32_t targetTicks);
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);

        virtual bool AbortFocuser();

    private:
        /**
         * @brief sendCommand Send a command to the focuser and optional read a response
         * @param command 2-byte unsigned char command.
         * @param response If nullptr (default) is passed, no response is read. If a valid pointer is passed, the response is read into the buffer up until
         *        MAX_FM_BUF bytes.
         * @return True if sending command is successful, false otherwise.
         */
        bool sendCommand(const uint8_t *command, char *response = nullptr);

        bool setPosition(uint32_t ticks);
        bool getPosition(uint32_t *ticks);

        bool sync(uint32_t ticks);

        hid_device *handle { nullptr };

        //uint32_t targetPosition { 0 };

        struct timeval focusMoveStart
        {
            0, 0
        };
        float focusMoveRequest { 0 };
        float CalcTimeLeft(timeval, float);

        // Sync to a particular position
        INumber SyncN[1];
        INumberVectorProperty SyncNP;

        // Full Forward / Reverse
        ISwitch FullMotionS[2];
        ISwitchVectorProperty FullMotionSP;

};
