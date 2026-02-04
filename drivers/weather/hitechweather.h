/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI HiTech Weather Station Driver

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

#include "indiweather.h"

#ifdef _USE_SYSTEM_HIDAPILIB
#include <hidapi/hidapi.h>
#else
#include <indi_hidapi.h>
#endif

class HitechWeather : public INDI::Weather
{
    public:
        HitechWeather();
        virtual ~HitechWeather();

        //  Generic indi device entries
        bool Connect() override;
        bool Disconnect() override;
        const char *getDefaultName() override;

        virtual bool initProperties() override;

    protected:
        virtual IPState updateWeather() override;

    private:
        // HID device handle
        hid_device *m_hidHandle {nullptr};

        // HiTech Weather USB identifiers
        static constexpr unsigned short HITECH_VID = 0x04D8;
        static constexpr unsigned short HITECH_PID = 0xF772;

        // Command bytes
        static constexpr unsigned char CMD_GET_SKY_TEMP = 0x50;
        static constexpr unsigned char CMD_GET_AMBIENT = 0x5A;

        // Helper functions
        bool getSkyTemperature(double &skyTemp);
        bool getAmbientTemperature(double &ambientTemp);
        double calculateCloudCover(double ambientTemp, double skyTemp);
};
