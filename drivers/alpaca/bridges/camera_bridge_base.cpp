/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Camera Bridge - Base Implementation

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
#include "device_manager.h"

#include <httplib.h>
#include <cmath>
#include <cstring>
#include <sstream>
#include <fitsio.h>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

CameraBridge::CameraBridge(INDI::BaseDevice device, int deviceNumber)
    : m_Device(device), m_DeviceNumber(deviceNumber)
{
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_SESSION, "Created camera bridge for device %s with number %d",
                 device.getDeviceName(), deviceNumber);

    // Check if the underlying INDI device supports pulse guiding
    if (m_Device.getDriverInterface() & INDI::BaseDevice::GUIDER_INTERFACE)
    {
        m_CanPulseGuide = true;
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_SESSION, "Device %s supports pulse guiding.",
                     device.getDeviceName());
    }
}

CameraBridge::~CameraBridge()
{
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_SESSION, "Destroyed camera bridge for device %s",
                 m_Device.getDeviceName());
}

std::string CameraBridge::getDeviceName() const
{
    return m_Device.getDeviceName();
}

std::string CameraBridge::getUniqueID() const
{
    // Generate a unique ID based on the device name
    return "INDI_" + std::string(m_Device.getDeviceName());
}

void CameraBridge::handleRequest(const std::string &method, const httplib::Request &req, httplib::Response &res)
{
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Handling camera request: %s", method.c_str());

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
    // Camera information properties
    else if (method == "cameraxsize")
        handleCameraXSize(req, res);
    else if (method == "cameraysize")
        handleCameraYSize(req, res);
    else if (method == "maxbinx")
        handleMaxBinX(req, res);
    else if (method == "maxbiny")
        handleMaxBinY(req, res);
    else if (method == "canasymmetricbin")
        handleCanAsymmetricBin(req, res);
    else if (method == "pixelsizex")
        handlePixelSizeX(req, res);
    else if (method == "pixelsizey")
        handlePixelSizeY(req, res);
    else if (method == "binx")
        handleBinX(req, res);
    else if (method == "biny")
        handleBinY(req, res);
    else if (method == "startx")
        handleStartX(req, res);
    else if (method == "starty")
        handleStartY(req, res);
    else if (method == "numx")
        handleNumX(req, res);
    else if (method == "numy")
        handleNumY(req, res);
    // Camera capabilities
    else if (method == "canabortexposure")
        handleCanAbortExposure(req, res);
    else if (method == "canstopexposure")
        handleCanStopExposure(req, res);
    else if (method == "canpulseguide")
        handleCanPulseGuide(req, res);
    else if (method == "cansetccdtemperature")
        handleCanSetCCDTemperature(req, res);
    else if (method == "hasshutter")
        handleHasShutter(req, res);
    // Temperature control
    else if (method == "ccdtemperature")
        handleCCDTemperature(req, res);
    else if (method == "cooleron")
        handleCoolerOn(req, res);
    else if (method == "coolerpower")
        handleCoolerPower(req, res);
    else if (method == "setccdtemperature")
        handleSetCCDTemperature(req, res);
    // Gain and offset
    else if (method == "gain")
        handleGain(req, res);
    else if (method == "gainmin")
        handleGainMin(req, res);
    else if (method == "gainmax")
        handleGainMax(req, res);
    else if (method == "gains")
        handleGains(req, res);
    else if (method == "offset")
        handleOffset(req, res);
    else if (method == "offsetmin")
        handleOffsetMin(req, res);
    else if (method == "offsetmax")
        handleOffsetMax(req, res);
    else if (method == "offsets")
        handleOffsets(req, res);
    // Readout modes and sensor info
    else if (method == "readoutmode")
        handleReadoutMode(req, res);
    else if (method == "readoutmodes")
        handleReadoutModes(req, res);
    else if (method == "sensortype")
        handleSensorType(req, res);
    else if (method == "bayeroffsetx")
        handleBayerOffsetX(req, res);
    else if (method == "bayeroffsety")
        handleBayerOffsetY(req, res);
    else if (method == "sensorname")
        handleSensorName(req, res);
    // Exposure control
    else if (method == "startexposure")
        handleStartExposure(req, res);
    else if (method == "stopexposure")
        handleStopExposure(req, res);
    else if (method == "abortexposure")
        handleAbortExposure(req, res);
    else if (method == "imageready")
        handleImageReady(req, res);
    else if (method == "camerastate")
        handleCameraState(req, res);
    else if (method == "percentcompleted")
        handlePercentCompleted(req, res);
    else if (method == "lastexposureduration")
        handleLastExposureDuration(req, res);
    else if (method == "lastexposurestarttime")
        handleLastExposureStartTime(req, res);
    else if (method == "exposuremin")
        handleExposureMin(req, res);
    else if (method == "exposuremax")
        handleExposureMax(req, res);
    // Image data
    else if (method == "imagearray")
        handleImageArray(req, res);
    else if (method == "imagearrayvariant")
        handleImageArrayVariant(req, res);
    // Guiding
    else if (method == "ispulseguiding")
        handleIsPulseGuiding(req, res);
    else if (method == "pulseguide")
        handlePulseGuide(req, res);
    // Additional properties
    else if (method == "maxadu")
        handleMaxADU(req, res);
    else if (method == "electronsperadu")
        handleElectronsPerADU(req, res);
    else if (method == "fullwellcapacity")
        handleFullWellCapacity(req, res);
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

void CameraBridge::updateProperty(INDI::Property property)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const char *propName = property.getName();
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updating property: %s", propName);

    // Update camera state based on property updates
    if (property.isNameMatch("CCD_INFO"))
    {
        // Update camera information
        INDI::PropertyNumber numberProperty(property);
        for (auto &num : numberProperty)
        {
            if (num.isNameMatch("CCD_MAX_X"))
                m_CameraXSize = static_cast<int>(num.getValue());
            else if (num.isNameMatch("CCD_MAX_Y"))
                m_CameraYSize = static_cast<int>(num.getValue());
            else if (num.isNameMatch("CCD_PIXEL_SIZE_X"))
                m_PixelSizeX = num.getValue();
            else if (num.isNameMatch("CCD_PIXEL_SIZE_Y"))
                m_PixelSizeY = num.getValue();
            else if (num.isNameMatch("CCD_BITSPERPIXEL"))
                m_BitsPerPixel = static_cast<int>(num.getValue());
        }
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG,
                     "Updated camera info: %dx%d, pixel size: %.2fx%.2f, BPP: %d",
                     m_CameraXSize, m_CameraYSize, m_PixelSizeX, m_PixelSizeY, m_BitsPerPixel);
    }
    else if (property.isNameMatch("CCD_BINNING"))
    {
        // Update binning
        INDI::PropertyNumber numberProperty(property);
        for (auto &num : numberProperty)
        {
            if (num.isNameMatch("HOR_BIN"))
                m_BinX = static_cast<int>(num.getValue());
            else if (num.isNameMatch("VER_BIN"))
                m_BinY = static_cast<int>(num.getValue());
        }
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated binning: %dx%d", m_BinX, m_BinY);
    }
    else if (property.isNameMatch("CCD_FRAME"))
    {
        // Update frame/ROI
        INDI::PropertyNumber numberProperty(property);
        for (auto &num : numberProperty)
        {
            if (num.isNameMatch("X"))
                m_StartX = static_cast<int>(num.getValue());
            else if (num.isNameMatch("Y"))
                m_StartY = static_cast<int>(num.getValue());
            else if (num.isNameMatch("WIDTH"))
                m_NumX = static_cast<int>(num.getValue());
            else if (num.isNameMatch("HEIGHT"))
                m_NumY = static_cast<int>(num.getValue());
        }
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG,
                     "Updated frame: (%d,%d) %dx%d", m_StartX, m_StartY, m_NumX, m_NumY);
    }
    else if (property.isNameMatch("CCD_CONTROLS"))
    {
        // Update gain and offset from CCD_CONTROLS property
        INDI::PropertyNumber numberProperty(property);
        for (auto &num : numberProperty)
        {
            if (num.isNameMatch("Gain"))
            {
                m_Gain = num.getValue();
                m_GainMin = num.getMin();
                m_GainMax = num.getMax();
                m_HasGain = true;
                m_UsesCCDControlsForGainOffset = true;
                DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG,
                             "Updated gain from CCD_CONTROLS: %.0f (Min: %.0f, Max: %.0f)",
                             m_Gain, m_GainMin, m_GainMax);
            }
            else if (num.isNameMatch("Offset"))
            {
                m_Offset = num.getValue();
                m_OffsetMin = num.getMin();
                m_OffsetMax = num.getMax();
                m_HasOffset = true;
                m_UsesCCDControlsForGainOffset = true;
                DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG,
                             "Updated offset from CCD_CONTROLS: %.0f (Min: %.0f, Max: %.0f)",
                             m_Offset, m_OffsetMin, m_OffsetMax);
            }
        }
    }
    else if (property.isNameMatch("CCD_TEMPERATURE"))
    {
        // Update temperature
        INDI::PropertyNumber numberProperty(property);
        m_CCDTemperature = numberProperty[0].getValue();
        m_CanSetCCDTemperature = (property.getPermission() != IP_RO);
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated temperature: %.2f C, CanSetCCDTemperature: %s",
                     m_CCDTemperature, m_CanSetCCDTemperature ? "true" : "false");
    }
    else if (property.isNameMatch("CCD_COOLER"))
    {
        // Update cooler state
        INDI::PropertySwitch switchProperty(property);
        m_CoolerOn = switchProperty[0].getState() == ISS_ON;
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated cooler state: %s", m_CoolerOn ? "ON" : "OFF");
    }
    else if (property.isNameMatch("CCD_COOLER_POWER"))
    {
        // Update cooler power
        INDI::PropertyNumber numberProperty(property);
        m_CoolerPower = numberProperty[0].getValue();
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated cooler power: %.1f%%", m_CoolerPower);
    }
    else if (property.isNameMatch("CCD_GAIN"))
    {
        // Update gain
        INDI::PropertyNumber numberProperty(property);
        m_Gain = numberProperty[0].getValue();
        m_GainMin = numberProperty[0].getMin();
        m_GainMax = numberProperty[0].getMax();
        m_HasGain = true;
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated gain: %.0f (Min: %.0f, Max: %.0f)",
                     m_Gain, m_GainMin, m_GainMax);
    }
    else if (property.isNameMatch("CCD_OFFSET"))
    {
        // Update offset
        INDI::PropertyNumber numberProperty(property);
        m_Offset = numberProperty[0].getValue();
        m_OffsetMin = numberProperty[0].getMin();
        m_OffsetMax = numberProperty[0].getMax();
        m_HasOffset = true;
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Updated offset: %.0f (Min: %.0f, Max: %.0f)",
                     m_Offset, m_OffsetMin, m_OffsetMax);
    }
    else if (property.isNameMatch("CCD_EXPOSURE"))
    {
        // Update exposure state
        INDI::PropertyNumber numberProperty(property);
        IPState state = numberProperty.getState();

        if (state == IPS_BUSY)
        {
            m_IsExposing = true;
            m_CameraState = 2; // Exposing
            m_LastExposureDuration = numberProperty[0].getValue();
            m_ExposureStartTime = std::chrono::steady_clock::now();
            m_ImageReady = false;
        }
        else if (state == IPS_OK)
        {
            m_IsExposing = false;
            m_CameraState = 0; // Idle
            m_PercentCompleted = 100.0;
        }
        else if (state == IPS_ALERT)
        {
            m_IsExposing = false;
            m_CameraState = 5; // Error
        }

        m_ExposureMin = numberProperty[0].getMin();
        m_ExposureMax = numberProperty[0].getMax();

        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG,
                     "Updated exposure state: %s, duration: %.3f, min: %.3f, max: %.3f",
                     m_IsExposing ? "EXPOSING" : "IDLE", m_LastExposureDuration, m_ExposureMin, m_ExposureMax);
    }
    else if (property.isNameMatch("CCD1"))
    {
        // Image data received
        INDI::PropertyBlob blobProperty(property);
        if (blobProperty.getState() == IPS_OK && blobProperty[0].getBlobLen() > 0)
        {
            // Extract image data from FITS
            const uint8_t* fitsData = static_cast<const uint8_t*>(blobProperty[0].getBlob());
            size_t fitsSize = blobProperty[0].getBlobLen();

            if (extractImageFromFITS(fitsData, fitsSize, m_LastImageData,
                                     m_LastImageWidth, m_LastImageHeight,
                                     m_LastImageBPP, m_LastImageNAxis))
            {
                m_ImageReady = true;
                m_CameraState = 0; // Idle
                DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG,
                             "Image ready: %dx%d, %d-bit, %d-axis",
                             m_LastImageWidth, m_LastImageHeight, m_LastImageBPP, m_LastImageNAxis);
            }
            else
            {
                DEBUGDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to extract image from FITS");
                m_CameraState = 5; // Error
            }
        }
    }
    else if (property.isNameMatch("CCD_CFA"))
    {
        // Update Bayer info
        INDI::PropertyText textProperty(property);
        for (auto &text : textProperty)
        {
            if (text.isNameMatch("CFA_OFFSET_X"))
                m_BayerOffsetX = atoi(text.getText());
            else if (text.isNameMatch("CFA_OFFSET_Y"))
                m_BayerOffsetY = atoi(text.getText());
            else if (text.isNameMatch("CFA_TYPE"))
            {
                std::string cfaType = text.getText();
                if (cfaType == "RGGB")
                    m_SensorType = 2;
                else if (cfaType == "BGGR")
                    m_SensorType = 2;
                else if (cfaType == "GRBG")
                    m_SensorType = 2;
                else if (cfaType == "GBRG")
                    m_SensorType = 2;
                else if (cfaType == "CMYG")
                    m_SensorType = 3;
                else
                    m_SensorType = 0; // Monochrome
            }
        }
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG,
                     "Updated Bayer info: type=%d, offset=(%d,%d)",
                     m_SensorType, m_BayerOffsetX, m_BayerOffsetY);
    }
}

