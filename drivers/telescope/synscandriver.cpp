/*******************************************************************************
  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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

#include "synscandriver.h"
#include "libastro.h"
#include "connectionplugins/connectioninterface.h"
#include "connectionplugins/connectiontcp.h"
#include "indicom.h"

#include <libnova/transform.h>
#include <libnova/precession.h>
// libnova specifies round() on old systems and it collides with the new gcc 5.x/6.x headers
#define HAVE_ROUND
#include <libnova/utility.h>

#include <cmath>
#include <map>
#include <memory>
#include <termios.h>
#include <cstring>
#include <assert.h>

constexpr uint16_t SynscanDriver::SIM_SLEW_RATE[];

SynscanDriver::SynscanDriver(): GI(this)
{
    setVersion(2, 0);

    m_MountInfo.push_back("--");
    m_MountInfo.push_back("--");
    m_MountInfo.push_back("--");
    m_MountInfo.push_back("--");
    m_MountInfo.push_back("--");
}

const char * SynscanDriver::getDefaultName()
{
    return "SynScan";
}

bool SynscanDriver::initProperties()
{
    INDI::Telescope::initProperties();

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TRACK_MODE, 10);
    SetParkDataType(PARK_RA_DEC_ENCODER);

    // Slew Rates
    SlewRateSP[0].setLabel("1x");
    SlewRateSP[1].setLabel("8x");
    SlewRateSP[2].setLabel("16x");
    SlewRateSP[3].setLabel("32x");
    SlewRateSP[4].setLabel("64x");
    SlewRateSP[5].setLabel("128x");
    SlewRateSP[6].setLabel("400x");
    SlewRateSP[7].setLabel("600x");
    SlewRateSP[8].setLabel("MAX");
    SlewRateSP[9].setLabel("Custom");
    SlewRateSP.reset();
    // Max is the default
    SlewRateSP[8].setState(ISS_ON);

    //////////////////////////////////////////////////////////////////////////////////////////////////
    /// Mount Info Text Property
    //////////////////////////////////////////////////////////////////////////////////////////////////
    IUFillText(&StatusT[MI_FW_VERSION], "MI_FW_VERSION", "Firmware", "-");
    IUFillText(&StatusT[MI_MOUNT_MODEL], "MI_MOUNT_MODEL", "Model", "-");
    IUFillText(&StatusT[MI_GOTO_STATUS], "MI_GOTO_STATUS", "Goto", "-");
    IUFillText(&StatusT[MI_POINT_STATUS], "MI_POINT_STATUS", "Pointing", "-");
    IUFillText(&StatusT[MI_TRACK_MODE], "MI_TRACK_MODE", "Tracking Mode", "-");
    IUFillTextVector(&StatusTP, StatusT, 5, getDeviceName(), "MOUNT_STATUS",
                     "Status", MOUNT_TAB, IP_RO, 60, IPS_IDLE);

    //////////////////////////////////////////////////////////////////////////////////////////////////
    /// Custom Slew Rate
    //////////////////////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&CustomSlewRateN[AXIS_RA], "AXIS1", "RA/AZ (arcsecs/s)", "%.2f", 0.05, 800, 10, 0);
    IUFillNumber(&CustomSlewRateN[AXIS_DE], "AXIS2", "DE/AL (arcsecs/s)", "%.2f", 0.05, 800, 10, 0);
    IUFillNumberVector(&CustomSlewRateNP, CustomSlewRateN, 2, getDeviceName(), "CUSTOM_SLEW_RATE", "Custom Slew", MOTION_TAB,
                       IP_RW, 60, IPS_IDLE);

    //////////////////////////////////////////////////////////////////////////////////////////////////
    /// Guide Rate
    //////////////////////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&GuideRateN[AXIS_RA], "GUIDE_RATE_WE", "W/E Rate", "%.2f", 0, 1, 0.1, 0.5);
    IUFillNumber(&GuideRateN[AXIS_DE], "GUIDE_RATE_NS", "N/S Rate", "%.2f", 0, 1, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", GUIDE_TAB, IP_RW, 0,
                       IPS_IDLE);

    //////////////////////////////////////////////////////////////////////////////////////////////////
    /// Horizontal Coords
    //////////////////////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&HorizontalCoordsN[AXIS_AZ], "AZ", "Az D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
    IUFillNumber(&HorizontalCoordsN[AXIS_ALT], "ALT", "Alt  D:M:S", "%10.6m", -90., 90.0, 0.0, 0);
    IUFillNumberVector(&HorizontalCoordsNP, HorizontalCoordsN, 2, getDeviceName(), "HORIZONTAL_COORD",
                       "Horizontal Coord", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    AddTrackMode("TRACK_ALTAZ", "Alt/Az");
    AddTrackMode("TRACK_EQ", "Equatorial", true);
    AddTrackMode("TRACK_PEC", "PEC Mode");

    IUFillSwitch(&GotoModeS[0], "ALTAZ", "Alt/Az", ISS_OFF);
    IUFillSwitch(&GotoModeS[1], "RADEC", "Ra/Dec", ISS_ON);
    IUFillSwitchVector(&GotoModeSP, GotoModeS, NARRAY(GotoModeS), getDeviceName(), "GOTOMODE", "Goto mode",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    SetParkDataType(PARK_AZ_ALT);

    // Initialize guiding properties.
    GI::initProperties(GUIDE_TAB);

    addAuxControls();

    //GUIDE Set guider interface.
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    return true;
}

bool SynscanDriver::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        setupParams();

        defineProperty(&HorizontalCoordsNP);
        defineProperty(&StatusTP);
        defineProperty(&CustomSlewRateNP);
        defineProperty(&GuideRateNP);

        if (m_isAltAz)
        {
            defineProperty(&GotoModeSP);
        }

        if (InitPark())
        {
            SetAxis1ParkDefault(359);
            SetAxis2ParkDefault(m_isAltAz ? 0 : LocationNP[LOCATION_LATITUDE].getValue());
        }
        else
        {
            SetAxis1Park(359);
            SetAxis2Park(m_isAltAz ? 0 : LocationNP[LOCATION_LATITUDE].getValue());
            SetAxis1ParkDefault(359);
            SetAxis2ParkDefault(m_isAltAz ? 0 : LocationNP[LOCATION_LATITUDE].getValue());
        }
    }
    else
    {
        deleteProperty(HorizontalCoordsNP.name);
        deleteProperty(StatusTP.name);
        deleteProperty(CustomSlewRateNP.name);
        deleteProperty(GuideRateNP.name);
        if (m_isAltAz)
        {
            deleteProperty(GotoModeSP.name);
        }
    }

    GI::updateProperties();

    return true;
}

void SynscanDriver::setupParams()
{
    readFirmware();
    //readModel();
    readTracking();

    sendLocation();
    sendTime();
}

int SynscanDriver::hexStrToInteger(const std::string &res)
{
    int result = 0;

    try
    {
        result = std::stoi(res, nullptr, 16);
    }
    catch (std::invalid_argument &)
    {
        LOGF_ERROR("Failed to parse %s to integer.", res.c_str());
    }

    return result;
}

bool SynscanDriver::Handshake()
{
    char res[SYN_RES] = {0};
    if (!echo())
        return false;

    // We can only proceed if the mount is aligned.
    if (!sendCommand("J", res))
        return false;

    if (res[0] == 0)
    {
        LOG_ERROR("Mount is not aligned. Please align the mount first and connect again.");
        return false;
    }

    readModel();

    if (m_isAltAz)
    {
        SetTelescopeCapability(GetTelescopeCapability() & ~TELESCOPE_HAS_PIER_SIDE, 10);
    }

    return true;
}

bool SynscanDriver::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    // Check guider interface
    if (GI::processNumber(dev, name, values, names, n))
        return true;

    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Guide Rate
        if (strcmp(name, "GUIDE_RATE") == 0)
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);
            return true;
        }

        // Custom Slew Rate
        if (strcmp(name, CustomSlewRateNP.name) == 0)
        {
            if (TrackState == SCOPE_SLEWING)
            {
                LOG_ERROR("Cannot change rate while slewing.");
                CustomSlewRateNP.s = IPS_ALERT;
                IDSetNumber(&CustomSlewRateNP, nullptr);
                return true;
            }

            IUUpdateNumber(&CustomSlewRateNP, values, names, n);
            CustomSlewRateNP.s = IPS_OK;
            IDSetNumber(&CustomSlewRateNP, nullptr);
            return true;
        }

        // Horizontal Coords
        if (!strcmp(name, HorizontalCoordsNP.name))
        {
            if (isParked())
            {
                LOG_WARN("Unpark mount before issuing GOTO commands.");
                HorizontalCoordsNP.s = IPS_IDLE;
                IDSetNumber(&HorizontalCoordsNP, nullptr);
                return true;
            }

            int nset = 0;
            double newAlt = 0, newAz = 0;
            for (int i = 0; i < n; i++)
            {
                INumber * horp = IUFindNumber(&HorizontalCoordsNP, names[i]);
                if (horp == &HorizontalCoordsN[AXIS_AZ])
                {
                    newAz = values[i];
                    nset += newAz >= 0. && newAz <= 360.0;
                }
                else if (horp == &HorizontalCoordsN[AXIS_ALT])
                {
                    newAlt = values[i];
                    nset += newAlt >= -90. && newAlt <= 90.0;
                }
            }

            if (nset == 2 && GotoAzAlt(newAz, newAlt))
                return true;

            HorizontalCoordsNP.s = IPS_ALERT;
            IDSetNumber(&HorizontalCoordsNP, "Altitude or Azimuth missing or invalid.");
            return false;
        }
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool SynscanDriver::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        auto svp = getSwitch(name);

        if (svp.isNameMatch(GotoModeSP.name))
        {
            svp.update(states, names, n);
            auto sp = svp.findOnSwitch();

            assert(sp != nullptr);

            if (sp->isNameMatch(GotoModeS[0].name))
                SetAltAzMode(true);
            else
                SetAltAzMode(false);
            return true;
        }

    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool SynscanDriver::echo()
{
    char res[SYN_RES] = {0};
    return sendCommand("Kx", res);
}

bool SynscanDriver::readFirmware()
{
    // Read the handset version
    char res[SYN_RES] = {0};
    if (sendCommand("V", res))
    {
        m_FirmwareVersion = static_cast<double>(hexStrToInteger(std::string(&res[0], 2)));
        m_FirmwareVersion += static_cast<double>(hexStrToInteger(std::string(&res[2], 2))) / 100;
        m_FirmwareVersion += static_cast<double>(hexStrToInteger(std::string(&res[4], 2))) / 10000;

        LOGF_INFO("Firmware version: %lf", m_FirmwareVersion);
        m_MountInfo[MI_FW_VERSION] = std::to_string(m_FirmwareVersion);
        IUSaveText(&StatusT[MI_FW_VERSION], m_MountInfo[MI_FW_VERSION].c_str());

        if (m_FirmwareVersion < 3.38 || (m_FirmwareVersion >= 4.0 && m_FirmwareVersion < 4.38))
        {
            LOGF_WARN("Firmware version is too old. Update Synscan firmware to %s",
                      m_FirmwareVersion < 3.38 ? "v3.38+" : "v4.38+");
            return false;
        }
        else
            return true;
    }
    else
        LOG_WARN("Firmware version is too old. Update Synscan firmware to v4.38+");

    return false;
}

bool SynscanDriver::readTracking()
{
    // Read the handset version
    char res[SYN_RES] = {0};
    if (sendCommand("t", res))
    {
        // Are we tracking or not?
        m_TrackingFlag = res[0];

        // Track mode?
        if (((m_TrackingFlag - 1) != TrackModeSP.findOnSwitchIndex()) && (m_TrackingFlag))
        {
            TrackModeSP.reset();
            TrackModeSP[m_TrackingFlag - 1].setState(ISS_ON);
            TrackModeSP.apply();
        }

        switch(res[0])
        {
            case 0:
                m_MountInfo[MI_TRACK_MODE] = "Tracking off";
                break;
            case 1:
                m_MountInfo[MI_TRACK_MODE] = "Alt/Az tracking";
                break;
            case 2:
                m_MountInfo[MI_TRACK_MODE] = "EQ tracking";
                break;
            case 3:
                m_MountInfo[MI_TRACK_MODE] = "PEC mode";
                break;
        }

        return true;
    }

    return false;
}

bool SynscanDriver::readModel()
{
    // extended list of mounts
    std::map<int, std::string> models =
    {
        {0, "EQ6 GOTO Series"},
        {1, "HEQ5 GOTO Series"},
        {2, "EQ5 GOTO Series"},
        {3, "EQ3 GOTO Series"},
        {4, "EQ8 GOTO Series"},
        {5, "AZ-EQ6 GOTO Series"},
        {6, "AZ-EQ5 GOTO Series"},
        {160, "AllView GOTO Series"},
        {161, "Virtuoso Alt/Az mount"},
        {165, "AZ-GTi GOTO Series"}
    };

    // Read the handset version
    char res[SYN_RES] = {0};

    if (!sendCommand("m", res))
        return false;

    m_MountModel = res[0];

    // 128 - 143 --> AZ Goto series
    if (m_MountModel >= 128 && m_MountModel <= 143)
        IUSaveText(&StatusT[MI_MOUNT_MODEL], "AZ GOTO Series");
    // 144 - 159 --> DOB Goto series
    else if (m_MountModel >= 144 && m_MountModel <= 159)
        IUSaveText(&StatusT[MI_MOUNT_MODEL], "Dob GOTO Series");
    else if (models.count(m_MountModel) > 0)
        IUSaveText(&StatusT[MI_MOUNT_MODEL], models[m_MountModel].c_str());
    else
        IUSaveText(&StatusT[MI_MOUNT_MODEL], "Unknown model");

    m_isAltAz = m_MountModel > 4;

    LOGF_INFO("Driver is running in %s mode.", m_isAltAz ? "Alt-Az" : "Equatorial");
    LOGF_INFO("Detected mount: %s. Mount must be aligned from the handcontroller before using the driver.",
              StatusT[MI_MOUNT_MODEL].text);

    return true;
}

bool SynscanDriver::ReadScopeStatus()
{
    if (isSimulation())
    {
        mountSim();
        return true;
    }

    char res[SYN_RES] = {0};

    // Goto in progress?
    if (sendCommand("L", res))
        m_MountInfo[MI_GOTO_STATUS] = res[0];

    // Pier side
    if (m_isAltAz == false && sendCommand("p", res))
    {
        m_MountInfo[MI_POINT_STATUS] = res[0];
        // INDI and mount pier sides are opposite to each other
        setPierSide(res[0] == 'W' ? PIER_EAST : PIER_WEST);
    }

    if (readTracking())
    {
        if (TrackState == SCOPE_SLEWING)
        {
            if (isSlewComplete())
            {
                TrackState = (m_TrackingFlag == 2) ? SCOPE_TRACKING : SCOPE_IDLE;
                HorizontalCoordsNP.s = (m_TrackingFlag == 2) ? IPS_OK : IPS_IDLE;
                IDSetNumber(&HorizontalCoordsNP, nullptr);
            }
        }
        else if (TrackState == SCOPE_PARKING)
        {
            if (isSlewComplete())
            {
                HorizontalCoordsNP.s = IPS_IDLE;
                IDSetNumber(&HorizontalCoordsNP, nullptr);
                TrackState = SCOPE_PARKED;
                SetTrackEnabled(false);
                SetParked(true);
            }
        }
        else if (TrackState == SCOPE_IDLE && m_TrackingFlag > 0)
            TrackState = SCOPE_TRACKING;
        else if (TrackState == SCOPE_TRACKING && m_TrackingFlag == 0)
            TrackState = SCOPE_IDLE;
    }

    sendStatus();

    // Get Precise RA/DE
    memset(res, 0, SYN_RES);
    if (!sendCommand("e", res))
        return false;

    uint32_t n1 = 0, n2 = 0;
    sscanf(res, "%x,%x#", &n1, &n2);
    double ra  = static_cast<double>(n1) / 0x100000000 * 360.0;
    double de  = static_cast<double>(n2) / 0x100000000 * 360.0;

    INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };
    J2000Pos.rightascension  = range24(ra / 15.0);
    J2000Pos.declination = rangeDec(de);

    // Synscan reports J2000 coordinates so we need to convert from J2000 to JNow
    INDI::J2000toObserved(&J2000Pos, ln_get_julian_from_sys(), &epochPos);

    CurrentRA = epochPos.rightascension;
    CurrentDE = epochPos.declination;

    char Axis1Coords[MAXINDINAME] = {0}, Axis2Coords[MAXINDINAME] = {0};
    fs_sexa(Axis1Coords, J2000Pos.rightascension, 2, 3600);
    fs_sexa(Axis2Coords, J2000Pos.declination, 2, 3600);
    LOGF_DEBUG("J2000 RA <%s> DE <%s>", Axis1Coords, Axis2Coords);
    memset(Axis1Coords, 0, MAXINDINAME);
    memset(Axis2Coords, 0, MAXINDINAME);
    fs_sexa(Axis1Coords, CurrentRA, 2, 3600);
    fs_sexa(Axis2Coords, CurrentDE, 2, 3600);
    LOGF_DEBUG("JNOW  RA <%s> DE <%s>", Axis1Coords, Axis2Coords);

    //  Now feed the rest of the system with corrected data
    NewRaDec(CurrentRA, CurrentDE);

    // Get precise az/alt
    memset(res, 0, SYN_RES);
    if (!sendCommand("z", res))
        return false;

    sscanf(res, "%x,%x#", &n1, &n2);
    double az  = static_cast<double>(n1) / 0x100000000 * 360.0;
    double al  = static_cast<double>(n2) / 0x100000000 * 360.0;
    al = rangeDec(al);

    HorizontalCoordsN[AXIS_AZ].value = az;
    HorizontalCoordsN[AXIS_ALT].value = al;

    memset(Axis1Coords, 0, MAXINDINAME);
    memset(Axis2Coords, 0, MAXINDINAME);
    fs_sexa(Axis1Coords, az, 2, 3600);
    fs_sexa(Axis2Coords, al, 2, 3600);
    LOGF_DEBUG("AZ <%s> ALT <%s>", Axis1Coords, Axis2Coords);

    IDSetNumber(&HorizontalCoordsNP, nullptr);

    return true;
}

bool SynscanDriver::SetTrackEnabled(bool enabled)
{
    char cmd[SYN_RES] = {0}, res[SYN_RES] = {0};

    if (isSimulation())
        return true;

    cmd[0] = 'T';
    cmd[1] = enabled ? (TrackModeSP.findOnSwitchIndex() + 1) : 0;
    return sendCommand(cmd, res, 2);
}

bool SynscanDriver::SetTrackMode(uint8_t mode)
{
    char cmd[SYN_RES] = {0}, res[SYN_RES] = {0};

    if (isSimulation())
        return true;

    cmd[0] = 'T';
    cmd[1] = mode + 1;
    return sendCommand(cmd, res);
}

bool SynscanDriver::SetAltAzMode(bool enable)
{
    IUResetSwitch(&GotoModeSP);

    MountTypeSP.reset();
    MountTypeSP[MOUNT_ALTAZ].setState(enable ? ISS_ON : ISS_OFF);
    MountTypeSP[MOUNT_EQ_GEM].setState(!enable ? ISS_ON : ISS_OFF);

    if (enable)
    {
        ISwitch *sp = IUFindSwitch(&GotoModeSP, "ALTAZ");
        if (sp)
        {
            LOG_INFO("Using AltAz goto.");
            sp->s = ISS_ON;
        }
        goto_AltAz = true;
    }
    else
    {
        ISwitch *sp = IUFindSwitch(&GotoModeSP, "RADEC");
        if (sp)
        {
            sp->s = ISS_ON;
            LOG_INFO("Using Ra/Dec goto.");
        }
        goto_AltAz = false;
    }

    GotoModeSP.s = IPS_OK;
    IDSetSwitch(&GotoModeSP, nullptr);
    return true;
}

bool SynscanDriver::Goto(double ra, double dec)
{
    char cmd[SYN_RES] = {0}, res[SYN_RES] = {0};

    TargetRA = ra;
    TargetDE = dec;

    if (isSimulation())
        return true;

    // INDI is JNow. Synscan Controll uses J2000 Epoch
    INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };

    epochPos.rightascension  = ra;
    epochPos.declination = dec;

    // For Alt/Az mounts, we must issue Goto Alt/Az
    if (goto_AltAz && m_isAltAz) // only if enabled to use AltAz goto
    {
        INDI::IHorizontalCoordinates altaz;
        INDI::EquatorialToHorizontal(&epochPos, &m_Location, ln_get_julian_from_sys(), &altaz);
        /* libnova measures azimuth from south towards west */
        double az = altaz.azimuth;
        double al = altaz.altitude;

        return GotoAzAlt(az, al);
    }

    // Synscan accepts J2000 coordinates so we need to convert from JNow to J2000
    INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);

    double dec_pos = J2000Pos.declination;
    if (J2000Pos.declination < 0)
        dec_pos = dec_pos + 360;
    // Mount deals in J2000 coords.
    uint32_t n1 = J2000Pos.rightascension * 15.0 / 360 * 0x100000000;
    uint32_t n2 = dec_pos / 360 * 0x100000000;

    LOGF_DEBUG("Goto - JNow RA: %g JNow DE: %g J2000 RA: %g J2000 DE: %g", ra, dec, J2000Pos.rightascension,
               J2000Pos.declination);

    snprintf(cmd, SYN_RES, "r%08X,%08X", n1, n2);
    if (sendCommand(cmd, res, 18))
    {
        TrackState = SCOPE_SLEWING;
        HorizontalCoordsNP.s = IPS_BUSY;
        IDSetNumber(&HorizontalCoordsNP, nullptr);
        return true;
    }

    return false;
}

