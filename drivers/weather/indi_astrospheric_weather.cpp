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

/*
 * Purpose: Serve the Astrospheric.com 82-hour RDPS forecast as an INDI weather
 * device, exposing current conditions (Parameters tab) and a 24-hour lookahead
 * (Forecast tab, 8 × 3-hour steps).  Requires an Astrospheric Professional
 * account with API access.
 *
 * Key design decisions:
 *   - Forecast data is cached for FORECAST_CACHE_TTL_SECS (6 h) to conserve
 *     API credits.  A fresh fetch is also triggered proactively when fewer than
 *     FORECAST_REFRESH_MARGIN_HOURS remain in the current window.
 *   - parseJSONResponse() stages new data in local vectors and only std::move()s
 *     them into class members on success, so a failed re-parse never corrupts
 *     a valid cached forecast.
 *   - updateWeather() only calls setParameterValue() and returns an IPState.
 *     ParametersNP bookkeeping and syncCriticalParameters() are owned by
 *     checkWeatherUpdate() in INDI::WeatherInterface and must not be duplicated.
 *   - All properties use the modern INDI C++ API (PropertyText, PropertyNumber,
 *     PropertySwitch with .fill()/.apply()/.setState()).  No IUFill*/IDSet*
 *     legacy calls are used.
 *   - Location is accepted from three sources (first one wins):
 *     1. Manual entry in the Options > Location property.
 *     2. Snooped GEOGRAPHIC_COORD from the configured Snoop Telescope device.
 *     3. EKOS/GPS push via the updateLocation() override.
 *   - API responses shorter than EXPECTED_FORECAST_HOURS are accepted with a
 *     warning; all arrays are truncated to the shortest returned length.
 */

#include "indi_astrospheric_weather.h"

#include <indicom.h>
#include <httplib.h>
#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif
using json = nlohmann::json;

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

static constexpr double kPI                            = 3.14159265358979323846;

static constexpr size_t EXPECTED_FORECAST_HOURS        = 82;
static constexpr int    FORECAST_CACHE_TTL_SECS        = 6 * 3600;
static constexpr int    FORECAST_REFRESH_MARGIN_HOURS  = 6;
static constexpr int    FORECAST_STEPS                 = 8;
static constexpr int    FORECAST_STEP_HOURS            = 3;
static constexpr int    API_CONNECT_TIMEOUT_SECS       = 15;
static constexpr int    API_READ_TIMEOUT_SECS          = 30;

#define FORECAST_TAB "Forecast"

AstrosphericWeather::AstrosphericWeather()
{
    setVersion(1, 0);
    setWeatherConnection(CONNECTION_NONE);
    forecastValid     = false;
    lastFetchTime     = 0;
    forecastHours     = 0;
    forecastStartTime = 0;
    apiCreditsUsed    = 0;
    locationReceived  = false;
}

// Returns the default name of the device.
const char *AstrosphericWeather::getDefaultName()
{
    return "Astrospheric Weather";
}

bool AstrosphericWeather::Connect()
{
    setConnected(true);
    updateProperties();
    if (ModeSP[0].getState() == ISS_ON && !locationReceived)
        LOG_WARN("No location configured. Enter coordinates in Options > Location, or configure a Snoop Telescope.");
    if (ModeSP[0].getState() == ISS_ON &&
        (APIKeyTP[0].getText() == nullptr || strlen(APIKeyTP[0].getText()) == 0))
        LOG_WARN("No API key configured. Enter your Astrospheric key in Options > API Key.");
    return true;
}

bool AstrosphericWeather::Disconnect()
{
    setConnected(false);
    updateProperties();
    return true;
}

// updateLocation() is called by the base class when EKOS or a GPS device pushes coordinates.
bool AstrosphericWeather::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    LocationNP[LOCATION_LATITUDE].setValue(latitude);
    LocationNP[LOCATION_LONGITUDE].setValue(longitude);
    LocationNP.setState(IPS_OK);
    LocationNP.apply();
    LOGF_INFO("Location updated from EKOS/GPS: Latitude=%.4f, Longitude=%.4f", latitude, longitude);
    locationReceived = true;
    forecastValid    = false;
    return true;
}