// Helper methods for sending commands to INDI server
void CameraBridge::requestNewNumber(const INDI::PropertyNumber &numberProperty)
{
    // Forward the request to the DeviceManager
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Request to send new number property: %s",
                 numberProperty.getName());

    DeviceManager::getInstance()->sendNewNumber(numberProperty);
}

void CameraBridge::requestNewSwitch(const INDI::PropertySwitch &switchProperty)
{
    // Forward the request to the DeviceManager
    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG, "Request to send new switch property: %s",
                 switchProperty.getName());

    DeviceManager::getInstance()->sendNewSwitch(switchProperty);
}

// Helper methods for sending standard JSON responses
void CameraBridge::sendResponse(httplib::Response &res, bool success, const std::string &errorMessage)
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
void CameraBridge::sendResponse(httplib::Response &res, const T &value, bool success,
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
void CameraBridge::sendResponseValue(httplib::Response &res, const T &value,
                                     bool success, const std::string &errorMessage)
{
    sendResponse(res, value, success, errorMessage, 0, 0);
}

// Implementation of sendResponseStatus
void CameraBridge::sendResponseStatus(httplib::Response &res, bool success,
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
template void CameraBridge::sendResponse<std::string>(httplib::Response &res, const std::string &value, bool success,
        const std::string &errorMessage, int clientID, int serverID);
template void CameraBridge::sendResponse<double>(httplib::Response &res, const double &value, bool success,
        const std::string &errorMessage, int clientID, int serverID);
template void CameraBridge::sendResponse<int>(httplib::Response &res, const int &value, bool success,
        const std::string &errorMessage, int clientID, int serverID);
template void CameraBridge::sendResponse<bool>(httplib::Response &res, const bool &value, bool success,
        const std::string &errorMessage, int clientID, int serverID);
template void CameraBridge::sendResponse<json>(httplib::Response &res, const json &value, bool success,
        const std::string &errorMessage, int clientID, int serverID);

template void CameraBridge::sendResponseValue<std::string>(httplib::Response &res,
        const std::string &value, bool success, const std::string &errorMessage);
template void CameraBridge::sendResponseValue<double>(httplib::Response &res,
        const double &value, bool success, const std::string &errorMessage);
template void CameraBridge::sendResponseValue<int>(httplib::Response &res,
        const int &value, bool success, const std::string &errorMessage);
template void CameraBridge::sendResponseValue<bool>(httplib::Response &res,
        const bool &value, bool success, const std::string &errorMessage);
template void CameraBridge::sendResponseValue<json>(httplib::Response &res,
        const json &value, bool success, const std::string &errorMessage);

// Image processing helper functions
bool CameraBridge::extractImageFromFITS(const uint8_t* fitsData, size_t fitsSize,
                                        std::vector<uint8_t> &rawData, int &width, int &height,
                                        int &bitsPerPixel, int &naxis)
{
    if (!fitsData || fitsSize == 0)
        return false;

    fitsfile *fptr = nullptr;
    int status = 0;
    long naxes[3] = {0, 0, 0};
    int bitpix = 0;

    // Open FITS file from memory
    void *memptr = (void *)fitsData;
    if (fits_open_memfile(&fptr, "", READONLY, &memptr, &fitsSize, 0, nullptr, &status))
    {
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to open FITS from memory: %d", status);
        return false;
    }

    // Get image parameters
    if (fits_get_img_param(fptr, 3, &bitpix, &naxis, naxes, &status))
    {
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to get FITS image parameters: %d", status);
        fits_close_file(fptr, &status);
        return false;
    }

    width = static_cast<int>(naxes[0]);
    height = static_cast<int>(naxes[1]);
    bitsPerPixel = abs(bitpix);

    // Calculate total pixels
    long totalPixels = naxes[0] * naxes[1];
    if (naxis == 3)
        totalPixels *= naxes[2];

    // Allocate buffer for raw data
    size_t bytesPerPixel = bitsPerPixel / 8;
    rawData.resize(totalPixels * bytesPerPixel);

    // Read image data
    long firstPixel = 1;
    int datatype = 0;

    switch (bitsPerPixel)
    {
        case 8:
            datatype = TBYTE;
            break;
        case 16:
            datatype = TUSHORT;
            break;
        case 32:
            datatype = TULONG;
            break;
        default:
            DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_ERROR, "Unsupported bits per pixel: %d", bitsPerPixel);
            fits_close_file(fptr, &status);
            return false;
    }

    if (fits_read_img(fptr, datatype, firstPixel, totalPixels, nullptr, rawData.data(), nullptr, &status))
    {
        DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_ERROR, "Failed to read FITS image data: %d", status);
        fits_close_file(fptr, &status);
        return false;
    }

    fits_close_file(fptr, &status);

    // Convert coordinate system from FITS (bottom-left origin) to Alpaca (top-left origin)
    convertCoordinateSystem(rawData, width, height, bytesPerPixel);

    DEBUGFDEVICE(m_Device.getDeviceName(), INDI::Logger::DBG_DEBUG,
                 "Extracted FITS image: %dx%d, %d-bit, %d-axis, %zu bytes",
                 width, height, bitsPerPixel, naxis, rawData.size());

    return true;
}