bool SynscanDriver::GotoAzAlt(double az, double alt)
{
    char cmd[SYN_RES] = {0}, res[SYN_RES] = {0};

    if (isSimulation())
        return true;

    if (m_isAltAz == false)
    {
        // For EQ Mount, we convert Parking Az/Alt to RA/DE and go to there.
        INDI::IHorizontalCoordinates horizontalPos;
        INDI::IEquatorialCoordinates equatorialPos;
        horizontalPos.azimuth = az;
        horizontalPos.altitude = alt;
        INDI::HorizontalToEquatorial(&horizontalPos, &m_Location, ln_get_julian_from_sys(), &equatorialPos);
        return Goto(equatorialPos.rightascension, equatorialPos.declination);
    }

    // Az/Alt to encoders
    uint32_t n1 = az  / 360.0 * 0x100000000;
    uint32_t n2 = alt / 360.0 * 0x100000000;

    LOGF_DEBUG("Goto - Az: %.2f Alt: %.2f", az, alt);

    snprintf(cmd, SYN_RES, "b%08X,%08X", n1, n2);
    if (sendCommand(cmd, res, 18))
    {
        TrackState = SCOPE_SLEWING;
        HorizontalCoordsNP.s = IPS_BUSY;
        IDSetNumber(&HorizontalCoordsNP, nullptr);
        return true;
    }

    return false;
}

