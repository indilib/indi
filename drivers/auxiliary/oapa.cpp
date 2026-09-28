/*******************************************************************************
  OAPA - Open Automatic Polar Alignment

  Copyright (C) 2026 Michele Bergo

  OAPA wire protocol (GRBL-style text lines, 115200 8N1).

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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*******************************************************************************/

#include "oapa.h"

#include "indicom.h"
#include "indilogger.h"
#include "connectionplugins/connectionserial.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>

static std::unique_ptr<OAPA> oapa(new OAPA());

// Firmware axis letters, indexed by AXIS_AZ / AXIS_ALT: X drives azimuth, Y altitude.
static constexpr char AXIS_LETTER[2] = {'X', 'Y'};

static constexpr const char *MOTOR_TAB = "Motor";

OAPA::OAPA()
    : PACInterface(this)
{
    setVersion(3, 0);
    SetCapability(PAC_CAN_REVERSE | PAC_HAS_POSITION);
}

const char *OAPA::getDefaultName()
{
    return "OAPA";
}

bool OAPA::initProperties()
{
    INDI::DefaultDevice::initProperties();

    PACI::initProperties(MAIN_CONTROL_TAB);

    // Speed per axis, sent as the F feed of each jog (the firmware clamps it to 50-3000).
    AxisSpeedNP[AXIS_AZ].fill("X_SPEED", "Azimuth (steps/s)", "%.0f", MIN_SPEED, MAX_SPEED, 50, DEFAULT_SPEED);
    AxisSpeedNP[AXIS_ALT].fill("Y_SPEED", "Altitude (steps/s)", "%.0f", MIN_SPEED, MAX_SPEED, 50, DEFAULT_SPEED);
    AxisSpeedNP.fill(getDeviceName(), "OAPA_SPEED", "Speed", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Calibration: motor steps per arcminute of correction. No default can be right,
    // since gear reductions range from ~15 to ~1000 steps/arcmin, so moves are refused
    // until the user sets it.
    StepsPerArcminNP[AXIS_AZ].fill("STEPS_AZ", "Azimuth (steps/arcmin)", "%.2f", 0, 100000, 1, 0);
    StepsPerArcminNP[AXIS_ALT].fill("STEPS_ALT", "Altitude (steps/arcmin)", "%.2f", 0, 100000, 1, 0);
    StepsPerArcminNP.fill(getDeviceName(), "OAPA_STEPS_PER_ARCMIN", "Calibration", MAIN_CONTROL_TAB, IP_RW, 60,
                          IPS_IDLE);

    // Driver currents. Defaults match the firmware's own, so a controller that has just
    // been powered on is left as it is until the user edits them.
    MotorCurrentNP[AXIS_AZ].fill("X_CURRENT", "Azimuth run (mA)", "%.0f", MIN_RUN_CURRENT, MAX_RUN_CURRENT, 50,
                                 DEFAULT_RUN_CURRENT);
    MotorCurrentNP[AXIS_ALT].fill("Y_CURRENT", "Altitude run (mA)", "%.0f", MIN_RUN_CURRENT, MAX_RUN_CURRENT, 50,
                                  DEFAULT_RUN_CURRENT);
    MotorCurrentNP.fill(getDeviceName(), "OAPA_MOTOR_CURRENT", "Run Current", MOTOR_TAB, IP_RW, 60, IPS_IDLE);

    MotorHoldNP[AXIS_AZ].fill("X_HOLD", "Azimuth hold (% of run)", "%.0f", 0, 100, 5, DEFAULT_HOLD_PERCENT);
    MotorHoldNP[AXIS_ALT].fill("Y_HOLD", "Altitude hold (% of run)", "%.0f", 0, 100, 5, DEFAULT_HOLD_PERCENT);
    MotorHoldNP.fill(getDeviceName(), "OAPA_MOTOR_HOLD", "Hold Current", MOTOR_TAB, IP_RW, 60, IPS_IDLE);

    FirmwareTP[0].fill("VERSION", "Version", "Unknown");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", INFO_TAB, IP_RO, 60, IPS_IDLE);

    setDriverInterface(AUX_INTERFACE | PAC_INTERFACE);

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    registerConnection(serialConnection);

    addAuxControls();
    return true;
}

bool OAPA::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(AxisSpeedNP);
        defineProperty(StepsPerArcminNP);
        defineProperty(MotorCurrentNP);
        defineProperty(MotorHoldNP);
        defineProperty(FirmwareTP);
    }
    else
    {
        deleteProperty(AxisSpeedNP);
        deleteProperty(StepsPerArcminNP);
        deleteProperty(MotorCurrentNP);
        deleteProperty(MotorHoldNP);
        deleteProperty(FirmwareTP);
    }

    PACI::updateProperties();
    return true;
}

