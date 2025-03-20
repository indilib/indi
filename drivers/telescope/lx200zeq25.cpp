/*
    ZEQ25 INDI driver

    Copyright (C) 2015 Jasem Mutlaq

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

#include "lx200zeq25.h"

#include "indicom.h"
#include "lx200driver.h"

#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <termios.h>
#include <unistd.h>

/* Simulation Parameters */
#define SLEWRATE 1        /* slew rate, degrees/s */
#define SIDRATE  0.004178 /* sidereal rate, degrees/s */

LX200ZEQ25::LX200ZEQ25()
{
    setVersion(1, 6);

    setLX200Capability(LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_HAS_PIER_SIDE |
                           TELESCOPE_CAN_HOME_GO,
                           9
                          );
}

bool LX200ZEQ25::initProperties()
{
    LX200Generic::initProperties();

    //SetParkDataType(PARK_AZ_ALT);
    SetParkDataType(PARK_NONE);

    // Slew Rates
    SlewRateSP[0].setLabel("1x");
    SlewRateSP[1].setLabel("2x");
    SlewRateSP[2].setLabel("8x");
    SlewRateSP[3].setLabel("16x");
    SlewRateSP[4].setLabel("64x");
    SlewRateSP[5].setLabel("128x");
    SlewRateSP[6].setLabel("256x");
    SlewRateSP[7].setLabel("512x");
    SlewRateSP[8].setLabel("MAX");

    SlewRateSP.reset();
    // 64x is the default
    SlewRateSP[4].setState(ISS_ON);

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[0], "GUIDE_RATE", "x Sidereal", "%g", 0.1, 1.0, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 1, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    return true;
}

bool LX200ZEQ25::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        defineProperty(&GuideRateNP);
    }
    else
    {
        deleteProperty(GuideRateNP.name);
    }

    return true;
}

const char *LX200ZEQ25::getDefaultName()
{
    return "ZEQ25";
}

