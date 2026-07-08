#include "toup_sh20.h"
#include "connectionplugins/connectiontcp.h"
#include "lx200driver.h"
#include "indicom.h"

#include <libnova/transform.h>
#include <cmath>
#include <cstring>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

ToupSH20::ToupSH20()
{
    setVersion(1, 0);

    setLX200Capability(LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(
        TELESCOPE_CAN_SYNC |
        TELESCOPE_CAN_GOTO |
        TELESCOPE_CAN_ABORT |
        TELESCOPE_HAS_TIME |
        TELESCOPE_HAS_LOCATION |
        TELESCOPE_CAN_PARK |
        TELESCOPE_CAN_HOME_GO |
        TELESCOPE_CAN_HOME_FIND |
        TELESCOPE_HAS_TRACK_MODE |
        TELESCOPE_CAN_CONTROL_TRACK,
        SLEW_RATE_COUNT);
}

bool ToupSH20::initProperties()
{
    LX200Generic::initProperties();

    SetParkDataType(PARK_AZ_ALT);
    timeFormat = LX200_24;

    // Mount Type
    MountTypeSP[TYPE_AZIMUTH].fill("Azimuth", "Azimuth", ISS_OFF);
    MountTypeSP[TYPE_EQUATORIAL].fill("Equatorial", "Equatorial", ISS_ON);
    MountTypeSP.fill(getDeviceName(), "MOUNT_TYPE", "Mount Type", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    MountTypeSP.load();
    if (MountTypeSP[TYPE_EQUATORIAL].getState() == ISS_ON)
        SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_HAS_PIER_SIDE, SLEW_RATE_COUNT);

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
    SlewRateSP[9].setState(ISS_ON);

    // Guide Rate
    GuideRateNP[0].fill("RATE", "Rate", "%.2f", 0.1, 0.9, 0.1, 0.5);
    GuideRateNP.fill(getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 60, IPS_IDLE);
    GuideRateNP.load();

    // Buzzer
    BuzzerSP[BUZZER_OFF].fill("OFF", "Off", ISS_OFF);
    BuzzerSP[BUZZER_LOW].fill("LOW", "Low", ISS_OFF);
    BuzzerSP[BUZZER_HIGH].fill("HIGH", "High", ISS_ON);
    BuzzerSP.fill(getDeviceName(), "BUZZER", "Buzzer", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    BuzzerSP.onUpdate([this]
    {
        if (setBuzzer(BuzzerSP.findOnSwitchIndex()))
        {
            BuzzerSP.setState(IPState::IPS_OK);
        }
        else
        {
            BuzzerSP.setState(IPState::IPS_ALERT);
        }
        BuzzerSP.apply();
    });

    // Meridian Flip Enable
    MeridianFlipSP[INDI_ENABLED].fill("INDI_ENABLED", "Enabled", ISS_ON);
    MeridianFlipSP[INDI_DISABLED].fill("INDI_DISABLED", "Disabled", ISS_OFF);
    MeridianFlipSP.fill(getDeviceName(), "MERIDIAN_FLIP", "Meridian Flip", "Meridian Flip", IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    MeridianFlipSP.load();

    // Meridian Flip Track
    MeridianTrackSP[MERIDIAN_TRACK].fill("TRACK", "Track", ISS_ON);
    MeridianTrackSP[MERIDIAN_STOP].fill("STOP", "Stop", ISS_OFF);
    MeridianTrackSP.fill(getDeviceName(), "MERIDIAN_TRACK", "After Meridian", "Meridian Flip", IP_RW, ISR_1OFMANY,
                             60, IPS_IDLE);
    MeridianTrackSP.load();

    // Meridian Flip Limit
    MeridianLimitNP[0].fill("LIMIT", "LIMIT (deg)", "%.f", -15, 15, 1, 0);
    MeridianLimitNP.fill(getDeviceName(), "MERIDIAN_LIMIT", "Meridian Limit", "Meridian Flip", IP_RW, 60, IPS_IDLE);
    MeridianLimitNP.load();

    return true;
}

bool ToupSH20::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        deleteProperty("TELESCOPE_MOUNT_TYPE");

        getStartupData();

        defineProperty(MountTypeSP);
        defineProperty(GuideRateNP);
        defineProperty(BuzzerSP);
        if (MountTypeSP[TYPE_EQUATORIAL].getState() == ISS_ON)
        {
            defineProperty(MeridianFlipSP);
            defineProperty(MeridianTrackSP);
            defineProperty(MeridianLimitNP);
        }
    }
    else
    {
        deleteProperty(MountTypeSP);
        deleteProperty(GuideRateNP);
        deleteProperty(BuzzerSP);
        if (MountTypeSP[TYPE_EQUATORIAL].getState() == ISS_ON)
        {
            deleteProperty(MeridianFlipSP);
            deleteProperty(MeridianTrackSP);
            deleteProperty(MeridianLimitNP);
        }
    }

    return true;
}

bool ToupSH20::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Mount Type
        if (MountTypeSP.isNameMatch(name))
        {
            int previousType = MountTypeSP.findOnSwitchIndex();
            MountTypeSP.update(states, names, n);
            IPState state = IPS_OK;
            // Only update if already connected.
            if (isConnected())
            {
                auto targetType = MountTypeSP.findOnSwitchIndex();
                state = setMountType(targetType) ? IPS_OK : IPS_ALERT;
                if (state == IPS_OK && previousType != targetType)
                    LOG_WARN("You must reconnect mount for change to take effect.");
            }
            MountTypeSP.setState(state);
            MountTypeSP.apply();
            saveConfig(MountTypeSP);
            return true;
        }

        // Meridian Flip Enable
        if (MeridianFlipSP.isNameMatch(name))
        {
            MeridianFlipSP.update(states, names, n);
            auto rc = setMeridianFlipSettings(MeridianFlipSP[INDI_ENABLED].getState() == ISS_ON,
                                              MeridianTrackSP[MERIDIAN_TRACK].getState() == ISS_ON,
                                              MeridianLimitNP[0].getValue());
            MeridianFlipSP.setState(rc ? IPS_OK : IPS_ALERT);
            MeridianFlipSP.apply();
            saveConfig(MeridianFlipSP);
            return true;
        }

        // Meridian Flip Track
        if (MeridianTrackSP.isNameMatch(name))
        {
            MeridianTrackSP.update(states, names, n);
            auto rc = setMeridianFlipSettings(MeridianFlipSP[INDI_ENABLED].getState() == ISS_ON,
                                              MeridianTrackSP[MERIDIAN_TRACK].getState() == ISS_ON,
                                              MeridianLimitNP[0].getValue());
            MeridianTrackSP.setState(rc ? IPS_OK : IPS_ALERT);
            MeridianTrackSP.apply();
            saveConfig(MeridianTrackSP);
            return true;
        }
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool ToupSH20::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Guide Rate
        if (GuideRateNP.isNameMatch(name))
        {
            GuideRateNP.update(values, names, n);
            GuideRateNP.setState(setGuideRate(GuideRateNP[0].getValue()) ? IPS_OK : IPS_ALERT);
            GuideRateNP.apply();
            return true;
        }

        // Meridian FLip Limit
        if (MeridianLimitNP.isNameMatch(name))
        {
            MeridianLimitNP.update(values, names, n);
            auto rc = setMeridianFlipSettings(MeridianFlipSP[INDI_ENABLED].getState() == ISS_ON,
                                              MeridianTrackSP[MERIDIAN_TRACK].getState() == ISS_ON,
                                              MeridianLimitNP[0].getValue());
            MeridianLimitNP.setState(rc ? IPS_OK : IPS_ALERT);
            MeridianLimitNP.apply();
            saveConfig(MeridianLimitNP);
            return true;
        }
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

const char* ToupSH20::getDefaultName()
{
    return "TOUPTEK SH20";
}

bool ToupSH20::checkConnection()
{
    LOG_DEBUG("Checking SH20 connection...");

    double targetRA = 0;
    for (int i = 0; i < 2; i++)
    {
        if (getLX200RA(PortFD, &targetRA) == 0)
            return true;

        const struct timespec timeout = {0, 250000000L};
        nanosleep(&timeout, NULL);
    }

    return false;
}


bool ToupSH20::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    bool isNotMove = true, isAtHome = false;

    char status[DRIVER_LEN] = {0};
    if (sendCommand(":GU#", status))
    {
        isNotMove = strchr(status, 'N');
        isAtHome = strchr(status, 'H');
    }

    if (HomeSP.getState() == IPS_BUSY && isAtHome)
    {
        HomeSP.reset();
        HomeSP.setState(IPS_OK);
        LOG_INFO("At home position now.");
        HomeSP.apply();
        TrackState = SCOPE_IDLE;
    }
    else if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if (isNotMove)
        {
            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (isNotMove)
        {
            SetParked(true);
        }
    }
    else
    {
        char response[DRIVER_LEN] = {0};
        if (sendCommand(":GTS#", response))
        {
            auto wasTracking = TrackStateSP[INDI_ENABLED].getState() == ISS_ON;
            auto nowTracking = response[0] == '1';
            if (wasTracking != nowTracking)
                TrackState = nowTracking ? SCOPE_TRACKING : SCOPE_IDLE;
        }
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
            if (response[0] == '1')
                setPierSide(PIER_WEST);
            else if (response[0] == '0')
                setPierSide(PIER_EAST);
            else
                setPierSide(PIER_UNKNOWN);
        }
    }

    NewRaDec(currentRA, currentDEC);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Override - Location
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    int d = 0, m = 0, s = 0;

    // JM: INDI Longitude is 0 to 360 increasing EAST. Standard East is Positive, West is negative
    if (longitude > 180)
        longitude = longitude - 360;

    char command[DRIVER_LEN] = {0};
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
/// Override - Date
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::setUTCOffset(double offset)
{
    int h = 0, m = 0, s = 0;
    char command[DRIVER_LEN] = {0};
    getSexComponents(offset, &h, &m, &s);

    snprintf(command, DRIVER_LEN, ":SG%c%02d:%02d#", offset >= 0 ? '+' : '-', std::abs(h), m);
    return setStandardProcedure(PortFD, command) == 0;
}