bool SynscanDriver::Park()
{
    double parkAZ  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    if (GotoAzAlt(parkAZ, parkAlt))
    {
        TrackState = SCOPE_PARKING;
        LOG_INFO("Parking is in progress...");
        return true;
    }

    return false;
}

bool SynscanDriver::UnPark()
{
    SetParked(false);
    SetTrackMode(m_isAltAz ? 1 : 2);
    SetTrackEnabled(true);
    return true;
}

bool SynscanDriver::SetCurrentPark()
{
    char res[SYN_RES] = {0};

    // Get Current Az/Alt
    memset(res, 0, SYN_RES);
    if (!sendCommand("z", res))
        return false;

    uint32_t n1 = 0, n2 = 0;
    sscanf(res, "%ux,%ux#", &n1, &n2);
    double az  = static_cast<double>(n1) / 0x100000000 * 360.0;
    double al  = static_cast<double>(n2) / 0x100000000 * 360.0;
    al = rangeDec(al);

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, az, 2, 3600);
    fs_sexa(AltStr, al, 2, 3600);

    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr, AltStr);

    SetAxis1Park(az);
    SetAxis2Park(al);

    return true;
}

bool SynscanDriver::SetDefaultPark()
{
    // By default az to north, and alt to pole
    LOG_DEBUG("Setting Park Data to Default.");
    SetAxis1Park(359);
    SetAxis2Park(LocationNP[LOCATION_LATITUDE].getValue());

    return true;
}

