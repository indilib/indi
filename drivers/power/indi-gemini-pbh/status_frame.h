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
#include <string>

namespace geminipbh
{

enum class DewMode { Automatic, Manual, Switch };

struct StatusFrame
{
    std::array<bool, 4> dc{};
    std::array<bool, 6> usb{};
    bool aht20Attached = false;
    bool ds18b20Attached = false;
    bool dew6Enabled = false;
    int dew6Mode = 0;
    bool dew7Enabled = false;
    int dew7Mode = 0;
    double dew6OutputPercent = 0.0;
    double dew7OutputPercent = 0.0;
    double deviceTemperatureC = 0.0;
    double airTemperatureC = 0.0;
    double humidityPercent = 0.0;
    double dewPointC = 0.0;
    double inputVoltageV = 0.0;
    double outputCurrentA = 0.0;
    double outputPowerW = 0.0;
};

} // namespace geminipbh
