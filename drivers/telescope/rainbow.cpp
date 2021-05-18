/*
    LX200 Rainbow Driver
    Copyright (C) 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "rainbow.h"
#include "lx200driver.h"

#include <connectionplugins/connectionserial.h>
#include <indicom.h>

#include <cstring>
#include <cmath>
#include <termios.h>
#include <regex>

static std::unique_ptr<Rainbow> scope(new Rainbow());

Rainbow::Rainbow() : INDI::Telescope ()
{
    setVersion(1, 1);

    SetTelescopeCapability(TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_HAS_PIER_SIDE_SIMULATION, 4);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
const char *Rainbow::getDefaultName()
{
    return "Rainbow";
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::initProperties()
{
    INDI::Telescope::initProperties();

    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    SetParkDataType(PARK_AZ_ALT);

    // Homing
    IUFillSwitch(&HomeS[0], "HOME", "Go Home", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 1, getDeviceName(), "HOME", "Homing", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                       IPS_IDLE);

    // Horizontal Coords
    IUFillNumber(&HorizontalCoordsN[AXIS_AZ], "AZ", "Az D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
    IUFillNumber(&HorizontalCoordsN[AXIS_ALT], "ALT", "Alt  D:M:S", "%10.6m", -90., 90.0, 0.0, 0);
    IUFillNumberVector(&HorizontalCoordsNP, HorizontalCoordsN, 2, getDeviceName(), "HORIZONTAL_COORD",
                       "Horizontal Coord", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Guide");

    IUFillNumber(&GuideRateN[0], "GUIDE_RATE", "x Sidereal", "%g", 0.1, 1.0, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 1, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    addDebugControl();

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        getStartupStatus();

        defineProperty(&HorizontalCoordsNP);
        defineProperty(&HomeSP);

        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(&GuideRateNP);

    }
    else
    {
        deleteProperty(HorizontalCoordsNP.name);
        deleteProperty(HomeSP.name);

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.name);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Horizontal Coordinates
        if (!strcmp(name, HorizontalCoordsNP.name))
        {
            int i = 0, nset = 0;
            double newAlt = -1, newAz = -1;
            INumber *horp = nullptr;

            for (nset = i = 0; i < n; i++)
            {
                if (horp == &HorizontalCoordsN[AXIS_ALT])
                {
                    newAlt = values[i];
                    nset += newAlt >= -90. && newAlt <= 90.0;
                }
                else if (horp == &HorizontalCoordsN[AXIS_AZ])
                {
                    newAz = values[i];
                    nset += newAz >= 0. && newAz <= 360.0;
                }
            }

            if (nset == 2)
            {
                if (slewToHorizontalCoords(newAz, newAlt))
                {
                    TrackState = SCOPE_SLEWING;
                    HorizontalCoordsNP.s = IPS_BUSY;
                    IDSetNumber(&HorizontalCoordsNP, nullptr);
                    return true;
                }
                else
                {
                    HorizontalCoordsNP.s = IPS_ALERT;
                    IDSetNumber(&HorizontalCoordsNP, nullptr);
                    LOG_ERROR("Failed to slew to target coordinates.");
                    return true;
                }
            }
            else
            {
                HorizontalCoordsNP.s = IPS_ALERT;
                IDSetNumber(&HorizontalCoordsNP, "Altitude or Azimuth missing or invalid");
                return true;
            }
        }
        // Guide Rate
        else if (!strcmp(name, GuideRateNP.name))
        {
            if (setGuideRate(values[0]))
            {
                IUUpdateNumber(&GuideRateNP, values, names, n);
                GuideRateNP.s = IPS_OK;
                LOGF_INFO("Guide rate updated to %.2fX sidereal rate.", values[0]);
            }
            else
                GuideRateNP.s = IPS_ALERT;

            IDSetNumber(&GuideRateNP, nullptr);
            return true;
        }
        else
            processGuiderProperties(name, values, names, n);
    }


    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Homing
        if (!strcmp(HomeSP.name, name))
        {
            if (HomeSP.s == IPS_BUSY)
            {
                LOG_WARN("Homing is already in progress.");
                return true;
            }

            HomeSP.s = findHome() ? IPS_BUSY : IPS_ALERT;
            if (HomeSP.s == IPS_BUSY)
            {
                TrackState = SCOPE_SLEWING;
                HomeS[0].s = ISS_ON;
                LOG_INFO("Mount is moving to home position...");
            }
            else
                LOG_ERROR("Mount failed to move to home position.");

            IDSetSwitch(&HomeSP, nullptr);
            return true;
        }

    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void Rainbow::getStartupStatus()
{
    LOGF_INFO("Detected firmware %s", m_Version.c_str());

    if (getTrackingState())
        IDSetSwitch(&TrackStateSP, nullptr);
    if (getGuideRate())
        IDSetNumber(&GuideRateNP, nullptr);

    double longitude = 0, latitude = 90;
    // Get value from config file if it exists.
    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(latitude >= 0 ? 0 : 180);
        SetAxis2ParkDefault(latitude);
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(latitude >= 0 ? 0 : 180);
        SetAxis2Park(latitude);
        SetAxis1ParkDefault(latitude >= 0 ? 0 : 180);
        SetAxis2ParkDefault(latitude);
    }

    sendScopeLocation();
    sendScopeTime();
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    char cmd[DRIVER_LEN] = {0};
    int degrees, minutes, seconds;

    // Convert from INDI standard to regular east/west -180 to 180
    if (longitude > 180)
        longitude -= 360;

    // Rainbow defines EAST as - and WEST as +
    // which is opposite of the standard
    longitude *= -1;

    getSexComponents(longitude, &degrees, &minutes, &seconds);

    snprintf(cmd, DRIVER_LEN, ":Sg%c%03d*%02d'%02d#", longitude >= 0 ? '+' : '-', std::abs(degrees), minutes, seconds);

    if (!sendCommand(cmd))
        return false;

    getSexComponents(latitude, &degrees, &minutes, &seconds);

    snprintf(cmd, DRIVER_LEN, ":St%c%02d*%02d'%02d#", latitude >= 0 ? '+' : '-', std::abs(degrees), minutes, seconds);

    return sendCommand(cmd);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
//bool Rainbow::updateTime(ln_date *utc, double utc_offset)
//{

//}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::Handshake()
{
    return getFirmwareVersion();
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::getFirmwareVersion()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand(":AV#", res) == false)
        return false;

    // :AV190905 --> 190905
    m_Version = std::string(res + 3);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::SetTrackEnabled(bool enabled)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, ":Ct%c#", enabled ? 'A' : 'L');
    return sendCommand(cmd);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::getTrackingState()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand(":AT#", res) == false)
        return false;

    TrackStateS[TRACK_ON].s = (res[3] == '1') ? ISS_ON : ISS_OFF;
    TrackStateS[TRACK_OFF].s = (res[3] == '0') ? ISS_ON : ISS_OFF;
    TrackStateSP.s = (TrackStateS[TRACK_ON].s == ISS_ON) ? IPS_BUSY : IPS_IDLE;

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::findHome()
{
    return sendCommand(":Ch#");
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::setGuideRate(double rate)
{
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, ":Cu0=%3.1f#", rate);
    return sendCommand(cmd);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::getGuideRate()
{
    char res[DRIVER_LEN] = {0};
    char rate[4] = {0};

    if (!sendCommand(":CU0#", res))
        return false;

    memcpy(rate, res + 5, 3);

    GuideRateN[0].value = std::stod(rate);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::SetCurrentPark()
{
    SetAxis1Park(m_CurrentAZ);
    SetAxis2Park(m_CurrentAL);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::SetDefaultPark()
{
    SetAxis1Park(0);
    SetAxis2Park(0);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::Park()
{
    double parkAZ  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    if (slewToHorizontalCoords(parkAZ, parkAlt))
    {
        TrackState = SCOPE_PARKING;
        HorizontalCoordsNP.s = IPS_BUSY;
        IDSetNumber(&HorizontalCoordsNP, nullptr);
        LOG_INFO("Parking is in progress...");
        return true;
    }

    return false;

}

/////////////////////////////////////////////////////////////////////////////////////
/// Unparking
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::UnPark()
{
    if (SetTrackEnabled(true))
    {
        TrackStateS[TRACK_ON].s = ISS_ON;
        TrackStateS[TRACK_OFF].s = ISS_OFF;
        TrackStateSP.s = IPS_BUSY;
        IDSetSwitch(&TrackStateSP, nullptr);

        SetParked(false);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::isSlewComplete()
{
    char res[DRIVER_LEN] = {0};
    int nbytes_read = 0;
    if (tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, 1, &nbytes_read) == TTY_OK)
    {
        LOGF_DEBUG("SlewCheck <%s>", res);
        if (!strcmp(res, ":MM0#") || !strcmp(res, ":CHO#"))
        {
            m_SlewErrorCode = 0;
            return true;
        }
        else if (!strcmp(res, ":MML#"))
        {
            m_SlewErrorCode = 1;
        }
        else if (!strcmp(res, ":MMU#"))
        {
            m_SlewErrorCode = 2;
        }
        else if (!strcmp(res, ":MME#"))
        {
            m_SlewErrorCode = 3;
        }
        else if (!strcmp(res, ":CH0#"))
        {
            m_SlewErrorCode = 4;
        }
        else if (!strcmp(res, ":CH<#"))
        {
            m_SlewErrorCode = 5;
        }
    }


    return false;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if Slewing is complete
        if (isSlewComplete())
        {
            HorizontalCoordsNP.s = IPS_OK;
            IDSetNumber(&HorizontalCoordsNP, nullptr);

            if (HomeSP.s == IPS_BUSY)
            {
                LOG_INFO("Homing completed successfully.");
                HomeSP.s = IPS_OK;
                HomeS[0].s = ISS_OFF;
                IDSetSwitch(&HomeSP, nullptr);
                TrackState = SCOPE_IDLE;
            }
            else
            {
                TrackState = SCOPE_TRACKING;
                // For Horizontal Goto, we must explicitly set tracking ON again.
                if (m_GotoType == Horizontal)
                    SetTrackEnabled(true);
                LOG_INFO("Slew is complete. Tracking...");
            }
        }
        else if (m_SlewErrorCode > 0)
        {
            HorizontalCoordsNP.s = IPS_ALERT;
            IDSetNumber(&HorizontalCoordsNP, nullptr);

            EqNP.s = IPS_ALERT;

            if (HomeSP.s == IPS_BUSY)
            {
                TrackState = SCOPE_IDLE;
                HomeSP.s = IPS_ALERT;
                HomeS[0].s = ISS_OFF;
                IDSetSwitch(&HomeSP, nullptr);
                LOGF_ERROR("Homing error: %s", getSlewErrorString(m_SlewErrorCode).c_str());
            }
            else
            {
                // JM TODO CHECK: Does the mount RESUME tracking after slew failure or it completely stops idle?
                TrackState = m_GotoType == Horizontal ? SCOPE_IDLE : SCOPE_TRACKING;
                LOGF_ERROR("Slewing error: %s", getSlewErrorString(m_SlewErrorCode).c_str());
            }
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (isSlewComplete())
        {
            SetParked(true);
            HorizontalCoordsNP.s = IPS_OK;
            IDSetNumber(&HorizontalCoordsNP, nullptr);
        }
        else if (m_SlewErrorCode > 0)
        {
            HorizontalCoordsNP.s = IPS_ALERT;
            EqNP.s = IPS_ALERT;
            // JM TODO CHECK: Does the mount RESUME tracking after slew failure or it completely stops idle?
            TrackState = m_GotoType == Horizontal ? SCOPE_IDLE : SCOPE_TRACKING;
            LOGF_ERROR("Parking error: %s", getSlewErrorString(m_SlewErrorCode).c_str());
            IDSetNumber(&HorizontalCoordsNP, nullptr);
        }
    }

    // Equatorial Coords
    if (!getRA() || !getDE())
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
        return false;
    }

    // Horizontal Coords
    if (!getAZ() || !getAL())
    {
        HorizontalCoordsNP.s = IPS_ALERT;
    }
    else
    {
        HorizontalCoordsN[AXIS_AZ].value = m_CurrentAZ;
        HorizontalCoordsN[AXIS_ALT].value = m_CurrentAL;
    }
    IDSetNumber(&HorizontalCoordsNP, nullptr);

    NewRaDec(m_CurrentRA, m_CurrentDE);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Slew RA/DE
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::Goto(double ra, double dec)
{
    const struct timespec timeout = {0, 100000000L};

    char RAStr[64] = {0}, DecStr[64] = {0};
    fs_sexa(RAStr, ra, 2, 36000);
    fs_sexa(DecStr, dec, 2, 36000);

    // If moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
        if (!isSimulation() && Abort() == false)
        {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
        }

        AbortSP.s = IPS_OK;
        EqNP.s    = IPS_IDLE;
        IDSetSwitch(&AbortSP, "Slew aborted.");
        IDSetNumber(&EqNP, nullptr);

        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
            MovementNSSP.s = IPS_IDLE;
            MovementWESP.s = IPS_IDLE;
            EqNP.s = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
        }

        // sleep for 100 mseconds
        nanosleep(&timeout, nullptr);
    }

    if (slewToEquatorialCoords(ra, dec) == false)
    {
        LOGF_ERROR("Error Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
        return false;
    }

    TrackState = SCOPE_SLEWING;
    LOGF_INFO("Slewing to RA: %s - DE: %s", RAStr, DecStr);

    // Also set Horizontal Coors to BUSY
    HorizontalCoordsNP.s = IPS_BUSY;
    IDSetNumber(&HorizontalCoordsNP, nullptr);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Get RA
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::getRA()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand(":GR#", res) == false)
        return false;

    // Skip :GR and read value
    return (f_scansexa(res + 3, &m_CurrentRA) == 0);
}

/////////////////////////////////////////////////////////////////////////////
/// Get DE
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::getDE()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand(":GD#", res) == false)
        return false;

    // Skip :GD and read value
    return (f_scansexa(res + 3, &m_CurrentDE) == 0);
}


/////////////////////////////////////////////////////////////////////////////
/// Set Target RA
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::setRA(double ra)
{
    char cmd[DRIVER_LEN] = {0};
    char res[DRIVER_LEN] = {0};
    int degrees, minutes;
    double seconds;

    getSexComponentsIID(ra, &degrees, &minutes, &seconds);

    snprintf(cmd, DRIVER_LEN, ":Sr%02d:%02d:%04.1f#", degrees, minutes, seconds);

    if (!sendCommand(cmd, res, -1, 1))
        return false;

    return res[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
/// Set Target Altitude
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::setDE(double de)
{
    char cmd[DRIVER_LEN] = {0};
    char res[DRIVER_LEN] = {0};
    int degrees, minutes;
    double seconds;

    getSexComponentsIID(de, &degrees, &minutes, &seconds);

    snprintf(cmd, DRIVER_LEN, ":Sd%c%02d*%02d:%04.1f#", de >= 0 ? '+' : '-', std::abs(degrees), minutes, seconds);

    if (!sendCommand(cmd, res, -1, 1))
        return false;

    return res[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
/// Slew to Equatorial Coordinates
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::slewToEquatorialCoords(double ra, double de)
{
    if (!setRA(ra) || !setDE(de))
        return false;

    if (sendCommand(":MS#"))
    {
        char RAStr[16], DEStr[16];
        fs_sexa(RAStr, ra, 2, 36000);
        fs_sexa(DEStr, de, 2, 36000);
        LOGF_DEBUG("Slewing to RA (%s) DE (%s)...", RAStr, DEStr);
        m_GotoType = Equatorial;
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Get Azimuth
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::getAZ()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand(":GZ#", res) == false)
        return false;

    // Skip :GZ and read value
    return (f_scansexa(res + 3, &m_CurrentAZ) == 0);
}

/////////////////////////////////////////////////////////////////////////////
/// Get Altitude
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::getAL()
{
    char res[DRIVER_LEN] = {0};
    if (sendCommand(":GA#", res) == false)
        return false;

    // Skip :GA and read value
    return (f_scansexa(res + 3, &m_CurrentAL) == 0);
}

/////////////////////////////////////////////////////////////////////////////
/// Set Target Azimuth
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::setAZ(double azimuth)
{
    char cmd[DRIVER_LEN] = {0};
    int degrees, minutes;
    double seconds;

    getSexComponentsIID(azimuth, &degrees, &minutes, &seconds);

    snprintf(cmd, DRIVER_LEN, ":Sz%03d*%02d:%04.1f#", degrees, minutes, seconds);

    return sendCommand(cmd);
}

/////////////////////////////////////////////////////////////////////////////
/// Set Target Altitude
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::setAL(double altitude)
{
    char cmd[DRIVER_LEN] = {0};
    int degrees, minutes;
    double seconds;

    getSexComponentsIID(altitude, &degrees, &minutes, &seconds);

    snprintf(cmd, DRIVER_LEN, ":Sa%c%02d*%02d:%04.1f#", degrees >= 0 ? '+' : '-', std::abs(degrees), minutes, seconds);

    return sendCommand(cmd);
}

/////////////////////////////////////////////////////////////////////////////
/// Slew to Horizontal Coordinates
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::slewToHorizontalCoords(double azimuth, double altitude)
{
    if (!setAZ(azimuth) || !setAL(altitude))
        return false;

    if (sendCommand(":MA#"))
    {
        char AzStr[16], AltStr[16];
        fs_sexa(AzStr, azimuth, 2, 36000);
        fs_sexa(AltStr, altitude, 2, 36000);
        LOGF_DEBUG("Slewing to Az (%s) Alt (%s)...", AzStr, AltStr);
        m_GotoType = Horizontal;
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Abort
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    switch (command)
    {
        case MOTION_START:
            if (!sendCommand(dir == DIRECTION_NORTH ? ":Mn#" : ":Ms#"))
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_DEBUG("Moving toward %s.", (dir == DIRECTION_NORTH) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (!sendCommand(":Q#"))
            {
                LOG_ERROR("Error stopping N/S motion.");
                return false;
            }
            else
                LOGF_DEBUG("Movement toward %s halted.", (dir == DIRECTION_NORTH) ? "North" : "South");
            break;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// W/E Motion
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    switch (command)
    {
        case MOTION_START:
            if (!sendCommand(dir == DIRECTION_WEST ? ":Mw#" : ":Me#"))
            {
                LOG_ERROR("Error setting W/E motion direction.");
                return false;
            }
            else
                LOGF_DEBUG("Moving toward %s.", (dir == DIRECTION_WEST) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (!sendCommand(":Q#"))
            {
                LOG_ERROR("Error stopping W/E motion.");
                return false;
            }
            else
                LOGF_DEBUG("Movement toward %s halted.", (dir == DIRECTION_WEST) ? "West" : "East");
            break;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Abort
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::Abort()
{
    if (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
    {
        GuideNSNP.s = GuideWENP.s = IPS_IDLE;
        GuideNSN[0].value = GuideNSN[1].value = 0.0;
        GuideWEN[0].value = GuideWEN[1].value = 0.0;

        if (m_GuideNSTID)
        {
            IERmTimer(m_GuideNSTID);
            m_GuideNSTID = 0;
        }

        if (m_GuideWETID)
        {
            IERmTimer(m_GuideWETID);
            m_GuideWETID = 0;
        }

        LOG_INFO("Guide aborted.");
        IDSetNumber(&GuideNSNP, nullptr);
        IDSetNumber(&GuideWENP, nullptr);

        return true;
    }

    return sendCommand(":Q#");
}

/////////////////////////////////////////////////////////////////////////////
/// Sync
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::Sync(double ra, double dec)
{
    char cmd[DRIVER_LEN] = {0};

    snprintf(cmd, DRIVER_LEN, ":Ck%07.3f%c%06.3f#", ra * 15.0, dec >= 0 ? '+' : '-', std::fabs(dec));

    if (sendCommand(cmd))
    {
        char RAStr[64] = {0}, DecStr[64] = {0};
        fs_sexa(RAStr, ra, 2, 36000);
        fs_sexa(DecStr, dec, 2, 36000);
        LOGF_INFO("Synced to RA %s DE %s", RAStr, DecStr);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Set Track Mode
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::SetTrackMode(uint8_t mode)
{
    switch (mode)
    {
        case TRACK_SIDEREAL:
            return sendCommand(":CtR#");
        case TRACK_SOLAR:
            return sendCommand(":CtS#");
        case TRACK_LUNAR:
            return sendCommand(":CtL#");
        case TRACK_CUSTOM:
            return sendCommand(":CtU#");

    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Set Slew Rate
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::SetSlewRate(int index)
{
    switch(index)
    {
        case SLEW_MAX:
            return sendCommand(":RS#");
        case SLEW_FIND:
            return sendCommand(":RM#");
        case SLEW_CENTERING:
            return sendCommand(":RC#");
        case SLEW_GUIDE:
            return sendCommand(":RG#");
    }

    return false;
}
/////////////////////////////////////////////////////////////////////////////
/// Error String Code
/////////////////////////////////////////////////////////////////////////////
const std::string Rainbow::getSlewErrorString(uint8_t code)
{
    switch (code)
    {
        case 1:
            return "The altitude of the target is lower than lower limit.";
        case 2:
            return "The altitude of the target is higher than upper limit.";
        case 3:
            return "Slewing was canceled by user";
        case 4:
            return "RA Axis homing failed.";
        case 5:
            return "DE Axis homing failed.";
    }

    return "Unknown error";
}

/////////////////////////////////////////////////////////////////////////////
/// Guide North
/////////////////////////////////////////////////////////////////////////////
IPState Rainbow::GuideNorth(uint32_t ms)
{
    return guide(Direction::North, ms);
}

/////////////////////////////////////////////////////////////////////////////
/// Guide South
/////////////////////////////////////////////////////////////////////////////
IPState Rainbow::GuideSouth(uint32_t ms)
{
    return guide(Direction::South, ms);
}

/////////////////////////////////////////////////////////////////////////////
/// Guide East
/////////////////////////////////////////////////////////////////////////////
IPState Rainbow::GuideEast(uint32_t ms)
{
    return guide(Direction::East, ms);
}

/////////////////////////////////////////////////////////////////////////////
/// Guide West
/////////////////////////////////////////////////////////////////////////////
IPState Rainbow::GuideWest(uint32_t ms)
{
    return guide(Direction::West, ms);
}

/////////////////////////////////////////////////////////////////////////////
/// Guide universal function
/////////////////////////////////////////////////////////////////////////////
IPState Rainbow::guide(Direction direction, uint32_t ms)
{
    // set up direction properties
    char dc = 'x';
    char cmd[DRIVER_LEN] = {0};
    ISwitchVectorProperty *moveSP = &MovementNSSP;
    ISwitch moveS = MovementNSS[0];
    int* guideTID = &m_GuideNSTID;

    // set up pointers to the various things needed
    switch (direction)
    {
        case North:
            dc = 'N';
            moveSP = &MovementNSSP;
            moveS = MovementNSS[0];
            guideTID = &m_GuideNSTID;
            break;
        case South:
            dc = 'S';
            moveSP = &MovementNSSP;
            moveS = MovementNSS[1];
            guideTID = &m_GuideNSTID;
            break;
        case East:
            dc = 'E';
            moveSP = &MovementWESP;
            moveS = MovementWES[1];
            guideTID = &m_GuideWETID;
            break;
        case West:
            dc = 'W';
            moveSP = &MovementWESP;
            moveS = MovementWES[0];
            guideTID = &m_GuideWETID;
            break;
    }

    if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }

    // If already moving (no pulse command), then stop movement
    if (moveSP->s == IPS_BUSY)
    {
        LOG_DEBUG("Already moving - stop");
        sendCommand(":Q#");
    }

    if (*guideTID)
    {
        LOGF_DEBUG("Stop timer %c", dc);
        IERmTimer(*guideTID);
        *guideTID = 0;
    }

    // Make sure TRACKING is set to Guide
    if (IUFindOnSwitchIndex(&TrackModeSP) != TRACK_CUSTOM)
    {
        SetTrackMode(TRACK_CUSTOM);
        IUResetSwitch(&TrackModeSP);
        TrackModeS[TRACK_CUSTOM].s = ISS_ON;
        IDSetSwitch(&TrackModeSP, nullptr);
        LOG_INFO("Tracking mode switched to guide.");
    }

    // Make sure SLEWING SPEED is set to Guide
    if (IUFindOnSwitchIndex(&SlewRateSP) != SLEW_GUIDE)
    {
        // Set slew to guiding
        SetSlewRate(SLEW_GUIDE);
        IUResetSwitch(&SlewRateSP);
        SlewRateS[SLEW_GUIDE].s = ISS_ON;
        IDSetSwitch(&SlewRateSP, nullptr);
    }

    moveS.s = ISS_ON;
    snprintf(cmd, DRIVER_LEN, ":M%c#", std::tolower(dc));

    // start movement at HC button rate 1
    if (!sendCommand(cmd))
    {
        LOGF_ERROR("Start motion %c failed", dc);
        return IPS_ALERT;
    }

    // start the guide timeout timer
    addGuideTimer(direction, ms);
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////
/// Guide Timeout North
/////////////////////////////////////////////////////////////////////////////
void Rainbow::guideTimeoutHelperN(void *p)
{
    static_cast<Rainbow *>(p)->guideTimeout(North);
}

/////////////////////////////////////////////////////////////////////////////
/// Guide Timeout South
/////////////////////////////////////////////////////////////////////////////
void Rainbow::guideTimeoutHelperS(void *p)
{
    static_cast<Rainbow *>(p)->guideTimeout(South);
}

/////////////////////////////////////////////////////////////////////////////
/// Guide Timeout West
/////////////////////////////////////////////////////////////////////////////
void Rainbow::guideTimeoutHelperW(void *p)
{
    static_cast<Rainbow *>(p)->guideTimeout(West);
}

/////////////////////////////////////////////////////////////////////////////
/// Guide Timeout East
/////////////////////////////////////////////////////////////////////////////
void Rainbow::guideTimeoutHelperE(void *p)
{
    static_cast<Rainbow *>(p)->guideTimeout(East);
}

/////////////////////////////////////////////////////////////////////////////
/// Guide Timeout
/////////////////////////////////////////////////////////////////////////////
void Rainbow::guideTimeout(Direction direction)
{
    char cmd[DRIVER_LEN] = {0};
    switch(direction)
    {
        case North:
        case South:
            IUResetSwitch(&MovementNSSP);
            IDSetSwitch(&MovementNSSP, nullptr);
            GuideNSNP.np[0].value = 0;
            GuideNSNP.np[1].value = 0;
            GuideNSNP.s           = IPS_IDLE;
            m_GuideNSTID            = 0;
            IDSetNumber(&GuideNSNP, nullptr);
            snprintf(cmd, DRIVER_LEN, ":Q%c#", direction == North ? 'n' : 's');
            break;
        case East:
        case West:
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementWESP, nullptr);
            GuideWENP.np[0].value = 0;
            GuideWENP.np[1].value = 0;
            GuideWENP.s           = IPS_IDLE;
            m_GuideWETID            = 0;
            IDSetNumber(&GuideWENP, nullptr);
            snprintf(cmd, DRIVER_LEN, ":Q%c#", direction == East ? 'e' : 'w');
            break;
    }
    sendCommand(cmd);
    LOGF_DEBUG("Guide %c finished", "NSWE"[direction]);
}

/////////////////////////////////////////////////////////////////////////////
/// Add guide timer
/////////////////////////////////////////////////////////////////////////////
void Rainbow::addGuideTimer(Direction direction, uint32_t ms)
{
    switch(direction)
    {
        case North:
            m_GuideNSTID = IEAddTimer(ms, guideTimeoutHelperN, this);
            break;
        case South:
            m_GuideNSTID = IEAddTimer(ms, guideTimeoutHelperS, this);
            break;
        case East:
            m_GuideWETID = IEAddTimer(ms, guideTimeoutHelperE, this);
            break;
        case West:
            m_GuideWETID = IEAddTimer(ms, guideTimeoutHelperW, this);
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////
/// Send Time
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::getLocalTime(char *timeString)
{
    if (isSimulation())
    {
        time_t now = time (nullptr);
        strftime(timeString, MAXINDINAME, "%T", localtime(&now));
    }
    else
    {
        double ctime = 0;
        int h, m, s;
        getLocalTime24(PortFD, &ctime);
        getSexComponents(ctime, &h, &m, &s);
        snprintf(timeString, MAXINDINAME, "%02d:%02d:%02d", h, m, s);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Time
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::getLocalDate(char *dateString)
{
    if (isSimulation())
    {
        time_t now = time (nullptr);
        strftime(dateString, MAXINDINAME, "%F", localtime(&now));
    }
    else
    {
        getCalendarDate(PortFD, dateString);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Time
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::getUTFOffset(double *offset)
{
    if (isSimulation())
    {
        *offset = 3;
        return true;
    }

    int lx200_utc_offset = 0;
    getUTCOffset(PortFD, &lx200_utc_offset);
    // LX200 TimeT Offset is defined at the number of hours added to LOCAL TIME to get TimeT. This is contrary to the normal definition.
    *offset = lx200_utc_offset * -1;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Time
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::sendScopeTime()
{
    char cdate[MAXINDINAME] = {0};
    char ctime[MAXINDINAME] = {0};
    struct tm ltm;
    struct tm utm;
    time_t time_epoch;

    double offset = 0;
    if (getUTFOffset(&offset))
    {
        char utcStr[8] = {0};
        snprintf(utcStr, 8, "%.2f", offset);
        IUSaveText(&TimeT[1], utcStr);
    }
    else
    {
        LOG_WARN("Could not obtain UTC offset from mount!");
        return false;
    }

    if (getLocalTime(ctime) == false)
    {
        LOG_WARN("Could not obtain local time from mount!");
        return false;
    }

    if (getLocalDate(cdate) == false)
    {
        LOG_WARN("Could not obtain local date from mount!");
        return false;
    }

    // To ISO 8601 format in LOCAL TIME!
    char datetime[MAXINDINAME] = {0};
    snprintf(datetime, MAXINDINAME, "%sT%s", cdate, ctime);

    // Now that date+time are combined, let's get tm representation of it.
    if (strptime(datetime, "%FT%T", &ltm) == nullptr)
    {
        LOGF_WARN("Could not process mount date and time: %s", datetime);
        return false;
    }

    // Get local time epoch in UNIX seconds
    time_epoch = mktime(&ltm);

    // LOCAL to UTC by subtracting offset.
    time_epoch -= static_cast<int>(offset * 3600.0);

    // Get UTC (we're using localtime_r, but since we shifted time_epoch above by UTCOffset, we should be getting the real UTC time)
    localtime_r(&time_epoch, &utm);

    // Format it into the final UTC ISO 8601
    strftime(cdate, MAXINDINAME, "%Y-%m-%dT%H:%M:%S", &utm);
    IUSaveText(&TimeT[0], cdate);

    LOGF_DEBUG("Mount controller UTC Time: %s", TimeT[0].text);
    LOGF_DEBUG("Mount controller UTC Offset: %s", TimeT[1].text);

    // Let's send everything to the client
    TimeTP.s = IPS_OK;
    IDSetText(&TimeTP, nullptr);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Location
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::sendScopeLocation()
{
    int dd = 0, mm = 0;
    double ssf = 0.0;

    if (isSimulation())
    {
        LocationNP.np[LOCATION_LATITUDE].value = 29.5;
        LocationNP.np[LOCATION_LONGITUDE].value = 48.0;
        LocationNP.np[LOCATION_ELEVATION].value = 10;
        LocationNP.s           = IPS_OK;
        IDSetNumber(&LocationNP, nullptr);
        return true;
    }

    if (getSiteLatitude(PortFD, &dd, &mm, &ssf) < 0)
    {
        LOG_WARN("Failed to get site latitude from device.");
        return false;
    }
    else
    {
        if (dd > 0)
            LocationNP.np[0].value = dd + mm / 60.0;
        else
            LocationNP.np[0].value = dd - mm / 60.0;
    }

    if (getSiteLongitude(PortFD, &dd, &mm, &ssf) < 0)
    {
        LOG_WARN("Failed to get site longitude from device.");
        return false;
    }
    else
    {
        if (dd > 0)
            LocationNP.np[1].value = 360.0 - (dd + mm / 60.0);
        else
            LocationNP.np[1].value = (dd - mm / 60.0) * -1.0;

    }

    LOGF_DEBUG("Mount Controller Latitude: %.3f Longitude: %.3f", LocationN[LOCATION_LATITUDE].value,
               LocationN[LOCATION_LONGITUDE].value);

    IDSetNumber(&LocationNP, nullptr);

    //saveConfig(true, "GEOGRAPHIC_COORD");

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);

        char formatted_command[DRIVER_LEN] = {0};
        snprintf(formatted_command, DRIVER_LEN, "%s\r", cmd);
        rc = tty_write_string(PortFD, formatted_command, &nbytes_written);
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

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        // Remove extra \r
        res[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void Rainbow::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> Rainbow::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
