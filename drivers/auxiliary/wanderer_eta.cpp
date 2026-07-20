/*******************************************************************************
  Copyright(c) 2025 cfuture81. All rights reserved.

  Wanderer ETA M54 - Electronic Tilt Adjuster

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

#include "wanderer_eta.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <mutex>
#include <chrono>

static std::unique_ptr<WandererETA> wandererETA(new WandererETA());

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Constructor
////////////////////////////////////////////////////////////////////////////////////////////////////////
WandererETA::WandererETA()
{
    setVersion(1, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Device name
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *WandererETA::getDefaultName()
{
    return "Wanderer ETA M54";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Init properties
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererETA::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE);
    addAuxControls();

    // Motor position targets (user-settable) - separate properties for independent control
    Position1NP[0].fill("POINT_1", "Position (mm)", "%.3f", 0.000, 1.200, 0.001, 0.000);
    Position1NP.fill(getDeviceName(), "POINT_1_TARGET", "Point 1 Target", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    Position2NP[0].fill("POINT_2", "Position (mm)", "%.3f", 0.000, 1.200, 0.001, 0.000);
    Position2NP.fill(getDeviceName(), "POINT_2_TARGET", "Point 2 Target", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    Position3NP[0].fill("POINT_3", "Position (mm)", "%.3f", 0.000, 1.200, 0.001, 0.000);
    Position3NP.fill(getDeviceName(), "POINT_3_TARGET", "Point 3 Target", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Motor position readback (encoder feedback, read-only)
    PositionReadNP[POINT_1].fill("POINT_1_READ", "Point 1 (mm)", "%.3f", 0.000, 1.200, 0.000, 0.000);
    PositionReadNP[POINT_2].fill("POINT_2_READ", "Point 2 (mm)", "%.3f", 0.000, 1.200, 0.000, 0.000);
    PositionReadNP[POINT_3].fill("POINT_3_READ", "Point 3 (mm)", "%.3f", 0.000, 1.200, 0.000, 0.000);
    PositionReadNP.fill(getDeviceName(), "POSITION_READBACK", "Current Positions", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Firmware information
    FirmwareTP[FIRMWARE_VERSION].fill("FIRMWARE_VERSION", "Firmware Version", "Unknown");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Zero all points
    ZeroAllSP[ZERO_ALL].fill("ZERO_ALL", "Zero All Points", ISS_OFF);
    ZeroAllSP.fill(getDeviceName(), "ZERO_ALL_CMD", "Zero All", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Backfocus offset - apply uniform offset to all 3 points simultaneously
    BackfocusOffsetNP[0].fill("OFFSET_MM", "Offset (mm)", "%.3f", -1.200, 1.200, 0.010, 0.000);
    BackfocusOffsetNP.fill(getDeviceName(), "BACKFOCUS_OFFSET", "Backfocus Offset", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    ApplyOffsetSP[APPLY_OFFSET].fill("APPLY_OFFSET", "Apply Offset", ISS_OFF);
    ApplyOffsetSP.fill(getDeviceName(), "APPLY_OFFSET_CMD", "Apply Offset", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);

    setDefaultPollingPeriod(2000);

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    serialConnection->registerHandshake([&]()
    {
        return getData();
    });
    registerConnection(serialConnection);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Update properties on connect/disconnect
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererETA::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        char firmwareStr[16];
        snprintf(firmwareStr, sizeof(firmwareStr), "%d", firmware);
        FirmwareTP[FIRMWARE_VERSION].setText(firmwareStr);
        LOGF_INFO("Firmware version: %d", firmware);

        defineProperty(Position1NP);
        defineProperty(Position2NP);
        defineProperty(Position3NP);
        defineProperty(PositionReadNP);
        defineProperty(FirmwareTP);
        defineProperty(ZeroAllSP);
        defineProperty(BackfocusOffsetNP);
        defineProperty(ApplyOffsetSP);
    }
    else
    {
        deleteProperty(Position1NP);
        deleteProperty(Position2NP);
        deleteProperty(Position3NP);
        deleteProperty(PositionReadNP);
        deleteProperty(FirmwareTP);
        deleteProperty(ZeroAllSP);
        deleteProperty(BackfocusOffsetNP);
        deleteProperty(ApplyOffsetSP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Get data from device (handshake + polling)
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererETA::getData()
{
    if (!serialPortMutex.try_lock_for(std::chrono::milliseconds(500)))
    {
        LOG_DEBUG("Serial port is busy, skipping status update");
        return true;
    }

    std::lock_guard<std::timed_mutex> lock(serialPortMutex, std::adopt_lock);

    try
    {
        PortFD = serialConnection->getPortFD();

        // On first read (handshake), flush stale serial buffer data.
        // The ETA device streams status continuously (~every 2s); the first bytes after
        // opening the port may contain garbage from power-on or prior session.
        // After flushing, we need to wait long enough for a fresh complete status line.
        if (m_FirstRead)
        {
            tcflush(PortFD, TCIOFLUSH);
            m_FirstRead = false;

            // Try up to 3 reads with 3s timeout each to get a valid status line.
            // The device streams every ~2s, so worst case we wait one full cycle.
            char firstBuffer[512] = {0};
            int firstBytes = 0;
            for (int attempt = 0; attempt < 3; attempt++)
            {
                memset(firstBuffer, 0, sizeof(firstBuffer));
                firstBytes = 0;
                int firstRc = tty_read_section(PortFD, firstBuffer, '\n', 3, &firstBytes);
                if (firstRc == TTY_OK && parseDeviceData(firstBuffer))
                {
                    LOG_INFO("Wanderer ETA M54 is online.");
                    return true;
                }
                // If we got data but it didn't parse (partial/garbage), flush and retry
                if (firstBytes > 0)
                    tcflush(PortFD, TCIOFLUSH);
            }
            LOG_WARN("Handshake: could not get valid data after 3 attempts");
            return false;
        }

        char buffer[512] = {0};
        int nbytes_read = 0, rc = -1;

        if ((rc = tty_read_section(PortFD, buffer, '\n', 2, &nbytes_read)) != TTY_OK)
        {
            if (rc == TTY_TIME_OUT)
            {
                LOG_DEBUG("Timeout reading from device, will try again later");
                return true;
            }

            char errorMessage[MAXRBUF];
            tty_error_msg(rc, errorMessage, MAXRBUF);
            LOGF_ERROR("Failed to read data from device. Error: %s", errorMessage);
            return false;
        }

        return parseDeviceData(buffer);
    }
    catch (std::exception &e)
    {
        LOG_ERROR("Exception occurred while reading data from device");
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Parse device status data
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererETA::parseDeviceData(const char *data)
{
    try
    {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(data);
        LOGF_DEBUG("Data: %s", data);

        // Split the data by 'A' separator
        while (std::getline(tokenStream, token, 'A'))
        {
            if (!token.empty())
                tokens.push_back(token);
        }

        // Need at least: device_id + firmware + 3 positions
        if (tokens.size() < 5)
        {
            LOGF_DEBUG("Insufficient tokens: %d (need 5)", static_cast<int>(tokens.size()));
            return false;
        }

        // Validate device identifier
        if (tokens[0] != "WandererTilterM54")
        {
            // During handshake, garbage on first read is expected — suppress warning
            if (m_Initializing)
                LOGF_DEBUG("Handshake: skipping invalid data: %s", tokens[0].c_str());
            else
                LOGF_WARN("Unknown device: %s (expected WandererTilterM54)", tokens[0].c_str());
            return false;
        }

        // Firmware version
        firmware = std::atoi(tokens[1].c_str());

        char firmwareStr[16];
        snprintf(firmwareStr, sizeof(firmwareStr), "%d", firmware);
        FirmwareTP[FIRMWARE_VERSION].setText(firmwareStr);
        FirmwareTP.setState(IPS_OK);
        FirmwareTP.apply();

        // Point positions from encoders
        double pos1 = std::strtod(tokens[2].c_str(), nullptr);
        double pos2 = std::strtod(tokens[3].c_str(), nullptr);
        double pos3 = std::strtod(tokens[4].c_str(), nullptr);

        PositionReadNP[POINT_1].setValue(pos1);
        PositionReadNP[POINT_2].setValue(pos2);
        PositionReadNP[POINT_3].setValue(pos3);
        PositionReadNP.setState(IPS_OK);
        PositionReadNP.apply();

        return true;
    }
    catch (std::exception &e)
    {
        LOG_ERROR("Failed to parse device data");
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper: update a position property by index
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererETA::setPositionNP(int index, double values[], char *names[], int n, IPState state)
{
    switch (index)
    {
        case 0:
            Position1NP.update(values, names, n);
            Position1NP.setState(state);
            Position1NP.apply();
            break;
        case 1:
            Position2NP.update(values, names, n);
            Position2NP.setState(state);
            Position2NP.apply();
            break;
        case 2:
            Position3NP.update(values, names, n);
            Position3NP.setState(state);
            Position3NP.apply();
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper: set state on a position property by index
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererETA::setPositionState(int index, IPState state)
{
    switch (index)
    {
        case 0:
            Position1NP.setState(state);
            Position1NP.apply();
            break;
        case 1:
            Position2NP.setState(state);
            Position2NP.apply();
            break;
        case 2:
            Position3NP.setState(state);
            Position3NP.apply();
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Handle new number values (position targets)
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererETA::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Handle backfocus offset value change (just stores the value)
        if (BackfocusOffsetNP.isNameMatch(name))
        {
            BackfocusOffsetNP.update(values, names, n);
            BackfocusOffsetNP.setState(IPS_OK);
            BackfocusOffsetNP.apply();
            return true;
        }

        // Determine which point is being commanded
        int pointIndex = -1;

        if (Position1NP.isNameMatch(name))
            pointIndex = 0;
        else if (Position2NP.isNameMatch(name))
            pointIndex = 1;
        else if (Position3NP.isNameMatch(name))
            pointIndex = 2;

        if (pointIndex >= 0)
        {
            double target = values[0];

            // During initialization (config load), just store values without moving
            if (m_Initializing)
            {
                setPositionNP(pointIndex, values, names, n, IPS_OK);
                return true;
            }

            // Validate
            if (target < 0.0 || target > 1.2)
            {
                LOGF_ERROR("Position value %.3f out of range (0.000 - 1.200 mm)", target);
                setPositionNP(pointIndex, values, names, n, IPS_ALERT);
                return true;
            }

            // Don't allow new moves while one is in progress
            if (m_Move.active)
            {
                LOG_WARN("A move is already in progress. Please wait for it to complete.");
                setPositionState(pointIndex, IPS_IDLE);
                return true;
            }

            setPositionNP(pointIndex, values, names, n, IPS_BUSY);
            startMove(pointIndex, target);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Handle new switch values (Zero All)
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererETA::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Apply backfocus offset to all 3 points equally
        if (ApplyOffsetSP.isNameMatch(name))
        {
            if (m_Move.active)
            {
                LOG_WARN("A move is already in progress. Please wait for it to complete.");
                return true;
            }

            double offset = BackfocusOffsetNP[0].getValue();

            if (std::abs(offset) < 0.001)
            {
                LOG_WARN("Offset is zero, nothing to apply");
                ApplyOffsetSP.setState(IPS_IDLE);
                ApplyOffsetSP[APPLY_OFFSET].setState(ISS_OFF);
                ApplyOffsetSP.apply();
                return true;
            }

            // Read current positions from readback
            double pos1 = PositionReadNP[POINT_1].getValue();
            double pos2 = PositionReadNP[POINT_2].getValue();
            double pos3 = PositionReadNP[POINT_3].getValue();

            // Calculate new targets
            double targets[3] = {pos1 + offset, pos2 + offset, pos3 + offset};

            // Validate all within range
            if (targets[0] < 0.0 || targets[0] > 1.2 ||
                targets[1] < 0.0 || targets[1] > 1.2 ||
                targets[2] < 0.0 || targets[2] > 1.2)
            {
                LOGF_ERROR("Offset %.3f mm would move one or more points out of range (0.000 - 1.200 mm). "
                           "Current positions: P1=%.3f, P2=%.3f, P3=%.3f",
                           offset, pos1, pos2, pos3);
                ApplyOffsetSP.setState(IPS_ALERT);
                ApplyOffsetSP[APPLY_OFFSET].setState(ISS_OFF);
                ApplyOffsetSP.apply();
                return true;
            }

            LOGF_INFO("Applying backfocus offset %.3f mm to all points (P1: %.3f→%.3f, P2: %.3f→%.3f, P3: %.3f→%.3f)",
                      offset, pos1, targets[0], pos2, targets[1], pos3, targets[2]);

            ApplyOffsetSP.setState(IPS_BUSY);
            ApplyOffsetSP.apply();
            startMultiPointMove(targets, false, true);
            return true;
        }

        if (ZeroAllSP.isNameMatch(name))
        {
            if (m_Move.active)
            {
                LOG_WARN("A move is already in progress. Please wait for it to complete.");
                return true;
            }

            double targets[3] = {0.0, 0.0, 0.0};
            LOG_INFO("Moving all points to zero position");

            ZeroAllSP.setState(IPS_BUSY);
            ZeroAllSP.apply();
            startMultiPointMove(targets, true, false);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Start a single-point move (non-blocking)
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererETA::startMove(int pointIndex, double target)
{
    m_Move.active = true;
    m_Move.isZeroAll = false;
    m_Move.isApplyOffset = false;
    m_Move.target[pointIndex] = target;
    m_Move.pending[0] = m_Move.pending[1] = m_Move.pending[2] = false;
    m_Move.pending[pointIndex] = true;
    m_Move.graceCount = 3;

    // Calculate timeout: travel distance / motor speed + safety margin
    double distance = std::abs(target - PositionReadNP[pointIndex].getValue());
    m_Move.timeoutCount[pointIndex] = static_cast<int>((distance / MOTOR_SPEED_MMS + TIMEOUT_SAFETY_S) / 2.0);
    m_Move.startPosition[pointIndex] = PositionReadNP[pointIndex].getValue();

    sendPointCommand(pointIndex, target);
    LOGF_INFO("Moving Point %d to %.3f mm", pointIndex + 1, target);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Start a multi-point move (non-blocking, sequential)
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererETA::startMultiPointMove(double targets[3], bool isZeroAll, bool isApplyOffset)
{
    m_Move.active = true;
    m_Move.isZeroAll = isZeroAll;
    m_Move.isApplyOffset = isApplyOffset;
    m_Move.currentPoint = 0;
    m_Move.graceCount = 3;

    for (int i = 0; i < 3; i++)
    {
        m_Move.target[i] = targets[i];
        m_Move.pending[i] = true;
        m_Move.startPosition[i] = PositionReadNP[i].getValue();
        double distance = std::abs(targets[i] - m_Move.startPosition[i]);
        m_Move.timeoutCount[i] = static_cast<int>((distance / MOTOR_SPEED_MMS + TIMEOUT_SAFETY_S) / 2.0);
    }

    // Set all target properties to BUSY
    setPositionState(0, IPS_BUSY);
    setPositionState(1, IPS_BUSY);
    setPositionState(2, IPS_BUSY);

    // Send command for first point
    sendPointCommand(0, targets[0]);
    LOGF_INFO("Moving Point 1 to %.3f mm", targets[0]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Send a move command for a single point
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererETA::sendPointCommand(int pointIndex, double target)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "%d%.3f\n", pointIndex + 1, target);

    // Flush input buffer to ensure clean command delivery.
    // The device streams status continuously; we need a clear window to send.
    tcflush(PortFD, TCIFLUSH);
    usleep(50000);  // 50ms settling time

    int nbytes_written = 0, rc = -1;
    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error on Point %d: %s", pointIndex + 1, errorMessage);
        m_Move.pending[pointIndex] = false;
        setPositionState(pointIndex, IPS_ALERT);
    }
    tcdrain(PortFD);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Check move progress (called from TimerHit every polling cycle)
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererETA::checkMoveProgress()
{
    if (!m_Move.active)
        return;

    // Grace period: skip first few polls after a command to allow device to respond
    if (m_Move.graceCount > 0)
    {
        m_Move.graceCount--;
        return;
    }

    bool allDone = true;

    for (int i = 0; i < 3; i++)
    {
        if (!m_Move.pending[i])
            continue;

        allDone = false;
        double currentPos = PositionReadNP[i].getValue();

        // Check if target reached (device reported updated position after motor stopped)
        if (std::abs(currentPos - m_Move.target[i]) < POSITION_TOLERANCE)
        {
            m_Move.pending[i] = false;
            setPositionState(i, IPS_OK);
            LOGF_INFO("Point %d reached target %.3f mm", i + 1, m_Move.target[i]);

            // For multi-point sequential moves: start next point
            if ((m_Move.isZeroAll || m_Move.isApplyOffset) && i == m_Move.currentPoint)
            {
                m_Move.currentPoint++;
                if (m_Move.currentPoint < 3 && m_Move.pending[m_Move.currentPoint])
                {
                    sendPointCommand(m_Move.currentPoint, m_Move.target[m_Move.currentPoint]);
                    m_Move.graceCount = 3;
                    double dist = std::abs(m_Move.target[m_Move.currentPoint] - m_Move.startPosition[m_Move.currentPoint]);
                    m_Move.timeoutCount[m_Move.currentPoint] = static_cast<int>((dist / MOTOR_SPEED_MMS + TIMEOUT_SAFETY_S) / 2.0);
                    LOGF_INFO("Moving Point %d to %.3f mm", m_Move.currentPoint + 1,
                              m_Move.target[m_Move.currentPoint]);
                    return;
                }
            }
            continue;
        }

        // Timeout: count down polls until expected travel time is exceeded
        if (m_Move.timeoutCount[i] > 0)
        {
            m_Move.timeoutCount[i]--;
            continue;
        }

        // Timeout expired — motor should be done; accept current position
        double finalPos = PositionReadNP[i].getValue();
        m_Move.pending[i] = false;
        if (std::abs(finalPos - m_Move.target[i]) < POSITION_TOLERANCE)
        {
            setPositionState(i, IPS_OK);
            LOGF_INFO("Point %d reached target %.3f mm", i + 1, m_Move.target[i]);
        }
        else
        {
            LOGF_WARN("Point %d timeout: reported %.3f mm (target %.3f mm) — motor likely completed, verify by reconnecting",
                      i + 1, finalPos, m_Move.target[i]);
            setPositionState(i, IPS_OK);
        }

        // For multi-point: continue with next point
        if ((m_Move.isZeroAll || m_Move.isApplyOffset) && i == m_Move.currentPoint)
        {
            m_Move.currentPoint++;
            if (m_Move.currentPoint < 3 && m_Move.pending[m_Move.currentPoint])
            {
                sendPointCommand(m_Move.currentPoint, m_Move.target[m_Move.currentPoint]);
                m_Move.graceCount = 3;
                double dist = std::abs(m_Move.target[m_Move.currentPoint] - m_Move.startPosition[m_Move.currentPoint]);
                m_Move.timeoutCount[m_Move.currentPoint] = static_cast<int>((dist / MOTOR_SPEED_MMS + TIMEOUT_SAFETY_S) / 2.0);
                LOGF_INFO("Moving Point %d to %.3f mm", m_Move.currentPoint + 1,
                          m_Move.target[m_Move.currentPoint]);
                return;
            }
        }
    }

    // Check if all points are done
    if (allDone || (!m_Move.pending[0] && !m_Move.pending[1] && !m_Move.pending[2]))
    {
        m_Move.active = false;

        if (m_Move.isZeroAll)
        {
            updateAllTargets(m_Move.target);
            LOG_INFO("All points moved to zero position successfully");
            ZeroAllSP.setState(IPS_OK);
            ZeroAllSP[ZERO_ALL].setState(ISS_OFF);
            ZeroAllSP.apply();
        }
        else if (m_Move.isApplyOffset)
        {
            updateAllTargets(m_Move.target);
            BackfocusOffsetNP[0].setValue(0.0);
            BackfocusOffsetNP.setState(IPS_OK);
            BackfocusOffsetNP.apply();
            LOGF_INFO("Backfocus offset applied successfully. New positions: P1=%.3f, P2=%.3f, P3=%.3f",
                      m_Move.target[0], m_Move.target[1], m_Move.target[2]);
            ApplyOffsetSP.setState(IPS_OK);
            ApplyOffsetSP[APPLY_OFFSET].setState(ISS_OFF);
            ApplyOffsetSP.apply();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Update all 3 target position properties after a successful multi-point move
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererETA::updateAllTargets(double targets[3])
{
    Position1NP[0].setValue(targets[0]);
    Position1NP.setState(IPS_OK);
    Position1NP.apply();
    Position2NP[0].setValue(targets[1]);
    Position2NP.setState(IPS_OK);
    Position2NP.apply();
    Position3NP[0].setValue(targets[2]);
    Position3NP.setState(IPS_OK);
    Position3NP.apply();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Send command to device (thread-safe)
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererETA::sendCommand(std::string command)
{
    std::lock_guard<std::timed_mutex> lock(serialPortMutex);

    int nbytes_written = 0, rc = -1;
    std::string command_termination = "\n";
    LOGF_DEBUG("CMD: %s", command.c_str());

    // Flush any pending input data before sending command.
    // The device streams status continuously; we need a clean state.
    tcflush(PortFD, TCIFLUSH);

    if ((rc = tty_write_string(PortFD, (command + command_termination).c_str(), &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    // Ensure the data is fully transmitted before releasing the port
    tcdrain(PortFD);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Timer callback - poll device status
////////////////////////////////////////////////////////////////////////////////////////////////////////
void WandererETA::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getPollingPeriod());
        return;
    }

    // Clear initialization flag on first timer tick — by now config loading is complete
    if (m_Initializing)
        m_Initializing = false;

    getData();

    // Check if any moves are in progress and update their status
    checkMoveProgress();

    SetTimer(getPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Save config items
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WandererETA::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    Position1NP.save(fp);
    Position2NP.save(fp);
    Position3NP.save(fp);
    return true;
}
