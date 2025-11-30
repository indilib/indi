/*******************************************************************************
  Copyright(c) 2025 Jasem Mutlaq. All rights reserved.

  INDI HiTech Weather Station Driver

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

#include "hitechweather.h"

#include <memory>
#include <cstring>
#include <cmath>
#include <algorithm>

// We declare an auto pointer to HitechWeather.
std::unique_ptr<HitechWeather> hitechWeather(new HitechWeather());

HitechWeather::HitechWeather()
{
    setVersion(1, 0);
    setWeatherConnection(CONNECTION_NONE);
}

HitechWeather::~HitechWeather()
{
    if (m_hidHandle)
    {
        hid_close(m_hidHandle);
        m_hidHandle = nullptr;
    }
}

const char *HitechWeather::getDefaultName()
{
    return "HiTech Weather";
}

bool HitechWeather::Connect()
{
    // Initialize HID library
    if (hid_init() != 0)
    {
        LOG_ERROR("Failed to initialize HID library");
        return false;
    }

    // Open HiTech Weather device
    m_hidHandle = hid_open(HITECH_VID, HITECH_PID, nullptr);
    if (!m_hidHandle)
    {
        LOGF_ERROR("Unable to open HiTech Weather device (VID: 0x%04X, PID: 0x%04X). "
                   "Please check device is connected and permissions are correct.",
                   HITECH_VID, HITECH_PID);
        hid_exit();
        return false;
    }

    // Get device information
    wchar_t wstr[256];
    if (hid_get_manufacturer_string(m_hidHandle, wstr, 256) == 0)
    {
        char manufacturer[256];
        wcstombs(manufacturer, wstr, 256);
        LOGF_INFO("Manufacturer: %s", manufacturer);
    }

    if (hid_get_product_string(m_hidHandle, wstr, 256) == 0)
    {
        char product[256];
        wcstombs(product, wstr, 256);
        LOGF_INFO("Product: %s", product);
    }

    LOG_INFO("HiTech Weather connected successfully");
    return true;
}

bool HitechWeather::Disconnect()
{
    if (m_hidHandle)
    {
        hid_close(m_hidHandle);
        m_hidHandle = nullptr;
    }
    hid_exit();

    LOG_INFO("HiTech Weather disconnected");
    return true;
}

bool HitechWeather::initProperties()
{
    INDI::Weather::initProperties();

    // Ambient temperature (standard INDI parameter)
    addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -30, 50, 15);

    // Sky temperature (informational)
    addParameter("WEATHER_SKY_TEMPERATURE", "Sky Temp (C)", -50, 30, 15);

    // Cloud cover (critical parameter)
    // 0-15% = OK (clear), 15-30% = WARNING (some clouds), 30-100% = ALERT (cloudy)
    addParameter("WEATHER_CLOUD_COVER", "Clouds (%)", 0, 100, 15);
    setCriticalParameter("WEATHER_CLOUD_COVER");

    addDebugControl();
    return true;
}

IPState HitechWeather::updateWeather()
{
    double skyTemp = 0;
    double ambientTemp = 0;

    // Read sky temperature
    if (!getSkyTemperature(skyTemp))
    {
        LOG_ERROR("Failed to read sky temperature");
        return IPS_ALERT;
    }

    // Read ambient temperature
    if (!getAmbientTemperature(ambientTemp))
    {
        LOG_ERROR("Failed to read ambient temperature");
        return IPS_ALERT;
    }

    // Calculate cloud cover from temperature differential
    double cloudCover = calculateCloudCover(ambientTemp, skyTemp);

    // Update parameters
    setParameterValue("WEATHER_TEMPERATURE", ambientTemp);
    setParameterValue("WEATHER_SKY_TEMPERATURE", skyTemp);
    setParameterValue("WEATHER_CLOUD_COVER", cloudCover);

    LOGF_DEBUG("Sky: %.2f°C, Ambient: %.2f°C, Cloud Cover: %.1f%%", 
               skyTemp, ambientTemp, cloudCover);

    return IPS_OK;
}

bool HitechWeather::getSkyTemperature(double &skyTemp)
{
    if (!m_hidHandle)
        return false;

    // Prepare command: getSkyTemp
    unsigned char cmd[8] = {CMD_GET_SKY_TEMP, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    // Send command
    int res = hid_write(m_hidHandle, cmd, sizeof(cmd));
    if (res < 0)
    {
        LOGF_ERROR("Failed to write getSkyTemp command: %ls", hid_error(m_hidHandle));
        return false;
    }

    // Read response
    unsigned char buf[8];
    res = hid_read_timeout(m_hidHandle, buf, sizeof(buf), 1000);
    if (res < 0)
    {
        LOGF_ERROR("Failed to read getSkyTemp response: %ls", hid_error(m_hidHandle));
        return false;
    }

    if (res < 3)
    {
        LOGF_ERROR("Insufficient data received for getSkyTemp: %d bytes", res);
        return false;
    }

    // Verify response signature (byte 3 should be 0x21)
    if (buf[2] != 0x21)
    {
        LOGF_ERROR("Invalid getSkyTemp response signature: 0x%02X", buf[2]);
        return false;
    }

    // Parse temperature
    // Formula: skytemp = byte1 * 256 + byte2; skytemp = skytemp * 0.02 - 273.7
    uint16_t rawTemp = (buf[0] * 256) + buf[1];
    skyTemp = (rawTemp * 0.02) - 273.7;

    return true;
}

bool HitechWeather::getAmbientTemperature(double &ambientTemp)
{
    if (!m_hidHandle)
        return false;

    // Prepare command: getAmb
    unsigned char cmd[8] = {CMD_GET_AMBIENT, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    // Send command
    int res = hid_write(m_hidHandle, cmd, sizeof(cmd));
    if (res < 0)
    {
        LOGF_ERROR("Failed to write getAmb command: %ls", hid_error(m_hidHandle));
        return false;
    }

    // Read response
    unsigned char buf[8];
    res = hid_read_timeout(m_hidHandle, buf, sizeof(buf), 1000);
    if (res < 0)
    {
        LOGF_ERROR("Failed to read getAmb response: %ls", hid_error(m_hidHandle));
        return false;
    }

    if (res < 5)
    {
        LOGF_ERROR("Insufficient data received for getAmb: %d bytes", res);
        return false;
    }

    // Verify response signature (byte 1 should be 0x64, byte 5 should be 0x21)
    if (buf[0] != 0x64 || buf[4] != 0x21)
    {
        LOGF_ERROR("Invalid getAmb response signature: 0x%02X 0x%02X", buf[0], buf[4]);
        return false;
    }

    // Parse temperature
    // Formula: temp = ((byte2 * 256) + byte1) / 100.0
    // If sign byte (byte 4) = 0x10, then temperature is negative
    uint16_t rawTemp = (buf[2] * 256) + buf[1];
    ambientTemp = rawTemp / 100.0;
    
    if (buf[3] == 0x10)
    {
        ambientTemp *= -1.0;
    }

    return true;
}

double HitechWeather::calculateCloudCover(double ambientTemp, double skyTemp)
{
    // Calculate temperature differential
    double hitecDelta = ambientTemp - skyTemp;
    double absDelta = std::abs(hitecDelta);
    
    // Convert to cloud cover percentage
    // Clear sky: large delta (sky much colder) -> low cloud cover
    // Cloudy sky: small delta (clouds radiate heat) -> high cloud cover
    // Maximum delta of 22°C normalized to 0-100% range
    double cloudCover = 100.0 * (1.0 - std::min(absDelta, 22.0) / 22.0);
    
    // Clamp to 0-100 range
    cloudCover = std::max(0.0, std::min(100.0, cloudCover));
    
    return cloudCover;
}
