/*******************************************************************************
  Copyright(c) 2026 Jérémie Klein. All rights reserved.

  Wanderer Snowflake Filter Wheel Driver

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License version 2 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "wanderer_snowflake.h"

#include "indicom.h"

#include <connectionplugins/connectionserial.h>
#include <cstring>
#include <memory>
#include <strings.h>
#include <termios.h>

// Wanderer Snowflake protocol specifics
#define WANDERER_TIMEOUT 5
/** Per-field timeout while parsing the continuous A-delimited status stream (motor activity can slow UART). */
#define WANDERER_STATUS_TIMEOUT 12
/** Optional line reply after 1500002; may be absent while the status stream is busy. */
#define WANDERER_CALIB_REPLY_TIMEOUT 10
#define WANDERER_MAX_FILTERS 8
/** Zero detection — default 1500003; override if your protocol PDF differs. */
#define WANDERER_CMD_ZERO_DETECT "1002"

// Single global instance
static std::unique_ptr<WandererSnowflakeFW> wanderer_snowflake(new WandererSnowflakeFW());

WandererSnowflakeFW::WandererSnowflakeFW()
{
    setVersion(1, 0);
    setFilterConnection(CONNECTION_SERIAL);
}

const char *WandererSnowflakeFW::getDefaultName()
{
    return "Wanderer Snowflake Filter Wheel";
}

bool WandererSnowflakeFW::initProperties()
{
    INDI::FilterWheel::initProperties();

    // Protocol: 19200 8N1. INDI serial defaults are already 8N1; only baud must be set.
    if (serialConnection != nullptr)
        serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(WANDERER_MAX_FILTERS);

    IUFillSwitch(&CalibrationCmdS[0], "CALIBRATE", "Auto calibrate", ISS_OFF);
    IUFillSwitch(&CalibrationCmdS[1], "ZERO_DETECT", "Zero detection", ISS_OFF);
    IUFillSwitchVector(&CalibrationCmdSP, CalibrationCmdS, 2, getDeviceName(), "CALIBRATION_CMDS", "Calibration",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    addAuxControls();

    return true;
}

bool WandererSnowflakeFW::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
        defineProperty(&CalibrationCmdSP);
    else
        deleteProperty(CalibrationCmdSP.name);

    return true;
}

bool WandererSnowflakeFW::Handshake()
{
    if (isSimulation())
    {
        sendAutomaticCalibration();
        return true;
    }

    tcflush(PortFD, TCIOFLUSH);

    int current = 0;
    if (!readCurrentFilterFromStatus(current))
    {
        LOG_ERROR("Failed to read status from Wanderer Snowflake filter wheel.");
        return false;
    }

    if (current >= 1 && current <= WANDERER_MAX_FILTERS)
    {
        CurrentFilter = current;
        FilterSlotNP[0].setValue(CurrentFilter);
    }

    usleep(200000);

    if (!sendAutomaticCalibration())
        LOG_WARN("Automatic calibration (1500002) failed at connect; use Calibration → Auto calibrate.");
    else
        LOG_INFO("Automatic calibration (1500002) sent at connection.");

    return true;
}

bool WandererSnowflakeFW::sendAutomaticCalibration()
{
    if (isSimulation())
    {
        LOG_INFO("Automatic calibration (1500002, simulation).");
        return true;
    }

    // The spec documents no reply for 1500002 — send only, no read.
    if (!sendCommand("1500002", nullptr, 0, WANDERER_TIMEOUT))
    {
        LOG_ERROR("Failed to send automatic calibration command (1500002).");
        return false;
    }
    LOG_INFO("Automatic calibration command (1500002) sent.");
    return true;
}

bool WandererSnowflakeFW::sendZeroDetection()
{
    if (isSimulation())
    {
        LOG_INFO("Zero detection (" WANDERER_CMD_ZERO_DETECT ", simulation). CurrentFilter → 1.");
        CurrentFilter = 1;
        FilterSlotNP[0].setValue(1);
        FilterSlotNP.setState(IPS_OK);
        FilterSlotNP.apply("Zero detection complete (simulation).");
        return true;
    }

    // Command 1002: triggers zero position detection and moves to filter 1.
    // No reply is documented; result is visible in the continuous status stream.
    if (!sendCommand(WANDERER_CMD_ZERO_DETECT, nullptr, 0, WANDERER_TIMEOUT))
    {
        LOGF_ERROR("Failed to send zero detection command (%s).", WANDERER_CMD_ZERO_DETECT);
        return false;
    }
    LOGF_INFO("Zero detection command (%s) sent; waiting for filter 1.", WANDERER_CMD_ZERO_DETECT);

    // Wait for the wheel to reach filter 1 (same logic as SelectFilter).
    constexpr int maxAttempts = 80;
    for (int attempts = 0; attempts < maxAttempts; ++attempts)
    {
        int current = 0;
        if (readCurrentFilterFromStatus(current) && current == 1)
        {
            CurrentFilter = 1;
            FilterSlotNP[0].setValue(1);
            FilterSlotNP.setState(IPS_OK);
            FilterSlotNP.apply("Zero detection complete, at filter 1.");
            return true;
        }
        usleep(500 * 1000);
    }

    LOG_WARN("Zero detection: timed out waiting for filter 1; wheel may still be moving.");
    return false;
}