bool SynscanDriver::Abort()
{
    if (TrackState == SCOPE_IDLE)
        return true;

    LOG_DEBUG("Abort mount...");
    TrackState = SCOPE_IDLE;

    if (isSimulation())
        return true;

    SetTrackEnabled(false);
    sendCommand("M");
    sendCommand("M");
    return true;
}

bool SynscanDriver::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (isSimulation())
        return true;

    bool rc = false;
    SynscanDirection move;

    if (currentPierSide == PIER_WEST)
        move = (dir == DIRECTION_NORTH) ? SYN_N : SYN_S;
    else
        move = (dir == DIRECTION_NORTH) ? SYN_S : SYN_N;

    uint8_t rate = static_cast<uint8_t>(SlewRateSP.findOnSwitchIndex()) + 1;
    double customRate = CustomSlewRateN[AXIS_DE].value;

    // If we have pulse guiding
    if (m_CustomGuideDE > 0)
    {
        rate = 10;
        customRate = m_CustomGuideDE;
    }

    switch (command)
    {
        case MOTION_START:
            rc = (rate < 10) ? slewFixedRate(move, rate) : slewVariableRate(move, customRate);
            if (!rc)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            // Only report messages if we are not guiding
            else if (!m_CustomGuideDE)
                LOGF_INFO("Moving toward %s.", (move == SYN_N) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (slewFixedRate(move, 0) == false)
            {
                LOG_ERROR("Error stopping N/S motion.");
                return false;
            }
            else if (!m_CustomGuideDE)
                LOGF_INFO("Movement toward %s halted.", (move == SYN_N) ? "North" : "South");
            break;
    }

    return true;
}

