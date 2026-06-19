/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Telescope Bridge

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

#include "device_bridge.h"
#include "basedevice.h"
#include "indiproperty.h"
#include <string>
#include <mutex>

class TelescopeBridge : public IDeviceBridge
{
    public:
        TelescopeBridge(INDI::BaseDevice device, int deviceNumber);
        ~TelescopeBridge();

        // IDeviceBridge implementation
        std::string getDeviceType() const override
        {
            return "telescope";
        }
        std::string getDeviceName() const override;
        int getDeviceNumber() const override
        {
            return m_DeviceNumber;
        }
        std::string getUniqueID() const override;

        void handleRequest(const std::string &method,
                           const httplib::Request &req,
                           httplib::Response &res) override;

        void updateProperty(INDI::Property property) override;

        // Common Alpaca API methods
        void handleConnected(const httplib::Request &req, httplib::Response &res) override;
        void handleName(const httplib::Request &req, httplib::Response &res) override;
        void handleDescription(const httplib::Request &req, httplib::Response &res) override;
        void handleDriverInfo(const httplib::Request &req, httplib::Response &res) override;
        void handleDriverVersion(const httplib::Request &req, httplib::Response &res) override;
        void handleInterfaceVersion(const httplib::Request &req, httplib::Response &res) override;

    private:
        // Telescope-specific Alpaca API methods
        void handleAlignmentMode(const httplib::Request &req, httplib::Response &res);
        void handleAltitude(const httplib::Request &req, httplib::Response &res);
        void handleApertureArea(const httplib::Request &req, httplib::Response &res);
        void handleApertureDiameter(const httplib::Request &req, httplib::Response &res);
        void handleAtHome(const httplib::Request &req, httplib::Response &res);
        void handleAtPark(const httplib::Request &req, httplib::Response &res);
        void handleAzimuth(const httplib::Request &req, httplib::Response &res);
        void handleCanFindHome(const httplib::Request &req, httplib::Response &res);
        void handleCanPark(const httplib::Request &req, httplib::Response &res);
        void handleCanPulseGuide(const httplib::Request &req, httplib::Response &res);
        void handleCanSetDeclinationRate(const httplib::Request &req, httplib::Response &res);
        void handleCanSetGuideRates(const httplib::Request &req, httplib::Response &res);
        void handleCanSetPark(const httplib::Request &req, httplib::Response &res);
        void handleCanSetPierSide(const httplib::Request &req, httplib::Response &res);
        void handleCanSetRightAscensionRate(const httplib::Request &req, httplib::Response &res);
        void handleCanSetTracking(const httplib::Request &req, httplib::Response &res);
        void handleCanSlew(const httplib::Request &req, httplib::Response &res);
        void handleCanSlewAltAz(const httplib::Request &req, httplib::Response &res);
        void handleCanSlewAltAzAsync(const httplib::Request &req, httplib::Response &res);
        void handleCanSlewAsync(const httplib::Request &req, httplib::Response &res);
        void handleCanSync(const httplib::Request &req, httplib::Response &res);
        void handleCanSyncAltAz(const httplib::Request &req, httplib::Response &res);
        void handleCanUnpark(const httplib::Request &req, httplib::Response &res);
        void handleDeclination(const httplib::Request &req, httplib::Response &res);
        void handleDeclinationRate(const httplib::Request &req, httplib::Response &res);
        void handleDestinationSideOfPier(const httplib::Request &req, httplib::Response &res);
        void handleDoesRefraction(const httplib::Request &req, httplib::Response &res);
        void handleEquatorialSystem(const httplib::Request &req, httplib::Response &res);
        void handleFocalLength(const httplib::Request &req, httplib::Response &res);
        void handleGuideRateDeclination(const httplib::Request &req, httplib::Response &res);
        void handleGuideRateRightAscension(const httplib::Request &req, httplib::Response &res);
        void handleIsPulseGuiding(const httplib::Request &req, httplib::Response &res);
        void handleRightAscension(const httplib::Request &req, httplib::Response &res);
        void handleRightAscensionRate(const httplib::Request &req, httplib::Response &res);
        void handleSideOfPier(const httplib::Request &req, httplib::Response &res);
        void handleSiderealTime(const httplib::Request &req, httplib::Response &res);
        void handleSiteElevation(const httplib::Request &req, httplib::Response &res);
        void handleSiteLatitude(const httplib::Request &req, httplib::Response &res);
        void handleSiteLongitude(const httplib::Request &req, httplib::Response &res);
        void handleSlewing(const httplib::Request &req, httplib::Response &res);
        void handleSlewSettleTime(const httplib::Request &req, httplib::Response &res);
        void handleTargetDeclination(const httplib::Request &req, httplib::Response &res);
        void handleTargetRightAscension(const httplib::Request &req, httplib::Response &res);
        void handleTracking(const httplib::Request &req, httplib::Response &res);
        void handleTrackingRate(const httplib::Request &req, httplib::Response &res);
        void handleTrackingRates(const httplib::Request &req, httplib::Response &res);
        void handleUTCDate(const httplib::Request &req, httplib::Response &res);

