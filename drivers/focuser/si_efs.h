/*******************************************************************************
  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

 Starlight Instruments EFS Focuser

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

#include <map>

class SIEFS : public INDI::Focuser
{
    public:

        // SI EFS State
        typedef enum { SI_NOOP,
                       SI_IN,
                       SI_OUT,
                       SI_GOTO,
                       SI_SET_POS,
                       SI_MAX_POS,
                       SI_FAST_IN  = 0x11,
                       SI_FAST_OUT = 0x12,
                       SI_HALT     = 0xFF,
                       SI_MOTOR_POLARITY = 0x61
                     } SI_COMMANDS;


        // SI EFS Motor State
        typedef enum { SI_NOT_MOVING,
                       SI_MOVING_IN,
                       SI_MOVING_OUT,
                       SI_LOCKED = 5,
                     } SI_MOTOR;


        SIEFS();

        const char *getDefaultName() override;
        virtual bool initProperties() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual void TimerHit() override;

        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual bool AbortFocuser() override;
        virtual bool SyncFocuser(uint32_t ticks) override;
        virtual bool SetFocuserMaxPosition(uint32_t ticks) override;

        virtual bool ReverseFocuser(bool enabled) override;

    private:
        /**
         * @brief setPosition Set Position (Either Absolute or Maximum)
         * @param ticks desired position
         * @param cmdCode 0x20 to set Absolute position. 0x22 to set Maximum position
         * @return True if successful, false otherwise.
         */
        bool setPosition(uint32_t ticks, uint8_t cmdCode);

        /**
         * @brief getPosition Get Position (Either Absolute or Maximum)
         * @param ticks pointer to store the returned position.
         * @param cmdCode 0x21 to get Absolute position. 0x23 to get Maximum position
         * @return True if successful, false otherwise.
         */
        bool getPosition(uint32_t *ticks, uint8_t cmdCode);

        // Set/Get Absolute Position
        bool setAbsPosition(uint32_t ticks);
        bool getAbsPosition(uint32_t *ticks);

        // Set/Get Maximum Position
        bool setMaxPosition(uint32_t ticks);
        bool getMaxPosition(uint32_t *ticks);

        // Polarity
        bool isReversed();
        bool setReversed(bool enabled);

        bool sendCommand(SI_COMMANDS targetCommand);
        bool getStatus();

        hid_device *handle { nullptr };
        SI_MOTOR m_Motor { SI_NOT_MOVING };
        int32_t simPosition { 0 };
        uint32_t targetPosition { 0 };

        // Driver Timeout in ms
        static const uint16_t SI_TIMEOUT { 1000 };

        static const std::map<SI_COMMANDS, std::string> CommandsMap;
        static const std::map<SI_MOTOR, std::string> MotorMap;
};