bool ToupSH20::setLocalDate(uint8_t days, uint8_t months, uint16_t years)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":SC%02d/%02d/%02d#", months, days, years % 100);
    return setStandardProcedure(PortFD, command) == 0;
}

/////////////////////////////////////////////////////////////////////////////
/// Override - Goto
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::Goto(double ra, double dec)
{
    const struct timespec timeout = {0, 100000000L};

    targetRA  = ra;
    targetDEC = dec;
    char RAStr[64] = {0}, DecStr[64] = {0};
    int fracbase = 0;

    switch (getLX200EquatorialFormat())
    {
        case LX200_EQ_LONGER_FORMAT:
            fracbase = 360000;
            break;
        case LX200_EQ_LONG_FORMAT:
        case LX200_EQ_SHORT_FORMAT:
        default:
            fracbase = 3600;
            break;
    }

    fs_sexa(RAStr, targetRA, 2, fracbase);
    fs_sexa(DecStr, targetDEC, 2, fracbase);

    // If moving, let's stop it first.
    if (EqNP.getState() == IPS_BUSY)
    {
        if (!isSimulation() && abortSlew(PortFD) < 0)
        {
            AbortSP.setState(IPS_ALERT);
            LOG_ERROR("Abort slew failed.");
            AbortSP.apply();
            return false;
        }

        AbortSP.setState(IPS_OK);
        EqNP.setState(IPS_IDLE);
        LOG_ERROR("Slew aborted.");
        AbortSP.apply();
        EqNP.apply();

        if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
        {
            MovementNSSP.setState(IPS_IDLE);
            MovementWESP.setState(IPS_IDLE);
            EqNP.setState(IPS_IDLE);
            MovementNSSP.reset();
            MovementWESP.reset();
            MovementNSSP.apply();
            MovementWESP.apply();
        }

        // sleep for 100 mseconds
        nanosleep(&timeout, nullptr);
    }

    if (!isSimulation())
    {
        if (setObjectRA(PortFD, targetRA) < 0 || (setObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Error setting RA/DEC.");
            EqNP.apply();
            return false;
        }

        int err = Slew(PortFD);

        // for sh20 0:failed, 1:success
        if (err == 0)
        {
            LOGF_ERROR("Error Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
            slewError(err);
            return false;
        }
    }

    TrackState = SCOPE_SLEWING;
    //EqNP.s     = IPS_BUSY;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Override - Slew rate
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::SetSlewRate(int index)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":R%d#", index);
    return sendCommand(command);
}

/////////////////////////////////////////////////////////////////////////////
/// Override - Track enable
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::SetTrackEnabled(bool enabled)
{
    char response[DRIVER_LEN] = {0};
    bool rc = sendCommand(enabled ? ":Te#" : ":Td#", response, 4, 1);
    return rc && response[0] == '1';
}

/////////////////////////////////////////////////////////////////////////////
/// Override - Park
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::SetCurrentPark()
{
    char response[DRIVER_LEN] = {0};
    bool rc = sendCommand(":SPc#", response, 5, 1);
    return rc && response[0] == '1';
}

bool ToupSH20::Park()
{
    if (!sendCommand(":MP1#"))
        return false;
    TrackState = SCOPE_PARKING;
    return true;
}

bool ToupSH20::UnPark()
{
    if (!sendCommand(":MP0#"))
        return false;
    TrackState = SCOPE_IDLE;
    SetParked(false);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Override - Home
/////////////////////////////////////////////////////////////////////////////
IPState ToupSH20::ExecuteHomeAction(TelescopeHomeAction action)
{
    switch (action)
    {
        case HOME_GO:
            if (goHome())
                return IPS_BUSY;
            else
                return IPS_ALERT;

        case HOME_FIND:
            if (findHome())
                return IPS_OK;
            else
                return IPS_ALERT;

        default:
            return IPS_ALERT;

    }
}

/////////////////////////////////////////////////////////////////////////////
/// Send command
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
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

void ToupSH20::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

/////////////////////////////////////////////////////////////////////////////
/// Startup
/////////////////////////////////////////////////////////////////////////////
void ToupSH20::getStartupData()
{
    InitPark();
    getMountType();
    getTrackMode();
    getGuideRate();
    getBuzzer();
    if (MountTypeSP[TYPE_EQUATORIAL].getState() == ISS_ON)
        getMeridianFlipSettings();
}

/////////////////////////////////////////////////////////////////////////////
/// Mount Type
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::setMountType(int type)
{
    return sendCommand((type == TYPE_EQUATORIAL) ? ":AP#" : ":AA#");
}

bool ToupSH20::getMountType()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GU#", response))
    {
        MountTypeSP.reset();
        MountTypeSP[TYPE_AZIMUTH].setState(strchr(response, 'Z') ? ISS_ON : ISS_OFF);
        MountTypeSP[TYPE_EQUATORIAL].setState(strchr(response, 'G') ? ISS_ON : ISS_OFF);
        MountTypeSP.setState(IPS_OK);
        return true;
    }
    else
    {
        MountTypeSP.setState(IPS_ALERT);
        return false;
    }
}

/////////////////////////////////////////////////////////////////////////////
/// Home
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::goHome()
{
    return sendCommand(":MH#");
}

bool ToupSH20::findHome()
{
    const char cmd[] = ":MFH#";
    char status = '0';
    return sendCommand(cmd, &status, strlen(cmd), sizeof(status)) && status == '1';
}

/////////////////////////////////////////////////////////////////////////////
/// Track mode
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::getTrackMode()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GT#", response))
    {
        TrackModeSP.reset();
        auto onIndex = response[0];
        TrackModeSP[onIndex].setState(ISS_ON);
        return true;
    }

    TrackModeSP.setState(IPS_ALERT);
    return false;

}

