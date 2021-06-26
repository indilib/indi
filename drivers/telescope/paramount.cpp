/*******************************************************************************
 Copyright(c) 2017 Jasem Mutlaq. All rights reserved.
 2021 Chris Lewicki. Refactor of TCP connections and error handling

 Driver for using TheSkyX Pro Scripted operations for mounts via the TCP server.
 While this technically can operate any mount connected to the TheSkyX Pro, it is
 intended for Paramount mounts control.

 Ref TheSky Functions:
 https://www.bisque.com/wp-content/scripttheskyx/classsky6_r_a_s_c_o_m_tele.html

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

/*******************************************************************************
 * TODO for improving / completing the driver
 *
 * 1. SlewToAZAlt()
 * 2. GetAzAlt()
 * 3. (3) Presets for SlewToAZAlt positions
 * 4. DoCommand(16) get/set atmospheric pressure for interfacing with other INDI devices
*******************************************************************************/

#include "paramount.h"

#include "indicom.h"

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>

// We declare an auto pointer to Paramount.
std::unique_ptr<Paramount> paramount_mount(new Paramount());

#define GOTO_RATE      5         /* slew rate, degrees/s */
#define SLEW_RATE      0.5       /* slew rate, degrees/s */
#define FINE_SLEW_RATE 0.1       /* slew rate, degrees/s */

#define GOTO_LIMIT      5.5 /* Move at GOTO_RATE until distance from target is GOTO_LIMIT degrees */
#define SLEW_LIMIT      1   /* Move at SLEW_LIMIT until distance from target is SLEW_LIMIT degrees */

#define PARAMOUNT_TIMEOUT 3 /* Timeout in seconds */
#define PARAMOUNT_NORTH   0
#define PARAMOUNT_SOUTH   1
#define PARAMOUNT_EAST    2
#define PARAMOUNT_WEST    3

#define RA_AXIS  0
#define DEC_AXIS 1

/* Preset Slew Speeds */
#define SLEWMODES 9
const double slewspeeds[SLEWMODES] = { 1.0, 2.0, 4.0, 8.0, 32.0, 64.0, 128.0, 256.0, 512.0 };

Paramount::Paramount()
{
    setVersion(1, 3);

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_HAS_TRACK_RATE |
                           TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_PIER_SIDE,
                           9);
    setTelescopeConnection(CONNECTION_TCP);

    m_NSTimer.setSingleShot(true);
    m_WETimer.setSingleShot(true);

    // Called when timer is up
    m_NSTimer.callOnTimeout([this]()
    {
        GuideNSNP.s = IPS_IDLE;
        GuideNSN[0].value = GuideNSN[1].value = 0;
        IDSetNumber(&GuideNSNP, nullptr);
    });

    m_WETimer.callOnTimeout([this]()
    {
        GuideWENP.s = IPS_IDLE;
        GuideWEN[0].value = GuideWEN[1].value = 0;
        IDSetNumber(&GuideWENP, nullptr);
    });
}

const char *Paramount::getDefaultName()
{
    return "Paramount";
}