bool OAPA::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    AxisSpeedNP.save(fp);
    StepsPerArcminNP.save(fp);
    MotorCurrentNP.save(fp);
    MotorHoldNP.save(fp);
    PACI::saveConfigItems(fp);
    return true;
}

bool OAPA::Handshake()
{
    PortFD = serialConnection->getPortFD();

    // Opening the port resets most ESP32 boards: the firmware prints a boot banner
    // and then waits silently. Probe until a status frame answers.
    Status status;
    bool found = false;
    for (int attempt = 0; attempt < HANDSHAKE_ATTEMPTS && !found; attempt++)
    {
        found = queryStatus(status);
        if (!found)
            usleep(500000);
    }

    if (!found)
    {
        LOG_ERROR("No OAPA status frame received. Check the port and that the OAPA firmware is flashed.");
        return false;
    }

    FirmwareTP[0].setText(status.version[0] ? status.version : "Unknown");

    int major = 0, minor = 0, patch = 0;
    const bool versioned = sscanf(status.version, "%d.%d.%d", &major, &minor, &patch) >= 2;
    if (!versioned || major < 1 || (major == 1 && (minor < 2 || (minor == 2 && patch < 1))))
        LOGF_WARN("Firmware %s predates 1.2.1: speed setting and Abort are not supported. Please update the firmware.",
                  status.version[0] ? status.version : "without version");

    m_MoveActive = false;
    updatePosition(status);

    if (!pushDriverConfig())
        LOG_WARN("Some motor driver settings were not acknowledged by the controller.");

    LOGF_INFO("OAPA connected (firmware %s).", FirmwareTP[0].getText());
    return true;
}

void OAPA::TimerHit()
{
    if (!isConnected())
        return;

    Status status;
    if (queryStatus(status))
    {
        updatePosition(status);
        checkMove(status);
    }

    // The deadline also covers a link that stopped answering mid-move.
    if (m_MoveActive && Clock::now() > m_Deadline)
    {
        char res[DRIVER_LEN] = {0};
        sendCommand("!", res);
        finishMove(IPS_ALERT, "Move timed out and was stopped.");
    }

    SetTimer(getCurrentPollingPeriod());
}

///////////////////////////////////////////////////////////////////////////////
/// Motion
///////////////////////////////////////////////////////////////////////////////
IPState OAPA::MoveAZ(double degrees)
{
    return startMove(degrees, 0);
}

IPState OAPA::MoveALT(double degrees)
{
    return startMove(0, degrees);
}

IPState OAPA::MoveBoth(double azDegrees, double altDegrees)
{
    return startMove(azDegrees, altDegrees);
}