bool SynscanDriver::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (isSimulation())
        return true;

    bool rc = false;
    SynscanDirection move = (dir == DIRECTION_WEST) ? SYN_W : SYN_E;
    uint8_t rate = static_cast<uint8_t>(SlewRateSP.findOnSwitchIndex()) + 1;
    double customRate = CustomSlewRateN[AXIS_RA].value;

    // If we have pulse guiding
    if (m_CustomGuideRA > 0)
    {
        rate = 10;
        customRate = m_CustomGuideRA;
    }

    switch (command)
    {
        case MOTION_START:
            rc = (rate < 10) ? slewFixedRate(move, rate) : slewVariableRate(move, customRate);
            if (!rc)
            {
                LOG_ERROR("Error setting W/E motion direction.");
                return false;
            }
            // Only report messages if we are not guiding
            else if (!m_CustomGuideRA)
                LOGF_INFO("Moving toward %s.", (move == SYN_W) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (slewFixedRate(move, 0) == false)
            {
                LOG_ERROR("Error stopping W/E motion.");
                return false;
            }
            else if (!m_CustomGuideRA)
                LOGF_INFO("Movement toward %s halted.", (move == SYN_W) ? "West" : "East");
            break;
    }

    return true;
}

bool SynscanDriver::SetSlewRate(int s)
{
    m_TargetSlewRate = s + 1;
    return true;
}

