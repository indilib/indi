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

class AstrosphericWeather : public INDI::Weather
{
public:
    AstrosphericWeather();
    virtual ~AstrosphericWeather() = default;

    virtual const char *getDefaultName() override;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    virtual bool saveConfigItems(FILE *fp) override;

    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

protected:
    virtual IPState updateWeather() override;

private:
    INDI::PropertyText   APIKeyTP{1};
    INDI::PropertyNumber LocationNP{2};
    INDI::PropertySwitch ModeSP{2};
    INDI::PropertySwitch RefreshNowSP{1};
    INDI::PropertyText   TelescopeNameTP{1};
    INDI::PropertyText   WeatherSummaryTP{1};
    INDI::PropertyText   LastUpdateTP{1};
    INDI::PropertyText   ForecastValidUntilTP{1};

    INDI::PropertyNumber ForecastCloudCoverNP{8};
    INDI::PropertyNumber ForecastTemperatureNP{8};
    INDI::PropertyNumber ForecastWindSpeedNP{8};
    INDI::PropertyNumber ForecastSeeingNP{8};
    INDI::PropertyNumber ForecastTransparencyNP{8};

    enum
    {
        LOCATION_LATITUDE,
        LOCATION_LONGITUDE
    };

    std::vector<double> cloudCover, temperature, windSpeed, dewPoint, windDirection, seeing, transparency;
    time_t forecastStartTime;
    int    forecastHours;
    bool   forecastValid;
    time_t lastFetchTime;
    int    apiCreditsUsed;
    bool   locationReceived;

    bool fetchDataFromAPI(std::string &responseBody);
    bool parseJSONResponse(const std::string &jsonResponse);
    static time_t parseUTCDateTime(const std::string &dateTimeStr);
    std::string buildWeatherSummary(double cloud, double temp, double wind, double dew, double dir, double see, double trans);
    void updateSummaryText(const std::string &text);
    void updateForecastProperties(int offset);
};

#endif // INDI_ASTROSPHERIC_WEATHER_H
