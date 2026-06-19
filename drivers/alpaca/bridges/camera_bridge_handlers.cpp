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
        sendResponseValue(res, req, connected);
    }
    else if (req.method == "PUT")
    {
        // Acknowledge connection status request, but do not control connection from bridge
        sendResponseStatus(res, req, true, "");
    }
}

void CameraBridge::handleName(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, std::string(m_Device.getDeviceName()));
}

void CameraBridge::handleDescription(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::string description = "INDI Camera Bridge for " + std::string(m_Device.getDeviceName());
    sendResponseValue(res, req, description);
}

void CameraBridge::handleDriverInfo(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::string driverInfo = "INDI Alpaca Camera Bridge v1.0";
    sendResponseValue(res, req, driverInfo);
}

void CameraBridge::handleDriverVersion(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::string version = "1.0.0";
    sendResponseValue(res, req, version);
}

void CameraBridge::handleInterfaceVersion(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    int interfaceVersion = 3; // Alpaca Camera Interface v3
    sendResponseValue(res, req, interfaceVersion);
}

// Camera Information Properties
void CameraBridge::handleCameraXSize(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_CameraXSize);
}

void CameraBridge::handleCameraYSize(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_CameraYSize);
}

void CameraBridge::handleMaxBinX(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_MaxBinX);
}

void CameraBridge::handleMaxBinY(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_MaxBinY);
}

void CameraBridge::handleCanAsymmetricBin(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_CanAsymmetricBin);
}

void CameraBridge::handlePixelSizeX(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_PixelSizeX);
}

void CameraBridge::handlePixelSizeY(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_PixelSizeY);
}

void CameraBridge::handleBinX(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, req, m_BinX);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("BinX"))
        {
            int newBinX = std::stoi(params.find("BinX")->second);
            INDI::Property binningProperty = m_Device.getProperty("CCD_BINNING");
            if (binningProperty.isValid() && binningProperty.getType() == INDI_NUMBER)
            {
                INDI::PropertyNumber numberProperty(binningProperty);
                auto horBinElement = numberProperty.findWidgetByName("HOR_BIN");
                if (horBinElement)
                {
                    horBinElement->setValue(newBinX);
                    requestNewNumber(numberProperty);
                    sendResponseStatus(res, req, true, "");
                    return;
                }
            }
            sendResponseStatus(res, req, false, "Failed to set BinX: CCD_BINNING property not found or invalid.");
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'BinX' parameter in request body");
        }
    }
}

void CameraBridge::handleBinY(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, req, m_BinY);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("BinY"))
        {
            int newBinY = std::stoi(params.find("BinY")->second);
            INDI::Property binningProperty = m_Device.getProperty("CCD_BINNING");
            if (binningProperty.isValid() && binningProperty.getType() == INDI_NUMBER)
            {
                INDI::PropertyNumber numberProperty(binningProperty);
                auto verBinElement = numberProperty.findWidgetByName("VER_BIN");
                if (verBinElement)
                {
                    verBinElement->setValue(newBinY);
                    requestNewNumber(numberProperty);
                    sendResponseStatus(res, req, true, "");
                    return;
                }
            }
            sendResponseStatus(res, req, false, "Failed to set BinY: CCD_BINNING property not found or invalid.");
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'BinY' parameter in request body");
        }
    }
}

void CameraBridge::handleStartX(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, req, m_StartX);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("StartX"))
        {
            int newStartX = std::stoi(params.find("StartX")->second);
            INDI::Property frameProperty = m_Device.getProperty("CCD_FRAME");
            if (frameProperty.isValid() && frameProperty.getType() == INDI_NUMBER)
            {
                INDI::PropertyNumber numberProperty(frameProperty);
                auto xElement = numberProperty.findWidgetByName("X");
                if (xElement)
                {
                    xElement->setValue(newStartX);
                    requestNewNumber(numberProperty);
                    sendResponseStatus(res, req, true, "");
                    return;
                }
            }
            sendResponseStatus(res, req, false, "Failed to set StartX: CCD_FRAME property not found or invalid.");
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'StartX' parameter in request body");
        }
    }
}