#if 0
bool SynscanDriver::passThruCommand(int cmd, int target, int msgsize, int data, int numReturn)
{
    char test[20] = {0};
    int bytesRead, bytesWritten;
    char a, b, c;
    int tt = data;

    a  = tt % 256;
    tt = tt >> 8;
    b  = tt % 256;
    tt = tt >> 8;
    c  = tt % 256;

    //  format up a passthru command
    memset(test, 0, 20);
    test[0] = 80;      // passhtru
    test[1] = msgsize; // set message size
    test[2] = target;  // set the target
    test[3] = cmd;     // set the command
    test[4] = c;       // set data bytes
    test[5] = b;
    test[6] = a;
    test[7] = numReturn;

    LOGF_DEBUG("CMD <%s>", test);
    tty_write(PortFD, test, 8, &bytesWritten);
    memset(test, 0, 20);
    tty_read(PortFD, test, numReturn + 1, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", test);
    if (numReturn > 0)
    {
        int retval = 0;
        retval     = test[0];
        if (numReturn > 1)
        {
            retval = retval << 8;
            retval += test[1];
        }
        if (numReturn > 2)
        {
            retval = retval << 8;
            retval += test[2];
        }
        return retval;
    }

    return 0;
}
#endif

bool SynscanDriver::sendTime()
{
    LOG_DEBUG("Reading mount time...");

    if (isSimulation())
    {
        char timeString[MAXINDINAME] = {0};
        time_t now = time (nullptr);
        strftime(timeString, MAXINDINAME, "%T", gmtime(&now));
        TimeTP[UTC].setText("3");
        TimeTP[OFFSET].setText(timeString);
        TimeTP.setState(IPS_OK);
        TimeTP.apply();
        return true;
    }

    char res[SYN_RES] = {0};
    if (sendCommand("h", res))
    {
        ln_zonedate localTime;
        ln_date utcTime;
        int offset, daylightflag;

        localTime.hours   = res[0];
        localTime.minutes = res[1];
        localTime.seconds = res[2];
        localTime.months  = res[3];
        localTime.days    = res[4];
        localTime.years   = res[5];
        offset            = static_cast<int>(res[6]);
        // Negative GMT offset is read. It needs special treatment
        if (offset > 200)
            offset -= 256;
        localTime.gmtoff = offset;
        //  this is the daylight savings flag in the hand controller, needed if we did not set the time
        daylightflag = res[7];
        localTime.years += 2000;
        localTime.gmtoff *= 3600;
        //  now convert to utc
        ln_zonedate_to_date(&localTime, &utcTime);

        //  now we have time from the hand controller, we need to set some variables
        int sec;
        char utc[100];
        char ofs[16] = {0};
        sec = static_cast<int>(utcTime.seconds);
        sprintf(utc, "%04d-%02d-%dT%d:%02d:%02d", utcTime.years, utcTime.months, utcTime.days, utcTime.hours,
                utcTime.minutes, sec);
        if (daylightflag == 1)
            offset = offset + 1;
        snprintf(ofs, 16, "%d", offset);

        TimeTP[UTC].setText(utc);
        TimeTP[OFFSET].setText(ofs);
        TimeTP.setState(IPS_OK);
        TimeTP.apply();

        LOGF_INFO("Mount UTC Time %s Offset %d", utc, offset);

        return true;
    }
    return false;
}

bool SynscanDriver::sendLocation()
{
    char res[SYN_RES] = {0};

    LOG_DEBUG("Reading mount location...");

    if (isSimulation())
    {
        LocationNP[LOCATION_LATITUDE].setValue(29.5);
        LocationNP[LOCATION_LONGITUDE].setValue(48);
        LocationNP.apply();
        return true;
    }

    if (!sendCommand("w", res))
        return false;

    double lat, lon;
    //  lets parse this data now
    int a, b, c, d, e, f, g, h;
    a = res[0];
    b = res[1];
    c = res[2];
    d = res[3];
    e = res[4];
    f = res[5];
    g = res[6];
    h = res[7];

    double t1, t2, t3;

    t1  = c;
    t2  = b;
    t3  = a;
    t1  = t1 / 3600.0;
    t2  = t2 / 60.0;
    lat = t1 + t2 + t3;

    t1  = g;
    t2  = f;
    t3  = e;
    t1  = t1 / 3600.0;
    t2  = t2 / 60.0;
    lon = t1 + t2 + t3;

    if (d == 1)
        lat = lat * -1;
    if (h == 1)
        lon = 360 - lon;
    LocationNP[LOCATION_LATITUDE].setValue(lat);
    LocationNP[LOCATION_LONGITUDE].setValue(lon);
    LocationNP.apply();

    saveConfig(true, "GEOGRAPHIC_COORD");

    char LongitudeStr[32] = {0}, LatitudeStr[32] = {0};
    fs_sexa(LongitudeStr, lon, 2, 3600);
    fs_sexa(LatitudeStr, lat, 2, 3600);
    LOGF_INFO("Mount Longitude %s Latitude %s", LongitudeStr, LatitudeStr);

    return true;
}

bool SynscanDriver::updateTime(ln_date * utc, double utc_offset)
{
    char cmd[SYN_RES] = {0}, res[SYN_RES] = {0};

    //  start by formatting a time for the hand controller
    //  we are going to set controller to local time
    struct ln_zonedate ltm;

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600.0);

    int yr = ltm.years;

    yr = yr % 100;

    cmd[0] = 'H';
    cmd[1] = ltm.hours;
    cmd[2] = ltm.minutes;
    cmd[3] = static_cast<char>(ltm.seconds);
    cmd[4] = ltm.months;
    cmd[5] = ltm.days;
    cmd[6] = yr;
    //  offset from utc so hand controller is running in local time
    cmd[7] = utc_offset > 0 ? static_cast<uint8_t>(utc_offset) : static_cast<uint8_t>(256 + utc_offset);
    //  and no daylight savings adjustments, it's already included in the offset
    cmd[8] = 0;

    LOGF_INFO("Setting mount date/time to %04d-%02d-%02d %d:%02d:%02d UTC Offset: %.2f",
              ltm.years, ltm.months, ltm.days, ltm.hours, ltm.minutes, static_cast<int>(rint(ltm.seconds)), utc_offset);

    if (isSimulation())
        return true;

    return sendCommand(cmd, res, 9);
}

