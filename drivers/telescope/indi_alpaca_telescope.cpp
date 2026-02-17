/*
    INDI alpaca Driver
    
    Copyright (C) 2026 Gord Tulloch
    
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
    
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
    
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "indi_alpaca_telescope.h"
#include <connectionplugins/connectiontcp.h>
#include <memory>
#include <cstring>
#include <cmath>
#include <indicom.h>
#include <libnova/julian_day.h>

// We declare an auto pointer to alpacaTelescopeDriver instance
std::unique_ptr<alpacaTelescopeDriver> alpaca(new alpacaTelescopeDriver());

#define RA_AXIS     0
#define DEC_AXIS    1

alpacaTelescopeDriver::alpacaTelescopeDriver()
{
    DBG_SCOPE = static_cast<uint32_t>(INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE"));

    setVersion(1, 0);
    SetTelescopeCapability(
        TELESCOPE_CAN_GOTO |
        TELESCOPE_CAN_SYNC |
        TELESCOPE_CAN_ABORT |
        TELESCOPE_CAN_PARK ,
        4
    );
    m_ClientID = getpid();
    LOG_DEBUG("Initializing from alpacaTelescope device...");
}

const char *alpacaTelescopeDriver::getDefaultName()
{
    return "Alpaca Telescope";
}

bool alpacaTelescopeDriver::initProperties()
{
    INDI::Telescope::initProperties();
    
    // Use built-in TCP connection with default alpaca.local:32323
    setTelescopeConnection(CONNECTION_TCP);
    tcpConnection->setDefaultHost("alpaca.local");
    tcpConnection->setDefaultPort(32323);
    
    // Device info
    DeviceInfoTP[DESCRIPTION].fill("DESCRIPTION", "Description", "");
    DeviceInfoTP[DRIVER_INFO].fill("DRIVER_INFO", "Driver Info", "");
    DeviceInfoTP[DRIVER_VERSION].fill("DRIVER_VERSION", "Driver Version", "");
    DeviceInfoTP[INTERFACE_VERSION].fill("INTERFACE_VERSION", "Interface Version", "");
    DeviceInfoTP.fill(getDeviceName(), "DEVICE_INFO", "Device Info", OPTIONS_TAB, IP_RO, 60, IPS_IDLE);
    
    SetParkDataType(PARK_RA_DEC);
    
    // RA is a rotating frame, while HA or Alt/Az is not
    SetParkDataType(PARK_HA_DEC);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();
    setDefaultPollingPeriod(250);
    //LOG_DEBUG("Properties initialized");

    return true;
}

bool alpacaTelescopeDriver::updateProperties()
{
    INDI::Telescope::updateProperties();
        
    if (isConnected())
    {
        defineProperty(DeviceInfoTP);

        if (InitPark())
        {
            // Load park data if available
            m_currentRA = ParkPositionNP[AXIS_RA].getValue();
            m_currentDEC = ParkPositionNP[AXIS_DE].getValue();
            SetAxis1ParkDefault(-6.);
            SetAxis2ParkDefault(0.);
        }
        else
        {
            SetAxis1Park(-6.);
            SetAxis2Park(0.);
            SetAxis1ParkDefault(-6.);
            SetAxis2ParkDefault(0.);
        }

        sendTimeFromSystem();
    }
    else
    {
        deleteProperty(DeviceInfoTP);
    }

    return true;
}

bool alpacaTelescopeDriver::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

void alpacaTelescopeDriver::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);
}

bool alpacaTelescopeDriver::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool alpacaTelescopeDriver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

void alpacaTelescopeDriver::TimerHit()
{
    if (!isConnected())
    {
        return;
    }
    
    ReadScopeStatus();
    
    // Schedule next timer hit
    SetTimer(POLLMS);
    
    // Call parent timer to handle any other periodic tasks
    INDI::Telescope::TimerHit();
}

bool alpacaTelescopeDriver::Connect()
{
    std::string host = tcpConnection->host();
    int port = tcpConnection->port();
    
    LOGF_INFO("Connecting to alpaca server at %s:%d", host.c_str(), port);
    
    httpClient = std::make_unique<httplib::Client>(host.c_str(), port);
    httpClient->set_connection_timeout(10);
    httpClient->set_read_timeout(30);  // Increased timeout for commands that may take time to respond
    
    nlohmann::json response;
    if (!sendAlpacaGET("/connected", response))
    {
        LOG_ERROR("Failed to connect to alpaca");
        httpClient.reset();
        return false;
    }
    
    LOGF_INFO("alpaca reachable, connected=%s", 
              response.value("Value", false) ? "true" : "false");
    
    nlohmann::json request;
    request["Connected"] = true;
    if (!sendAlpacaPUT("/connected", request, response))
    {
        LOG_ERROR("Failed to set connected state");
        httpClient.reset();
        return false;
    }
    
    // Start the timer
    SetTimer(POLLMS);
    
    LOG_INFO("Successfully connected to alpaca");
    return true;
}

bool alpacaTelescopeDriver::Disconnect()
{
    if (httpClient)
    {
        nlohmann::json request, response;
        request["Connected"] = false;
        sendAlpacaPUT("/connected", request, response);
        httpClient.reset();
    }
    LOG_INFO("Disconnected from alpaca");
    return true;
}

bool alpacaTelescopeDriver::Handshake()
{
    nlohmann::json response;
    
    if (sendAlpacaGET("/description", response) && response.contains("Value"))
        DeviceInfoTP[DESCRIPTION].setText(response["Value"].get<std::string>().c_str());
    
    if (sendAlpacaGET("/driverinfo", response) && response.contains("Value"))
        DeviceInfoTP[DRIVER_INFO].setText(response["Value"].get<std::string>().c_str());
    
    if (sendAlpacaGET("/driverversion", response) && response.contains("Value"))
        DeviceInfoTP[DRIVER_VERSION].setText(response["Value"].get<std::string>().c_str());
    
    if (sendAlpacaGET("/interfaceversion", response) && response.contains("Value"))
        DeviceInfoTP[INTERFACE_VERSION].setText(std::to_string(response["Value"].get<int>()).c_str());
    
    DeviceInfoTP.apply();
    
    // Query alignment mode to determine mount behavior
    // 0 = algPolar (equatorial), 1 = algAltAz, 2 = algGermanPolar
    if (sendAlpacaGET("/alignmentmode", response) && response.contains("Value"))
    {
        int alignmentMode = response["Value"].get<int>();
        if (alignmentMode == 1)
        {
            LOG_INFO("Telescope alignment mode: Alt-Az (no meridian flips)");
        }
        else if (alignmentMode == 0 || alignmentMode == 2)
        {
            LOG_INFO("Telescope alignment mode: Equatorial (Fork mount - no meridian flips)");
        }
    }
    
    bool canPark = false;
    if (sendAlpacaGET("/canpark", response) && response.contains("Value"))
        canPark = response["Value"].get<bool>();
    
    if (canPark)
    {
        SetParkDataType(PARK_RA_DEC);
        
        // Initialize park position if not already set
        // Default to zenith (RA=0, Dec=observer latitude)
        if (!InitPark())
        {
            // Set default park position to zenith
            SetAxis1Park(0);
            SetAxis2Park(90);
            LOG_INFO("Park position initialized to zenith");
        }
        
        // Get current park state
        if (sendAlpacaGET("/atpark", response) && response.contains("Value"))
        {
            isParked = response["Value"].get<bool>();
            SetParked(isParked);
        }
    }
    
    // Get site location from device
    double latitude = 0.0, longitude = 0.0;
    bool locationValid = false;
    
    if (sendAlpacaGET("/sitelatitude", response) && response.contains("Value"))
    {
        latitude = response["Value"].get<double>();
        locationValid = true;
        LOGF_INFO("Site latitude from device: %.6f°", latitude);
    }
    else
    {
        LOG_WARN("Failed to get site latitude from device");
    }
    
    if (sendAlpacaGET("/sitelongitude", response) && response.contains("Value"))
    {
        longitude = response["Value"].get<double>();
        LOGF_INFO("Site longitude from device: %.6f°", longitude);
    }
    else
    {
        LOG_WARN("Failed to get site longitude from device");
        locationValid = false;
    }
    
    // Update INDI location property if we got valid data
    if (locationValid)
    {
        // INDI uses: LOCATION_LATITUDE = latitude (positive north), LOCATION_LONGITUDE = longitude (positive east)
        // Alpaca uses same convention
        LocationNP[LOCATION_LATITUDE].setValue(latitude);
        LocationNP[LOCATION_LONGITUDE].setValue(longitude);
        LocationNP[LOCATION_ELEVATION].setValue(0); // Elevation not available from Alpaca
        LocationNP.setState(IPS_OK);
        LocationNP.apply();
        
        // Calculate and cache sin/cos for coordinate transformations
        double latRad = latitude * M_PI / 180.0;
        m_sinLat = sin(latRad);
        m_cosLat = cos(latRad);
        
        LOGF_INFO("Site location set: Lat=%.6f° Long=%.6f°", latitude, longitude);
    }
    
    // Get initial coordinates
    ReadScopeStatus();
    
    LOG_INFO("alpaca connected successfully");
    return true;
}

bool alpacaTelescopeDriver::ReadScopeStatus()
{
    nlohmann::json response;
    
    // Get RA/Dec
    double newRA = m_currentRA, newDec = m_currentDEC;
    
    // Get current coordinates from Alpaca API
    bool raUpdated = false, decUpdated = false;
    if (sendAlpacaGET("/rightascension", response) && response.contains("Value"))
    {
        newRA = response["Value"].get<double>();
        raUpdated = true;
    }
    else
    {
        LOG_WARN("Failed to get RA from Alpaca API");
    }
    
    if (sendAlpacaGET("/declination", response) && response.contains("Value"))
    {
        newDec = response["Value"].get<double>();
        decUpdated = true;
    }
    else
    {
        LOG_WARN("Failed to get Dec from Alpaca API");
    }
    
    // Update current coordinates
    if (raUpdated && decUpdated)
    {
        m_currentRA = newRA;
        m_currentDEC = newDec;
        
        // Always call NewRaDec to update client display
        // This is critical during slewing to show real-time position updates
        NewRaDec(m_currentRA, m_currentDEC);
    }
    else
    {
        LOG_WARN("NewRaDec NOT called - coordinate update failed");
    }
    
    // Get slewing state
    if (sendAlpacaGET("/slewing", response) && response.contains("Value"))
        isSlewing = response["Value"].get<bool>();
    
    // Get tracking state
    if (sendAlpacaGET("/tracking", response) && response.contains("Value"))
        isTracking = response["Value"].get<bool>();
    
    // Get park state
    if (sendAlpacaGET("/atpark", response) && response.contains("Value"))
    {
        bool parked = response["Value"].get<bool>();
        if (parked != isParked)
        {
            isParked = parked;
            SetParked(isParked);
        }
    }
    
    // Update telescope state
    if (TrackState == SCOPE_PARKING)
    {
        // If we're parking and slewing stopped, we've reached park position
        if (!isSlewing)
        {
            TrackState = SCOPE_PARKED;
            LOG_INFO("Parking complete - telescope at park position");
        }
        // else keep SCOPE_PARKING state while slewing to park position
    }
    else if (isSlewing)
        TrackState = SCOPE_SLEWING;
    else if (isTracking)
        TrackState = SCOPE_TRACKING;
    else if (isParked)
        TrackState = SCOPE_PARKED;
    else
        TrackState = SCOPE_IDLE;
    
    return true;
}

bool alpacaTelescopeDriver::Goto(double ra, double dec)
{
    // Check if telescope is parked - must unpark before slewing
    if (isParked)
    {
        LOG_ERROR("Cannot GoTo while parked. Please unpark the telescope first.");
        return false;
    }
    
    LOGF_INFO("GoTo command: RA=%f Dec=%f", ra, dec);
    
    // Use ASCOM Alpaca pattern: set target coordinates first, then slew
    // Step 1: Set target RA
    nlohmann::json response;
    nlohmann::json raRequest;
    raRequest["TargetRightAscension"] = ra;
    if (!sendAlpacaPUT("/targetrightascension", raRequest, response))
    {
        LOG_ERROR("Failed to set target RA");
        return false;
    }
    
    // Step 2: Set target Dec
    nlohmann::json decRequest;
    decRequest["TargetDeclination"] = dec;
    if (!sendAlpacaPUT("/targetdeclination", decRequest, response))
    {
        LOG_ERROR("Failed to set target Dec");
        return false;
    }
    
    // Step 3: Start slew to target
    nlohmann::json emptyRequest;
    if (!sendAlpacaPUT("/slewtotarget", emptyRequest, response))
    {
        LOG_ERROR("Failed to send GoTo command");
        return false;
    }
    
    TrackState = SCOPE_SLEWING;
    LOG_INFO("GoTo command sent - slewing to target");
    return true;
}

bool alpacaTelescopeDriver::Sync(double ra, double dec)
{
    nlohmann::json request, response;
    request["RightAscension"] = ra;
    request["Declination"] = dec;
    
    if (!sendAlpacaPUT("/synctocoordinates", request, response))
    {
        LOG_ERROR("Failed to sync");
        return false;
    }
    
    LOG_INFO("Sync successful");
    return true;
}

bool alpacaTelescopeDriver::Abort()
{
    nlohmann::json request, response;
    
    // Check if we're parking - parking cannot be aborted via AbortSlew
    if (TrackState == SCOPE_PARKING)
    {
        LOG_WARN("Cannot abort parking operation - park must complete");
        return false;
    }
    
    if (!sendAlpacaPUT("/abortslew", request, response))
    {
        LOG_ERROR("Failed to abort");
        return false;
    }
    
    TrackState = SCOPE_IDLE;
    LOG_INFO("Slew aborted");
    return true;
}

bool alpacaTelescopeDriver::Park()
{
    nlohmann::json request, response;
    
    // Abort any ongoing slew first
    if (isSlewing)
    {
        LOG_INFO("Aborting slew before park");
        nlohmann::json abortReq, abortResp;
        sendAlpacaPUT("/abortslew", abortReq, abortResp);
    }
    
    LOG_INFO("Sending park command to Alpaca device");
    
    // Use Alpaca park command
    if (!sendAlpacaPUT("/park", request, response))
    {
        LOG_ERROR("Failed to send park command to Alpaca device");
        return false;
    }
    
    // Mark as parked - set state to PARKED not PARKING since command is sent
    isParked = true;
    SetParked(true);
    TrackState = SCOPE_PARKED;
    LOG_INFO("Park command sent - telescope parked");
    
    return true;
}

bool alpacaTelescopeDriver::UnPark()
{
    nlohmann::json request, response;
    
    LOG_INFO("Sending unpark command to Alpaca device");
    
    // Use Alpaca unpark command
    if (!sendAlpacaPUT("/unpark", request, response))
    {
        LOG_ERROR("Failed to send unpark command to Alpaca device");
        return false;
    }
    
    // Immediately mark as unparked so INDI allows motion commands
    // ReadScopeStatus will sync with actual device state
    isParked = false;
    SetParked(false);
    TrackState = SCOPE_IDLE;
    LOG_INFO("Unpark command sent - telescope unparked");
    
    return true;
}

bool alpacaTelescopeDriver::SetCurrentPark()
{
    // Save current RA/Dec position as park position
    // This allows user to park at current position via "Park Options -> Set Current"
    
    // Get current coordinates
    nlohmann::json response;
    double ra = 0, dec = 0;
    
    if (sendAlpacaGET("/rightascension", response) && response.contains("Value"))
    {
        ra = response["Value"].get<double>();
    }
    else
    {
        LOG_ERROR("Failed to get current RA for SetCurrentPark");
        return false;
    }
    
    if (sendAlpacaGET("/declination", response) && response.contains("Value"))
    {
        dec = response["Value"].get<double>();
    }
    else
    {
        LOG_ERROR("Failed to get current Dec for SetCurrentPark");
        return false;
    }
    
    // Save current position as park position
    SetAxis1Park(ra);   // RA in hours
    SetAxis2Park(dec);  // Dec in degrees
    
    LOGF_INFO("Park position set to current coordinates: RA=%.6f hours, Dec=%.6f degrees", ra, dec);
    
    return true;
}

bool alpacaTelescopeDriver::SetTrackEnabled(bool enabled)
{
    nlohmann::json request, response;
    request["Tracking"] = enabled;
    
    if (!sendAlpacaPUT("/tracking", request, response))
    {
        LOGF_ERROR("Failed to %s tracking", enabled ? "enable" : "disable");
        return false;
    }
    
    isTracking = enabled;
    LOGF_INFO("Tracking %s", enabled ? "enabled" : "disabled");
    return true;
}

bool alpacaTelescopeDriver::SetTrackMode(uint8_t mode)
{
    nlohmann::json request, response;
    // Mode: 0=Sidereal, 1=Lunar, 2=Solar
    request["TrackingRate"] = static_cast<int>(mode);
    
    if (!sendAlpacaPUT("/trackingrate", request, response))
    {
        LOG_ERROR("Failed to set track mode");
        return false;
    }
    
    LOG_INFO("Track mode set");
    return true;
}

bool alpacaTelescopeDriver::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    nlohmann::json request, response;
    
    // Secondary axis (Dec/Altitude): 1
    // Positive rate = North/Up, Negative rate = South/Down
    request["Axis"] = 1;
    
    if (command == MOTION_START)
    {
        // Use moderate rate for manual movement (0.5 deg/sec)
        // TODO: Query /axisrates to get valid rates and use client-selected slew rate
        double rate = (dir == DIRECTION_NORTH) ? 0.5 : -0.5;
        request["Rate"] = rate;
        LOGF_INFO("Moving %s at rate %.2f deg/sec", (dir == DIRECTION_NORTH) ? "North" : "South", rate);
    }
    else
    {
        request["Rate"] = 0.0;
        LOG_INFO("Stopping NS motion");
    }
    
    if (!sendAlpacaPUT("/moveaxis", request, response))
    {
        LOG_ERROR("Failed to move NS");
        return false;
    }
    
    // Log response for debugging
    if (response.contains("ErrorNumber"))
    {
        int errNum = response["ErrorNumber"].get<int>();
        if (errNum != 0)
        {
            LOGF_WARN("MoveAxis NS returned error %d: %s", errNum, 
                     response.value("ErrorMessage", "Unknown error").c_str());
        }
    }
    
    return true;
}

bool alpacaTelescopeDriver::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    nlohmann::json request, response;
    
    // Primary axis (RA/Azimuth): 0
    // Positive rate = East/Right, Negative rate = West/Left
    request["Axis"] = 0;
    
    if (command == MOTION_START)
    {
        // Use moderate rate for manual movement (0.5 deg/sec)
        // TODO: Query /axisrates to get valid rates and use client-selected slew rate
        double rate = (dir == DIRECTION_EAST) ? 0.5 : -0.5;
        request["Rate"] = rate;
        LOGF_INFO("Moving %s at rate %.2f deg/sec", (dir == DIRECTION_EAST) ? "East" : "West", rate);
    }
    else
    {
        request["Rate"] = 0.0;
        LOG_INFO("Stopping WE motion");
    }
    
    if (!sendAlpacaPUT("/moveaxis", request, response))
    {
        LOG_ERROR("Failed to move WE");
        return false;
    }
    
    // Log response for debugging
    if (response.contains("ErrorNumber"))
    {
        int errNum = response["ErrorNumber"].get<int>();
        if (errNum != 0)
        {
            LOGF_WARN("MoveAxis WE returned error %d: %s", errNum,
                     response.value("ErrorMessage", "Unknown error").c_str());
        }
    }
    
    return true;
}

bool alpacaTelescopeDriver::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    return true;
}

bool alpacaTelescopeDriver::updateLocation(double latitude, double longitude, double elevation)
{
    // Update location on the Alpaca device
    nlohmann::json request, response;
    
    // Set latitude
    request["SiteLatitude"] = latitude;
    if (!sendAlpacaPUT("/sitelatitude", request, response))
    {
        LOG_ERROR("Failed to set site latitude on device");
        return false;
    }
    
    // Set longitude  
    request.clear();
    request["SiteLongitude"] = longitude;
    if (!sendAlpacaPUT("/sitelongitude", request, response))
    {
        LOG_ERROR("Failed to set site longitude on device");
        return false;
    }
    
    // Calculate and cache sin/cos for coordinate transformations
    double latRad = latitude * M_PI / 180.0;
    m_sinLat = sin(latRad);
    m_cosLat = cos(latRad);
    
    LOGF_INFO("Site location updated: Lat=%.6f° Long=%.6f°", latitude, longitude);
    
    // Call base class to update INDI properties
    return INDI::Telescope::updateLocation(latitude, longitude, elevation);
}

// Alpaca helper methods
std::string alpacaTelescopeDriver::getAlpacaURL(const std::string& endpoint)
{
    return "/api/v1/telescope/" + std::to_string(m_DeviceNumber) + endpoint;
}

bool alpacaTelescopeDriver::sendAlpacaGET(const std::string& endpoint, nlohmann::json& response)
{
    if (!httpClient)
    {
        LOG_ERROR("HTTP client not initialized");
        return false;
    }

    std::string url = getAlpacaURL(endpoint);
    url += "?ClientID=" + std::to_string(m_ClientID) + 
           "&ClientTransactionID=" + std::to_string(getTransactionId());

    auto result = httpClient->Get(url.c_str());

    if (!result)
    {
        LOGF_ERROR("GET %s failed: %s", endpoint.c_str(),
                   httplib::to_string(result.error()).c_str());
        return false;
    }

    if (result->status != 200)
    {
        LOGF_ERROR("GET %s HTTP %d", endpoint.c_str(), result->status);
        return false;
    }

    try
    {
        response = nlohmann::json::parse(result->body);

        if (response.contains("ErrorNumber") && response["ErrorNumber"].get<int>() != 0)
        {
            LOGF_ERROR("Alpaca error %s: %d - %s", endpoint.c_str(),
                       response["ErrorNumber"].get<int>(),
                       response.value("ErrorMessage", "").c_str());
            return false;
        }

        return true;
    }
    catch (const nlohmann::json::exception& e)
    {
        LOGF_ERROR("JSON parse error %s: %s", endpoint.c_str(), e.what());
        return false;
    }
}

bool alpacaTelescopeDriver::sendAlpacaPUT(const std::string& endpoint, const nlohmann::json& request, nlohmann::json& response)
{
    if (!httpClient)
    {
        LOG_ERROR("HTTP client not initialized");
        return false;
    }

    std::string url = getAlpacaURL(endpoint);

    // Build form data - Alpaca expects form-encoded parameters
    httplib::Params params;
    
    for (auto& [key, value] : request.items())
    {
        if (value.is_string())
            params.emplace(key, value.get<std::string>());
        else if (value.is_boolean())
            params.emplace(key, value.get<bool>() ? "true" : "false");
        else if (value.is_number_integer())
            params.emplace(key, std::to_string(value.get<int>()));
        else if (value.is_number())
            params.emplace(key, std::to_string(value.get<double>()));
    }

    params.emplace("ClientID", std::to_string(m_ClientID));
    params.emplace("ClientTransactionID", std::to_string(getTransactionId()));

    auto result = httpClient->Put(url.c_str(), params);

    if (!result)
    {
        // Some Alpaca devices (like Seestar) close connection immediately after
        // receiving /slewtotarget command. Log as debug instead of error.
        if (endpoint == "/slewtotarget")
        {
            LOGF_DEBUG("PUT %s: connection closed (expected for some devices)", endpoint.c_str());
        }
        else
        {
            LOGF_ERROR("PUT %s failed: %s", endpoint.c_str(),
                       httplib::to_string(result.error()).c_str());
        }
        return false;
    }

    if (result->status != 200)
    {
        LOGF_ERROR("PUT %s HTTP %d", endpoint.c_str(), result->status);
        return false;
    }

    try
    {
        response = nlohmann::json::parse(result->body);

        if (response.contains("ErrorNumber") && response["ErrorNumber"].get<int>() != 0)
        {
            int errorNum = response["ErrorNumber"].get<int>();
            std::string errorMsg = response.value("ErrorMessage", "");
            LOGF_ERROR("Alpaca error %s: %d - %s", endpoint.c_str(), errorNum, errorMsg.c_str());
            return false;
        }

        return true;
    }
    catch (const nlohmann::json::exception& e)
    {
        LOGF_ERROR("JSON parse error %s: %s", endpoint.c_str(), e.what());
        return false;
    }
}

void ISGetProperties(const char *dev)
{
    alpaca->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    alpaca->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    alpaca->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    alpaca->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    alpaca->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice(XMLEle *root)
{
    alpaca->ISSnoopDevice(root);
}
