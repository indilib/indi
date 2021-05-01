/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

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

#include "temmadriver.h"

#include <indicom.h>
#include <connectionplugins/connectionserial.h>

#include <bitset>
#include <unistd.h>
#include <termios.h>

#define TEMMA_SLEW_RATES 2
#define TEMMA_TIMEOUT   5
#define TEMMA_BUFFER    64
#define TEMMA_SLEWRATE  5        /* slew rate, degrees/s */

// TODO enable Alignment System Later
#if 0
#include <alignment/DriverCommon.h>
using namespace INDI::AlignmentSubsystem;
#endif

// We declare an auto pointer to temma.
static std::unique_ptr<TemmaMount> temma(new TemmaMount());

TemmaMount::TemmaMount()
{
    SetTelescopeCapability(TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_PIER_SIDE, TEMMA_SLEW_RATES);

    SetParkDataType(PARK_RA_DEC);

    // Should be set to invalid first
    Longitude = std::numeric_limits<double>::quiet_NaN();
    Latitude  = std::numeric_limits<double>::quiet_NaN();

    setVersion(0, 5);
}

const char *TemmaMount::getDefaultName()
{
    return "Temma Takahashi";
}

bool TemmaMount::initProperties()
{
    INDI::Telescope::initProperties();

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    //  Temma runs at 19200 8 e 1
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    serialConnection->setParity(1);

    addAuxControls();

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");

    // TODO enable later
#if 0
    InitAlignmentProperties(this);

    // Force the alignment system to always be on
    getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE")->sp[0].s = ISS_ON;
#endif

    // Get value from config file if it exists.
    //    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &Longitude);
    //    currentRA  = get_local_sidereal_time(Longitude);
    //    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &Latitude);
    //    currentDEC = Latitude > 0 ? 90 : -90;

    return true;
}

#if 0
void TemmaMount::ISGetProperties(const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

    //defineProperty(&GuideNSNP);
    //defineProperty(&GuideWENP);

    // JM 2016-11-10: This is not used anywhere in the code now. Enable it again when you use it
    //defineProperty(&GuideRateNP);
}
#endif

bool TemmaMount::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, GuideNSNP.name) == 0 || strcmp(name, GuideWENP.name) == 0)
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }

        // Check alignment properties
        //ProcessAlignmentNumberProperties(this, name, values, names, n);
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool TemmaMount::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Check alignment properties
        //ProcessAlignmentSwitchProperties(this, name, states, names, n);
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool TemmaMount::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                           char *formats[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // It is for us
        //ProcessAlignmentBLOBProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool TemmaMount::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //ProcessAlignmentTextProperties(this, name, texts, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool TemmaMount::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        TrackState = motorsEnabled() ? SCOPE_TRACKING : SCOPE_IDLE;

        double lst = get_local_sidereal_time(Longitude);

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(range24(lst + 3 / 60.0));
            SetAxis2ParkDefault(Latitude >= 0 ? 90 : -90);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(range24(lst + 3 / 60.0));
            SetAxis2Park(Latitude >= 0 ? 90 : -90);
            SetAxis1ParkDefault(range24(lst + 3 / 60.0));
            SetAxis2ParkDefault(Latitude >= 0 ? 90 : -90);
        }

        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);

        // Load location so that it could trigger mount initiailization
        loadConfig(true, "GEOGRAPHIC_COORD");

    }
    else
    {
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
    }

    return true;
}

