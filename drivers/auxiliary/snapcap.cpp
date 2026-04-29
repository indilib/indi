/*******************************************************************************
  Copyright(c) 2017-2024 Jarno Paananen. All right reserved.

  Based on Flip Flat driver by:

  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

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

#include "snapcap.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

namespace
{
uint64_t monotonicMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
}

class SnapCapReconnect final : public SnapCap::ReconnectInterface
{
    public:
        explicit SnapCapReconnect(SnapCap &owner) : m_Owner(owner) {}

        bool sendCommand(const char *command, char *response) override
        {
            if (!m_Owner.sendCommand(command, response))
            {
                m_ConnectionFailureCount++;
                if (m_ConnectionFailureCount >= m_Owner.maxConsecutiveFailures())
                    schedule(command);
                return false;
            }

            m_ConnectionFailureCount = 0;
            return true;
        }

        void process() override
        {
            if (!m_ReconnectPending)
                return;

            const uint64_t nowMs = monotonicMs();
            if (nowMs < m_NextReconnectAttemptMs)
                return;

            if (m_Owner.attemptReconnect())
            {
                reset();
                m_Owner.getStartupData();
                return;
            }

            m_ReconnectAttemptCount++;
            const int backoffMs = m_Owner.computeReconnectDelayMs(m_ReconnectAttemptCount);
            m_NextReconnectAttemptMs = nowMs + static_cast<uint64_t>(backoffMs);
            INDI::Logger::getInstance().print(
                m_Owner.getDeviceName(), INDI::Logger::DBG_WARNING, __FILE__, __LINE__,
                "Reconnect attempt %u failed, next retry in %d ms (base delay %d ms, exponential backoff)",
                m_ReconnectAttemptCount, backoffMs, m_Owner.reconnectBaseDelayMs());
        }

        void reset() override
        {
            m_ConnectionFailureCount = 0;
            m_ReconnectPending       = false;
            m_ReconnectAttemptCount  = 0;
            m_NextReconnectAttemptMs = 0;
        }

        void rescheduleNow() override
        {
            if (m_ReconnectPending)
                m_NextReconnectAttemptMs = monotonicMs();
        }

        bool isPending() const override
        {
            return m_ReconnectPending;
        }

    private:
        void schedule(const char *reason)
        {
            if (!m_ReconnectPending)
            {
                INDI::Logger::getInstance().print(
                    m_Owner.getDeviceName(), INDI::Logger::DBG_WARNING, __FILE__, __LINE__,
                    "Scheduling non-blocking reconnect after %d consecutive command failures (reason: %s)",
                    m_Owner.maxConsecutiveFailures(), reason);
                m_ReconnectPending      = true;
                m_ReconnectAttemptCount = 0;
            }

            if (m_NextReconnectAttemptMs == 0)
                m_NextReconnectAttemptMs = monotonicMs();
        }

        SnapCap &m_Owner;
        int m_ConnectionFailureCount{ 0 };
        bool m_ReconnectPending{ false };
        uint64_t m_NextReconnectAttemptMs{ 0 };
        uint8_t m_ReconnectAttemptCount{ 0 };
};

// We declare an auto pointer to SnapCap.
std::unique_ptr<SnapCap> snapcap(new SnapCap());

#define SNAP_CMD 7
#define SNAP_RES 8 // Includes terminating null
#define SNAP_TIMEOUT 3

SnapCap::SnapCap() : LightBoxInterface(this), DustCapInterface(this), reconnect(std::make_unique<SnapCapReconnect>(*this))
{
    setVersion(1, 5);
}

SnapCap::~SnapCap()
{

    delete serialConnection;
    delete tcpConnection;
}

bool SnapCap::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Status
    StatusTP[0].fill("COVER", "Cover", nullptr);
    StatusTP[1].fill("LIGHT", "Light", nullptr);
    StatusTP[2].fill("MOTOR", "Motor", nullptr);
    StatusTP.fill(getDeviceName(), "STATUS", "Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Firmware version
    FirmwareTP[0].fill("VERSION", "Version", nullptr);
    FirmwareTP.fill(getDeviceName(), "FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    ForceSP[0].fill("OFF", "Off", ISS_ON);
    ForceSP[1].fill("ON", "On", ISS_OFF);
    ForceSP.fill(getDeviceName(), "FORCE", "Force movement", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    ReconnectNP[0].fill("FAILURES_BEFORE_RECONNECT", "Max Failures", "%.f",
                        MIN_FAILURES_BEFORE_RECONNECT, MAX_FAILURES_BEFORE_RECONNECT, 1,
                        DEFAULT_MAX_CONSECUTIVE_FAILURES);
    ReconnectNP[1].fill("DELAY_MS", "Base delay (ms)", "%.f", MIN_RECONNECT_DELAY_MS,
                        MAX_RECONNECT_DELAY_SETTING_MS, 100,
                        DEFAULT_RECONNECT_BASE_DELAY_MS);
    ReconnectNP.fill(getDeviceName(), "RECONNECT_POLICY", "Reconnect Policy", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);
    ReconnectNP.load();

    DI::initProperties(MAIN_CONTROL_TAB, CAN_ABORT);
    LI::initProperties(MAIN_CONTROL_TAB, CAN_DIM);

    LightIntensityNP[0].setMin(0);
    LightIntensityNP[0].setMax(255);
    LightIntensityNP[0].setStep(10);

    hasLight = true;

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);

    addAuxControls();

    if (dustcapConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(serialConnection);
    }

    if (dustcapConnection & CONNECTION_TCP)
    {
        tcpConnection = new Connection::TCP(this);
        tcpConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(tcpConnection);
    }


    return true;
}

void SnapCap::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);
    defineProperty(ReconnectNP);

    // Get Light box properties
    LI::ISGetProperties(dev);
}

bool SnapCap::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    DI::updateProperties();

    if (hasLight)
        LI::updateProperties();

    if (isConnected())
    {
        defineProperty(StatusTP);
        defineProperty(FirmwareTP);
        defineProperty(ForceSP);

        getStartupData();
    }
    else
    {
        deleteProperty(StatusTP);
        deleteProperty(FirmwareTP);
        deleteProperty(ForceSP);
        PortFD = -1;
        reconnect->reset();
    }

    return true;
}

const char *SnapCap::getDefaultName()
{
    return "SnapCap";
}

bool SnapCap::Handshake()
{
    if (isSimulation())
    {
        LOGF_INFO("Connected successfully to simulated %s. Retrieving startup data...", getDeviceName());

        SetTimer(getCurrentPollingPeriod());
        return true;
    }

    if (!ping())
    {
        LOG_ERROR("Device ping failed.");
        return false;
    }

    return true;
}

bool SnapCap::callHandshake()
{
    if (dustcapConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == tcpConnection)
            PortFD = tcpConnection->getPortFD();
    }

    reconnect->reset();
    return Handshake();
}

bool SnapCap::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (ReconnectNP.isNameMatch(name))
    {
        ReconnectNP.update(values, names, n);

        int failuresBeforeReconnect = static_cast<int>(ReconnectNP[0].getValue());
        int delayMs = static_cast<int>(ReconnectNP[1].getValue());

        failuresBeforeReconnect =
            std::clamp(failuresBeforeReconnect, MIN_FAILURES_BEFORE_RECONNECT, MAX_FAILURES_BEFORE_RECONNECT);
        delayMs = std::clamp(delayMs, MIN_RECONNECT_DELAY_MS, MAX_RECONNECT_DELAY_SETTING_MS);

        ReconnectNP[0].setValue(failuresBeforeReconnect);
        ReconnectNP[1].setValue(delayMs);
        ReconnectNP.setState(IPS_OK);
        ReconnectNP.apply();
        saveConfig(ReconnectNP);

        LOGF_INFO(
            "Reconnect policy updated: reconnect after %d consecutive command failures; base backoff delay %d ms",
            failuresBeforeReconnect, delayMs);

        // Apply new timing immediately for any pending reconnect cycle.
        reconnect->rescheduleNow();

        return true;
    }

    if (LI::processNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool SnapCap::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (LI::processText(dev, name, texts, names, n))
        return true;

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool SnapCap::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (!dev || strcmp(dev, getDeviceName()))
        return false;

    if (ForceSP.isNameMatch(name))
    {
        ForceSP.update(states, names, n);
        ForceSP.apply();
        return true;
    }

    if (DI::processSwitch(dev, name, states, names, n))
        return true;

    if (LI::processSwitch(dev, name, states, names, n))
        return true;

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool SnapCap::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool SnapCap::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    ReconnectNP.save(fp);

    return LI::saveConfigItems(fp);
}

bool SnapCap::ping()
{
    bool found = getFirmwareVersion();
    // Sometimes the controller does a corrupt reply at first connect
    // so retry once just in case
    if (!found)
        found = getFirmwareVersion();
    return found;
}

bool SnapCap::isConnectionValid()
{
    if (PortFD < 0)
    {
        return false;
    }

    // For TCP connections, check if socket is still open by attempting a non-blocking check
    if (getActiveConnection() == tcpConnection)
    {
        // Check if file descriptor is still valid using fstat
        struct stat sb;
        if (fstat(PortFD, &sb) < 0)
        {
            LOGF_WARN("TCP connection appears to be broken (fstat failed): %s", strerror(errno));
            return false;
        }
    }

    return true;
}

bool SnapCap::attemptReconnect()
{
    LOGF_INFO("Attempting to reconnect to %s...", getDeviceName());

    // Disconnect first
    if (PortFD >= 0)
    {
        if (getActiveConnection() == serialConnection) {
            serialConnection->Disconnect();
            setConnected(false, IPS_ALERT);
        }
        else if (getActiveConnection() == tcpConnection) {
            tcpConnection->Disconnect();
            setConnected(false, IPS_ALERT);
        }
    }

    PortFD = -1;

    if (getActiveConnection() == serialConnection)
    {
        if (!serialConnection->Connect())
        {
            LOG_ERROR("Failed to reconnect serial connection");
            setConnected(true, IPS_ALERT);
            return false;
        }
        PortFD = serialConnection->getPortFD();
    }
    else if (getActiveConnection() == tcpConnection)
    {
        if (!tcpConnection->Connect())
        {
            LOG_ERROR("Failed to reconnect TCP connection");
            setConnected(true, IPS_ALERT);
            return false;
        }
        PortFD = tcpConnection->getPortFD();
    }

    char response[SNAP_RES];
    if (!sendCommand(">V000", response))
    {
        LOG_ERROR("Device ping failed after reconnect");
        setConnected(true, IPS_ALERT);
        return false;
    }

    LOGF_INFO("Successfully reconnected to %s", getDeviceName());
    setConnected(true, IPS_OK);
    return true;
}

int SnapCap::computeReconnectDelayMs(uint8_t attemptCount) const
{
    int delayMs = reconnectBaseDelayMs();

    // Use a capped exponential backoff: base * 2^attempt.
    for (uint8_t i = 0; i < attemptCount && delayMs < reconnectMaxDelayMs() / 2; i++)
        delayMs *= 2;

    delayMs = std::min(delayMs, reconnectMaxDelayMs());


    return delayMs;
}

int SnapCap::maxConsecutiveFailures() const
{
    return std::clamp(static_cast<int>(ReconnectNP[0].getValue()), MIN_FAILURES_BEFORE_RECONNECT,
                      MAX_FAILURES_BEFORE_RECONNECT);
}

int SnapCap::reconnectBaseDelayMs() const
{
    return std::clamp(static_cast<int>(ReconnectNP[1].getValue()), MIN_RECONNECT_DELAY_MS,
                      MAX_RECONNECT_DELAY_SETTING_MS);
}

int SnapCap::reconnectMaxDelayMs() const
{
    return MAX_RECONNECT_DELAY_MS;
}

bool SnapCap::sendCommand(const char *command, char *response)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    if (!isConnectionValid())
    {
        LOGF_WARN("Connection appears invalid before sending command: %s", command);
        return false;
    }

    // Flush any pending data — only valid for serial TTY file descriptors, not TCP sockets.
    if (isatty(PortFD))
        tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", command);

    char buffer[SNAP_CMD + 1]; // space for terminating null
    snprintf(buffer, SNAP_CMD + 1, "%s\r\n", command);

    if ((rc = tty_write(PortFD, buffer, SNAP_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Write failed for command '%s': %s", command, errstr);
        return false;
    }

    if ((rc = tty_nread_section(PortFD, response, SNAP_RES, '\n', SNAP_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Read failed for command '%s': %s", command, errstr);
        return false;
    }

    if (nbytes_read < 3)
    {
        LOGF_ERROR("Response too short for command '%s': got %d bytes", command, nbytes_read);
        return false;
    }

    response[nbytes_read - 2] = 0; // strip \r\n

    LOGF_DEBUG("RES (%s)", response);
    return true;
}

bool SnapCap::sendCommandWithRetry(const char *command, char *response)
{
    return reconnect->sendCommand(command, response);
}

bool SnapCap::getStartupData()
{
    bool rc1 = getFirmwareVersion();
    bool rc2 = getStatus();
    bool rc3 = getBrightness();

    return (rc1 && rc2 && rc3);
}

IPState SnapCap::ParkCap()
{
    if (isSimulation())
    {
        simulationWorkCounter = 3;
        return IPS_BUSY;
    }

    char response[SNAP_RES];

    const char *command = (ForceSP[1].getState() == ISS_ON) ? ">c000" : ">C000";

    if (!sendCommandWithRetry(command, response))
        return IPS_ALERT;

    if (strcmp(response, "*C000") == 0 || strcmp(response, "*c000") == 0)
    {
        // Set cover status to random value outside of range to force it to refresh
        prevCoverStatus   = 10;
        targetCoverStatus = 2;
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

IPState SnapCap::UnParkCap()
{
    if (isSimulation())
    {
        simulationWorkCounter = 3;
        return IPS_BUSY;
    }

    char response[SNAP_RES];

    const char *command = (ForceSP[1].getState() == ISS_ON) ? ">o000" : ">O000";

    if (!sendCommandWithRetry(command, response))
        return IPS_ALERT;

    if (strcmp(response, "*O000") == 0 || strcmp(response, "*o000") == 0)
    {
        // Set cover status to random value outside of range to force it to refresh
        prevCoverStatus   = 10;
        targetCoverStatus = 1;
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

IPState SnapCap::AbortCap()
{
    if (isSimulation())
    {
        simulationWorkCounter = 0;
        return IPS_OK;
    }

    char response[SNAP_RES];

    if (!sendCommandWithRetry(">A000", response))
        return IPS_ALERT;

    if (strcmp(response, "*A000") == 0)
    {
        // Set cover status to random value outside of range to force it to refresh
        prevCoverStatus = 10;
        return IPS_OK;
    }
    else
        return IPS_ALERT;
}

bool SnapCap::EnableLightBox(bool enable)
{
    char response[SNAP_RES];

    if (isSimulation())
        return true;

    const char *command          = enable ? ">L000" : ">D000";
    const char *expectedResponse = enable ? "*L000" : "*D000";

    if (!sendCommandWithRetry(command, response))
        return false;

    return strcmp(response, expectedResponse) == 0;
}

bool SnapCap::getStatus()
{
    char response[SNAP_RES];

    if (isSimulation())
    {
        if (ParkCapSP.getState() == IPS_BUSY && --simulationWorkCounter <= 0)
        {
            ParkCapSP.setState(IPS_IDLE);
            ParkCapSP.apply();
            simulationWorkCounter = 0;
        }

        if (ParkCapSP.getState() == IPS_BUSY)
        {
            response[2] = '1';
            response[4] = '0';
        }
        else
        {
            response[2] = '0';
            // Parked/Closed
            response[4] = (ParkCapSP[CAP_PARK].getState() == ISS_ON) ? '2' : '1';
        }

        response[3] = (LightSP[FLAT_LIGHT_ON].getState() == ISS_ON) ? '1' : '0';
    }
    else
    {
        if (!sendCommandWithRetry(">S000", response))
            return false;
    }

    char motorStatus = response[2] - '0';
    char lightStatus = response[3] - '0';
    char coverStatus = response[4] - '0';

    // Force cover status as it doesn't reflect moving state otherwise...
    if (motorStatus)
    {
        coverStatus = 0;
    }
    bool statusUpdated = false;

    if (coverStatus != prevCoverStatus)
    {
        prevCoverStatus = coverStatus;

        statusUpdated = true;

        switch (coverStatus)
        {
            case 0:
                StatusTP[0].setText("Opening/closing");
                break;

            case 1:
                if ((targetCoverStatus == 1 && ParkCapSP.getState() == IPS_BUSY) || ParkCapSP.getState() == IPS_IDLE)
                {
                    StatusTP[0].setText("Open");
                    ParkCapSP.reset();
                    ParkCapSP[CAP_UNPARK].setState(ISS_ON);
                    ParkCapSP.setState(IPS_OK);
                    LOG_INFO("Cover open.");
                    ParkCapSP.apply();
                }
                break;

            case 2:
                if ((targetCoverStatus == 2 && ParkCapSP.getState() == IPS_BUSY) || ParkCapSP.getState() == IPS_IDLE)
                {
                    StatusTP[0].setText("Closed");
                    ParkCapSP.reset();
                    ParkCapSP[CAP_PARK].setState(ISS_ON);
                    ParkCapSP.setState(IPS_OK);
                    LOG_INFO("Cover closed.");
                    ParkCapSP.apply();
                }
                break;

            case 3:
                StatusTP[0].setText("Timed out");
                break;

            case 4:
                StatusTP[0].setText("Open circuit");
                break;

            case 5:
                StatusTP[0].setText("Overcurrent");
                break;

            case 6:
                StatusTP[0].setText("User abort");
                break;
        }
    }

    if (lightStatus != prevLightStatus)
    {
        prevLightStatus = lightStatus;

        statusUpdated = true;

        switch (lightStatus)
        {
            case 0:
                StatusTP[1].setText("Off");
                if (LightSP[0].getState() == ISS_ON)
                {
                    LightSP[0].setState(ISS_OFF);
                    LightSP[1].setState(ISS_ON);
                    LightSP.apply();
                }
                break;

            case 1:
                StatusTP[1].setText("On");
                if (LightSP[1].getState() == ISS_ON)
                {
                    LightSP[0].setState(ISS_ON);
                    LightSP[1].setState(ISS_OFF);
                    LightSP.apply();
                }
                break;
        }
    }

    if (motorStatus != prevMotorStatus)
    {
        prevMotorStatus = motorStatus;

        statusUpdated = true;

        switch (motorStatus)
        {
            case 0:
                StatusTP[2].setText("Stopped");
                break;

            case 1:
                StatusTP[2].setText("Running");
                break;
        }
    }

    if (statusUpdated)
        StatusTP.apply();

    return true;
}

bool SnapCap::getFirmwareVersion()
{
    if (isSimulation())
    {
        FirmwareTP[0].setText("Ssimulation");
        FirmwareTP.apply();
        return true;
    }

    char response[SNAP_RES];

    if (!sendCommandWithRetry(">V000", response))
        return false;

    char versionString[4] = { 0 };
    snprintf(versionString, 4, "%s", response + 2);
    FirmwareTP[0].setText(versionString);
    FirmwareTP.apply();

    return true;
}

void SnapCap::TimerHit()
{
    if (!isConnected() && !reconnect->isPending())
        return;

    reconnect->process();

    if (isConnected() && !reconnect->isPending())
        getStatus();

    SetTimer(getCurrentPollingPeriod());
}

bool SnapCap::getBrightness()
{
    if (isSimulation())
    {
        return true;
    }

    char response[SNAP_RES];

    if (!sendCommandWithRetry(">J000", response))
        return false;

    int brightnessValue = 0;
    int rc              = sscanf(response, "*J%d", &brightnessValue);

    if (rc <= 0)
    {
        LOGF_ERROR("Unable to parse brightness value (%s)", response);
        return false;
    }

    if (brightnessValue != prevBrightness)
    {
        prevBrightness           = brightnessValue;
        LightIntensityNP[0].setValue(brightnessValue);
        LightIntensityNP.apply();
    }

    return true;
}

bool SnapCap::SetLightBoxBrightness(uint16_t value)
{
    if (isSimulation())
    {
        LightIntensityNP[0].setValue(value);
        LightIntensityNP.apply();
        return true;
    }

    char command[SNAP_CMD];
    char response[SNAP_RES];

    snprintf(command, SNAP_CMD, ">B%03d", value);

    if (!sendCommandWithRetry(command, response))
        return false;

    int brightnessValue = 0;
    int rc              = sscanf(response, "*B%d", &brightnessValue);

    if (rc <= 0)
    {
        LOGF_ERROR("Unable to parse brightness value (%s)", response);
        return false;
    }

    if (brightnessValue != prevBrightness)
    {
        prevBrightness           = brightnessValue;
        LightIntensityNP[0].setValue(brightnessValue);
        LightIntensityNP.apply();
    }

    return true;
}

uint8_t SnapCap::getDustcapConnection() const
{
    return dustcapConnection;
}

void SnapCap::setDustcapConnection(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_TCP | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        LOGF_ERROR( "Invalid connection mode %d", value);
        return;
    }

    dustcapConnection = value;
}