bool Paramount::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    for (int i = 0; i < SlewRateSP.nsp - 1; i++)
    {
        sprintf(SlewRateSP.sp[i].label, "%.fx", slewspeeds[i]);
        SlewRateSP.sp[i].aux = (void *)&slewspeeds[i];
    }

    // Set 64x as default speed
    SlewRateSP.sp[5].s = ISS_ON;

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&JogRateN[RA_AXIS], "JOG_RATE_WE", "W/E Rate (arcmin)", "%g", 0, 600, 60, 30);
    IUFillNumber(&JogRateN[DEC_AXIS], "JOG_RATE_NS", "N/S Rate (arcmin)", "%g", 0, 600, 60, 30);
    IUFillNumberVector(&JogRateNP, JogRateN, 2, getDeviceName(), "JOG_RATE", "Jog Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[RA_AXIS], "GUIDE_RATE_WE", "W/E Rate", "%1.1f", 0.0, 1.0, 0.1, 0.5);
    IUFillNumber(&GuideRateN[DEC_AXIS], "GUIDE_RATE_NS", "N/S Rate", "%1.1f", 0.0, 1.0, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Homing
    IUFillSwitch(&HomeS[0], "GO", "Go", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 1, getDeviceName(), "TELESCOPE_HOME", "Homing", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);
    // Tracking Mode
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Let's simulate it to be an F/7.5 120mm telescope with 50m 175mm guide scope
    ScopeParametersN[0].value = 120;
    ScopeParametersN[1].value = 900;
    ScopeParametersN[2].value = 50;
    ScopeParametersN[3].value = 175;

    TrackState = SCOPE_IDLE;

    SetParkDataType(PARK_HA_DEC);

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    addAuxControls();

    currentRA  = get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
    currentDEC = LocationN[LOCATION_LATITUDE].value > 0 ? 90 : -90;
    return true;
}

bool Paramount::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        if (isTheSkyTracking())
        {
            IUResetSwitch(&TrackModeSP);
            TrackModeS[TRACK_SIDEREAL].s = ISS_ON;
            TrackState = SCOPE_TRACKING;
        }
        else
        {
            IUResetSwitch(&TrackModeSP);
            TrackState = SCOPE_IDLE;
        }

        //defineProperty(&TrackModeSP);
        //defineProperty(&TrackRateNP);

        defineProperty(&JogRateNP);

        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(&GuideRateNP);

        // Initial HA to 0 and currentDEC (+90 or -90)
        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(currentDEC);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(0);
            SetAxis2Park(currentDEC);
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(currentDEC);
        }

        SetParked(isTheSkyParked());

        defineProperty(&HomeSP);
    }
    else
    {
        //deleteProperty(TrackModeSP.name);
        //deleteProperty(TrackRateNP.name);

        deleteProperty(JogRateNP.name);

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.name);
        deleteProperty(HomeSP.name);
    }

    return true;
}

/*******************************************************************************
* Note that for all successful TheSky TCP requests, the following string is
* prepended to the result:
*
*    |No error. Error = 0.
*
* This is true everwhere except for the Handshake(), which just returns "1" on success.
*
* In order to know when the response is complete, we append the # character in
* Javascript commands and read from the port until the # character is reached.
*******************************************************************************/