void CameraBridge::handleStartY(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, req, m_StartY);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("StartY"))
        {
            int newStartY = std::stoi(params.find("StartY")->second);
            INDI::Property frameProperty = m_Device.getProperty("CCD_FRAME");
            if (frameProperty.isValid() && frameProperty.getType() == INDI_NUMBER)
            {
                INDI::PropertyNumber numberProperty(frameProperty);
                auto yElement = numberProperty.findWidgetByName("Y");
                if (yElement)
                {
                    yElement->setValue(newStartY);
                    requestNewNumber(numberProperty);
                    sendResponseStatus(res, req, true, "");
                    return;
                }
            }
            sendResponseStatus(res, req, false, "Failed to set StartY: CCD_FRAME property not found or invalid.");
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'StartY' parameter in request body");
        }
    }
}

void CameraBridge::handleNumX(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, req, m_NumX);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("NumX"))
        {
            int newNumX = std::stoi(params.find("NumX")->second);
            INDI::Property frameProperty = m_Device.getProperty("CCD_FRAME");
            if (frameProperty.isValid() && frameProperty.getType() == INDI_NUMBER)
            {
                INDI::PropertyNumber numberProperty(frameProperty);
                auto widthElement = numberProperty.findWidgetByName("WIDTH");
                if (widthElement)
                {
                    widthElement->setValue(newNumX);
                    requestNewNumber(numberProperty);
                    sendResponseStatus(res, req, true, "");
                    return;
                }
            }
            sendResponseStatus(res, req, false, "Failed to set NumX: CCD_FRAME property not found or invalid.");
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'NumX' parameter in request body");
        }
    }
}

void CameraBridge::handleNumY(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, req, m_NumY);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("NumY"))
        {
            int newNumY = std::stoi(params.find("NumY")->second);
            INDI::Property frameProperty = m_Device.getProperty("CCD_FRAME");
            if (frameProperty.isValid() && frameProperty.getType() == INDI_NUMBER)
            {
                INDI::PropertyNumber numberProperty(frameProperty);
                auto heightElement = numberProperty.findWidgetByName("HEIGHT");
                if (heightElement)
                {
                    heightElement->setValue(newNumY);
                    requestNewNumber(numberProperty);
                    sendResponseStatus(res, req, true, "");
                    return;
                }
            }
            sendResponseStatus(res, req, false, "Failed to set NumY: CCD_FRAME property not found or invalid.");
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'NumY' parameter in request body");
        }
    }
}

// Camera Capabilities
void CameraBridge::handleCanAbortExposure(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_CanAbortExposure);
}

void CameraBridge::handleCanStopExposure(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_CanStopExposure);
}

void CameraBridge::handleCanPulseGuide(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_CanPulseGuide);
}

void CameraBridge::handleCanSetCCDTemperature(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_CanSetCCDTemperature);
}

void CameraBridge::handleCanFastReadout(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    // INDI cameras typically don't support fast readout mode
    sendResponseValue(res, req, false);
}

void CameraBridge::handleHasShutter(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_HasShutter);
}

// Temperature Control
void CameraBridge::handleCCDTemperature(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_CCDTemperature);
}

void CameraBridge::handleCoolerOn(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, req, m_CoolerOn);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("CoolerOn"))
        {
            bool newCoolerOn = (params.find("CoolerOn")->second == "true");
            INDI::Property coolerProperty = m_Device.getProperty("CCD_COOLER");
            if (coolerProperty.isValid() && coolerProperty.getType() == INDI_SWITCH)
            {
                INDI::PropertySwitch switchProperty(coolerProperty);
                if (newCoolerOn)
                {
                    switchProperty.findWidgetByName("COOLER_ON")->setState(ISS_ON);
                    switchProperty.findWidgetByName("COOLER_OFF")->setState(ISS_OFF);
                }
                else
                {
                    switchProperty.findWidgetByName("COOLER_ON")->setState(ISS_OFF);
                    switchProperty.findWidgetByName("COOLER_OFF")->setState(ISS_ON);
                }
                requestNewSwitch(switchProperty);
                sendResponseStatus(res, req, true, "");
                return;
            }
            sendResponseStatus(res, req, false, "Failed to set CoolerOn: CCD_COOLER property not found or invalid.");
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'CoolerOn' parameter in request body");
        }
    }
}

void CameraBridge::handleCoolerPower(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_CoolerPower);
}

void CameraBridge::handleCanGetCoolerPower(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    // Return true if CCD_COOLER_POWER property exists
    INDI::Property coolerPowerProperty = m_Device.getProperty("CCD_COOLER_POWER");
    bool canGet = coolerPowerProperty.isValid();
    sendResponseValue(res, req, canGet);
}

