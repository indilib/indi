/*******************************************************************************
  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

  Pegasus Pocket Power Box

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
#include "indipowerinterface.h"

#include <vector>
#include <stdint.h>

namespace Connection
{
class Serial;
}

class PegasusPPB : public INDI::DefaultDevice, public INDI::WeatherInterface, public INDI::PowerInterface
{
    public:
        PegasusPPB();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

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

        // Power Interface Overrides
        virtual bool SetPowerPort(size_t port, bool enabled) override;
        virtual bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
        virtual bool SetVariablePort(size_t port, bool enabled, double voltage) override;
        virtual bool SetLEDEnabled(bool enabled) override;
        virtual bool SetAutoDewEnabled(size_t port, bool enabled) override;
        virtual bool CyclePower() override;
        virtual bool SetUSBPort(size_t port, bool enabled) override;


    private:
        bool Handshake();

        // Get Data
        bool sendFirmware();
        bool getSensorData();
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
            PA_DSLR_STATUS,
            PA_DEW_1,
            PA_DEW_2,
            PA_AUTO_DEW,
            PA_N,
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

        INDI::PropertySwitch DSLRPowerSP {2};

        // Select which power is ON on bootup
        INDI::PropertySwitch PowerOnBootSP {4};
        enum
        {
          POWER_PORT_1,
          POWER_PORT_2,
          POWER_PORT_3,
          POWER_PORT_4
        };

        std::vector<std::string> lastSensorData;
        char stopChar { 0xD };

        static constexpr const uint8_t PEGASUS_TIMEOUT {3};
        static constexpr const uint8_t PEGASUS_LEN {128};
        static constexpr const char *DEW_TAB {"Dew"};
        static constexpr const char *ENVIRONMENT_TAB {"Environment"};
};