bool Paramount::Handshake()
{
    if (isSimulation())
        return true;

    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF] = {0}, pRES[MAXRBUF] = {0};

    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "sky6RASCOMTele.ConnectAndDoNotUnpark();"
            "Out = sky6RASCOMTele.IsConnected + '#';",
            MAXRBUF);

    LOGF_DEBUG("CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        LOGF_ERROR("Error writing Handshake to TheSkyX TCP server. Result: %d", rc);
        return false;
    }

    if ((rc = tty_read_section(PortFD, pRES, '#', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        LOGF_ERROR("Error reading Handshake from TheSkyX TCP server. Result: %d", rc);
        return false;
    }

    if (strcmp(pRES, "1#") != 0)
    {
        LOGF_ERROR("Error connecting to TheSky. Result: %s", pRES);
        return false;
    }

    return true;
}

bool Paramount::getMountRADE()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF] = {0}, pRES[MAXRBUF] = {0};
    double SkyXRA = 0., SkyXDEC = 0.;

    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "sky6RASCOMTele.GetRaDec();"
            "Out = String(sky6RASCOMTele.dRa) + ',' + String(sky6RASCOMTele.dDec) + '#';",
            MAXRBUF);

    LOGF_DEBUG("CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        LOGF_ERROR("Error writing GetRaDec to TheSkyX TCP server. Response: %d", rc);
        return false;
    }

    if ((rc = tty_read_section(PortFD, pRES, '#', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        LOGF_ERROR("Error reading GetRaDec from TheSkyX TCP server. Result: %d", rc);
        return false;
    }

    LOGF_DEBUG("RES: %s", pRES);

    // Read results successfully into temporary values before committing
    if (sscanf(pRES, "|No error. Error = 0.%lf,%lf#", &SkyXRA, &SkyXDEC) == 2)
    {
        currentRA  = SkyXRA;
        currentDEC = SkyXDEC;
        return true;
    }

    LOGF_ERROR("Error reading coordinates. Result: %s", pRES);
    return false;
}

INDI::Telescope::TelescopePierSide Paramount::getPierSide()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF] = {0}, pRES[MAXRBUF] = {0};
    int SkyXPierSide = -1;

    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "sky6RASCOMTele.DoCommand(11, \"Pier Side\");"
            "Out = sky6RASCOMTele.DoCommandOutput + '#';",
            MAXRBUF);

    LOGF_DEBUG("CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        LOGF_ERROR("Error writing DoCommand(Pier Side) to TheSkyX TCP server. Result: %d", rc);
        return PIER_UNKNOWN;
    }

    if ((rc = tty_read_section(PortFD, pRES, '#', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        LOGF_ERROR("Error reading Pier Side from TheSkyX TCP server. Result: %d", rc);
        return PIER_UNKNOWN;
    }

    LOGF_DEBUG("RES: %s", pRES);

    if (sscanf(pRES, "|No error. Error = 0.%d#", &SkyXPierSide) == 1)
    {
        return SkyXPierSide == 0 ? PIER_WEST : PIER_EAST;
    }

    LOGF_ERROR("Error reading Pier Side. Result: %s", pRES);
    return PIER_UNKNOWN;
}

bool Paramount::ReadScopeStatus()
{
    if (isSimulation())
    {
        mountSim();
        return true;
    }

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if Scope is done slewing
        if (isSlewComplete())
        {
            TrackState = SCOPE_TRACKING;

            if (HomeSP.s == IPS_BUSY)
            {
                IUResetSwitch(&HomeSP);
                HomeSP.s = IPS_OK;
                LOG_INFO("Finding home completed.");
            }
            else
                LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (isTheSkyParked())
        {
            SetParked(true);
        }
    }

    if (!getMountRADE())
        return false;

    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, currentRA, 2, 3600);
    fs_sexa(DecStr, currentDEC, 2, 3600);

    DEBUGF(DBG_SCOPE, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    setPierSide(getPierSide());

    NewRaDec(currentRA, currentDEC);
    return true;
}

bool Paramount::Goto(double r, double d)
{
    targetRA  = r;
    targetDEC = d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    char pCMD[MAXRBUF] = {0};
    snprintf(pCMD, MAXRBUF,
             "sky6RASCOMTele.Asynchronous = true;"
             "sky6RASCOMTele.SlewToRaDec(%g, %g,'');",
             targetRA, targetDEC);

    if (!sendTheSkyOKCommand(pCMD, "Slewing to target"))
        return false;

    TrackState = SCOPE_SLEWING;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool Paramount::isSlewComplete()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF] = {0}, pRES[MAXRBUF] = {0};

    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "Out = sky6RASCOMTele.IsSlewComplete + '#';",
            MAXRBUF);

    LOGF_DEBUG("CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        LOGF_ERROR("Error writing IsSlewComplete to TheSkyX TCP server. Result: %d", rc);
        return false;
    }

    if ((rc = tty_read_section(PortFD, pRES, '#', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        LOGF_ERROR("Error reading IsSlewComplete from TheSkyX TCP server. Result: %d", rc);
        return false;
    }

    LOGF_DEBUG("RES: %s", pRES);

    int isComplete = 0;
    if (sscanf(pRES, "|No error. Error = 0.%d#", &isComplete) == 1)
    {
        return  isComplete == 1 ? 1 : 0;
    }

    LOGF_ERROR("Error reading isSlewComplete. Result: %s", pRES);
    return false;
}

bool Paramount::isTheSkyParked()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF] = {0}, pRES[MAXRBUF] = {0};

    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "Out = sky6RASCOMTele.IsParked() + '#';",
            MAXRBUF);

    LOGF_DEBUG("CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        LOGF_ERROR("Error writing sky6RASCOMTele.IsParked() to TheSkyX TCP server. Result: %d", rc);
        return false;
    }

    if ((rc = tty_read_section(PortFD, pRES, '#', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        LOGF_ERROR("Error reading sky6RASCOMTele.IsParked() from TheSkyX TCP server. Result: %d", rc);
        return false;
    }

    LOGF_DEBUG("RES: %s", pRES);

    if (strcmp(pRES, "|No error. Error = 0.true#") == 0)
        return true;
    if (strcmp(pRES, "|No error. Error = 0.false#") == 0)
        return false;

    LOGF_ERROR("Error checking for park. Invalid response: %s", pRES);
    return false;
}

bool Paramount::isTheSkyTracking()
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF] = {0}, pRES[MAXRBUF] = {0};

    strncpy(pCMD,
            "/* Java Script */"
            "var Out;"
            "Out = sky6RASCOMTele.IsTracking + '#';",
            MAXRBUF);

    LOGF_DEBUG("CMD: %s", pCMD);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        LOGF_ERROR("Error writing sky6RASCOMTele.IsTracking to TheSkyX TCP server. Result: %d", rc);
        return false;
    }

    if ((rc = tty_read_section(PortFD, pRES, '#', PARAMOUNT_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        LOGF_ERROR("Error reading sky6RASCOMTele.IsTracking from TheSkyX TCP server. Result: %d", rc);
        return false;
    }

    LOGF_DEBUG("RES: %s", pRES);

    double SkyXTrackRate = 0.;
    if (sscanf(pRES, "|No error. Error = 0.%lf#", &SkyXTrackRate) == 1)
    {
        if (SkyXTrackRate == 0)
            return false;
        else if (SkyXTrackRate > 0)
            return true;
    }

    LOGF_ERROR("Error checking for tracking. Invalid response: %s", pRES);
    return false;
}

bool Paramount::Sync(double ra, double dec)
{
    char pCMD[MAXRBUF] = {0};

    snprintf(pCMD, MAXRBUF, "sky6RASCOMTele.Sync(%g, %g,'');", targetRA, targetDEC);
    if (!sendTheSkyOKCommand(pCMD, "Syncing to target"))
        return false;

    currentRA  = ra;
    currentDEC = dec;

    LOG_INFO("Sync is successful.");

    EqNP.s = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool Paramount::Park()
{
    double targetHA = GetAxis1Park();
    targetRA  = range24(get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value) - targetHA);
    targetDEC = GetAxis2Park();

    char pCMD[MAXRBUF] = {0};
    strncpy(pCMD,
            "sky6RASCOMTele.Asynchronous = true;"
            "sky6RASCOMTele.ParkAndDoNotDisconnect();",
            MAXRBUF);

    if (!sendTheSkyOKCommand(pCMD, "Parking mount"))
        return false;
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking telescope in progress...");

    return true;
}

bool Paramount::UnPark()
{
    char pCMD[MAXRBUF] = {0};
    strncpy(pCMD, "sky6RASCOMTele.Unpark();", MAXRBUF);
    if (!sendTheSkyOKCommand(pCMD, "Unparking mount"))
        return false;

    // Confirm we unparked
    if (isTheSkyParked())
        LOG_ERROR("Could not unpark for some reason.");
    else
        SetParked(false);

    return true;
}

bool Paramount::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "JOG_RATE") == 0)
        {
            IUUpdateNumber(&JogRateNP, values, names, n);
            JogRateNP.s = IPS_OK;
            IDSetNumber(&JogRateNP, nullptr);
            return true;
        }

        // Guiding Rate
        if (strcmp(name, GuideRateNP.name) == 0)
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);
            return true;
        }

        if (strcmp(name, GuideNSNP.name) == 0 || strcmp(name, GuideWENP.name) == 0)
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }
    }

    //  if we didn't process it, continue up the chain, let somebody else give it a shot
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool Paramount::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(HomeSP.name, name))
        {
            LOG_INFO("Moving to home position. Please stand by...");
            if (findHome())
            {
                HomeS[0].s = ISS_OFF;
                TrackState = SCOPE_IDLE;
                HomeSP.s = IPS_OK;
                LOG_INFO("Mount arrived at home position.");
            }
            else
            {
                HomeS[0].s = ISS_OFF;
                HomeSP.s = IPS_ALERT;
                LOG_ERROR("Failed to go to home position");
            }

            IDSetSwitch(&HomeSP, nullptr);
            return true;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool Paramount::Abort()
{
    char pCMD[MAXRBUF] = {0};

    strncpy(pCMD, "sky6RASCOMTele.Abort();", MAXRBUF);
    return sendTheSkyOKCommand(pCMD, "Abort mount slew");
}

bool Paramount::findHome()
{
    char pCMD[MAXRBUF] = {0};

    strncpy(pCMD, "sky6RASCOMTele.FindHome();"
            "while(!sky6RASCOMTele.IsSlewComplete) {"
            "sky6Web.Sleep(1000);}",
            MAXRBUF);
    return sendTheSkyOKCommand(pCMD, "Find home", 60);
}


bool Paramount::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    int motion = (dir == DIRECTION_NORTH) ? PARAMOUNT_NORTH : PARAMOUNT_SOUTH;
    //int rate   = IUFindOnSwitchIndex(&SlewRateSP);
    int rate = slewspeeds[IUFindOnSwitchIndex(&SlewRateSP)];

    switch (command)
    {
        case MOTION_START:
            if (!isSimulation() && !startOpenLoopMotion(motion, rate))
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (motion == PARAMOUNT_NORTH) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (!isSimulation() && !stopOpenLoopMotion())
            {
                LOG_ERROR("Error stopping N/S motion.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s halted.",
                          (motion == PARAMOUNT_NORTH) ? "North" : "South");
            break;
    }

    return true;
}

bool Paramount::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    int motion = (dir == DIRECTION_WEST) ? PARAMOUNT_WEST : PARAMOUNT_EAST;
    int rate   = IUFindOnSwitchIndex(&SlewRateSP);

    switch (command)
    {
        case MOTION_START:
            if (!isSimulation() && !startOpenLoopMotion(motion, rate))
            {
                LOG_ERROR("Error setting W/E motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (motion == PARAMOUNT_WEST) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (!isSimulation() && !stopOpenLoopMotion())
            {
                LOG_ERROR("Error stopping W/E motion.");
                return false;
            }
            else
                LOGF_INFO("Movement toward %s halted.",
                          (motion == PARAMOUNT_WEST) ? "West" : "East");
            break;
    }

    return true;
}

bool Paramount::startOpenLoopMotion(uint8_t motion, uint16_t rate)
{
    char pCMD[MAXRBUF] = {0};

    snprintf(pCMD, MAXRBUF, "sky6RASCOMTele.DoCommand(9,'%d|%d');", motion, rate);
    return sendTheSkyOKCommand(pCMD, "Starting open loop motion");
}

bool Paramount::stopOpenLoopMotion()
{
    char pCMD[MAXRBUF] = {0};

    strncpy(pCMD, "sky6RASCOMTele.DoCommand(10,'');", MAXRBUF);
    return sendTheSkyOKCommand(pCMD, "Stopping open loop motion");
}

bool Paramount::updateTime(ln_date *utc, double utc_offset)
{
    INDI_UNUSED(utc);
    INDI_UNUSED(utc_offset);
    return true;
}

bool Paramount::SetCurrentPark()
{
    char pCMD[MAXRBUF] = {0};

    strncpy(pCMD, "sky6RASCOMTele.SetParkPosition();", MAXRBUF);
    if (!sendTheSkyOKCommand(pCMD, "Setting Park Position"))
        return false;

    double lst = get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
    double ha  = get_local_hour_angle(lst, currentRA);

    SetAxis1Park(ha);
    SetAxis2Park(currentDEC);

    return true;
}

bool Paramount::SetDefaultPark()
{
    // By default set HA to 0
    SetAxis1Park(0);

    // Set DEC to 90 or -90 depending on the hemisphere
    SetAxis2Park((LocationN[LOCATION_LATITUDE].value > 0) ? 90 : -90);

    return true;
}

bool Paramount::SetParkPosition(double Axis1Value, double Axis2Value)
{
    INDI_UNUSED(Axis1Value);
    INDI_UNUSED(Axis2Value);
    LOG_ERROR("Setting custom parking position directly is not supported. Slew to the desired "
              "parking position and click Current.");
    return false;
}

void Paramount::mountSim()
{
    static struct timeval ltv
    {
        0, 0
    };
    struct timeval tv
    {
        0, 0
    };
    double dt, dx, da_ra = 0, da_dec = 0;
    int nlocked;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&tv, nullptr);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt  = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec) / 1e6;
    ltv = tv;

    if (fabs(targetRA - currentRA) * 15. >= GOTO_LIMIT)
        da_ra = GOTO_RATE * dt;
    else if (fabs(targetRA - currentRA) * 15. >= SLEW_LIMIT)
        da_ra = SLEW_RATE * dt;
    else
        da_ra = FINE_SLEW_RATE * dt;

    if (fabs(targetDEC - currentDEC) >= GOTO_LIMIT)
        da_dec = GOTO_RATE * dt;
    else if (fabs(targetDEC - currentDEC) >= SLEW_LIMIT)
        da_dec = SLEW_RATE * dt;
    else
        da_dec = FINE_SLEW_RATE * dt;

    double motionRate = 0;

    if (MovementNSSP.s == IPS_BUSY)
        motionRate = JogRateN[0].value;
    else if (MovementWESP.s == IPS_BUSY)
        motionRate = JogRateN[1].value;

    if (motionRate != 0)
    {
        da_ra  = motionRate * dt * 0.05;
        da_dec = motionRate * dt * 0.05;

        switch (MovementNSSP.s)
        {
            case IPS_BUSY:
                if (MovementNSS[DIRECTION_NORTH].s == ISS_ON)
                    currentDEC += da_dec;
                else if (MovementNSS[DIRECTION_SOUTH].s == ISS_ON)
                    currentDEC -= da_dec;
                break;

            default:
                break;
        }

        switch (MovementWESP.s)
        {
            case IPS_BUSY:
                if (MovementWES[DIRECTION_WEST].s == ISS_ON)
                    currentRA += da_ra / 15.;
                else if (MovementWES[DIRECTION_EAST].s == ISS_ON)
                    currentRA -= da_ra / 15.;
                break;

            default:
                break;
        }

        NewRaDec(currentRA, currentDEC);
        return;
    }

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
    switch (TrackState)
    {
        case SCOPE_IDLE:
            /* RA moves at sidereal, Dec stands still */
            currentRA += (TRACKRATE_SIDEREAL / 3600.0 * dt / 15.);
            break;

        case SCOPE_SLEWING:
        case SCOPE_PARKING:
            /* slewing - nail it when both within one pulse @ SLEWRATE */
            nlocked = 0;

            dx = targetRA - currentRA;

            // Take shortest path
            if (fabs(dx) > 12)
                dx *= -1;

            if (fabs(dx) <= da_ra)
            {
                currentRA = targetRA;
                nlocked++;
            }
            else if (dx > 0)
                currentRA += da_ra / 15.;
            else
                currentRA -= da_ra / 15.;

            if (currentRA < 0)
                currentRA += 24;
            else if (currentRA > 24)
                currentRA -= 24;

            dx = targetDEC - currentDEC;
            if (fabs(dx) <= da_dec)
            {
                currentDEC = targetDEC;
                nlocked++;
            }
            else if (dx > 0)
                currentDEC += da_dec;
            else
                currentDEC -= da_dec;

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

bool Paramount::sendTheSkyOKCommand(const char *command, const char *errorMessage, uint8_t timeout)
{
    int rc = 0, nbytes_written = 0, nbytes_read = 0;
    char pCMD[MAXRBUF] = {0}, pRES[MAXRBUF] = {0};

    snprintf(pCMD, MAXRBUF,
             "/* Java Script */"
             "var Out;"
             "try {"
             "%s"
             "Out  = 'OK#'; }"
             "catch (err) {Out = err; }",
             command);

    LOGF_DEBUG("CMD: %s", pCMD);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write_string(PortFD, pCMD, &nbytes_written)) != TTY_OK)
    {
        LOGF_ERROR("Error writing sendTheSkyOKCommand to TheSkyX TCP server. Result: $%d", rc);
        return false;
    }

    // No response is requested.
    if (errorMessage == nullptr)
    {
        return true;
    }

    if ((rc = tty_read_section(PortFD, pRES, '#', timeout, &nbytes_read)) != TTY_OK)
    {
        LOGF_ERROR("Error reading sendTheSkyOKCommand from TheSkyX TCP server. Result: %d", rc);
        return false;
    }

    LOGF_DEBUG("RES: %s", pRES);

    tcflush(PortFD, TCIOFLUSH);

    if (strcmp("|No error. Error = 0.OK#", pRES) == 0 )
        return true;
    else
    {
        LOGF_ERROR("sendTheSkyOKCommand Error %s - Invalid response: %s", errorMessage, pRES);
        return false;
    }
}