void CameraBridge::handleSetCCDTemperature(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        // Return the target temperature setpoint
        sendResponseValue(res, req, m_TargetCCDTemperature);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("SetCCDTemperature"))
        {
            double newTemperature = std::stod(params.find("SetCCDTemperature")->second);
            m_TargetCCDTemperature = newTemperature;  // Track the target

            INDI::Property temperatureProperty = m_Device.getProperty("CCD_TEMPERATURE");
            if (temperatureProperty.isValid() && temperatureProperty.getType() == INDI_NUMBER)
            {
                INDI::PropertyNumber numberProperty(temperatureProperty);
                numberProperty[0].setValue(newTemperature);
                requestNewNumber(numberProperty);
                sendResponseStatus(res, req, true, "");
                return;
            }
            sendResponseStatus(res, req, false, "Failed to set CCDTemperature: CCD_TEMPERATURE property not found or invalid.");
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'SetCCDTemperature' parameter in request body");
        }
    }
    else
    {
        sendResponseStatus(res, req, false, "Method not supported");
    }
}

// Gain and Offset
void CameraBridge::handleGain(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_HasGain)
    {
        sendResponseStatus(res, req, false, "Gain not supported");
        return;
    }

    if (req.method == "GET")
    {
        sendResponseValue(res, req, static_cast<int>(m_Gain));
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
                        sendResponseStatus(res, req, true, "");
                        return;
                    }
                }
                sendResponseStatus(res, req, false, "Failed to set Gain via CCD_CONTROLS");
            }
            else
            {
                INDI::Property gainProperty = m_Device.getProperty("CCD_GAIN");
                if (gainProperty.isValid() && gainProperty.getType() == INDI_NUMBER)
                {
                    INDI::PropertyNumber numberProperty(gainProperty);
                    numberProperty[0].setValue(newGain);
                    requestNewNumber(numberProperty);
                    sendResponseStatus(res, req, true, "");
                    return;
                }
                sendResponseStatus(res, req, false, "Failed to set Gain via CCD_GAIN");
            }
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'Gain' parameter in request body");
        }
    }
}

void CameraBridge::handleGainMin(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasGain)
        sendResponseValue(res, req, static_cast<int>(m_GainMin));
    else
        sendResponseStatus(res, req, false, "Gain not supported");
}

void CameraBridge::handleGainMax(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasGain)
        sendResponseValue(res, req, static_cast<int>(m_GainMax));
    else
        sendResponseStatus(res, req, false, "Gain not supported");
}

void CameraBridge::handleGains(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasGain)
    {
        json gainsArray = json::array();
        for (const auto& gain : m_Gains)
            gainsArray.push_back(gain);
        sendResponseValue(res, req, gainsArray);
    }
    else
    {
        sendResponseStatus(res, req, false, "Gain not supported");
    }
}

void CameraBridge::handleOffset(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_HasOffset)
    {
        sendResponseStatus(res, req, false, "Offset not supported");
        return;
    }

    if (req.method == "GET")
    {
        sendResponseValue(res, req, static_cast<int>(m_Offset));
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
                        sendResponseStatus(res, req, true, "");
                        return;
                    }
                }
                sendResponseStatus(res, req, false, "Failed to set Offset via CCD_CONTROLS");
            }
            else
            {
                INDI::Property offsetProperty = m_Device.getProperty("CCD_OFFSET");
                if (offsetProperty.isValid() && offsetProperty.getType() == INDI_NUMBER)
                {
                    INDI::PropertyNumber numberProperty(offsetProperty);
                    numberProperty[0].setValue(newOffset);
                    requestNewNumber(numberProperty);
                    sendResponseStatus(res, req, true, "");
                    return;
                }
                sendResponseStatus(res, req, false, "Failed to set Offset via CCD_OFFSET");
            }
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'Offset' parameter in request body");
        }
    }
}

void CameraBridge::handleOffsetMin(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasOffset)
        sendResponseValue(res, req, static_cast<int>(m_OffsetMin));
    else
        sendResponseStatus(res, req, false, "Offset not supported");
}

void CameraBridge::handleOffsetMax(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasOffset)
        sendResponseValue(res, req, static_cast<int>(m_OffsetMax));
    else
        sendResponseStatus(res, req, false, "Offset not supported");
}

void CameraBridge::handleOffsets(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_HasOffset)
    {
        json offsetsArray = json::array();
        for (const auto& offset : m_Offsets)
            offsetsArray.push_back(offset);
        sendResponseValue(res, req, offsetsArray);
    }
    else
    {
        sendResponseStatus(res, req, false, "Offset not supported");
    }
}

