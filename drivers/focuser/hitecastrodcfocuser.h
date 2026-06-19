/*******************************************************************************
  Copyright(c) 2016 Andy Kirkham. All rights reserved.

 HitecAstroDCFocuser Focuser

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

#ifdef _USE_SYSTEM_HIDAPILIB
#include <hidapi/hidapi.h>
#else
#include <indi_hidapi.h>
#endif

#include "indifocuser.h"
#include "indiusbdevice.h"

class HitecAstroDCFocuser : public INDI::Focuser, public INDI::USBDevice
{
    public:
        typedef enum { IDLE, SLEWING } STATE;

        HitecAstroDCFocuser();
        virtual ~HitecAstroDCFocuser() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool saveConfigItems(FILE *fp) override;

        bool Connect() override;
        bool Disconnect() override;

        void TimerHit() override;

        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
        virtual bool ReverseFocuser(bool enabled) override
        {
            INDI_UNUSED(enabled);
            return true;
        }

    private:
        hid_device *m_HIDHandle;
        char m_StopChar;
        STATE m_State;
        uint16_t m_Duration;

        //        INumber MaxPositionN[1];
        //        INumberVectorProperty MaxPositionNP;

        INDI::PropertyNumber SlewSpeedNP {1};

        //        ISwitch ReverseDirectionS[1];
        //        ISwitchVectorProperty ReverseDirectionSP;
};
