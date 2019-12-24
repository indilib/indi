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

#include <connectionplugins/connectionserial.h>
#include <indicom.h>

#include <cstring>
#include <termios.h>
#include <regex>

static std::unique_ptr<Rainbow> scope(new Rainbow());

void ISGetProperties(const char *dev)
{
    scope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    scope->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    scope->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    scope->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}
void ISSnoopDevice(XMLEle *root)
{
    scope->ISSnoopDevice(root);
}

Rainbow::Rainbow() : INDI::Telescope ()
{
    setVersion(1, 0);

    SetTelescopeCapability(TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_HAS_PIER_SIDE_SIMULATION, 4);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
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

    SetParkDataType(PARK_AZ_ALT);

    // Homing
    IUFillSwitch(&HomeS[0], "HOME", "Go Home", ISS_OFF);
    IUFillSwitchVector(&HomeSP, HomeS, 1, getDeviceName(), "HOME", "Homing", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Horizontal Coords
    IUFillNumber(&HorizontalCoordsN[AXIS_AZ], "AZ", "Az D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
    IUFillNumber(&HorizontalCoordsN[AXIS_ALT], "ALT", "Alt  D:M:S", "%10.6m", -90., 90.0, 0.0, 0);
    IUFillNumberVector(&HorizontalCoordsNP, HorizontalCoordsN, 2, getDeviceName(), "HORIZONTAL_COORD",
                       "Horizontal Coord", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Guide");
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
        defineNumber(&HorizontalCoordsNP);
        defineSwitch(&HomeSP);

    }
    else
    {
        deleteProperty(HorizontalCoordsNP.name);
        deleteProperty(HomeSP.name);
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
void Rainbow::getBasicData()
{
    if (getFirmwareVersion())
        LOGF_INFO("Detected firmware %s", m_Version.c_str());
    if (getTrackingState())
        IDSetSwitch(&TrackStateSP, nullptr);
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool Rainbow::checkConnection()
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
    TrackStateSP.s = TrackStateS[TRACK_ON].s == ISS_ON ? IPS_BUSY : IPS_OK;

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
                if (m_GotoType == GOTO_HORIZONTAL)
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
                TrackState = m_GotoType == GOTO_HORIZONTAL ? SCOPE_IDLE : SCOPE_TRACKING;
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
            TrackState = m_GotoType == GOTO_HORIZONTAL ? SCOPE_IDLE : SCOPE_TRACKING;
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
            MovementNSSP.s = MovementWESP.s = IPS_IDLE;
            EqNP.s                          = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
        }

        // sleep for 100 mseconds
        nanosleep(&timeout, nullptr);
    }

    if (slewToEquatorialCoords(ra, dec))
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

    snprintf(cmd, DRIVER_LEN, ":Sd%c%02d*%02d:%04.1f#", de >= 0 ? '+' : '-', degrees, minutes, seconds);

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
        m_GotoType = GOTO_EQUATORIAL;
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
    char res[DRIVER_LEN] = {0};
    int degrees, minutes;
    double seconds;

    getSexComponentsIID(azimuth, &degrees, &minutes, &seconds);

    snprintf(cmd, DRIVER_LEN, ":Sa%03d*%02d:%04.1f#", degrees, minutes, seconds);

    if (!sendCommand(cmd, res, -1, 1))
        return false;

    return res[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
/// Set Target Altitude
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::setAL(double altitude)
{
    char cmd[DRIVER_LEN] = {0};
    char res[DRIVER_LEN] = {0};
    int degrees, minutes;
    double seconds;

    getSexComponentsIID(altitude, &degrees, &minutes, &seconds);

    snprintf(cmd, DRIVER_LEN, ":Sa%02d*%02d:%04.1f#", degrees, minutes, seconds);

    if (!sendCommand(cmd, res, -1, 1))
        return false;

    return res[0] == '1';
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
        m_GotoType = GOTO_HORIZONTAL;
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Abort
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::Abort()
{
    return sendCommand(":Q#");
}

/////////////////////////////////////////////////////////////////////////////
/// Sync
/////////////////////////////////////////////////////////////////////////////
bool Rainbow::Sync(double ra, double dec)
{
    char cmd[DRIVER_LEN] = {0};

    snprintf(cmd, DRIVER_LEN, ":Ck%07.3f%c%06.3f#", ra, dec >= 0 ? '+' : '-', dec);

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
