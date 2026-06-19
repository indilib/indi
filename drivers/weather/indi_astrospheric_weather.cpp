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

#include "indi_astrospheric_weather.h"

#include <indicom.h>
#include <httplib.h>
#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif
using json = nlohmann::json;

#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <utility>

static constexpr size_t EXPECTED_FORECAST_HOURS = 82;
static constexpr int    FORECAST_CACHE_TTL_SECS  = 6 * 3600;
static constexpr int    FORECAST_STEPS            = 8;
static constexpr int    FORECAST_STEP_HOURS       = 3;

#define FORECAST_TAB "Forecast"

// Constructor for AstrosphericWeather.
AstrosphericWeather::AstrosphericWeather()
{
    // Set driver version to 0.2 (alpha).
    setVersion(0, 2);
    // Set connection type to none (weather driver, no physical device).
    setWeatherConnection(CONNECTION_NONE);
    // Initialize forecast-related variables.
    forecastValid = false;
    lastFetchTime = 0;
    forecastHours = 0;
    forecastStartTime = 0;
    apiCreditsUsed = 0;
    locationReceived = false;
}

// Returns the default name of the device.
const char *AstrosphericWeather::getDefaultName()
{
    return "Astrospheric Weather";
}

// Handles device connection.
bool AstrosphericWeather::Connect()
{
    setConnected(true);
    updateProperties();
    return true;
}

// Handles device disconnection.
bool AstrosphericWeather::Disconnect()
{
    setConnected(false);
    updateProperties();
    return true;
}