        // Telescope-specific Alpaca API actions
        void handleAbortSlew(const httplib::Request &req, httplib::Response &res);
        void handleAxisRates(const httplib::Request &req, httplib::Response &res);
        void handleCanMoveAxis(const httplib::Request &req, httplib::Response &res);
        void handleFindHome(const httplib::Request &req, httplib::Response &res);
        void handleMoveAxis(const httplib::Request &req, httplib::Response &res);
        void handlePark(const httplib::Request &req, httplib::Response &res);
        void handlePulseGuide(const httplib::Request &req, httplib::Response &res);
        void handleSetDeclinationRate(const httplib::Request &req, httplib::Response &res);
        void handleSetGuideRates(const httplib::Request &req, httplib::Response &res);
        void handleSetPark(const httplib::Request &req, httplib::Response &res);
        void handleSetRightAscensionRate(const httplib::Request &req, httplib::Response &res);
        void handleSetTracking(const httplib::Request &req, httplib::Response &res);
        void handleSlewToAltAz(const httplib::Request &req, httplib::Response &res);
        void handleSlewToAltAzAsync(const httplib::Request &req, httplib::Response &res);
        void handleSlewToCoordinates(const httplib::Request &req, httplib::Response &res);
        void handleSlewToCoordinatesAsync(const httplib::Request &req, httplib::Response &res);
        void handleSlewToTarget(const httplib::Request &req, httplib::Response &res);
        void handleSlewToTargetAsync(const httplib::Request &req, httplib::Response &res);
        void handleSyncToAltAz(const httplib::Request &req, httplib::Response &res);
        void handleSyncToCoordinates(const httplib::Request &req, httplib::Response &res);
        void handleSyncToTarget(const httplib::Request &req, httplib::Response &res);
        void handleUnpark(const httplib::Request &req, httplib::Response &res);

        // Helper methods
        // These methods should be implemented by the AlpacaClient
        void requestNewNumber(const INDI::PropertyNumber &numberProperty);
        void requestNewSwitch(const INDI::PropertySwitch &switchProperty);

        // Helper methods for sending standard JSON responses
        void sendResponse(httplib::Response &res, bool success, const std::string &errorMessage);

        // Generic method for sending a response with a value of any type
        template <typename T>
        void sendResponse(httplib::Response &res, const T &value, bool success = true,
                          const std::string &errorMessage = "", int clientID = 0, int serverID = 0);

        // Simplified response methods without transaction IDs (added by DeviceManager)
        template <typename T>
        void sendResponseValue(httplib::Response &res, const T &value,
                               bool success = true, const std::string &errorMessage = "");

        void sendResponseStatus(httplib::Response &res, bool success,
                                const std::string &errorMessage = "");


        // Note: Implementation is split between telescope_bridge_base.cpp and telescope_bridge_handlers.cpp

        // Device state
        INDI::BaseDevice m_Device;
        int m_DeviceNumber;

        // Current state tracking
        double m_CurrentRA {0}, m_CurrentDEC {0};
        double m_CurrentAZ {0}, m_CurrentALT {0};
        double m_TargetRA {0}, m_TargetDEC {0};
        double m_RightAscensionRate {0}, m_DeclinationRate {0};
        bool m_IsTracking {false};
        bool m_IsParked {false};
        bool m_IsSlewing {false};
        int m_PierSide {0};

        // Thread safety
        std::mutex m_Mutex;
};