bool TemmaMount::sendCommand(const char *cmd, char *response)
{
    int bytesWritten = 0, bytesRead = 0, rc = 0;
    char errmsg[MAXRBUF];

    char cmd_temma[TEMMA_BUFFER] = {0};
    strncpy(cmd_temma, cmd, TEMMA_BUFFER);
    int cmd_size = strlen(cmd_temma);
    if (cmd_size - 2 >= TEMMA_BUFFER)
    {
        LOG_ERROR("Command is too long!");
        return false;
    }

    cmd_temma[cmd_size] = 0xD;
    cmd_temma[cmd_size + 1] = 0xA;

    // Special case for M since it has slewbits
    if (cmd[0] == 'M')
    {
        std::string binary = std::bitset<8>(cmd[1]).to_string();
        LOGF_DEBUG("CMD <M %s>", binary.c_str());
    }
    else
        LOGF_DEBUG("CMD <%s>", cmd);

    if (isSimulation())
    {
        if (response == nullptr)
            return true;

        switch (cmd[0])
        {
            // Version
            case 'v':
                strncpy(response, "vSimulation v1.0", TEMMA_BUFFER);
                break;

            // Get LST
            case 'g':
                if (TemmaInitialized == false || std::isnan(Longitude))
                    return false;
                else
                {
                    double lst = get_local_sidereal_time(Longitude);
                    snprintf(response, TEMMA_BUFFER, "%02d%02d%02d", (int)lst, ((int)(lst * 60)) % 60, ((int)(lst * 3600)) % 60);
                }
                break;

            // Get coords
            case 'E':
            {
                char sign;
                double dec = currentDEC;
                if (dec < 0)
                    sign = '-';
                else
                    sign = '+';

                dec = fabs(dec);

                // Computing meridian side is quite involved.. so for simulation now just set it to east always if slewing or parking
                char state = 'E';
                if (TrackState == SCOPE_PARKED || TrackState == SCOPE_IDLE || TrackState == SCOPE_TRACKING)
                    state = 'F';
                snprintf(response, TEMMA_BUFFER, "E%.2d%.2d%.2d%c%.2d%.2d%.1d%c", (int)currentRA, (int)(currentRA * (double)60) % 60,
                         ((int)(currentRA * (double)6000)) % 100, sign, (int)dec, (int)(dec * (double)60) % 60,
                         ((int)(dec * (double)600)) % 10, state);
            }
            break;

            // Sync
            case 'D':
                currentRA = targetRA;
                currentDEC = targetDEC;
                strncpy(response, "R0", TEMMA_BUFFER);
                break;

            // Goto
            case 'P':
                TrackState = SCOPE_SLEWING;
                strncpy(response, "R0", TEMMA_BUFFER);
                break;

            default:
                LOGF_ERROR("Command %s is unhandled in Simulation.", cmd);
                return false;
        }

        return true;
    }

    tcflush(PortFD, TCIOFLUSH);

    // Ask mount for current position
    if ( (rc = tty_write(PortFD, cmd_temma, strlen(cmd_temma), &bytesWritten)) != TTY_OK)
    {
        tty_error_msg(rc, errmsg, MAXRBUF);
        LOGF_ERROR("%s: %s", __FUNCTION__, errmsg);
        return false;
    }

    // If no response is required, let's return.
    if (response == nullptr)
        return true;

    memset(response, 0, TEMMA_BUFFER);

    if ( (rc = tty_nread_section(PortFD, response, TEMMA_BUFFER, 0xA, TEMMA_TIMEOUT, &bytesRead)) != TTY_OK)
    {
        tty_error_msg(rc, errmsg, MAXRBUF);
        LOGF_ERROR("%s: %s", __FUNCTION__, errmsg);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    response[bytesRead - 2] = 0;

    LOGF_DEBUG("RES <%s>", response);

    return true;
}

bool TemmaMount::getCoords()
{
    char response[TEMMA_BUFFER];

    bool rc = sendCommand("E", response);
    if (rc == false)
        return false;

    if (response[0] != 'E')
        return false;

    int d, m, s;
    sscanf(response + 1, "%02d%02d%02d", &d, &m, &s);
    //LOG_DEBUG"%d  %d  %d\n",d,m,s);
    currentRA = d * 3600 + m * 60 + s * .6;
    currentRA /= 3600;

    sscanf(response + 8, "%02d%02d%01d", &d, &m, &s);
    //LOG_DEBUG"%d  %d  %d\n",d,m,s);
    currentDEC = d * 3600 + m * 60 + s * 6;
    currentDEC /= 3600;
    if(response[7] == '-')
        currentDEC *= -1;

    char side = response[13];
    switch(side)
    {
        case 'E':
            setPierSide(PIER_EAST);
            break;

        case 'W':
            setPierSide(PIER_WEST);
            break;

        case 'F':
            //  lets see if our goto has finished
            if (strstr(response, "F") != nullptr)
            {
                if (TrackState == SCOPE_SLEWING)
                    TrackState = SCOPE_TRACKING;
                else if (TrackState == SCOPE_PARKING)
                {
                    SetParked(true);
                    //  turn off the motor
                    setMotorsEnabled(false);
                }
            }
            else
            {
                LOG_DEBUG("Goto in Progress...");
            }
            break;

    }

    return true;
}

bool TemmaMount::ReadScopeStatus()
{
    // JM: Do not read mount until it is initilaized.
    if (TemmaInitialized == false)
        return false;

    if (isSimulation())
    {
        mountSim();
        return true;
    }

    bool rc = getCoords();

    if (rc == false)
        return false;

    alignedRA = currentRA;
    alignedDEC = currentDEC;

    // TODO enable alignment system later
#if 0
    if (IsAlignmentSubsystemActive())
    {
        const char *maligns[3] = {"ZENITH", "NORTH", "SOUTH"};
        double juliandate, lst;
        //double alignedRA, alignedDEC;
        struct ln_equ_posn RaDec;
        bool aligned;
        juliandate = ln_get_julian_from_sys();
        lst = ln_get_apparent_sidereal_time(juliandate) + (LocationN[1].value * 24.0 / 360.0);
        // Use HA/Dec as  telescope coordinate system
        RaDec.ra = ((lst - currentRA) * 360.0) / 24.0;
        //RaDec.ra = (ra * 360.0) / 24.0;
        RaDec.dec = currentDEC;
        INDI::AlignmentSubsystem::TelescopeDirectionVector TDV = TelescopeDirectionVectorFromLocalHourAngleDeclination(RaDec);
        //AlignmentSubsystem::TelescopeDirectionVector TDV = TelescopeDirectionVectorFromEquatorialCoordinates(RaDec);
        DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Status: Mnt. Algnt. %s LST %lf RA %lf DEC %lf HA %lf TDV(x %lf y %lf z %lf)",
               maligns[GetApproximateMountAlignment()], lst, currentRA, currentDEC, RaDec.ra, TDV.x, TDV.y, TDV.z);

        aligned = true;

        if (!TransformTelescopeToCelestial(TDV, alignedRA, alignedDEC))
        {
            aligned = false;
            DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "Failed TransformTelescopeToCelestial: Scope RA=%g Scope DE=%f, Aligned RA=%f DE=%f", currentRA, currentDEC, alignedRA, alignedDEC);
        }
        else
        {
            DEBUGF(INDI::AlignmentSubsystem::DBG_ALIGNMENT, "TransformTelescopeToCelestial: Scope RA=%f Scope DE=%f, Aligned RA=%f DE=%f", currentRA, currentDEC, alignedRA, alignedDEC);
        }
    }

    if (!aligned && (syncdata.lst != 0.0))
    {
        DEBUGF(DBG_SCOPE_STATUS, "Aligning with last sync delta RA %g DE %g", syncdata.deltaRA, syncdata.deltaDEC);
        // should check values are in range!
        alignedRA += syncdata.deltaRA;
        alignedDEC += syncdata.deltaDEC;
        if (alignedDEC > 90.0 || alignedDEC < -90.0)
        {
            alignedRA += 12.00;
            if (alignedDEC > 0.0)
                alignedDEC = 180.0 - alignedDEC;
            else
                alignedDEC = -180.0 - alignedDEC;
        }
        alignedRA = range24(alignedRA);
    }