// Initializes all device properties.
bool AstrosphericWeather::initProperties()
{
    // Initialize base weather properties.
    INDI::Weather::initProperties();

    // Define API key property in Options tab.
    APIKeyTP[0].fill("API_KEY_VALUE", "Key", "");
    APIKeyTP.fill(getDeviceName(), "ASTROSPHERIC_API_KEY", "API Key", "Options", IP_RW, 60, IPS_IDLE);
    defineProperty(APIKeyTP);

    // Define location property in Options tab.
    LocationNP[LOCATION_LATITUDE].fill("LATITUDE", "Latitude (deg)", "%.4f", -90.0, 90.0, 0.0, 0.0);
    LocationNP[LOCATION_LONGITUDE].fill("LONGITUDE", "Longitude (deg)", "%.4f", -180.0, 360.0, 0.0, 0.0);
    LocationNP.fill(getDeviceName(), "LOCATION", "Location", "Options", IP_RW, 0, IPS_IDLE);
    defineProperty(LocationNP);

    // Define telescope name property for snooping location.
    TelescopeNameTP[0].fill("TELESCOPE_NAME", "Telescope", "Telescope Simulator");
    TelescopeNameTP.fill(getDeviceName(), "TELESCOPE_NAME", "Snoop Telescope", "Options", IP_RW, 60, IPS_IDLE);
    defineProperty(TelescopeNameTP);

    // Define mode switch property in Options tab (API vs Simulated).
    ModeSP[0].fill("API_MODE", "API Mode", ISS_OFF);
    ModeSP[1].fill("SIMULATED_MODE", "Simulated Mode", ISS_ON);
    ModeSP.fill(getDeviceName(), "WEATHER_MODE", "Mode", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    defineProperty(ModeSP);

    // Define weather parameters for the Parameters tab.
    addParameter("WEATHER_CLOUD_COVER", "Cloud Cover (%)", 0, 100, 50);
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -50, 50, 0);
    addParameter("WEATHER_WIND_SPEED", "Wind Speed (kph)", 0, 200, 50);
    addParameter("WEATHER_DEW_POINT", "Dew Point (C)", -50, 50, 0);
    addParameter("WEATHER_WIND_DIRECTION", "Wind Direction (°)", 0, 360, 0);
    addParameter("WEATHER_SEEING", "Seeing (0–5)", 0, 5, 0);
    addParameter("WEATHER_TRANSPARENCY", "Transparency (0–27+)", 0, 30, 0);

    // Set cloud cover as the critical parameter for weather alerts.
    setCriticalParameter("WEATHER_CLOUD_COVER");

    // Override the base-class default update period from 60 s to 30 minutes.
    UpdatePeriodNP[0].setValue(1800);

    // Current-conditions summary (always visible in Main Control tab).
    WeatherSummaryTP[0].fill("SUMMARY", "Weather Summary", "N/A");
    WeatherSummaryTP.fill(getDeviceName(), "WEATHER_SUMMARY", "Status", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);
    defineProperty(WeatherSummaryTP);

    // Last successful API fetch timestamp (always visible in Main Control tab).
    LastUpdateTP[0].fill("TIMESTAMP", "Last API Fetch", "Never");
    LastUpdateTP.fill(getDeviceName(), "WEATHER_LAST_UPDATE", "Last Update", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);
    defineProperty(LastUpdateTP);

    // Forecast tab: 8 steps × 3 h = 24-hour lookahead.
    static const char *stepNames[]  = {"HOUR_0",  "HOUR_3",  "HOUR_6",  "HOUR_9",
                                        "HOUR_12", "HOUR_15", "HOUR_18", "HOUR_21"};
    static const char *stepLabels[] = {"Now",  "+3h",  "+6h",  "+9h",
                                        "+12h", "+15h", "+18h", "+21h"};
    for (int i = 0; i < FORECAST_STEPS; i++)
    {
        ForecastCloudCoverNP[i].fill(stepNames[i],  stepLabels[i], "%.1f",  0,   100, 0, 0);
        ForecastTemperatureNP[i].fill(stepNames[i], stepLabels[i], "%.1f", -50,   50, 0, 0);
        ForecastWindSpeedNP[i].fill(stepNames[i],   stepLabels[i], "%.1f",  0,   200, 0, 0);
        ForecastSeeingNP[i].fill(stepNames[i],      stepLabels[i], "%.2f",  0,     5, 0, 0);
        ForecastTransparencyNP[i].fill(stepNames[i],stepLabels[i], "%.1f",  0,    30, 0, 0);
    }
    ForecastCloudCoverNP.fill(getDeviceName(),  "FORECAST_CLOUD_COVER",  "Cloud Cover (%)",    FORECAST_TAB, IP_RO, 60, IPS_IDLE);
    ForecastTemperatureNP.fill(getDeviceName(), "FORECAST_TEMPERATURE",  "Temperature (C)",    FORECAST_TAB, IP_RO, 60, IPS_IDLE);
    ForecastWindSpeedNP.fill(getDeviceName(),   "FORECAST_WIND_SPEED",   "Wind Speed (kph)",   FORECAST_TAB, IP_RO, 60, IPS_IDLE);
    ForecastSeeingNP.fill(getDeviceName(),      "FORECAST_SEEING",       "Seeing (0-5)",       FORECAST_TAB, IP_RO, 60, IPS_IDLE);
    ForecastTransparencyNP.fill(getDeviceName(),"FORECAST_TRANSPARENCY", "Transparency (0-27+)",FORECAST_TAB, IP_RO, 60, IPS_IDLE);

    // Add debug control for logging.
    addDebugControl();

    // Load saved configuration.
    loadConfig(true, "ASTROSPHERIC_API_KEY");
    loadConfig(true, "LOCATION");
    loadConfig(true, "TELESCOPE_NAME");
    loadConfig(true, "WEATHER_MODE");
    loadConfig(true, "WEATHER_UPDATE");

    // Start snooping on the telescope for location data.
    if (TelescopeNameTP[0].getText() && strlen(TelescopeNameTP[0].getText()) > 0)
    {
        IDSnoopDevice(TelescopeNameTP[0].getText(), "GEOGRAPHIC_COORD");
        LOGF_INFO("Snooping on telescope %s for location data.", TelescopeNameTP[0].getText());
    }

    return true;
}

// Updates properties based on connection state.
bool AstrosphericWeather::updateProperties()
{
    INDI::Weather::updateProperties();

    if (isConnected())
    {
        defineProperty(ForecastCloudCoverNP);
        defineProperty(ForecastTemperatureNP);
        defineProperty(ForecastWindSpeedNP);
        defineProperty(ForecastSeeingNP);
        defineProperty(ForecastTransparencyNP);
    }
    else
    {
        deleteProperty(ForecastCloudCoverNP);
        deleteProperty(ForecastTemperatureNP);
        deleteProperty(ForecastWindSpeedNP);
        deleteProperty(ForecastSeeingNP);
        deleteProperty(ForecastTransparencyNP);
    }

    return true;
}

