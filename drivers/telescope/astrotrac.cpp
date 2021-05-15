/*******************************************************************************
 Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Driver for using TheSky6 Pro Scripted operations for mounts via the TCP server.
 While this technically can operate any mount connected to the TheSky6 Pro, it is
 intended for AstroTrac mounts control.

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

#include "astrotrac.h"

#include "indicom.h"

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <regex>

std::unique_ptr<AstroTrac> AstroTrac_mount(new AstroTrac());

AstroTrac::AstroTrac()
{
    setVersion(1, 0);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_HAS_TRACK_RATE |
                           TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_PIER_SIDE,
                           9);
    setTelescopeConnection(CONNECTION_TCP);
}

const char *AstroTrac::getDefaultName()
{
    return "AstroTrac";
}

bool AstroTrac::initProperties()
{
    INDI::Telescope::initProperties();

    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Custom Tracking Rate
    IUFillNumber(&TrackRateN[0], "TRACK_RATE_RA", "RA (arcsecs/s)", "%.6f", -16384.0, 16384.0, 0.000001, 15.041067);
    IUFillNumber(&TrackRateN[1], "TRACK_RATE_DE", "DE (arcsecs/s)", "%.6f", -16384.0, 16384.0, 0.000001, 0);
    IUFillNumberVector(&TrackRateNP, TrackRateN, 2, getDeviceName(), "TELESCOPE_TRACK_RATE", "Track Rates",
                       MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    TrackState = SCOPE_IDLE;

    SetParkDataType(PARK_AZ_ALT);

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    addAuxControls();

    //    double longitude = 0, latitude = 90;
    //    // Get value from config file if it exists.
    //    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
    //    currentRA  = get_local_sidereal_time(longitude);
    //    IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
    //    currentDEC = latitude > 0 ? 90 : -90;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        //        if (isTheSkyTracking())
        //        {
        //            IUResetSwitch(&TrackModeSP);
        //            TrackModeS[TRACK_SIDEREAL].s = ISS_ON;
        //            TrackState = SCOPE_TRACKING;
        //        }
        //        else
        //        {
        //            IUResetSwitch(&TrackModeSP);
        //            TrackState = SCOPE_IDLE;
        //        }

        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(&GuideRateNP);

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

        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP->getName());
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::Handshake()
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::ReadScopeStatus()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::Goto(double r, double d)
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::isSlewComplete()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::Sync(double ra, double dec)
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::Park()
{
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking telescope in progress...");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::UnPark()
{
    SetParked(false);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::Abort()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    // JM: INDI Longitude is 0 to 360 increasing EAST. libnova East is Positive, West is negative
    lnobserver.lng = longitude;

    if (lnobserver.lng > 180)
        lnobserver.lng -= 360;
    lnobserver.lat = latitude;

    LOGF_INFO("Location updated: Longitude (%g) Latitude (%g)", lnobserver.lng, lnobserver.lat);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::updateTime(ln_date *utc, double utc_offset)
{
    INDI_UNUSED(utc);
    INDI_UNUSED(utc_offset);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::SetCurrentPark()
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::SetDefaultPark()
{
    // By default set HA to 0
    SetAxis1Park(0);

    // Set DEC to 90 or -90 depending on the hemisphere
    SetAxis2Park((LocationN[LOCATION_LATITUDE].value > 0) ? 90 : -90);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::SetParkPosition(double Axis1Value, double Axis2Value)
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState AstroTrac::GuideNorth(uint32_t ms)
{
    // Movement in arcseconds
    double dDE = GuideRateNP.at(AXIS_DE)->getValue() * TRACKRATE_SIDEREAL * ms / 1000.0;
    return IPS_OK;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState AstroTrac::GuideSouth(uint32_t ms)
{
    // Movement in arcseconds
    double dDE = GuideRateNP.at(AXIS_DE)->getValue() * TRACKRATE_SIDEREAL * ms / -1000.0;
    return IPS_OK;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState AstroTrac::GuideEast(uint32_t ms)
{
    // Movement in arcseconds
    double dRA = GuideRateNP.at(AXIS_RA)->getValue() * TRACKRATE_SIDEREAL * ms / 1000.0;
    return IPS_OK;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState AstroTrac::GuideWest(uint32_t ms)
{
    // Movement in arcseconds
    double dRA = GuideRateNP.at(AXIS_RA)->getValue() * TRACKRATE_SIDEREAL * ms / -1000.0;
    return IPS_OK;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::SetTrackRate(double raRate, double deRate)
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::SetTrackMode(uint8_t mode)
{
    bool isSidereal = (mode == TRACK_SIDEREAL);
    double dRA = 0, dDE = 0;

    if (mode == TRACK_SOLAR)
        dRA = TRACKRATE_SOLAR;
    else if (mode == TRACK_LUNAR)
        dRA = TRACKRATE_LUNAR;
    else if (mode == TRACK_CUSTOM)
    {
        dRA = TrackRateN[AXIS_RA].value;
        dDE = TrackRateN[AXIS_DE].value;
    }
    //return setTheSkyTracking(true, isSidereal, dRA, dDE);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::SetTrackEnabled(bool enabled)
{
    // On engaging track, we simply set the current track mode and it will take care of the rest including custom track rates.
    if (enabled)
        return SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP));
    else
        // Otherwise, simply switch everything off
        return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

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
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
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

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void AstroTrac::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> AstroTrac::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
