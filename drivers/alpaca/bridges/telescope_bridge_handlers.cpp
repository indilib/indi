/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Telescope Bridge - Handlers Implementation

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

#include "telescope_bridge.h"
#include "indilogger.h"
#include "device_manager.h"

#include <httplib.h>
#include <cmath>
#include <cstring>
#include <sstream>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

// Common Alpaca API methods
void TelescopeBridge::handleConnected(const httplib::Request &req, httplib::Response &res)
{
    bool isConnected = m_Device.isConnected();
    json response;

    // Handle PUT request to connect/disconnect
    if (req.method == "PUT")
    {
        try
        {
            // Parse form-urlencoded data from request body
            std::map<std::string, std::string> formData;
            DeviceManager::parseFormUrlEncodedBody(req.body, formData);

            // Check if Connected parameter exists
            if (formData.find("Connected") == formData.end())
            {
                sendResponseValue(res, isConnected, false, "Missing Connected parameter");
                return;
            }

            // Convert Connected value to bool
            bool connected = (formData["Connected"] == "True" || formData["Connected"] == "true" ||
                              formData["Connected"] == "1");

            if (connected && !isConnected)
            {
                // Connect the device
                auto connectSw = m_Device.getSwitch("CONNECTION");
                if (connectSw)
                {
                    INDI::PropertySwitch switchProperty(connectSw);
                    switchProperty.reset();
                    switchProperty[0].setState(ISS_ON);
                    // Request to send the switch to the device
                    requestNewSwitch(switchProperty);
                    sendResponseValue(res, true);
                }
                else
                {
                    sendResponseValue(res, false, "Failed to connect device");
                }
            }
            else if (!connected && isConnected)
            {
                // Disconnect the device
                auto connectSw = m_Device.getSwitch("CONNECTION");
                if (connectSw)
                {
                    INDI::PropertySwitch switchProperty(connectSw);
                    switchProperty.reset();
                    switchProperty[1].setState(ISS_ON);
                    // Request to send the switch to the device
                    requestNewSwitch(switchProperty);
                    sendResponseValue(res, false);
                }
                else
                {
                    sendResponseValue(res, true, "Failed to disconnect device");
                }
            }
            else
            {
                // No change needed
                sendResponseValue(res, isConnected);
            }
        }
        catch (const std::exception &e)
        {
            sendResponseValue(res, isConnected, false, std::string("Invalid request: ") + e.what());
        }
    }
    else
    {
        // GET request
        sendResponseValue(res, isConnected);
    }
}

void TelescopeBridge::handleName(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::string deviceName = m_Device.getDeviceName();
    sendResponseValue(res, deviceName);
}

void TelescopeBridge::handleDescription(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::string description = std::string("INDI Telescope: ") + m_Device.getDeviceName();
    sendResponseValue(res, description);
}

void TelescopeBridge::handleDriverInfo(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::string driverInfo = "INDI Alpaca Bridge";
    sendResponseValue(res, driverInfo);
}

void TelescopeBridge::handleDriverVersion(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::string version = "1.0";
    sendResponseValue(res, version);
}

void TelescopeBridge::handleInterfaceVersion(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    sendResponseValue(res, 1);
}

// Telescope-specific properties
void TelescopeBridge::handleAltitude(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CurrentALT);
}

void TelescopeBridge::handleAzimuth(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CurrentAZ);
}

void TelescopeBridge::handleCanPark(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    sendResponseValue(res, m_Device.getProperty("TELESCOPE_PARK").isValid());
}

void TelescopeBridge::handleCanPulseGuide(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    bool canPulseGuide = m_Device.getProperty("TELESCOPE_TIMED_GUIDE_NS").isValid() &&
                         m_Device.getProperty("TELESCOPE_TIMED_GUIDE_WE").isValid();
    sendResponseValue(res, canPulseGuide);
}

void TelescopeBridge::handleCanSetTracking(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    sendResponseValue(res, m_Device.getProperty("TELESCOPE_TRACK_STATE").isValid());
}

void TelescopeBridge::handleCanSlew(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    sendResponseValue(res, m_Device.getProperty("EQUATORIAL_EOD_COORD").isValid());
}

void TelescopeBridge::handleCanSlewAsync(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    sendResponseValue(res, m_Device.getProperty("EQUATORIAL_EOD_COORD").isValid());
}

void TelescopeBridge::handleCanSync(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    sendResponseValue(res, m_Device.getProperty("ON_COORD_SET").isValid());
}

void TelescopeBridge::handleDeclination(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CurrentDEC);
}

void TelescopeBridge::handleRightAscension(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CurrentRA);
}

void TelescopeBridge::handleSideOfPier(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_PierSide);
}

void TelescopeBridge::handleSlewing(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_IsSlewing);
}

void TelescopeBridge::handleTracking(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_IsTracking);
}

void TelescopeBridge::handleAtPark(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_IsParked);
}