/////////////////////////////////////////////////////////////////////////////
/// Guide Rate
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::setGuideRate(double value)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":Sgr%.2f#", value);
    return sendCommand(command);
}

bool ToupSH20::getGuideRate()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":Ggr#", response))
    {
        double value = 0;
        if (sscanf(response, "%lf", &value) == 1)
        {
            GuideRateNP[0].setValue(value);
            return true;
        }
    }
    GuideRateNP.setState(IPS_ALERT);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Buzzer
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::setBuzzer(int value)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":SBz%d#", value);
    return sendCommand(command);
}

bool ToupSH20::getBuzzer()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GBz#", response))
    {
        BuzzerSP.reset();
        auto onIndex = response[0];
        BuzzerSP[onIndex].setState(ISS_ON);
        BuzzerSP.setState(IPS_OK);
        return true;
    }
    else
    {
        BuzzerSP.setState(IPS_ALERT);
        return false;
    }
}

/////////////////////////////////////////////////////////////////////////////
/// Meridian Flip
/////////////////////////////////////////////////////////////////////////////
bool ToupSH20::setMeridianFlipSettings(bool enabled, bool track, double limit)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":STa%d%d%c%02d#", enabled ? 1 : 0, track ? 1 : 0,
             limit >= 0 ? '+' : '-', static_cast<int>(std::abs(limit)));
    char response[2] = {0};
    auto rc = sendCommand(command, response, -1, 1);
    return rc && response[0] == '1';
}

bool ToupSH20::getMeridianFlipSettings()
{
    char response[DRIVER_LEN] = {0};
    if (sendCommand(":GTa#", response))
    {
        if (strlen(response) >= 5)
        {
            MeridianFlipSP.reset();
            MeridianFlipSP[response[0] == '1' ? INDI_ENABLED : INDI_DISABLED].setState(ISS_ON);
            MeridianFlipSP.setState(IPS_OK);

            MeridianTrackSP.reset();
            MeridianTrackSP[response[1] == '1' ? MERIDIAN_TRACK : MERIDIAN_STOP].setState(ISS_ON);
            MeridianTrackSP.setState(IPS_OK);

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
    MeridianTrackSP.setState(IPS_ALERT);
    MeridianLimitNP.setState(IPS_ALERT);
    return false;
}
