/*******************************************************************************
 Copyright(c) 2025 Jacob Nowatzke. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.

 Contributions:
 - Grok-3 (xAI)
 - GPT-4o (OpenAI)
*******************************************************************************/

#ifndef INDI_ASTROSPHERIC_WEATHER_H
#define INDI_ASTROSPHERIC_WEATHER_H

#include <indiweather.h>
#include <vector>
#include <string>
#include <ctime>

// AstrosphericWeather class inherits from INDI::Weather, providing weather forecast data via the Astrospheric API.
class AstrosphericWeather : public INDI::Weather
{
public:
    // Constructor for the AstrosphericWeather class.
    AstrosphericWeather();
    // Virtual destructor (default implementation).
    virtual ~AstrosphericWeather() = default;

    // Override of the getDefaultName method to return the name of the device.
    virtual const char *getDefaultName() override;

    // Override of initProperties to initialize the properties of the device.
    virtual bool initProperties() override;
    // Override of updateProperties to manage the properties when the device is connected or disconnected.
    virtual bool updateProperties() override;

    // Override of ISNewText to handle changes to text properties (e.g., API key).
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    // Override of ISNewNumber to handle changes to number properties (e.g., location coordinates).
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    // Override of ISNewSwitch to handle changes to switch properties (e.g., mode selection).
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    // Override of saveConfigItems to save the configuration of the device.
    virtual bool saveConfigItems(FILE *fp) override;

    // Override of Connect to handle the connection to the device.
    virtual bool Connect() override;
    // Override of Disconnect to handle the disconnection from the device.
    virtual bool Disconnect() override;
    // Override of updateLocation to receive geographic-location updates from EKOS or GPS.
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

protected:
    // Override of updateWeather to fetch and update weather forecast data.
    virtual IPState updateWeather() override;

private:
    // Property for the API key, a text input required for Astrospheric API access.
    INDI::PropertyText APIKeyTP{1};
    // Property for location coordinates (latitude and longitude).
    INDI::PropertyNumber LocationNP{2};
    // Property for mode selection (API or Simulated).
    INDI::PropertySwitch ModeSP{2};
    // Property to specify the telescope device to snoop for location.
    INDI::PropertyText TelescopeNameTP{1};
    // Current-conditions summary shown in Main Control tab.
    INDI::PropertyText WeatherSummaryTP{1};
    // Timestamp of the last successful API fetch.
    INDI::PropertyText LastUpdateTP{1};
    // UTC time at which the current forecast window expires.
    INDI::PropertyText ForecastValidUntilTP{1};

    // Forecast tab properties: next 24 hours in 3-hour steps (8 elements each).
    INDI::PropertyNumber ForecastCloudCoverNP{8};
    INDI::PropertyNumber ForecastTemperatureNP{8};
    INDI::PropertyNumber ForecastWindSpeedNP{8};
    INDI::PropertyNumber ForecastSeeingNP{8};
    INDI::PropertyNumber ForecastTransparencyNP{8};

    // Enumeration for the indices of the LocationNP property.
    enum
    {
        LOCATION_LATITUDE,    // Latitude in degrees
        LOCATION_LONGITUDE    // Longitude in degrees
    };

    // Vectors to store the 82-hour forecast data for each weather parameter.
    std::vector<double> cloudCover, temperature, windSpeed, dewPoint, windDirection, seeing, transparency;
    time_t forecastStartTime;  // Start time of the forecast in UTC
    int forecastHours;         // Number of hours in the forecast (should be 82)
    bool forecastValid;        // Flag indicating if the forecast is valid
    time_t lastFetchTime;      // Last time data was fetched from the API
    int apiCreditsUsed;        // Number of API credits used in the last 24 hours
    bool locationReceived;     // Flag to track if location data was received via snooping

    // Method to fetch data from the Astrospheric API using the provided latitude, longitude, and API key.
    bool fetchDataFromAPI(std::string &responseBody);
    // Method to parse the JSON response from the API and populate the forecast data vectors.
    bool parseJSONResponse(const std::string &jsonResponse);
    // Static method to parse UTC date-time strings into time_t.
    static time_t parseUTCDateTime(const std::string &dateTimeStr);

    std::string buildWeatherSummary(double cloud, double temp, double wind, double dew, double dir, double see, double trans);
    void updateSummaryText(const std::string &text);
    void updateForecastProperties(int offset);
};

#endif // INDI_ASTROSPHERIC_WEATHER_H