#endif

    NewRaDec(currentRA, currentDEC);

    // If NSWE directional slew is ongoing, continue to command the mount.
    if (SlewActive)
    {
        char cmd[3] = {0};
        cmd[0] = 'M';
        cmd[1] = Slewbits;

        sendCommand(cmd, nullptr);
    }

    return true;
}

bool TemmaMount::Sync(double ra, double dec)
{
    char cmd[TEMMA_BUFFER] = {0}, res[TEMMA_BUFFER] = {0};
    char sign;

    targetRA = ra;
    targetDEC = dec;

    /*  sync involves jumping thru considerable hoops
    first we have to set local sideral time
    then we have to send a Z
    then we set local sideral time again
    and finally we send the co-ordinates we are syncing on
    */
    LOG_DEBUG("Sending LST --> Z --> LST before Sync.");
    setLST();
    sendCommand("Z");
    setLST();

    //  now lets format up our sync command
    if (dec < 0)
        sign = '-';
    else
        sign = '+';

    dec = fabs(dec);

    snprintf(cmd, TEMMA_BUFFER, "D%.2d%.2d%.2d%c%.2d%.2d%.1d", (int)ra, (int)(ra * (double)60) % 60,
             ((int)(ra * (double)6000)) % 100, sign, (int)dec, (int)(dec * (double)60) % 60,
             ((int)(dec * (double)600)) % 10);

    if (sendCommand(cmd, res) == false)
        return false;

    //  if the first character is an R, it's a correct response
    if (res[0] != 'R')
        return false;

    if (res[1] != '0')
    {
        // 0 = success
        // 1 = RA error
        // 2 = Dec error
        // 3 = To many digits
        return false;
    }

    return true;
}