IPState OAPA::startMove(double azDegrees, double altDegrees)
{
    if (m_MoveActive)
    {
        LOG_ERROR("A move is already in progress. Abort it before starting another.");
        return IPS_ALERT;
    }

    const double azSteps  = degreesToSteps(azDegrees, AXIS_AZ);
    const double altSteps = degreesToSteps(altDegrees, AXIS_ALT);
    if (std::isnan(azSteps) || std::isnan(altSteps))
        return IPS_ALERT;

    const long x = std::lround(azSteps);
    const long y = std::lround(altSteps);
    if (x == 0 && y == 0)
    {
        LOG_INFO("Requested correction is smaller than one motor step. Nothing to move.");
        return IPS_OK;
    }

    // Read the starting position fresh, so the target is exact.
    Status status;
    if (!queryStatus(status))
    {
        LOG_ERROR("Cannot read platform status before moving.");
        return IPS_ALERT;
    }
    if (status.moving)
    {
        LOG_ERROR("Platform is already moving. Abort it before starting a correction.");
        return IPS_ALERT;
    }

    const int speedAz  = static_cast<int>(AxisSpeedNP[AXIS_AZ].getValue());
    const int speedAlt = static_cast<int>(AxisSpeedNP[AXIS_ALT].getValue());

    // The firmware takes one feed per jog command. With equal speeds both axes share one
    // command; otherwise each axis gets its own jog with its own feed.
    char cmds[2][DRIVER_LEN] = {{0}};
    int count = 0;
    if (x != 0 && y != 0 && speedAz == speedAlt)
        snprintf(cmds[count++], DRIVER_LEN, "$J=G91G21X%ldY%ldF%d", x, y, speedAz);
    else
    {
        if (x != 0)
            snprintf(cmds[count++], DRIVER_LEN, "$J=G91G21X%ldF%d", x, speedAz);
        if (y != 0)
            snprintf(cmds[count++], DRIVER_LEN, "$J=G91G21Y%ldF%d", y, speedAlt);
    }

    for (int i = 0; i < count; i++)
    {
        char res[DRIVER_LEN] = {0};
        if (!sendCommand(cmds[i], res) || strncmp(res, "ok", 2) != 0)
        {
            LOGF_ERROR("Jog command %s not acknowledged: %s", cmds[i], res);
            // An axis that already started must not keep moving on its own.
            if (i > 0)
            {
                char stopRes[DRIVER_LEN] = {0};
                sendCommand("!", stopRes);
            }
            return IPS_ALERT;
        }
    }

    // Travel time of the slower axis plus its acceleration ramp, doubled, plus slack.
    double travel = 0;
    if (x != 0)
        travel = std::max(travel, std::labs(x) / static_cast<double>(speedAz) + speedAz / ACCELERATION);
    if (y != 0)
        travel = std::max(travel, std::labs(y) / static_cast<double>(speedAlt) + speedAlt / ACCELERATION);

    const auto now = Clock::now();
    m_Deadline = now + std::chrono::milliseconds(static_cast<long>((2 * travel + 10) * 1000));
    m_LastProgress = now;
    m_LastX = status.x;
    m_LastY = status.y;
    m_TargetX = status.x + x;
    m_TargetY = status.y + y;
    m_MoveActive = true;

    LOGF_INFO("Correction: AZ %+.4f deg (%ld steps at %d steps/s), ALT %+.4f deg (%ld steps at %d steps/s).",
              azDegrees, x, speedAz, altDegrees, y, speedAlt);
    return IPS_BUSY;
}

double OAPA::degreesToSteps(double degrees, int axis)
{
    if (degrees == 0)
        return 0;

    const double stepsPerArcmin = StepsPerArcminNP[axis].getValue();
    if (stepsPerArcmin <= 0)
    {
        LOGF_ERROR("%s is not calibrated. Set Calibration (steps/arcmin) before correcting.",
                   axis == AXIS_AZ ? "Azimuth" : "Altitude");
        return NAN;
    }

    return degrees * 60.0 * stepsPerArcmin * (isReversed(axis) ? -1 : 1);
}

bool OAPA::isReversed(int axis)
{
    // Read the switch itself rather than a cached flag, so a value restored from the
    // config file is honoured however it was applied.
    auto &sp = (axis == AXIS_AZ) ? AZReverseSP : ALTReverseSP;
    return sp[INDI_ENABLED].getState() == ISS_ON;
}

