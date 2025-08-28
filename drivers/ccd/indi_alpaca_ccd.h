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

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

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
        virtual bool loadConfig(bool silent = false, const char *property = nullptr) override;

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

    private:
        void updateStatus();
        void updateExposureProgress();
        void updateCoolerStatus();
        void updateImageReady();
        void updateCameraState();
        void updateReadoutModes();

        INDI::PropertyText ServerAddressTP {2};  // Host and port
        INDI::PropertyNumber DeviceNumberNP {1};  // Alpaca device number
        INDI::PropertyNumber ConnectionSettingsNP {3}; // Timeout, Retries, Retry Delay

        INDI::PropertyNumber GainNP {1}; // CCD_GAIN
        INDI::PropertyNumber OffsetNP {1}; // CCD_OFFSET

        // HTTP helper methods
        bool makeAlpacaRequest(const std::string& path, nlohmann::json& response, bool isPut = false,
                               const nlohmann::json* body = nullptr);
        bool retryRequest(const std::function<bool()> &request);

        // Internal state variables
        float m_ExposureDuration {0};
        bool m_ExposureInProgress {false};
        std::string m_CurrentReadoutModeName {"0"}; // Default to mode 0
        int m_CurrentReadoutModeIndex {0};

        INDI::PropertyText CameraStateTP {1}; // Camera state (e.g., Idle, Exposing, Reading)

        static const constexpr char* CAMERA_CONTROL_TAB = "Camera Control";
        static const constexpr char* CONNECTION_TAB = "Connection";
};