// Handles updates to number properties (e.g., location, snooped telescope data).
bool AstrosphericWeather::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        // Handle updates to the LocationNP property.
        if (LocationNP.isNameMatch(name))
        {
            LocationNP.update(values, names, n);
            LocationNP.setState(IPS_OK);
            LocationNP.apply();
            LOGF_INFO("Location updated: Latitude=%.4f, Longitude=%.4f",
                      LocationNP[LOCATION_LATITUDE].getValue(),
                      LocationNP[LOCATION_LONGITUDE].getValue());
            // Invalidate forecast when location changes.
            forecastValid = false;
            locationReceived = true; // Consider manually set location as received
            return true;
        }
    }
    // Handle snooped GEOGRAPHIC_COORD data from the configured telescope only.
    if (strcmp(name, "GEOGRAPHIC_COORD") == 0 &&
        dev && TelescopeNameTP[0].getText() && strcmp(dev, TelescopeNameTP[0].getText()) == 0)
    {
        double lat = 0.0, lon = 0.0;
        bool latFound = false, lonFound = false;

        for (int i = 0; i < n; i++)
        {
            if (strcmp(names[i], "LAT") == 0)
            {
                lat = values[i];
                latFound = true;
            }
            else if (strcmp(names[i], "LONG") == 0)
            {
                lon = values[i];
                lonFound = true;
            }
        }

        if (latFound && lonFound)
        {
            LocationNP[LOCATION_LATITUDE].setValue(lat);
            LocationNP[LOCATION_LONGITUDE].setValue(lon);
            LocationNP.setState(IPS_OK);
            LocationNP.apply();
            locationReceived = true;
            LOGF_INFO("Snooped location from %s: Latitude=%.4f, Longitude=%.4f", dev, lat, lon);
            // Invalidate forecast when location updates.
            forecastValid = false;
        }
        else
        {
            LOGF_WARN("Snooped GEOGRAPHIC_COORD from %s incomplete: LAT=%s, LONG=%s",
                      dev, latFound ? "found" : "missing", lonFound ? "found" : "missing");
        }
        return true;
    }
    return INDI::Weather::ISNewNumber(dev, name, values, names, n);
}

// Handles updates to text properties (e.g., API key, telescope name).
bool AstrosphericWeather::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        // Handle updates to the API key.
        if (APIKeyTP.isNameMatch(name))
        {
            APIKeyTP.update(texts, names, n);
            APIKeyTP.setState(IPS_OK);
            APIKeyTP.apply();
            saveConfig(true, APIKeyTP.getName());
            forecastValid = false;
            LOG_INFO("API Key updated.");
            return true;
        }
        // Handle updates to the telescope name for snooping.
        if (TelescopeNameTP.isNameMatch(name))
        {
            TelescopeNameTP.update(texts, names, n);
            TelescopeNameTP.setState(IPS_OK);
            TelescopeNameTP.apply();
            saveConfig(true, TelescopeNameTP.getName());
            if (TelescopeNameTP[0].getText() && strlen(TelescopeNameTP[0].getText()) > 0)
            {
                IDSnoopDevice(TelescopeNameTP[0].getText(), "GEOGRAPHIC_COORD");
                LOGF_INFO("Now snooping on telescope %s for location data.", TelescopeNameTP[0].getText());
            }
            return true;
        }
    }
    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

// Handles updates to switch properties (e.g., mode switch).
bool AstrosphericWeather::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        if (ModeSP.isNameMatch(name))
        {
            ModeSP.update(states, names, n);
            ModeSP.setState(IPS_OK);
            ModeSP.apply();
            LOGF_INFO("Mode updated to: %s",
                      ModeSP[0].getState() == ISS_ON ? "API Mode" : "Simulated Mode");
            // Invalidate forecast when mode changes.
            forecastValid = false;
            return true;
        }
    }
    return INDI::Weather::ISNewSwitch(dev, name, states, names, n);
}

// Saves configuration items to a file.
bool AstrosphericWeather::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);
    APIKeyTP.save(fp);
    LocationNP.save(fp);
    TelescopeNameTP.save(fp);
    ModeSP.save(fp);
    return true;
}

