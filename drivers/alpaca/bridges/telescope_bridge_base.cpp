/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Telescope Bridge - Base Implementation

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

TelescopeBridge::TelescopeBridge(INDI::BaseDevice device, int deviceNumber)
    : m_Device(device), m_DeviceNumber(deviceNumber)
{
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_SESSION, "Created telescope bridge for device %s with number %d",
                 device.getDeviceName(), deviceNumber);
}

TelescopeBridge::~TelescopeBridge()
{
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_SESSION, "Destroyed telescope bridge for device %s",
                 m_Device.getDeviceName());
}

std::string TelescopeBridge::getDeviceName() const
{
    return m_Device.getDeviceName();
}

std::string TelescopeBridge::getUniqueID() const
{
    // Generate a unique ID based on the device name
    return "INDI_" + std::string(m_Device.getDeviceName());
}

void TelescopeBridge::handleRequest(const std::string &method, const httplib::Request &req, httplib::Response &res)
{
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Handling telescope request: %s", method.c_str());

    // Common methods
    if (method == "connected")
        handleConnected(req, res);
    else if (method == "name")
        handleName(req, res);
    else if (method == "description")
        handleDescription(req, res);
    else if (method == "driverinfo")
        handleDriverInfo(req, res);
    else if (method == "driverversion")
        handleDriverVersion(req, res);
    else if (method == "interfaceversion")
        handleInterfaceVersion(req, res);
    // Telescope-specific properties
    else if (method == "alignmentmode")
        handleAlignmentMode(req, res);
    else if (method == "altitude")
        handleAltitude(req, res);
    else if (method == "azimuth")
        handleAzimuth(req, res);
    else if (method == "canpark")
        handleCanPark(req, res);
    else if (method == "canpulseguide")
        handleCanPulseGuide(req, res);
    else if (method == "cansettracking")
        handleCanSetTracking(req, res);
    else if (method == "cansetrightascensionrate")
        handleCanSetRightAscensionRate(req, res);
    else if (method == "cansetdeclinationrate")
        handleCanSetDeclinationRate(req, res);
    else if (method == "canslew")
        handleCanSlew(req, res);
    else if (method == "canmoveaxis")
        handleCanMoveAxis(req, res);
    else if (method == "canslewasync")
        handleCanSlewAsync(req, res);
    else if (method == "canslewaltazasync")
        handleCanSlewAltAzAsync(req, res);
    else if (method == "cansync")
        handleCanSync(req, res);
    else if (method == "declination")
        handleDeclination(req, res);
    else if (method == "declinationrate")
        handleDeclinationRate(req, res);
    else if (method == "rightascension")
        handleRightAscension(req, res);
    else if (method == "rightascensionrate")
        handleRightAscensionRate(req, res);
    else if (method == "sideofpier")
        handleSideOfPier(req, res);
    else if (method == "slewing")
        handleSlewing(req, res);
    else if (method == "tracking")
        handleTracking(req, res);
    else if (method == "atpark")
        handleAtPark(req, res);
    // Telescope-specific actions
    else if (method == "abortslew")
        handleAbortSlew(req, res);
    else if (method == "park")
        handlePark(req, res);
    else if (method == "unpark")
        handleUnpark(req, res);
    else if (method == "slewtocoordinates")
        handleSlewToCoordinates(req, res);
    else if (method == "slewtocoordinatesasync")
        handleSlewToCoordinatesAsync(req, res);
    else if (method == "synctocoordinates")
        handleSyncToCoordinates(req, res);
    else if (method == "pulseguide")
        handlePulseGuide(req, res);
    else if (method == "moveaxis")
        handleMoveAxis(req, res);
    else if (method == "axisrates")
        handleAxisRates(req, res);
    else if (method == "settracking")
        handleSetTracking(req, res);
    else if (method == "setrightascensionrate")
        handleSetRightAscensionRate(req, res);
    else if (method == "setdeclinationrate")
        handleSetDeclinationRate(req, res);
    else if (method == "equatorialsystem")
        handleEquatorialSystem(req, res);
    else if (method == "sitelatitude")
        handleSiteLatitude(req, res);
    else if (method == "sitelongitude")
        handleSiteLongitude(req, res);
    else if (method == "siteelevation")
        handleSiteElevation(req, res);
    else
    {
        // Unknown method
        json response =
        {
            {"ErrorNumber", 1025},
            {"ErrorMessage", "Method not implemented: " + method}
        };
        res.status = 400;
        res.set_content(response.dump(), "application/json");
    }
}

