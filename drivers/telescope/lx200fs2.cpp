/*
    Astro-Electronic FS-2 Driver
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "lx200fs2.h"

#include "indicom.h"

#include <libnova/transform.h>

#include <cmath>
#include <cstring>

LX200FS2::LX200FS2() : LX200Generic()
{
    setVersion(2, 2);

    SetTelescopeCapability(
        TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_HAS_LOCATION | TELESCOPE_CAN_ABORT, 4);
}

bool LX200FS2::initProperties()
{
    LX200Generic::initProperties();

    SlewAccuracyNP[0].fill("SlewRA", "RA (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    SlewAccuracyNP[1].fill("SlewDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    SlewAccuracyNP.fill(getDeviceName(), "Slew Accuracy", "",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    StopAfterParkSP.fill(getDeviceName(), "Stop after Park", "Stop after Park", OPTIONS_TAB,
                       IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    StopAfterParkSP[0].fill("ON", "ON", ISS_OFF);
    StopAfterParkSP[1].fill("OFF", "OFF", ISS_ON);

    SetParkDataType(PARK_AZ_ALT);

    return true;
}

bool LX200FS2::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        defineProperty(&SlewRateSP);
        defineProperty(SlewAccuracyNP);
        defineProperty(StopAfterParkSP);

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].value);

            if (isParked())
            {
                // Force tracking to stop at startup.
                ParkedStatus = PARKED_NOTPARKED;
                TrackingStop();
            }
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(0);
            SetAxis2Park(LocationNP[LOCATION_LATITUDE].value);
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].value);
        }
    }
    else
    {
        deleteProperty(SlewRateSP.name);
        deleteProperty(SlewAccuracyNP.getName());
        deleteProperty(StopAfterParkSP.getName());
    }

    return true;
}

bool LX200FS2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (SlewAccuracyNP.isNameMatch(name))
        {
            if (!SlewAccuracyNP.update(values, names, n))
                return false;

            SlewAccuracyNP.setState(IPS_OK);

            if (SlewAccuracyNP[0].value < 3 || SlewAccuracyNP[1].getValue() < 3)
                SlewAccuracyNP.apply("Warning: Setting the slew accuracy too low may result in a dead lock");

            SlewAccuracyNP.apply();
            return true;
        }
    }

    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200FS2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (StopAfterParkSP.isNameMatch(name))
        {
            // Find out which state is requested by the client
            const char *actionName = IUFindOnSwitchName(states, names, n);
            // If switch is the same state as actionName, then we do nothing.
            int currentIndex = StopAfterParkSP.findOnSwitchIndex();
            if (!strcmp(actionName, StopAfterParkSP[currentIndex].getName()))
            {
                DEBUGF(INDI::Logger::DBG_SESSION, "Stop After Park is already %s", StopAfterParkSP[currentIndex].label);
                StopAfterParkSP.setState(IPS_IDLE);
                StopAfterParkSP.apply(NULL);
                return true;
            }

            // Otherwise, let us update the switch state
            StopAfterParkSP.update(states, names, n);
            currentIndex = StopAfterParkSP.findOnSwitchIndex();
            DEBUGF(INDI::Logger::DBG_SESSION, "Stop After Park is now %s", StopAfterParkSP[currentIndex].label);
            StopAfterParkSP.setState(IPS_OK);
            StopAfterParkSP.apply(NULL);
            return true;
        }
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

const char *LX200FS2::getDefaultName()
{
    return "Astro-Electronic FS-2";
}

bool LX200FS2::isSlewComplete()
{
    const double dx = targetRA - currentRA;
    const double dy = targetDEC - currentDEC;
    return fabs(dx) <= (SlewAccuracyNP[0].value / (900.0)) && fabs(dy) <= (SlewAccuracyNP[1].value / 60.0);
}

bool LX200FS2::checkConnection()
{
    return true;
}

bool LX200FS2::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    SlewAccuracyNP.save(fp);
    StopAfterParkSP.save(fp);

    return true;
}

bool LX200FS2::Park()
{
    double parkAZ  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    ln_hrz_posn horizontalPos;
    horizontalPos.az = parkAZ;
    horizontalPos.alt = parkAlt;

    ln_lnlat_posn observer;

    observer.lat = LocationNP[LOCATION_LATITUDE].getValue();
    observer.lng = LocationNP[LOCATION_LONGITUDE].getValue();

    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;

    get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

    char RAStr[16], DEStr[16];
    fs_sexa(RAStr, equatorialPos.ra / 15.0, 2, 3600);
    fs_sexa(DEStr, equatorialPos.dec, 2, 3600);
    LOGF_DEBUG("Parking to RA (%s) DEC (%s)...", RAStr, DEStr);

    if (Goto(equatorialPos.ra / 15.0, equatorialPos.dec))
    {
        TrackState = SCOPE_PARKING;
        LOG_INFO("Parking is in progress...");

        return true;
    }
    else
        return false;
}

void LX200FS2::TrackingStop()
{
    if (ParkedStatus != PARKED_NOTPARKED) return;

    // Remember current slew rate
    savedSlewRateIndex = static_cast <enum TelescopeSlewRate> (IUFindOnSwitchIndex(&SlewRateSP));

    updateSlewRate(SLEW_CENTERING);
    ParkedStatus = PARKED_NEEDABORT;
}

void LX200FS2::TrackingStop_Abort()
{
    if (ParkedStatus != PARKED_NEEDABORT) return;

    Abort();
    ParkedStatus = PARKED_NEEDSTOP;
}

void LX200FS2::TrackingStop_AllStop()
{
    if (ParkedStatus != PARKED_NEEDSTOP) return;

    MoveWE(DIRECTION_EAST, MOTION_START);
    ParkedStatus = PARKED_STOPPED;
}

void LX200FS2::TrackingStart()
{
    if (ParkedStatus != PARKED_STOPPED) return;

    MoveWE(DIRECTION_EAST, MOTION_STOP);

    ParkedStatus = UNPARKED_NEEDSLEW;
}

void LX200FS2::TrackingStart_RestoreSlewRate()
{
    if (ParkedStatus != UNPARKED_NEEDSLEW) return;

    updateSlewRate(savedSlewRateIndex);

    ParkedStatus = PARKED_NOTPARKED;
}

bool LX200FS2::ReadScopeStatus()
{
    bool retval = LX200Generic::ReadScopeStatus();

    // For FS-2 v1.21 owners, stop tracking once Parked.
    if (retval &&
            StopAfterParkSP[0].getState() == ISS_ON &&
            isConnected() &&
            !isSimulation())
    {
        switch (TrackState)
        {
            case SCOPE_PARKED:
                // If you are changing state from parking to parked,
                // kick off the motor-stopping state machine
                switch (ParkedStatus)
                {
                    case PARKED_NOTPARKED:
                        LOG_INFO("Mount at park position. Tracking stopping.");
                        TrackingStop();
                        break;
                    case PARKED_NEEDABORT:
                        LOG_INFO("Mount at 1x sidereal.");
                        TrackingStop_Abort();
                        break;
                    case PARKED_NEEDSTOP:
                        LOG_INFO("Mount is parked, motors stopped.");
                        TrackingStop_AllStop();
                        break;
                    case PARKED_STOPPED:
                    default:
                        break;

                }
                break;
            case SCOPE_IDLE:
                // If you are changing state from parked to tracking,
                // kick off the motor-starting state machine
                switch (ParkedStatus)
                {
                    case UNPARKED_NEEDSLEW:
                        LOG_INFO("Mount is unparked, restoring slew rate.");
                        TrackingStart_RestoreSlewRate();
                        break;
                    default:
                        break;

                }
                break;
            default:
                break;
        }
        return true;
    }


    return retval;
}

bool LX200FS2::UnPark()
{
    double parkAZ  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Unparking from Az (%s) Alt (%s)...", AzStr, AltStr);

    ln_hrz_posn horizontalPos;
    horizontalPos.az = parkAZ;
    horizontalPos.alt = parkAlt;

    ln_lnlat_posn observer;

    observer.lat = LocationNP[LOCATION_LATITUDE].getValue();
    observer.lng = LocationNP[LOCATION_LONGITUDE].getValue();

    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;

    get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

    char RAStr[16], DEStr[16];
    fs_sexa(RAStr, equatorialPos.ra / 15.0, 2, 3600);
    fs_sexa(DEStr, equatorialPos.dec, 2, 3600);
    LOGF_DEBUG("Syncing to parked coordinates RA (%s) DEC (%s)...", RAStr, DEStr);

    if (Sync(equatorialPos.ra / 15.0, equatorialPos.dec))
    {
        SetParked(false);
        if (StopAfterParkSP[0].getState() == ISS_ON)
        {
            TrackingStart();
        }
        return true;
    }
    else
        return false;
}

bool LX200FS2::SetCurrentPark()
{
    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270

    ln_lnlat_posn observer;
    observer.lat = LocationNP[LOCATION_LATITUDE].getValue();
    observer.lng = LocationNP[LOCATION_LONGITUDE].getValue();
    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;
    equatorialPos.ra  = currentRA * 15;
    equatorialPos.dec = currentDEC;
    get_hrz_from_equ(&equatorialPos, &observer, ln_get_julian_from_sys(), &horizontalPos);
    double parkAZ = horizontalPos.az;
    double parkAlt = horizontalPos.alt;

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr,
               AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool LX200FS2::SetDefaultPark()
{
    // By defualt azimuth 0
    SetAxis1Park(0);

    // Altitude = latitude of observer
    SetAxis2Park(LocationNP[LOCATION_LATITUDE].value);

    return true;
}

bool LX200FS2::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(latitude);
    INDI_UNUSED(longitude);
    INDI_UNUSED(elevation);
    return true;
}
