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
#include "inditimer.h"
#include "connectionplugins/connectiontcp.h"
#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <regex>
#include <array>
#include <termios.h>

std::unique_ptr<AstroTrac> AstroTrac_mount(new AstroTrac());
const std::array<uint32_t, AstroTrac::SLEW_MODES> AstroTrac::SLEW_SPEEDS = {{1, 2, 4, 8, 32, 64, 128, 600, 700, 800}};

using namespace INDI::AlignmentSubsystem;

AstroTrac::AstroTrac()
{
    setVersion(1, 0);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_HAS_TRACK_RATE |
                           TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_TRACK_RATE | TELESCOPE_HAS_PIER_SIDE,
                           SLEW_MODES);
    setTelescopeConnection(CONNECTION_TCP);
}

const char *AstroTrac::getDefaultName()
{
    return "AstroTrac";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::initProperties()
{
    INDI::Telescope::initProperties();

    // Track Modes
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Slew Speeds
    for (uint8_t i = 0; i < SLEW_MODES; i++)
    {
        sprintf(SlewRateSP.sp[i].label, "%dx", SLEW_SPEEDS[i]);
        SlewRateSP.sp[i].aux = (void *)&SLEW_SPEEDS[i];
    }
    SlewRateS[5].s = ISS_ON;

    // Mount Type
    int configMountType = MOUNT_GEM;
    IUGetConfigOnSwitchIndex(getDeviceName(), "MOUNT_TYPE", &configMountType);
    MountTypeSP[MOUNT_GEM].fill("MOUNT_GEM", "GEM",
                                configMountType == MOUNT_GEM ? ISS_ON : ISS_OFF);
    MountTypeSP[MOUNT_SINGLE_ARM].fill("MOUNT_SINGLE_ARM", "Single ARM",
                                       configMountType == MOUNT_GEM ? ISS_OFF : ISS_ON);
    MountTypeSP.fill(getDeviceName(), "MOUNT_TYPE", "Mount Type", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Acceleration
    AccelerationNP[AXIS_RA].fill("AXIS_RA", "RA arcsec/sec^2", "%.2f", 0, 3600, 100, 0);
    AccelerationNP[AXIS_DE].fill("AXIS_DE", "DE arcsec/sec^2", "%.2f", 0, 3600, 100, 0);
    AccelerationNP.fill(getDeviceName(), "MOUNT_ACCELERATION", "Acceleration", MOTION_TAB, IP_RW, 60, IPS_IDLE);

    // Encoders
    EncoderNP[AXIS_RA].fill("AXIS_RA", "Hour Angle", "%.2f", -3600, 3600, 100, 0);
    EncoderNP[AXIS_DE].fill("AXIS_DE", "Declination", "%.2f", -3600, 3600, 100, 0);
    EncoderNP.fill(getDeviceName(), "MOUNT_ENCODERS", "Encoders", MOTION_TAB, IP_RW, 60, IPS_IDLE);

    TrackState = SCOPE_IDLE;

    SetParkDataType(PARK_RA_DEC_ENCODER);

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    tcpConnection->setDefaultHost("192.168.1.1");
    tcpConnection->setDefaultPort(23);

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    addAuxControls();

    InitAlignmentProperties(this);
    // set mount type to alignment subsystem
    SetApproximateMountAlignmentFromMountType(EQUATORIAL);
    // Init Math plugin
    Initialise(this);

    // Force the alignment system to always be on
    getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE")->sp[0].s = ISS_ON;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void AstroTrac::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);

    defineProperty(&MountTypeSP);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        getAcceleration(AXIS_RA);
        getAcceleration(AXIS_DE);
        getVelocity(AXIS_RA);
        getVelocity(AXIS_DE);

        defineProperty(&FirmwareTP);
        defineProperty(&AccelerationNP);
        defineProperty(&EncoderNP);
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
        deleteProperty(FirmwareTP.getName());
        deleteProperty(AccelerationNP.getName());
        deleteProperty(EncoderNP.getName());
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
    return getVersion();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::getVersion()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand("<1zv?>", response))
    {
        FirmwareTP[0].setText(response + 4, 4);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::getAcceleration(INDI_EQ_AXIS axis)
{
    char command[DRIVER_LEN] = {0}, response[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, "<%da?>", axis + 1);
    if (sendCommand(command, response))
    {
        try
        {
            std::string acceleration = std::regex_replace(
                                           response,
                                           std::regex("<.a(\\d+)>"),
                                           std::string("$1"));

            AccelerationNP[axis].setValue(std::stoi(acceleration));
            return true;
        }
        catch(...)
        {
            LOGF_DEBUG("Failed to parse acceleration (%s)", response);
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::setAcceleration(INDI_EQ_AXIS axis, uint32_t a)
{
    char command[DRIVER_LEN] = {0}, response[DRIVER_LEN] = {0};

    snprintf(command, DRIVER_LEN, "<%da%u>", axis + 1, a);
    if (sendCommand(command, response))
        return response[3] == '#';

    return false;

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::getVelocity(INDI_EQ_AXIS axis)
{
    char command[DRIVER_LEN] = {0}, response[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, "<%dv?>", axis + 1);
    if (sendCommand(command, response))
    {
        try
        {
            std::string velocity = std::regex_replace(
                                       response,
                                       std::regex("<.v([+-]?[0-9]+\\.[0-9]+?)>"),
                                       std::string("$1"));

            TrackRateN[axis].value = std::stod(velocity) * (m_Location.latitude >= 0 ? 1 : -1);
            return true;
        }
        catch(...)
        {
            LOGF_DEBUG("Failed to parse velocity (%s)", response);
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::setVelocity(INDI_EQ_AXIS axis, double value)
{
    char command[DRIVER_LEN] = {0}, response[DRIVER_LEN] = {0};

    // Reverse value depending on hemisphere
    snprintf(command, DRIVER_LEN, "<%dve%f>", axis + 1, value  * (m_Location.latitude >= 0 ? 1 : -1));
    if (sendCommand(command, response))
        return response[4] == '#';

    return false;

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::stopMotion(INDI_EQ_AXIS axis)
{
    char command[DRIVER_LEN] = {0}, response[DRIVER_LEN] = {0};

    snprintf(command, DRIVER_LEN, "<%dx>", axis + 1);
    if (sendCommand(command, response))
        return response[3] == '#';

    return false;

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::isSlewComplete()
{
    char HAResponse[DRIVER_LEN] = {0}, DEResponse[DRIVER_LEN] = {0};
    if (sendCommand("<1t?>", HAResponse) && sendCommand("<2t?>", DEResponse))
    {
        return (HAResponse[3] == '0' && DEResponse[3] == '0');
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::syncEncoder(INDI_EQ_AXIS axis, double value)
{
    char command[DRIVER_LEN] = {0}, response[DRIVER_LEN] = {0};

    snprintf(command, DRIVER_LEN, "<%dy%f>", axis + 1, value);
    if (sendCommand(command, response))
        return response[3] == '#';

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::slewEncoder(INDI_EQ_AXIS axis, double value)
{
    char command[DRIVER_LEN] = {0}, response[DRIVER_LEN] = {0};

    snprintf(command, DRIVER_LEN, "<%dp%f>", axis + 1, value);
    if (sendCommand(command, response))
        return response[3] == '#';

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// DE encoder range: 0 to +180 degrees CW, 0 to -180 CCW
/// HA encoder range: 0 to +180 degrees CW, 0 to -180 CCW
/// The range begins from mount home position looking at celestial pole
/// with counter weight down.
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::getEncoderPosition(INDI_EQ_AXIS axis)
{
    char command[DRIVER_LEN] = {0}, response[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, "<%dp?>", axis + 1);
    if (sendCommand(command, response))
    {
        try
        {
            char regex_str[64] = {0};
            snprintf(regex_str, 64, "<%dp([+-]?[0-9]+\\.[0-9]+?)>", axis + 1);
            std::string position = std::regex_replace(
                                       response,
                                       std::regex(regex_str),
                                       std::string("$1"));

            EncoderNP[axis].setValue(std::stod(position));
            return true;
        }
        catch(...)
        {
            try
            {
                // Check if the response for the other axis
                INDI_EQ_AXIS other = (axis == AXIS_RA) ? AXIS_DE : AXIS_RA;
                char regex_str[64] = {0};
                snprintf(regex_str, 64, "<%dp([+-]?[0-9]+\\.[0-9]+?)>", other + 1);
                std::string position = std::regex_replace(
                                           response,
                                           std::regex(regex_str),
                                           std::string("$1"));

                EncoderNP[other].setValue(std::stod(position));
                return true;
            }
            catch(...)
            {
                LOGF_DEBUG("Failed to parse position (%s)", response);
            }
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Based on X2 plugin
/// https://github.com/mcgillca/AstroTrac/blob/3dc9d0d6f41750297696390bdcd6c10c87525b53/AstroTrac.cpp#L454
/// Mechanical DE Range: -180 to +180 degrees. Home Position Mechanical DE: 0
/// Mechanical HA Range: -90 to +90 degrees. Home Position Mechanical HA: 0
/// For north hemisphere, home position HA = -6 hours, DE = 90 degrees.
/////////////////////////////////////////////////////////////////////////////
void AstroTrac::getRADEFromEncoders(double haEncoder, double deEncoder, double &ra, double &de)
{
    static const double jitter = 0.0005;
    double ha = 0;

    // Take care of jitter
    if (haEncoder > jitter * -1 && haEncoder < jitter)
        haEncoder = 0;
    if (deEncoder > jitter * -1 && deEncoder < jitter)
        deEncoder = 0;

    // Northern Hemisphere
    if (LocationN[LOCATION_LATITUDE].value >= 0)
    {
        // "Normal" Pointing State (East, looking West)
        if (MountTypeSP.findOnSwitchIndex() == MOUNT_SINGLE_ARM || deEncoder >= 0)
        {
            de = std::min(90 - deEncoder, 90.0);
            ha = -6.0 + (haEncoder / 360.0) * 24.0 ;
        }
        // "Reversed" Pointing State (West, looking East)
        else
        {
            de = 90 + deEncoder;
            ha = 6.0 + (haEncoder / 360.0) * 24.0 ;
        }
    }
    else
    {
        // East
        if (MountTypeSP.findOnSwitchIndex() == MOUNT_SINGLE_ARM || deEncoder <= 0)
        {
            de = std::max(-90 - deEncoder, -90.0);
            ha = -6.0 - (haEncoder / 360.0) * 24.0 ;
        }
        // West
        else
        {
            de = -90 + deEncoder;
            ha = 6.0 - (haEncoder / 360.0) * 24.0 ;
        }
    }

    double lst = get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
    ra = range24(lst - ha);

    char RAStr[32] = {0}, DecStr[32] = {0};
    fs_sexa(RAStr, ra, 2, 3600);
    fs_sexa(DecStr, de, 2, 3600);
    LOGF_DEBUG("Encoders HA: %.4f DE: %.4f Processed: HA: %.4f DE: %.4f (%s) LST: %.4f RA: %.4f (%s)",
               haEncoder, deEncoder, ha, de, DecStr, lst, ra, RAStr);
}

/////////////////////////////////////////////////////////////////////////////
/// Based on X2 plugin
/// Mechanical DE Range: -180 to +180 degrees. Home Position Mechanical DE: 0
/// Mechanical HA Range: -90 to +90 degrees. Home Position Mechanical HA: 0
///
/////////////////////////////////////////////////////////////////////////////
void AstroTrac::getEncodersFromRADE(double ra, double de, double &haEncoder, double &deEncoder)
{
    double lst = get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
    double dHA = rangeHA(lst - ra);
    // Northern Hemisphere
    if (LocationN[LOCATION_LATITUDE].value >= 0)
    {
        // "Normal" Pointing State (East, looking West)
        if (MountTypeSP.findOnSwitchIndex() == MOUNT_SINGLE_ARM || dHA <= 0)
        {
            deEncoder = -(de - 90.0);
            haEncoder = (dHA + 6.0) * 360.0 / 24.0;
        }
        // "Reversed" Pointing State (West, looking East)
        else
        {
            deEncoder = de - 90.0;
            haEncoder = (dHA - 6.0) * 360.0 / 24.0;
        }
    }
    else
    {
        // "Normal" Pointing State (East, looking West)
        if (MountTypeSP.findOnSwitchIndex() == MOUNT_SINGLE_ARM || dHA <= 0)
        {
            deEncoder = -(de + 90.0);
            haEncoder = -(dHA + 6.0) * 360.0 / 24.0;
        }
        // "Reversed" Pointing State (West, looking East)
        else
        {
            deEncoder = (de + 90.0);
            haEncoder = -(dHA - 6.0) * 360 / 24.0;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::Sync(double ra, double dec)
{
    AlignmentDatabaseEntry NewEntry;
    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    // Actual Celestial Coordinates
    NewEntry.RightAscension        = ra;
    NewEntry.Declination           = dec;
    // Apparent Telescope Coordinates
    NewEntry.TelescopeDirection = TelescopeDirectionVectorFromEquatorialCoordinates(m_MountInternalCoordinates);
    NewEntry.PrivateDataSize = 0;

    if (!CheckForDuplicateSyncPoint(NewEntry, 0.001))
    {
        GetAlignmentDatabase().push_back(NewEntry);

        // Tell the client about size change
        UpdateSize();

        // Tell the math plugin to reinitiali
        Initialise(this);

        char RAStr[32], DecStr[32];
        fs_sexa(RAStr, ra, 2, 3600);
        fs_sexa(DecStr, dec, 2, 3600);
        LOGF_INFO("Syncing to JNOW RA %s - DEC %s", RAStr, DecStr);

        return true;
    }

    LOGF_DEBUG("Sync - duplicate entry RA: %lf(%lf) DEC: %lf", ra * 360.0 / 24.0, ra, dec);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::Goto(double ra, double dec)
{
    double haEncoder = 0, deEncoder = 0;
    INDI::IEquatorialCoordinates telescopeCoordinates;
    if (getTelescopeFromSkyCoordinates(ra, dec, telescopeCoordinates))
    {
        char mountRAString[32] = {0}, mountDEString[32] = {0}, skyRAString[32] = {0}, skyDEString[32] = {0};
        fs_sexa(mountRAString, telescopeCoordinates.rightascension, 2, 3600);
        fs_sexa(mountDEString, telescopeCoordinates.declination, 2, 3600);
        fs_sexa(skyRAString, ra, 2, 3600);
        fs_sexa(skyDEString, dec, 2, 3600);

        LOGF_DEBUG("GOTO Sky RA: %s DE: %s ---> Mount RA: %s DE: %s", skyRAString, skyDEString, mountRAString, mountDEString);

        getEncodersFromRADE(telescopeCoordinates.rightascension, telescopeCoordinates.declination, haEncoder, deEncoder);

        // Account for acceletaion, max speed, and deceleration by the time we get there.
        // Get time in seconds
        double tHA = calculateSlewTime(haEncoder - EncoderNP[AXIS_RA].getValue());
        // Adjust for hemisphere. Convert time to delta degrees
        tHA = tHA * (m_Location.latitude >= 0 ? 1 : -1) * TRACKRATE_SIDEREAL / 3600;

        LOGF_DEBUG("GOTO Encoders HA: %.4f (%.4f + %.4f) DE: %.4f", haEncoder + tHA, haEncoder, tHA, deEncoder);

        // Now go to each encoder
        bool rc1 = slewEncoder(AXIS_RA, haEncoder + tHA);
        bool rc2 = slewEncoder(AXIS_DE, deEncoder);

        if (rc1 && rc2)
        {
            TrackState = SCOPE_SLEWING;

            char RAStr[32], DecStr[32];
            fs_sexa(RAStr, ra, 2, 3600);
            fs_sexa(DecStr, dec, 2, 3600);
            LOGF_INFO("Slewing to JNOW RA %s - DEC %s", RAStr, DecStr);
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Function to estimate time to slew - distance in degrees
/// From X2 Plugin
/// distance in degrees
/////////////////////////////////////////////////////////////////////////////
double AstroTrac::calculateSlewTime(double distance)
{
    // Firstly throw away sign of distance - don't care about direction - and convert to arcsec
    distance = fabs(distance) * 3600.0;

    // Now estimate how far mount travels during accelertion and deceleration period
    double accelerate_decelerate = MAX_SLEW_VELOCITY * MAX_SLEW_VELOCITY / AccelerationNP[AXIS_RA].getValue();

    // If distance less than this, then calulate using accleration forumlae:
    if (distance < accelerate_decelerate)
    {
        return (2 * sqrt(distance / AccelerationNP[AXIS_RA].getValue()));
    }
    else
    {
        // Time is equal to twice the time required to accelerate or decelerate, plus the remaining distance at max slew speed
        return (2.0 * MAX_SLEW_VELOCITY / AccelerationNP[AXIS_RA].getValue() + (distance - accelerate_decelerate) /
                MAX_SLEW_VELOCITY);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::ReadScopeStatus()
{
    TelescopeDirectionVector TDV;
    double ra = 0, de = 0, skyRA = 0, skyDE = 0;

    if (isSimulation())
        simulateMount();

    double lastHAEncoder = EncoderNP[AXIS_RA].getValue();
    double lastDEEncoder = EncoderNP[AXIS_DE].getValue();
    if (getEncoderPosition(AXIS_RA) && getEncoderPosition(AXIS_DE))
    {
        getRADEFromEncoders(EncoderNP[AXIS_RA].getValue(), EncoderNP[AXIS_DE].getValue(), ra, de);
        // Send to client if changed.
        if (std::fabs(lastHAEncoder - EncoderNP[AXIS_RA].getValue()) > 0
                || std::fabs(lastDEEncoder - EncoderNP[AXIS_DE].getValue()) > 0)
        {
            EncoderNP.apply();
        }
    }
    else
        return false;

    if (TrackState == SCOPE_SLEWING || TrackState == SCOPE_PARKING)
    {
        if (isSlewComplete())
        {
            if (TrackState == SCOPE_SLEWING)
            {
                LOG_INFO("Slew complete, tracking...");
                TrackState = SCOPE_TRACKING;
                SetTrackEnabled(true);
            }
            // Parking
            else
            {
                SetTrackEnabled(false);
                SetParked(true);
            }
        }
    }

    m_MountInternalCoordinates.rightascension = ra;
    m_MountInternalCoordinates.declination = de;
    TDV = TelescopeDirectionVectorFromEquatorialCoordinates(m_MountInternalCoordinates);

    if (TransformTelescopeToCelestial(TDV, skyRA, skyDE))
    {
        double lst = get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
        double dHA = rangeHA(lst - skyRA);
        setPierSide(dHA < 0 ? PIER_EAST : PIER_WEST);

        char mountRAString[32] = {0}, mountDEString[32] = {0}, skyRAString[32] = {0}, skyDEString[32] = {0};
        fs_sexa(mountRAString, ra, 2, 3600);
        fs_sexa(mountDEString, de, 2, 3600);
        fs_sexa(skyRAString, skyRA, 2, 3600);
        fs_sexa(skyDEString, skyDE, 2, 3600);

        LOGF_DEBUG("Mount RA: %s DE: %s ---> Sky RA: %s DE: %s", mountRAString, mountDEString, skyRAString, skyDEString);

        NewRaDec(skyRA, skyDE);
        return true;
    }

    LOG_ERROR("TransformTelescopeToCelestial failed in ReadScopeStatus");
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::getTelescopeFromSkyCoordinates(double ra, double de, INDI::IEquatorialCoordinates &telescopeCoordinates)
{
    TelescopeDirectionVector TDV;
    if (TransformCelestialToTelescope(ra, de, 0.0, TDV))
    {
        EquatorialCoordinatesFromTelescopeDirectionVector(TDV, telescopeCoordinates);
        LOGF_DEBUG("TransformCelestialToTelescope: RA=%lf DE=%lf, TDV (x :%lf, y: %lf, z: %lf), local hour RA %lf DEC %lf",
                   ra, de, TDV.x, TDV.y, TDV.z, telescopeCoordinates.rightascension, telescopeCoordinates.declination);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::Park()
{
    if (slewEncoder(AXIS_RA, GetAxis1Park()) && slewEncoder(AXIS_DE, GetAxis2Park()))
    {
        TrackState = SCOPE_PARKING;
        LOG_INFO("Parking is in progress...");
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::UnPark()
{
    SetParked(false);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool AstroTrac::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Process alignment properties
        ProcessAlignmentTextProperties(this, name, texts, names, n);
    }

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Guide Rate
        if (GuideRateNP.isNameMatch(name))
        {
            GuideRateNP.update(values, names, n);
            GuideRateNP.setState(IPS_OK);
            GuideRateNP.apply();
            return true;
        }

        // Acceleration
        if (AccelerationNP.isNameMatch(name))
        {
            AccelerationNP.update(values, names, n);

            if (setAcceleration(AXIS_RA, AccelerationNP[AXIS_RA].getValue())
                    && setAcceleration(AXIS_DE, AccelerationNP[AXIS_DE].getValue()))
                AccelerationNP.setState(IPS_OK);
            else
                AccelerationNP.setState(IPS_ALERT);

            AccelerationNP.apply();
            return true;
        }

        // Encoders
        if (EncoderNP.isNameMatch(name))
        {
            if (slewEncoder(AXIS_RA, values[0]) && slewEncoder(AXIS_DE, values[1]))
            {
                TrackState = SCOPE_SLEWING;
                EncoderNP.setState(IPS_OK);
            }
            else
                EncoderNP.setState(IPS_ALERT);

            EncoderNP.apply();
            return true;
        }

        processGuiderProperties(name, values, names, n);

        // Process alignment properties
        ProcessAlignmentNumberProperties(this, name, values, names, n);

    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::ISNewSwitch(const char *dev, const char *name, ISState * states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Mount Type
        if (MountTypeSP.isNameMatch(name))
        {
            MountTypeSP.update(states, names, n);
            MountTypeSP.setState(IPS_OK);
            MountTypeSP.apply();
            return true;
        }

        // Process alignment properties
        ProcessAlignmentSwitchProperties(this, name, states, names, n);
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool AstroTrac::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                          char *formats[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Process alignment properties
        ProcessAlignmentBLOBProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::Abort()
{
    bool rc1 = setVelocity(AXIS_RA, 0) && stopMotion(AXIS_RA);
    bool rc2 = setVelocity(AXIS_DE, 0) && stopMotion(AXIS_DE);

    return rc1 && rc2;
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

    if (command == MOTION_START)
    {
        double velocity = SLEW_SPEEDS[IUFindOnSwitchIndex(&SlewRateSP)] * TRACKRATE_SIDEREAL
                          * (dir == DIRECTION_NORTH ? 1 : -1);
        setVelocity(AXIS_DE,  velocity);
    }
    else
    {
        setVelocity(AXIS_DE, TrackRateN[AXIS_DE].value);
        stopMotion(AXIS_DE);
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

    if (command == MOTION_START)
    {
        double velocity = SLEW_SPEEDS[IUFindOnSwitchIndex(&SlewRateSP)] * TRACKRATE_SIDEREAL
                          * (dir == DIRECTION_WEST ? 1 : -1);
        setVelocity(AXIS_RA,  velocity);
    }
    else
    {
        setVelocity(AXIS_RA, TrackRateN[AXIS_RA].value);
        stopMotion(AXIS_RA);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::updateLocation(double latitude, double longitude, double elevation)
{
    INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers::UpdateLocation(latitude, longitude, elevation);
    // Set this according to mount type
    SetApproximateMountAlignmentFromMountType(EQUATORIAL);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::updateTime(ln_date * utc, double utc_offset)
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
    SetAxis1Park(EncoderNP[AXIS_RA].getValue());
    SetAxis2Park(EncoderNP[AXIS_DE].getValue());

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::SetDefaultPark()
{
    SetAxis1Park(0);
    SetAxis2Park(0);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState AstroTrac::GuideNorth(uint32_t ms)
{
    // If track rate is zero, assume sidereal for DEC
    double rate = TrackRateN[AXIS_DE].value > 0 ? TrackRateN[AXIS_DE].value : TRACKRATE_SIDEREAL;
    // Find delta declination
    double dDE = GuideRateNP.at(AXIS_DE)->getValue() * rate * ms / 1000.0;
    // Final velocity guiding north is rate + dDE
    setVelocity(AXIS_DE, rate + dDE);
    INDI::Timer::singleShot(ms, [this]()
    {
        setVelocity(AXIS_DE, TrackRateN[AXIS_DE].value);
        GuideNSN[AXIS_RA].value = GuideNSN[AXIS_DE].value = 0;
        GuideNSNP.s = IPS_OK;
        IDSetNumber(&GuideNSNP, nullptr);
    });
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState AstroTrac::GuideSouth(uint32_t ms)
{
    // If track rate is zero, assume sidereal for DEC
    double rate = TrackRateN[AXIS_DE].value > 0 ? TrackRateN[AXIS_DE].value : TRACKRATE_SIDEREAL;
    // Find delta declination
    double dDE = GuideRateNP.at(AXIS_DE)->getValue() * rate * ms / 1000.0;
    // Final velocity guiding south is rate - dDE
    setVelocity(AXIS_DE, rate - dDE);
    INDI::Timer::singleShot(ms, [this]()
    {
        setVelocity(AXIS_DE, TrackRateN[AXIS_DE].value);
        GuideNSN[AXIS_RA].value = GuideNSN[AXIS_DE].value = 0;
        GuideNSNP.s = IPS_OK;
        IDSetNumber(&GuideNSNP, nullptr);
    });
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState AstroTrac::GuideEast(uint32_t ms)
{
    // Movement in arcseconds
    double dRA = GuideRateNP.at(AXIS_RA)->getValue() * TrackRateN[AXIS_RA].value * ms / 1000.0;
    // Final velocity guiding east is Sidereal + dRA
    setVelocity(AXIS_RA, TrackRateN[AXIS_RA].value + dRA);
    INDI::Timer::singleShot(ms, [this]()
    {
        setVelocity(AXIS_RA, TrackRateN[AXIS_RA].value);
        GuideWEN[AXIS_RA].value = GuideWEN[AXIS_DE].value = 0;
        GuideWENP.s = IPS_OK;
        IDSetNumber(&GuideWENP, nullptr);
    });
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState AstroTrac::GuideWest(uint32_t ms)
{
    // Movement in arcseconds
    double dRA = GuideRateNP.at(AXIS_RA)->getValue() * TrackRateN[AXIS_RA].value * ms / 1000.0;
    // Final velocity guiding east is Sidereal + dRA
    setVelocity(AXIS_RA, TrackRateN[AXIS_RA].value - dRA);
    INDI::Timer::singleShot(ms, [this]()
    {
        setVelocity(AXIS_RA, TrackRateN[AXIS_RA].value);
        GuideWEN[AXIS_RA].value = GuideWEN[AXIS_DE].value = 0;
        GuideWENP.s = IPS_OK;
        IDSetNumber(&GuideWENP, nullptr);
    });
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::SetTrackRate(double raRate, double deRate)
{
    return setVelocity(AXIS_RA, raRate) && setVelocity(AXIS_DE, deRate);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::SetTrackMode(uint8_t mode)
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

    return setVelocity(AXIS_RA, dRA) && setVelocity(AXIS_DE, dDE);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::SetTrackEnabled(bool enabled)
{
    // On engaging track, we simply set the current track mode and it will take care of the rest including custom track rates.
    if (enabled)
        return SetTrackMode(IUFindOnSwitchIndex(&TrackModeSP));
    // Disable tracking
    else
    {
        setVelocity(AXIS_RA, 0);
        setVelocity(AXIS_DE, 0);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Config Items
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &MountTypeSP);
    SaveAlignmentConfigProperties(fp);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Simple Mount Simulation
/////////////////////////////////////////////////////////////////////////////
void AstroTrac::simulateMount()
{
    // milliseconds elapsed
    uint32_t elapsed = m_SimulationTimer.elapsed();

    // If too much time elapsed, restart the timer.
    if (elapsed > 5000)
    {
        m_SimulationTimer.restart();
        return;
    }

    if (MovementWESP.s == IPS_BUSY || MovementNSSP.s == IPS_BUSY)
    {
        double haVelocity = SLEW_SPEEDS[IUFindOnSwitchIndex(&SlewRateSP)] * TRACKRATE_SIDEREAL
                            * (IUFindOnSwitchIndex(&MovementWESP) == DIRECTION_NORTH ? 1 : -1) * (m_Location.latitude >= 0 ? 1 : -1);
        double deVelocity = SLEW_SPEEDS[IUFindOnSwitchIndex(&SlewRateSP)] * TRACKRATE_SIDEREAL
                            * (IUFindOnSwitchIndex(&MovementNSSP) == DIRECTION_NORTH ? 1 : -1) * (m_Location.latitude >= 0 ? 1 : -1);

        haVelocity *= MovementWESP.s == IPS_BUSY ? 1 : 0;
        deVelocity *= MovementNSSP.s == IPS_BUSY ? 1 : 0;

        // In degrees
        double elapsedDistanceHA = (elapsed / 1000.0 * haVelocity) / 3600.0;
        double elapsedDistanceDE = (elapsed / 1000.0 * deVelocity) / 3600.0;

        // Hour Angle
        SimData.currentMechanicalHA += elapsedDistanceHA;
        if (SimData.currentMechanicalHA > 180)
            SimData.currentMechanicalHA -= 360;
        else if (SimData.currentMechanicalHA < -180)
            SimData.currentMechanicalHA += 360;

        SimData.currentMechanicalDE += elapsedDistanceDE;
        if (SimData.currentMechanicalDE > 180)
            SimData.currentMechanicalDE -= 360;
        else if (SimData.currentMechanicalDE < -180)
            SimData.currentMechanicalDE += 360;
    }
    else
    {
        switch (TrackState)
        {
            case SCOPE_IDLE:
            case SCOPE_PARKED:
                break;

            case SCOPE_SLEWING:
            case SCOPE_PARKING:
            {
                // In degrees
                double elapsedDistance = (elapsed / 1000.0 * MAX_SLEW_VELOCITY) / 3600.0;

                // Hour Angle
                double dHA = SimData.targetMechanicalHA - SimData.currentMechanicalHA;
                if (std::abs(dHA) <= elapsedDistance)
                    SimData.currentMechanicalHA = SimData.targetMechanicalHA;
                else if (dHA > 0)
                    SimData.currentMechanicalHA += elapsedDistance;
                else
                    SimData.currentMechanicalHA -= elapsedDistance;

                // Declination
                double dDE = SimData.targetMechanicalDE - SimData.currentMechanicalDE;
                if (std::abs(dDE) <= elapsedDistance)
                    SimData.currentMechanicalDE = SimData.targetMechanicalDE;
                else if (dDE > 0)
                    SimData.currentMechanicalDE += elapsedDistance;
                else
                    SimData.currentMechanicalDE -= elapsedDistance;
            }
            break;

            case SCOPE_TRACKING:
            {
                // Increase HA axis at selected tracking rate (arcsec/s).
                SimData.currentMechanicalHA += (elapsed / 1000.0 * TrackRateN[AXIS_RA].value) / 3600.0;
                if (SimData.currentMechanicalHA > 180)
                    SimData.currentMechanicalHA = 180;
                else if (SimData.currentMechanicalHA < -180)
                    SimData.currentMechanicalHA = -180;
            }
            break;
        }
    }

    m_SimulationTimer.restart();
}

/////////////////////////////////////////////////////////////////////////////
/// Handle Simulation Trigger
/////////////////////////////////////////////////////////////////////////////
void AstroTrac::simulationTriggered(bool enable)
{
    if (enable)
        m_SimulationTimer.start();
}

/////////////////////////////////////////////////////////////////////////////
/// Handle Simulation Command
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::handleSimulationCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    INDI_UNUSED(cmd_len);

    // Get version
    if (strstr(cmd, "zv?"))
    {
        snprintf(res, res_len, "<%czvSIMU>", cmd[1]);
    }
    // Get Encoder Position
    else if (strstr (cmd, "p?"))
    {
        snprintf(res, res_len, "<%cp%.6f>", cmd[1], cmd[1] == '1' ? SimData.currentMechanicalHA : SimData.currentMechanicalDE);
    }
    // Set Encoder Position
    else if (strstr (cmd, "p"))
    {
        double value = 0;
        sscanf(cmd, "<%*cp%lf", &value);
        if (cmd[1] == '1')
            SimData.targetMechanicalHA = value;
        else
            SimData.targetMechanicalDE = value;
        snprintf(res, res_len, "<%cp#>", cmd[1]);
    }
    // Get Acceleration
    else if (strstr (cmd, "a?"))
    {
        snprintf(res, res_len, "<%ca%u>", cmd[1], SimData.acceleration[cmd[1] - '1']);
    }
    // Set Acceleration
    else if (strstr (cmd, "a"))
    {
        uint32_t value = 0;
        sscanf(cmd, "<%*ca%u", &value);
        SimData.acceleration[cmd[1] - '1'] = value;
        snprintf(res, res_len, "<%ca#>", cmd[1]);
    }
    // Get Velocity
    else if (strstr (cmd, "v?"))
    {
        snprintf(res, res_len, "<%cv%.6f>", cmd[1], SimData.velocity[cmd[1] - '1']);
    }
    // Set Velocity using encoders
    else if (strstr (cmd, "ve"))
    {
        double value = 0;
        sscanf(cmd, "<%*cve%lf", &value);
        SimData.velocity[cmd[1] - '1'] = value;
        snprintf(res, res_len, "<%cve#>", cmd[1]);
    }
    // Get Slew status
    else if (strstr (cmd, "t"))
    {
        char result = '0';
        if (cmd[1] == '1')
            result = std::abs(SimData.currentMechanicalHA - SimData.targetMechanicalHA) <= DIFF_THRESHOLD ? '0' : '1';
        else
            result = std::abs(SimData.currentMechanicalDE - SimData.targetMechanicalDE) <= DIFF_THRESHOLD ? '0' : '1';
        snprintf(res, res_len, "<%ct%c#>", cmd[1], result);
    }
    // Abort
    else if (strstr (cmd, "x"))
    {
        snprintf(res, res_len, "<%cx#>", cmd[1]);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool AstroTrac::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    if (isSimulation())
        return handleSimulationCommand(cmd, res, cmd_len, res_len);

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
    {
        tcdrain(PortFD);
        return true;
    }

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