// Fetches data from the Astrospheric API.
bool AstrosphericWeather::fetchDataFromAPI(std::string &responseBody)
{
    LOG_DEBUG("Fetching data from Astrospheric API...");
    const std::string endpoint = "/api/GetForecastData_V1";
    const std::string host = "astrosphericpublicaccess.azurewebsites.net";

    // Get latitude and longitude from LocationNP
    double lat = LocationNP[LOCATION_LATITUDE].getValue();
    double lon = LocationNP[LOCATION_LONGITUDE].getValue();

    // Convert longitude from [0, 360] to [-180, 180] for Astrospheric API
    if (lon > 180.0)
    {
        lon -= 360.0;
    }

    LOGF_DEBUG("Sending coordinates to API: Latitude=%.4f, Longitude=%.4f", lat, lon);

    json payload;
    payload["Latitude"] = lat;
    payload["Longitude"] = lon;
    payload["APIKey"] = APIKeyTP[0].getText();
    std::string jsonDataString = payload.dump();

    httplib::Client client(host.c_str());
    client.set_connection_timeout(15, 0);
    client.set_read_timeout(30, 0);
    auto res = client.Post(endpoint.c_str(), jsonDataString, "application/json");
    if (!res || res->status != 200)
    {
        LOGF_ERROR("API request failed: %d - %s", res ? res->status : -1, res ? res->body.c_str() : "No response");
        return false;
    }

    responseBody = res->body;
    LOGF_DEBUG("API response: %s", responseBody.c_str());
    return true;
}

// Parses the JSON response from the API.
bool AstrosphericWeather::parseJSONResponse(const std::string &jsonResponse)
{
    LOG_DEBUG("Parsing JSON response...");
    try
    {
        json j = json::parse(jsonResponse);
        std::string utcStartTimeStr = j["UTCStartTime"].get<std::string>();
        forecastStartTime = parseUTCDateTime(utcStartTimeStr);
        if (forecastStartTime == static_cast<time_t>(-1))
        {
            LOG_ERROR("Failed to parse UTCStartTime.");
            return false;
        }

        apiCreditsUsed = j["APICreditUsedToday"].get<int>();
        LOGF_INFO("API credits used today: %d", apiCreditsUsed);

        std::vector<double> newCloudCover, newTemperature, newWindSpeed,
                            newDewPoint, newWindDirection, newSeeing, newTransparency;

        for (const auto &hour : j["RDPS_CloudCover"]) newCloudCover.push_back(hour["Value"]["ActualValue"].get<double>());
        for (const auto &hour : j["RDPS_Temperature"]) newTemperature.push_back(hour["Value"]["ActualValue"].get<double>() - 273.15);
        for (const auto &hour : j["RDPS_WindVelocity"]) newWindSpeed.push_back(hour["Value"]["ActualValue"].get<double>() * 3.6);
        for (const auto &hour : j["RDPS_DewPoint"]) newDewPoint.push_back(hour["Value"]["ActualValue"].get<double>() - 273.15);
        for (const auto &hour : j["RDPS_WindDirection"]) newWindDirection.push_back(hour["Value"]["ActualValue"].get<double>());
        for (const auto &hour : j["Astrospheric_Seeing"]) newSeeing.push_back(hour["Value"]["ActualValue"].get<double>());
        for (const auto &hour : j["Astrospheric_Transparency"]) newTransparency.push_back(hour["Value"]["ActualValue"].get<double>());

        if (newCloudCover.size() != EXPECTED_FORECAST_HOURS || newTemperature.size() != EXPECTED_FORECAST_HOURS ||
            newWindSpeed.size() != EXPECTED_FORECAST_HOURS || newDewPoint.size() != EXPECTED_FORECAST_HOURS ||
            newWindDirection.size() != EXPECTED_FORECAST_HOURS || newSeeing.size() != EXPECTED_FORECAST_HOURS ||
            newTransparency.size() != EXPECTED_FORECAST_HOURS)
        {
            LOGF_ERROR("Forecast data length mismatch: expected %zu hours.", EXPECTED_FORECAST_HOURS);
            return false;
        }

        cloudCover    = std::move(newCloudCover);
        temperature   = std::move(newTemperature);
        windSpeed     = std::move(newWindSpeed);
        dewPoint      = std::move(newDewPoint);
        windDirection = std::move(newWindDirection);
        seeing        = std::move(newSeeing);
        transparency  = std::move(newTransparency);
        forecastHours = static_cast<int>(cloudCover.size());
        forecastValid = true;
        lastFetchTime = time(nullptr);

        char timeBuf[32];
        struct tm tmInfo = {};
        gmtime_r(&lastFetchTime, &tmInfo);
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M UTC", &tmInfo);
        LastUpdateTP[0].setText(timeBuf);
        LastUpdateTP.setState(IPS_OK);
        LastUpdateTP.apply();

        LOGF_INFO("Parsed forecast for %d hours starting at %s.", forecastHours, utcStartTimeStr.c_str());
        return true;
    }
    catch (const json::exception &e)
    {
        LOGF_ERROR("JSON parsing error: %s", e.what());
        return false;
    }
}