bool AstrosphericWeather::initProperties()
{
    INDI::Weather::initProperties();

    APIKeyTP[0].fill("API_KEY_VALUE", "Key", "");
    APIKeyTP.fill(getDeviceName(), "ASTROSPHERIC_API_KEY", "API Key", "Options", IP_RW, 60, IPS_IDLE);
    defineProperty(APIKeyTP);

    LocationNP[LOCATION_LATITUDE].fill("LATITUDE", "Latitude (deg)", "%.4f", -90.0, 90.0, 0.0, 0.0);
    LocationNP[LOCATION_LONGITUDE].fill("LONGITUDE", "Longitude (deg)", "%.4f", -180.0, 360.0, 0.0, 0.0);
    LocationNP.fill(getDeviceName(), "LOCATION", "Location", "Options", IP_RW, 0, IPS_IDLE);
    defineProperty(LocationNP);

    TelescopeNameTP[0].fill("TELESCOPE_NAME", "Telescope", "Telescope Simulator");
    TelescopeNameTP.fill(getDeviceName(), "TELESCOPE_NAME", "Snoop Telescope", "Options", IP_RW, 60, IPS_IDLE);
    defineProperty(TelescopeNameTP);

    ModeSP[0].fill("API_MODE", "API Mode", ISS_OFF);
    ModeSP[1].fill("SIMULATED_MODE", "Simulated Mode", ISS_ON);
    ModeSP.fill(getDeviceName(), "WEATHER_MODE", "Mode", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    defineProperty(ModeSP);

    // Momentary push-button; visible only when connected (defined in updateProperties).
    RefreshNowSP[0].fill("REFRESH", "Refresh Now", ISS_OFF);
    RefreshNowSP.fill(getDeviceName(), "WEATHER_REFRESH", "Refresh", "Options", IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Cloud cover is the critical (dome-safety) parameter.
    // addParameter threshold model: (min, max, percWarning) ─ warning band width = max*percWarning/100.
    // (0, 80, 25) → warning 0–20%, OK 20–60%, warning 60–80%, alert above 80%.
    addParameter("WEATHER_CLOUD_COVER",    "Cloud Cover (%)",        0,    80, 25);
    addParameter("WEATHER_TEMPERATURE",    "Temperature (C)",      -50,    50,  0);
    addParameter("WEATHER_WIND_SPEED",     "Wind Speed (kph)",       0,   200, 50);
    addParameter("WEATHER_DEW_POINT",      "Dew Point (C)",        -50,    50,  0);
    addParameter("WEATHER_WIND_DIRECTION", "Wind Direction (\xc2\xb0)", 0, 360,  0);
    addParameter("WEATHER_SEEING",         "Seeing (0-5)",           0,     5,  0);
    addParameter("WEATHER_TRANSPARENCY",   "Transparency (0-27+)",   0,    30,  0);

    setCriticalParameter("WEATHER_CLOUD_COVER");

    UpdatePeriodNP[0].setValue(1800);

    WeatherSummaryTP[0].fill("SUMMARY", "Weather Summary", "N/A");
    WeatherSummaryTP.fill(getDeviceName(), "WEATHER_SUMMARY", "Status", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);
    defineProperty(WeatherSummaryTP);

    LastUpdateTP[0].fill("TIMESTAMP", "Last API Fetch", "Never");
    LastUpdateTP.fill(getDeviceName(), "WEATHER_LAST_UPDATE", "Last Update", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);
    defineProperty(LastUpdateTP);

    ForecastValidUntilTP[0].fill("EXPIRY", "Valid Until", "Unknown");
    ForecastValidUntilTP.fill(getDeviceName(), "WEATHER_FORECAST_EXPIRY", "Forecast Expires", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);
    defineProperty(ForecastValidUntilTP);

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
    ForecastCloudCoverNP.fill(getDeviceName(),  "FORECAST_CLOUD_COVER",   "Cloud Cover (%)",     FORECAST_TAB, IP_RO, 60, IPS_IDLE);
    ForecastTemperatureNP.fill(getDeviceName(), "FORECAST_TEMPERATURE",   "Temperature (C)",     FORECAST_TAB, IP_RO, 60, IPS_IDLE);
    ForecastWindSpeedNP.fill(getDeviceName(),   "FORECAST_WIND_SPEED",    "Wind Speed (kph)",    FORECAST_TAB, IP_RO, 60, IPS_IDLE);
    ForecastSeeingNP.fill(getDeviceName(),      "FORECAST_SEEING",        "Seeing (0-5)",        FORECAST_TAB, IP_RO, 60, IPS_IDLE);
    ForecastTransparencyNP.fill(getDeviceName(),"FORECAST_TRANSPARENCY",  "Transparency (0-27+)",FORECAST_TAB, IP_RO, 60, IPS_IDLE);

    addDebugControl();

    loadConfig(true, "ASTROSPHERIC_API_KEY");
    loadConfig(true, "LOCATION");
    if (LocationNP[LOCATION_LATITUDE].getValue() != 0.0 || LocationNP[LOCATION_LONGITUDE].getValue() != 0.0)
    {
        locationReceived = true;
        LOGF_INFO("Loaded saved location: Latitude=%.4f, Longitude=%.4f",
                  LocationNP[LOCATION_LATITUDE].getValue(), LocationNP[LOCATION_LONGITUDE].getValue());
    }
    loadConfig(true, "TELESCOPE_NAME");
    loadConfig(true, "WEATHER_MODE");
    loadConfig(true, "WEATHER_UPDATE");

    if (TelescopeNameTP[0].getText() && strlen(TelescopeNameTP[0].getText()) > 0)
    {
        IDSnoopDevice(TelescopeNameTP[0].getText(), "GEOGRAPHIC_COORD");
        LOGF_INFO("Snooping on telescope %s for location data.", TelescopeNameTP[0].getText());
    }

    return true;
}

bool AstrosphericWeather::updateProperties()
{
    INDI::Weather::updateProperties();

    if (isConnected())
    {
        defineProperty(RefreshNowSP);
        defineProperty(ForecastCloudCoverNP);
        defineProperty(ForecastTemperatureNP);
        defineProperty(ForecastWindSpeedNP);
        defineProperty(ForecastSeeingNP);
        defineProperty(ForecastTransparencyNP);
    }
    else
    {
        deleteProperty(RefreshNowSP);
        deleteProperty(ForecastCloudCoverNP);
        deleteProperty(ForecastTemperatureNP);
        deleteProperty(ForecastWindSpeedNP);
        deleteProperty(ForecastSeeingNP);
        deleteProperty(ForecastTransparencyNP);
    }

    return true;
}

bool AstrosphericWeather::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        if (LocationNP.isNameMatch(name))
        {
            LocationNP.update(values, names, n);
            LocationNP.setState(IPS_OK);
            LocationNP.apply();
            LOGF_INFO("Location updated: Latitude=%.4f, Longitude=%.4f",
                      LocationNP[LOCATION_LATITUDE].getValue(),
                      LocationNP[LOCATION_LONGITUDE].getValue());
            forecastValid    = false;
            locationReceived = true;
            return true;
        }
    }
    // Snooped GEOGRAPHIC_COORD from the configured telescope.
    if (strcmp(name, "GEOGRAPHIC_COORD") == 0 &&
        dev && TelescopeNameTP[0].getText() && strcmp(dev, TelescopeNameTP[0].getText()) == 0)
    {
        double lat = 0.0, lon = 0.0;
        bool latFound = false, lonFound = false;

        for (int i = 0; i < n; i++)
        {
            if (strcmp(names[i], "LAT") == 0)  { lat = values[i]; latFound = true; }
            else if (strcmp(names[i], "LONG") == 0) { lon = values[i]; lonFound = true; }
        }

        if (latFound && lonFound)
        {
            LocationNP[LOCATION_LATITUDE].setValue(lat);
            LocationNP[LOCATION_LONGITUDE].setValue(lon);
            LocationNP.setState(IPS_OK);
            LocationNP.apply();
            locationReceived = true;
            forecastValid    = false;
            LOGF_INFO("Snooped location from %s: Latitude=%.4f, Longitude=%.4f", dev, lat, lon);
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

bool AstrosphericWeather::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        if (APIKeyTP.isNameMatch(name))
        {
            APIKeyTP.update(texts, names, n);
            APIKeyTP.setState(IPS_OK);
            APIKeyTP.apply();
            saveConfig(true, APIKeyTP.getName());
            forecastValid = false;
            LOG_INFO("API key updated.");
            return true;
        }
        if (TelescopeNameTP.isNameMatch(name))
        {
            TelescopeNameTP.update(texts, names, n);
            TelescopeNameTP.setState(IPS_OK);
            TelescopeNameTP.apply();
            saveConfig(true, TelescopeNameTP.getName());
            if (TelescopeNameTP[0].getText() && strlen(TelescopeNameTP[0].getText()) > 0)
            {
                IDSnoopDevice(TelescopeNameTP[0].getText(), "GEOGRAPHIC_COORD");
                LOGF_INFO("Now snooping on %s for GEOGRAPHIC_COORD.", TelescopeNameTP[0].getText());
            }
            return true;
        }
    }
    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

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
            forecastValid = false;
            return true;
        }

        if (RefreshNowSP.isNameMatch(name))
        {
            LOG_INFO("Manual refresh requested.");
            forecastValid = false;
            RefreshNowSP[0].setState(ISS_OFF);
            RefreshNowSP.setState(IPS_BUSY);
            RefreshNowSP.apply();
            // checkWeatherUpdate() calls updateWeather(), updates ParametersNP, and
            // calls syncCriticalParameters() — the same path as the scheduled timer.
            checkWeatherUpdate();
            RefreshNowSP.setState(IPS_IDLE);
            RefreshNowSP.apply();
            return true;
        }
    }
    return INDI::Weather::ISNewSwitch(dev, name, states, names, n);
}

