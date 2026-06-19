/*******************************************************************************
  Copyright(c) 2014 Jasem Mutlaq. All rights reserved.
  Driver for Wake-on-LAN power control

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#pragma once

#include "defaultdevice.h"

class WakeOnLAN : public INDI::DefaultDevice
{
    public:
        WakeOnLAN();
        virtual ~WakeOnLAN() = default;

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;

    protected:
        bool Connect() override;
        bool Disconnect() override;
        void TimerHit() override;

    private:
        bool sendWakeOnLan(const char *macAddress);
        bool pingDevice(const char *ip);
        void checkPingStatus(int idx, INDI::PropertyText &devTP, INDI::PropertySwitch &sendSP);

        bool m_stopPing { false };

        // MAC Address properties for 4 devices (MAC + Alias + IP)
        INDI::PropertyText Device1TP {3};
        INDI::PropertyText Device2TP {3};
        INDI::PropertyText Device3TP {3};
        INDI::PropertyText Device4TP {3};

        // Send buttons for each MAC address
        INDI::PropertySwitch Send1SP {1};
        INDI::PropertySwitch Send2SP {1};
        INDI::PropertySwitch Send3SP {1};
        INDI::PropertySwitch Send4SP {1};
};