bool LX200ZEQ25::checkConnection()
{
    if (isSimulation())
        return true;

    const struct timespec timeout = {0, 50000000L};
    char initCMD[] = ":V#";
    int errcode    = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    LOG_DEBUG("Initializing IOptron using :V# CMD...");

    for (int i = 0; i < 2; i++)
    {
        if ((errcode = tty_write(PortFD, initCMD, 3, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            nanosleep(&timeout, nullptr);
            continue;
        }

        if ((errcode = tty_read_section(PortFD, response, '#', 3, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            nanosleep(&timeout, nullptr);
            continue;
        }

        if (nbytes_read > 0)
        {
            response[nbytes_read] = '\0';
            LOGF_DEBUG("RES (%s)", response);

            if (!strcmp(response, "V1.00#"))
                return true;
        }

        nanosleep(&timeout, nullptr);
    }

    return false;
}

bool LX200ZEQ25::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Guiding Rate
        if (!strcmp(name, GuideRateNP.name))
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);

            if (setZEQ25GuideRate(GuideRateN[0].value) == TTY_OK)
                GuideRateNP.s = IPS_OK;
            else
                GuideRateNP.s = IPS_ALERT;

            IDSetNumber(&GuideRateNP, nullptr);

            return true;
        }
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200ZEQ25::isZEQ25Home()
{
    const struct timespec timeout = {0, 10000000L};
    char bool_return[2];
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    if (isSimulation())
        return true;

    DEBUG(DBG_SCOPE, "CMD <:AH#>");

    if ((error_type = tty_write_string(PortFD, ":AH#", &nbytes_write)) != TTY_OK)
        return false;

    error_type = tty_read(PortFD, bool_return, 1, 5, &nbytes_read);

    // JM: Hack from Jon in the INDI forums to fix longitude/latitude settings failure on ZEQ25
    nanosleep(&timeout, nullptr);
    tcflush(PortFD, TCIFLUSH);
    nanosleep(&timeout, nullptr);

    if (nbytes_read < 1)
        return false;

    DEBUGF(DBG_SCOPE, "RES <%c>", bool_return[0]);

    return (bool_return[0] == '1');
}

int LX200ZEQ25::gotoZEQ25Home()
{
    return setZEQ25StandardProcedure(PortFD, ":MH#");
}

bool LX200ZEQ25::isSlewComplete()
{
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    //strncpy(cmd, ":SE#", 16);
    const char *cmd = ":SE#";

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    if ((errcode = tty_write(PortFD, cmd, 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if ((errcode = tty_read(PortFD, response, 1, 3, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        tcflush(PortFD, TCIFLUSH);

        if (response[0] == '0')
            return true;
        else
            return false;
    }

    LOGF_ERROR("Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool LX200ZEQ25::getMountInfo()
{
    char cmd[]  = ":MountInfo#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if ((errcode = tty_read(PortFD, response, 4, 3, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        if (nbytes_read == 4)
        {
            if (!strcmp(response, "8407"))
                LOG_INFO("Detected iEQ45/iEQ30 Mount.");
            else if (!strcmp(response, "8497"))
                LOG_INFO("Detected iEQ45 AA Mount.");
            else if (!strcmp(response, "8408"))
                LOG_INFO("Detected ZEQ25 Mount.");
            else if (!strcmp(response, "8498"))
                LOG_INFO("Detected SmartEQ Mount.");
            else
                LOG_INFO("Unknown mount detected.");

            tcflush(PortFD, TCIFLUSH);

            return true;
        }
    }

    LOGF_ERROR("Only received #%d bytes, expected 4.", nbytes_read);
    return false;
}

void LX200ZEQ25::getBasicData()
{
    getMountInfo();

    int moveRate = getZEQ25MoveRate();
    if (moveRate >= 0)
    {
        SlewRateSP.reset();
        SlewRateSP[moveRate].setState(ISS_ON);
        SlewRateSP.setState(IPS_OK);
        SlewRateSP.apply();
    }

    if (InitPark())
    {
        // If loading parking data is successful, we just set the default parking values.
        SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
        SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].getValue());
    }
    else
    {
        // Otherwise, we set all parking data to default in case no parking data is found.
        SetAxis1Park(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
        SetAxis2Park(LocationNP[LOCATION_LATITUDE].getValue());
        SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
        SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].getValue());
    }

    bool isMountParked = isZEQ25Parked();
    if (isMountParked != isParked())
        SetParked(isMountParked);

    // Is home?
    LOG_DEBUG("Checking if mount is at home position...");
    if (isZEQ25Home())
    {
        HomeSP.reset();
        HomeSP.setState(IPS_OK);
        HomeSP.apply();
    }

    LOG_DEBUG("Getting guiding rate...");
    double guideRate = 0;
    if (getZEQ25GuideRate(&guideRate) == TTY_OK)
    {
        GuideRateN[0].value = guideRate;
        IDSetNumber(&GuideRateNP, nullptr);
    }

    if (sendLocationOnStartup && (GetTelescopeCapability() & TELESCOPE_HAS_LOCATION))
        sendScopeLocation();
    if (sendTimeOnStartup && (GetTelescopeCapability() & TELESCOPE_HAS_TIME))
        sendScopeTime();
}

bool LX200ZEQ25::Sync(double ra, double dec)
{
    if (!isSimulation() && (setZEQ25ObjectRA(PortFD, ra) < 0 || (setZEQ25ObjectDEC(PortFD, dec)) < 0))
    {
        EqNP.setState(IPS_ALERT);
        LOG_ERROR("Error setting RA/DEC. Unable to Sync.");
        EqNP.apply();
        return false;
    }

    if (!isSimulation() && setZEQ25StandardProcedure(PortFD, ":CM#") < 0)
    {
        EqNP.setState(IPS_ALERT);
        LOG_ERROR("Synchronization failed.");
        EqNP.apply();
        return false;
    }

    currentRA  = ra;
    currentDEC = dec;

    LOG_INFO("Synchronization successful.");

    EqNP.setState(IPS_OK);

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200ZEQ25::Goto(double r, double d)
{
    const struct timespec timeout = {0, 100000000L};

    targetRA  = r;
    targetDEC = d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

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
        if (setZEQ25ObjectRA(PortFD, targetRA) < 0 || (setZEQ25ObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Error setting RA/DEC.");
            EqNP.apply();
            return false;
        }

        if (slewZEQ25() == false)
        {
            EqNP.setState(IPS_ALERT);
            LOGF_DEBUG("Error Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
            slewError(1);
            return false;
        }
    }

    TrackState = SCOPE_SLEWING;
    //EqNP.s     = IPS_BUSY;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool LX200ZEQ25::slewZEQ25()
{
    DEBUGF(DBG_SCOPE, "<%s>", __FUNCTION__);
    char slewNum[2];
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", ":MS#");

    if ((error_type = tty_write_string(PortFD, ":MS#", &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read(PortFD, slewNum, 1, 3, &nbytes_read);

    if (nbytes_read < 1)
    {
        DEBUGF(DBG_SCOPE, "RES ERROR <%d>", error_type);
        return error_type;
    }

    /* We don't need to read the string message, just return corresponding error code */
    tcflush(PortFD, TCIFLUSH);

    DEBUGF(DBG_SCOPE, "RES <%c>", slewNum[0]);

    return (slewNum[0] == '1');
}

bool LX200ZEQ25::SetSlewRate(int index)
{
    if (isSimulation())
        return true;

    char cmd[8];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[2];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, 8, ":SR%d#", index + 1);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if ((errcode = tty_read(PortFD, response, 1, 3, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        tcflush(PortFD, TCIFLUSH);

        return (response[0] == '1');
    }

    LOGF_ERROR("Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

int LX200ZEQ25::getZEQ25MoveRate()
{
    if (isSimulation())
    {
        return SlewRateSP.findOnSwitchIndex();
    }

    char cmd[]  = ":Gr#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[3];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return -1;
    }

    if ((errcode = tty_read_section(PortFD, response, '#', 3, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return -1;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        tcflush(PortFD, TCIFLUSH);

        int moveRate = -1;

        sscanf(response, "%d", &moveRate);

        return moveRate;
    }

    LOGF_ERROR("Only received #%d bytes, expected 2.", nbytes_read);
    return -1;
}

bool LX200ZEQ25::updateTime(ln_date *utc, double utc_offset)
{
    struct ln_zonedate ltm;

    if (isSimulation())
        return true;

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600.0);

    JD = ln_get_julian_day(utc);

    LOGF_DEBUG("New JD is %.2f", JD);

    // Set Local Time
    if (setLocalTime(PortFD, ltm.hours, ltm.minutes, ltm.seconds, true) < 0)
    {
        LOG_ERROR("Error setting local time.");
        return false;
    }

    if (setZEQ25Date(ltm.days, ltm.months, ltm.years) < 0)
    {
        LOG_ERROR("Error setting local date.");
        return false;
    }

    if (setZEQ25UTCOffset(utc_offset) < 0)
    {
        LOG_ERROR("Error setting UTC Offset.");
        return false;
    }

    return true;
}

bool LX200ZEQ25::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    double final_longitude;

    if (longitude > 180)
        final_longitude = longitude - 360.0;
    else
        final_longitude = longitude;

    if (!isSimulation() && setZEQ25Longitude(final_longitude) < 0)
    {
        LOG_ERROR("Error setting site longitude coordinates");
        return false;
    }

    if (!isSimulation() && setZEQ25Latitude(latitude) < 0)
    {
        LOG_ERROR("Error setting site latitude coordinates");
        return false;
    }

    char l[32], L[32];
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

int LX200ZEQ25::setZEQ25Longitude(double Long)
{
    int d, m, s;
    char sign;
    char temp_string[32];

    if (Long > 0)
        sign = '+';
    else
        sign = '-';

    getSexComponents(Long, &d, &m, &s);

    snprintf(temp_string, sizeof(temp_string), ":Sg %c%03d*%02d:%02d#", sign, abs(d), m, s);

    return (setZEQ25StandardProcedure(PortFD, temp_string));
}

int LX200ZEQ25::setZEQ25Latitude(double Lat)
{
    int d, m, s;
    char sign;
    char temp_string[32];

    if (Lat > 0)
        sign = '+';
    else
        sign = '-';

    getSexComponents(Lat, &d, &m, &s);

    snprintf(temp_string, sizeof(temp_string), ":St %c%02d*%02d:%02d#", sign, abs(d), m, s);

    return (setZEQ25StandardProcedure(PortFD, temp_string));
}

int LX200ZEQ25::setZEQ25UTCOffset(double hours)
{
    char temp_string[16];
    char sign;
    int h = 0, m = 0, s = 0;

    if (hours > 0)
        sign = '+';
    else
        sign = '-';

    getSexComponents(hours, &h, &m, &s);

    snprintf(temp_string, sizeof(temp_string), ":SG %c%02d:%02d#", sign, abs(h), m);

    return (setZEQ25StandardProcedure(PortFD, temp_string));
}

int LX200ZEQ25::setZEQ25Date(int days, int months, int years)
{
    char command[16] = {0};
    snprintf(command, sizeof(command), ":SC %02d/%02d/%02d#", months, days, years % 100);
    return (setZEQ25StandardProcedure(PortFD, command));
}

int LX200ZEQ25::setZEQ25ObjectRA(int fd, double ra)
{
    int h, m, s;
    char cmd[16] = {0};

    getSexComponents(ra, &h, &m, &s);
    snprintf(cmd, sizeof(cmd), ":Sr %02d:%02d:%02d#", h, m, s);

    return (setStandardProcedure(fd, cmd));
}

int LX200ZEQ25::setZEQ25ObjectDEC(int fd, double dec)
{
    char cmd[16] = {0};
    int d, m, s;
    
    getSexComponents(dec, &d, &m, &s);
    
    // case negative zero
    if (!d && dec < 0)
        snprintf(cmd, sizeof(cmd), ":Sd -%02d*%02d:%02d#", d, m,s);
    else
        snprintf(cmd, sizeof(cmd), ":Sd %+03d*%02d:%02d#", d, m, s);

    return (setStandardProcedure(fd, cmd));
}

int LX200ZEQ25::setZEQ25StandardProcedure(int fd, const char *data)
{
    const struct timespec timeout = {0, 10000000L};
    char bool_return[2];
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", data);

    if ((error_type = tty_write_string(fd, data, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read(fd, bool_return, 1, 5, &nbytes_read);

    // JM: Hack from Jon in the INDI forums to fix longitude/latitude settings failure on ZEQ25
    nanosleep(&timeout, nullptr);
    tcflush(fd, TCIFLUSH);
    nanosleep(&timeout, nullptr);

    if (nbytes_read < 1)
        return error_type;

    DEBUGF(DBG_SCOPE, "RES <%c>", bool_return[0]);

    if (bool_return[0] == '0')
    {
        DEBUGF(DBG_SCOPE, "CMD <%s> failed.", data);
        return -1;
    }

    DEBUGF(DBG_SCOPE, "CMD <%s> successful.", data);

    return 0;
}

bool LX200ZEQ25::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    int current_move = (dir == DIRECTION_NORTH) ? LX200_NORTH : LX200_SOUTH;

    switch (command)
    {
        case MOTION_START:
            if (!isSimulation() && moveZEQ25To(current_move) < 0)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.",
                          (current_move == LX200_NORTH) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (!isSimulation() && haltZEQ25Movement() < 0)
            {
                LOG_ERROR("Error stopping N/S motion.");
                return false;
            }
            else
                LOGF_INFO("Movement toward %s halted.",
                          (current_move == LX200_NORTH) ? "North" : "South");
            break;
    }

    return true;
}

bool LX200ZEQ25::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    int current_move = (dir == DIRECTION_WEST) ? LX200_WEST : LX200_EAST;

    switch (command)
    {
        case MOTION_START:
            if (!isSimulation() && moveZEQ25To(current_move) < 0)
            {
                LOG_ERROR("Error setting W/E motion direction.");
                return false;
            }
            else
                LOGF_INFO("Moving toward %s.", (current_move == LX200_WEST) ? "West" : "East");
            break;

        case MOTION_STOP:
            if (!isSimulation() && haltZEQ25Movement() < 0)
            {
                LOG_ERROR("Error stopping W/E motion.");
                return false;
            }
            else
                LOGF_INFO("Movement toward %s halted.",
                          (current_move == LX200_WEST) ? "West" : "East");
            break;
    }

    return true;
}

int LX200ZEQ25::moveZEQ25To(int direction)
{
    DEBUGF(DBG_SCOPE, "<%s>", __FUNCTION__);
    int nbytes_write = 0;

    switch (direction)
    {
        case LX200_NORTH:
            DEBUGF(DBG_SCOPE, "CMD <%s>", ":mn#");
            tty_write_string(PortFD, ":mn#", &nbytes_write);
            break;
        case LX200_WEST:
            DEBUGF(DBG_SCOPE, "CMD <%s>", ":mw#");
            tty_write_string(PortFD, ":mw#", &nbytes_write);
            break;
        case LX200_EAST:
            DEBUGF(DBG_SCOPE, "CMD <%s>", ":me#");
            tty_write_string(PortFD, ":me#", &nbytes_write);
            break;
        case LX200_SOUTH:
            DEBUGF(DBG_SCOPE, "CMD <%s>", ":ms#");
            tty_write_string(PortFD, ":ms#", &nbytes_write);
            break;
        default:
            break;
    }

    tcflush(PortFD, TCIFLUSH);
    return 0;
}

int LX200ZEQ25::haltZEQ25Movement()
{
    DEBUGF(DBG_SCOPE, "<%s>", __FUNCTION__);
    int error_type;
    int nbytes_write = 0;

    if ((error_type = tty_write_string(PortFD, ":q#", &nbytes_write)) != TTY_OK)
        return error_type;

    tcflush(PortFD, TCIFLUSH);
    return 0;
}

bool LX200ZEQ25::SetTrackMode(uint8_t mode)
{
    return (setZEQ25TrackMode(mode) == 0);
}

int LX200ZEQ25::setZEQ25TrackMode(int mode)
{
    DEBUGF(DBG_SCOPE, "<%s>", __FUNCTION__);

    // We don't support KING mode :RT3, so we turn mode=3 to custom :RT4#
    if (mode == 3)
        mode = 4;

    char cmd[6];
    snprintf(cmd, 6, ":RT%d#", mode);

    return setZEQ25StandardProcedure(PortFD, cmd);
}

int LX200ZEQ25::setZEQ25Park()
{
    int error_type;
    int nbytes_write = 0;

    LOGF_DEBUG("CMD <%s>", ":MP1#");

    if ((error_type = tty_write_string(PortFD, ":MP1#", &nbytes_write)) != TTY_OK)
        return error_type;

    tcflush(PortFD, TCIFLUSH);
    return 0;
}

int LX200ZEQ25::setZEQ25UnPark()
{
    int error_type;
    int nbytes_write = 0;

    LOGF_DEBUG("CMD <%s>", ":MP0#");

    if ((error_type = tty_write_string(PortFD, ":MP0#", &nbytes_write)) != TTY_OK)
        return error_type;

    tcflush(PortFD, TCIFLUSH);
    return 0;
}

bool LX200ZEQ25::isZEQ25Parked()
{
    if (isSimulation())
    {
        return isParked();
    }

    char cmd[]  = ":AP#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[2];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if ((errcode = tty_read(PortFD, response, 1, 3, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        tcflush(PortFD, TCIFLUSH);

        return (response[0] == '1');
    }

    LOGF_ERROR("Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool LX200ZEQ25::SetCurrentPark()
{
    INDI::IHorizontalCoordinates horizontalPos;
    INDI::IEquatorialCoordinates equatorialPos;
    equatorialPos.rightascension  = currentRA;
    equatorialPos.declination = currentDEC;
    INDI::EquatorialToHorizontal(&equatorialPos, &m_Location, ln_get_julian_from_sys(), &horizontalPos);
    double parkAZ = horizontalPos.azimuth;
    double parkAlt = horizontalPos.altitude;

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr,
               AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool LX200ZEQ25::SetDefaultPark()
{
    // Az = 0 for North hemisphere
    SetAxis1Park(LocationNP[LOCATION_LATITUDE].getValue() > 0 ? 0 : 180);

    // Alt = Latitude
    SetAxis2Park(LocationNP[LOCATION_LATITUDE].getValue());

    return true;
}

bool LX200ZEQ25::Park()
{
    // JM 2012-12-16: Using Homing instead of custom parking
    // to fix reported issues with parking
    if (gotoZEQ25Home() < 0)
    {
        LOG_ERROR("Error parking...");
        return false;
    }
    else
    {
        HomeSP.setState(IPS_BUSY);
        HomeSP.apply();
    }

    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking is in progress...");
    return true;

#if 0
    double parkAz  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    LOGF_DEBUG("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    INDI::IHorizontalCoordinates horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270

    horizontalPos.alt = parkAlt;
    horizontalPos.az  = parkAz + 180;
    if (horizontalPos.az > 360)
        horizontalPos.az -= 360;

    IGeographicCoordinates observer;
    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;
    if (observer.lng > 180)
        observer.lng -= 360;

    INDI::IEquatorialCoordinates equatorialPos;
    ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);
    equatorialPos.rightascension /= 15.0;

    if (setZEQ25ObjectRA(PortFD, equatorialPos.rightascension) < 0 || (setZEQ25ObjectDEC(PortFD, equatorialPos.dec)) < 0)
    {
        LOG_ERROR("Error setting RA/Dec.");
        return false;
    }

    /* Slew reads the '0', that is not the end of the slew */
    if (slewZEQ25() == false)
    {
        LOGF_ERROR("Error Slewing to Az %s - Alt %s", AzStr, AltStr);
        slewError(1);
        return false;
    }

    EqNP.s     = IPS_BUSY;
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking is in progress...");

    return true;
#endif
}

bool LX200ZEQ25::UnPark()
{
    // First we unpark astrophysics
    if (!isSimulation())
    {
        if (setZEQ25UnPark() < 0)
        {
            LOG_ERROR("UnParking Failed.");
            return false;
        }
    }

    // Then we sync with to our last stored position
#if 0
    double parkAz  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Syncing to parked coordinates Az (%s) Alt (%s)...", AzStr, AltStr);

    INDI::IHorizontalCoordinates horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270

    horizontalPos.alt = parkAlt;
    horizontalPos.az  = parkAz + 180;
    if (horizontalPos.az > 360)
        horizontalPos.az -= 360;

    IGeographicCoordinates observer;
    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;
    if (observer.lng > 180)
        observer.lng -= 360;

    INDI::IEquatorialCoordinates equatorialPos;
    ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);
    equatorialPos.rightascension /= 15.0;

    if (setZEQ25ObjectRA(PortFD, equatorialPos.rightascension) < 0 || (setZEQ25ObjectDEC(PortFD, equatorialPos.dec)) < 0)
    {
        LOG_ERROR("Error setting RA/DEC.");
        return false;
    }

    if (Sync(equatorialPos.rightascension, equatorialPos.dec) == false)
    {
        LOG_WARN("Sync failed.");
        return false;
    }
#endif

    SetParked(false);
    return true;
}

bool LX200ZEQ25::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    if (isSimulation())
    {
        mountSim();
        return true;
    }

    //if (check_lx200_connection(PortFD))
    //return false;

    if (HomeSP.getState() == IPS_BUSY)
    {
        if (isZEQ25Home())
        {
            HomeSP.reset();
            HomeSP.setState(IPS_OK);
            LOG_INFO("Telescope arrived at home position.");
            HomeSP.apply();
        }
    }

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if (isSlewComplete())
        {
            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (HomeSP.getState() == IPS_OK)
        {
            SetParked(true);
        }
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.setState(IPS_ALERT);
        LOG_ERROR("Error reading RA/DEC.");
        EqNP.apply();
        return false;
    }

    // Get Pier side

    TelescopePierSide side = PIER_UNKNOWN;
    if (getZEQ25PierSide(side))
        setPierSide(side);

    NewRaDec(currentRA, currentDEC);

    return true;
}

void LX200ZEQ25::mountSim()
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
    da  = SLEWRATE * dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act accordingly */
    switch (TrackState)
    {
        case SCOPE_TRACKING:
            /* RA moves at sidereal, Dec stands still */
            currentRA += (SIDRATE * dt / 15.);
            break;

        case SCOPE_SLEWING:
        case SCOPE_PARKING:
            /* slewing - nail it when both within one pulse @ SLEWRATE */
            nlocked = 0;

            dx = targetRA - currentRA;

            if (fabs(dx) <= da)
            {
                currentRA = targetRA;
                nlocked++;
            }
            else if (dx > 0)
                currentRA += da / 15.;
            else
                currentRA -= da / 15.;

            dx = targetDEC - currentDEC;
            if (fabs(dx) <= da)
            {
                currentDEC = targetDEC;
                nlocked++;
            }
            else if (dx > 0)
                currentDEC += da;
            else
                currentDEC -= da;

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

    TelescopePierSide side = PIER_UNKNOWN;
    if (getZEQ25PierSide(side))
        setPierSide(side);

    NewRaDec(currentRA, currentDEC);
}

int LX200ZEQ25::getZEQ25GuideRate(double *rate)
{
    char cmd[]  = ":AG#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    LOGF_DEBUG("CMD <%s>", cmd);

    if (isSimulation())
    {
        snprintf(response, 8, "%3d#", static_cast<int>(GuideRateN[0].value * 100));
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return errcode;
        }

        if ((errcode = tty_read(PortFD, response, 4, 3, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return errcode;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        int rate_num;

        if (sscanf(response, "%d#", &rate_num) > 0)
        {
            *rate = rate_num / 100.0;
            tcflush(PortFD, TCIFLUSH);
            return TTY_OK;
        }
        else
        {
            LOGF_ERROR("Error: Malformed result (%s).", response);
            return -1;
        }
    }

    LOGF_ERROR("Only received #%d bytes, expected 1.", nbytes_read);
    return -1;
}

int LX200ZEQ25::setZEQ25GuideRate(double rate)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    int num = rate * 100;
    snprintf(cmd, 16, ":RG%03d#", num);

    LOGF_DEBUG("CMD <%s>", cmd);

    if (isSimulation())
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return errcode;
        }

        if ((errcode = tty_read(PortFD, response, 1, 3, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return errcode;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        tcflush(PortFD, TCIFLUSH);
        return TTY_OK;
    }

    LOGF_ERROR("Only received #%d bytes, expected 1.", nbytes_read);
    return -1;
}

int LX200ZEQ25::SendPulseCmd(int8_t direction, uint32_t duration_msec)
{
    int nbytes_write = 0;
    char cmd[20];
    switch (direction)
    {
        case LX200_NORTH:
            sprintf(cmd, ":Mn%05d#", duration_msec);
            break;
        case LX200_SOUTH:
            sprintf(cmd, ":Ms%05d#", duration_msec);
            break;
        case LX200_EAST:
            sprintf(cmd, ":Me%05d#", duration_msec);
            break;
        case LX200_WEST:
            sprintf(cmd, ":Mw%05d#", duration_msec);
            break;
        default:
            return 1;
    }

    LOGF_DEBUG("CMD <%s>", cmd);

    tty_write_string(PortFD, cmd, &nbytes_write);

    tcflush(PortFD, TCIFLUSH);
    return TTY_OK;
}

bool LX200ZEQ25::getZEQ25PierSide(TelescopePierSide &side)
{
    int nbytes_written = 0;
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8] = {0};
    int nbytes_read    = 0;

    if (isSimulation())
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ((errcode = tty_write_string(PortFD, ":pS#", &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(PortFD, response, 1, 3, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        LOGF_DEBUG("RES (%s)", response);

        if ( response[0] == '0')
            side = PIER_EAST;
        else if ( response[0] == '1')
            side = PIER_WEST;
        else
            side = PIER_UNKNOWN;

        return true;
    }

    return false;
}

bool LX200ZEQ25::setUTCOffset(double offset)
{
    int h, m, s;
    char command[64] = {0};
    getSexComponents(offset, &h, &m, &s);

    snprintf(command, 64, ":SG %c%02d:%02d#", offset >= 0 ? '+' : '-', h, m);
    return setZEQ25StandardProcedure(PortFD, command);
}

// Home
IPState LX200ZEQ25::ExecuteHomeAction(TelescopeHomeAction action)
{
    switch (action)
    {
        case HOME_GO:

            // If already home, nothing to be done
            //if (HomeS[0].s == ISS_ON)
            if (isZEQ25Home())
            {
                LOG_WARN("Telescope is already homed.");
                return IPS_OK;
            }

            if (gotoZEQ25Home() < 0)
            {
                LOG_ERROR("Error slewing to home position.");
                return IPS_ALERT;
            }
            else
            {
                LOG_INFO("Slewing to home position.");
                return IPS_BUSY;
            }

        default:
            return IPS_ALERT;

    }

    return IPS_ALERT;
}