// Parses UTC date-time strings into time_t.
time_t AstrosphericWeather::parseUTCDateTime(const std::string &dateTimeStr)
{
    std::tm tm = {};
    std::istringstream ss(dateTimeStr);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) return static_cast<time_t>(-1);
#if defined(_WIN32)
    return _mkgmtime(&tm);
#else
    return timegm(&tm);
#endif
}

// Updates weather data based on mode (API or Simulated).
IPState AstrosphericWeather::updateWeather()
{
    LOG_DEBUG("Updating weather...");
    if (!isConnected())
    {
        LOG_ERROR("Not connected. Please connect the device first.");
        return IPS_ALERT;
    }

    // Wait for location data before proceeding
    if (!locationReceived)
    {
        LOG_INFO("Waiting for location data...");
        return IPS_BUSY;
    }

    if (ModeSP[0].getState() == ISS_ON) // API Mode
    {
        if (APIKeyTP[0].getText() == nullptr || strlen(APIKeyTP[0].getText()) == 0)
        {
            LOG_ERROR("API key is not set. Set it in the Options tab.");
            return IPS_ALERT;
        }
        time_t currentTime = time(nullptr);
        if (!forecastValid || (currentTime - lastFetchTime) > FORECAST_CACHE_TTL_SECS)
        {
            LOG_INFO("Fetching new forecast data...");
            std::string responseBody;
            bool refreshed = fetchDataFromAPI(responseBody) && parseJSONResponse(responseBody);
            if (!refreshed)
            {
                if (!forecastValid)
                {
                    LOG_ERROR("Failed to fetch forecast data and no cached data is available.");
                    return IPS_ALERT;
                }
                LOG_WARN("API refresh failed; continuing with cached forecast data.");
            }
        }

        time_t now = time(nullptr);
        int hourOffset = static_cast<int>((now - forecastStartTime) / 3600);
        if (hourOffset < 0)
            hourOffset = 0;
        if (hourOffset >= forecastHours)
        {
            LOGF_WARN("Forecast window exceeded (offset %d of %d); using last available hour.", hourOffset, forecastHours);
            hourOffset = forecastHours - 1;
            forecastValid = false;
        }

        setParameterValue("WEATHER_CLOUD_COVER", cloudCover[hourOffset]);
        setParameterValue("WEATHER_TEMPERATURE", temperature[hourOffset]);
        setParameterValue("WEATHER_WIND_SPEED", windSpeed[hourOffset]);
        setParameterValue("WEATHER_DEW_POINT", dewPoint[hourOffset]);
        setParameterValue("WEATHER_WIND_DIRECTION", windDirection[hourOffset]);
        setParameterValue("WEATHER_SEEING", seeing[hourOffset]);
        setParameterValue("WEATHER_TRANSPARENCY", transparency[hourOffset]);

        updateSummaryText(buildWeatherSummary(
            cloudCover[hourOffset], temperature[hourOffset], windSpeed[hourOffset],
            dewPoint[hourOffset], windDirection[hourOffset], seeing[hourOffset], transparency[hourOffset]));

        updateForecastProperties(hourOffset);

        LOGF_INFO("Weather updated for hour %d: Cloud=%.2f%%, Temp=%.2fC, Wind=%.2fkph",
                  hourOffset, cloudCover[hourOffset], temperature[hourOffset], windSpeed[hourOffset]);

        // Return IPS_OK — checkWeatherUpdate() applies ParametersNP state and fires syncCriticalParameters().
        return IPS_OK;
    }
    else // Simulated Mode
    {
        LOG_DEBUG("Updating weather in simulated mode...");
        setParameterValue("WEATHER_CLOUD_COVER", 50.0);
        setParameterValue("WEATHER_TEMPERATURE", 20.0);
        setParameterValue("WEATHER_WIND_SPEED", 10.0);
        setParameterValue("WEATHER_DEW_POINT", 10.0);
        setParameterValue("WEATHER_WIND_DIRECTION", 180.0);
        setParameterValue("WEATHER_SEEING", 2.5);
        setParameterValue("WEATHER_TRANSPARENCY", 15.0);

        updateSummaryText(buildWeatherSummary(50.0, 20.0, 10.0, 10.0, 180.0, 2.5, 15.0));

        // Simulated forecast: constant values across all steps.
        for (int i = 0; i < FORECAST_STEPS; i++)
        {
            ForecastCloudCoverNP[i].setValue(50.0);
            ForecastTemperatureNP[i].setValue(20.0);
            ForecastWindSpeedNP[i].setValue(10.0);
            ForecastSeeingNP[i].setValue(2.5);
            ForecastTransparencyNP[i].setValue(15.0);
        }
        ForecastCloudCoverNP.setState(IPS_OK);   ForecastCloudCoverNP.apply();
        ForecastTemperatureNP.setState(IPS_OK);  ForecastTemperatureNP.apply();
        ForecastWindSpeedNP.setState(IPS_OK);    ForecastWindSpeedNP.apply();
        ForecastSeeingNP.setState(IPS_OK);       ForecastSeeingNP.apply();
        ForecastTransparencyNP.setState(IPS_OK); ForecastTransparencyNP.apply();

        return IPS_OK;
    }
}