void OAPA::updatePosition(const Status &status)
{
    const double azScale  = StepsPerArcminNP[AXIS_AZ].getValue() * 60.0;
    const double altScale = StepsPerArcminNP[AXIS_ALT].getValue() * 60.0;

    PositionNP[POSITION_AZ].setValue(azScale > 0 ? status.x / azScale * (isReversed(AXIS_AZ) ? -1 : 1) : 0);
    PositionNP[POSITION_ALT].setValue(altScale > 0 ? status.y / altScale * (isReversed(AXIS_ALT) ? -1 : 1) : 0);
    PositionNP.setState(status.moving ? IPS_BUSY : IPS_OK);
    PositionNP.apply();
}

void OAPA::checkMove(const Status &status)
{
    if (!m_MoveActive)
        return;

    const auto now = Clock::now();

    if (status.moving)
    {
        if (std::fabs(status.x - m_LastX) >= POSITION_TOLERANCE || std::fabs(status.y - m_LastY) >= POSITION_TOLERANCE)
        {
            m_LastX = status.x;
            m_LastY = status.y;
            m_LastProgress = now;
        }
        else if (std::chrono::duration<double>(now - m_LastProgress).count() > STALL_SECONDS)
        {
            char res[DRIVER_LEN] = {0};
            sendCommand("!", res);
            finishMove(IPS_ALERT, "Platform reports motion but its position is not changing. Stopped.");
        }
        return;
    }

    if (std::fabs(status.x - m_TargetX) <= POSITION_TOLERANCE && std::fabs(status.y - m_TargetY) <= POSITION_TOLERANCE)
        finishMove(IPS_OK, "Correction move complete.");
    else
    {
        LOGF_WARN("Platform stopped at (%.0f, %.0f) steps, target was (%.0f, %.0f).",
                  status.x, status.y, m_TargetX, m_TargetY);
        finishMove(IPS_ALERT, "Move ended before reaching the target.");
    }
}

void OAPA::finishMove(IPState state, const char *message)
{
    m_MoveActive = false;
    ManualAdjustmentNP.setState(state);
    ManualAdjustmentNP.apply();
    if (state == IPS_OK)
        LOGF_INFO("%s", message);
    else
        LOGF_ERROR("%s", message);
}

bool OAPA::AbortMotion()
{
    // A stale move must not complete as OK after an abort.
    m_MoveActive = false;

    char res[DRIVER_LEN] = {0};
    if (!sendCommand("!", res))
        return false;

    LOG_INFO("OAPA motion aborted.");
    return true;
}

bool OAPA::ReverseAZ(bool enabled)
{
    LOGF_INFO("Azimuth direction reverse %s.", enabled ? "enabled" : "disabled");
    return true;
}

bool OAPA::ReverseALT(bool enabled)
{
    LOGF_INFO("Altitude direction reverse %s.", enabled ? "enabled" : "disabled");
    return true;
}