bool AstrosphericWeather::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);
    APIKeyTP.save(fp);
    LocationNP.save(fp);
    TelescopeNameTP.save(fp);
    ModeSP.save(fp);
    return true;
}

bool AstrosphericWeather::fetchDataFromAPI(std::string &responseBody)
{
    const std::string host     = "astrosphericpublicaccess.azurewebsites.net";
    const std::string endpoint = "/api/GetForecastData_V1";

    double lat = LocationNP[LOCATION_LATITUDE].getValue();
    double lon = LocationNP[LOCATION_LONGITUDE].getValue();
    if (lon > 180.0)
        lon -= 360.0; // [0,360] → [-180,180] as expected by the API

    LOGF_DEBUG("Sending coordinates to API: Latitude=%.4f, Longitude=%.4f", lat, lon);

    json payload;
    payload["Latitude"] = lat;
    payload["Longitude"] = lon;
    payload["APIKey"] = APIKeyTP[0].getText();
    std::string jsonDataString = payload.dump();

    httplib::Client client(host.c_str());
    client.set_connection_timeout(API_CONNECT_TIMEOUT_SECS, 0);
    client.set_read_timeout(API_READ_TIMEOUT_SECS, 0);
    auto res = client.Post(endpoint.c_str(), jsonDataString, "application/json");
    if (!res || res->status != 200)
    {
        LOGF_ERROR("API request failed: %d - %s", res ? res->status : -1, res ? res->body.c_str() : "No response");
        return false;
    }

    responseBody = res->body;
    LOGF_DEBUG("API response received (%zu bytes).", responseBody.size());
    return true;
}

