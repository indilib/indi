/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI Alpaca Camera Bridge

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
#include <vector>
#include <chrono>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

class CameraBridge : public IDeviceBridge
{
    public:
        CameraBridge(INDI::BaseDevice device, int deviceNumber);
        ~CameraBridge();

        // IDeviceBridge implementation
        std::string getDeviceType() const override
        {
            return "camera";
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
        // Camera-specific Alpaca API methods - Information
        void handleCameraXSize(const httplib::Request &req, httplib::Response &res);
        void handleCameraYSize(const httplib::Request &req, httplib::Response &res);
        void handleMaxBinX(const httplib::Request &req, httplib::Response &res);
        void handleMaxBinY(const httplib::Request &req, httplib::Response &res);
        void handleCanAsymmetricBin(const httplib::Request &req, httplib::Response &res);
        void handlePixelSizeX(const httplib::Request &req, httplib::Response &res);
        void handlePixelSizeY(const httplib::Request &req, httplib::Response &res);
        void handleBinX(const httplib::Request &req, httplib::Response &res);
        void handleBinY(const httplib::Request &req, httplib::Response &res);
        void handleStartX(const httplib::Request &req, httplib::Response &res);
        void handleStartY(const httplib::Request &req, httplib::Response &res);
        void handleNumX(const httplib::Request &req, httplib::Response &res);
        void handleNumY(const httplib::Request &req, httplib::Response &res);

        // Camera-specific Alpaca API methods - Capabilities
        void handleCanAbortExposure(const httplib::Request &req, httplib::Response &res);
        void handleCanStopExposure(const httplib::Request &req, httplib::Response &res);
        void handleCanPulseGuide(const httplib::Request &req, httplib::Response &res);
        void handleCanSetCCDTemperature(const httplib::Request &req, httplib::Response &res);
        void handleHasShutter(const httplib::Request &req, httplib::Response &res);

        // Camera-specific Alpaca API methods - Temperature
        void handleCCDTemperature(const httplib::Request &req, httplib::Response &res);
        void handleCoolerOn(const httplib::Request &req, httplib::Response &res);
        void handleCoolerPower(const httplib::Request &req, httplib::Response &res);
        void handleSetCCDTemperature(const httplib::Request &req, httplib::Response &res);

        // Camera-specific Alpaca API methods - Gain/Offset
        void handleGain(const httplib::Request &req, httplib::Response &res);
        void handleGainMin(const httplib::Request &req, httplib::Response &res);
        void handleGainMax(const httplib::Request &req, httplib::Response &res);
        void handleGains(const httplib::Request &req, httplib::Response &res);
        void handleOffset(const httplib::Request &req, httplib::Response &res);
        void handleOffsetMin(const httplib::Request &req, httplib::Response &res);
        void handleOffsetMax(const httplib::Request &req, httplib::Response &res);
        void handleOffsets(const httplib::Request &req, httplib::Response &res);

        // Camera-specific Alpaca API methods - Readout
        void handleReadoutMode(const httplib::Request &req, httplib::Response &res);
        void handleReadoutModes(const httplib::Request &req, httplib::Response &res);
        void handleSensorType(const httplib::Request &req, httplib::Response &res);
        void handleBayerOffsetX(const httplib::Request &req, httplib::Response &res);
        void handleBayerOffsetY(const httplib::Request &req, httplib::Response &res);
        void handleSensorName(const httplib::Request &req, httplib::Response &res);

        // Camera-specific Alpaca API methods - Exposure
        void handleStartExposure(const httplib::Request &req, httplib::Response &res);
        void handleStopExposure(const httplib::Request &req, httplib::Response &res);
        void handleAbortExposure(const httplib::Request &req, httplib::Response &res);
        void handleImageReady(const httplib::Request &req, httplib::Response &res);
        void handleCameraState(const httplib::Request &req, httplib::Response &res);
        void handlePercentCompleted(const httplib::Request &req, httplib::Response &res);
        void handleLastExposureDuration(const httplib::Request &req, httplib::Response &res);
        void handleLastExposureStartTime(const httplib::Request &req, httplib::Response &res);
        void handleExposureMin(const httplib::Request &req, httplib::Response &res);
        void handleExposureMax(const httplib::Request &req, httplib::Response &res);

        // Camera-specific Alpaca API methods - Image Data
        void handleImageArray(const httplib::Request &req, httplib::Response &res);
        void handleImageArrayVariant(const httplib::Request &req, httplib::Response &res);

        // Camera-specific Alpaca API methods - Guiding
        void handleIsPulseGuiding(const httplib::Request &req, httplib::Response &res);
        void handlePulseGuide(const httplib::Request &req, httplib::Response &res);

        // Camera-specific Alpaca API methods - Additional Properties
        void handleMaxADU(const httplib::Request &req, httplib::Response &res);
        void handleElectronsPerADU(const httplib::Request &req, httplib::Response &res);
        void handleFullWellCapacity(const httplib::Request &req, httplib::Response &res);

        // Helper methods for sending commands to INDI server
        void requestNewNumber(const INDI::PropertyNumber &numberProperty);
        void requestNewSwitch(const INDI::PropertySwitch &switchProperty);

        // Helper methods for sending standard JSON responses
        void sendResponse(httplib::Response &res, bool success, const std::string &errorMessage);

        // Generic method for sending a response with a value of any type
        template <typename T>
        void sendResponse(httplib::Response &res, const T &value, bool success = true,
                          const std::string &errorMessage = "", int clientID = 0, int serverID = 0);

        // Simplified response methods without transaction IDs
        template <typename T>
        void sendResponseValue(httplib::Response &res, const T &value,
                               bool success = true, const std::string &errorMessage = "");

        void sendResponseStatus(httplib::Response &res, bool success,
                                const std::string &errorMessage = "");

        // Image processing helpers
        bool extractImageFromFITS(const uint8_t* fitsData, size_t fitsSize,
                                  std::vector<uint8_t> &rawData, int &width, int &height,
                                  int &bitsPerPixel, int &naxis);
        void convertCoordinateSystem(std::vector<uint8_t> &imageData, int width, int height, int bytesPerPixel);
        void formatImageAsJSON(const std::vector<uint8_t> &rawData, int width, int height,
                               int bitsPerPixel, int naxis, nlohmann::json& response);

        // Device state
        INDI::BaseDevice m_Device;
        int m_DeviceNumber;

        // Current state tracking - Camera Information
        int m_CameraXSize {0};
        int m_CameraYSize {0};
        double m_PixelSizeX {0.0};
        double m_PixelSizeY {0.0};
        int m_MaxBinX {1};
        int m_MaxBinY {1};
        int m_BitsPerPixel {16};

        // Current state tracking - Camera Settings
        int m_BinX {1};
        int m_BinY {1};
        int m_StartX {0};
        int m_StartY {0};
        int m_NumX {0};
        int m_NumY {0};

        // Current state tracking - Capabilities
        bool m_CanAbortExposure {true};
        bool m_CanStopExposure {false};
        bool m_CanPulseGuide {false};
        bool m_CanSetCCDTemperature {false};
        bool m_HasShutter {false};
        bool m_CanAsymmetricBin {false};

        // Current state tracking - Temperature
        double m_CCDTemperature {0.0};
        bool m_CoolerOn {false};
        double m_CoolerPower {0.0};

        // Current state tracking - Gain/Offset
        bool m_HasGain {false};
        bool m_HasOffset {false};
        double m_Gain {0.0};
        double m_GainMin {0.0};
        double m_GainMax {1000.0};
        double m_Offset {0.0};
        double m_OffsetMin {0.0};
        double m_OffsetMax {10000.0};
        std::vector<std::string> m_Gains;
        std::vector<std::string> m_Offsets;

        // Current state tracking - Readout
        int m_ReadoutMode {0};
        std::vector<std::string> m_ReadoutModes;
        int m_SensorType {0}; // 0=Monochrome, 1=Color, 2=RGGB, 3=CMYG, etc.
        int m_BayerOffsetX {0};
        int m_BayerOffsetY {0};

        // Current state tracking - Exposure
        bool m_IsExposing {false};
        bool m_ImageReady {false};
        int m_CameraState {0}; // 0=Idle, 1=Waiting, 2=Exposing, 3=Reading, 4=Download, 5=Error
        double m_PercentCompleted {0.0};
        double m_LastExposureDuration {0.0};
        std::string m_LastExposureStartTime;
        std::chrono::steady_clock::time_point m_ExposureStartTime;
        double m_ExposureMin {0.0};
        double m_ExposureMax {10000.0};

        // Current state tracking - Image Data
        std::vector<uint8_t> m_LastImageData;
        int m_LastImageWidth {0};
        int m_LastImageHeight {0};
        int m_LastImageBPP {16};
        int m_LastImageNAxis {2};

        // Current state tracking - Guiding
        bool m_IsPulseGuiding {false};

        // Current state tracking - Additional Properties
        int m_MaxADU {65535};
        double m_ElectronsPerADU {1.0};
        double m_FullWellCapacity {100000.0};

        // Flag to indicate if CCD_CONTROLS property is used for Gain/Offset
        bool m_UsesCCDControlsForGainOffset {false};

        // Thread safety
        std::mutex m_Mutex;
};