IPState Paramount::GuideNorth(uint32_t ms)
{
    return GuideNS(static_cast<int>(ms));
}

IPState Paramount::GuideSouth(uint32_t ms)
{
    return GuideNS(-static_cast<int>(ms));
}

IPState Paramount::GuideEast(uint32_t ms)
{
    return GuideWE(static_cast<int>(ms));
}

IPState Paramount::GuideWest(uint32_t ms)
{
    return GuideWE(-static_cast<int>(ms));
}

/* Note: Handling Guide requests synchronously resources in serial implementation of
   moves for each axis, when they could be handled concurrently with timers */
IPState Paramount::GuideNS(int32_t ms)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return IPS_ALERT;
    }

    // Movement in arcseconds
    double dDec = GuideRateN[DEC_AXIS].value * TRACKRATE_SIDEREAL * ms / 1000.0;
    char pCMD[MAXRBUF] = {0};
    snprintf(pCMD, MAXRBUF,
             "sky6RASCOMTele.Asynchronous = true;"
             "sky6DirectGuide.MoveTelescope(%g, %g);", 0., dDec);

    // Send async and don't wait
    if (!sendTheSkyOKCommand(pCMD, nullptr))
        return IPS_ALERT;

    m_NSTimer.start(ms);

    return IPS_BUSY;
}

