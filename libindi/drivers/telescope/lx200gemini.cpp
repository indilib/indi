/*
    Losmandy Gemini INDI driver

    Copyright (C) 2017 Jasem Mutlaq

    Difference from LX200 Generic:

    1. Added Side of Pier
    2. Reimplemented isSlewComplete to use :Gv# since it is more reliable

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

#include "lx200gemini.h"

#include "indicom.h"
#include "lx200driver.h"

#include <cstring>
#include <termios.h>

LX200Gemini::LX200Gemini()
{
    setVersion(1, 3);

    setLX200Capability(LX200_HAS_SITES | LX200_HAS_FOCUS);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                               TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_TRACK_MODE,
                           4);
}

const char *LX200Gemini::getDefaultName()
{
    return (const char *)"Losmandy Gemini";
}

void LX200Gemini::ISGetProperties(const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    defineSwitch(&StartupModeSP);
    loadConfig(true, StartupModeSP.name);
}

bool LX200Gemini::initProperties()
{
    LX200Generic::initProperties();

    // Park Option
    IUFillSwitch(&ParkSettingsS[PARK_HOME], "HOME", "Home", ISS_ON);
    IUFillSwitch(&ParkSettingsS[PARK_STARTUP], "STARTUP", "Startup", ISS_OFF);
    IUFillSwitch(&ParkSettingsS[PARK_ZENITH], "ZENITH", "Zenith", ISS_OFF);
    IUFillSwitchVector(&ParkSettingsSP, ParkSettingsS, 3, getDeviceName(), "PARK_SETTINGS", "Park Settings",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&StartupModeS[COLD_START], "COLD_START", "Cold", ISS_ON);
    IUFillSwitch(&StartupModeS[PARK_STARTUP], "WARM_START", "Warm", ISS_OFF);
    IUFillSwitch(&StartupModeS[PARK_ZENITH], "WARM_RESTART", "Restart", ISS_OFF);
    IUFillSwitchVector(&StartupModeSP, StartupModeS, 3, getDeviceName(), "STARTUP_MODE", "Startup Mode",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);


    IUFillSwitch(&TrackModeS[GEMINI_TRACK_SIDEREAL], "TRACK_SIDEREAL", "Sidereal", ISS_ON);
    IUFillSwitch(&TrackModeS[GEMINI_TRACK_KING], "TRACK_CUSTOM", "King", ISS_OFF);
    IUFillSwitch(&TrackModeS[GEMINI_TRACK_LUNAR], "TRACK_LUNAR", "Lunar", ISS_OFF);
    IUFillSwitch(&TrackModeS[GEMINI_TRACK_SOLAR], "TRACK_SOLAR", "Solar", ISS_OFF);

    return true;
}

bool LX200Gemini::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        defineSwitch(&ParkSettingsSP);
    }
    else
    {
        deleteProperty(ParkSettingsSP.name);
    }

    return true;
}

bool LX200Gemini::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, StartupModeSP.name))
        {
            IUUpdateSwitch(&StartupModeSP, states, names, n);
            StartupModeSP.s = IPS_OK;

            DEBUG(INDI::Logger::DBG_SESSION, "Startup mode will take effect on future connections.");
            IDSetSwitch(&StartupModeSP, nullptr);
            return true;
        }

        if (!strcmp(name, ParkSettingsSP.name))
        {
            IUUpdateSwitch(&ParkSettingsSP, states, names, n);
            ParkSettingsSP.s = IPS_OK;
            IDSetSwitch(&ParkSettingsSP, nullptr);
            return true;
        }
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200Gemini::checkConnection()
{
    if (isSimulation())
        return true;

    // Response
    char response[8] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: <%#02X>", 0x06);

    tcflush(PortFD, TCIFLUSH);

    char ack[1] = { 0x06 };

    if ((rc = tty_write(PortFD, ack, 1, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    // Read response
    if ((rc = tty_read_section(PortFD, response, '#', GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error reading from device %s (%d)", errmsg, rc);
        return false;
    }

    //response[1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: <%s>", response);

    // If waiting for selection of startup mode, let us select it
    if (response[0] == 'b')
    {
        DEBUG(INDI::Logger::DBG_DEBUG, "Mount is waiting for selection of the startup mode.");
        char cmd[4]     = "bC#";
        int startupMode = IUFindOnSwitchIndex(&StartupModeSP);
        if (startupMode == WARM_START)
            strncpy(cmd, "bW#", 4);
        else if (startupMode == WARM_RESTART)
            strncpy(cmd, "bR#", 4);

        DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: <%s>", cmd);

        if ((rc = tty_write(PortFD, cmd, 4, &nbytes_written)) != TTY_OK)
        {
            char errmsg[256];
            tty_error_msg(rc, errmsg, 256);
            DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
            return false;
        }

        // Send ack again and check response
        return checkConnection();
    }
    else if (response[0] == 'B')
    {
        DEBUG(INDI::Logger::DBG_DEBUG, "Initial startup message is being displayed.");
    }
    else if (response[0] == 'S')
    {
        DEBUG(INDI::Logger::DBG_DEBUG, "Cold start in progress.");
    }
    else if (response[0] == 'G')
    {
        DEBUG(INDI::Logger::DBG_DEBUG, "Startup complete with equatorial mount selected.");
    }
    else if (response[0] == 'A')
    {
        DEBUG(INDI::Logger::DBG_DEBUG, "Startup complete with Alt-Az mount selected.");
    }

    return true;
}

bool LX200Gemini::isSlewComplete()
{
    // Send ':Gv#'
    const char *cmd = "#:Gv#";
    // Response
    char response[2] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: <%s>", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    // Read 1 character
    if ((rc = tty_read(PortFD, response, 1, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error reading from device %s (%d)", errmsg, rc);
        return false;
    }

    response[1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: <%s>", response);

    if (response[0] == 'T' || response[0] == 'G' || response[0] == 'N')
        return true;
    else
        return false;
}

bool LX200Gemini::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    if (isSimulation())
        return LX200Generic::ReadScopeStatus();

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if (isSlewComplete())
        {
            // Set slew mode to "Centering"
            IUResetSwitch(&SlewRateSP);
            SlewRateS[SLEW_CENTERING].s = ISS_ON;
            IDSetSwitch(&SlewRateSP, nullptr);

            TrackState = SCOPE_TRACKING;
            DEBUG(INDI::Logger::DBG_SESSION, "Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (isSlewComplete())
        {
            SetParked(true);
            sleepMount();
        }
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
        return false;
    }

    NewRaDec(currentRA, currentDEC);

    syncSideOfPier();

    return true;
}

void LX200Gemini::syncSideOfPier()
{
    // Send ':Gv#'
    const char *cmd = "#:Gm#";
    // Response
    char response[8] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: <%s>", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return;
    }

    // Read 1 character
    if ((rc = tty_read_section(PortFD, response, '#', GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error reading from device %s (%d)", errmsg, rc);
        return;
    }

    response[nbytes_read - 1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: <%s>", response);

    setPierSide(response[0] == 'E' ? INDI::Telescope::PIER_EAST : INDI::Telescope::PIER_WEST);
}

bool LX200Gemini::Park()
{
    char cmd[6] = "#:hP#";

    int parkSetting = IUFindOnSwitchIndex(&ParkSettingsSP);

    if (parkSetting == PARK_STARTUP)
        strncpy(cmd, "#:hC#", 5);
    else if (parkSetting == PARK_ZENITH)
        strncpy(cmd, "#:hZ#", 5);

    // Response
    int rc = 0, nbytes_written = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: <%s>", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    ParkSP.s   = IPS_BUSY;
    TrackState = SCOPE_PARKING;
    return true;
}

bool LX200Gemini::UnPark()
{
    wakeupMount();

    TrackState = SCOPE_IDLE;
    return true;
}

bool LX200Gemini::sleepMount()
{
    const char *cmd = "#:hN#";

    // Response
    int rc = 0, nbytes_written = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: <%s>", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Mount is sleeping...");
    return true;
}

bool LX200Gemini::wakeupMount()
{
    const char *cmd = "#:hW#";

    // Response
    int rc = 0, nbytes_written = 0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: <%s>", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Mount is awake...");
    return true;
}

bool LX200Gemini::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &StartupModeSP);
    IUSaveConfigSwitch(fp, &ParkSettingsSP);

    return true;
}

bool LX200Gemini::SetTrackMode(uint8_t mode)
{
    int rc = TTY_OK, nbytes_written=0;
    char prefix[16] = {0};
    char cmd[16] = {0};

    snprintf(prefix, 16, ">130:%d", mode + 131);

    uint8_t checksum = calculateChecksum(prefix);

    snprintf(cmd, 16, "%s%c#", prefix, checksum);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIFLUSH);

    return true;
}

uint8_t LX200Gemini::calculateChecksum(char *cmd)
{
    uint8_t result = cmd[0];

    for (size_t i=1; i < strlen(cmd); i++)
        result = result ^ cmd[i];

    result = result % 128;
    result += 64;

    return result;
}
