/*******************************************************************************
  Copyright(c) 2024 WeatherFlow Tempest Weather Driver

  INDI WeatherFlow Tempest Weather Driver

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
#include "indipropertytext.h"
#include "indipropertynumber.h"

#include <memory>
#include <string>
#include <chrono>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

class WeatherFlow : public INDI::Weather
{
    public:
        WeatherFlow();
        virtual ~WeatherFlow();

        //  Generic indi device entries
        bool Connect() override;
        bool Disconnect() override;
        const char *getDefaultName() override;

        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual IPState updateWeather() override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;

    private:
        // Configuration properties
        INDI::PropertyText wfAPIKeyTP{1};
        INDI::PropertyText wfStationIDTP{1};
        INDI::PropertyNumber wfSettingsNP{3};

        // Settings indices
        enum
        {
            WF_UPDATE_INTERVAL,
            WF_CONNECTION_TIMEOUT,
            WF_RETRY_ATTEMPTS
        };

        // WeatherFlow API data structures
        struct WeatherFlowData
        {
            double air_temperature = 0.0;
            double relative_humidity = 0.0;
            double barometric_pressure = 0.0;
            double wind_avg = 0.0;
            double wind_gust = 0.0;
            double wind_direction = 0.0;
            double precip_accum_local_day = 0.0;
            double precip_rate = 0.0;
            double solar_radiation = 0.0;
            double uv = 0.0;
            std::chrono::system_clock::time_point timestamp;
        };

        // Private helper methods
        bool fetchStationInfo();
        bool fetchCurrentObservations();
        bool parseStationResponse(const std::string &response);
        bool parseObservationsResponse(const std::string &response);
        bool makeAPIRequest(const std::string &endpoint, std::string &response);
        bool retryRequest(const std::function<bool()> &request);

        // State variables
        std::string m_stationID;
        std::string m_deviceID;
        WeatherFlowData m_lastData;
        std::chrono::system_clock::time_point m_lastUpdate;
        bool m_isConnected = false;

        // API configuration
        const std::string API_BASE_URL = "https://swd.weatherflow.com/swd/rest/";
        const std::string STATIONS_ENDPOINT = "stations";
        const std::string OBSERVATIONS_ENDPOINT = "observations/station/";
        const std::string DEVICE_OBSERVATIONS_ENDPOINT = "observations/";

        // Rate limiting
        const int RATE_LIMIT_REQUESTS = 1000;
        const int RATE_LIMIT_PERIOD = 3600; // 1 hour in seconds
        std::chrono::system_clock::time_point m_lastRequestTime;
        int m_requestCount = 0;
};
