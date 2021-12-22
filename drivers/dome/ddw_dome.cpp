/*******************************************************************************
 DDW Dome INDI Driver

 Copyright(c) 2020-2021 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2021 Jarno Paananen. All rights reserved.

 and

 Baader Planetarium Dome INDI Driver

 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 Baader Dome INDI Driver

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "ddw_dome.h"

#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#define DDW_TIMEOUT 2

// We declare an auto pointer to DDW.
std::unique_ptr<DDW> ddw(new DDW());

DDW::DDW()
{
    setVersion(1, 0);
    SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_ABS_MOVE | DOME_CAN_PARK | DOME_HAS_SHUTTER);
    setDomeConnection(CONNECTION_SERIAL);
}

bool DDW::initProperties()
{
    INDI::Dome::initProperties();

    IUFillNumber(&FirmwareVersionN[0], "VERSION", "Version", "%2.0f", 0.0, 99.0, 1.0, 0.0);
    IUFillNumberVector(&FirmwareVersionNP, FirmwareVersionN, 2, getDeviceName(), "FIRMWARE", "Firmware", INFO_TAB,
                       IP_RO, 60, IPS_IDLE);

    SetParkDataType(PARK_AZ);

    addAuxControls();

    // Set serial parameters
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);

    setPollingPeriodRange(1000, 3000); // Device doesn't like too long interval
    setDefaultPollingPeriod(1000);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::SetupParms()
{
    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking
        // values.
        SetAxis1ParkDefault(0);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is
        // found.
        SetDefaultPark();
        SetAxis1ParkDefault(0);
    }

    FirmwareVersionN[0].value = fwVersion;
    FirmwareVersionNP.s       = IPS_OK;
    IDSetNumber(&FirmwareVersionNP, nullptr);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::Handshake()
{
    // Send GINF command and check if the controller responds
    int rc = writeCmd("GINF");
    if (rc != TTY_OK)
        return false;

    std::string response;
    rc = readStatus(response);
    if (rc != TTY_OK)
        return false;

    LOGF_DEBUG("Initial response: %s", response.c_str());

    // Response should start with V
    if (response[0] != 'V')
        return false;

    parseGINF(response.c_str());
    cmdState = IDLE;
    DomeAbsPosNP.s = IPS_OK;
    IDSetNumber(&DomeAbsPosNP, nullptr);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *DDW::getDefaultName()
{
    return (const char *)"DDW Dome";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        defineProperty(&FirmwareVersionNP);
        SetupParms();
    }
    else
    {
        deleteProperty(FirmwareVersionNP.name);
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

bool DDW::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

int DDW::writeCmd(const char *cmd)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error writing command: %s. Cmd: %s", errstr, cmd);
    }
    return rc;
}

int DDW::readStatus(std::string &status)
{
    int nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];

    // Read response and parse it
    char response[256] = { 0 };

    // Read buffer
    if ((rc = tty_nread_section(PortFD, response, sizeof(response) - 1, '\r', DDW_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error reading: %s.", errstr);
    }
    status = response;
    return rc;
}

void DDW::parseGINF(const char *response)
{
    /* GINF packet structure:

    Field   Content     Note (each datum is separated by comma)
    1       V#          Denotes Version Data.  E.g., V1
    2       Dticks      DTICKS is dome circumference in ticks 0-32767.  Value is sent as a string of characters, e.g., 457.  Leading zeros not transmitted.
    3       Home1       Azimuth location of the HOME position in ticks 0-32767
    4       Coast       Coast value in ticks (0-255)
    5       ADAZ        Current dome azimuth in Ticks 0-32767
    6       Slave       Slave Status 0=slave off 1=slave on
    7       Shutter     Shutter status 0=indeterminate, 1=closed, 2=open
    8       DSR status  DSR Status 0=indet, 1=closed, 2=open
    9       Home        Home sensor 0=home, 1=not home
    10      HTICK_CCLK  Azimuth ticks of counterclockwise edge of Home position
    11      HTICK_CLK   Azimuth ticks of clockwise edge of Home position
    12      UPINS       Status of all user digital output pins
    13      WEAAGE      Age of weather info in minutes 0 to 255 (255 means expired)
    14      WINDDIR     0-255 wind direction (use (n/255)*359 to compute actual direction), subtract dome azimuth if weather module is mounted on dome top.
    15      WINDSPD     Windspeed 0-255 miles per hour
    16      TEMP        Temperature 0-255, representing -100 to 155 degrees F
    17      HUMID       Humidity 0-100% relative
    18      WETNESS     Wetness 0 (dry) to 100 (soaking wet)
    19      SNOW        Snow 0 (none) to 100 (sensor covered)
    20      WIND PEAK   Windspeed Peak level over session 0-255 miles per hour
    21      SCOPEAZ     Scope azimuth from LX-200 (999 if not available)
    22      INTDZ       Internal “deadzone”- angular displacement around the dome opening centerline within which desired dome azimuth can change without causing dome movement.
    23      INTOFF      Internal offset- angular distance DDW will add to the desired azimuth, causing the dome to preceed the telescope’s position when a slaved goto occurs.
    24      car ret
    25      car ret
    */

    int version;
    int dticks;
    int homepos;
    int coast;
    int azimuth;
    int slave;
    int shutter;
    int dsr;
    int home;
    int htick_cclk;
    int htick_clk;
    int upins;
    int weatherAge;
    int windDir;
    int windSpd;
    int temperature;
    int humidity;
    int wetness;
    int snow;
    int windPeak;
    int scopeAzimuth;
    int deadzone;
    int offset;

    int num = sscanf(response, "V%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &version,
                     &dticks, &homepos, &coast, &azimuth, &slave, &shutter, &dsr, &home, &htick_cclk, &htick_clk,
                     &upins, &weatherAge, &windDir, &windSpd, &temperature, &humidity, &wetness, &snow, &windPeak,
                     &scopeAzimuth, &deadzone, &offset);

    if (num == 23)
    {
        fwVersion   = version;
        ticksPerRev = dticks;
        homeAz = 360.0 * homepos / ticksPerRev;

        DomeAbsPosN[0].value = 360.0 * azimuth / ticksPerRev;

        ShutterState newState;
        switch (shutter)
        {
            case 1:
                newState = SHUTTER_CLOSED;
                break;
            case 2:
                newState = SHUTTER_OPENED;
                break;
            default:
                newState = SHUTTER_UNKNOWN;
                break;
        }

        // Only send if changed to avoid spam
        if (getShutterState() != newState)
        {
            setShutterState(newState);
        }
    }
}