void CameraBridge::convertCoordinateSystem(std::vector<uint8_t> &imageData, int width, int height, int bytesPerPixel)
{
    // FITS uses bottom-left origin, Alpaca uses top-left origin
    // We need to flip the image vertically

    size_t rowSize = width * bytesPerPixel;
    std::vector<uint8_t> tempRow(rowSize);

    for (int y = 0; y < height / 2; y++)
    {
        uint8_t* topRow = imageData.data() + (y * rowSize);
        uint8_t* bottomRow = imageData.data() + ((height - 1 - y) * rowSize);

        // Swap rows
        std::memcpy(tempRow.data(), topRow, rowSize);
        std::memcpy(topRow, bottomRow, rowSize);
        std::memcpy(bottomRow, tempRow.data(), rowSize);
    }
}

void CameraBridge::formatImageAsJSON(const std::vector<uint8_t> &rawData, int width, int height,
                                     int bitsPerPixel, int naxis, nlohmann::json& response)
{
    // Create JSON array structure according to ASCOM Alpaca specification
    // For 2D arrays: array[x][y] where x is width index, y is height index
    // For 3D arrays: array[x][y][plane] where plane is color component

    json imageArray = json::array();

    if (naxis == 2)
    {
        // 2D monochrome image
        for (int x = 0; x < width; x++)
        {
            json column = json::array();
            for (int y = 0; y < height; y++)
            {
                int pixelIndex = (y * width + x);
                int32_t pixelValue = 0;

                switch (bitsPerPixel)
                {
                    case 8:
                        pixelValue = static_cast<int32_t>(rawData[pixelIndex]);
                        break;
                    case 16:
                        pixelValue = static_cast<int32_t>(reinterpret_cast<const uint16_t*>(rawData.data())[pixelIndex]);
                        break;
                    case 32:
                        pixelValue = static_cast<int32_t>(reinterpret_cast<const uint32_t*>(rawData.data())[pixelIndex]);
                        break;
                }

                column.push_back(pixelValue);
            }
            imageArray.push_back(column);
        }
    }
    else if (naxis == 3)
    {
        // 3D color image - assume RGB with 3 planes
        int planes = 3;
        for (int x = 0; x < width; x++)
        {
            json column = json::array();
            for (int y = 0; y < height; y++)
            {
                json pixel = json::array();
                for (int p = 0; p < planes; p++)
                {
                    int pixelIndex = (p * width * height) + (y * width + x);
                    int32_t pixelValue = 0;

                    switch (bitsPerPixel)
                    {
                        case 8:
                            pixelValue = static_cast<int32_t>(rawData[pixelIndex]);
                            break;
                        case 16:
                            pixelValue = static_cast<int32_t>(reinterpret_cast<const uint16_t*>(rawData.data())[pixelIndex]);
                            break;
                        case 32:
                            pixelValue = static_cast<int32_t>(reinterpret_cast<const uint32_t*>(rawData.data())[pixelIndex]);
                            break;
                    }

                    pixel.push_back(pixelValue);
                }
                column.push_back(pixel);
            }
            imageArray.push_back(column);
        }
    }

    response = imageArray;
}
