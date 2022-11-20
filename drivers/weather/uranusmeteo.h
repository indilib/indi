/*******************************************************************************
  Copyright(c) 2022 Jasem Mutlaq. All rights reserved.

  Pegasus Uranus Meteo Sensor Driver.

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

#include "indigps.h"
#include "indiweatherinterface.h"

#include <vector>
#include <stdint.h>

namespace Connection
{
class Serial;
}

class UranusMeteo : public INDI::GPS, public INDI::WeatherInterface
{
    public:
        UranusMeteo();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        const char *getDefaultName() override;
        virtual bool saveConfigItems(FILE *fp) override;

        // Event loop
        virtual void TimerHit() override;

        // GPS Override
        virtual IPState updateGPS() override;

        // Weather Overrides
        virtual IPState updateWeather() override;

    private:
        bool Handshake();

        ////////////////////////////////////////////////////////////////////////////////////
        /// Sensors
        ////////////////////////////////////////////////////////////////////////////////////
        bool readSensors();
        enum
        {
            AmbientTemperature,
            RelativeHumidity,
            DewPoint,
            AbsolutePressure,
            RelativePressure,
            BarometricAltitude,
            SkyTemperature,
            InfraredTemperature,
            BatteryUsage,
            BatteryVoltage,
        };
        INDI::PropertyNumber SensorNP {10};

        ////////////////////////////////////////////////////////////////////////////////////
        /// Sky Quality
        ////////////////////////////////////////////////////////////////////////////////////
        bool readSkyQuality();
        enum
        {
            MPAS,
            NELM,
            FullSpectrum,
            VisualSpectrum,
            InfraredSpectrum,
        };
        INDI::PropertyNumber SkyQualityNP {5};

        ////////////////////////////////////////////////////////////////////////////////////
        /// Cloud Report
        ////////////////////////////////////////////////////////////////////////////////////
        bool readClouds();
        enum
        {
            TemperatureDifference,
            CloudIndex,
            CloudSkyTemperature,
            CloudAmbientTemperature,
            InfraredEmissivity,
        };
        INDI::PropertyNumber CloudsNP {5};

        ////////////////////////////////////////////////////////////////////////////////////
        /// GPS Report
        ////////////////////////////////////////////////////////////////////////////////////
        bool readGPS();
        enum
        {
            GPSFix,
            GPSTime,
            UTCOffset,
            Latitude,
            Longitude,
            SatelliteNumber,
            GPSSpeed,
            GPSBearing
        };
        INDI::PropertyNumber GPSNP {8};

        ////////////////////////////////////////////////////////////////////////////////////
        /// Moon Report
        ////////////////////////////////////////////////////////////////////////////////////
        bool readMoon();
        enum
        {
            MoonPhase,
            MoonType,
            MoonVisibility
        };
        INDI::PropertyNumber MoonNP {3};

        ////////////////////////////////////////////////////////////////////////////////////
        /// Twilight Report
        ////////////////////////////////////////////////////////////////////////////////////
        bool readTwilight();
        enum
        {
            TwilightSunrise,
            TwilightSunset,
            TwilightMoonPhase,
        };
        INDI::PropertyNumber TwilightNP {3};

        ////////////////////////////////////////////////////////////////////////////////////
        /// Device Config
        ////////////////////////////////////////////////////////////////////////////////////
        bool readConfig();
        enum
        {
            CONFIG_UNITS,
            CONFIG_NMEA_RJ12,
            CONFIG_OLED_ON,
            CONFIG_RESERVED_1,
            CONFIG_SLEEP,
            CONFIG_SQM_OFFSET,
            CONFIG_TIMEZONE_OFFSET,
            CONFIG_RESERVED_2,
            CONFIG_LAST_ID,
            CONFIG_TIMEZONE_ID,
        };
        INDI::PropertyNumber ConfigNP {10};

        ////////////////////////////////////////////////////////////////////////////////////
        /// Device Control
        ////////////////////////////////////////////////////////////////////////////////////
        bool reset();
        INDI::PropertySwitch ResetSP {1};

        INDI::PropertyText DeviceInfoTP {2};
        enum
        {
            DEVICE_RELEASE,
            DEVICE_SERIAL
        };

        ////////////////////////////////////////////////////////////////////////////////////
        /// Utility
        ////////////////////////////////////////////////////////////////////////////////////
        /**
         * @brief sendCommand Send command to unit.
         * @param cmd Command
         * @param res if nullptr, respones is ignored, otherwise read response and store it in the buffer.
         * @return
         */
        bool sendCommand(const char *cmd, char *res);
        std::vector<std::string> split(const std::string &input, const std::string &regex);
        bool setSystemTime(time_t &raw_time);

        ////////////////////////////////////////////////////////////////////////////////////
        /// Variables
        ////////////////////////////////////////////////////////////////////////////////////

        int PortFD { -1 };
        bool m_SetupComplete { false };
        Connection::Serial *serialConnection { nullptr };

        std::vector<std::string> m_Sensors;
        std::vector<std::string> m_Clouds;
        std::vector<std::string> m_SkyQuality;
        std::vector<std::string> m_GPS;

        static constexpr const uint8_t PEGASUS_STOP_CHAR {0xA};
        static constexpr const uint8_t PEGASUS_TIMEOUT {3};
        static constexpr const uint8_t PEGASUS_LEN {128};
        static constexpr const char *SENSORS_TAB {"Sensors"};
        static constexpr const char *SKYQUALITY_TAB {"Sky Quality"};
        static constexpr const char *CLOUDS_TAB {"Clouds"};
        static constexpr const char *ENVIRONMENT_TAB {"Environment"};
        static constexpr const char *GPS_TAB {"GPS"};
};