// Readout Modes and Sensor Info
void CameraBridge::handleReadoutMode(const httplib::Request &req, httplib::Response &res)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (req.method == "GET")
    {
        sendResponseValue(res, req, m_ReadoutMode);
    }
    else if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("ReadoutMode"))
        {
            int newReadoutModeIndex = std::stoi(params.find("ReadoutMode")->second);

            if (newReadoutModeIndex >= 0 && static_cast<size_t>(newReadoutModeIndex) < m_ReadoutModes.size())
            {
                std::string newReadoutModeName = m_ReadoutModes[newReadoutModeIndex];
                INDI::Property readoutModeProperty = m_Device.getProperty("CCD_READOUT_MODE");
                if (readoutModeProperty.isValid() && readoutModeProperty.getType() == INDI_SWITCH)
                {
                    INDI::PropertySwitch switchProperty(readoutModeProperty);
                    for (auto &sw : switchProperty)
                    {
                        if (sw.isNameMatch(newReadoutModeName.c_str()))
                            sw.setState(ISS_ON);
                        else
                            sw.setState(ISS_OFF);
                    }
                    requestNewSwitch(switchProperty);
                    sendResponseStatus(res, req, true, "");
                    return;
                }
                sendResponseStatus(res, req, false, "Failed to set ReadoutMode: CCD_READOUT_MODE property not found or invalid.");
            }
            else
            {
                sendResponseStatus(res, req, false, "Invalid ReadoutMode index.");
            }
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'ReadoutMode' parameter in request body");
        }
    }
}

void CameraBridge::handleReadoutModes(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);

    json modesArray = json::array();
    for (const auto& mode : m_ReadoutModes)
        modesArray.push_back(mode);
    sendResponseValue(res, req, modesArray);
}

void CameraBridge::handleSensorType(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_SensorType);
}

void CameraBridge::handleBayerOffsetX(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_BayerOffsetX);
}

void CameraBridge::handleBayerOffsetY(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_BayerOffsetY);
}

void CameraBridge::handleSensorName(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, std::string());
}

// Exposure Control
void CameraBridge::handleStartExposure(const httplib::Request &req, httplib::Response &res)
{
    if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        // Accept both "Duration" (ASCOM standard) and "ExposureDuration" (backward compatibility)
        double exposureDuration = 0.0;
        if (params.count("Duration"))
        {
            exposureDuration = std::stod(params.find("Duration")->second);
        }
        else if (params.count("ExposureDuration"))
        {
            exposureDuration = std::stod(params.find("ExposureDuration")->second);
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'Duration' parameter in request body");
            return;
        }

        // Handle Light parameter for frame type (light vs dark frame)
        bool isLightFrame = true; // Default to light frame
        if (params.count("Light"))
        {
            std::string lightParam = params.find("Light")->second;
            isLightFrame = (lightParam == "true" || lightParam == "True" || lightParam == "1");
        }

        // Set frame type if supported
        INDI::Property frameTypeProperty = m_Device.getProperty("CCD_FRAME_TYPE");
        if (frameTypeProperty.isValid() && frameTypeProperty.getType() == INDI_SWITCH)
        {
            INDI::PropertySwitch switchProperty(frameTypeProperty);
            for (auto &sw : switchProperty)
            {
                if (isLightFrame && sw.isNameMatch("FRAME_LIGHT"))
                    sw.setState(ISS_ON);
                else if (!isLightFrame && sw.isNameMatch("FRAME_DARK"))
                    sw.setState(ISS_ON);
                else
                    sw.setState(ISS_OFF);
            }
            requestNewSwitch(switchProperty);
        }

        // Start exposure
        INDI::Property exposureProperty = m_Device.getProperty("CCD_EXPOSURE");
        if (exposureProperty.isValid() && exposureProperty.getType() == INDI_NUMBER)
        {
            INDI::PropertyNumber numberProperty(exposureProperty);
            numberProperty[0].setValue(exposureDuration);
            requestNewNumber(numberProperty);
            sendResponseStatus(res, req, true, "");
            return;
        }
        sendResponseStatus(res, req, false, "Failed to start exposure: CCD_EXPOSURE property not found or invalid.");
    }
    else
    {
        sendResponseStatus(res, req, false, "Method not supported");
    }
}