/************************************************************************************
 *
* ***********************************************************************************/
void DDW::TimerHit()
{
    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    // The controller may write responses at any time due to hand controller usage
    // and command responses, so need to poll for them here
    while(true)
    {
        char errstr[MAXRBUF];
        char response[256] = { 0 };
        int nr;
        int rc = tty_read(PortFD, response, 1, 0, &nr);
        if (rc != TTY_OK || nr != 1)
        {
            // Nothing more to read
            if (++watchdog * getCurrentPollingPeriod() > 60 * 1000)
            {
                // If we haven't heard from the controller for 60s, send GINF command to keep DDW watchdog from triggering
                writeCmd("GINF");
                watchdog = 0; // prevent from triggering immediately again
            }
            break;
        }

        watchdog = 0;
        switch (response[0])
        {
            case 'L':
                LOG_DEBUG("Moving left");
                break;
            case 'R':
                LOG_DEBUG("Moving right");
                break;
            case 'T':
                LOG_DEBUG("Move tick");
                break;
            case 'P':
            {
                // Read tick count
                if ((rc = tty_nread_section(PortFD, response + 1, sizeof(response) - 2, '\r', DDW_TIMEOUT, &nr)) !=
                        TTY_OK)
                {
                    tty_error_msg(rc, errstr, MAXRBUF);
                    LOGF_ERROR("Error reading: %s.", errstr);
                }
                int tick = -1;
                sscanf(response, "P%d", &tick);
                LOGF_DEBUG("Tick counter %d", tick);

                // Update current position
                DomeAbsPosN[0].value = 359.0 * tick / ticksPerRev;
                IDSetNumber(&DomeAbsPosNP, nullptr);
                break;
            }
            case 'O':
                LOG_DEBUG("Opening shutter");
                break;
            case 'C':
                LOG_DEBUG("Closing shutter");
                break;
            case 'S':
                LOG_DEBUG("Shutter tick");
                break;
            case 'Z':
            {
                if ((rc = tty_nread_section(PortFD, response + 1, sizeof(response) - 2, '\r', DDW_TIMEOUT, &nr)) !=
                        TTY_OK)
                {
                    tty_error_msg(rc, errstr, MAXRBUF);
                    LOGF_ERROR("Error reading: %s.", errstr);
                }
                int tick = -1;
                sscanf(response, "Z%d", &tick);
                LOGF_DEBUG("Shutter encoder %d", tick);
                break;
            }
            case 'V':
            {
                // Move finished, read rest of GINF packet and parse it
                // Read buffer
                if ((rc = tty_nread_section(PortFD, response + 1, sizeof(response) - 2, '\r', DDW_TIMEOUT, &nr)) !=
                        TTY_OK)
                {
                    tty_error_msg(rc, errstr, MAXRBUF);
                    LOGF_ERROR("Error reading: %s.", errstr);
                }

                LOGF_DEBUG("GINF packet: %s", response);
                parseGINF(response);

                auto prevState = cmdState;
                cmdState = IDLE;

                // Check which operation was completed
                switch (getDomeState())
                {
                    case DOME_PARKING:
                        if(prevState == SHUTTER_OPERATION)
                        {
                            // First phase of parking done, now move to park position
                            DomeAbsPosNP.s = MoveAbs(GetAxis1Park());
                        }
                        else
                        {
                            SetParked(true);
                        }
                        break;
                    case DOME_UNPARKING:
                        SetParked(false);
                        break;
                    default:
                        if(prevState == MOVING)
                        {
                            if (gotoPending)
                            {
                                LOGF_DEBUG("Performing pending goto to %f", gotoTarget);
                                DomeAbsPosNP.s = MoveAbs(gotoTarget);
                                gotoPending = false;
                            }
                            else
                            {
                                LOG_DEBUG("Move complete");
                                setDomeState(DOME_SYNCED);
                            }
                        }
                        break;
                }
                IDSetNumber(&DomeAbsPosNP, nullptr);
                break;
            }
            default:
                LOGF_ERROR("Unknown response character %c", response[0]);
                break;
        }
    }
    SetTimer(getCurrentPollingPeriod());
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState DDW::MoveAbs(double az)
{
    LOGF_DEBUG("MoveAbs (%f)", az);

    if (cmdState == MOVING)
    {
        LOG_DEBUG("Dome already moving, latching the new move command");
        gotoTarget = az;
        gotoPending = true;
        return IPS_BUSY;
    }
    else if (cmdState != IDLE)
    {
        LOG_ERROR("Dome needs to be idle to issue moves");
        return IPS_ALERT;
    }

    // Construct move command
    int rounded = round(az); // Controller only supports degrees
    char cmd[5];
    snprintf(cmd, sizeof(cmd), "G%03d", rounded);
    cmdState = MOVING;
    writeCmd(cmd);
    return IPS_BUSY;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState DDW::Park()
{
    if (cmdState != IDLE)
    {
        LOG_ERROR("Dome needs to be idle to issue park");
        return IPS_ALERT;
    }

    // First close the shutter if wanted (moves to home position), then move to park position
    if (ShutterParkPolicyS[SHUTTER_CLOSE_ON_PARK].s == ISS_ON)
    {
        return ControlShutter(SHUTTER_CLOSE);
    }
    else
    {
        return MoveAbs(GetAxis1Park());
    }
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState DDW::UnPark()
{
    if (cmdState != IDLE)
    {
        LOG_ERROR("Dome needs to be idle to issue unpark");
        return IPS_ALERT;
    }

    if (ShutterParkPolicyS[SHUTTER_OPEN_ON_UNPARK].s == ISS_ON)
    {
        return ControlShutter(SHUTTER_OPEN);
    }
    return IPS_OK;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState DDW::ControlShutter(ShutterOperation operation)
{
    if (cmdState != IDLE)
    {
        LOG_ERROR("Dome needs to be idle for shutter operations");
        return IPS_ALERT;
    }
    if (operation == SHUTTER_OPEN)
    {
        LOG_INFO("Opening shutter");
        cmdState = SHUTTER_OPERATION;
        if (writeCmd("GOPN") != TTY_OK)
            return IPS_ALERT;
    }
    else
    {
        LOG_INFO("Closing shutter");
        cmdState = SHUTTER_OPERATION;
        if (writeCmd("GCLS") != TTY_OK)
            return IPS_ALERT;
    }
    return IPS_BUSY;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::Abort()
{
    writeCmd("!!"); // Any two characters not used for command will do
    cmdState = IDLE;
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::saveConfigItems(FILE *fp)
{
    return INDI::Dome::saveConfigItems(fp);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool DDW::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosN[0].value);
    return true;
}
/************************************************************************************
 *
* ***********************************************************************************/

bool DDW::SetDefaultPark()
{
    // By default set park position to home position
    SetAxis1Park(homeAz);
    return true;
}