bool TemmaMount::Goto(double ra, double dec)
{
    char cmd[TEMMA_BUFFER] = {0}, res[TEMMA_BUFFER] = {0};
    char sign;

    targetRA = ra;
    targetDEC = dec;

    /*  goto involves hoops, but, not as many as a sync
        first set sideral time
        then issue the goto command
    */
    if (MotorStatus == false)
    {
        LOG_DEBUG("Goto turns on motors");
        setMotorsEnabled(true);
    }

    setLST();

    //  now lets format up our goto command
    if (dec < 0)
        sign = '-';
    else
        sign = '+';

    dec = fabs(dec);

    snprintf(cmd, TEMMA_BUFFER, "P%.2d%.2d%.2d%c%.2d%.2d%.1d", (int)ra, (int)(ra * (double)60) % 60,
             ((int)(ra * (double)6000)) % 100, sign, (int)dec, (int)(dec * (double)60) % 60,
             ((int)(dec * (double)600)) % 10);

    if (sendCommand(cmd, res) == false)
        return false;

    //  if the first character is an R, it's a correct response
    if (res[0] != 'R')
        return false;
    if (res[1] != '0')
    {
        // 0 = success
        // 1 = RA error
        // 2 = Dec error
        // 3 = To many digits
        return false;
    }

    TrackState = SCOPE_SLEWING;
    return true;
}

bool TemmaMount::Park()
{
    double lst = get_local_sidereal_time(Longitude);

    // Set Axis 1 parking to LST + 3 minutes as a safe offset
    // so that a GOTO to it would work
    // this would end up with mount looking at pole with counter-weight down
    SetAxis1Park(range24(lst + 3 / 60.0));
    LOGF_DEBUG("heading to Park position %4.2f %4.2f", GetAxis1Park(), GetAxis2Park());

    Goto(GetAxis1Park(), GetAxis2Park());

    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking is in progress...");

    return true;
}

bool TemmaMount::UnPark()
{
    // Get LST and set it as Axis1 Park position
    double lst = get_local_sidereal_time(Longitude);
    SetAxis1Park(lst);

    LOGF_INFO("Syncing to Park position %4.2f %4.2f", GetAxis1Park(), GetAxis2Park());
    Sync(GetAxis1Park(), GetAxis2Park());

    SetParked(false);

    setMotorsEnabled(true);
    if (MotorStatus)
        TrackState = SCOPE_TRACKING;
    else
        TrackState = SCOPE_IDLE;

    return true;
}

bool TemmaMount::SetCurrentPark()
{
    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);
    return true;
}

bool TemmaMount::SetDefaultPark()
{
    double lst = get_local_sidereal_time(Longitude);
    SetAxis1Park(range24(lst + 3 / 60.0));
    SetAxis2Park(Latitude >= 0 ? 90 : -90);

    return true;
}

bool TemmaMount::Abort()
{
    char res[TEMMA_BUFFER] = {0};

    if (sendCommand("PS") == false)
        return false;

    if (sendCommand("s", res) == false)
        return false;

    return true;
}

bool TemmaMount::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    char cmd[TEMMA_BUFFER] = {0};

    LOGF_DEBUG("Temma::MoveNS %d dir %d", command, dir);
    if (!MotorStatus)
        setMotorsEnabled(true);
    if (!MotorStatus)
        return false;

    Slewbits = 0;
    Slewbits |= 64; //  dec says always on

    LOGF_DEBUG("Temma::MoveNS %d dir %d", command, dir);
    if (command == MOTION_START)
    {
        if (SlewRate != 0)
            Slewbits |= 1;
        if (dir != DIRECTION_NORTH)
        {
            LOG_DEBUG("Start slew Dec Up");
            Slewbits |= 16;
        }
        else
        {
            LOG_DEBUG("Start Slew Dec down");
            Slewbits |= 8;
        }
        //sprintf(buf,"M \r\n");
        //buf[1]=Slewbits;
        //tty_write(PortFD,buf,4,&bytesWritten);
        SlewActive = true;
    }
    else
    {
        //  No direction bytes to turn it off
        LOG_DEBUG("Abort slew e/w");
        //Abort();
        SlewActive = false;
    }
    cmd[0] = 'M';
    cmd[1] = Slewbits;

    return sendCommand(cmd);
}

