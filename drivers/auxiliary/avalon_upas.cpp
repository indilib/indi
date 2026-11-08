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
    setVersion(1, 2);
    SetCapability(PAC_CAN_REVERSE | PAC_HAS_POSITION);
}

const char *AvalonUPAS::getDefaultName()
{
    return "Avalon UPAS";
}

bool AvalonUPAS::initProperties()
{
    INDI::DefaultDevice::initProperties();

    PACI::initProperties(MAIN_CONTROL_TAB);

    // Feed rate per axis in mm/min (GRBL F word). Defaults are Avalon's suggested speeds (AZ 1000, ALT 700).
    FeedRateNP[FEED_AZ].fill("FEED_AZ", "Azimuth (mm/min)", "%.0f", 10, 1000, 50, 1000);
    FeedRateNP[FEED_ALT].fill("FEED_ALT", "Altitude (mm/min)", "%.0f", 10, 1000, 50, 700);
    FeedRateNP.fill(getDeviceName(), "PAC_FEED_RATE", "Feed Rate", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Gear ratio: mm of motor travel per arcminute of polar-axis correction.
    // Defaults are the Avalon specifications (AZ 2, ALT 22), as also used by Avalon's own software.
    GearRatioNP[GEAR_AZ].fill("GEAR_AZ", "Azimuth (mm/arcmin)", "%.4f", 0.001, 1000, 0.1, 2.0);
    GearRatioNP[GEAR_ALT].fill("GEAR_ALT", "Altitude (mm/arcmin)", "%.4f", 0.001, 1000, 0.1, 22.0);
    GearRatioNP.fill(getDeviceName(), "PAC_GEAR_RATIO", "Gear Ratio", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

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
        defineProperty(FirmwareTP);
    }
    else
    {
        deleteProperty(GearRatioNP);
        deleteProperty(FeedRateNP);
        deleteProperty(FirmwareTP);
    }

    PACI::updateProperties();
    return true;
}

bool AvalonUPAS::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    GearRatioNP.save(fp);
    FeedRateNP.save(fp);
    PACI::saveConfigItems(fp);
    return true;
}

bool AvalonUPAS::Handshake()
{
    PortFD = serialConnection->getPortFD();

    // Flush any GRBL boot message
    tcflush(PortFD, TCIOFLUSH);

    // Send soft-reset so GRBL is in a known idle state, then discard response.
    const char softReset[2] = {'\x18', '\n'};
    int nbytes = 0;
    tty_write(PortFD, softReset, 1, &nbytes);  // 0x18 is single byte, no \n needed
    usleep(500000);  // wait 500 ms for GRBL to boot/reset
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

    // Try to read GRBL version via $I command.
    char ver[DRIVER_LEN] = {0};
    if (sendCommand("$I", ver))
    {
        // GRBL version is typically on a line like "[VER:1.1f.20170801:]"
        const char *tag = strstr(ver, "[VER:");
        if (tag)
        {
            // Extract up to the closing bracket
            char version[64] = {0};
            sscanf(tag, "[VER:%63[^]]]", version);
            FirmwareTP[0].setText(version);
        }
        else
        {
            FirmwareTP[0].setText(ver);
        }
    }

    LOG_INFO("UPAS connected and ready.");
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

    // Determine if the device is still jogging.
    bool wasMoving = m_IsMoving;
    m_IsMoving = (strstr(response, "Jog") != nullptr || strstr(response, "Run") != nullptr);

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

IPState AvalonUPAS::MoveAZ(double degrees)
{
    // The gear ratio is in mm/arcmin, so convert the requested degrees to arcminutes first.
    const double mm       = degrees * 60 * GearRatioNP[GEAR_AZ].getValue();
    const double feedRate = feedRateFor(mm, 0);

    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "$J=G91G21X%.4f F%.0f", mm, feedRate);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return IPS_ALERT;

    // GRBL acknowledges jog commands with "ok"
    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("Unexpected AZ jog response: %s", res);
        return IPS_ALERT;
    }

    m_IsMoving = true;
    LOGF_INFO("AZ jog: %.4f deg → %.4f mm @ %.0f mm/min.", degrees, mm, feedRate);
    return IPS_BUSY;
}

IPState AvalonUPAS::MoveALT(double degrees)
{
    // The gear ratio is in mm/arcmin, so convert the requested degrees to arcminutes first.
    const double mm       = degrees * 60 * GearRatioNP[GEAR_ALT].getValue();
    const double feedRate = feedRateFor(0, mm);

    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "$J=G91G21Y%.4f F%.0f", mm, feedRate);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return IPS_ALERT;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("Unexpected ALT jog response: %s", res);
        return IPS_ALERT;
    }

    m_IsMoving = true;
    LOGF_INFO("ALT jog: %.4f deg → %.4f mm @ %.0f mm/min.", degrees, mm, feedRate);
    return IPS_BUSY;
}

IPState AvalonUPAS::MoveBoth(double azDegrees, double altDegrees)
{
    // GRBL supports multi-axis jog in a single command: X (AZ) and Y (ALT) together.
    // This moves both axes simultaneously, which is more efficient and accurate than
    // two separate jog commands.
    const double mmAZ    = azDegrees  * 60 * GearRatioNP[GEAR_AZ].getValue();
    const double mmALT   = altDegrees * 60 * GearRatioNP[GEAR_ALT].getValue();
    const double feedRate = feedRateFor(mmAZ, mmALT);

    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "$J=G91G21X%.4fY%.4f F%.0f", mmAZ, mmALT, feedRate);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return IPS_ALERT;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("MoveBoth GRBL response: %s", res);
        return IPS_ALERT;
    }

    m_IsMoving = true;
    LOGF_INFO("MoveBoth: AZ %.4f° (%.4f mm), ALT %.4f° (%.4f mm) @ %.0f mm/min.",
              azDegrees, mmAZ, altDegrees, mmALT, feedRate);
    return IPS_BUSY;
}

double AvalonUPAS::feedRateFor(double azMM, double altMM)
{
    // GRBL applies one feed rate to the whole jog vector. Choose it so that each axis moves
    // at most at its own feed rate; the limiting axis then moves at exactly that rate.
    const double length = std::hypot(azMM, altMM);
    if (length == 0)
        return FeedRateNP[FEED_AZ].getValue();

    double feedRate = std::numeric_limits<double>::max();
    if (azMM != 0)
        feedRate = std::min(feedRate, FeedRateNP[FEED_AZ].getValue() * length / std::fabs(azMM));
    if (altMM != 0)
        feedRate = std::min(feedRate, FeedRateNP[FEED_ALT].getValue() * length / std::fabs(altMM));
    return feedRate;
}

bool AvalonUPAS::AbortMotion()
{
    // GRBL real-time jog cancel command (single byte 0x85, no newline needed).
    const char cancel = '\x85';
    int nbytes = 0;
    int rc = tty_write(PortFD, &cancel, 1, &nbytes);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("AbortMotion serial write error: %s.", errstr);
        return false;
    }

    m_IsMoving = false;
    LOG_INFO("UPAS motion aborted.");
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

        if (PACI::processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool AvalonUPAS::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
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
        // String command: append newline
        LOGF_DEBUG("CMD <%s>", cmd);
        char formatted[DRIVER_LEN] = {0};
        snprintf(formatted, DRIVER_LEN, "%s\n", cmd);
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

void AvalonUPAS::hexDump(char *buf, const char *data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));
    if (size > 0)
        buf[3 * size - 1] = '\0';
}