bool SynscanDriver::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    char cmd[SYN_RES] = {0}, res[SYN_RES] = {0};
    bool IsWest = false;

    ln_lnlat_posn p1 { 0, 0 };
    lnh_lnlat_posn p2;

    LocationNP[LOCATION_LATITUDE].setValue(latitude);
    LocationNP[LOCATION_LONGITUDE].setValue(longitude);
    LocationNP.apply();

    if (isSimulation())
    {
        if (!CurrentDE)
        {
            CurrentDE = latitude > 0 ? 90 : -90;
            CurrentRA = get_local_sidereal_time(longitude);
        }
        return true;
    }

    if (longitude > 180)
    {
        p1.lng = 360.0 - longitude;
        IsWest = true;
    }
    else
    {
        p1.lng = longitude;
    }
    p1.lat = latitude;
    ln_lnlat_to_hlnlat(&p1, &p2);
    LOGF_INFO("Update location to latitude %d:%d:%1.2f longitude %d:%d:%1.2f",
              p2.lat.degrees, p2.lat.minutes, p2.lat.seconds, p2.lng.degrees, p2.lng.minutes, p2.lng.seconds);

    cmd[0] = 'W';
    cmd[1] = p2.lat.degrees;
    cmd[2] = p2.lat.minutes;
    cmd[3] = rint(p2.lat.seconds);
    cmd[4] = (p2.lat.neg == 0) ? 0 : 1;
    cmd[5] = p2.lng.degrees;
    cmd[6] = p2.lng.minutes;
    cmd[7] = rint(p2.lng.seconds);
    cmd[8] = IsWest ? 1 : 0;

    return sendCommand(cmd, res, 9);
}

bool SynscanDriver::Sync(double ra, double dec)
{
    char cmd[SYN_RES] = {0}, res[SYN_RES] = {0};

    TargetRA = ra;
    TargetDE = dec;

    if (isSimulation())
        return true;

    // INDI is JNow. Synscan Controll uses J2000 Epoch
    INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };
    epochPos.rightascension = ra;
    epochPos.declination = dec;

    // Synscan accepts J2000 coordinates so we need to convert from JNow to J2000
    INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);

    // Mount deals in J2000 coords.
    uint32_t n1 = J2000Pos.rightascension * 15.0 / 360 * 0x100000000;
    uint32_t n2 = J2000Pos.declination / 360 * 0x100000000;

    LOGF_DEBUG("Sync - JNow RA: %g JNow DE: %g J2000 RA: %g J2000 DE: %g", ra, dec, J2000Pos.rightascension,
               J2000Pos.declination);

    snprintf(cmd, SYN_RES, "s%08X,%08X", n1, n2);
    return sendCommand(cmd, res, 18);
}

void SynscanDriver::sendStatus()
{
    bool BasicMountInfoHasChanged = false;

    if (std::string(StatusT[MI_GOTO_STATUS].text) != m_MountInfo[MI_GOTO_STATUS])
    {
        IUSaveText(&StatusT[MI_GOTO_STATUS], m_MountInfo[MI_GOTO_STATUS].c_str());
        BasicMountInfoHasChanged = true;
    }
    if (std::string(StatusT[MI_POINT_STATUS].text) != m_MountInfo[MI_POINT_STATUS])
    {
        IUSaveText(&StatusT[MI_POINT_STATUS], m_MountInfo[MI_POINT_STATUS].c_str());
        BasicMountInfoHasChanged = true;
    }
    if (std::string(StatusT[MI_TRACK_MODE].text) != m_MountInfo[MI_TRACK_MODE])
    {
        IUSaveText(&StatusT[MI_TRACK_MODE], m_MountInfo[MI_TRACK_MODE].c_str());
        BasicMountInfoHasChanged = true;
    }

    if (BasicMountInfoHasChanged)
    {
        StatusTP.s = IPS_OK;
        IDSetText(&StatusTP, nullptr);
    }
}

bool SynscanDriver::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[SYN_RES * 3] = {0};
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
        rc = tty_read(PortFD, res, res_len, SYN_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, SYN_RES, SYN_DEL, SYN_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[SYN_RES * 3] = {0};
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

void SynscanDriver::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

void SynscanDriver::mountSim()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt, da, dx;
    int nlocked;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&tv, nullptr);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt  = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec) / 1e6;
    ltv = tv;
    double currentSlewRate = SIM_SLEW_RATE[SlewRateSP.findOnSwitchIndex()] * TRACKRATE_SIDEREAL / 3600.0;
    da  = currentSlewRate * dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act accordingly */
    switch (TrackState)
    {
        case SCOPE_IDLE:
            CurrentRA += (TrackRateNP[AXIS_RA].getValue() / 3600.0 * dt) / 15.0;
            CurrentRA = range24(CurrentRA);
            break;

        case SCOPE_TRACKING:
            break;

        case SCOPE_SLEWING:
        case SCOPE_PARKING:
            /* slewing - nail it when both within one pulse @ SLEWRATE */
            nlocked = 0;

            dx = TargetRA - CurrentRA;

            // Take shortest path
            if (fabs(dx) > 12)
                dx *= -1;

            if (fabs(dx) <= da)
            {
                CurrentRA = TargetRA;
                nlocked++;
            }
            else if (dx > 0)
                CurrentRA += da / 15.;
            else
                CurrentRA -= da / 15.;

            if (CurrentRA < 0)
                CurrentRA += 24;
            else if (CurrentRA > 24)
                CurrentRA -= 24;

            dx = TargetDE - CurrentDE;
            if (fabs(dx) <= da)
            {
                CurrentDE = TargetDE;
                nlocked++;
            }
            else if (dx > 0)
                CurrentDE += da;
            else
                CurrentDE -= da;

            if (nlocked == 2)
            {
                if (TrackState == SCOPE_SLEWING)
                    TrackState = SCOPE_TRACKING;
                else
                    TrackState = SCOPE_PARKED;
            }

            break;

        default:
            break;
    }

    NewRaDec(CurrentRA, CurrentDE);
}

