/*
    OnStepX INDI Driver — Weather sensor helper (both binaries)
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "OnStepXWeather.h"
#include "OnStepXComm.h"
#include "OnStepXFormat.h"

#include <defaultdevice.h>

#include <cstdio>
#include <cstdlib>

#define WEATHER_TAB "Weather"
#define SITE_MGMT_TAB "Site Management"   // INDI::Telescope SITE_TAB
#define ONSTEPX_TAB   "OnStepX"           // with the firmware and status information

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
void OnStepXWeather::initProperties()
{
    const char *dev = m_dev ? m_dev->getDeviceName() : "";

    // --- OSX_WEATHER_SET ---
    // Absolute values the firmware uses for refraction when no weather
    // sensor is fitted (:SX9A/B/C).  With a sensor fitted the firmware keeps
    // the measured values and ignores these.  Ranges are the firmware's.
    m_weatherSetNP[0].fill("WEATHER_SET_TEMPERATURE", "Temperature (C)", "%.1f", -60.0,   60.0, 0.5,   10.0);
    m_weatherSetNP[1].fill("WEATHER_SET_PRESSURE",    "Pressure (hPa)",  "%.1f", 100.0, 1100.0, 1.0, 1010.0);
    m_weatherSetNP[2].fill("WEATHER_SET_HUMIDITY",    "Humidity (%)",    "%.1f",   0.0,  100.0, 1.0,   70.0);
    m_weatherSetNP.fill(dev, "OSX_WEATHER_SET", "Manual Weather",
                        WEATHER_TAB, IP_RW, 60, IPS_IDLE);

    // --- OSX_DUT1 ---
    m_dut1NP[0].fill("DUT1", "DUT1 (UT1-UTC, s)", "%.3f", -0.9, 0.9, 0.001, 0.0);
    m_dut1NP.fill(dev, "OSX_DUT1", "DUT1 Correction",
                  SITE_MGMT_TAB, IP_RW, 60, IPS_IDLE);

    // --- OSX_MCU_TEMP ---
    // Controller information, not weather: shown whenever the firmware reports
    // it, with or without a weather sensor.
    m_mcuTempNP[0].fill("MCU_TEMPERATURE", "Temperature (C)", "%.1f", -40, 125, 0, 0);
    m_mcuTempNP.fill(dev, "OSX_MCU_TEMP", "MCU Temp.", ONSTEPX_TAB, IP_RO, 60, IPS_IDLE);
}

// ---------------------------------------------------------------------------
// MCU temperature (:GX9F#)
// ---------------------------------------------------------------------------
void OnStepXWeather::updateMcuTemp(bool connected, bool hasMcuTemp)
{
    if (!m_dev)
        return;
    if (connected && hasMcuTemp)
    {
        m_dev->defineProperty(m_mcuTempNP);
        m_mcuDefined = true;
        pollMcuTemp();
    }
    else if (!connected && m_mcuDefined)
    {
        m_dev->deleteProperty(m_mcuTempNP);
        m_mcuDefined = false;
    }
}

void OnStepXWeather::pollMcuTemp()
{
    if (!m_mcuDefined)
        return;
    const WeatherReading r = readMcuTemp();
    if (r.ok)
        m_mcuTempNP[0].setValue(r.value);
    // A missed read (e.g. an ESP32 stall) keeps the last value shown.
    m_mcuTempNP.setState(r.ok ? IPS_OK : IPS_ALERT);
    m_mcuTempNP.apply();
}

WeatherReading OnStepXWeather::readMcuTemp()
{
    return tryRead(":GX9F#");
}

// ---------------------------------------------------------------------------
// updateProperties
// ---------------------------------------------------------------------------
void OnStepXWeather::updateProperties(bool connected)
{
    if (connected)
    {
        m_dev->defineProperty(m_weatherSetNP);
        m_dev->defineProperty(m_dut1NP);
    }
    else
    {
        m_dev->deleteProperty(m_weatherSetNP);
        m_dev->deleteProperty(m_dut1NP);
    }
}

// ---------------------------------------------------------------------------
// handleNumber
// ---------------------------------------------------------------------------
bool OnStepXWeather::handleNumber(const char *name, double values[], char *names[], int n)
{
    if (m_weatherSetNP.isNameMatch(name))
    {
        m_weatherSetNP.update(values, names, n);

        // Send each offset to firmware: :SX9A,[v]# / :SX9B,[v]# / :SX9C,[v]#
        const char *cmds[3] = { ":SX9A,%.2f#", ":SX9B,%.2f#", ":SX9C,%.2f#" };
        bool allOk = true;
        for (int i = 0; i < 3; i++)
        {
            char cmd[OnStepXComm::CMD_MAX_LEN];
            snprintf(cmd, sizeof(cmd), cmds[i], m_weatherSetNP[i].getValue());
            if (!m_comm->sendBool(cmd))
                allOk = false;
        }
        m_weatherSetNP.setState(allOk ? IPS_OK : IPS_ALERT);
        m_weatherSetNP.apply();
        return true;
    }

    if (m_dut1NP.isNameMatch(name))
    {
        m_dut1NP.update(values, names, n);
        double dut1 = m_dut1NP[0].getValue();

        char cmd[OnStepXComm::CMD_MAX_LEN];
        snprintf(cmd, sizeof(cmd), ":SU%+.3f#", dut1);
        if (m_comm->sendBool(cmd))
            m_dut1NP.setState(IPS_OK);
        else
            m_dut1NP.setState(IPS_ALERT);

        m_dut1NP.apply();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// saveConfig
// ---------------------------------------------------------------------------
void OnStepXWeather::saveConfig(FILE *fp)
{
    // Saved deliberately: the manual weather values (:SX9A/B/C) and DUT1 (:SU)
    // live only in the firmware's RAM, so the INDI config restores them after
    // the controller reboots.
    m_weatherSetNP.save(fp);
    m_dut1NP.save(fp);
}

// ---------------------------------------------------------------------------
// tryRead
// ---------------------------------------------------------------------------
WeatherReading OnStepXWeather::tryRead(const char *cmd)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_comm->sendText(cmd, reply))
        return {};

    // Absent values: a bare '0' (rejected by sendText) or "nan" (no sensor,
    // or a failed read) — parseNumber() accepts finite numbers only.
    double val = 0;
    if (!OnStepXFormat::parseNumber(reply, val))
        return {};

    return { true, val };
}

// ---------------------------------------------------------------------------
// readSensors
// ---------------------------------------------------------------------------
SensorData OnStepXWeather::readSensors()
{
    SensorData data;

    data.temp     = tryRead(":GX9A#");
    data.pressure = tryRead(":GX9B#");
    data.humidity = tryRead(":GX9C#");
    data.dewpoint = tryRead(":GX9E#");

    return data;
}