bool OAPA::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (AxisSpeedNP.isNameMatch(name))
        {
            AxisSpeedNP.update(values, names, n);
            AxisSpeedNP.setState(IPS_OK);
            AxisSpeedNP.apply();
            saveConfig(AxisSpeedNP);
            return true;
        }

        if (StepsPerArcminNP.isNameMatch(name))
        {
            if (m_MoveActive)
            {
                LOG_ERROR("Cannot change calibration while a move is in progress.");
                StepsPerArcminNP.setState(IPS_ALERT);
                StepsPerArcminNP.apply();
                return true;
            }
            StepsPerArcminNP.update(values, names, n);
            StepsPerArcminNP.setState(IPS_OK);
            StepsPerArcminNP.apply();
            saveConfig(StepsPerArcminNP);
            return true;
        }

        if (MotorCurrentNP.isNameMatch(name) || MotorHoldNP.isNameMatch(name))
        {
            const bool isRun = MotorCurrentNP.isNameMatch(name);
            auto &np = isRun ? MotorCurrentNP : MotorHoldNP;
            np.update(values, names, n);

            bool ok = true;
            if (isConnected())
            {
                for (int axis = AXIS_AZ; axis <= AXIS_ALT; axis++)
                    ok = sendDriverSetting(isRun ? 'C' : 'H', axis, np[axis].getValue()) && ok;
            }
            np.setState(ok ? IPS_OK : IPS_ALERT);
            np.apply();
            saveConfig(np);
            return true;
        }

        if (PACI::processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool OAPA::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (PACI::processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

///////////////////////////////////////////////////////////////////////////////
/// Driver settings
///////////////////////////////////////////////////////////////////////////////
bool OAPA::pushDriverConfig()
{
    // The controller forgets driver settings at power-off, so they are sent on every
    // connection: run then hold, azimuth first, as the N.I.N.A. plugin does.
    bool ok = true;
    for (int axis = AXIS_AZ; axis <= AXIS_ALT; axis++)
    {
        ok = sendDriverSetting('C', axis, MotorCurrentNP[axis].getValue()) && ok;
        ok = sendDriverSetting('H', axis, MotorHoldNP[axis].getValue()) && ok;
    }
    return ok;
}

bool OAPA::sendDriverSetting(char type, int axis, double value)
{
    // Type letter first, then the axis: "CX600", "HY25". The firmware acknowledges any
    // other shape (e.g. "XC600") with "ok" and ignores it, so the order matters.
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "%c%c%ld", type, AXIS_LETTER[axis], std::lround(value));

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;
    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_WARN("Unexpected reply to %s: %s", cmd, res);
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
/// Communication
///////////////////////////////////////////////////////////////////////////////
bool OAPA::sendCommand(const char *cmd, char *res)
{
    int nbytes_written = 0, nbytes_read = 0;

    // Drop anything unsolicited (boot banner, homing chatter) so the reply we read
    // belongs to this command.
    tcflush(PortFD, TCIFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);
    char formatted[DRIVER_LEN] = {0};
    snprintf(formatted, DRIVER_LEN, "%s\n", cmd);
    int rc = tty_write_string(PortFD, formatted, &nbytes_written);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    res[std::min(nbytes_read, DRIVER_LEN - 1)] = '\0';
    res[strcspn(res, "\r\n")] = '\0';
    LOGF_DEBUG("RES <%s>", res);
    return true;
}

bool OAPA::queryStatus(Status &status)
{
    char line[DRIVER_LEN] = {0};
    if (!sendCommand("?", line))
        return false;

    // The first line may still be a banner line printed just before the query.
    int nbytes_read = 0;
    for (int extra = 0; !parseStatus(line, status); extra++)
    {
        if (extra >= 4 || tty_nread_section(PortFD, line, DRIVER_LEN, DRIVER_STOP_CHAR, 1, &nbytes_read) != TTY_OK)
            return false;
        line[std::min(nbytes_read, DRIVER_LEN - 1)] = '\0';
        line[strcspn(line, "\r\n")] = '\0';
    }

    // "?" always answers with two lines. Consume the "ok" so it cannot be read as the
    // reply to the next command.
    char ok[DRIVER_LEN] = {0};
    if (tty_nread_section(PortFD, ok, DRIVER_LEN, DRIVER_STOP_CHAR, 1, &nbytes_read) != TTY_OK)
        LOG_DEBUG("Status frame was not followed by ok.");

    return true;
}

bool OAPA::parseStatus(const char *line, Status &status)
{
    const char *start = strchr(line, '<');
    if (start == nullptr)
        return false;

    const char *mpos = strstr(start, "|MPos:");
    double x = 0, y = 0;
    if (mpos == nullptr || sscanf(mpos + 6, "%lf,%lf", &x, &y) != 2)
        return false;

    status.moving = strncmp(start + 1, "Idle", 4) != 0;
    status.x = x;
    status.y = y;
    status.version[0] = '\0';

    const char *version = strstr(start, "|V:");
    if (version != nullptr)
        sscanf(version + 3, "%15[0-9.]", status.version);

    return true;
}