bool TemmaMount::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    char cmd[TEMMA_BUFFER] = {0};

    if (!MotorStatus)
        setMotorsEnabled(true);
    if (!MotorStatus)
        return false;

    Slewbits = 0;
    Slewbits |= 64; //  documentation says always on

    LOGF_DEBUG("Temma::MoveWE %d dir %d", command, dir);
    if (command == MOTION_START)
    {
        if (SlewRate != 0)
            Slewbits |= 1;
        if (dir != DIRECTION_WEST)
        {
            LOG_DEBUG("Start slew East");
            Slewbits |= 4;
        }
        else
        {
            LOG_DEBUG("Start Slew West");
            Slewbits |= 2;
        }
        SlewActive = true;
    }
    else
    {
        //  No direction bytes to turn it off
        LOG_DEBUG("Abort slew e/w");
        SlewActive = false;
    }

    cmd[0] = 'M';
    cmd[1] = Slewbits;

    return sendCommand(cmd);
}

#if 0
bool TemmaMount::SetSlewRate(int index)
{
    // Is this possible for Temma?
    //LOGF_DEBUG("Temma::Slew rate %d", index);
    SlewRate = index;
    return true;
}
#endif

// TODO For large ms > 1000ms this function should be asynchronous
IPState TemmaMount::GuideNorth(uint32_t ms)
{
    char cmd[TEMMA_BUFFER] = {0};
    char bits;

    LOGF_DEBUG("Guide North %4.0f", ms);
    if (!MotorStatus)
        return IPS_ALERT;
    if (SlewActive)
        return IPS_ALERT;

    bits = 0;
    bits |= 64; //  documentation says always on
    bits |= 8;  //  going north
    cmd[0] = 'M';
    cmd[1] = bits;
    sendCommand(cmd);
    usleep(ms * 1000);
    bits   = 64;
    cmd[1] = bits;
    sendCommand(cmd);

    return IPS_OK;
}

IPState TemmaMount::GuideSouth(uint32_t ms)
{
    char cmd[TEMMA_BUFFER] = {0};
    char bits;

    LOGF_DEBUG("Guide South %4.0f", ms);

    if (!MotorStatus)
        return IPS_ALERT;
    if (SlewActive)
        return IPS_ALERT;

    bits = 0;
    bits |= 64; //  documentation says always on
    bits |= 16; //  going south
    cmd[0] = 'M';
    cmd[1] = bits;
    sendCommand(cmd);
    usleep(ms * 1000);
    bits   = 64;
    cmd[1] = bits;
    sendCommand(cmd);

    return IPS_OK;
}

IPState TemmaMount::GuideEast(uint32_t ms)
{
    char cmd[TEMMA_BUFFER] = {0};
    char bits;
    LOGF_DEBUG("Guide East %4.0f", ms);
    if (!MotorStatus)
        return IPS_ALERT;
    if (SlewActive)
        return IPS_ALERT;

    bits = 0;
    bits |= 64; //  doc says always on
    bits |= 2;  //  going east
    cmd[0] = 'M';
    cmd[1] = bits;
    sendCommand(cmd);
    usleep(ms * 1000);
    bits   = 64;
    cmd[1] = bits;
    sendCommand(cmd);
    return IPS_OK;
}

IPState TemmaMount::GuideWest(uint32_t ms)
{
    char cmd[TEMMA_BUFFER] = {0};
    char bits;
    LOGF_DEBUG("Guide West %4.0f", ms);
    if (!MotorStatus)
        return IPS_ALERT;
    if (SlewActive)
        return IPS_ALERT;

    bits = 0;
    bits |= 64; //  documentation says always on
    bits |= 4;  //  going west
    cmd[0] = 'M';
    cmd[1] = bits;
    sendCommand(cmd);
    usleep(ms * 1000);
    bits   = 64;
    cmd[1] = bits;
    sendCommand(cmd);

    return IPS_OK;
}

#if 0
bool TemmaMount::updateTime(ln_date *utc, double utc_offset)
{
    INDI_UNUSED(utc);
    INDI_UNUSED(utc_offset);
    LOG_DEBUG("Temma::UpdateTime()");
    return true;
}
#endif

bool TemmaMount::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    Longitude = longitude;
    Latitude = latitude;

    double lst = get_local_sidereal_time(Longitude);

    //  A temma mount must have the LST and Latitude set
    //  Prior to these being set, reads will return garbage
    if (TemmaInitialized == false)
    {
        setLatitude(latitude);
        setLST();

        TemmaInitialized = true;

        //  We were NOT initialized, so, in case there is not park position set
        //  Sync to the position of bar vertical, telescope pointed at pole
        LOGF_DEBUG("Temma is initilized. Latitude: %.2f LST: %.2f", latitude, lst);
        SetAxis1Park(lst);

        LOGF_INFO("Syncing to default home position %4.2f %4.2f", GetAxis1Park(), GetAxis2Park());
        Sync(GetAxis1Park(), GetAxis2Park());
    }