std::string AstrosphericWeather::buildWeatherSummary(double cloud, double temp, double wind, double dew, double dir, double see, double trans)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
             "Cloud: %.2f%%, Temp: %.2fC, Wind: %.2fkph, Dew: %.2fC, Dir: %.2f°, See: %.2f, Trans: %.2f",
             cloud, temp, wind, dew, dir, see, trans);
    return buf;
}

void AstrosphericWeather::updateSummaryText(const std::string &text)
{
    WeatherSummaryTP[0].setText(text.c_str());
    WeatherSummaryTP.setState(IPS_OK);
    WeatherSummaryTP.apply();
}

void AstrosphericWeather::updateForecastProperties(int offset)
{
    for (int i = 0; i < FORECAST_STEPS; i++)
    {
        int h = offset + i * FORECAST_STEP_HOURS;
        ForecastCloudCoverNP[i].setValue(h < forecastHours ? cloudCover[h]    : 0.0);
        ForecastTemperatureNP[i].setValue(h < forecastHours ? temperature[h]  : 0.0);
        ForecastWindSpeedNP[i].setValue(h < forecastHours ? windSpeed[h]      : 0.0);
        ForecastSeeingNP[i].setValue(h < forecastHours ? seeing[h]            : 0.0);
        ForecastTransparencyNP[i].setValue(h < forecastHours ? transparency[h]: 0.0);
    }
    ForecastCloudCoverNP.setState(IPS_OK);   ForecastCloudCoverNP.apply();
    ForecastTemperatureNP.setState(IPS_OK);  ForecastTemperatureNP.apply();
    ForecastWindSpeedNP.setState(IPS_OK);    ForecastWindSpeedNP.apply();
    ForecastSeeingNP.setState(IPS_OK);       ForecastSeeingNP.apply();
    ForecastTransparencyNP.setState(IPS_OK); ForecastTransparencyNP.apply();
}

// Global unique pointer to the AstrosphericWeather instance.
static std::unique_ptr<AstrosphericWeather> weather(new AstrosphericWeather());

// Factory function to create the device instance.
extern "C" INDI::DefaultDevice *createDevice()
{
    return weather.get();
}
