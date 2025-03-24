/*******************************************************************************
  Copyright(c) 2025 Jérémie Klein. All rights reserved.

  ASCOM Alpaca Weather Safety INDI Driver

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

#include "indiweather.h"
#include <string>
#include <functional>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

class WeatherSafetyAlpaca : public INDI::Weather
{
    public:
        WeatherSafetyAlpaca();
        virtual ~WeatherSafetyAlpaca() = default;

        virtual bool initProperties() override;
        const char *getDefaultName() override;
        void ISGetProperties(const char *dev) override;
        virtual bool updateProperties() override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual bool loadConfig(bool silent = false, const char *property = nullptr) override;

    protected:
        bool Connect() override;
        bool Disconnect() override;
        virtual IPState updateWeather() override;

    private:
        // HTTP helper methods
        bool makeAlpacaRequest(const std::string& path, nlohmann::json& response, bool isPut = false);
        bool retryRequest(const std::function<bool()>& request);

        // Properties
        INDI::PropertyText ServerAddressTP {2};  // Host and port
        INDI::PropertyNumber DeviceNumberNP {1};  // Alpaca device number
        INDI::PropertyNumber ConnectionSettingsNP {3};  // Timeout, retries, retry delay

        // State variables
        bool LastParseSuccess {false};
}; 