#if 0
    //  if the mount is parked, then we should sync it on our park position
    else if (isParked())
    {
        //        //  Here we have to sync on our park position
        //        double RightAscension;
        //        //  Get the park position
        //        RightAscension = lst - (rangeHA(GetAxis1Park()));
        //        RightAscension = range24(RightAscension);

        double lst = get_local_sidereal_time(Longitude);
        SetAxis1Park(lst);

        LOGF_INFO("Syncing to Park position %4.2f %4.2f", GetAxis1Park(), GetAxis2Park());
        Sync(GetAxis1Park(), GetAxis2Park());
        LOG_DEBUG("Turning motors off.");
        setMotorsEnabled(false);
    }
    else
    {
        sleep(1);
        LOG_DEBUG("Mount is not parked");
        //SetTemmaMotorStatus(true);
    }
#endif

    return true;
}

#if 0
ln_equ_posn TemmaMount::TelescopeToSky(double ra, double dec)
{
    double RightAscension, Declination;
    ln_equ_posn eq { 0, 0 };

    if (GetAlignmentDatabase().size() > 1)
    {
        TelescopeDirectionVector TDV;

        /*  Use this if we ar converting eq co-ords
        //  but it's broken for now
            eq.ra=ra*360/24;
            eq.dec=dec;
            TDV=TelescopeDirectionVectorFromEquatorialCoordinates(eq);
        */

        /* This code does a conversion from ra/dec to alt/az
        // before calling the alignment stuff
            ln_lnlat_posn here;
            ln_hrz_posn altaz;

            here.lat=LocationN[LOCATION_LATITUDE].value;
            here.lng=LocationN[LOCATION_LONGITUDE].value;
            eq.ra=ra*360.0/24.0;	//  this is wanted in degrees, not hours
            eq.dec=dec;
            ln_get_hrz_from_equ(&eq,&here,ln_get_julian_from_sys(),&altaz);
            TDV=TelescopeDirectionVectorFromAltitudeAzimuth(altaz);
        */

        /*  and here we convert from ra/dec to hour angle / dec before calling alignment stuff */
        double lha, lst;
        lst = get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
        lha = get_local_hour_angle(lst, ra);
        //  convert lha to degrees
        lha    = lha * 360 / 24;
        eq.ra  = lha;
        eq.dec = dec;
        TDV    = TelescopeDirectionVectorFromLocalHourAngleDeclination(eq);

        if (TransformTelescopeToCelestial(TDV, RightAscension, Declination))
        {
            //  if we get here, the conversion was successful
            //LOG_DEBUG"new values %6.4f %6.4f %6.4f  %6.4f Deltas %3.0lf %3.0lf\n",ra,dec,RightAscension,Declination,(ra-RightAscension)*60,(dec-Declination)*60);
        }
        else
        {
            //if the conversion failed, return raw data
            RightAscension = ra;
            Declination    = dec;
        }
    }
    else
    {
        //  With less than 2 align points
        // Just return raw data
        RightAscension = ra;
        Declination    = dec;
    }

    eq.ra  = RightAscension;
    eq.dec = Declination;
    return eq;
}

ln_equ_posn TemmaMount::SkyToTelescope(double ra, double dec)
{
    ln_equ_posn eq { 0, 0 };
    TelescopeDirectionVector TDV;
    double RightAscension, Declination;

    /*
    */

    if (GetAlignmentDatabase().size() > 1)
    {
        //  if the alignment system has been turned off
        //  this transformation will fail, and we fall thru
        //  to using raw co-ordinates from the mount
        if (TransformCelestialToTelescope(ra, dec, 0.0, TDV))
        {
            /*  Initial attemp, using RA/DEC co-ordinates talking to alignment system
            EquatorialCoordinatesFromTelescopeDirectionVector(TDV,eq);
            RightAscension=eq.ra*24.0/360;
            Declination=eq.dec;
            if(RightAscension < 0) RightAscension+=24.0;
            DEBUGF(INDI::Logger::DBG_SESSION,"Transformed Co-ordinates %g %g\n",RightAscension,Declination);
            */

            //  nasty altaz kludge, use al/az co-ordinates to talk to alignment system
            /*
                eq.ra=ra*360/24;
                eq.dec=dec;
                here.lat=LocationN[LOCATION_LATITUDE].value;
                here.lng=LocationN[LOCATION_LONGITUDE].value;
                ln_get_hrz_from_equ(&eq,&here,ln_get_julian_from_sys(),&altaz);
            AltitudeAzimuthFromTelescopeDirectionVector(TDV,altaz);
            //  now convert the resulting altaz into radec
                ln_get_equ_from_hrz(&altaz,&here,ln_get_julian_from_sys(),&eq);
            RightAscension=eq.ra*24.0/360.0;
            Declination=eq.dec;
                */

            /* now lets convert from telescope to lha/dec */
            double lst;
            LocalHourAngleDeclinationFromTelescopeDirectionVector(TDV, eq);
            //  and now we have to convert from lha back to RA
            lst            = get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
            eq.ra          = eq.ra * 24 / 360;
            RightAscension = lst - eq.ra;
            RightAscension = range24(RightAscension);
            Declination    = eq.dec;
        }
        else
        {
            LOGF_INFO("Transform failed, using raw co-ordinates %g %g", ra, dec);
            RightAscension = ra;
            Declination    = dec;
        }
    }
    else
    {
        RightAscension = ra;
        Declination    = dec;
    }

    eq.ra  = RightAscension;
    eq.dec = Declination;
    //eq.ra=ra;
    //eq.dec=dec;
    return eq;
}
#endif