// Telescope-specific actions
void TelescopeBridge::handleAbortSlew(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    bool success = false;
    auto abortSw = m_Device.getSwitch("TELESCOPE_ABORT_MOTION");
    if (abortSw)
    {
        INDI::PropertySwitch switchProperty(abortSw);
        switchProperty.reset();
        switchProperty[0].setState(ISS_ON);
        // Request to send the switch to the device
        requestNewSwitch(switchProperty);
        success = true;
    }

    sendResponseStatus(res, success, success ? "" : "Failed to abort slew");
}

void TelescopeBridge::handlePark(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    bool success = false;
    auto parkSw = m_Device.getSwitch("TELESCOPE_PARK");
    if (parkSw)
    {
        INDI::PropertySwitch switchProperty(parkSw);
        switchProperty.reset();
        switchProperty[0].setState(ISS_ON);
        // Request to send the switch to the device
        requestNewSwitch(switchProperty);
        success = true;
    }

    sendResponseStatus(res, success, success ? "" : "Failed to park telescope");
}

void TelescopeBridge::handleUnpark(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    bool success = false;
    auto parkSw = m_Device.getSwitch("TELESCOPE_PARK");
    if (parkSw)
    {
        INDI::PropertySwitch switchProperty(parkSw);
        switchProperty.reset();
        switchProperty[1].setState(ISS_ON);
        // Request to send the switch to the device
        requestNewSwitch(switchProperty);
        success = true;
    }

    sendResponseStatus(res, success, success ? "" : "Failed to unpark telescope");
}

void TelescopeBridge::handleSlewToCoordinates(const httplib::Request &req, httplib::Response &res)
{
    bool success = false;
    try
    {
        // Parse form-urlencoded data from request body
        std::map<std::string, std::string> formData;
        DeviceManager::parseFormUrlEncodedBody(req.body, formData);

        // Check if required parameters exist
        if (formData.find("RightAscension") == formData.end() || formData.find("Declination") == formData.end())
        {
            sendResponseStatus(res, false, "Missing RightAscension or Declination parameter");
            return;
        }

        // Convert values to double
        double ra = std::stod(formData["RightAscension"]);
        double dec = std::stod(formData["Declination"]);

        auto coordSet = m_Device.getSwitch("ON_COORD_SET");
        auto eqCoord = m_Device.getNumber("EQUATORIAL_EOD_COORD");
        if (coordSet && eqCoord)
        {
            // Set to TRACK mode
            INDI::PropertySwitch switchProperty(coordSet);
            switchProperty.reset();
            switchProperty[0].setState(ISS_ON);
            // Request to send the switch to the device
            requestNewSwitch(switchProperty);

            // Set coordinates
            INDI::PropertyNumber numberProperty(eqCoord);
            for (auto &num : numberProperty)
            {
                if (num.isNameMatch("RA"))
                    num.setValue(ra);
                else if (num.isNameMatch("DEC"))
                    num.setValue(dec);
            }
            // Request to send the coordinates to the device
            requestNewNumber(numberProperty);
            success = true;

            // Update target coordinates
            m_TargetRA = ra;
            m_TargetDEC = dec;
        }
    }
    catch (const std::exception &e)
    {
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to parse SlewToCoordinates request: %s", e.what());
    }

    sendResponseStatus(res, success, success ? "" : "Failed to slew to coordinates");
}

void TelescopeBridge::handleSlewToCoordinatesAsync(const httplib::Request &req, httplib::Response &res)
{
    // Same implementation as SlewToCoordinates for now
    handleSlewToCoordinates(req, res);
}

void TelescopeBridge::handleSyncToCoordinates(const httplib::Request &req, httplib::Response &res)
{
    bool success = false;
    try
    {
        // Parse form-urlencoded data from request body
        std::map<std::string, std::string> formData;
        DeviceManager::parseFormUrlEncodedBody(req.body, formData);

        // Check if required parameters exist
        if (formData.find("RightAscension") == formData.end() || formData.find("Declination") == formData.end())
        {
            sendResponseStatus(res, false, "Missing RightAscension or Declination parameter");
            return;
        }

        // Convert values to double
        double ra = std::stod(formData["RightAscension"]);
        double dec = std::stod(formData["Declination"]);

        auto coordSet = m_Device.getSwitch("ON_COORD_SET");
        auto eqCoord = m_Device.getNumber("EQUATORIAL_EOD_COORD");
        if (coordSet && eqCoord)
        {
            // Set to SYNC mode
            INDI::PropertySwitch switchProperty(coordSet);
            switchProperty.reset();
            switchProperty[1].setState(ISS_ON);
            // Request to send the switch to the device
            requestNewSwitch(switchProperty);

            // Set coordinates
            INDI::PropertyNumber numberProperty(eqCoord);
            for (auto &num : numberProperty)
            {
                if (num.isNameMatch("RA"))
                    num.setValue(ra);
                else if (num.isNameMatch("DEC"))
                    num.setValue(dec);
            }
            // Request to send the coordinates to the device
            requestNewNumber(numberProperty);
            success = true;
        }
    }
    catch (const std::exception &e)
    {
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to parse SyncToCoordinates request: %s", e.what());
    }

    sendResponseStatus(res, success, success ? "" : "Failed to sync to coordinates");
}

