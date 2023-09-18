/*******************************************************************************
 Copyright(c) 2023 Jasem Mutlaq. All rights reserved.

 Planewave Mount
 API Communication

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

#include "planewave_mount.h"

#include "indicom.h"
#include "inditimer.h"
#include "httplib.h"
#include "connectionplugins/connectiontcp.h"
#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <regex>
#include <array>
#include <termios.h>

std::unique_ptr<PlaneWave> PlaneWave_mount(new PlaneWave());

PlaneWave::PlaneWave()
{
    setVersion(0, 1);

    SetTelescopeCapability(TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_PIER_SIDE,
                           4);

    setTelescopeConnection(CONNECTION_TCP);
}

const char *PlaneWave::getDefaultName()
{
    return "PlaneWave";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::initProperties()
{
    INDI::Telescope::initProperties();

    // Track Modes
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    TrackState = SCOPE_IDLE;

    SetParkDataType(PARK_AZ_ALT);

    tcpConnection->setDefaultHost("192.168.1.1");
    tcpConnection->setDefaultPort(8220);

    addAuxControls();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineProperty(FirmwareTP);

        // Initial AZ/AL parking position.
        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(0);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(0);
            SetAxis2Park(0);
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(0);
        }
    }
    else
    {
        deleteProperty(FirmwareTP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::Handshake()
{
    return getStatus();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::getStatus()
{
    return dispatch("/status");
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::Sync(double ra, double dec)
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::Goto(double ra, double dec)
{
    httplib::Client cli(tcpConnection->host(), tcpConnection->port());
    std::string request = "/mount/goto_ra_dec_apparent?ra_hours=" + std::to_string(ra) + "&dec_degs=" + std::to_string(dec);
    return cli.Get(request);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::ReadScopeStatus()
{
    if (getStatus() == false)
        return false;

    auto ra = m_Status["mount.ra_apparent_hours"].as<double>();
    auto de = m_Status["mount.dec_apparent_degs"].as<double>();

    auto isSlewing = m_Status["mount.is_slewing"].as<bool>();
    auto isTracking = m_Status["mount.is_tracking"].as<bool>();

    switch (TrackState)
    {
        case SCOPE_PARKING:
            if (!isSlewing)
                SetParked(true);
            break;
        case SCOPE_SLEWING:
            if (isTracking)
            {
                TrackState = SCOPE_TRACKING;
                SetTrackEnabled(true);
            }
            break;
        default:
            break;
    }

    NewRaDec(ra, de);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::Park()
{
    httplib::Client cli(tcpConnection->host(), tcpConnection->port());
    std::string request = "/mount/park";
    return cli.Get(request);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::UnPark()
{
    SetParked(false);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool PlaneWave::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

    }

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::ISNewSwitch(const char *dev, const char *name, ISState * states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::Abort()
{
    httplib::Client cli(tcpConnection->host(), tcpConnection->port());
    std::string request = "/mount/stop";
    return cli.Get(request);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    if (command == MOTION_START)
    {

    }
    else
    {
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    if (command == MOTION_START)
    {
    }
    else
    {
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::updateLocation(double latitude, double longitude, double elevation)
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::updateTime(ln_date * utc, double utc_offset)
{
    INDI_UNUSED(utc);
    INDI_UNUSED(utc_offset);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::SetCurrentPark()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::SetDefaultPark()
{
    SetAxis1Park(0);
    SetAxis2Park(0);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::SetTrackRate(double raRate, double deRate)
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::SetTrackMode(uint8_t mode)
{
    double dRA = 0, dDE = 0;
    if (mode == TRACK_SIDEREAL)
        dRA = TRACKRATE_SIDEREAL;
    else if (mode == TRACK_SOLAR)
        dRA = TRACKRATE_SOLAR;
    else if (mode == TRACK_LUNAR)
        dRA = TRACKRATE_LUNAR;
    else if (mode == TRACK_CUSTOM)
    {
        dRA = TrackRateN[AXIS_RA].value;
        dDE = TrackRateN[AXIS_DE].value;
    }

    INDI_UNUSED(dRA);
    INDI_UNUSED(dDE);

    // TODO figure out how to set tracking rate per axis
    httplib::Client cli(tcpConnection->host(), tcpConnection->port());
    std::string request = "/mount/tracking_on";
    return cli.Get(request);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::SetTrackEnabled(bool enabled)
{
    // On engaging track, we simply set the current track mode and it will take care of the rest including custom track rates.
    if (enabled)
        return SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP));
    // Disable tracking
    else
    {
        httplib::Client cli(tcpConnection->host(), tcpConnection->port());
        std::string request = "/mount/tracking_off";
        return cli.Get(request);
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Config Items
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::dispatch(const std::string &request)
{
    httplib::Client cli(tcpConnection->host(), tcpConnection->port());

    if (auto res = cli.Get(request))
    {
        try
        {
            ini::IniFile inif;
            inif.decode("[status]\n" + res->body);
            if (inif["status"].size() == 0)
                return false;
            m_Status = inif["status"];
            return true;
        }
        catch (std::exception &e)
        {
            LOGF_ERROR("Failed to process status response: %s", e.what());
            return false;
        }
    }
    else
    {
        LOGF_ERROR("Request %s to %s:%d failed (%d)", request.c_str(), tcpConnection->host(), tcpConnection->port(), res.error());
        return false;
    }
}