void TelescopeBridge::updateProperty(INDI::Property property)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const char *propName = property.getName();
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updating property: %s", propName);

    // Update telescope state based on property updates
    if (property.isNameMatch("EQUATORIAL_EOD_COORD"))
    {
        // Update RA/DEC
        INDI::PropertyNumber numberProperty(property);
        for (auto &num : numberProperty)
        {
            if (num.isNameMatch("RA"))
                m_CurrentRA = num.getValue();
            else if (num.isNameMatch("DEC"))
                m_CurrentDEC = num.getValue();
        }
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated RA/DEC: %f, %f", m_CurrentRA, m_CurrentDEC);
    }
    else if (property.isNameMatch("HORIZONTAL_COORD"))
    {
        // Update AZ/ALT
        INDI::PropertyNumber numberProperty(property);
        for (auto &num : numberProperty)
        {
            if (num.isNameMatch("AZ"))
                m_CurrentAZ = num.getValue();
            else if (num.isNameMatch("ALT"))
                m_CurrentALT = num.getValue();
        }
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated AZ/ALT: %f, %f", m_CurrentAZ, m_CurrentALT);
    }
    else if (property.isNameMatch("TELESCOPE_TRACK_STATE"))
    {
        // Update tracking state
        INDI::PropertySwitch switchProperty(property);
        m_IsTracking = switchProperty[0].getState() == ISS_ON;
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated tracking state: %s", m_IsTracking ? "ON" : "OFF");
    }
    else if (property.isNameMatch("TELESCOPE_PARK"))
    {
        // Update park state
        INDI::PropertySwitch switchProperty(property);
        m_IsParked = switchProperty[0].getState() == ISS_ON;
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated park state: %s",
                     m_IsParked ? "PARKED" : "UNPARKED");
    }
    else if (property.isNameMatch("TELESCOPE_MOTION_NS") || property.isNameMatch("TELESCOPE_MOTION_WE"))
    {
        // Update slewing state
        INDI::PropertySwitch switchProperty(property);
        bool isMoving = (switchProperty[0].getState() == ISS_ON || switchProperty[1].getState() == ISS_ON);
        if (isMoving)
            m_IsSlewing = true;
        else
        {
            // Check if the other axis is still moving
            if (property.isNameMatch("TELESCOPE_MOTION_NS"))
            {
                auto weSp = m_Device.getSwitch("TELESCOPE_MOTION_WE");
                if (weSp)
                {
                    INDI::PropertySwitch weSwitch(weSp);
                    m_IsSlewing = (weSwitch[0].getState() == ISS_ON || weSwitch[1].getState() == ISS_ON);
                }
                else
                    m_IsSlewing = false;
            }
            else
            {
                auto nsSp = m_Device.getSwitch("TELESCOPE_MOTION_NS");
                if (nsSp)
                {
                    INDI::PropertySwitch nsSwitch(nsSp);
                    m_IsSlewing = (nsSwitch[0].getState() == ISS_ON || nsSwitch[1].getState() == ISS_ON);
                }
                else
                    m_IsSlewing = false;
            }
        }
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated slewing state: %s",
                     m_IsSlewing ? "SLEWING" : "NOT SLEWING");
    }
    else if (property.isNameMatch("TELESCOPE_PIER_SIDE"))
    {
        // Update pier side
        INDI::PropertySwitch switchProperty(property);
        if (switchProperty[0].getState() == ISS_ON)
            m_PierSide = 0; // PIER_WEST in Alpaca
        else if (switchProperty[1].getState() == ISS_ON)
            m_PierSide = 1; // PIER_EAST in Alpaca
        else
            m_PierSide = -1; // PIER_UNKNOWN in Alpaca
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated pier side: %d", m_PierSide);
    }
    else if (property.isNameMatch("TELESCOPE_TRACK_RATE"))
    {
        // Update tracking rates
        INDI::PropertyNumber numberProperty(property);
        for (auto &num : numberProperty)
        {
            if (num.isNameMatch("TRACK_RATE_RA"))
                m_RightAscensionRate = num.getValue();
            else if (num.isNameMatch("TRACK_RATE_DE"))
                m_DeclinationRate = num.getValue();
        }
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated tracking rates: RA=%f, DEC=%f",
                     m_RightAscensionRate, m_DeclinationRate);
    }
}