void TelescopeBridge::handlePulseGuide(const httplib::Request &req, httplib::Response &res)
{
    bool success = false;
    try
    {
        // Parse form-urlencoded data from request body
        std::map<std::string, std::string> formData;
        DeviceManager::parseFormUrlEncodedBody(req.body, formData);

        // Check if required parameters exist
        if (formData.find("Direction") == formData.end() || formData.find("Duration") == formData.end())
        {
            sendResponseStatus(res, false, "Missing Direction or Duration parameter");
            return;
        }

        // Convert values to integers
        int direction = std::stoi(formData["Direction"]);
        int duration = std::stoi(formData["Duration"]);

        // Direction: 0=North, 1=South, 2=East, 3=West
        if (direction == 0 || direction == 1)
        {
            auto guideNS = m_Device.getNumber("TELESCOPE_TIMED_GUIDE_NS");
            if (guideNS)
            {
                INDI::PropertyNumber numberProperty(guideNS);
                numberProperty[direction].setValue(duration);
                // Request to send the guide command to the device
                requestNewNumber(numberProperty);
                success = true;
            }
        }
        else if (direction == 2 || direction == 3)
        {
            auto guideWE = m_Device.getNumber("TELESCOPE_TIMED_GUIDE_WE");
            if (guideWE)
            {
                INDI::PropertyNumber numberProperty(guideWE);
                numberProperty[direction - 2].setValue(duration);
                // Request to send the guide command to the device
                requestNewNumber(numberProperty);
                success = true;
            }
        }
    }
    catch (const std::exception &e)
    {
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to parse PulseGuide request: %s", e.what());
    }

    sendResponseStatus(res, success, success ? "" : "Failed to pulse guide");
}

void TelescopeBridge::handleMoveAxis(const httplib::Request &req, httplib::Response &res)
{
    bool success = false;
    try
    {
        // Parse form-urlencoded data from request body
        std::map<std::string, std::string> formData;
        DeviceManager::parseFormUrlEncodedBody(req.body, formData);

        // Check if required parameters exist
        if (formData.find("Axis") == formData.end() || formData.find("Rate") == formData.end())
        {
            sendResponseStatus(res, false, "Missing Axis or Rate parameter");
            return;
        }

        // Convert values
        int axis = std::stoi(formData["Axis"]);
        double rate = std::stod(formData["Rate"]);

        // Axis: 0=Primary (RA/AZ), 1=Secondary (DEC/ALT)
        if (axis == 0)
        {
            auto motionWE = m_Device.getSwitch("TELESCOPE_MOTION_WE");
            if (motionWE)
            {
                INDI::PropertySwitch switchProperty(motionWE);
                switchProperty.reset();
                if (rate > 0)
                    switchProperty[0].setState(ISS_ON); // West
                else if (rate < 0)
                    switchProperty[1].setState(ISS_ON); // East
                // Request to send the switch to the device
                requestNewSwitch(switchProperty);
                success = true;
            }
        }
        else if (axis == 1)
        {
            auto motionNS = m_Device.getSwitch("TELESCOPE_MOTION_NS");
            if (motionNS)
            {
                INDI::PropertySwitch switchProperty(motionNS);
                switchProperty.reset();
                if (rate > 0)
                    switchProperty[0].setState(ISS_ON); // North
                else if (rate < 0)
                    switchProperty[1].setState(ISS_ON); // South
                // Request to send the switch to the device
                requestNewSwitch(switchProperty);
                success = true;
            }
        }
    }
    catch (const std::exception &e)
    {
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to parse MoveAxis request: %s", e.what());
    }

    sendResponseStatus(res, success, success ? "" : "Failed to move axis");
}

void TelescopeBridge::handleSetTracking(const httplib::Request &req, httplib::Response &res)
{
    bool success = false;
    try
    {
        // Parse form-urlencoded data from request body
        std::map<std::string, std::string> formData;
        DeviceManager::parseFormUrlEncodedBody(req.body, formData);

        // Check if required parameters exist
        if (formData.find("Tracking") == formData.end())
        {
            sendResponseStatus(res, false, "Missing Tracking parameter");
            return;
        }

        // Convert Tracking value to bool
        bool tracking = (formData["Tracking"] == "True" || formData["Tracking"] == "true" ||
                         formData["Tracking"] == "1");

        auto trackState = m_Device.getSwitch("TELESCOPE_TRACK_STATE");
        if (trackState)
        {
            INDI::PropertySwitch switchProperty(trackState);
            switchProperty.reset();
            switchProperty[tracking ? 0 : 1].setState(ISS_ON);
            // Request to send the switch to the device
            requestNewSwitch(switchProperty);
            success = true;
        }
    }
    catch (const std::exception &e)
    {
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to parse SetTracking request: %s", e.what());
    }

    sendResponseStatus(res, success, success ? "" : "Failed to set tracking state");
}

// Equatorial System
void TelescopeBridge::handleEquatorialSystem(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, 1);
}
