/*******************************************************************************
  Avalon Instruments Universal Polar Alignment System (UPAS)

  Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

  GRBL-based serial protocol.

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

#include "avalon_upas.h"

#include "indicom.h"
#include "indilogger.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <termios.h>
#include <regex>

static std::unique_ptr<AvalonUPAS> upas(new AvalonUPAS());

AvalonUPAS::AvalonUPAS()
    : PACInterface(this)
{
    setVersion(1, 3);
    SetCapability(PAC_HAS_SPEED | PAC_CAN_REVERSE | PAC_HAS_POSITION | PAC_HAS_BACKLASH);
}

const char *AvalonUPAS::getDefaultName()
{
    return "Avalon UPAS";
}

bool AvalonUPAS::initProperties()
{
    INDI::DefaultDevice::initProperties();

    PACI::initProperties(MAIN_CONTROL_TAB);

    // Speed: GRBL feed rate in mm/min, range 100–1000 as before. It caps the feed rate of both axes;
    // the default 1000 leaves Avalon's per-axis speeds below unchanged.
    SpeedNP[0].setLabel("Max (mm/min)");
    SpeedNP[0].setMin(100);
    SpeedNP[0].setMax(1000);
    SpeedNP[0].setStep(100);
    SpeedNP[0].setValue(1000);

    // Feed rate per axis in mm/min (GRBL F word). Defaults are Avalon's suggested speeds (AZ 1000, ALT 700).
    FeedRateNP[FEED_AZ].fill("FEED_AZ", "Azimuth (mm/min)", "%.0f", 10, 1000, 50, 1000);
    FeedRateNP[FEED_ALT].fill("FEED_ALT", "Altitude (mm/min)", "%.0f", 10, 1000, 50, 700);
    FeedRateNP.fill(getDeviceName(), "PAC_FEED_RATE", "Feed Rate", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Minimum move: smaller moves of an axis are skipped (0 disables). Clients can send tiny corrections,
    // which are lost in the backlash anyway but would run the full backlash compensation.
    MinMoveNP[0].fill("MIN_MOVE", "Minimum (arcmin)", "%.2f", 0, 10, 0.05, 0.1);
    MinMoveNP.fill(getDeviceName(), "PAC_MIN_MOVE", "Min. Move", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Gear ratio: mm of motor travel per arcminute of polar-axis correction.
    // Defaults are the Avalon specifications (AZ 2, ALT 22), as also used by Avalon's own software.
    GearRatioNP[GEAR_AZ].fill("GEAR_AZ", "Azimuth (mm/arcmin)", "%.4f", 0.001, 1000, 0.1, 2.0);
    GearRatioNP[GEAR_ALT].fill("GEAR_ALT", "Altitude (mm/arcmin)", "%.4f", 0.001, 1000, 0.1, 22.0);
    GearRatioNP.fill(getDeviceName(), "PAC_GEAR_RATIO", "Gear Ratio", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Backlash: compensated in software, so use arcminutes instead of the interface's motor steps.
    BacklashNP[BACKLASH_AZ].fill("BACKLASH_AZ", "Azimuth (arcmin)", "%.2f", 0, 60, 0.1, 0);
    BacklashNP[BACKLASH_ALT].fill("BACKLASH_ALT", "Altitude (arcmin)", "%.2f", 0, 60, 0.1, 0);
    BacklashNP.setLabel("Backlash");

    // Final approach direction per axis. ALT usually works best when the last move lifts against gravity.
    AZApproachSP[APPROACH_POSITIVE].fill("APPROACH_POSITIVE", "+", ISS_ON);
    AZApproachSP[APPROACH_NEGATIVE].fill("APPROACH_NEGATIVE", "-", ISS_OFF);
    AZApproachSP.fill(getDeviceName(), "PAC_AZ_APPROACH", "Azimuth Approach", MAIN_CONTROL_TAB, IP_RW,
                      ISR_1OFMANY, 60, IPS_IDLE);
    ALTApproachSP[APPROACH_POSITIVE].fill("APPROACH_POSITIVE", "+", ISS_ON);
    ALTApproachSP[APPROACH_NEGATIVE].fill("APPROACH_NEGATIVE", "-", ISS_OFF);
    ALTApproachSP.fill(getDeviceName(), "PAC_ALT_APPROACH", "Altitude Approach", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    // Unlock: clears a GRBL alarm ($X) without a power cycle.
    UnlockSP[0].fill("UNLOCK", "Unlock", ISS_OFF);
    UnlockSP.fill(getDeviceName(), "PAC_UNLOCK", "Alarm", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Firmware (read-only, populated in Handshake)
    FirmwareTP[0].fill("VERSION", "Version", "Unknown");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", INFO_TAB, IP_RO, 60, IPS_IDLE);

    setDriverInterface(AUX_INTERFACE | PAC_INTERFACE);

    // Serial connection
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

bool AvalonUPAS::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(GearRatioNP);
        defineProperty(FeedRateNP);
        defineProperty(MinMoveNP);
        defineProperty(UnlockSP);
        defineProperty(FirmwareTP);
    }
    else
    {
        deleteProperty(GearRatioNP);
        deleteProperty(FeedRateNP);
        deleteProperty(MinMoveNP);
        deleteProperty(UnlockSP);
        deleteProperty(FirmwareTP);
    }

    PACI::updateProperties();

    // Define after the PAC properties so the approach switches follow the backlash controls.
    if (isConnected())
    {
        defineProperty(AZApproachSP);
        defineProperty(ALTApproachSP);
    }
    else
    {
        deleteProperty(AZApproachSP);
        deleteProperty(ALTApproachSP);
    }

    return true;
}

bool AvalonUPAS::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    GearRatioNP.save(fp);
    FeedRateNP.save(fp);
    MinMoveNP.save(fp);
    AZApproachSP.save(fp);
    ALTApproachSP.save(fp);
    PACI::saveConfigItems(fp);
    return true;
}

bool AvalonUPAS::Handshake()
{
    PortFD = serialConnection->getPortFD();

    // Flush any GRBL boot message
    tcflush(PortFD, TCIOFLUSH);

    // Send soft-reset so GRBL is in a known idle state.
    const char softReset = '\x18';
    int nbytes = 0;
    tty_write(PortFD, &softReset, 1, &nbytes);

    // Opening the port may also hard-reset the Arduino (DTR), and its bootloader swallows anything
    // sent before GRBL runs. Either way GRBL prints its "Grbl x.y" banner once it is ready, so wait
    // for that line instead of a fixed delay.
    if (!waitForBanner())
        LOG_WARN("No GRBL startup banner received, trying the status query anyway.");
    tcflush(PortFD, TCIOFLUSH);

    // Query status to confirm communication.
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("?", res))
        return false;

    if (strstr(res, "MPos") == nullptr && strstr(res, "WPos") == nullptr)
    {
        LOGF_ERROR("Unexpected handshake response: %s", res);
        return false;
    }

    // Parse position and state from the response.
    parseStatus(res);

    // Read the GRBL version with $I. It answers with "[VER:1.1f.20170801:]", "[OPT:...]" and "ok".
    sendLineCommand("$I", [this](const char *line)
    {
        // Extract up to the colon that separates the optional build info, or the closing bracket
        char version[64] = {0};
        if (sscanf(line, "[VER:%63[^]:]", version) == 1)
            FirmwareTP[0].setText(version);
    });

    readReverseFlags();

    LOG_INFO("UPAS connected and ready.");
    return true;
}

bool AvalonUPAS::waitForBanner()
{
    // Blank lines around the banner are skipped.
    for (int i = 0; i < 10; i++)
    {
        // Read one byte less than the buffer so the line always stays null-terminated.
        char line[DRIVER_LEN] = {0};
        int nbytes = 0;
        if (tty_nread_section(PortFD, line, DRIVER_LEN - 1, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes) != TTY_OK)
            return false;
        if (strstr(line, "Grbl") != nullptr)
            return true;
    }
    return false;
}

bool AvalonUPAS::readReverseFlags()
{
    // "$$" lists every GRBL setting as one "$n=value" line, followed by "ok". Read the listing to
    // its end, so none of its lines is left over as the reply to the next command.
    int mask = -1;
    const bool ok = sendLineCommand("$$", [&mask](const char *line)
    {
        sscanf(line, "$3=%d", &mask);
    });

    if (!ok || mask < 0)
    {
        LOG_WARN("Could not read the GRBL axis reverse setting ($3), assuming no axis is reversed.");
        return false;
    }

    m_ReverseFlags = static_cast<uint8_t>(mask & 0x03);
    AZReverseSP[INDI_ENABLED].setState((m_ReverseFlags & 0x01) ? ISS_ON : ISS_OFF);
    AZReverseSP[INDI_DISABLED].setState((m_ReverseFlags & 0x01) ? ISS_OFF : ISS_ON);
    ALTReverseSP[INDI_ENABLED].setState((m_ReverseFlags & 0x02) ? ISS_ON : ISS_OFF);
    ALTReverseSP[INDI_DISABLED].setState((m_ReverseFlags & 0x02) ? ISS_OFF : ISS_ON);
    LOGF_DEBUG("GRBL axis reverse mask $3=%d.", m_ReverseFlags);
    return true;
}

bool AvalonUPAS::unlock()
{
    // GRBL answers "[MSG:Caution: Unlocked]" and "ok", or an error if it cannot unlock (e.g. Door).
    if (!sendLineCommand("$X"))
    {
        LOG_ERROR("UPAS could not be unlocked. Check the device, then power-cycle it.");
        return false;
    }

    LOG_INFO("UPAS unlocked. A move interrupted by the alarm may have shifted the position.");

    // Show the new state right away instead of at the next poll.
    char res[DRIVER_LEN] = {0};
    if (sendCommand("?", res))
        parseStatus(res);
    return true;
}

void AvalonUPAS::TimerHit()
{
    if (!isConnected())
        return;

    char res[DRIVER_LEN] = {0};
    if (sendCommand("?", res))
        parseStatus(res);

    SetTimer(getCurrentPollingPeriod());
}

bool AvalonUPAS::parseStatus(const char *response)
{
    // GRBL status format: <State|MPos:x.xxx,y.xxx|...>
    // State can be: Idle, Jog, Run, Hold, Alarm, ...
    const char *mposTag = strstr(response, "MPos:");
    if (mposTag == nullptr)
        return false;

    double azMM = 0.0, altMM = 0.0;
    if (sscanf(mposTag + 5, "%lf,%lf", &azMM, &altMM) < 2)
        return false;

    const double gearAZ  = GearRatioNP[GEAR_AZ].getValue();
    const double gearALT = GearRatioNP[GEAR_ALT].getValue();

    PositionNP[POSITION_AZ].setValue(gearAZ  > 0 ? azMM  / (60 * gearAZ)  : 0);
    PositionNP[POSITION_ALT].setValue(gearALT > 0 ? altMM / (60 * gearALT) : 0);

    // The state is the first field, e.g. <Idle|...>, <Jog|...>, <Hold:0|...> or <Alarm|...>.
    char state[16] = {0};
    const char *start = strchr(response, '<');
    if (start != nullptr)
        sscanf(start, "<%15[^|:>]", state);

    // Alarm and Door stop GRBL until it is unlocked or reset. A feed hold pauses the move until GRBL
    // gets a cycle start, which the driver never sends, so it ends the move instead of keeping it busy.
    bool wasMoving = m_IsMoving;
    m_IsMoving = strcmp(state, "Jog") == 0 || strcmp(state, "Run") == 0;
    const bool alarm = strcmp(state, "Alarm") == 0;
    const bool door  = strcmp(state, "Door") == 0;
    const bool held  = strcmp(state, "Hold") == 0;

    if (alarm || door || held)
    {
        if (alarm && !m_IsFault)
            LOG_ERROR("UPAS reports an alarm and does not move. Check the device, then press Unlock.");
        else if (door && !m_IsFault)
            LOG_ERROR("UPAS reports state Door and does not move. Check the device, then power-cycle it.");
        else if (held && !m_IsHeld)
            LOG_WARN("UPAS motion is on feed hold. Abort discards the held move.");

        m_IsFault = alarm || door;
        m_IsHeld  = held;

        PositionNP.setState(IPS_ALERT);
        PositionNP.apply();

        if (ManualAdjustmentNP.getState() == IPS_BUSY)
        {
            ManualAdjustmentNP.setState(IPS_ALERT);
            ManualAdjustmentNP.apply();
        }
        return true;
    }
    m_IsFault = false;
    m_IsHeld  = false;

    PositionNP.setState(m_IsMoving ? IPS_BUSY : IPS_OK);
    PositionNP.apply();

    // Motion just completed?
    if (wasMoving && !m_IsMoving && ManualAdjustmentNP.getState() == IPS_BUSY)
    {
        ManualAdjustmentNP.setState(IPS_OK);
        ManualAdjustmentNP.apply();
        LOG_INFO("UPAS motion complete.");
    }

    return true;
}

bool AvalonUPAS::belowMinimum(const char *axis, double degrees)
{
    const double arcmin  = std::fabs(degrees) * 60;
    const double minimum = MinMoveNP[0].getValue();
    if (arcmin >= minimum)
        return false;

    LOGF_INFO("%s move of %.3f arcmin is below the minimum of %.2f arcmin, skipped.", axis, arcmin, minimum);
    return true;
}

IPState AvalonUPAS::MoveAZ(double degrees)
{
    // A skipped move is complete at once, so the client is not left waiting.
    if (belowMinimum("AZ", degrees))
        return IPS_OK;

    // The gear ratio is in mm/arcmin, so convert the requested degrees to arcminutes first.
    const double mm       = degrees * 60 * GearRatioNP[GEAR_AZ].getValue();
    const double feedRate = feedRateFor(mm, 0);

    IPState state = jogWithBacklash(mm, 0);
    if (state == IPS_BUSY)
        LOGF_INFO("AZ jog: %.4f deg → %.4f mm @ %.0f mm/min.", degrees, mm, feedRate);
    return state;
}

IPState AvalonUPAS::MoveALT(double degrees)
{
    if (belowMinimum("ALT", degrees))
        return IPS_OK;

    // The gear ratio is in mm/arcmin, so convert the requested degrees to arcminutes first.
    const double mm       = degrees * 60 * GearRatioNP[GEAR_ALT].getValue();
    const double feedRate = feedRateFor(0, mm);

    IPState state = jogWithBacklash(0, mm);
    if (state == IPS_BUSY)
        LOGF_INFO("ALT jog: %.4f deg → %.4f mm @ %.0f mm/min.", degrees, mm, feedRate);
    return state;
}

IPState AvalonUPAS::MoveBoth(double azDegrees, double altDegrees)
{
    // Skip an axis whose move is below the minimum; with both skipped the move is complete at once.
    if (belowMinimum("AZ", azDegrees))
        azDegrees = 0;
    if (belowMinimum("ALT", altDegrees))
        altDegrees = 0;
    if (azDegrees == 0 && altDegrees == 0)
        return IPS_OK;

    // GRBL supports multi-axis jog in a single command: X (AZ) and Y (ALT) together.
    // This moves both axes simultaneously, which is more efficient and accurate than
    // two separate jog commands.
    const double mmAZ    = azDegrees  * 60 * GearRatioNP[GEAR_AZ].getValue();
    const double mmALT   = altDegrees * 60 * GearRatioNP[GEAR_ALT].getValue();
    const double feedRate = feedRateFor(mmAZ, mmALT);

    IPState state = jogWithBacklash(mmAZ, mmALT);
    if (state == IPS_BUSY)
        LOGF_INFO("MoveBoth: AZ %.4f° (%.4f mm), ALT %.4f° (%.4f mm) @ %.0f mm/min.",
                  azDegrees, mmAZ, altDegrees, mmALT, feedRate);
    return state;
}

IPState AvalonUPAS::jogWithBacklash(double azMM, double altMM)
{
    // Never turn a NaN or infinite value into a GRBL jog command.
    if (!std::isfinite(azMM) || !std::isfinite(altMM))
    {
        LOG_ERROR("Invalid move request, ignored.");
        return IPS_ALERT;
    }

    const double azApproach  = finalApproachMM(BACKLASH_AZ, azMM);
    const double altApproach = finalApproachMM(BACKLASH_ALT, altMM);

    if (!sendJog(azMM, altMM))
        return IPS_ALERT;

    // GRBL queues jogs in its planner, so the final approach runs right after the main jog.
    if (azApproach != 0 || altApproach != 0)
    {
        if (!sendJog(azApproach, altApproach))
        {
            AbortMotion();
            return IPS_ALERT;
        }

        LOGF_INFO("Backlash: overshoot and final approach AZ %.4f mm, ALT %.4f mm.", azApproach, altApproach);
    }

    m_IsMoving = true;
    return IPS_BUSY;
}

double AvalonUPAS::finalApproachMM(int axis, double &mm)
{
    if (BacklashSP[DefaultDevice::INDI_ENABLED].getState() != ISS_ON)
        return 0;

    // BACKLASH_AZ/ALT and GEAR_AZ/ALT share the same indices.
    const double backlashMM = BacklashNP[axis].getValue() * GearRatioNP[axis].getValue();
    const auto &approachSP  = (axis == BACKLASH_AZ) ? AZApproachSP : ALTApproachSP;
    const double direction  = approachSP[APPROACH_NEGATIVE].getState() == ISS_ON ? -1 : 1;

    // Moves that already end in the approach direction need no compensation.
    if (backlashMM <= 0 || mm * direction >= 0)
        return 0;

    mm -= direction * backlashMM;
    return direction * backlashMM;
}

bool AvalonUPAS::sendJog(double azMM, double altMM)
{
    char cmd[DRIVER_LEN] = {0};
    int len = snprintf(cmd, DRIVER_LEN, "$J=G91G21");
    if (azMM != 0)
        len += snprintf(cmd + len, DRIVER_LEN - len, "X%.4f", azMM);
    if (altMM != 0)
        len += snprintf(cmd + len, DRIVER_LEN - len, "Y%.4f", altMM);
    snprintf(cmd + len, DRIVER_LEN - len, " F%.0f", feedRateFor(azMM, altMM));

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;

    // GRBL acknowledges jog commands with "ok"
    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("Unexpected jog response to %s: %s", cmd, res);
        return false;
    }

    return true;
}

double AvalonUPAS::feedRateFor(double azMM, double altMM)
{
    // Each axis runs at most at its own feed rate, capped by the interface's speed.
    const double maxRate = SpeedNP[0].getValue();
    const double azRate  = std::min(maxRate, FeedRateNP[FEED_AZ].getValue());
    const double altRate = std::min(maxRate, FeedRateNP[FEED_ALT].getValue());

    // GRBL applies one feed rate to the whole jog vector. Choose it so that each axis moves
    // at most at its rate; the limiting axis then moves at exactly that rate.
    const double length = std::hypot(azMM, altMM);
    if (length == 0)
        return azRate;

    double feedRate = std::numeric_limits<double>::max();
    if (azMM != 0)
        feedRate = std::min(feedRate, azRate * length / std::fabs(azMM));
    if (altMM != 0)
        feedRate = std::min(feedRate, altRate * length / std::fabs(altMM));
    return feedRate;
}

bool AvalonUPAS::SetPACSpeed(uint16_t speed)
{
    // The feed rate is computed for every jog, so there is nothing to send now.
    LOGF_INFO("UPAS speed limited to %u mm/min.", speed);
    return true;
}

bool AvalonUPAS::AbortMotion()
{
    // GRBL real-time jog cancel command (single byte 0x85, no newline needed). GRBL ignores it
    // during a feed hold; a soft reset (0x18) then discards the held move and keeps the position.
    const bool held = m_IsHeld;
    const char cancel = held ? '\x18' : '\x85';
    int nbytes = 0;
    int rc = tty_write(PortFD, &cancel, 1, &nbytes);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("AbortMotion serial write error: %s.", errstr);
        return false;
    }

    // After a soft reset GRBL prints its banner; read it so it is not taken as a reply.
    if (held)
    {
        waitForBanner();
        tcflush(PortFD, TCIOFLUSH);
    }

    m_IsMoving = false;
    m_IsHeld   = false;
    LOG_INFO(held ? "UPAS held motion discarded." : "UPAS motion aborted.");
    return true;
}

bool AvalonUPAS::ReverseAZ(bool enabled)
{
    if (enabled)
        m_ReverseFlags |= 0x01;
    else
        m_ReverseFlags &= ~0x01;

    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "$3=%d", m_ReverseFlags);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;

    LOGF_INFO("AZ reverse %s (flags=0x%02X).", enabled ? "enabled" : "disabled", m_ReverseFlags);
    return (strncmp(res, "ok", 2) == 0);
}

bool AvalonUPAS::ReverseALT(bool enabled)
{
    if (enabled)
        m_ReverseFlags |= 0x02;
    else
        m_ReverseFlags &= ~0x02;

    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "$3=%d", m_ReverseFlags);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;

    LOGF_INFO("ALT reverse %s (flags=0x%02X).", enabled ? "enabled" : "disabled", m_ReverseFlags);
    return (strncmp(res, "ok", 2) == 0);
}

bool AvalonUPAS::SetBacklashEnabled(bool enabled)
{
    // Compensation is applied when jogging; nothing to send to GRBL.
    LOGF_INFO("Backlash compensation %s.", enabled ? "enabled" : "disabled");

    // GRBL starts at 0 on every power-up and the UPAS has no limit switches, so the driver
    // cannot keep the overshoot away from the mechanical stops.
    if (enabled)
        LOG_INFO("Moves against the approach direction overshoot by the backlash. "
                 "Keep at least that margin from the mechanical stops.");
    return true;
}

bool AvalonUPAS::SetBacklashAZ(int32_t steps)
{
    // The interface passes whole steps; use the arcminute value from the property instead.
    INDI_UNUSED(steps);
    LOGF_INFO("AZ backlash set to %.2f arcmin.", BacklashNP[BACKLASH_AZ].getValue());
    return true;
}

bool AvalonUPAS::SetBacklashALT(int32_t steps)
{
    INDI_UNUSED(steps);
    LOGF_INFO("ALT backlash set to %.2f arcmin.", BacklashNP[BACKLASH_ALT].getValue());
    return true;
}

bool AvalonUPAS::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (GearRatioNP.isNameMatch(name))
        {
            GearRatioNP.update(values, names, n);
            GearRatioNP.setState(IPS_OK);
            GearRatioNP.apply();
            return true;
        }

        if (FeedRateNP.isNameMatch(name))
        {
            FeedRateNP.update(values, names, n);
            FeedRateNP.setState(IPS_OK);
            FeedRateNP.apply();
            return true;
        }

        if (MinMoveNP.isNameMatch(name))
        {
            MinMoveNP.update(values, names, n);
            MinMoveNP.setState(IPS_OK);
            MinMoveNP.apply();
            return true;
        }

        if (PACI::processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool AvalonUPAS::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (AZApproachSP.isNameMatch(name) || ALTApproachSP.isNameMatch(name))
        {
            auto &approachSP = AZApproachSP.isNameMatch(name) ? AZApproachSP : ALTApproachSP;
            approachSP.update(states, names, n);
            approachSP.setState(IPS_OK);
            approachSP.apply();
            saveConfig(approachSP);
            return true;
        }

        if (UnlockSP.isNameMatch(name))
        {
            UnlockSP.reset();
            UnlockSP.setState(unlock() ? IPS_OK : IPS_ALERT);
            UnlockSP.apply();
            return true;
        }

        if (PACI::processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// Communication
/////////////////////////////////////////////////////////////////////////////
bool AvalonUPAS::sendCommand(const char *cmd, char *res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        // Raw binary write
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        // String command: append newline. The status query '?' is a real-time command and is sent
        // alone: GRBL would read a newline after it as an empty line and answer it with an extra "ok".
        LOGF_DEBUG("CMD <%s>", cmd);
        char formatted[DRIVER_LEN] = {0};
        snprintf(formatted, DRIVER_LEN, strcmp(cmd, "?") == 0 ? "%s" : "%s\n", cmd);
        rc = tty_write_string(PortFD, formatted, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    // Strip trailing newline/carriage-return
    if (nbytes_read > 0 && (res[nbytes_read - 1] == '\n' || res[nbytes_read - 1] == '\r'))
        res[nbytes_read - 1] = '\0';
    if (nbytes_read > 1 && (res[nbytes_read - 2] == '\r'))
        res[nbytes_read - 2] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);
    return true;
}

bool AvalonUPAS::sendLineCommand(const char *cmd, const std::function<void(const char *line)> &onLine)
{
    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);
    char formatted[DRIVER_LEN] = {0};
    snprintf(formatted, DRIVER_LEN, "%s\n", cmd);
    int nbytes = 0;
    int rc = tty_write_string(PortFD, formatted, &nbytes);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    // "$$" sends about 35 lines; the limit only guards against a device that never ends its reply.
    for (int i = 0; i < 100; i++)
    {
        // Read one byte less than the buffer so the line always stays null-terminated.
        char line[DRIVER_LEN] = {0};
        rc = tty_nread_section(PortFD, line, DRIVER_LEN - 1, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes);
        if (rc != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Serial read error after %s: %s.", cmd, errstr);
            return false;
        }

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0')
            continue;
        LOGF_DEBUG("RES <%s>", line);

        if (strcmp(line, "ok") == 0)
            return true;
        if (strncmp(line, "error", 5) == 0)
        {
            LOGF_ERROR("UPAS rejected %s: %s", cmd, line);
            return false;
        }
        if (onLine)
            onLine(line);
    }

    LOGF_ERROR("No end of the reply to %s.", cmd);
    tcflush(PortFD, TCIOFLUSH);
    return false;
}

void AvalonUPAS::hexDump(char *buf, const char *data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));
    if (size > 0)
        buf[3 * size - 1] = '\0';
}
