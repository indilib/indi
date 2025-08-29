/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Camera Bridge - API Handlers

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

#include "camera_bridge.h"
#include "indilogger.h"
#include <httplib.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <map>


#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

// Common Alpaca API methods
void CameraBridge::handleConnected(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        // Return connection status
        bool connected = m_Device.isConnected();
        sendResponseValue(res, connected);
    }
    else if (req.method == "PUT")
    {
        // Acknowledge connection status request, but do not control connection from bridge
        sendResponseStatus(res, true, "");
    }
}

void CameraBridge::handleName(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, std::string(m_Device.getDeviceName()));
}

void CameraBridge::handleDescription(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::string description = "INDI Camera Bridge for " + std::string(m_Device.getDeviceName());
    sendResponseValue(res, description);
}

void CameraBridge::handleDriverInfo(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::string driverInfo = "INDI Alpaca Camera Bridge v1.0";
    sendResponseValue(res, driverInfo);
}

void CameraBridge::handleDriverVersion(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::string version = "1.0.0";
    sendResponseValue(res, version);
}

void CameraBridge::handleInterfaceVersion(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    int interfaceVersion = 3; // Alpaca Camera Interface v3
    sendResponseValue(res, interfaceVersion);
}

// Camera Information Properties
void CameraBridge::handleCameraXSize(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CameraXSize);
}

void CameraBridge::handleCameraYSize(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CameraYSize);
}

void CameraBridge::handleMaxBinX(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_MaxBinX);
}

void CameraBridge::handleMaxBinY(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_MaxBinY);
}

void CameraBridge::handleCanAsymmetricBin(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CanAsymmetricBin);
}

void CameraBridge::handlePixelSizeX(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_PixelSizeX);
}

void CameraBridge::handlePixelSizeY(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_PixelSizeY);
}

void CameraBridge::handleBinX(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, m_BinX);
    }
    else if (req.method == "PUT")
    {
        // Set binning X - would need to send to INDI
        sendResponseStatus(res, false, "Setting binning not implemented yet");
    }
}

void CameraBridge::handleBinY(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, m_BinY);
    }
    else if (req.method == "PUT")
    {
        // Set binning Y - would need to send to INDI
        sendResponseStatus(res, false, "Setting binning not implemented yet");
    }
}

void CameraBridge::handleStartX(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, m_StartX);
    }
    else if (req.method == "PUT")
    {
        // Set start X - would need to send to INDI
        sendResponseStatus(res, false, "Setting frame not implemented yet");
    }
}

void CameraBridge::handleStartY(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, m_StartY);
    }
    else if (req.method == "PUT")
    {
        // Set start Y - would need to send to INDI
        sendResponseStatus(res, false, "Setting frame not implemented yet");
    }
}

void CameraBridge::handleNumX(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, m_NumX);
    }
    else if (req.method == "PUT")
    {
        // Set num X - would need to send to INDI
        sendResponseStatus(res, false, "Setting frame not implemented yet");
    }
}

void CameraBridge::handleNumY(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, m_NumY);
    }
    else if (req.method == "PUT")
    {
        // Set num Y - would need to send to INDI
        sendResponseStatus(res, false, "Setting frame not implemented yet");
    }
}

// Camera Capabilities
void CameraBridge::handleCanAbortExposure(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CanAbortExposure);
}

void CameraBridge::handleCanStopExposure(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CanStopExposure);
}

void CameraBridge::handleCanPulseGuide(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CanPulseGuide);
}

void CameraBridge::handleCanSetCCDTemperature(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CanSetCCDTemperature);
}

void CameraBridge::handleHasShutter(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_HasShutter);
}

// Temperature Control
void CameraBridge::handleCCDTemperature(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CCDTemperature);
}

void CameraBridge::handleCoolerOn(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, m_CoolerOn);
    }
    else if (req.method == "PUT")
    {
        // Set cooler state - would need to send to INDI
        sendResponseStatus(res, false, "Setting cooler not implemented yet");
    }
}

void CameraBridge::handleCoolerPower(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CoolerPower);
}

void CameraBridge::handleSetCCDTemperature(const httplib::Request &req, httplib::Response &res)
{
    if (req.method == "PUT")
    {
        // Set temperature - would need to send to INDI
        sendResponseStatus(res, false, "Setting temperature not implemented yet");
    }
    else
    {
        sendResponseStatus(res, false, "Method not supported");
    }
}

