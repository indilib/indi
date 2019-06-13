/*******************************************************************************
  Copyright(c) 2019 Wouter van Reeven. All rights reserved.

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

    // FCUSB Motor State
    typedef enum { FC_NOT_MOVING,
                   FC_MOVING_IN,
                   FC_MOVING_OUT,
                 } FC_MOTOR;


        FCUSB();

        const char *getDefaultName() override;
        virtual bool initProperties() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual void TimerHit() override;

        virtual bool SetFocuserSpeed(int speed) override;
        virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
        virtual bool AbortFocuser() override;

    private:
        bool getStatus();

        hid_device *handle { nullptr };
        FC_MOTOR m_Motor { FC_NOT_MOVING };

        // Driver Timeout in ms
        static const uint16_t FC_TIMEOUT { 1000 };
        static const std::map<FC_MOTOR, std::string> MotorMap;
};