bool WandererSnowflakeFW::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && std::strcmp(dev, getDeviceName()) == 0)
    {
        if (std::strcmp(CalibrationCmdSP.name, name) == 0)
        {
            IUUpdateSwitch(&CalibrationCmdSP, states, names, n);
            const bool wantCal  = (CalibrationCmdS[0].s == ISS_ON);
            const bool wantZero = (CalibrationCmdS[1].s == ISS_ON);
            IUResetSwitch(&CalibrationCmdSP);

            CalibrationCmdSP.s = IPS_BUSY;
            IDSetSwitch(&CalibrationCmdSP, nullptr);

            bool ok = true;
            if (wantCal)
                ok = sendAutomaticCalibration();
            else if (wantZero)
                ok = sendZeroDetection();

            CalibrationCmdSP.s = ok ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&CalibrationCmdSP, nullptr);
            return true;
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

int WandererSnowflakeFW::QueryFilter()
{
    return CurrentFilter;
}

bool WandererSnowflakeFW::SelectFilter(int position)
{
    if (position == CurrentFilter)
    {
        SelectFilterDone(CurrentFilter);
        return true;
    }

    if (position < 1 || position > WANDERER_MAX_FILTERS)
    {
        LOGF_ERROR("Requested filter position %d is out of range (1-%d).", position, WANDERER_MAX_FILTERS);
        return false;
    }

    TargetFilter = position;

    if (isSimulation())
    {
        CurrentFilter = TargetFilter;
        FilterSlotNP[0].setValue(CurrentFilter);
        FilterSlotNP.setState(IPS_OK);
        FilterSlotNP.apply("Selected filter position reached (simulation).");
        SelectFilterDone(CurrentFilter);
        return true;
    }

    // Command 200X where X = 1–8 moves to the specified filter position.
    char cmd[16] = {0};
    snprintf(cmd, sizeof(cmd), "%d", 2000 + TargetFilter);

    // Do not read a fixed-length reply: the wheel continuously emits status frames; tty_read
    // would consume arbitrary bytes and break parsing. NP (no 12V) is visible in status if needed.
    if (!sendCommand(cmd, nullptr, 0, WANDERER_TIMEOUT))
    {
        LOG_ERROR("Failed to send move command to Wanderer Snowflake filter wheel.");
        return false;
    }

    // Wait for the wheel to report the new position via its status stream.
    int attempts = 0;
    constexpr int maxAttempts = 80; // up to ~40s with 500ms sleeps (slow mechanical moves)

    while (attempts++ < maxAttempts)
    {
        int current = 0;
        if (!readCurrentFilterFromStatus(current))
        {
            usleep(500 * 1000);
            continue;
        }

        if (current == TargetFilter)
        {
            CurrentFilter = current;
            FilterSlotNP[0].setValue(CurrentFilter);
            FilterSlotNP.setState(IPS_OK);
            FilterSlotNP.apply("Selected filter position reached");
            SelectFilterDone(CurrentFilter);
            LOGF_DEBUG("CurrentFilter set to %d", CurrentFilter);
            return true;
        }

        usleep(500 * 1000);
    }

    LOG_ERROR("Timed out waiting for Wanderer Snowflake filter wheel to reach target position.");
    return false;
}

bool WandererSnowflakeFW::sendCommand(const char *command, char *response, int responseLen, int timeoutSeconds)
{
    int nbytes_written = 0;
    int nbytes_read = 0;
    int rc;

    if (PortFD < 0)
        return false;

    // Protocol text commands are CR-terminated (Wanderer serial docs / common UART practice).
    char line[48];
    const int linelen = std::snprintf(line, sizeof(line), "%s\r", command);
    if (linelen <= 0 || linelen >= static_cast<int>(sizeof(line)))
    {
        LOG_ERROR("Wanderer Snowflake command too long for transmit buffer.");
        return false;
    }

    // For move commands (no response read), do not flush RX: while the wheel is turning the
    // buffer fills with status; flushing can discard data and timing can make the next write
    // look like a failure on older builds that still read a fixed reply after 200X.
    // Flush RX only when we wait for an optional line reply (e.g. 1500002).
    if (response != nullptr && responseLen > 1)
        tcflush(PortFD, TCIFLUSH);

    if ((rc = tty_write(PortFD, line, linelen, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error sending command '%s' to Wanderer Snowflake filter wheel: %s", command, errstr);
        return false;
    }

    if (response == nullptr || responseLen <= 1)
        return true;

    // Optional line reply (e.g. "NP") — read until CR/LF, not a fixed byte count (tty_read
    // would otherwise soak arbitrary bytes from the status stream).
    rc = tty_read_section(PortFD, response, '\r', timeoutSeconds, &nbytes_read);
    if (rc != TTY_OK)
        rc = tty_read_section(PortFD, response, '\n', timeoutSeconds, &nbytes_read);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_DEBUG("No CR/LF line after command '%s' (status stream may omit it while moving): %s", command, errstr);
        response[0] = '\0';
        return true; // write succeeded; calibration / NP detection is best-effort only
    }

    while (nbytes_read > 0 && (response[nbytes_read - 1] == '\r' || response[nbytes_read - 1] == '\n'))
    {
        nbytes_read--;
        response[nbytes_read] = '\0';
    }
    if (nbytes_read >= responseLen)
        nbytes_read = responseLen - 1;
    response[nbytes_read] = '\0';

    return true;
}

namespace
{
/** tty_read_section stores the stop character in buf; strip it and trailing CR/LF. */
void wandererStripFieldDelimiter(char *field, int &nbytes_read, char delimiter)
{
    while (nbytes_read > 0)
    {
        char c = field[nbytes_read - 1];
        if (c == delimiter || c == '\r' || c == '\n')
        {
            nbytes_read--;
            field[nbytes_read] = '\0';
        }
        else
            break;
    }
    field[nbytes_read] = '\0';
}

/** Left-trim spaces / CR / LF; result is within the same buffer. */
char *wandererTrimLeft(char *s)
{
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    if (p != s)
        std::memmove(s, p, std::strlen(p) + 1);
    return s;
}

static const char *wandererFindWsfw(const char *s)
{
    for (const char *p = s; *p != '\0'; ++p)
    {
        if (strncasecmp(p, "WSFW", 4) == 0)
            return p;
    }
    return nullptr;
}

/** True if this A-delimited field is the model token (WSFW… family). */
bool wandererFieldLooksLikeModel(char *field, int nbytes_read)
{
    if (nbytes_read <= 0)
        return false;
    field[nbytes_read] = '\0';
    wandererTrimLeft(field);
    nbytes_read = static_cast<int>(std::strlen(field));
    if (nbytes_read < 4)
        return false;
    if (strncasecmp(field, "WSFW", 4) == 0)
        return true;
    const char *hit = wandererFindWsfw(field);
    if (hit == nullptr)
        return false;
    for (const char *q = field; q < hit; ++q)
    {
        if (*q != ' ' && *q != '\t')
            return false;
    }
    return true;
}

bool wandererReadFilterFromThreeFields(int PortFD, char *field, int &position)
{
    int rc = 0;
    int nbytes_read = 0;

    if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        return false;
    wandererStripFieldDelimiter(field, nbytes_read, 'A');

    if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        return false;
    wandererStripFieldDelimiter(field, nbytes_read, 'A');

    if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        return false;
    wandererStripFieldDelimiter(field, nbytes_read, 'A');

    int current = std::atoi(field);
    if (current < 1 || current > WANDERER_MAX_FILTERS)
        return false;
    position = current;
    return true;
}
} // namespace

bool WandererSnowflakeFW::readCurrentFilterFromStatus(int &position)
{
    int rc = -1;
    int nbytes_read = 0;
    char field[64] = {0};

    if (PortFD < 0)
        return false;

    // Status format (Wanderer Snowflake serial protocol):
    // <Model>A<FirmwareVersion>A<CurrentFilter>A<8 filter names>A<offsets>...A<DeviceID>A
    //
    // tty_read_section includes the delimiter 'A' in the buffer — strip before parsing.
    // Resync: skip A-fields until one looks like the model (WSFW…), then firmware, then slot.

    constexpr int maxResync = 32;
    bool haveModel = false;

    for (int skip = 0; skip < maxResync; ++skip)
    {
        if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_WARN("Failed to read Wanderer Snowflake status field: %s", errstr);
            return false;
        }
        wandererStripFieldDelimiter(field, nbytes_read, 'A');

        if (wandererFieldLooksLikeModel(field, static_cast<int>(std::strlen(field))))
        {
            haveModel = true;
            break;
        }

        LOGF_DEBUG("Skipping Wanderer Snowflake status field while resyncing: '%s'", field);
    }

    if (haveModel)
    {
        if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_WARN("Failed to read firmware field from Wanderer Snowflake status: %s", errstr);
            return false;
        }
        wandererStripFieldDelimiter(field, nbytes_read, 'A');

        if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_WARN("Failed to read current filter field from Wanderer Snowflake status: %s", errstr);
            return false;
        }
        wandererStripFieldDelimiter(field, nbytes_read, 'A');

        int current = std::atoi(field);
        if (current >= 1 && current <= WANDERER_MAX_FILTERS)
        {
            position = current;
            return true;
        }
        LOGF_WARN("Received invalid current filter value '%s' from Wanderer Snowflake status.", field);
        return false;
    }

    /* Resync consumed partial frames; flush RX and try three A-fields from a fresh boundary. */
    tcflush(PortFD, TCIFLUSH);
    usleep(150000);
    LOG_DEBUG("No WSFW token after resync; flushed RX and trying 3-field status read.");
    if (wandererReadFilterFromThreeFields(PortFD, field, position))
        return true;

    LOG_WARN("Could not parse Wanderer Snowflake status (no WSFW* field and 3-field read failed).");
    return false;
}

