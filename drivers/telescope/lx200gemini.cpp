/*
    Losmandy Gemini INDI driver

    Copyright (C) 2017 Jasem Mutlaq
    Copyright (C) 2018 Eric Vickery

    Difference from LX200 Generic:

    1. Added Side of Pier
    2. Reimplemented isSlewComplete to use :Gv# since it is more reliable
    3. Support networked connections.
    4. Side of pier
    5. Variable GOTO/SLEW/MOVE speeds.

    v1.4:

    + Added MOUNT_STATE_UPDATE_FREQ to reduce number of calls to updateMountState to reduce traffic
    + All TCIFLUSH --> TCIOFLUSH to make sure both pipes are flushed since we received logs with mismatched traffic.

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
#include "connectionplugins/connectioninterface.h"
#include "connectionplugins/connectiontcp.h"

#include <cstring>
#include <termios.h>

#define MANUAL_SLEWING_SPEED_ID 120
#define GOTO_SLEWING_SPEED_ID 140
#define MOVE_SPEED_ID 145
#define GUIDING_SPEED_ID 150
#define CENTERING_SPEED_ID 170

LX200Gemini::LX200Gemini()
{
    setVersion(1, 6);

    setLX200Capability(LX200_HAS_SITES | LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_CAN_CONTROL_TRACK,
                           4);
}

const char *LX200Gemini::getDefaultName()
{
    return "Losmandy Gemini";
}

bool LX200Gemini::Connect()
{
    Connection::Interface *activeConnection = getActiveConnection();

    if (!activeConnection->name().compare("CONNECTION_TCP"))
    {
        tty_set_gemini_udp_format(1);
    }

    return LX200Generic::Connect();
}

void LX200Gemini::ISGetProperties(const char *dev)
{
    LX200Generic::ISGetProperties(dev);

    // Read config from file
    int index = 0;
    if (IUGetConfigOnSwitch(&StartupModeSP, &index) == 0)
    {
        IUResetSwitch(&StartupModeSP);
        StartupModeSP.sp[index].s = ISS_ON;
        defineSwitch(&StartupModeSP);
    }
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

    IUFillNumber(&ManualSlewingSpeedN[0], "MANUAL_SLEWING_SPEED", "Manual Slewing Speed", "%g", 20, 2000., 10., 800);
    IUFillNumberVector(&ManualSlewingSpeedNP, ManualSlewingSpeedN, 1, getDeviceName(), "MANUAL_SLEWING_SPEED",
                       "Manual Slewing Speed", MOTION_TAB, IP_RW,  0, IPS_IDLE);

    IUFillNumber(&GotoSlewingSpeedN[0], "GOTO_SLEWING_SPEED", "Goto Slewing Speed", "%g", 20, 2000., 10., 800);
    IUFillNumberVector(&GotoSlewingSpeedNP, GotoSlewingSpeedN, 1, getDeviceName(), "GOTO_SLEWING_SPEED", "Goto Slewing Speed",
                       MOTION_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&MoveSpeedN[0], "MOVE_SPEED", "Move Speed", "%g", 20, 2000., 10., 10);
    IUFillNumberVector(&MoveSpeedNP, MoveSpeedN, 1, getDeviceName(), "MOVE_SLEWING_SPEED", "Move Slewing Speed", MOTION_TAB,
                       IP_RW, 0, IPS_IDLE);

    IUFillNumber(&GuidingSpeedN[0], "GUIDING_SPEED", "Guiding Speed", "%g", 0.2, 0.8, 0.1, 0.5);
    IUFillNumberVector(&GuidingSpeedNP, GuidingSpeedN, 1, getDeviceName(), "GUIDING_SLEWING_SPEED", "Guiding Slewing Speed",
                       GUIDE_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&CenteringSpeedN[0], "CENTERING_SPEED", "Centering Speed", "%g", 20, 2000., 10., 10);
    IUFillNumberVector(&CenteringSpeedNP, CenteringSpeedN, 1, getDeviceName(), "CENTERING_SLEWING_SPEED",
                       "Centering Slewing Speed", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    IUFillSwitch(&TrackModeS[GEMINI_TRACK_SIDEREAL], "TRACK_SIDEREAL", "Sidereal", ISS_ON);
    IUFillSwitch(&TrackModeS[GEMINI_TRACK_KING], "TRACK_CUSTOM", "King", ISS_OFF);
    IUFillSwitch(&TrackModeS[GEMINI_TRACK_LUNAR], "TRACK_LUNAR", "Lunar", ISS_OFF);
    IUFillSwitch(&TrackModeS[GEMINI_TRACK_SOLAR], "TRACK_SOLAR", "Solar", ISS_OFF);

    return true;
}

bool LX200Gemini::updateProperties()
{
    const int MAX_VALUE_LENGTH = 32;
    LX200Generic::updateProperties();

    if (isConnected())
    {
        uint32_t speed = 0;
        char value[MAX_VALUE_LENGTH] = {0};
        defineSwitch(&ParkSettingsSP);

        if (getGeminiProperty(MANUAL_SLEWING_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            ManualSlewingSpeedN[0].value = speed;
            defineNumber(&ManualSlewingSpeedNP);
        }
        if (getGeminiProperty(GOTO_SLEWING_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            GotoSlewingSpeedN[0].value = speed;
            defineNumber(&GotoSlewingSpeedNP);
        }
        if (getGeminiProperty(MOVE_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            MoveSpeedN[0].value = speed;
            defineNumber(&MoveSpeedNP);
        }
        if (getGeminiProperty(GUIDING_SPEED_ID, value))
        {
            float guidingSpeed = 0.0;
            sscanf(value, "%f", &guidingSpeed);
            GuidingSpeedN[0].value = guidingSpeed;
            defineNumber(&GuidingSpeedNP);
        }
        if (getGeminiProperty(CENTERING_SPEED_ID, value))
        {
            sscanf(value, "%u", &speed);
            CenteringSpeedN[0].value = speed;
            defineNumber(&CenteringSpeedNP);
        }

        updateParkingState();
        updateMovementState();
    }
    else
    {
        deleteProperty(ParkSettingsSP.name);
        deleteProperty(ManualSlewingSpeedNP.name);
        deleteProperty(GotoSlewingSpeedNP.name);
        deleteProperty(MoveSpeedNP.name);
        deleteProperty(GuidingSpeedNP.name);
        deleteProperty(CenteringSpeedNP.name);
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
            if (isConnected())
                LOG_INFO("Startup mode will take effect on future connections.");
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

bool LX200Gemini::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        char valueString[16] = {0};
        snprintf(valueString, 16, "%2.0f", values[0]);

        if (!strcmp(name, ManualSlewingSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set manual slewing speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(MANUAL_SLEWING_SPEED_ID, valueString))
            {
                ManualSlewingSpeedNP.s = IPS_ALERT;
                IDSetNumber(&ManualSlewingSpeedNP, "Error setting manual slewing speed");
                return false;
            }

            ManualSlewingSpeedNP.s    = IPS_OK;
            ManualSlewingSpeedN[0].value = values[0];
            IDSetNumber(&ManualSlewingSpeedNP, "Manual slewing speed set to %f", values[0]);

            return true;
        }
        if (!strcmp(name, GotoSlewingSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set goto slewing speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(GOTO_SLEWING_SPEED_ID, valueString))
            {
                GotoSlewingSpeedNP.s = IPS_ALERT;
                IDSetNumber(&GotoSlewingSpeedNP, "Error setting goto slewing speed");
                return false;
            }

            GotoSlewingSpeedNP.s       = IPS_OK;
            GotoSlewingSpeedN[0].value = values[0];
            IDSetNumber(&GotoSlewingSpeedNP, "Goto slewing speed set to %f", values[0]);

            return true;
        }
        if (!strcmp(name, MoveSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set move speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(MOVE_SPEED_ID, valueString))
            {
                MoveSpeedNP.s = IPS_ALERT;
                IDSetNumber(&MoveSpeedNP, "Error setting move speed");
                return false;
            }

            MoveSpeedNP.s       = IPS_OK;
            MoveSpeedN[0].value = values[0];
            IDSetNumber(&MoveSpeedNP, "Move speed set to %f", values[0]);

            return true;
        }
        if (!strcmp(name, GuidingSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set guiding speed of: %f", values[0]);

            // Special formatting for guiding speed
            snprintf(valueString, 16, "%1.1f", values[0]);

            if (!isSimulation() && !setGeminiProperty(GUIDING_SPEED_ID, valueString))
            {
                GuidingSpeedNP.s = IPS_ALERT;
                IDSetNumber(&GuidingSpeedNP, "Error setting guiding speed");
                return false;
            }

            GuidingSpeedNP.s       = IPS_OK;
            GuidingSpeedN[0].value = values[0];
            IDSetNumber(&GuidingSpeedNP, "Guiding speed set to %f", values[0]);

            return true;
        }
        if (!strcmp(name, CenteringSpeedNP.name))
        {
            LOGF_DEBUG("Trying to set centering speed of: %f", values[0]);

            if (!isSimulation() && !setGeminiProperty(CENTERING_SPEED_ID, valueString))
            {
                CenteringSpeedNP.s = IPS_ALERT;
                IDSetNumber(&CenteringSpeedNP, "Error setting centering speed");
                return false;
            }

            CenteringSpeedNP.s       = IPS_OK;
            CenteringSpeedN[0].value = values[0];
            IDSetNumber(&CenteringSpeedNP, "Centering speed set to %f", values[0]);

            return true;
        }
    }

    //  If we didn't process it, continue up the chain, let somebody else give it a shot
    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200Gemini::checkConnection()
{
    if (isSimulation())
        return true;

    // Response
    char response[8] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%#02X>", 0x06);

    tcflush(PortFD, TCIFLUSH);

    char ack[1] = { 0x06 };

    if ((rc = tty_write(PortFD, ack, 1, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    // Read response
    if ((rc = tty_read_section(PortFD, response, '#', GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return false;
    }

    //response[1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    // If waiting for selection of startup mode, let us select it
    if (response[0] == 'b')
    {
        LOG_DEBUG("Mount is waiting for selection of the startup mode.");
        char cmd[4]     = "bC#";
        int startupMode = IUFindOnSwitchIndex(&StartupModeSP);
        if (startupMode == WARM_START)
            strncpy(cmd, "bW#", 4);
        else if (startupMode == WARM_RESTART)
            strncpy(cmd, "bR#", 4);

        LOGF_DEBUG("CMD: <%s>", cmd);

        if ((rc = tty_write(PortFD, cmd, 4, &nbytes_written)) != TTY_OK)
        {
            char errmsg[256];
            tty_error_msg(rc, errmsg, 256);
            LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
            return false;
        }

        tcflush(PortFD, TCIFLUSH);

        // Send ack again and check response
        return checkConnection();
    }
    else if (response[0] == 'B')
    {
        LOG_DEBUG("Initial startup message is being displayed.");
    }
    else if (response[0] == 'S')
    {
        LOG_DEBUG("Cold start in progress.");
    }
    else if (response[0] == 'G')
    {
        updateParkingState();
        updateMovementState();
        LOG_DEBUG("Startup complete with equatorial mount selected.");
    }
    else if (response[0] == 'A')
    {
        LOG_DEBUG("Startup complete with Alt-Az mount selected.");
    }

    return true;
}

bool LX200Gemini::isSlewComplete()
{
    LX200Gemini::MovementState movementState = getMovementState();

    if (movementState == TRACKING || movementState == GUIDING || movementState == NO_MOVEMENT)
        return true;
    else
        return false;
}

bool LX200Gemini::ReadScopeStatus()
{
    LOGF_DEBUG("ReadScopeStatus: TrackState is <%d>", TrackState);

    if (!isConnected())
        return false;

    if (isSimulation())
        return LX200Generic::ReadScopeStatus();

    if(m_isSleeping)
    {
        return true;
    }

    if (TrackState == SCOPE_SLEWING)
    {
        updateMovementState();

        EqNP.s = IPS_BUSY;
        IDSetNumber(&EqNP, NULL);

        // Check if LX200 is done slewing
        if (isSlewComplete())
        {
            // Set slew mode to "Centering"
            IUResetSwitch(&SlewRateSP);
            SlewRateS[SLEW_CENTERING].s = ISS_ON;
            IDSetSwitch(&SlewRateSP, nullptr);

            EqNP.s = IPS_OK;
            IDSetNumber(&EqNP, NULL);

            LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        updateParkingState();

        if (isSlewComplete())
        {
            LOG_DEBUG("Park is complete ...");
            SetParked(true);
            sleepMount();

            EqNP.s = IPS_IDLE;
            IDSetNumber(&EqNP, NULL);

            return true;
        }
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        LOG_ERROR("Error reading RA/DEC.");
        return false;
    }

    NewRaDec(currentRA, currentDEC);

    syncSideOfPier();

    return true;
}

void LX200Gemini::syncSideOfPier()
{
    // Send ':Gm#'
    const char *cmd = ":Gm#";
    // Response
    char response[8] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return;
    }

    if ((rc = tty_read_section(PortFD, response, '#', GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return;
    }

    response[nbytes_read - 1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    //LOGF_DEBUG("RES: <%s>", response);

    // fix to pier side read from the mount using the hour angle as a guide
    // see https://www.indilib.org/forum/general/6785-side-of-pier-problem-bug.html?start=12#52492
    // for a description of the problem and the proposed fix
    //
    auto lst = get_local_sidereal_time(this->LocationN[LOCATION_LONGITUDE].value);
    auto ha = rangeHA(lst - currentRA);
    auto pointingState = PIER_UNKNOWN;

    if (ha >= -5.0 && ha <= 5.0)
    {
        // mount pier side is used unchanged
        pointingState = response[0] == 'E' ? PIER_EAST : PIER_WEST;
    }
    else if (ha <= -7.0 || ha >= 7.0)
    {
        // mount pier side is reversed
        pointingState = response[0] == 'W' ? PIER_EAST : PIER_WEST;
    }
    else
    {
        // use hour angle because the pier side changes spontaneously near +-6h
        pointingState = ha > 0 ? PIER_EAST : PIER_WEST;
    }

    LOGF_DEBUG("RES: <%s>, lst %f, ha %f, pierSide %d", response, lst, ha, pointingState);

    setPierSide(pointingState);
}


bool LX200Gemini::Park()
{
    char cmd[6] = ":hP#";

    int parkSetting = IUFindOnSwitchIndex(&ParkSettingsSP);

    if (parkSetting == PARK_STARTUP)
        strncpy(cmd, ":hC#", 5);
    else if (parkSetting == PARK_ZENITH)
        strncpy(cmd, ":hZ#", 5);

    // Response
    int rc = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    ParkSP.s   = IPS_BUSY;
    TrackState = SCOPE_PARKING;

    updateParkingState();
    return true;
}

bool LX200Gemini::UnPark()
{
    wakeupMount();

    SetParked(false);
    TrackState = SCOPE_TRACKING;

    updateParkingState();
    return true;
}

bool LX200Gemini::sleepMount()
{
    const char *cmd = ":hN#";

    // Response
    int rc = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    m_isSleeping = true;
    LOG_INFO("Mount is sleeping...");
    return true;
}

bool LX200Gemini::wakeupMount()
{
    const char *cmd = ":hW#";

    // Response
    int rc = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    m_isSleeping = false;
    LOG_INFO("Mount is awake...");
    return true;
}

void LX200Gemini::setTrackState(INDI::Telescope::TelescopeStatus state)
{
    if (TrackState != state)
        TrackState = state;
}

void LX200Gemini::updateMovementState()
{
    LX200Gemini::MovementState movementState = getMovementState();

    switch (movementState)
    {
        case NO_MOVEMENT:
            if (priorParkingState == PARKED)
                setTrackState(SCOPE_PARKED);
            else
                setTrackState(SCOPE_IDLE);
            break;

        case TRACKING:
        case GUIDING:
            setTrackState(SCOPE_TRACKING);
            break;

        case CENTERING:
        case SLEWING:
            setTrackState(SCOPE_SLEWING);
            break;

        case STALLED:
            setTrackState(SCOPE_IDLE);
            break;
    }
}

void LX200Gemini::updateParkingState()
{
    LX200Gemini::ParkingState parkingState = getParkingState();

    if (parkingState != priorParkingState)
    {
        if (parkingState == PARKED)
            SetParked(true);
        else if (parkingState == NOT_PARKED)
            SetParked(false);
    }
    priorParkingState = parkingState;
}

LX200Gemini::MovementState LX200Gemini::getMovementState()
{
    const char *cmd = ":Gv#";
    char response[2] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return LX200Gemini::MovementState::NO_MOVEMENT;
    }

    if ((rc = tty_read(PortFD, response, 1, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return LX200Gemini::MovementState::NO_MOVEMENT;
    }

    response[1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    switch (response[0])
    {
        case 'N':
            return LX200Gemini::MovementState::NO_MOVEMENT;

        case 'T':
            return LX200Gemini::MovementState::TRACKING;

        case 'G':
            return LX200Gemini::MovementState::GUIDING;

        case 'C':
            return LX200Gemini::MovementState::CENTERING;

        case 'S':
            return LX200Gemini::MovementState::SLEWING;

        case '!':
            return LX200Gemini::MovementState::STALLED;

        default:
            return LX200Gemini::MovementState::NO_MOVEMENT;
    }
}

LX200Gemini::ParkingState LX200Gemini::getParkingState()
{
    const char *cmd = ":h?#";
    char response[2] = { 0 };
    int rc = 0, nbytes_read = 0, nbytes_written = 0;

    LOGF_DEBUG("CMD: <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 5, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return LX200Gemini::ParkingState::NOT_PARKED;
    }

    if ((rc = tty_read(PortFD, response, 1, GEMINI_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return LX200Gemini::ParkingState::NOT_PARKED;
    }

    response[1] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES: <%s>", response);

    switch (response[0])
    {
        case '0':
            return LX200Gemini::ParkingState::NOT_PARKED;

        case '1':
            return LX200Gemini::ParkingState::PARKED;

        case '2':
            return LX200Gemini::ParkingState::PARK_IN_PROGRESS;

        default:
            return LX200Gemini::ParkingState::NOT_PARKED;
    }
}

bool LX200Gemini::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &StartupModeSP);
    IUSaveConfigSwitch(fp, &ParkSettingsSP);

    return true;
}

bool LX200Gemini::getGeminiProperty(uint8_t propertyNumber, char* value)
{
    int rc = TTY_OK;
    int nbytes = 0;
    char prefix[16] = {0};
    char cmd[16] = {0};

    snprintf(prefix, 16, "<%d:", propertyNumber);

    uint8_t checksum = calculateChecksum(prefix);

    snprintf(cmd, 16, "%s%c#", prefix, checksum);

    LOGF_DEBUG("CMD: <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    if ((rc = tty_read_section(PortFD, value, '#', GEMINI_TIMEOUT, &nbytes)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error reading from device %s (%d)", errmsg, rc);
        return false;
    }

    value[nbytes - 1] = '\0';

    tcflush(PortFD, TCIFLUSH);

    LOGF_DEBUG("RES: <%s>", value);
    return true;
}

bool LX200Gemini::setGeminiProperty(uint8_t propertyNumber, char* value)
{
    int rc = TTY_OK;
    int nbytes_written = 0;
    char prefix[16] = {0};
    char cmd[16] = {0};

    snprintf(prefix, 16, ">%d:%s", propertyNumber, value);

    uint8_t checksum = calculateChecksum(prefix);

    snprintf(cmd, 16, "%s%c#", prefix, checksum);

    LOGF_DEBUG("CMD: <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIFLUSH);

    return true;
}

bool LX200Gemini::SetTrackMode(uint8_t mode)
{
    int rc = TTY_OK, nbytes_written = 0;
    char prefix[16] = {0};
    char cmd[16] = {0};

    snprintf(prefix, 16, ">130:%d", mode + 131);

    uint8_t checksum = calculateChecksum(prefix);

    snprintf(cmd, 16, "%s%c#", prefix, checksum);

    LOGF_DEBUG("CMD: <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        LOGF_ERROR("Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    tcflush(PortFD, TCIFLUSH);

    return true;
}

bool LX200Gemini::SetTrackEnabled(bool enabled)
{
    if (enabled)
    {
        return wakeupMount();
    }
    else
    {
        return sleepMount();
    }
}

uint8_t LX200Gemini::calculateChecksum(char *cmd)
{
    uint8_t result = cmd[0];

    for (size_t i = 1; i < strlen(cmd); i++)
        result = result ^ cmd[i];

    result = result % 128;
    result += 64;

    return result;
}