// Gain and Offset
void CameraBridge::handleGain(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_HasGain)
    {
        sendResponseStatus(res, false, "Gain not supported");
        return;
    }

    if (req.method == "GET")
    {
        sendResponseValue(res, m_Gain);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("Gain"))
        {
            double newGain = std::stod(params.find("Gain")->second);

            if (m_UsesCCDControlsForGainOffset)
            {
                INDI::Property controlsProperty = m_Device.getProperty("CCD_CONTROLS");
                if (controlsProperty.isValid() && controlsProperty.getType() == INDI_NUMBER)
                {
                    INDI::PropertyNumber numberProperty(controlsProperty);
                    auto gainElement = numberProperty.findWidgetByName("Gain");
                    if (gainElement)
                    {
                        gainElement->setValue(newGain);
                        requestNewNumber(numberProperty);
                        sendResponseStatus(res, true, "");
                        return;
                    }
                }
                sendResponseStatus(res, false, "Failed to set Gain via CCD_CONTROLS");
            }
            else
            {
                INDI::Property gainProperty = m_Device.getProperty("CCD_GAIN");
                if (gainProperty.isValid() && gainProperty.getType() == INDI_NUMBER)
                {
                    INDI::PropertyNumber numberProperty(gainProperty);
                    numberProperty[0].setValue(newGain);
                    requestNewNumber(numberProperty);
                    sendResponseStatus(res, true, "");
                    return;
                }
                sendResponseStatus(res, false, "Failed to set Gain via CCD_GAIN");
            }
        }
        else
        {
            sendResponseStatus(res, false, "Missing 'Gain' parameter in request body");
        }
    }
}

void CameraBridge::handleGainMin(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasGain)
        sendResponseValue(res, m_GainMin);
    else
        sendResponseStatus(res, false, "Gain not supported");
}

void CameraBridge::handleGainMax(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasGain)
        sendResponseValue(res, m_GainMax);
    else
        sendResponseStatus(res, false, "Gain not supported");
}

void CameraBridge::handleGains(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasGain)
    {
        json gainsArray = json::array();
        for (const auto& gain : m_Gains)
            gainsArray.push_back(gain);
        sendResponseValue(res, gainsArray);
    }
    else
    {
        sendResponseStatus(res, false, "Gain not supported");
    }
}

void CameraBridge::handleOffset(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_HasOffset)
    {
        sendResponseStatus(res, false, "Offset not supported");
        return;
    }

    if (req.method == "GET")
    {
        sendResponseValue(res, m_Offset);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("Offset"))
        {
            double newOffset = std::stod(params.find("Offset")->second);

            if (m_UsesCCDControlsForGainOffset)
            {
                INDI::Property controlsProperty = m_Device.getProperty("CCD_CONTROLS");
                if (controlsProperty.isValid() && controlsProperty.getType() == INDI_NUMBER)
                {
                    INDI::PropertyNumber numberProperty(controlsProperty);
                    auto offsetElement = numberProperty.findWidgetByName("Offset");
                    if (offsetElement)
                    {
                        offsetElement->setValue(newOffset);
                        requestNewNumber(numberProperty);
                        sendResponseStatus(res, true, "");
                        return;
                    }
                }
                sendResponseStatus(res, false, "Failed to set Offset via CCD_CONTROLS");
            }
            else
            {
                INDI::Property offsetProperty = m_Device.getProperty("CCD_OFFSET");
                if (offsetProperty.isValid() && offsetProperty.getType() == INDI_NUMBER)
                {
                    INDI::PropertyNumber numberProperty(offsetProperty);
                    numberProperty[0].setValue(newOffset);
                    requestNewNumber(numberProperty);
                    sendResponseStatus(res, true, "");
                    return;
                }
                sendResponseStatus(res, false, "Failed to set Offset via CCD_OFFSET");
            }
        }
        else
        {
            sendResponseStatus(res, false, "Missing 'Offset' parameter in request body");
        }
    }
}

void CameraBridge::handleOffsetMin(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasOffset)
        sendResponseValue(res, m_OffsetMin);
    else
        sendResponseStatus(res, false, "Offset not supported");
}

void CameraBridge::handleOffsetMax(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasOffset)
        sendResponseValue(res, m_OffsetMax);
    else
        sendResponseStatus(res, false, "Offset not supported");
}