bool AstrosphericWeather::parseJSONResponse(const std::string &jsonResponse)
{
    LOG_DEBUG("Parsing JSON response...");
    try
    {
        json j = json::parse(jsonResponse);

        // Validate top-level structure before accessing any forecast arrays.
        if (!j.contains("UTCStartTime") || !j["UTCStartTime"].is_string())
        {
            if (j.contains("Message") && j["Message"].is_string())
                LOGF_ERROR("Astrospheric API error: %s", j["Message"].get<std::string>().c_str());
            else if (j.contains("Error") && j["Error"].is_string())
                LOGF_ERROR("Astrospheric API error: %s", j["Error"].get<std::string>().c_str());
            else
                LOG_ERROR("Unexpected API response: missing 'UTCStartTime' field.");
            return false;
        }
        static constexpr const char *REQUIRED_ARRAYS[] = {
            "RDPS_CloudCover", "RDPS_Temperature", "RDPS_WindVelocity",
            "RDPS_DewPoint", "RDPS_WindDirection",
            "Astrospheric_Seeing", "Astrospheric_Transparency"
        };
        for (const char *key : REQUIRED_ARRAYS)
        {
            if (!j.contains(key) || !j[key].is_array() || j[key].empty())
            {
                LOGF_ERROR("API response missing or empty forecast array '%s'.", key);
                return false;
            }
        }

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

        // Accept the shortest vector length so all arrays index consistently.
        size_t actualHours = newCloudCover.size();
        for (size_t sz : {newTemperature.size(), newWindSpeed.size(), newDewPoint.size(),
                          newWindDirection.size(), newSeeing.size(), newTransparency.size()})
            actualHours = std::min(actualHours, sz);

        if (actualHours == 0)
        {
            LOG_ERROR("API returned empty forecast vectors.");
            return false;
        }
        if (actualHours < EXPECTED_FORECAST_HOURS)
            LOGF_WARN("API returned only %zu of %zu expected hours; accepting shorter forecast.",
                      actualHours, EXPECTED_FORECAST_HOURS);

        newCloudCover.resize(actualHours);
        newTemperature.resize(actualHours);
        newWindSpeed.resize(actualHours);
        newDewPoint.resize(actualHours);
        newWindDirection.resize(actualHours);
        newSeeing.resize(actualHours);
        newTransparency.resize(actualHours);

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

        time_t expiryTime = forecastStartTime + static_cast<time_t>(forecastHours) * 3600;
        char expiryBuf[32];
        struct tm expiryTm = {};
        gmtime_r(&expiryTime, &expiryTm);
        strftime(expiryBuf, sizeof(expiryBuf), "%Y-%m-%d %H:%M UTC", &expiryTm);
        ForecastValidUntilTP[0].setText(expiryBuf);
        ForecastValidUntilTP.setState(IPS_OK);
        ForecastValidUntilTP.apply();

        LOGF_INFO("Parsed forecast for %d hours starting at %s.", forecastHours, utcStartTimeStr.c_str());
        return true;
    }
    catch (const json::exception &e)
    {
        LOGF_ERROR("JSON parsing error: %s", e.what());
        return false;
    }
}

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