void CameraBridge::handleStopExposure(const httplib::Request &req, httplib::Response &res)
{
    if (req.method == "PUT")
    {
        INDI::Property abortProperty = m_Device.getProperty("CCD_ABORT_EXPOSURE");
        if (abortProperty.isValid() && abortProperty.getType() == INDI_SWITCH)
        {
            INDI::PropertySwitch switchProperty(abortProperty);
            auto abortElement = switchProperty.findWidgetByName("ABORT");
            if (abortElement)
            {
                abortElement->setState(ISS_ON);
                requestNewSwitch(switchProperty);
                sendResponseStatus(res, req, true, "");
                return;
            }
        }
        sendResponseStatus(res, req, false, "Failed to stop exposure: CCD_ABORT_EXPOSURE property not found or invalid.");
    }
    else
    {
        sendResponseStatus(res, req, false, "Method not supported");
    }
}

void CameraBridge::handleAbortExposure(const httplib::Request &req, httplib::Response &res)
{
    if (req.method == "PUT")
    {
        INDI::Property abortProperty = m_Device.getProperty("CCD_ABORT_EXPOSURE");
        if (abortProperty.isValid() && abortProperty.getType() == INDI_SWITCH)
        {
            INDI::PropertySwitch switchProperty(abortProperty);
            auto abortElement = switchProperty.findWidgetByName("ABORT");
            if (abortElement)
            {
                abortElement->setState(ISS_ON);
                requestNewSwitch(switchProperty);
                sendResponseStatus(res, req, true, "");
                return;
            }
        }
        sendResponseStatus(res, req, false, "Failed to abort exposure: CCD_ABORT_EXPOSURE property not found or invalid.");
    }
    else
    {
        sendResponseStatus(res, req, false, "Method not supported");
    }
}

void CameraBridge::handleImageReady(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_ImageReady);
}

void CameraBridge::handleCameraState(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_CameraState);
}

void CameraBridge::handlePercentCompleted(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);

    // Calculate percentage if exposing
    if (m_IsExposing && m_LastExposureDuration > 0)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_ExposureStartTime).count() / 1000.0;
        double percentage = std::min(100.0, (elapsed / m_LastExposureDuration) * 100.0);
        sendResponseValue(res, req, percentage);
    }
    else
    {
        sendResponseValue(res, req, m_PercentCompleted);
    }
}

void CameraBridge::handleLastExposureDuration(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_LastExposureDuration);
}

void CameraBridge::handleLastExposureStartTime(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_LastExposureStartTime);
}

void CameraBridge::handleExposureMin(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_ExposureMin);
}

void CameraBridge::handleExposureMax(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_ExposureMax);
}

// Image Data
void CameraBridge::handleImageArray(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_ImageReady || m_LastImageData.empty())
    {
        sendResponseStatus(res, req, false, "No image available");
        return;
    }

    // Format image as JSON array
    json imageArray;
    formatImageAsJSON(m_LastImageData, m_LastImageWidth, m_LastImageHeight,
                      m_LastImageBPP, m_LastImageNAxis, imageArray);

    // Even though the actual data is UInt16, we must report it as Int32
    int elementType = 2;  // Int32

    // Create response with metadata (Type and Rank are required by NINA)
    uint32_t clientID, serverID;
    extractTransactionIDs(req, clientID, serverID);

    json response =
    {
        {"Value", imageArray},
        {"Type", elementType},
        {"Rank", m_LastImageNAxis},
        {"ClientTransactionID", clientID},
        {"ServerTransactionID", serverID},
        {"ErrorNumber", 0},
        {"ErrorMessage", ""}
    };

    res.set_content(response.dump(), "application/json");
}

void CameraBridge::handleImageArrayVariant(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_ImageReady || m_LastImageData.empty())
    {
        sendResponseStatus(res, req, false, "No image available");
        return;
    }

    // Format image as JSON array
    json imageArray;
    formatImageAsJSON(m_LastImageData, m_LastImageWidth, m_LastImageHeight,
                      m_LastImageBPP, m_LastImageNAxis, imageArray);

    // Even though the actual data is UInt16, we must report it as Int32
    int elementType = 2;  // Int32

    // Create response with metadata
    uint32_t clientID, serverID;
    extractTransactionIDs(req, clientID, serverID);

    json response =
    {
        {"Value", imageArray},
        {"Type", elementType},
        {"Rank", m_LastImageNAxis},
        {"ClientTransactionID", clientID},
        {"ServerTransactionID", serverID},
        {"ErrorNumber", 0},
        {"ErrorMessage", ""}
    };

    res.set_content(response.dump(), "application/json");
}

