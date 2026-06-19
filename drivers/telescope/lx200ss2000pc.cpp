/*
    SkySensor2000PC
    Copyright (C) 2015 Camiel Severijns
    Copyright (C) 2025 Jasem Mutlaq

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

#include "lx200ss2000pc.h"

#include "indicom.h"
#include "lx200driver.h"

#include <cmath>
#include <string.h>

#include <libnova/transform.h>
#include <termios.h>
#include <unistd.h>

#define RB_MAX_LEN    64

const int LX200SS2000PC::ShortTimeOut = 2;  // In seconds.
const int LX200SS2000PC::LongTimeOut  = 10; // In seconds.

LX200SS2000PC::LX200SS2000PC(void) : LX200Generic()
{
    setVersion(1, 1);

    setLX200Capability(LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_CAN_PARK |
                           TELESCOPE_HAS_LOCATION, 4);
}

bool LX200SS2000PC::initProperties()
{
    LX200Generic::initProperties();

    IUFillNumber(&SlewAccuracyN[0], "SlewRA", "RA (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    IUFillNumber(&SlewAccuracyN[1], "SlewDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    IUFillNumberVector(&SlewAccuracyNP, SlewAccuracyN, NARRAY(SlewAccuracyN), getDeviceName(), "Slew Accuracy", "",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    SetParkDataType(PARK_AZ_ALT);

    return true;
}

bool LX200SS2000PC::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        defineProperty(&SlewAccuracyNP);
    }
    else
    {
        deleteProperty(SlewAccuracyNP.name);
    }

    return true;
}

bool LX200SS2000PC::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, SlewAccuracyNP.name))
        {
            if (IUUpdateNumber(&SlewAccuracyNP, values, names, n) < 0)
                return false;

            SlewAccuracyNP.s = IPS_OK;

            if (SlewAccuracyN[0].value < 3 || SlewAccuracyN[1].value < 3)
                IDSetNumber(&SlewAccuracyNP, "Warning: Setting the slew accuracy too low may result in a dead lock");

            IDSetNumber(&SlewAccuracyNP, nullptr);
            return true;
        }
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200SS2000PC::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &SlewAccuracyNP);

    return true;
}

const char *LX200SS2000PC::getDefaultName(void)
{
    return const_cast<const char *>("SkySensor2000PC");
}

bool LX200SS2000PC::updateTime(ln_date *utc, double utc_offset)
{
    bool result = true;
    // This method is largely identical to the one in the LX200Generic class.
    // The difference is that it ensures that updates that require planetary
    // data to be recomputed by the SkySensor2000PC are only done when really
    // necessary because this takes quite some time.
    if (!isSimulation())
    {
        result = false;
        struct ln_zonedate ltm;
        ln_date_to_zonedate(utc, &ltm, static_cast<long>(utc_offset * 3600.0 + 0.5));
        LOGF_DEBUG("New zonetime is %04d-%02d-%02d %02d:%02d:%06.3f (offset=%ld)", ltm.years,
                   ltm.months, ltm.days, ltm.hours, ltm.minutes, ltm.seconds, ltm.gmtoff);
        JD = ln_get_julian_day(utc);
        LOGF_DEBUG("New JD is %f", JD);
        if (setLocalTime(PortFD, ltm.hours, ltm.minutes, static_cast<int>(ltm.seconds + 0.5), true))
        {
            LOG_ERROR("Error setting local time.");
        }
        else if (!setCalenderDate(ltm.years, ltm.months, ltm.days))
        {
            LOG_ERROR("Error setting local date.");
        }
        // Meade defines UTC Offset as the offset ADDED to local time to yield UTC, which
        // is the opposite of the standard definition of UTC offset!
        else if (!setUTCOffset(-(utc_offset)))
        {
            LOG_ERROR("Error setting UTC Offset.");
        }
        else
        {
            LOG_INFO("Time updated.");
            result = true;
        }
    }
    return result;
}

void LX200SS2000PC::getBasicData(void)
{
    if (!isSimulation())
        checkLX200EquatorialFormat(PortFD);
    sendScopeLocation();
    sendScopeTime();
}

bool LX200SS2000PC::isSlewComplete()
{
    const double dx = targetRA - currentRA;
    const double dy = targetDEC - currentDEC;
    return fabs(dx) <= (SlewAccuracyN[0].value / (900.0)) && fabs(dy) <= (SlewAccuracyN[1].value / 60.0);
}

bool LX200SS2000PC::getCalendarDate(int &year, int &month, int &day)
{
    char date[RB_MAX_LEN];
    bool result = (getCommandString(PortFD, date, ":GC#") == 0);
    LOGF_DEBUG("LX200SS2000PC::getCalendarDate():: Date string from telescope: %s", date);
    if (result)
    {
        result = (sscanf(date, "%d%*c%d%*c%d", &month, &day, &year) == 3); // Meade format is MM/DD/YY
        LOGF_DEBUG("setCalenderDate: Date retrieved from telescope: %02d/%02d/%02d.", month, day,
                   year);
        if (result)
            year +=
                (year > 50 ? 1900 :
                 2000); // Year 50 or later is in the 20th century, anything less is in the 21st century.
    }
    return result;
}

bool LX200SS2000PC::setCalenderDate(int year, int month, int day)
{
    // This method differs from the setCalenderDate function in lx200driver.cpp
    // in that it reads and checks the complete response from the SkySensor2000PC.
    // In addition, this method only sends the date when it differs from the date
    // of the SkySensor2000PC because the resulting update of the planetary data
    // takes quite some time.
    bool result = true;
    int ss_year = 0, ss_month = 0, ss_day = 0;
    const bool send_to_skysensor =
        (!getCalendarDate(ss_year, ss_month, ss_day) || year != ss_year || month != ss_month || day != ss_day);
    LOGF_DEBUG("LX200SS2000PC::setCalenderDate(): Driver date %02d/%02d/%02d, SS2000PC date %02d/%02d/%02d.", month, day,
               year, ss_month, ss_day, ss_year);
    if (send_to_skysensor)
    {
        char buffer[RB_MAX_LEN];
        int nbytes_written = 0;

        snprintf(buffer, sizeof(buffer), ":SC %02d/%02d/%02d#", month, day, (year % 100));
        result = (tty_write_string(PortFD, buffer, &nbytes_written) == TTY_OK && nbytes_written == (int)strlen(buffer));
        if (result)
        {
            int nbytes_read = 0;
            result          = (tty_nread_section(PortFD, buffer, RB_MAX_LEN, '#', ShortTimeOut, &nbytes_read) == TTY_OK
                               && nbytes_read == 1 &&
                               buffer[0] == '1');
            if (result)
            {
                if (tty_nread_section(PortFD, buffer, RB_MAX_LEN, '#', ShortTimeOut, &nbytes_read) != TTY_OK ||
                        strncmp(buffer, "Updating        planetary data#", 24) != 0)
                {
                    LOGF_ERROR(
                        "LX200SS2000PC::setCalenderDate(): Received unexpected first line '%s'.", buffer);
                    result = false;
                }
                else if (tty_nread_section(PortFD, buffer, RB_MAX_LEN, '#', LongTimeOut, &nbytes_read) != TTY_OK &&
                         strncmp(buffer, "                              #", 24) != 0)
                {
                    LOGF_ERROR(
                        "LX200SS2000PC::setCalenderDate(): Received unexpected second line '%s'.", buffer);
                    result = false;
                }
            }
        }
    }
    return result;
}

bool LX200SS2000PC::setUTCOffset(double offset)
{
    bool result = true;
    int ss_timezone;
    const bool send_to_skysensor = (getUTCOffset(PortFD, &ss_timezone) != 0 || offset != ss_timezone);
    if (send_to_skysensor)
    {
        char temp_string[RB_MAX_LEN];
        snprintf(temp_string, sizeof(temp_string), ":SG %+03d#", static_cast<int>(offset));
        result = (setStandardProcedure(PortFD, temp_string) == 0);
    }
    return result;
}

bool LX200SS2000PC::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    if (latitude == 0.0 && longitude == 0.0)
        return true;

    if (setSiteLatitude(PortFD, latitude) < 0)
    {
        LOG_ERROR("Error setting site latitude coordinates");
    }

    if (setSiteLongitude(PortFD, 360.0 - longitude) < 0)
    {
        LOG_ERROR("Error setting site longitude coordinates");
        return false;
    }

    char slat[RB_MAX_LEN], slong[RB_MAX_LEN];
    fs_sexa(slat, latitude, 3, 3600);
    fs_sexa(slong, longitude, 4, 3600);

    LOGF_INFO("Site location updated to Latitude: %.32s - Longitude: %.32s", slat, slong);

    return true;
}

// This override is needed, because the Sky Sensor 2000 PC requires a space
// between the command its argument, unlike the 'standard' LX200 mounts, which
// does not work on this mount.
int LX200SS2000PC::setSiteLatitude(int fd, double Lat)
{
    int d, m, s;
    char sign;
    char temp_string[RB_MAX_LEN];

    if (Lat > 0)
        sign = '+';
    else
        sign = '-';

    getSexComponents(Lat, &d, &m, &s);

    snprintf(temp_string, sizeof(temp_string), ":St %c%03d*%02d#", sign, d, m);

    return setStandardProcedure(fd, temp_string);
}

// This override is needed, because the Sky Sensor 2000 PC requires a space
// between the command its argument, unlike the 'standard' LX200 mounts, which
// does not work on this mount.
int LX200SS2000PC::setSiteLongitude(int fd, double Long)
{
    int d, m, s;
    char temp_string[RB_MAX_LEN];

    getSexComponents(Long, &d, &m, &s);

    snprintf(temp_string, sizeof(temp_string), ":Sg %03d*%02d#", d, m);

    return setStandardProcedure(fd, temp_string);
}

bool LX200SS2000PC::Park()
{
    double parkAz  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    if (isSimulation())
    {

        INDI::IEquatorialCoordinates equatorialCoords {0, 0};
        INDI::IHorizontalCoordinates horizontalCoords {parkAz, parkAlt};
        INDI::HorizontalToEquatorial(&horizontalCoords, &m_Location, ln_get_julian_from_sys(), &equatorialCoords);
        Goto(equatorialCoords.rightascension, equatorialCoords.declination);
    }
    else
    {
        if (setObjAz(PortFD, parkAz) < 0 || setObjAlt(PortFD, parkAlt) < 0)
        {
            LOG_ERROR("Error setting Az/Alt.");
            return false;
        }

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = Slew(PortFD)))
        {
            LOGF_ERROR("Error Slewing to Az %s - Alt %s", AzStr, AltStr);
            slewError(err);
            return false;
        }
    }

    EqNP.setState(IPS_BUSY);
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking is in progress...");

    return true;
}

bool LX200SS2000PC::UnPark()
{
    // First we unpark astrophysics
    if (isSimulation() == false)
    {
        if (setAlignmentMode(PortFD, LX200_ALIGN_POLAR) < 0)
        {
            LOG_ERROR("UnParking Failed.");
            return false;
        }
    }

    // Then we sync with to our last stored position
    double parkAz  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Syncing to parked coordinates Az (%s) Alt (%s)...", AzStr, AltStr);

    if (isSimulation())
    {
        INDI::IEquatorialCoordinates equatorialCoords {0, 0};
        INDI::IHorizontalCoordinates horizontalCoords {parkAz, parkAlt};
        INDI::HorizontalToEquatorial(&horizontalCoords, &m_Location, ln_get_julian_from_sys(), &equatorialCoords);
        currentRA = equatorialCoords.rightascension;
        currentDEC = equatorialCoords.declination;
    }
    else
    {
        if (setObjAz(PortFD, parkAz) < 0 || (setObjAlt(PortFD, parkAlt)) < 0)
        {
            LOG_ERROR("Error setting Az/Alt.");
            return false;
        }

        char syncString[256];
        if (::Sync(PortFD, syncString) < 0)
        {
            LOG_WARN("Sync failed.");
            return false;
        }
    }

    SetParked(false);
    return true;
}

bool LX200SS2000PC::SetCurrentPark()
{
    INDI::IEquatorialCoordinates equatorialCoords {currentRA, currentDEC};
    INDI::IHorizontalCoordinates horizontalCoords {0, 0};
    INDI::EquatorialToHorizontal(&equatorialCoords, &m_Location, ln_get_julian_from_sys(), &horizontalCoords);
    double parkAZ = horizontalCoords.azimuth;
    double parkAlt = horizontalCoords.altitude;

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr, AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool LX200SS2000PC::SetDefaultPark()
{
    // Az = 0 for North hemisphere
    SetAxis1Park(LocationNP[LOCATION_LATITUDE].getValue() > 0 ? 0 : 180);

    // Alt = Latitude
    SetAxis2Park(LocationNP[LOCATION_LATITUDE].getValue());

    return true;
}

bool LX200SS2000PC::Goto(double ra, double dec)
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
        if (setObjectRA(PortFD, targetRA, true) < 0 || (setObjectDEC(PortFD, targetDEC, true)) < 0)
        {
            EqNP.setState(IPS_ALERT);
            LOG_ERROR("Error setting RA/DEC.");
            EqNP.apply();
            return false;
        }

        int err = 0;

        /* Slew reads the '0', that is not the end of the slew */
        if ((err = Slew(PortFD)))
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

bool LX200SS2000PC::Sync(double ra, double dec)
{
    char syncString[256] = {0};

    if (!isSimulation() && (setObjectRA(PortFD, ra, true) < 0 || (setObjectDEC(PortFD, dec, true)) < 0))
    {
        EqNP.setState(IPS_ALERT);
        LOG_ERROR("Error setting RA/DEC. Unable to Sync.");
        EqNP.apply();
        return false;
    }

    if (!isSimulation() && ::Sync(PortFD, syncString) < 0)
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

bool LX200SS2000PC::ReadScopeStatus()
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

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if (isSlewComplete())
        {
            // Set slew mode to "Centering"
            SlewRateSP.reset();
            SlewRateSP[SLEW_CENTERING].setState(ISS_ON);
            SlewRateSP.apply();

            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState == SCOPE_PARKING)
    {
        if (isSlewComplete())
        {
            SetParked(true);

            setAlignmentMode(PortFD, LX200_ALIGN_LAND);
        }
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.setState(IPS_ALERT);
        LOG_ERROR("Error reading RA/DEC.");
        EqNP.apply();
        return false;
    }

    NewRaDec(currentRA, currentDEC);

    return true;
}