IPState AstrosphericWeather::updateWeather()
{
    if (ModeSP[0].getState() == ISS_ON) // API Mode
    {
        if (!locationReceived)
        {
            LOG_DEBUG("API mode: no location set — update skipped.");
            return IPS_IDLE;
        }
        if (APIKeyTP[0].getText() == nullptr || strlen(APIKeyTP[0].getText()) == 0)
        {
            LOG_DEBUG("API mode: no API key — update skipped.");
            return IPS_IDLE;
        }
        time_t currentTime = time(nullptr);
        bool cacheExpired = !forecastValid || (currentTime - lastFetchTime) > FORECAST_CACHE_TTL_SECS;
        if (cacheExpired)
            LOG_INFO("Cache miss or TTL expired — fetching fresh forecast data...");
        else
            LOGF_DEBUG("Cache hit (age %lds / TTL %ds).",
                       static_cast<long>(currentTime - lastFetchTime), FORECAST_CACHE_TTL_SECS);

        if (cacheExpired)
        {
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

        int hoursRemaining = forecastHours - hourOffset;
        LOGF_DEBUG("Forecast index: offset=%d, hoursRemaining=%d of %d.", hourOffset, hoursRemaining, forecastHours);
        if (forecastValid && hoursRemaining < FORECAST_REFRESH_MARGIN_HOURS)
        {
            LOGF_DEBUG("Only %d hours remain in forecast window — scheduling early refresh.", hoursRemaining);
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

        return IPS_OK;
    }
    else // Simulated Mode
    {
        // Anchor to the current UTC hour so the forecast advances naturally each cycle.
        time_t now = time(nullptr);
        struct tm tmNow {};
        gmtime_r(&now, &tmNow);
        const int currentHour = tmNow.tm_hour;

        const int simHours = FORECAST_STEPS * FORECAST_STEP_HOURS; // 24 h of coverage
        cloudCover.resize(simHours);
        temperature.resize(simHours);
        windSpeed.resize(simHours);
        dewPoint.resize(simHours);
        windDirection.resize(simHours);
        seeing.resize(simHours);
        transparency.resize(simHours);
        forecastHours     = simHours;
        forecastStartTime = now;

        for (int h = 0; h < simHours; h++)
        {
            const int    hour  = (currentHour + h) % 24;
            const double phase = (hour - 8) * kPI / 12.0;

            temperature[h]   = 15.0 + 7.0  * std::sin(phase);
            cloudCover[h]    = std::max(0.0, std::min(100.0, 30.0 + 25.0 * std::sin(phase + kPI)));
            windSpeed[h]     = std::max(0.0, 12.0 + 6.0  * std::sin(phase - kPI / 4.0));
            windDirection[h] = std::fmod(225.0 + 90.0 * std::sin(h * kPI / 12.0), 360.0);
            dewPoint[h]      = temperature[h] - 8.0;
            seeing[h]        = std::max(0.5, std::min(5.0, 3.0 - 1.5 * std::sin(phase)));
            transparency[h]  = std::max(0.0, std::min(27.0, 18.0 * (1.0 - cloudCover[h] / 100.0)));
        }

        setParameterValue("WEATHER_CLOUD_COVER",    cloudCover[0]);
        setParameterValue("WEATHER_TEMPERATURE",    temperature[0]);
        setParameterValue("WEATHER_WIND_SPEED",     windSpeed[0]);
        setParameterValue("WEATHER_DEW_POINT",      dewPoint[0]);
        setParameterValue("WEATHER_WIND_DIRECTION", windDirection[0]);
        setParameterValue("WEATHER_SEEING",         seeing[0]);
        setParameterValue("WEATHER_TRANSPARENCY",   transparency[0]);

        updateSummaryText(buildWeatherSummary(
            cloudCover[0], temperature[0], windSpeed[0],
            dewPoint[0], windDirection[0], seeing[0], transparency[0]));

        updateForecastProperties(0);

        time_t simExpiry = now + static_cast<time_t>(simHours) * 3600;
        char expiryBuf[32];
        struct tm expiryTm = {};
        gmtime_r(&simExpiry, &expiryTm);
        strftime(expiryBuf, sizeof(expiryBuf), "%Y-%m-%d %H:%M UTC", &expiryTm);
        ForecastValidUntilTP[0].setText(expiryBuf);
        ForecastValidUntilTP.setState(IPS_OK);
        ForecastValidUntilTP.apply();

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

static std::unique_ptr<AstrosphericWeather> weather(new AstrosphericWeather());

extern "C" INDI::DefaultDevice *createDevice()
{
    return weather.get();
}
