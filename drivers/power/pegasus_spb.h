/*******************************************************************************
  Copyright(c) 2023 Efstathios Chrysikos. All rights reserved.

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
*/

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

class PegasusSPB : public INDI::DefaultDevice, public INDI::WeatherInterface, public INDI::PowerInterface
{

    public:
        PegasusSPB();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n) override;

        enum PORT_MODE
        {
            DEW = 0,
            POWER = 1
        };
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
        enum
        {
            SENSOR_AVG_AMPS,
            SENSOR_AMP_HOURS,
            SENSOR_WATT_HOURS,
            SENSOR_EXT_N
        };
    protected:
        const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual void TimerHit() override;

        virtual bool SetPowerPort(size_t port, bool enabled) override;
        virtual bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
        virtual bool SetVariablePort(size_t port, bool enabled, double voltage) override;
        virtual bool SetLEDEnabled(bool enabled) override;
        virtual bool SetAutoDewEnabled(size_t port, bool enabled) override;
        virtual bool SetUSBPort(size_t port, bool enabled) override;

        // Weather Overrides
        virtual IPState updateWeather() override
        {
            return IPS_OK;
        }

    private:
        bool Handshake();

        bool sendCommand(const char *cmd, char *res);
        bool setFixedPowerPortState(int portNumber, bool enabled);
        bool setPowerDewPortMode(int portNumber, int mode);
        int getPowerDewPortMode(int portNumber);
        int getDewPortPower(int portNumber);
        bool setDewPortPower(int portNumber, int power);
        bool setDewAutoState(bool enabled);
        bool setDewAggressiveness(uint8_t level);
        int getDewAggressiveness();
        bool setHumdityOffset(int level);
        int getHumidityOffset();
        bool setTemperatureOffset(int level);
        int getTemperatureOffset();
        bool getSensorData();
        bool getConsumptionData();
        bool getMetricsData();
        void updatePropertiesPowerDewMode(int portNumber, int mode);

        int PortFD { -1 };
        bool setupComplete { false };
        Connection::Serial *serialConnection { nullptr };

        std::vector<std::string> split(const std::string &input, const std::string &regex);
        std::vector<std::string> lastSensorData;
        std::vector<std::string> lastConsumptionData;
        std::vector<std::string> lastMetricsData;
        char stopChar { 0xD };
        static constexpr const uint8_t PEGASUS_TIMEOUT {3};
        static constexpr const uint8_t PEGASUS_LEN {128};
        static constexpr const char *ENVIRONMENT_TAB {"Environment"};
        double map(double value, double from1, double to1, double from2, double to2);

        ////////////////////////////////////////////////////////////////////////////////////
        /// Main Control
        ////////////////////////////////////////////////////////////////////////////////////

        // Power Sensors
        INDI::PropertyNumber ExtendedPowerNP {3};

        ////////////////////////////////////////////////////////////////////////////////////
        /// Adjustable Hub
        ////////////////////////////////////////////////////////////////////////////////////
        INDI::PropertySwitch PowerDewSwitchASP {2};
        INDI::PropertySwitch PowerDewSwitchBSP {2};
        INDI::PropertyNumber DewAggressNP {1};

        ////////////////////////////////////////////////////////////////////////////////////
        /// Sensor Offset
        ////////////////////////////////////////////////////////////////////////////////////
        INDI::PropertyNumber HumidityOffsetNP {1};
        INDI::PropertyNumber TemperatureOffsetNP {1};

};
