/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  ASCOM Alpaca Camera INDI Driver

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

#include "indiccd.h"
#include <string>
#include <functional>
#include <vector>
#include <limits>
#include <httplib.h>
#include <indielapsedtimer.h>
#include <indisinglethreadpool.h>
#include <inditimer.h>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

typedef enum ALPACA_GUIDE_DIRECTION  //Guider Direction
{
    ALPACA_GUIDE_NORTH = 0,
    ALPACA_GUIDE_SOUTH,
    ALPACA_GUIDE_EAST,
    ALPACA_GUIDE_WEST
} ALPACA_GUIDE_DIRECTION;

class AlpacaCCD : public INDI::CCD
{
    public:
        AlpacaCCD();
        virtual ~AlpacaCCD() = default;

        virtual bool initProperties() override;
        const char *getDefaultName() override;
        void ISGetProperties(const char *dev) override;
        bool updateProperties() override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;

    protected:
        bool Connect() override;
        bool Disconnect() override;

        void TimerHit() override;

        virtual bool StartExposure(float duration) override;
        virtual bool AbortExposure() override;
        virtual int SetTemperature(double temperature) override;
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int hor, int ver) override;
        virtual bool SetCaptureFormat(uint8_t index) override;
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

    private:
        // Internal state variables
        bool m_ExposureInProgress {false};
        std::string m_CurrentReadoutModeName {"0"}; // Default to mode 0
        int m_CurrentReadoutModeIndex {0};
        uint32_t m_MaxADU { 0 };
        uint8_t m_SensorType { 0 };
        int m_CameraXSize {1}, m_CameraYSize {1};
        double m_PixelSizeX {1.0}, m_PixelSizeY {1.0};
        std::string m_Description, m_DriverInfo, m_DriverVersion, m_CameraName;
        double m_GainMin {0}, m_GainMax {0};
        uint8_t m_BayerOffsetX {0}, m_BayerOffsetY {0};

        // Alpaca Communication
        std::unique_ptr<httplib::Client> httpClient; // Declared here
        bool sendAlpacaGET(const std::string &endpoint, nlohmann::json &response);
        bool sendAlpacaPUT(const std::string &endpoint, const nlohmann::json &request, nlohmann::json &response);
        std::string getAlpacaURL(const std::string &endpoint);
        uint32_t getTransactionId()
        {
            return ++m_ClientTransactionID;
        }
        uint32_t m_ClientTransactionID {1};

        // ImageBytes metadata structure (44 bytes) as per ASCOM Alpaca API v10 section 8.7.1
        struct ImageBytesMetadata
        {
            int32_t MetadataVersion;        // Bytes 0-3: Should be 1
            int32_t ErrorNumber;            // Bytes 4-7: 0 for success
            uint32_t ClientTransactionID;   // Bytes 8-11
            uint32_t ServerTransactionID;   // Bytes 12-15
            int32_t DataStart;              // Bytes 16-19: Offset to image data
            int32_t ImageElementType;       // Bytes 20-23: Source array element type
            int32_t TransmissionElementType;// Bytes 24-27: Network transmission type
            int32_t Rank;                   // Bytes 28-31: 2 or 3 dimensions
            int32_t Dimension1;             // Bytes 32-35: Width
            int32_t Dimension2;             // Bytes 36-39: Height
            int32_t Dimension3;             // Bytes 40-43: Planes (0 for 2D)
        } __attribute__((packed));

        // Extended image metadata for FITS headers
        struct ImageMetadata
        {
            uint8_t type;           // ASCOM type code
            uint8_t rank;           // Number of dimensions
            uint32_t width;         // Pixel width
            uint32_t height;        // Pixel height
            uint32_t planes;        // Color planes (0=mono)
            uint32_t max_adu;       // Maximum pixel value
            uint8_t sensor_type;    // Mono/Color/Bayer type
            uint8_t bayer_offset_x; // Bayer pattern X offset
            uint8_t bayer_offset_y; // Bayer pattern Y offset
        };

        ImageMetadata m_CurrentImage;
        bool alpacaGetImageReady(); // Declared here
        bool alpacaGetImageArrayImageBytes(uint8_t** buffer, size_t* buffer_size, ImageBytesMetadata* metadata);
        bool alpacaGetImageArrayJSON(ImageMetadata &meta, uint8_t** buffer, size_t* buffer_size);
        bool downloadImage();
        bool processImageBytesData(uint8_t* buffer, size_t buffer_size, const ImageBytesMetadata &metadata);
        bool processMonoImage(uint8_t* buffer);
        bool processColorImage(uint8_t* buffer); // Placeholder for color image processing

        // ImageBytes data conversion helpers
        void convertImageBytesToINDI2D(uint8_t* src_buffer, uint16_t* dst_buffer, uint32_t width, uint32_t height,
                                       int32_t transmission_type);
        void convertImageBytesToINDI3D(uint8_t* src_buffer, uint16_t* dst_buffer, uint32_t width, uint32_t height, uint32_t planes,
                                       int32_t transmission_type);

        // Data Translation
        void translateCoordinates(uint8_t* buffer, const ImageMetadata &meta);
        virtual void addFITSKeywords(INDI::CCDChip * targetChip, std::vector<INDI::FITSRecord> &fitsKeywords) override;

        // Worker thread for exposure handling
        INDI::SingleThreadPool mWorker;
        void workerExposure(const std::atomic_bool &isAboutToQuit, float duration);

        void updateStatus();
        void updateCoolerStatus();
        void updateCameraState();
        void updateReadoutModes();
        void updateCameraCapabilities();

        bool sendPulseGuide(ALPACA_GUIDE_DIRECTION direction, long duration);
        std::string getSensorTypeString(uint8_t sensorType);

        INDI::PropertyText ServerAddressTP {2};  // Host and port
        INDI::PropertyNumber DeviceNumberNP {1};  // Alpaca device number
        INDI::PropertyNumber ConnectionSettingsNP {3}; // Timeout, Retries, Retry Delay

        INDI::PropertyNumber GainNP {1}; // CCD_GAIN
        INDI::PropertyNumber OffsetNP {1}; // CCD_OFFSET
        INDI::PropertyNumber CoolerPowerNP {1}; // Cooler power percentage

        INDI::PropertyText DeviceInfoTP {4}; // Description, DriverInfo, DriverVersion, Name

        INDI::PropertySwitch CoolerSP {2}; // Declared here
        INDI::PropertyText CameraStateTP {1}; // Declared here
        INDI::PropertySwitch ReadoutModeSP {0}; // Readout mode selection

        bool m_HasGain {false}; // Flag to track if device supports Gain
        bool m_HasOffset {false}; // Flag to track if device supports Offset
        bool m_CanPulseGuide {false}; // Flag to track if device supports pulse guiding
        bool m_CanStopExposure {false}; // Flag to track if device supports stopping exposure

        // Temperature and cooler monitoring
        INDI::Timer mTimerTemperature;
        void temperatureTimerTimeout();
        double m_CurrentTemperature {0};
        double m_TargetTemperature {std::numeric_limits<double>::quiet_NaN()};

        static const constexpr char* CONNECTION_TAB = "Connection";
        static constexpr int TEMP_TIMER_MS = 10000;
        // Differential temperature threshold (C)
        static constexpr double TEMP_THRESHOLD = 0.25;
};
