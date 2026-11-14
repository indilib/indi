/*
    OnStepX INDI Driver
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

// Debug only
// #include <signal.h>
// #include <unistd.h>
// Debug only end

#include "OnStepXAux.h"
#include "OnStepXVersion.h"

#define WEATHER_TAB "Weather"   // tab name for WeatherInterface properties

// SmartWebServer command channel: 9999 (standard), 9996-9998 (persistent).
static constexpr int OSX_DEFAULT_TCP_PORT = 9999;

OnStepXAux::OnStepXAux() : INDI::WeatherInterface(this)
{
    // Debug only
    // Halts the process at this point. Allows remote debugger to attach which is required
    // when launching the driver from a client eg. Ekos
    // kill(getpid(), SIGSTOP);
    // Debug only end

    setVersion(ONSTEPX_VERSION_MAJOR, ONSTEPX_VERSION_MINOR);
    m_core.setDevice(this);
    m_weather.setDevice(this);
    m_driverStatus.setDevice(this);
}

const char *OnStepXAux::getDefaultName()
{
    return "OnStepX Controller";
}

bool OnStepXAux::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // The rotator is a child device, OnStepXRotatorDevice ("<name> Rotator").

    WI::initProperties(WEATHER_TAB, WEATHER_TAB);
    m_weather.initProperties();

    // --- OSX_FIRMWARE ---
    m_firmwareTP[0].fill("FIRMWARE_VERSION", "Version", "");
    m_firmwareTP[1].fill("FIRMWARE_DATE",    "Date",    "");
    m_firmwareTP[2].fill("FIRMWARE_TIME",    "Time",    "");
    m_firmwareTP[3].fill("FIRMWARE_CONFIG",  "Config",  "");
    m_firmwareTP.fill(getDeviceName(), "OSX_FIRMWARE", "Firmware Info",
                      "OnStepX", IP_RO, 60, IPS_IDLE);

    m_serialConnection = new Connection::Serial(this);
    m_serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    m_serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);  // OnStepX SERIAL_A default
    registerConnection(m_serialConnection);

    m_tcpConnection = new Connection::TCP(this);
    m_tcpConnection->setDefaultHost("192.168.4.1");
    m_tcpConnection->setDefaultPort(OSX_DEFAULT_TCP_PORT);
    m_tcpConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(m_tcpConnection);

    addAuxControls();
    return true;
}

bool OnStepXAux::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (m_core.caps().hasWeatherRead)
    {
        if (isConnected() && !m_weatherParamsAdded)
        {
            // Deferred until connected; WeatherInterface::addParameter()
            // appends, so add exactly once per process.
            addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -40,   80, 15);
            addParameter("WEATHER_PRESSURE",    "Pressure (hPa)",  800, 1100, 15);
            addParameter("WEATHER_HUMIDITY",    "Humidity (%)",      0,  100, 15);
            addParameter("WEATHER_DEWPOINT",    "Dew Point (C)",   -40,   40, 15);
            m_weatherParamsAdded = true;
        }

        WI::updateProperties();

        if (isConnected())
        {
            // Weather is polled from TimerHit; with the period at 0 the WI
            // timer only re-arms itself to retry after a failed read.
            UpdatePeriodNP[0].setValue(0);
            UpdatePeriodNP.apply();
        }
    }

    // Manual weather values (used for refraction when no sensor is fitted)
    // are always offered: on a new controller without a sensor the readings
    // are "nan" until these are set, so they cannot depend on hasWeatherRead.
    m_weather.updateProperties(isConnected());

    if (isConnected())
    {
        defineProperty(m_firmwareTP);
        m_firmwareTP[0].setText(m_core.caps().firmwareVersion);
        m_firmwareTP[1].setText(m_core.caps().firmwareDate);
        m_firmwareTP[2].setText(m_core.caps().firmwareTime);
        m_firmwareTP[3].setText(m_core.caps().configName);
        m_firmwareTP.setState(IPS_OK);
        m_firmwareTP.apply();
        m_weather.updateMcuTemp(true, m_core.caps().hasMcuTemp);   // below the firmware info

        createFocusers();

        OnStepXRotatorDevice::connectChild(m_rotatorDev, m_core.caps().hasRotator, m_core.caps().hasDerotator,
                                           &m_core.comm(), getDeviceName());

        OnStepXPowerDevice::connectChild(m_powerDev, m_core.caps().featureMask, m_core.caps().portMask,
                                         &m_core.comm(), getDeviceName());

        m_driverStatus.discoverAndDefine(m_core.caps().driverStatusMask);

        m_pollCount = 0;
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(m_firmwareTP);
        m_weather.updateMcuTemp(false, false);

        m_driverStatus.deleteAll();
    }

    return true;
}

bool OnStepXAux::Handshake()
{
    if (getActiveConnection() == m_serialConnection)
        m_core.setFd(m_serialConnection->getPortFD());
    else if (getActiveConnection() == m_tcpConnection)
        m_core.setFd(m_tcpConnection->getPortFD());

    if (!m_core.probeController())
    {
        LOG_ERROR("Not an OnStepX controller. Aborting connection.");
        return false;
    }

    m_driverStatus.setComm(&m_core.comm());
    m_weather.setComm(&m_core.comm());
    return true;
}

// ---------------------------------------------------------------------------
// Disconnect — stop all controller I/O before the port is closed, so neither
// this device nor the focuser children can write to a stale (possibly
// reused) file descriptor.
// ---------------------------------------------------------------------------
bool OnStepXAux::Disconnect()
{
    m_core.setFd(-1);
    disconnectFocusers();
    OnStepXRotatorDevice::disconnectChild(m_rotatorDev);
    OnStepXPowerDevice::disconnectChild(m_powerDev);
    return INDI::DefaultDevice::Disconnect();
}

bool OnStepXAux::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (WI::processSwitch(dev, name, states, names, n))
        return true;
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool OnStepXAux::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (WI::processNumber(dev, name, values, names, n))
        return true;
    if (isConnected() && m_weather.handleNumber(name, values, names, n))
        return true;
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool OnStepXAux::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool OnStepXAux::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    m_weather.saveConfig(fp);
    return true;
}

// ---------------------------------------------------------------------------
// TimerHit — periodic polling, armed in updateProperties() on connect
// ---------------------------------------------------------------------------
void OnStepXAux::TimerHit()
{
    if (!isConnected())
        return;

    if (m_core.comm().linkLost())
    {
        LOGF_ERROR("No reply from the controller for %.0f s; disconnecting. Check its power and cable, then reconnect.",
                   m_core.comm().secondsSinceReply());
        m_core.setFd(-1);   // so the clean-up in Disconnect() does not wait on the dead link
        Disconnect();
        setConnected(false, IPS_ALERT);
        updateProperties();
        return;             // no further polls
    }

    // Staggered so a poll carries at most one subsystem (see
    // OnStepXMount::ReadScopeStatus).  Periods: 5 polls (focusers, outputs,
    // rotator) and 10 (weather, MCU temperature, driver status).
    m_pollCount++;
    switch (m_pollCount % 5)
    {
        case 0:
            pollFocusers();
            break;
        case 1:
            pollPower();
            break;
        case 2:
            updateRotatorState();
            break;
        case 3:
            if (m_pollCount % 10 == 3)
            {
                if (m_core.caps().hasWeatherRead)
                    WI::checkWeatherUpdate();
            }
            else
                m_weather.pollMcuTemp();
            break;
        default:
            if (m_pollCount % 10 == 4)
                m_driverStatus.poll();
            break;
    }

    SetTimer(getCurrentPollingPeriod());
}

// ---------------------------------------------------------------------------
// Focuser child devices
// ---------------------------------------------------------------------------
void OnStepXAux::createFocusers()
{
    OnStepXFocuser::connectChildren(m_focusers, m_core.caps().focuserMask, &m_core.comm(), getDeviceName());
}

void OnStepXAux::disconnectFocusers()
{
    OnStepXFocuser::disconnectChildren(m_focusers);
}

void OnStepXAux::pollFocusers()
{
    for (auto &f : m_focusers)
        if (f)
            f->pollStatus();
}

void OnStepXAux::pollPower()
{
    if (m_powerDev)
        m_powerDev->pollStatus();
}

// ---------------------------------------------------------------------------
// updateWeather — WeatherInterface callback
// ---------------------------------------------------------------------------
IPState OnStepXAux::updateWeather()
{
    SensorData data = m_weather.readSensors();
    if (data.temp.ok)      setParameterValue("WEATHER_TEMPERATURE", data.temp.value);
    if (data.pressure.ok)  setParameterValue("WEATHER_PRESSURE",    data.pressure.value);
    if (data.humidity.ok)  setParameterValue("WEATHER_HUMIDITY",    data.humidity.value);
    if (data.dewpoint.ok)  setParameterValue("WEATHER_DEWPOINT",    data.dewpoint.value);
    return data.anyOk() ? IPS_OK : IPS_ALERT;
}

// ---------------------------------------------------------------------------
// updateRotatorState — poll angle and status
// ---------------------------------------------------------------------------
void OnStepXAux::updateRotatorState()
{
    if (m_rotatorDev)
        m_rotatorDev->pollStatus();
}