bool SynscanDriver::slewFixedRate(SynscanDirection direction, uint8_t rate)
{
    char cmd[SYN_RES] = {0}, res[SYN_RES] = {0};

    cmd[0] = 'P';
    cmd[1] = 2;
    // Axis 17 for DE/AL, 16 for RA/AZ
    cmd[2] = (direction == SYN_N || direction == SYN_S) ? 17 : 16;
    // Command 36 positive direction, 37 negative direction
    if (!m_isAltAz)
        cmd[3] = (direction == SYN_N || direction == SYN_W) ? 36 : 37;
    else
        cmd[3] = (direction == SYN_N || direction == SYN_W) ? 37 : 36;
    // Fixed rate (0 to 9) where 0 is stop
    cmd[4] = rate;

    return sendCommand(cmd, res, 8);
}

bool SynscanDriver::slewVariableRate(SynscanDirection direction, double rate)
{
    char cmd[SYN_RES] = {0}, res[SYN_RES] = {0};

    // According to Synscan documentation. We need to multiply by 4
    // then separate into high and low bytes
    uint16_t synRate = rint(rate * 4);

    cmd[0] = 'P';
    cmd[1] = 3;
    // Axis 17 for DE/AL, 16 for RA/AZ
    cmd[2] = (direction == SYN_N || direction == SYN_S) ? 17 : 16;
    // Command 6 positive direction, 7 negative direction
    cmd[3] = (direction == SYN_N || direction == SYN_W) ? 6 : 7;
    // High byte
    cmd[4] = synRate >> 8;
    // Low byte
    cmd[5] = synRate & 0xFF;

    return sendCommand(cmd, res, 8);
}

IPState SynscanDriver::GuideNorth(uint32_t ms)
{
    if (m_GuideNSTID)
    {
        IERmTimer(m_GuideNSTID);
        m_GuideNSTID = 0;
    }

    m_CustomGuideDE = TRACKRATE_SIDEREAL + GuideRateN[AXIS_DE].value * TRACKRATE_SIDEREAL;
    MoveNS(DIRECTION_NORTH, MOTION_START);
    m_GuideNSTID = IEAddTimer(ms, guideTimeoutHelperNS, this);
    return IPS_BUSY;
}

IPState SynscanDriver::GuideSouth(uint32_t ms)
{
    if (m_GuideNSTID)
    {
        IERmTimer(m_GuideNSTID);
        m_GuideNSTID = 0;
    }

    m_CustomGuideDE = TRACKRATE_SIDEREAL + GuideRateN[AXIS_DE].value * TRACKRATE_SIDEREAL;
    MoveNS(DIRECTION_SOUTH, MOTION_START);
    m_GuideNSTID = IEAddTimer(ms, guideTimeoutHelperNS, this);
    return IPS_BUSY;
}

IPState SynscanDriver::GuideEast(uint32_t ms)
{
    if (m_GuideWETID)
    {
        IERmTimer(m_GuideWETID);
        m_GuideWETID = 0;
    }

    // So if we SID_RATE + 0.5 * SID_RATE for example, that's 150% of sidereal rate
    // but for east we'd be going a lot faster since the stars are moving toward the west
    // in sidereal rate. Just standing still we would SID_RATE moving across. So for east
    // we just go GuideRate * SID_RATE without adding any more values.
    //m_CustomGuideRA = TRACKRATE_SIDEREAL + GuideRateN[AXIS_RA].value * TRACKRATE_SIDEREAL;
    m_CustomGuideRA = GuideRateN[AXIS_RA].value * TRACKRATE_SIDEREAL;

    MoveWE(DIRECTION_EAST, MOTION_START);
    m_GuideWETID = IEAddTimer(ms, guideTimeoutHelperWE, this);
    return IPS_BUSY;
}

IPState SynscanDriver::GuideWest(uint32_t ms)
{
    if (m_GuideWETID)
    {
        IERmTimer(m_GuideWETID);
        m_GuideWETID = 0;
    }

    // Sky already going westward (or earth rotating eastward, pick your favorite)
    // So we go SID_RATE + whatever guide rate was set to.
    m_CustomGuideRA = TRACKRATE_SIDEREAL + GuideRateN[AXIS_RA].value * TRACKRATE_SIDEREAL;
    MoveWE(DIRECTION_WEST, MOTION_START);
    m_GuideWETID = IEAddTimer(ms, guideTimeoutHelperWE, this);
    return IPS_BUSY;
}

void SynscanDriver::guideTimeoutHelperNS(void * context)
{
    static_cast<SynscanDriver *>(context)->guideTimeoutCallbackNS();
}

void SynscanDriver::guideTimeoutHelperWE(void * context)
{
    static_cast<SynscanDriver *>(context)->guideTimeoutCallbackWE();
}

void SynscanDriver::guideTimeoutCallbackNS()
{
    INDI_DIR_NS direction = static_cast<INDI_DIR_NS>(MovementNSSP.findOnSwitchIndex());
    MoveNS(direction, MOTION_STOP);
    GuideComplete(AXIS_DE);
    m_CustomGuideDE = m_GuideNSTID = 0;
}

void SynscanDriver::guideTimeoutCallbackWE()
{
    INDI_DIR_WE direction = static_cast<INDI_DIR_WE>(MovementWESP.findOnSwitchIndex());
    MoveWE(direction, MOTION_STOP);
    GuideComplete(AXIS_RA);
    m_CustomGuideRA = m_GuideWETID = 0;
}

bool SynscanDriver::isSlewComplete()
{
    char res[SYN_RES] = {0};

    if (!sendCommand("L", res))
        return false;

    return res[0] == '0';
}