IPState Paramount::GuideWE(int32_t ms)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return IPS_ALERT;
    }

    // Movement in arcseconds
    double dRA  = GuideRateN[RA_AXIS].value * TRACKRATE_SIDEREAL * ms / 1000.0;
    char pCMD[MAXRBUF] = {0};
    snprintf(pCMD, MAXRBUF,
             "sky6RASCOMTele.Asynchronous = true;"
             "sky6DirectGuide.MoveTelescope(%g, %g);", dRA, 0.);

    // Send async and do not wait
    if (!sendTheSkyOKCommand(pCMD, nullptr))
        return IPS_ALERT;

    m_WETimer.start(ms);

    return IPS_BUSY;
}

bool Paramount::setTheSkyTracking(bool enable, bool isSidereal, double raRate, double deRate)
{
    int on     = enable ? 1 : 0;
    int ignore = isSidereal ? 1 : 0;
    char pCMD[MAXRBUF] = {0};

    snprintf(pCMD, MAXRBUF, "sky6RASCOMTele.SetTracking(%d, %d, %g, %g);", on, ignore, raRate, deRate);
    return sendTheSkyOKCommand(pCMD, "Setting tracking rate");
}

bool Paramount::SetTrackRate(double raRate, double deRate)
{
    return setTheSkyTracking(true, false, raRate, deRate);
}

bool Paramount::SetTrackMode(uint8_t mode)
{
    bool isSidereal = (mode == TRACK_SIDEREAL);
    double dRA = 0, dDE = 0;

    if (mode == TRACK_SOLAR)
        dRA = TRACKRATE_SOLAR;
    else if (mode == TRACK_LUNAR)
        dRA = TRACKRATE_LUNAR;
    else if (mode == TRACK_CUSTOM)
    {
        dRA = TrackRateN[RA_AXIS].value;
        dDE = TrackRateN[DEC_AXIS].value;
    }
    return setTheSkyTracking(true, isSidereal, dRA, dDE);
}

bool Paramount::SetTrackEnabled(bool enabled)
{
    // On engaging track, we simply set the current track mode and it will take care of the rest including custom track rates.
    if (enabled)
        return SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP));
    else
        // Otherwise, simply switch everything off
        return setTheSkyTracking(0, 0, 0., 0.);
}