void CameraBridge::handleOffsets(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasOffset)
    {
        json offsetsArray = json::array();
        for (const auto& offset : m_Offsets)
            offsetsArray.push_back(offset);
        sendResponseValue(res, offsetsArray);
    }
    else
    {
        sendResponseStatus(res, false, "Offset not supported");
    }
}

// Readout Modes and Sensor Info
void CameraBridge::handleReadoutMode(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, m_ReadoutMode);
    }
    else if (req.method == "PUT")
    {
        // Set readout mode - would need to send to INDI
        sendResponseStatus(res, false, "Setting readout mode not implemented yet");
    }
}

void CameraBridge::handleReadoutModes(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);

    json modesArray = json::array();
    for (const auto& mode : m_ReadoutModes)
        modesArray.push_back(mode);
    sendResponseValue(res, modesArray);
}

void CameraBridge::handleSensorType(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_SensorType);
}

void CameraBridge::handleBayerOffsetX(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_BayerOffsetX);
}

void CameraBridge::handleBayerOffsetY(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_BayerOffsetY);
}

// Exposure Control
void CameraBridge::handleStartExposure(const httplib::Request &req, httplib::Response &res)
{
    if (req.method == "PUT")
    {
        // Start exposure - would need to send to INDI
        sendResponseStatus(res, false, "Starting exposure not implemented yet");
    }
    else
    {
        sendResponseStatus(res, false, "Method not supported");
    }
}

void CameraBridge::handleStopExposure(const httplib::Request &req, httplib::Response &res)
{
    if (req.method == "PUT")
    {
        // Stop exposure - would need to send to INDI
        sendResponseStatus(res, false, "Stopping exposure not implemented yet");
    }
    else
    {
        sendResponseStatus(res, false, "Method not supported");
    }
}

void CameraBridge::handleAbortExposure(const httplib::Request &req, httplib::Response &res)
{
    if (req.method == "PUT")
    {
        // Abort exposure - would need to send to INDI
        sendResponseStatus(res, false, "Aborting exposure not implemented yet");
    }
    else
    {
        sendResponseStatus(res, false, "Method not supported");
    }
}

void CameraBridge::handleImageReady(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_ImageReady);
}

void CameraBridge::handleCameraState(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_CameraState);
}

void CameraBridge::handlePercentCompleted(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);

    // Calculate percentage if exposing
    if (m_IsExposing && m_LastExposureDuration > 0)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_ExposureStartTime).count() / 1000.0;
        double percentage = std::min(100.0, (elapsed / m_LastExposureDuration) * 100.0);
        sendResponseValue(res, percentage);
    }
    else
    {
        sendResponseValue(res, m_PercentCompleted);
    }
}

void CameraBridge::handleLastExposureDuration(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_LastExposureDuration);
}

void CameraBridge::handleLastExposureStartTime(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_LastExposureStartTime);
}

// Image Data
void CameraBridge::handleImageArray(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_ImageReady || m_LastImageData.empty())
    {
        sendResponseStatus(res, false, "No image available");
        return;
    }

    // Format image as JSON array
    json imageArray;
    formatImageAsJSON(m_LastImageData, m_LastImageWidth, m_LastImageHeight,
                      m_LastImageBPP, m_LastImageNAxis, imageArray);
    sendResponseValue(res, imageArray);
}

void CameraBridge::handleImageArrayVariant(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_ImageReady || m_LastImageData.empty())
    {
        sendResponseStatus(res, false, "No image available");
        return;
    }

    // For now, return same as ImageArray - ImageBytes format would be implemented later
    json imageArray;
    formatImageAsJSON(m_LastImageData, m_LastImageWidth, m_LastImageHeight,
                      m_LastImageBPP, m_LastImageNAxis, imageArray);
    sendResponseValue(res, imageArray);
}

// Guiding
void CameraBridge::handleIsPulseGuiding(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_IsPulseGuiding);
}

void CameraBridge::handlePulseGuide(const httplib::Request &req, httplib::Response &res)
{
    if (req.method == "PUT")
    {
        // Pulse guide - would need to send to INDI
        sendResponseStatus(res, false, "Pulse guiding not implemented yet");
    }
    else
    {
        sendResponseStatus(res, false, "Method not supported");
    }
}

// Additional Properties
void CameraBridge::handleMaxADU(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_MaxADU);
}

void CameraBridge::handleElectronsPerADU(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_ElectronsPerADU);
}

void CameraBridge::handleFullWellCapacity(const httplib::Request &req, httplib::Response &res)
{
    INDI_UNUSED(req);
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, m_FullWellCapacity);
}
