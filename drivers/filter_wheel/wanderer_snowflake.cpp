/*******************************************************************************
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
#include <termios.h>

// Wanderer Snowflake protocol specifics
#define WANDERER_TIMEOUT 5
#define WANDERER_MAX_FILTERS 8

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

    // The protocol specifies 19200 8N1.
    if (serialConnection != nullptr)
    {
        serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
        serialConnection->setDefaultDataBits(Connection::Serial::DATABITS_8);
        serialConnection->setDefaultParity(Connection::Serial::PARITY_NONE);
        serialConnection->setDefaultStopBits(Connection::Serial::STOPBITS_1);
    }

    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(WANDERER_MAX_FILTERS);

    addAuxControls();

    return true;
}

bool WandererSnowflakeFW::Handshake()
{
    int current = 0;

    if (isSimulation())
        return true;

    // Flush any stale data.
    tcflush(PortFD, TCIOFLUSH);

    // The wheel continuously sends status frames. Try to parse the current filter
    // position from the incoming stream to verify communication.
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

    // Enable automatic calibration before next switch, as recommended in the
    // protocol documentation.
    char response[8] = {0};
    if (!sendCommand("1500002", response, sizeof(response), WANDERER_TIMEOUT))
    {
        LOG_WARN("Failed to send automatic calibration command (1500002). Continuing without it.");
    }

    return true;
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

    char response[16] = {0};
    if (!sendCommand(cmd, response, sizeof(response), WANDERER_TIMEOUT))
    {
        LOG_ERROR("Failed to send move command to Wanderer Snowflake filter wheel.");
        return false;
    }

    // If the device replies with "NP", 12V power is not connected.
    if (std::strncmp(response, "NP", 2) == 0)
    {
        LOG_ERROR("Wanderer Snowflake filter wheel reported 'NP' (12V power not connected).");
        return false;
    }

    // Wait for the wheel to report the new position via its status stream.
    int attempts = 0;
    constexpr int maxAttempts = 40; // up to ~20s with 500ms sleeps

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

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, command, std::strlen(command), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error sending command '%s' to Wanderer Snowflake filter wheel: %s", command, errstr);
        return false;
    }

    if (response != nullptr && responseLen > 1)
    {
        // Try to read a short response (e.g. "NP" or simple acknowledgement).
        if ((rc = tty_read(PortFD, response, responseLen - 1, timeoutSeconds, &nbytes_read)) != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_WARN("Error reading response to command '%s' from Wanderer Snowflake filter wheel: %s", command, errstr);
            response[0] = '\0';
            return false;
        }

        response[nbytes_read] = '\0';
    }

    tcflush(PortFD, TCIOFLUSH);
    return true;
}

bool WandererSnowflakeFW::readCurrentFilterFromStatus(int &position)
{
    int rc = -1;
    int nbytes_read = 0;
    char field[64] = {0};

    if (PortFD < 0)
        return false;

    // The status format is:
    // <Model>A<FirmwareVersion>A<CurrentFilter>A<8 Filter Names>A<Offset1>...A<Offset8>A<DeviceID>A
    //
    // We read three 'A'-terminated fields to reach the current filter.

    // Model
    if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_WARN("Failed to read model field from Wanderer Snowflake status: %s", errstr);
        return false;
    }
    field[nbytes_read] = '\0';

    if (std::strncmp(field, "WSFW", 4) != 0)
    {
        LOGF_WARN("Unexpected Wanderer Snowflake model prefix '%s'.", field);
    }

    // Firmware version (ignored for now)
    if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_WARN("Failed to read firmware field from Wanderer Snowflake status: %s", errstr);
        return false;
    }
    field[nbytes_read] = '\0';

    // Current filter number
    if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_WARN("Failed to read current filter field from Wanderer Snowflake status: %s", errstr);
        return false;
    }
    field[nbytes_read] = '\0';

    int current = std::atoi(field);
    if (current < 1 || current > WANDERER_MAX_FILTERS)
    {
        LOGF_WARN("Received invalid current filter value '%s' from Wanderer Snowflake status.", field);
        return false;
    }

    position = current;
    return true;
}