// Helper methods for sending commands to INDI server
void TelescopeBridge::requestNewNumber(const INDI::PropertyNumber &numberProperty)
{
    // Forward the request to the DeviceManager
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Request to send new number property: %s",
                 numberProperty.getName());

    DeviceManager::getInstance()->sendNewNumber(numberProperty);
}

void TelescopeBridge::requestNewSwitch(const INDI::PropertySwitch &switchProperty)
{
    // Forward the request to the DeviceManager
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Request to send new switch property: %s",
                 switchProperty.getName());

    DeviceManager::getInstance()->sendNewSwitch(switchProperty);
}


// Helper methods for sending standard JSON responses
void TelescopeBridge::sendResponse(httplib::Response &res, bool success, const std::string &errorMessage)
{
    // Use default transaction IDs (0) for backward compatibility
    int clientID = 0;
    int serverID = 0;

    json response =
    {
        {"ClientTransactionID", clientID},
        {"ServerTransactionID", serverID},
        {"ErrorNumber", success ? 0 : 1},
        {"ErrorMessage", success ? "" : errorMessage}
    };
    res.set_content(response.dump(), "application/json");
}

// Implementation of template methods
template <typename T>
void TelescopeBridge::sendResponse(httplib::Response &res, const T &value, bool success,
                                   const std::string &errorMessage, int clientID, int serverID)
{
    json response =
    {
        {"Value", value},
        {"ClientTransactionID", clientID},
        {"ServerTransactionID", serverID},
        {"ErrorNumber", success ? 0 : 1},
        {"ErrorMessage", success ? "" : errorMessage}
    };
    res.set_content(response.dump(), "application/json");
}

template <typename T>
void TelescopeBridge::sendResponseValue(httplib::Response &res, const T &value,
                                        bool success, const std::string &errorMessage)
{
    sendResponse(res, value, success, errorMessage, 0, 0);
}

// Implementation of sendResponseStatus
void TelescopeBridge::sendResponseStatus(httplib::Response &res, bool success,
        const std::string &errorMessage)
{
    json response =
    {
        {"ErrorNumber", success ? 0 : 1},
        {"ErrorMessage", success ? "" : errorMessage}
    };
    res.set_content(response.dump(), "application/json");
}

// Explicit template instantiations for common types
template void TelescopeBridge::sendResponse<std::string>(httplib::Response &res, const std::string &value, bool success,
        const std::string &errorMessage, int clientID, int serverID);
template void TelescopeBridge::sendResponse<double>(httplib::Response &res, const double &value, bool success,
        const std::string &errorMessage, int clientID, int serverID);
template void TelescopeBridge::sendResponse<int>(httplib::Response &res, const int &value, bool success,
        const std::string &errorMessage, int clientID, int serverID);
template void TelescopeBridge::sendResponse<bool>(httplib::Response &res, const bool &value, bool success,
        const std::string &errorMessage, int clientID, int serverID);
template void TelescopeBridge::sendResponse<json>(httplib::Response &res, const json &value, bool success,
        const std::string &errorMessage, int clientID, int serverID);

template void TelescopeBridge::sendResponseValue<std::string>(httplib::Response &res,
        const std::string &value, bool success, const std::string &errorMessage);
template void TelescopeBridge::sendResponseValue<double>(httplib::Response &res,
        const double &value, bool success, const std::string &errorMessage);
template void TelescopeBridge::sendResponseValue<int>(httplib::Response &res,
        const int &value, bool success, const std::string &errorMessage);
template void TelescopeBridge::sendResponseValue<bool>(httplib::Response &res,
        const bool &value, bool success, const std::string &errorMessage);
template void TelescopeBridge::sendResponseValue<json>(httplib::Response &res,
        const json &value, bool success, const std::string &errorMessage);
