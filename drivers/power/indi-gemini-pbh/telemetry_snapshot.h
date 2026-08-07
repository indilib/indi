/*
    Gemini Power Box Hub Advanced v3 INDI Driver

    Copyright (C) 2026 Dieter R Kedrowitsch <dieter@kedrowitsch.net>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA  02111-1307, USA.

    The full GNU General Public License is included in this distribution in the
    file called LICENSE.
*/

#pragma once

#include <array>
#include <chrono>
#include <cstddef>

namespace geminipbh
{

enum class HeaterMode { Auto, ManualPwm, BinarySwitch };

struct TelemetrySnapshot
{
    static constexpr std::size_t DcOutputCount = 4;
    static constexpr std::size_t UsbOutputCount = 6;
    static constexpr std::size_t HeaterCount = 2;

    const std::array<bool, DcOutputCount> dcOutputs;
    const std::array<bool, UsbOutputCount> usbOutputs;
    const bool aht20Attached;
    const bool ds18b20Attached;
    const std::array<bool, HeaterCount> heaterEnabled;
    const std::array<HeaterMode, HeaterCount> heaterModes;
    const std::array<double, HeaterCount> heaterOutputPercent;
    const double deviceSurfaceTemperatureC;
    const double airTemperatureC;
    const double humidityPercent;
    const double dewPointC;
    const double inputVoltageV;
    const double outputCurrentA;
    const double outputPowerW;
    const unsigned long long sequence;
    const std::chrono::steady_clock::time_point timestamp;

    TelemetrySnapshot(std::array<bool, DcOutputCount> dcOutputs,
                      std::array<bool, UsbOutputCount> usbOutputs,
                      bool aht20Attached,
                      bool ds18b20Attached,
                      std::array<bool, HeaterCount> heaterEnabled,
                      std::array<HeaterMode, HeaterCount> heaterModes,
                      std::array<double, HeaterCount> heaterOutputPercent,
                      double deviceSurfaceTemperatureC,
                      double airTemperatureC,
                      double humidityPercent,
                      double dewPointC,
                      double inputVoltageV,
                      double outputCurrentA,
                      double outputPowerW,
                      unsigned long long sequence,
                      std::chrono::steady_clock::time_point timestamp)
        : dcOutputs(dcOutputs),
          usbOutputs(usbOutputs),
          aht20Attached(aht20Attached),
          ds18b20Attached(ds18b20Attached),
          heaterEnabled(heaterEnabled),
          heaterModes(heaterModes),
          heaterOutputPercent(heaterOutputPercent),
          deviceSurfaceTemperatureC(deviceSurfaceTemperatureC),
          airTemperatureC(airTemperatureC),
          humidityPercent(humidityPercent),
          dewPointC(dewPointC),
          inputVoltageV(inputVoltageV),
          outputCurrentA(outputCurrentA),
          outputPowerW(outputPowerW),
          sequence(sequence),
          timestamp(timestamp)
    {
    }
};

} // namespace geminipbh