bool TemmaMount::getVersion()
{
    char res[TEMMA_BUFFER] = {0};
    for (int i = 0; i < 3; i++)
    {
        if (sendCommand("v", res) && res[0] == 'v')
            break;
        usleep(100000);
    }

    if (res[0] != 'v')
    {
        LOG_ERROR("Read version failed.");
        return false;
    }


    LOGF_INFO("Detected version: %s", res + 1);

    return true;
}

bool TemmaMount::motorsEnabled()
{
    char res[TEMMA_BUFFER] = {0};

    if (sendCommand("STN-COD", res) == false)
        return false;

    if (strstr(res, "off") != nullptr)
        MotorStatus = true;
    else
        MotorStatus = false;

    LOGF_DEBUG("Motor is %s", res);

    return MotorStatus;
}

bool TemmaMount::setMotorsEnabled(bool enable)
{
    char res[TEMMA_BUFFER] = {0};
    bool rc = false;

    // STN-ON  --> Standby mode ON  --> Motor OFF
    // STN-OFF --> Standby mode OFF --> Motor ON
    if (enable)
        rc = sendCommand("STN-OFF", res);
    else
        rc = sendCommand("STN-ON", res);

    if (rc == false)
        return false;

    motorsEnabled();

    return true;
}

/*  bit of a hack, returns true if lst is a sane value, false if it is not sane */
bool TemmaMount::getLST(double &lst)
{
    char res[TEMMA_BUFFER] = {0};

    if (sendCommand("g", res) == false)
        return false;

    int hh = 0, mm = 0, ss = 0;
    if (sscanf(res + 1, "%02d%02d%02d", &hh, &mm, &ss) == 3)
    {
        lst = hh + mm / 60.0 + ss / 3600.0;
        return true;
    }

    return false;
}

bool TemmaMount::setLST()
{
    char cmd[TEMMA_BUFFER] = {0};
    double lst = get_local_sidereal_time(Longitude);
    snprintf(cmd, TEMMA_BUFFER, "T%.2d%.2d%.2d", (int)lst, ((int)(lst * 60)) % 60, ((int)(lst * 3600)) % 60);

    return sendCommand(cmd);
}

bool TemmaMount::getLatitude(double &lat)
{
    char res[TEMMA_BUFFER] = {0};

    if (sendCommand("i", res) == false)
        return false;

    int dd = 0, mm = 0, parial_m = 0;
    if (sscanf(res + 1, "%02d%02d%01d", &dd, &mm, &parial_m) == 3)
    {
        lat = dd + mm / 60.0 + parial_m / 600.0;
        return true;
    }

    return false;
}

bool TemmaMount::setLatitude(double lat)
{
    char cmd[TEMMA_BUFFER];
    char sign;
    double l;
    int d, m, s;

    if (lat > 0)
    {
        sign = '+';
    }
    else
    {
        sign = '-';
    }
    l = fabs(lat);
    d = (int)l;
    l = (l - d) * 60;
    m = (int)l;
    l = (l - m) * 6;
    s = (int)l;

    snprintf(cmd, TEMMA_BUFFER, "I%c%.2d%.2d%.1d", sign, d, m, s);

    return sendCommand(cmd);
}

