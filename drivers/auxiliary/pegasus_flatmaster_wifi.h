/*******************************************************************************
  Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

  Pegasus FlatMaster WiFi

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

#include "indilightboxinterface.h"
#include "defaultdevice.h"


class PegasusFlatMasterWifi : public INDI::DefaultDevice, public INDI::LightBoxInterface
{
    public:
        PegasusFlatMasterWifi();
        virtual ~PegasusFlatMasterWifi() override = default;

        const char *getDefaultName() override;
        void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

    protected:
        virtual bool SetLightBoxBrightness(uint16_t value) override;
        virtual bool EnableLightBox(bool enable) override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual void TimerHit() override;

    private:
        bool Ack();
        bool getStatusData();
        bool sendCommand(const char *cmd, char *response);
        void updateFirmwareVersion();

        int PortFD { -1 };

        // Firmware version
        INDI::PropertyText FirmwareTP {1};

        // Uptime (read-only)
        INDI::PropertyNumber UptimeNP {1};

        // Reboot button
        INDI::PropertySwitch RebootSP {1};

        Connection::Serial *serialConnection { nullptr };

        // FA response field indices: FA:<brightness>:<power>
        enum
        {
            FA_PREFIX = 0,
            FA_BRIGHTNESS,
            FA_POWER,
            FA_N,
        };
};
