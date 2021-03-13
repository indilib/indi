/*******************************************************************************
  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

  Pegasus Pocket Power Box Advance

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
#include "indiweatherinterface.h"

#include <vector>
#include <stdint.h>

/* Smart Widget-Property */
#include "indipropertytext.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertylight.h"

namespace Connection
{
class Serial;
}

class PegasusPPBA : public INDI::DefaultDevice, public INDI::WeatherInterface
{
    public:
        PegasusPPBA();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;

        // Event loop
        virtual void TimerHit() override;

        // Weather Overrides
        virtual IPState updateWeather() override
        {
            return IPS_OK;
        }


    private:
        bool Handshake();

        // Get Data
        bool sendFirmware();
        bool getSensorData();
        bool getConsumptionData();
        bool getMetricsData();
        std::vector<std::string> split(const std::string &input, const std::string &regex);
        enum
        {
            PA_NAME,
            PA_VOLTAGE,
            PA_CURRENT,
            PA_TEMPERATURE,
            PA_HUMIDITY,
            PA_DEW_POINT,
            PA_PORT_STATUS,
            PA_ADJ_STATUS,
            PA_DEW_1,
            PA_DEW_2,
            PA_AUTO_DEW,
            PA_PWR_WARN,
            PA_PWRADJ,
            PA_N,
        };
        enum
        {
            PS_NAME,
            PS_AVG_AMPS,
            PS_AMP_HOURS,
            PS_WATT_HOURS,
            PS_UPTIME,
            PS_N,
        };
        enum
        {
            PC_NAME,
            PC_TOTAL_CURRENT,
            PC_12V_CURRENT,
            PC_DEWA_CURRENT,
            PC_DEWB_CURRENT,
            PC_UPTIME,
            PC_N,
        };

        // Device Control
        bool reboot();

        // Power
        bool setPowerEnabled(uint8_t port, bool enabled);
        bool setPowerOnBoot();

        // Dew
        bool setAutoDewEnabled(bool enabled);
        bool setDewPWM(uint8_t id, uint8_t value);

        /**
         * @brief sendCommand Send command to unit.
         * @param cmd Command
         * @param res if nullptr, respones is ignored, otherwise read response and store it in the buffer.
         * @return
         */
        bool sendCommand(const char *cmd, char *res);

        int PortFD { -1 };
        bool setupComplete { false };

        Connection::Serial *serialConnection { nullptr };

        ////////////////////////////////////////////////////////////////////////////////////
        /// Main Control
        ////////////////////////////////////////////////////////////////////////////////////
        /// Reboot Device
        INDI::PropertySwitch RebootSP {1};

        // Power Sensors
        INDI::PropertyNumber PowerSensorsNP {9};
        enum
        {
            SENSOR_VOLTAGE,
            SENSOR_CURRENT,
            SENSOR_AVG_AMPS,
            SENSOR_AMP_HOURS,
            SENSOR_WATT_HOURS,
            SENSOR_TOTAL_CURRENT,
            SENSOR_12V_CURRENT,
            SENSOR_DEWA_CURRENT,
            SENSOR_DEWB_CURRENT
        };

        ////////////////////////////////////////////////////////////////////////////////////
        /// Power Group
        ////////////////////////////////////////////////////////////////////////////////////

        INDI::PropertySwitch QuadOutSP {2};

        //        INDI::PropertySwitch AdjOutSP {2};
        //

        INDI::PropertySwitch AdjOutVoltSP {6};
        enum
        {
            ADJOUT_OFF,
            ADJOUT_3V,
            ADJOUT_5V,
            ADJOUT_8V,
            ADJOUT_9V,
            ADJOUT_12V,
        };

        // Select which power is ON on bootup
        INDI::PropertySwitch PowerOnBootSP {4};

        // Short circuit warn
        INDI::PropertyLight PowerWarnLP {1};

        INDI::PropertySwitch LedIndicatorSP {2};

        ////////////////////////////////////////////////////////////////////////////////////
        /// Dew Group
        ////////////////////////////////////////////////////////////////////////////////////

        // Auto Dew
        INDI::PropertySwitch AutoDewSP {2};

        // Dew PWM
        INDI::PropertyNumber DewPWMNP {2};
        enum
        {
            DEW_PWM_A,
            DEW_PWM_B,
        };

        ////////////////////////////////////////////////////////////////////////////////////
        /// Firmware
        ////////////////////////////////////////////////////////////////////////////////////
        INDI::PropertyText FirmwareTP {2};
        enum
        {
            FIRMWARE_VERSION,
            FIRMWARE_UPTIME,
        };

        std::vector<std::string> lastSensorData;
        std::vector<std::string> lastConsumptionData;
        std::vector<std::string> lastMetricsData;
        char stopChar { 0xD };

        static constexpr const uint8_t PEGASUS_TIMEOUT {3};
        static constexpr const uint8_t PEGASUS_LEN {128};
        static constexpr const char *DEW_TAB {"Dew"};
        static constexpr const char *ENVIRONMENT_TAB {"Environment"};
        static constexpr const char *FIRMWARE_TAB {"Firmware"};
};