// Guiding
void CameraBridge::handleIsPulseGuiding(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_IsPulseGuiding);
}

void CameraBridge::handlePulseGuide(const httplib::Request &req, httplib::Response &res)
{
    if (req.method == "PUT")
    {
        httplib::Params params;
        httplib::detail::parse_query_text(req.body, params);

        if (params.count("Direction") && params.count("Duration"))
        {
            int direction = std::stoi(params.find("Direction")->second);
            double duration = std::stod(params.find("Duration")->second); // Duration in milliseconds

            INDI::Property guideNSProperty = m_Device.getProperty("TELESCOPE_TIMED_GUIDE_NS");
            INDI::Property guideWEProperty = m_Device.getProperty("TELESCOPE_TIMED_GUIDE_WE");

            if (!guideNSProperty.isValid() && !guideWEProperty.isValid())
            {
                sendResponseStatus(res, req, false,
                                   "Pulse guiding properties (TELESCOPE_TIMED_GUIDE_NS, TELESCOPE_TIMED_GUIDE_WE) not found.");
                return;
            }

            if (direction == 0) // North (ASCOM Alpaca standard)
            {
                if (guideNSProperty.isValid() && guideNSProperty.getType() == INDI_NUMBER)
                {
                    INDI::PropertyNumber numberProperty(guideNSProperty);
                    auto northElement = numberProperty.findWidgetByName("TIMED_GUIDE_N");
                    if (northElement)
                    {
                        northElement->setValue(duration);
                        requestNewNumber(numberProperty);
                        sendResponseStatus(res, req, true, "");
                        return;
                    }
                }
                sendResponseStatus(res, req, false, "Failed to pulse guide North.");
            }
            else if (direction == 1) // South (ASCOM Alpaca standard)
            {
                if (guideNSProperty.isValid() && guideNSProperty.getType() == INDI_NUMBER)
                {
                    INDI::PropertyNumber numberProperty(guideNSProperty);
                    auto southElement = numberProperty.findWidgetByName("TIMED_GUIDE_S");
                    if (southElement)
                    {
                        southElement->setValue(duration);
                        requestNewNumber(numberProperty);
                        sendResponseStatus(res, req, true, "");
                        return;
                    }
                }
                sendResponseStatus(res, req, false, "Failed to pulse guide South.");
            }
            else if (direction == 2) // East (ASCOM Alpaca standard)
            {
                if (guideWEProperty.isValid() && guideWEProperty.getType() == INDI_NUMBER)
                {
                    INDI::PropertyNumber numberProperty(guideWEProperty);
                    auto eastElement = numberProperty.findWidgetByName("TIMED_GUIDE_E");
                    if (eastElement)
                    {
                        eastElement->setValue(duration);
                        requestNewNumber(numberProperty);
                        sendResponseStatus(res, req, true, "");
                        return;
                    }
                }
                sendResponseStatus(res, req, false, "Failed to pulse guide East.");
            }
            else if (direction == 3) // West (ASCOM Alpaca standard)
            {
                if (guideWEProperty.isValid() && guideWEProperty.getType() == INDI_NUMBER)
                {
                    INDI::PropertyNumber numberProperty(guideWEProperty);
                    auto westElement = numberProperty.findWidgetByName("TIMED_GUIDE_W");
                    if (westElement)
                    {
                        westElement->setValue(duration);
                        requestNewNumber(numberProperty);
                        sendResponseStatus(res, req, true, "");
                        return;
                    }
                }
                sendResponseStatus(res, req, false, "Failed to pulse guide West.");
            }
            else
            {
                sendResponseStatus(res, req, false, "Invalid 'Direction' parameter.");
            }
        }
        else
        {
            sendResponseStatus(res, req, false, "Missing 'Direction' or 'Duration' parameter in request body");
        }
    }
    else
    {
        sendResponseStatus(res, req, false, "Method not supported");
    }
}

// Additional Properties
void CameraBridge::handleMaxADU(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_MaxADU);
}

void CameraBridge::handleElectronsPerADU(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_ElectronsPerADU);
}

void CameraBridge::handleFullWellCapacity(const httplib::Request &req, httplib::Response &res)
{
    // Transaction IDs extracted in sendResponseValue
    std::lock_guard<std::mutex> lock(m_Mutex);
    sendResponseValue(res, req, m_FullWellCapacity);
}
