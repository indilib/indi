/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  SVBONYPowerBox

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
#include "indipowerinterface.h"

#include <vector>
#include <stdint.h>

#include "basedevice.h"

namespace Connection
{
class Serial;
}

class SVBONYPowerBox : public INDI::DefaultDevice, public INDI::PowerInterface
{
    public:
        SVBONYPowerBox();
    public:
        virtual ~SVBONYPowerBox() = default;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void TimerHit() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;

        // Implement virtual methods from INDI::PowerInterface
        virtual bool SetPowerPort(size_t port, bool enabled) override;
        virtual bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
        virtual bool SetVariablePort(size_t port, bool enabled, double voltage) override;
        virtual bool SetLEDEnabled(bool enabled) override;
        virtual bool SetAutoDewEnabled(size_t port, bool enabled) override;
        virtual bool CyclePower() override;
        virtual bool SetUSBPort(size_t port, bool enabled) override;

    private:
        bool Handshake();
        bool sendCommand(const unsigned char* cmd, unsigned int cmd_len, unsigned char* res, int res_len);
        int PortFD{-1};

        bool setupComplete { false };
        bool processButtonSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        void Get_State();
        
        static double convert4BytesToDouble(const unsigned char* data, double scale);

        // Dew point calculation helpers
        static double CalculateSVP(double temperature);
        static double CalculateVP(double humidity, double svp);
        static double CalculateDewPointFromVP(double vp);

        // Weather Sensors Indices
        enum
        {
            SVB_SENSOR_DS18B20_TEMP,   /*!< DS18B20 Temperature */
            SVB_SENSOR_SHT40_TEMP,     /*!< SHT40 Temperature */
            SVB_SENSOR_SHT40_HUMIDITY, /*!< SHT40 Humidity */
            SVB_SENSOR_DEW_POINT,      /*!< Dew Point */
            N_SVB_WEATHER_SENSORS     /*!< Number of weather sensors */
        };
        // Main Control - Overall Weather Sensors
        INDI::PropertyNumber WeatherSVBSensorsNP {N_SVB_WEATHER_SENSORS};

        Connection::Serial *serialConnection { nullptr };
};
