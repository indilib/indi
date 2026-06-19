/*******************************************************************************
  Copyright(c) 2026 Dominik Merkel. All rights reserved.

  Pegasus FlatMaster Neo

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
#include "indidustcapinterface.h"
#include "indiweatherinterface.h"

#include <vector>
#include <string>


class PegasusFlatMasterNeo : public INDI::DefaultDevice, public INDI::LightBoxInterface, public INDI::DustCapInterface,
    public INDI::WeatherInterface
{
    public:
        PegasusFlatMasterNeo();
        virtual ~PegasusFlatMasterNeo() override = default;

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

        virtual IPState ParkCap() override;
        virtual IPState UnParkCap() override;
        virtual IPState AbortCap() override;

        virtual IPState updateWeather() override { return IPS_OK; }
        virtual void TimerHit() override;

    private:
        bool Ack();
        bool getStatusData();
        bool getWeatherData();
        bool getWeatherOffsets();
        bool setTemperatureOffset(double value);
        bool setHumidityOffset(double value);
        bool setDewHeater(uint8_t value);
        bool setAutoDew(bool enable);
        bool setCapAngle(uint16_t angle);
        std::vector<std::string> split(const std::string &input, const std::string &sep);

        int PortFD{ -1 };
        bool sendCommand(const char* cmd, char* response);
        void updateFirmwareVersion();

        // Firmware version
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[1] {};

        // Dew Control
        INDI::PropertyNumber DewHeaterNP {1};
        INDI::PropertySwitch AutoDewSP {2};

        // Cap angle
        INDI::PropertyNumber CapAngleNP {1};

        // Weather offsets (CT / CH / CR)
        INDI::PropertyNumber WeatherOffsetNP {2};

        // Device overview status (FA command)
        INDI::PropertyNumber DeviceStatusNP {9};

        Connection::Serial *serialConnection{ nullptr };

        std::vector<std::string> lastWeatherData;
        std::vector<std::string> lastStatusData;

        static constexpr const char *ENVIRONMENT_TAB {"Environment"};
        static constexpr const char *LIGHTBOX_TAB {"Light Box"};
        static constexpr const char *DUSTCAP_TAB {"Dust Cap"};
        static constexpr const char *DEW_TAB {"Dew Control"};
        static constexpr const char *STATUS_TAB {"Overview"};
        static constexpr const uint8_t NEO_LEN {64};

        // FA response field indices
        // FA response: "FMNEO:lightIntensity:lightActive:capActual:capTarget:capStatus:v6:autoDew:v8:v9"
        enum
        {
            FA_NAME = 0,
            FA_LIGHT_INTENSITY,
            FA_LIGHT_ACTIVE,
            FA_CAP_ACTUAL_ANGLE,
            FA_CAP_TARGET_ANGLE,
            FA_CAP_STATUS,
            FA_VALUE_6,
            FA_AUTO_DEW_STATUS,
            FA_VALUE_8,
            FA_VALUE_9,
            FA_N,
        };

        enum CapMotionStatus
        {
            CAP_STATUS_PARKED   = 1,
            CAP_STATUS_UNPARKED = 3,
        };

        // DeviceStatusNP indices (FA[1..9])
        enum
        {
            STATUS_LIGHT_INTENSITY = 0,
            STATUS_LIGHT_ACTIVE,
            STATUS_CAP_ACTUAL_ANGLE,
            STATUS_CAP_TARGET_ANGLE,
            STATUS_CAP_STATUS,
            STATUS_VALUE_6,
            STATUS_AUTO_DEW,
            STATUS_VALUE_8,
            STATUS_VALUE_9,
        };

        // ES response field indices
        enum
        {
            WI_NAME = 0,
            WI_TEMPERATURE,
            WI_HUMIDITY,
            WI_DEWPOINT,
            WI_N,
        };

        // CR response field indices
        enum
        {
            CR_PREFIX = 0,
            CR_TEMP_OFFSET,
            CR_HUM_OFFSET,
            CR_N,
        };

        // WeatherOffsetNP indices
        enum
        {
            OFFSET_TEMPERATURE = 0,
            OFFSET_HUMIDITY,
        };
};
