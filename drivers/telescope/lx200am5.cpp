/*
    ZWO Mount INDI driver

    Copyright (C) 2022-2027 Jasem Mutlaq

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

#include "lx200am5.h"

#include "connectionplugins/connectiontcp.h"
#include "lx200driver.h"
#include "indicom.h"

#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <regex>

LX200AM5::LX200AM5()
{
    setVersion(1, 6);

    setLX200Capability(LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_CAN_HOME_GO,
                           SLEW_MODES
                          );
}

bool LX200AM5::initProperties()
{
    LX200Generic::initProperties();

    SetParkDataType(PARK_SIMPLE);
    timeFormat = LX200_24;

    tcpConnection->setDefaultHost("192.168.4.1");
    tcpConnection->setDefaultPort(4030);
    tcpConnection->setLANSearchEnabled(true);

    if (strstr(getDeviceName(), "WiFi"))
        setActiveConnection(tcpConnection);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Properties
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    MountTypeSP[Azimuth].fill("Azimuth", "Azimuth", ISS_OFF);
    MountTypeSP[Equatorial].fill("Equatorial", "Equatorial", ISS_ON);
    MountTypeSP.fill(getDeviceName(), "MOUNT_TYPE", "Mount Type", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    MountTypeSP.load();

    if (MountTypeSP[Equatorial].getState() == ISS_ON)
        SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_HAS_PIER_SIDE, SLEW_MODES);

    // Slew Rates

    SlewRateSP[0].setLabel("0.25x");
    SlewRateSP[1].setLabel("0.5x");
    SlewRateSP[2].setLabel("1x");
    SlewRateSP[3].setLabel("2x");
    SlewRateSP[4].setLabel("4x");
    SlewRateSP[5].setLabel("8x");
    SlewRateSP[6].setLabel("20x");
    SlewRateSP[7].setLabel("60x");
    SlewRateSP[8].setLabel("720x");
    SlewRateSP[9].setLabel("1440x");
    SlewRateSP.reset();
    // 1440x is the default
    SlewRateSP[9].setState(ISS_ON);

    // Home/Zero position
    // HomeSP[0].fill("GO", "Go", ISS_OFF);
    // HomeSP.fill(getDeviceName(), "TELESCOPE_HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Guide Rate
    GuideRateNP[0].fill("RATE", "Rate", "%.2f", 0.1, 0.9, 0.1, 0.5);
    GuideRateNP.fill(getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 60, IPS_IDLE);
    GuideRateNP.load();

    // Buzzer
    BuzzerSP[Off].fill("OFF", "Off", ISS_OFF);
    BuzzerSP[Low].fill("LOW", "Low", ISS_OFF);
    BuzzerSP[High].fill("HIGH", "High", ISS_ON);
    BuzzerSP.fill(getDeviceName(), "BUZZER", "Buzzer", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Heavy Duty Mode
    HeavyDutyModeSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_OFF);
    HeavyDutyModeSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_OFF);
    HeavyDutyModeSP.fill(getDeviceName(), "HEAVY_DUTY_MODE", "Heavy Duty Mode", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Meridian Flip Enable
    MeridianFlipSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_ON);
    MeridianFlipSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_OFF);
    MeridianFlipSP.fill(getDeviceName(), "MERIDIAN_FLIP", "Meridian Flip", MERIDIAN_FLIP_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    MeridianFlipSP.load();

    // Post Meridian Track
    PostMeridianTrackSP[TRACK].fill("TRACK", "Track", ISS_ON);
    PostMeridianTrackSP[STOP].fill("STOP", "Stop", ISS_OFF);
    PostMeridianTrackSP.fill(getDeviceName(), "POST_MERIDIAN_TRACK", "After Meridian", MERIDIAN_FLIP_TAB, IP_RW, ISR_1OFMANY,
                             60, IPS_IDLE);
    PostMeridianTrackSP.load();

    // Meridian Flip Limit
    MeridianLimitNP[0].fill("LIMIT", "Limit (deg)", "%.f", -15, 15, 1, 0);
    MeridianLimitNP.fill(getDeviceName(), "MERIDIAN_LIMIT", "Meridian Limit", MERIDIAN_FLIP_TAB, IP_RW, 60, IPS_IDLE);
    MeridianLimitNP.load();

    // Altitude Limits
    AltitudeLimitSP[INDI_ENABLED].fill("ENABLE", "Enable", ISS_OFF);
    AltitudeLimitSP[INDI_DISABLED].fill("DISABLE", "Disable", ISS_ON);
    AltitudeLimitSP.fill(getDeviceName(), "ALTITUDE_LIMIT_CONTROL", "Altitude Limit Control", ALTITUDE_LIMIT_TAB, IP_RW,
                         ISR_1OFMANY, 60, IPS_IDLE);

    AltitudeLimitUpperNP[0].fill("UPPER_LIMIT", "Upper Limit (deg)", "%.f", 60, 90, 1, 90);
    AltitudeLimitUpperNP.fill(getDeviceName(), "ALTITUDE_UPPER_LIMIT", "Altitude Upper Limit", ALTITUDE_LIMIT_TAB, IP_RW, 60,
                              IPS_IDLE);

    AltitudeLimitLowerNP[0].fill("LOWER_LIMIT", "Lower Limit (deg)", "%.f", 0, 30, 1, 0);
    AltitudeLimitLowerNP.fill(getDeviceName(), "ALTITUDE_LOWER_LIMIT", "Altitude Lower Limit", ALTITUDE_LIMIT_TAB, IP_RW, 60,
                              IPS_IDLE);

    // Multi-Star Alignment
    MultiStarAlignSP[CLEAR_ALIGNMENT_DATA].fill("CLEAR", "Clear Data", ISS_OFF);
    MultiStarAlignSP.fill(getDeviceName(), "MULTI_STAR_ALIGNMENT", "Multi-Star Alignment", ALIGNMENT_TAB, IP_RW, ISR_ATMOST1,
                          60, IPS_IDLE);

    // Variable Slew Speed
    VariableSlewRateNP[0].fill("RATE", "Rate (x Sidereal)", "%.2f", 0.00, 1440.00, 0.01, 1440.00);
    VariableSlewRateNP.fill(getDeviceName(), "VARIABLE_SLEW_RATE", "Variable Slew Rate", MOTION_TAB, IP_RW, 60, IPS_IDLE);

    // Firmware
    FirmwareTP[FIRMWARE_MODEL].fill("MODEL", "Model", "");
    FirmwareTP[FIRMWARE_VERSION].fill("VERSION", "Version", "");
    FirmwareTP.fill(getDeviceName(), "MOUNT_FIRMWARE", "Firmware", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // Custom park position (config only, never defined)
    CustomParkSP[0].fill("SET", "Set", ISS_OFF);
    CustomParkSP.fill(getDeviceName(), "CUSTOM_PARK_POSITION", "Custom Park", SITE_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);
    CustomParkSP.load();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        setup();

        //defineProperty(HomeSP);
        defineProperty(GuideRateNP);
        defineProperty(BuzzerSP);
        defineProperty(HeavyDutyModeSP);

        // Only define meridian flip properties for equatorial mount
        if (MountTypeSP[Equatorial].getState() == ISS_ON)
        {
            defineProperty(MeridianFlipSP);
            defineProperty(PostMeridianTrackSP);
            defineProperty(MeridianLimitNP);
        }

        // Altitude Limits
        defineProperty(AltitudeLimitSP);
        defineProperty(AltitudeLimitUpperNP);
        defineProperty(AltitudeLimitLowerNP);

        // Multi-Star Alignment
        defineProperty(MultiStarAlignSP);

        // Variable Slew Speed
        defineProperty(VariableSlewRateNP);

        defineProperty(FirmwareTP);
    }
    else
    {
        //deleteProperty(HomeSP);
        deleteProperty(GuideRateNP);
        deleteProperty(BuzzerSP);
        deleteProperty(HeavyDutyModeSP);

        // Only delete meridian flip properties if they were defined
        if (MountTypeSP[Equatorial].getState() == ISS_ON)
        {
            deleteProperty(MeridianFlipSP);
            deleteProperty(PostMeridianTrackSP);
            deleteProperty(MeridianLimitNP);
        }

        // Altitude Limits
        deleteProperty(AltitudeLimitSP);
        deleteProperty(AltitudeLimitUpperNP);
        deleteProperty(AltitudeLimitLowerNP);

        // Multi-Star Alignment
        deleteProperty(MultiStarAlignSP);

        // Variable Slew Speed
        deleteProperty(VariableSlewRateNP);

        deleteProperty(FirmwareTP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *LX200AM5::getDefaultName()
{
    return "ZWO AM5";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::checkConnection()
{
    LOG_DEBUG("Checking AM5 connection...");

    double targetRA = 0;
    for (int i = 0; i < 2; i++)
    {
        if (getLX200RA(PortFD, &targetRA) == 0)
            return true;

        const struct timespec ms250_delay = {.tv_sec = 0, .tv_nsec = 250000000};
        nanosleep(&ms250_delay, NULL);
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void LX200AM5::setup()
{
    // Mount Type
    //    MountTypeSP.setState(setMountType(MountTypeSP.findOnSwitchIndex()) ? IPS_OK : IPS_ALERT);
    //    MountTypeSP.apply();

    InitPark();

    getFirmwareInfo();
    getMountType();
    getTrackMode();
    getGuideRate();
    getBuzzer();
    getHeavyDutyMode();

    // Only get meridian flip settings for equatorial mount
    if (MountTypeSP[Equatorial].getState() == ISS_ON)
        getMeridianFlipSettings();

    // Get altitude limit settings
    getAltitudeLimitStatus();
    getAltitudeLimitUpper();
    getAltitudeLimitLower();

    // A park made outside the driver (e.g. by the vendor app) leaves the mount latched in the parked state.
    checkMountParkState();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Mount Type
        // updateProperty only calls the lambda when the value actually changes.
        // When disconnected the lambda returns true so the selection is still saved.
        if (MountTypeSP.isNameMatch(name))
        {
            return updateProperty(MountTypeSP, states, names, n, [this, names]()
            {
                if (isConnected())
                {
                    int previousType = MountTypeSP.findOnSwitchIndex();
                    int newType = MountTypeSP[Azimuth].isNameMatch(names[0]) ? Azimuth : Equatorial;
                    bool rc = setMountType(newType);
                    if (rc && previousType != newType)
                        LOG_WARN("You must restart mount for change to take effect.");
                    return rc;
                }
                return true;
            }, true);
        }

        // Buzzer
        if (BuzzerSP.isNameMatch(name))
        {
            return updateProperty(BuzzerSP, states, names, n, [this, names]()
            {
                if (BuzzerSP[Off].isNameMatch(names[0]))
                    return setBuzzer(Off);
                if (BuzzerSP[Low].isNameMatch(names[0]))
                    return setBuzzer(Low);
                if (BuzzerSP[High].isNameMatch(names[0]))
                    return setBuzzer(High);
                return false;
            }, true);
        }

        // Heavy Duty Mode
        if (HeavyDutyModeSP.isNameMatch(name))
        {
            return updateProperty(HeavyDutyModeSP, states, names, n, [this, names]()
            {
                return setHeavyDutyMode(HeavyDutyModeSP[INDI_ENABLED].isNameMatch(names[0]));
            }, true);
        }

        // Meridian Flip Enable
        if (MeridianFlipSP.isNameMatch(name))
        {
            return updateProperty(MeridianFlipSP, states, names, n, [this, names]()
            {
                return setMeridianFlipSettings(MeridianFlipSP[INDI_ENABLED].isNameMatch(names[0]),
                                               PostMeridianTrackSP[TRACK].getState() == ISS_ON,
                                               MeridianLimitNP[0].getValue());
            }, true);
        }

        // Post Meridian Track
        if (PostMeridianTrackSP.isNameMatch(name))
        {
            return updateProperty(PostMeridianTrackSP, states, names, n, [this, names]()
            {
                return setMeridianFlipSettings(MeridianFlipSP[INDI_ENABLED].getState() == ISS_ON,
                                               PostMeridianTrackSP[TRACK].isNameMatch(names[0]),
                                               MeridianLimitNP[0].getValue());
            }, true);
        }

        // Custom park position, only received when the config is loaded.
        if (CustomParkSP.isNameMatch(name))
        {
            CustomParkSP.update(states, names, n);
            return true;
        }

        // Altitude Limit Enable/Disable
        if (AltitudeLimitSP.isNameMatch(name))
        {
            return updateProperty(AltitudeLimitSP, states, names, n, [this, names]()
            {
                return setAltitudeLimitEnabled(AltitudeLimitSP[INDI_ENABLED].isNameMatch(names[0]));
            }, true);
        }

        // Multi-Star Alignment (momentary action — handled manually like AbortRotatorSP in rotator interface)
        if (MultiStarAlignSP.isNameMatch(name))
        {
            MultiStarAlignSP.update(states, names, n);
            if (MultiStarAlignSP[CLEAR_ALIGNMENT_DATA].getState() == ISS_ON)
            {
                MultiStarAlignSP.setState(clearMultiStarAlignmentData() ? IPS_OK : IPS_ALERT);

            }
            else
                MultiStarAlignSP.setState(IPS_IDLE);

            MultiStarAlignSP.apply();
            return true;
        }
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Guide Rate
        if (GuideRateNP.isNameMatch(name))
        {
            return updateProperty(GuideRateNP, values, names, n, [this, values]()
            {
                return setGuideRate(values[0]);
            }, true);
        }

        // Meridian Flip Limit
        if (MeridianLimitNP.isNameMatch(name))
        {
            return updateProperty(MeridianLimitNP, values, names, n, [this, values]()
            {
                return setMeridianFlipSettings(MeridianFlipSP[INDI_ENABLED].getState() == ISS_ON,
                                               PostMeridianTrackSP[TRACK].getState() == ISS_ON,
                                               values[0]);
            }, true);
        }

        // Altitude Upper Limit
        if (AltitudeLimitUpperNP.isNameMatch(name))
        {
            return updateProperty(AltitudeLimitUpperNP, values, names, n, [this, values]()
            {
                return setAltitudeLimitUpper(values[0]);
            }, true);
        }

        // Altitude Lower Limit
        if (AltitudeLimitLowerNP.isNameMatch(name))
        {
            return updateProperty(AltitudeLimitLowerNP, values, names, n, [this, values]()
            {
                return setAltitudeLimitLower(values[0]);
            }, true);
        }

        // Variable Slew Rate
        if (VariableSlewRateNP.isNameMatch(name))
        {
            return updateProperty(VariableSlewRateNP, values, names, n, [this, values]()
            {
                return setVariableSlewRate(values[0]);
            });
        }
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setMountType(int type)
{
    return sendCommand((type == Azimuth) ? ":AA#" : ":AP#");
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::getMountType()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GU#", response))
    {
        MountTypeSP.reset();
        MountTypeSP[Azimuth].setState(strchr(response, 'Z') ? ISS_ON : ISS_OFF);
        MountTypeSP[Equatorial].setState(strchr(response, 'G') ? ISS_ON : ISS_OFF);
        MountTypeSP.setState(IPS_OK);
        return true;
    }
    else
    {
        MountTypeSP.setState(IPS_ALERT);
        return true;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::SetSlewRate(int index)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":R%d#", index);
    return sendCommand(command);
}



/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setGuideRate(double value)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":Rg%.2f#", value);
    return sendCommand(command);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::getGuideRate()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":Ggr#", response))
    {
        float rate = 0;
        int result = sscanf(response, "%f", &rate);
        if (result == 1)
        {
            GuideRateNP[0].setValue(rate);
            return true;
        }
    }

    GuideRateNP.setState(IPS_ALERT);
    return false;

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::getTrackMode()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GT#", response))
    {
        TrackModeSP.reset();
        auto onIndex = response[0] - 0x30;
        if (onIndex >= 0 && onIndex < static_cast<int>(TrackModeSP.count()))
        {
            TrackModeSP[onIndex].setState(ISS_ON);
            return true;
        }
    }

    TrackModeSP.setState(IPS_ALERT);
    return false;

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setBuzzer(int value)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":SBu%d#", value);
    return sendCommand(command);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::getBuzzer()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GBu#", response))
    {
        BuzzerSP.reset();
        auto onIndex = response[0] - 0x30;
        if (onIndex >= 0 && onIndex < static_cast<int>(BuzzerSP.count()))
        {
            BuzzerSP[onIndex].setState(ISS_ON);
            BuzzerSP.setState(IPS_OK);
            return true;
        }
    }

    BuzzerSP.setState(IPS_ALERT);
    return true;

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::getHeavyDutyMode()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GRl#", response))
    {
        HeavyDutyModeSP.reset();

        if (strcmp(response, "1440#") == 0)
        {
            HeavyDutyModeSP[INDI_DISABLED].setState(ISS_ON);
        }

        if (strcmp(response, "720#") == 0)
        {
            HeavyDutyModeSP[INDI_ENABLED].setState(ISS_ON);
        }

        HeavyDutyModeSP.setState(IPS_OK);
        return true;
    }
    else
    {
        HeavyDutyModeSP.setState(IPS_ALERT);
        return true;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setHeavyDutyMode(bool enable)
{
    // Reply must be read, otherwise it is left in the input stream for the next command.
    char response[DRIVER_LEN] = {0};
    return sendCommand(enable ? ":SRl720#" : ":SRl1440#", response, -1, 1) && response[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setMeridianFlipSettings(bool enabled, bool track, double limit)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":STa%d%d%c%02d#", enabled ? 1 : 0, track ? 1 : 0,
             limit >= 0 ? '+' : '-', static_cast<int>(std::abs(limit)));
    char response[2] = {0};
    auto rc = sendCommand(command, response, -1, 1);
    return rc && response[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::getMeridianFlipSettings()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GTa#", response))
    {
        if (strlen(response) >= 5)
        {
            // First digit: meridian flip enabled
            MeridianFlipSP.reset();
            MeridianFlipSP[response[0] == '1' ? INDI_ENABLED : INDI_DISABLED].setState(ISS_ON);
            MeridianFlipSP.setState(IPS_OK);

            // Second digit: track after meridian
            PostMeridianTrackSP.reset();
            PostMeridianTrackSP[response[1] == '1' ? TRACK : STOP].setState(ISS_ON);
            PostMeridianTrackSP.setState(IPS_OK);

            // Last three digits: limit angle
            char *sign = &response[2];
            char *limit = &response[3];
            int limitValue = atoi(limit);
            if (*sign == '-')
                limitValue = -limitValue;
            MeridianLimitNP[0].setValue(limitValue);
            MeridianLimitNP.setState(IPS_OK);

            return true;
        }
    }

    MeridianFlipSP.setState(IPS_ALERT);
    PostMeridianTrackSP.setState(IPS_ALERT);
    MeridianLimitNP.setState(IPS_ALERT);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Altitude Limits
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setAltitudeLimitEnabled(bool enable)
{
    // Reply must be read, otherwise it is left in the input stream for the next command.
    char response[DRIVER_LEN] = {0};
    return sendCommand(enable ? ":SLE#" : ":SLD#", response, -1, 1) && response[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::getAltitudeLimitStatus()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GLC#", response))
    {
        AltitudeLimitSP.reset();
        AltitudeLimitSP[response[0] == '1' ? INDI_ENABLED : INDI_DISABLED].setState(ISS_ON);
        AltitudeLimitSP.setState(IPS_OK);
        return true;
    }
    AltitudeLimitSP.setState(IPS_ALERT);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setAltitudeLimitUpper(double limit)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":SLH%02d#", static_cast<int>(limit));
    char response[2] = {0};
    auto rc = sendCommand(command, response, -1, 1);
    return rc && response[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::getAltitudeLimitUpper()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GLH#", response))
    {
        int limit = 0;
        if (sscanf(response, "%d#", &limit) == 1)
        {
            AltitudeLimitUpperNP[0].setValue(limit);
            AltitudeLimitUpperNP.setState(IPS_OK);
            return true;
        }
    }
    AltitudeLimitUpperNP.setState(IPS_ALERT);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setAltitudeLimitLower(double limit)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":SLL%02d#", static_cast<int>(limit));
    char response[2] = {0};
    auto rc = sendCommand(command, response, -1, 1);
    return rc && response[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::getAltitudeLimitLower()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GLL#", response))
    {
        int limit = 0;
        if (sscanf(response, "%d#", &limit) == 1)
        {
            AltitudeLimitLowerNP[0].setValue(limit);
            AltitudeLimitLowerNP.setState(IPS_OK);
            return true;
        }
    }
    AltitudeLimitLowerNP.setState(IPS_ALERT);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Multi-Star Alignment
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::clearMultiStarAlignmentData()
{
    char response[2] = {0};
    auto rc = sendCommand(":NSC#", response, -1, 1);
    return rc && response[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
/// Variable Slew Speed
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setVariableSlewRate(double rate)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":Rv%.2f#", rate);
    return sendCommand(command);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setUTCOffset(double offset)
{
    int h {0}, m {0}, s {0};
    char command[DRIVER_LEN] = {0};
    offset *= -1;
    getSexComponents(offset, &h, &m, &s);

    snprintf(command, DRIVER_LEN, ":SG%c%02d:%02d#", offset >= 0 ? '+' : '-', std::abs(h), m);
    return setStandardProcedure(PortFD, command) == 0;
}

bool LX200AM5::setLocalDate(uint8_t days, uint8_t months, uint16_t years)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":SC%02d/%02d/%02d#", months, days, years % 100);
    return setStandardProcedure(PortFD, command) == 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::SetTrackEnabled(bool enabled)
{
    char response[DRIVER_LEN] = {0};
    bool rc = sendCommand(enabled ? ":Te#" : ":Td#", response, 4, 1);
    return rc && response[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::isTracking()
{
    char response[DRIVER_LEN] = {0};
    bool rc = sendCommand(":GAT#", response);
    return rc && response[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::goHome()
{
    return sendCommand(":hC#");
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::park()
{
    // JM 2025.11.08: Many users do not like default ZWO parking position
    // which is horizontal and does not go back to expected home position
    // with CW down and looking at celestial pole.
    // For now this is reverted now back to go back to that position instead
    // of parking until ZWO releases update for custom parking positions.
    return goHome();
    //return sendCommand(":hP#");
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::setHome()
{
    const char cmd[] = ":SOa#";
    char status = '0';
    return sendCommand(cmd, &status, strlen(cmd), sizeof(status)) && status == '1';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    int d{0}, m{0}, s{0};

    // JM 2021-04-10: MUST convert from INDI longitude to standard longitude.
    // DO NOT REMOVE
    if (longitude > 180)
        longitude = longitude - 360;

    char command[DRIVER_LEN] = {0};
    // Reverse as per Meade way
    longitude *= -1;
    getSexComponents(longitude, &d, &m, &s);
    snprintf(command, DRIVER_LEN, ":Sg%c%03d*%02d:%02d#", longitude >= 0 ? '+' : '-', std::abs(d), m, s);
    if (setStandardProcedure(PortFD, command) < 0)
    {
        LOG_ERROR("Error setting site longitude coordinates");
        return false;
    }

    getSexComponents(latitude, &d, &m, &s);
    snprintf(command, DRIVER_LEN, ":St%c%02d*%02d:%02d#", latitude >= 0 ? '+' : '-', std::abs(d), m, s);
    if (setStandardProcedure(PortFD, command) < 0)
    {
        LOG_ERROR("Error setting site latitude coordinates");
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::Park()
{
    m_ParkStage = ParkStage::None;

    bool rc = false;
    // Without a custom park position, the mount would go to its factory park position which is horizontal.
    if (hasCustomParkPosition() && CustomParkSP[0].getState() == ISS_ON)
        rc = parkToMountPosition();
    else
        rc = park();

    if (rc)
        TrackState = SCOPE_PARKING;
    return rc;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::UnPark()
{
    // :hP# latches the parked state in the mount and only :Spu# releases it.
    if (hasMountPark())
    {
        char response[DRIVER_LEN] = {0};
        if (!sendStatusCommand(":Spu#", response))
        {
            LOG_ERROR("Failed to unpark mount.");
            return false;
        }

        if (response[0] != '1')
        {
            // A mount that is not latched as parked (e.g. parked by going home) refuses to unpark.
            char state = 0;
            if (!getMountParkState(state) || state != '0')
            {
                LOGF_ERROR("Mount refused to unpark (%s).", response);
                return false;
            }
        }
    }

    TrackState = SCOPE_IDLE;
    SetParked(false);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::SetCurrentPark()
{
    if (!hasCustomParkPosition())
    {
        LOGF_WARN("Setting the park position requires mount firmware 1.3.0 or later (detected %s).",
                  FirmwareTP[FIRMWARE_VERSION].getText());
        return false;
    }

    char response[DRIVER_LEN] = {0};
    if (!sendStatusCommand(":Sp01#", response))
    {
        LOG_ERROR("Failed to set park position.");
        return false;
    }

    switch (response[0])
    {
        case '1':
            LOG_INFO("Current position is stored as the mount park position.");
            break;
        case '2':
            LOG_INFO("Current position is already the mount park position.");
            break;
        case '3':
            LOG_ERROR("Mount refused the park position: only one park position can be set.");
            return false;
        case '4':
            LOG_ERROR("Mount refused the park position: only available in equatorial mode.");
            return false;
        case '5':
            LOG_ERROR("Mount refused the park position: the mount has to be homed first.");
            return false;
        case '9':
            LOG_ERROR("Mount refused the park position: the mount is moving.");
            return false;
        default:
            LOGF_ERROR("Mount refused the park position (%s).", response);
            return false;
    }

    // Park to the stored position from now on.
    CustomParkSP[0].setState(ISS_ON);
    saveConfig();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// There is no command to restore the factory park position, so default
/// parking goes back to the home position.
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::SetDefaultPark()
{
    CustomParkSP[0].setState(ISS_OFF);
    saveConfig();

    LOG_INFO("Mount parks at the home position.");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// The park slew starts from home: away from home :hC# goes first and :hP#
/// follows once the home slew is over (see updateMountPark).
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::parkToMountPosition()
{
    char status[DRIVER_LEN] = {0};
    if (!sendCommand(":GU#", status))
        return false;

    if (strchr(status, 'Z'))
    {
        LOG_ERROR("Parking to the mount park position is only available in equatorial mode.");
        return false;
    }

    if (!SetTrackEnabled(false))
    {
        LOG_ERROR("Failed to stop tracking before parking.");
        return false;
    }

    m_ParkMoved = false;
    m_ParkDeadline = std::chrono::steady_clock::now() + PARK_START_TIMEOUT;

    if (strchr(status, 'H'))
    {
        LOG_INFO("Parking to the mount park position...");
        m_ParkStage = ParkStage::Parking;
        if (sendCommand(":hP#"))
            return true;
    }
    else
    {
        LOG_INFO("Going home before parking to the mount park position...");
        m_ParkStage = ParkStage::Homing;
        if (goHome())
            return true;
    }

    m_ParkStage = ParkStage::None;
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// :Gps# 0: not parked, 1: parking, 2: parked, 3: park error
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::getMountParkState(char &state)
{
    char response[DRIVER_LEN] = {0};
    if (!sendCommand(":Gps#", response))
        return false;

    state = response[0];
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void LX200AM5::checkMountParkState()
{
    m_ParkStage = ParkStage::None;

    char state = 0;
    if (!hasMountPark() || !getMountParkState(state))
        return;

    if (state == '2' && !isParked())
    {
        LOG_INFO("Mount reports it is parked.");
        SetParked(true);
    }
    else if (state == '1')
    {
        LOG_INFO("Mount is parking...");
        TrackState = SCOPE_PARKING;
        m_ParkStage = ParkStage::Parking;
        m_ParkMoved = true;
        ParkSP.setState(IPS_BUSY);
        ParkSP.apply();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void LX200AM5::updateMountPark(bool slewComplete, bool isHome)
{
    const bool slewing = !slewComplete;
    const bool timedOut = std::chrono::steady_clock::now() > m_ParkDeadline;

    if (m_ParkStage == ParkStage::Homing)
    {
        if (slewing)
            m_ParkMoved = true;

        // Older firmware does not report H at the end of the home slew, so wait for the mount to stop instead.
        bool homeReached = isHome && (m_FirmwareVersion >= FIRMWARE_HOME_FLAG || slewComplete);
        if (homeReached || (m_FirmwareVersion < FIRMWARE_HOME_FLAG && m_ParkMoved && slewComplete))
        {
            LOG_INFO("Arrived at home. Parking to the mount park position...");
            m_ParkStage = ParkStage::Parking;
            m_ParkMoved = false;
            m_ParkDeadline = std::chrono::steady_clock::now() + PARK_START_TIMEOUT;
            if (!sendCommand(":hP#"))
                failMountPark("failed to send park command");
        }
        else if (!m_ParkMoved && timedOut)
            failMountPark("mount did not start moving home");

        return;
    }

    char state = 0;
    // Try again on next poll.
    if (!getMountParkState(state))
        return;

    switch (state)
    {
        case '2':
            m_ParkStage = ParkStage::None;
            SetParked(true);
            break;

        case '1':
            m_ParkMoved = true;
            break;

        case '0':
            if (slewing)
                m_ParkMoved = true;
            else if (!m_ParkMoved && timedOut)
                failMountPark("mount did not start moving to the park position");
            break;

        default:
            failMountPark("mount reported a park error");
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void LX200AM5::failMountPark(const char *reason)
{
    LOGF_ERROR("Park failed: %s.", reason);
    m_ParkStage = ParkStage::None;
    TrackState = SCOPE_IDLE;
    ParkSP.reset();
    ParkSP[isParked() ? PARK : UNPARK].setState(ISS_ON);
    ParkSP.setState(IPS_ALERT);
    ParkSP.apply();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    bool slewComplete = false, isHome = false;
    char status[DRIVER_LEN] = {0};
    if (sendCommand(":GU#", status))
    {
        slewComplete = strchr(status, 'N');
        isHome = strchr(status, 'H');
    }

    if (HomeSP.getState() == IPS_BUSY && isHome)
    {
        HomeSP.reset();
        HomeSP.setState(IPS_OK);
        LOG_INFO("Arrived at home.");
        HomeSP.apply();
        TrackState = SCOPE_IDLE;
    }
    else if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if (slewComplete)
        {
            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (m_ParkStage != ParkStage::None)
            updateMountPark(slewComplete, isHome);
        else if (slewComplete)
            SetParked(true);
    }
    else
    {
        // Tracking changed?
        auto wasTracking = TrackStateSP[INDI_ENABLED].getState() == ISS_ON;
        auto nowTracking = isTracking();
        if (wasTracking != nowTracking)
            TrackState = nowTracking ? SCOPE_TRACKING : SCOPE_IDLE;
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.setState(IPS_ALERT);
        LOG_ERROR("Error reading RA/DEC.");
        EqNP.apply();
        return false;
    }

    if (HasPierSide())
    {
        char response[DRIVER_LEN] = {0};
        if (sendCommand(":Gm#", response))
        {
            if (response[0] == 'W')
                setPierSide(PIER_WEST);
            else if (response[0] == 'E')
                setPierSide(PIER_EAST);
            else
                setPierSide(PIER_UNKNOWN);
        }
    }

    NewRaDec(currentRA, currentDEC);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void LX200AM5::getFirmwareInfo()
{
    m_FirmwareVersion = 0;

    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GVP#", response))
    {
        char *term = strchr(response, DRIVER_STOP_CHAR);
        if (term)
            *term = '\0';
        FirmwareTP[FIRMWARE_MODEL].setText(response);
    }

    memset(response, 0, sizeof(response));
    if (sendCommand(":GV#", response))
    {
        char *term = strchr(response, DRIVER_STOP_CHAR);
        if (term)
            *term = '\0';
        FirmwareTP[FIRMWARE_VERSION].setText(response);

        int major = 0, minor = 0, patch = 0;
        if (sscanf(response, "%d.%d.%d", &major, &minor, &patch) == 3)
            m_FirmwareVersion = (major << 16) | (minor << 8) | patch;
        else
            LOGF_WARN("Unexpected firmware version format: %s", response);
    }

    FirmwareTP.setState(m_FirmwareVersion > 0 ? IPS_OK : IPS_ALERT);

    LOGF_INFO("Mount %s firmware %s.", FirmwareTP[FIRMWARE_MODEL].getText(), FirmwareTP[FIRMWARE_VERSION].getText());
    if (!hasCustomParkPosition())
        LOG_INFO("Mount parks at the home position. Custom park position requires firmware 1.3.0 or later.");
    else if (CustomParkSP[0].getState() == ISS_ON)
        LOG_INFO("Mount parks at the custom park position.");
    else
        LOG_INFO("Mount parks at the home position. To use a custom park position, slew to it and set it as current park position.");
}

/////////////////////////////////////////////////////////////////////////////
/// Send Status Command
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::sendStatusCommand(const char * cmd, char * res)
{
    int nbytes_written = 0, nbytes_read = 0;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);
    int rc = tty_write_string(PortFD, cmd, &nbytes_written);
    if (rc == TTY_OK)
        rc = tty_read(PortFD, res, 1, DRIVER_TIMEOUT, &nbytes_read);
    // Errors are reported as e<code>#
    if (rc == TTY_OK && res[0] == 'e')
        rc = tty_nread_section(PortFD, res + 1, DRIVER_LEN - 2, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial error: %s.", errstr);
        return false;
    }

    char *term = strchr(res, DRIVER_STOP_CHAR);
    if (term)
        *term = '\0';
    else if (res[0] != 'e')
        res[1] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
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
IPState LX200AM5::ExecuteHomeAction(TelescopeHomeAction action)
{
    switch (action)
    {
        case HOME_GO:
            if (goHome())
                return IPS_BUSY;
            else
                return IPS_ALERT;

        case HOME_SET:
            // The ZWO AM5 protocol does not have a specific "set home" command.
            // We can potentially use the current position as home, but for now,
            // we'll mark it as unsupported.
            LOG_WARN("Setting home position is not supported by the ZWO AM5 protocol.");
            return IPS_ALERT;

        case HOME_FIND:
            // The ZWO AM5 protocol does not have a specific "find home" command.
            LOG_WARN("Finding home position is not supported by the ZWO AM5 protocol.");
            return IPS_ALERT;

        default:
            return IPS_ALERT;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void LX200AM5::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> LX200AM5::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool LX200AM5::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    MountTypeSP.save(fp);
    GuideRateNP.save(fp);
    BuzzerSP.save(fp);
    HeavyDutyModeSP.save(fp);
    MeridianFlipSP.save(fp);
    PostMeridianTrackSP.save(fp);
    MeridianLimitNP.save(fp);
    AltitudeLimitSP.save(fp);
    AltitudeLimitUpperNP.save(fp);
    AltitudeLimitLowerNP.save(fp);
    VariableSlewRateNP.save(fp);
    CustomParkSP.save(fp);

    return true;
}
