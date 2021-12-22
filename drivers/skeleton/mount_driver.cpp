/*******************************************************************************
 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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

#include "mount_driver.h"

#include "indicom.h"

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include <termios.h>
#include <cmath>
#include <cstring>
#include <memory>

// Single unique pointer to the driver.
static std::unique_ptr<MountDriver> telescope_sim(new MountDriver());

MountDriver::MountDriver()
{
    // Let's specify the driver version
    setVersion(1, 0);

    // Set capabilities supported by the mount.
    // The last parameters is the number of slew rates available.
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_PIER_SIDE |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_TRACK_RATE,
                           4);

}

const char *MountDriver::getDefaultName()
{
    return "Mount Driver";
}

bool MountDriver::initProperties()
{
    // Make sure to init parent properties first
    INDI::Telescope::initProperties();

    // How fast do we guide compared to sidereal rate
    IUFillNumber(&GuideRateN[AXIS_RA], "GUIDE_RATE_WE", "W/E Rate", "%.1f", 0, 1, 0.1, 0.5);
    IUFillNumber(&GuideRateN[AXIS_DE], "GUIDE_RATE_NS", "N/S Rate", "%.1f", 0, 1, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    // Since we have 4 slew rates, let's fill them out
    IUFillSwitch(&SlewRateS[SLEW_GUIDE], "SLEW_GUIDE", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_CENTERING], "SLEW_CENTERING", "Centering", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_FIND], "SLEW_FIND", "Find", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_MAX], "SLEW_MAX", "Max", ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 4, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Add Tracking Modes. If you have SOLAR, LUNAR..etc, add them here as well.
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // The mount is initially in IDLE state.
    TrackState = SCOPE_IDLE;

    // How does the mount perform parking?
    // Some mounts can handle the parking functionality internally in the controller.
    // Other mounts have no native parking support and we use INDI to slew to a particular
    // location (Equatorial or Horizontal) and then turn off tracking there and save the location to a file
    // which would be remembered in the next power cycle.
    // This is not required if there is native support in the mount controller itself.
    SetParkDataType(PARK_AZ_ALT);

    // Let init the pulse guiding properties
    initGuiderProperties(getDeviceName(), MOTION_TAB);

    // Add debug controls
    addDebugControl();

    // Set the driver interface to indicate that we can also do pulse guiding
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    // We want to query the mount every 500ms by default. The user can override this value.
    setDefaultPollingPeriod(500);

    return true;
}

bool MountDriver::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(&GuideRateNP);

        // Read the parking file, and check if we can load any saved parking information.
        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            // By default in this example, we consider parking position Az=0 and Alt=0
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
        deleteProperty(GuideRateNP.name);
    }

    return true;
}

bool MountDriver::Handshake()
{
    // This functin is ensure that we have communication with the mount
    // Below we send it 0x6 byte and check for 'S' in the return. Change this
    // to be valid for your driver. It could be anything, you can simply put this below
    // return readScopeStatus()
    // since this will try to read the position and if successful, then communicatoin is OK.
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    // Ack
    cmd[0] = 0x6;

    bool rc = sendCommand(cmd, res, 1, 1);
    if (rc == false)
        return false;

    return res[0] == 'S';
}

bool MountDriver::ReadScopeStatus()
{

    // Here we read the mount position, pier side, any status of interest.
    // This is called every POLLMS milliseconds (default 1000, but our driver set the default to 500)

    // For example, it could be a command like this
    char cmd[DRIVER_LEN]={0}, res[DRIVER_LEN]={0};
    if (sendCommand("GetCoordinates", res) == false)
        return false;

    double currentRA=0, currentDE=0;
    // Assuming we get response as RA:DEC (Hours:Degree) e.g. "12.4:-34.6"
    sscanf(res, "%lf:%lf", &currentRA, &currentDE);

    char RAStr[DRIVER_LEN]={0}, DecStr[DRIVER_LEN]={0};
    fs_sexa(RAStr, currentRA, 2, 3600);
    fs_sexa(DecStr, currentDE, 2, 3600);
    LOGF_DEBUG("Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(currentRA, currentDE);

    // E.g. get pier side as well
    // assuming we need to send 3-bytes 0x11 0x22 0x33 to get the pier side, which is always 1 byte as 0 (EAST) or 1 (WEST)
    cmd[0] = 0x11;
    cmd[1] = 0x22;
    cmd[2] = 0x33;

    // Let us not forget to reset res buffer by zeroing it out
    memset(res, 0, DRIVER_LEN);
    if (sendCommand(cmd, res, 3, 1))
    {
        setPierSide(res[0] == 0 ? PIER_EAST : PIER_WEST);
    }

    return true;
}

bool MountDriver::Goto(double RA, double DE)
{
    char cmd[DRIVER_LEN]={0}, res[DRIVER_LEN]={0};

    // Assuming the command is in this format: sendCoords RA:DE
    snprintf(cmd, DRIVER_LEN, "sendCoords %g:%g", RA, DE);
    // Assuming response is 1-byte with '1' being OK, and anything else being failed.
    if (sendCommand(cmd, res, -1, 1) == false)
        return false;

    if (res[0] != '1')
        return false;

    TrackState = SCOPE_SLEWING;

    char RAStr[DRIVER_LEN]={0}, DecStr[DRIVER_LEN]={0};
    fs_sexa(RAStr, RA, 2, 3600);
    fs_sexa(DecStr, DE, 2, 3600);
    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool MountDriver::Sync(double RA, double DE)
{
    char cmd[DRIVER_LEN]={0}, res[DRIVER_LEN]={0};

    // Assuming the command is in this format: syncCoords RA:DE
    snprintf(cmd, DRIVER_LEN, "syncCoords %g:%g", RA, DE);
    // Assuming response is 1-byte with '1' being OK, and anything else being failed.
    if (sendCommand(cmd, res, -1, 1) == false)
        return false;

    if (res[0] != '1')
        return false;

    NewRaDec(RA, DE);

    return true;
}

bool MountDriver::Park()
{    
    // Send command for parking here
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking telescope in progress...");
    return true;
}

bool MountDriver::UnPark()
{
    SetParked(false);
    return true;
}

bool MountDriver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Guide Rate
        if (strcmp(name, "GUIDE_RATE") == 0)
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);
            return true;
        }

        // For guiding pulse, let's pass the properties up to the guide framework
        if (strcmp(name, GuideNSNP.name) == 0 || strcmp(name, GuideWENP.name) == 0)
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }
    }

    // Otherwise, send it up the chains to INDI::Telescope to process any further properties
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}


bool MountDriver::Abort()
{
    // Example of a function call where we expect no respose
    return sendCommand("AbortMount");
}

bool MountDriver::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(command);
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    // Implement here the actual calls to do the motion requested
    return true;
}

bool MountDriver::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(command);
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    // Implement here the actual calls to do the motion requested
    return true;
}

IPState MountDriver::GuideNorth(uint32_t ms)
{
    // Implement here the actual calls to do the motion requested
    return IPS_BUSY;
}

IPState MountDriver::GuideSouth(uint32_t ms)
{
    // Implement here the actual calls to do the motion requested
    return IPS_BUSY;
}

IPState MountDriver::GuideEast(uint32_t ms)
{
    // Implement here the actual calls to do the motion requested
    return IPS_BUSY;
}

IPState MountDriver::GuideWest(uint32_t ms)
{
    // Implement here the actual calls to do the motion requested
    return IPS_BUSY;
}

bool MountDriver::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    // JM: INDI Longitude is 0 to 360 increasing EAST. libnova East is Positive, West is negative
    m_GeographicLocation.lng = longitude;

    if (m_GeographicLocation.lng > 180)
        m_GeographicLocation.lng -= 360;
    m_GeographicLocation.lat = latitude;

    // Implement here the actual calls to the controller to set the location if supported.

    // Inform the client that location was updated if all goes well
    LOGF_INFO("Location updated: Longitude (%g) Latitude (%g)", m_GeographicLocation.lng, m_GeographicLocation.lat);

    return true;
}

bool MountDriver::SetCurrentPark()
{
    // Depending on the parking type defined initially (PARK_RA_DEC or PARK_AZ_ALT...etc) set the current
    // position AS the parking position.

    // Assumg PARK_AZ_ALT, we need to do something like this:

    // SetAxis1Park(getCurrentAz());
    // SetAxis2Park(getCurrentAlt());

    // Or if currentAz, currentAlt are defined as variables in our driver, then
    // SetAxis1Park(currentAz);
    // SetAxis2Park(currentAlt);

    return true;
}

bool MountDriver::SetDefaultPark()
{
    // For RA_DE park, we can use something like this:

    // By default set RA to HA
    SetAxis1Park(get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value));
    // Set DEC to 90 or -90 depending on the hemisphere
    SetAxis2Park((LocationN[LOCATION_LATITUDE].value > 0) ? 90 : -90);

    // For Az/Alt, we can use something like this:

    // Az = 0
    SetAxis1Park(0);
    // Alt = 0
    SetAxis2Park(0);

    return true;
}

bool MountDriver::SetTrackMode(uint8_t mode)
{
    // Sidereal/Lunar/Solar..etc

    // Send actual command here to device
    INDI_UNUSED(mode);
    return true;
}

bool MountDriver::SetTrackEnabled(bool enabled)
{
    // Tracking on or off?
    INDI_UNUSED(enabled);
    // Send actual command here to device
    return true;
}

bool MountDriver::SetTrackRate(double raRate, double deRate)
{
    // Send actual command here to device
    INDI_UNUSED(raRate);
    INDI_UNUSED(deRate);
    return true;
}

bool MountDriver::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
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
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

void MountDriver::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}