bool TemmaMount::Handshake()
{
    //LOG_DEBUG("Calling get version");
    /*
        This is an ugly hack, but, it works
        On first open we often dont get an immediate read from the temma
        but it seems to read much more reliably if we enforce a short wait
        between opening the port and doing the first query for version

    */
    usleep(100);
    if (getVersion() == false)
        return false;

    double lst = 0;
    if (getLST(lst))
    {
        LOG_DEBUG("Temma is initialized.");
        TemmaInitialized = true;
    }
    else
    {
        LOG_DEBUG("Temma is not initialized.");
        TemmaInitialized = false;
    }

    motorsEnabled();

    return true;
}

#if 0
int TemmaMount::TemmaRead(char *buf, int size)
{
    int bytesRead = 0;
    int count     = 0;
    int ptr       = 0;
    int rc        = 0;

    while (ptr < size)
    {
        rc = tty_read(PortFD, &buf[ptr], 1, 2,
                      &bytesRead); //  Read 1 byte of response into the buffer with 5 second timeout
        if (bytesRead == 1)
        {
            //  we got a byte
            count++;
            if (count > 1)
            {
                //LOG_DEBUG"%d ",buf[ptr]);
                if (buf[ptr] == 10)
                {
                    if (buf[ptr - 1] == 13)
                    {
                        //  we have the cr/lf from the response
                        //  Send it back to the caller
                        return count;
                    }
                }
            }
            ptr++;
        }
        else
        {
            fprintf(stderr, "timed out reading %d\n", count);
            LOGF_DEBUG("We timed out reading bytes %d", count);
            return 0;
        }
    }
    //  if we get here, we got more than size bytes, and still dont have a cr/lf
    fprintf(stderr, "Read error after %d bytes\n", bytesRead);
    LOGF_DEBUG("Read return error after %d bytes", bytesRead);
    return -1;
}
#endif

bool TemmaMount::SetTrackMode(uint8_t mode)
{
    if (mode == TRACK_SIDEREAL)
        return sendCommand("LL");
    else if (mode == TRACK_SOLAR)
        return sendCommand("LK");

    return false;
}

bool TemmaMount::SetTrackEnabled(bool enabled)
{
    if (enabled)
    {
        setMotorsEnabled(true);
        return SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP));
    }
    else
    {
        setMotorsEnabled(false);
        return sendCommand("PS");
    }
}

void TemmaMount::mountSim()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt = 0, da = 0, dx = 0;
    int nlocked = 0;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&tv, nullptr);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt  = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec) / 1e6;
    ltv = tv;
    da  = TEMMA_SLEWRATE * dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
    switch (TrackState)
    {

        case SCOPE_IDLE:
            currentRA  += (TRACKRATE_SIDEREAL / 3600.0 * dt / 15.);
            break;

        case SCOPE_TRACKING:
            switch (IUFindOnSwitchIndex(&TrackModeSP))
            {
                case TRACK_SIDEREAL:
                    da = 0;
                    dx = 0;
                    break;

                case TRACK_LUNAR:
                    da = ((TRACKRATE_LUNAR - TRACKRATE_SIDEREAL) / 3600.0 * dt / 15.);
                    dx = 0;
                    break;

                case TRACK_SOLAR:
                    da = ((TRACKRATE_SOLAR - TRACKRATE_SIDEREAL) / 3600.0 * dt / 15.);
                    dx = 0;
                    break;

                case TRACK_CUSTOM:
                    da = ((TrackRateN[AXIS_RA].value - TRACKRATE_SIDEREAL) / 3600.0 * dt / 15.);
                    dx = (TrackRateN[AXIS_DE].value / 3600.0 * dt);
                    break;

            }

            currentRA  += da;
            currentDEC += dx;
            break;

        case SCOPE_SLEWING:
        case SCOPE_PARKING:
            /* slewing - nail it when both within one pulse @ LX200_GENERIC_SLEWRATE */
            nlocked = 0;

            dx = targetRA - currentRA;

            if (fabs(dx) <= da)
            {
                currentRA = targetRA;
                nlocked++;
            }
            else if (dx > 0)
                currentRA += da / 15.;
            else
                currentRA -= da / 15.;

            dx = targetDEC - currentDEC;
            if (fabs(dx) <= da)
            {
                currentDEC = targetDEC;
                nlocked++;
            }
            else if (dx > 0)
                currentDEC += da;
            else
                currentDEC -= da;

            if (nlocked == 2)
            {
                if (TrackState == SCOPE_SLEWING)
                    TrackState = SCOPE_TRACKING;
                else
                    SetParked(true);
            }

            break;

        default:
            break;
    }

    NewRaDec(currentRA, currentDEC);
}
