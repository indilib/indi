#if 0
LX200-based EQ500X Equatorial Mount
Copyright (C) 2019 Eric Dejouhanet (eric.dejouhanet@gmail.com)
Low-level communication from elias.erdnuess@nimax.de

This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA
#endif

#include "lx200generic.h"
#include "eq500x.h"

#include "indicom.h"
#include "lx200driver.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <cassert>

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

static struct _simEQ500X
{
    char MechanicalRA[16];
    char MechanicalDEC[16];
} simEQ500X = {
    "00:00:00",
    "+00*00'00",
};

#define MechanicalPoint_DEC_Format "+DD:MM:SS"
#define MechanicalPoint_RA_Format  "HH:MM:SS"

static int const EQ500X_TIMEOUT = 5;
static double const ONEDEGREE = 1.0;
static double const ARCMINUTE = ONEDEGREE/60.0;
static double const ARCSECOND = ONEDEGREE/3600.0;
static double const RADIUS_ONEDEGREE = std::sqrt(ONEDEGREE*ONEDEGREE + ONEDEGREE*ONEDEGREE);
static double const RADIUS_ARCMINUTE = RADIUS_ONEDEGREE/60.0;
static double const RADIUS_ARCSECOND = RADIUS_ONEDEGREE/3660.0;

/**************************************************************************************
** EQ500X Constructor
***************************************************************************************/
EQ500X::EQ500X(): LX200Generic()
{
    setVersion(1, 0);
    // Only pulse guiding, no tracking frequency
    setLX200Capability(LX200_HAS_PULSE_GUIDING);
    // Sync, goto, abort, location and 4 slew rates, no guiding rates and no park position
    SetTelescopeCapability(TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_LOCATION, 4);
    LOG_DEBUG("Initializing from EQ500X device...");
}

/**************************************************************************************
**
***************************************************************************************/
const char *EQ500X::getDefautName()
{
    return (const char*)"EQ500X";
}

/**************************************************************************************
**
***************************************************************************************/


bool EQ500X::initProperties()
{
    LX200Telescope::initProperties();

    // Mount tracks as soon as turned on
    TrackState = SCOPE_TRACKING;

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
void EQ500X::getBasicData()
{
    /* */
}

bool EQ500X::checkConnection()
{
    if (isSimulation())
    {
        // Test suite is run on connection to allow attaching a gdb to the driver pid for debugging
        runTestSuite();
        return true;
    }

    if (PortFD <= 0)
        return false;

    LOGF_DEBUG("Testing telescope connection using GR...", "");
    tty_set_debug(1);

    LOGF_DEBUG("Clearing input...", "");
    tcflush(PortFD, TCIFLUSH);

    char b[64/*RB_MAX_LEN*/] = {0};

    for (int i = 0; i < 2; i++)
    {
        LOGF_DEBUG("Getting RA...", "");
        if (getCommandString(PortFD, b, ":GR#") && 1 <= i)
        {
            LOGF_DEBUG("Failure. Telescope is not responding to GR!", "");
            return false;
        }
        const struct timespec timeout = {0, 50000000L};
        nanosleep(&timeout, nullptr);
    }

    /* Blink the control pad */
    const struct timespec timeout = {0, 1000000L};
    sendCmd(":RG#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RC#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RM#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RS#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RC#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RM#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RS#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RC#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RM#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RS#");

    LOGF_DEBUG("Connection check successful!", "");
    tty_set_debug(0);
    return true;
}

bool EQ500X::updateLocation(double latitude, double longitude, double elevation)
{
    // Picked from ScopeSim
    INDI_UNUSED(elevation);
    lnobserver.lng = longitude;

    if (lnobserver.lng > 180)
        lnobserver.lng -= 360;
    lnobserver.lat = latitude;

    double const LST = get_local_sidereal_time(lnobserver.lng);
    currentPosition.setLST(LST);
    targetPosition.setLST(LST);

    LOGF_INFO("Location updated: Longitude (%g) Latitude (%g) LST (%g)", lnobserver.lng, lnobserver.lat, LST);
    return true;
}

bool EQ500X::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    if (isSimulation())
    {
        LOGF_DEBUG("Current RA/DEC is %f/%f, stored as %s/%s", currentRA, currentDEC, simEQ500X.MechanicalRA, simEQ500X.MechanicalDEC);
        mountSim();
        currentPosition.RAm(currentRA);
        currentPosition.toStringRA(simEQ500X.MechanicalRA, sizeof(simEQ500X.MechanicalRA));
        currentPosition.DECm(currentDEC);
        currentPosition.toStringDEC(simEQ500X.MechanicalDEC, sizeof(simEQ500X.MechanicalDEC));
        LOGF_DEBUG("New RA/DEC simulated as %f/%f, stored as %s/%s", currentRA, currentDEC, simEQ500X.MechanicalRA, simEQ500X.MechanicalDEC);
    }

    if (getCurrentPosition(currentPosition))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
        return false;
    }

    currentRA = currentPosition.RAm();
    currentDEC = currentPosition.DECm();

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if mount is done slewing - NewRaDec will update EqN, but mountSim as well
        if (!isSimulation() && EqN[AXIS_RA].value == currentPosition.RAm() && EqN[AXIS_DE].value == currentPosition.DECm())
        {
            // If mount stopped, but is not at target, issue a slower goto
            double ra_delta = currentPosition.RA_hours_to(targetPosition);
            double dec_delta = currentPosition.DEC_degrees_to(targetPosition);
            LOGF_INFO("Distance to target is %lf/%lf, adjusting until less than (%lf,%lf).", ra_delta, dec_delta, ARCSECOND, ARCSECOND);
            if (ARCSECOND <= std::abs(ra_delta) || ARCSECOND <= std::abs(dec_delta))
            {
                char CmdString[16] = {0};

                struct _adjustment {
                    char const * slew_rate;
                    double epsilon;
                    double distance;
                } const adjustments[] =
                {
                    {":RG#", 1*ARCSECOND,    0.5*ARCMINUTE }, // Guiding speed
                    {":RC#", 0.25*ARCMINUTE, 2*ARCMINUTE    }, // Centering speed
                    {":RM#", 1*ARCMINUTE,    4*ONEDEGREE   }, // Finding speed
                    {":RS#", 3*ONEDEGREE,    360*ONEDEGREE  }  // Slew speed
                };

                bool east = false, west = false, north = false, south = false;
                int countdown = 50;
                struct _adjustment const * previous_adjustment = nullptr;

                while (true) //(RADIUS_ARCSECOND <= ra_delta || RADIUS_ARCSECOND <= dec_delta)
                {
                    struct _adjustment const *ra_adjust = nullptr, *dec_adjust = nullptr;

                    // Choose slew rate for RA based on distance to target
                    double const abs_ra_delta = std::abs(ra_delta);
                    for(size_t i = 0; i < sizeof(adjustments) && nullptr == ra_adjust; i++)
                        if (abs_ra_delta <= adjustments[i].distance)
                            ra_adjust = &adjustments[i];
                    assert(nullptr != ra_adjust);

                    // Choose slew rate for DEC based on distance to target
                    double const abs_dec_delta = std::abs(dec_delta);
                    for(size_t i = 0; i < sizeof(adjustments) && nullptr == dec_adjust; i++)
                        if (abs_dec_delta <= adjustments[i].distance)
                            dec_adjust = &adjustments[i];
                    assert(nullptr != dec_adjust);

                    // Reset command string
                    CmdString[0] = '\0';

                    // We adjust the axis which has the faster slew rate, eventually both axis at the same time, at the same speed
                    struct _adjustment const * const adjustment = ra_adjust < dec_adjust ? dec_adjust : ra_adjust;
                    if (previous_adjustment != adjustment)
                    {
                        strcat(CmdString, adjustment->slew_rate);
                        previous_adjustment = adjustment;
                    }

                    // If RA is being adjusted, check delta against adjustment epsilon to enable or disable movement
                    if (ra_adjust == adjustment)
                    {
                        if (adjustment->epsilon <= ra_delta)
                        {
                            if (!east) { strcat(CmdString, ":Me#"); east = true;  }
                        }
                        else
                        {
                            if (east)  { strcat(CmdString, ":Qe#"); east = false; }
                        }

                        if (ra_delta <= -adjustment->epsilon)
                        {
                            if (!west) { strcat(CmdString, ":Mw#"); west = true;  }
                        }
                        else
                        {
                            if (west)  { strcat(CmdString, ":Qw#"); west = false; }
                        }
                    }

                    // If DEC is being adjusted, check delta against adjustment epsilon to enable or disable movement
                    if (dec_adjust == adjustment)
                    {
                        if (adjustment->epsilon <= dec_delta)
                        {
                            if (!north) { strcat(CmdString, ":Mn#"); north = true;  }
                        }
                        else
                        {
                            if (north)  { strcat(CmdString, ":Qn#"); north = false; }
                        }

                        if (dec_delta <= -adjustment->epsilon)
                        {
                            if (!south) { strcat(CmdString, ":Ms#"); south = true;  }
                        }
                        else
                        {
                            if (south)  { strcat(CmdString, ":Qs#"); south = false; }
                        }
                    }

                    // Basic algorithm sanitization on movement orientation: move one way or the other, or not at all
                    assert(!(east && west) && !(north && south));

                    LOGF_INFO("Centering (%lf,%lf) delta (%lf,%lf) moving %c%c%c%c at %s until less than %lf", targetPosition.RAm(), targetPosition.DECm(), ra_delta, dec_delta, north?'N':'.', south?'S':'.', west?'W':'.', east?'E':'.', adjustment->slew_rate, adjustment->epsilon);
                    if (CmdString[0] != '\0')
                    {
                        if (sendCmd(CmdString))
                        {
                            LOGF_ERROR("Error centering to (%lf,%lf)", targetPosition.RAm(), targetPosition.DECm());
                            slewError(-1);
                            return false;
                        }
                    }

                    if (--countdown <= 0)
                    {
                        LOG_INFO("Cancelling to check slew intermediate adjustment.");
                        if (sendCmd(":Q#"))
                        {
                            LOGF_ERROR("Error centering to (%lf,%lf)", targetPosition.RAm(), targetPosition.DECm());
                            slewError(-1);
                            return false;
                        }
                        break;
                    }

                    if (!east && !west && !north && !south)
                    {
                        LOGF_INFO("Centering delta (%lf,%lf) intermediate adjustment complete (%c%c%c%c)", ra_delta, dec_delta, north?'N':'.', south?'S':'.', west?'W':'.', east?'E':'.');
                        break;
                    }

                    const struct timespec timeout = {0, 100000000L};
                    nanosleep(&timeout, nullptr);

                    if (getCurrentPosition(currentPosition))
                    {
                        EqNP.s = IPS_ALERT;
                        IDSetNumber(&EqNP, "Error reading RA/DEC.");
                        return false;
                    }

                    // Update displayed RA/DEC
                    currentRA = currentPosition.RAm();
                    currentDEC = currentPosition.DECm();
                    NewRaDec(currentRA, currentDEC);

                    // Update deltas for next loop
                    ra_delta = currentPosition.RA_hours_to(targetPosition);
                    dec_delta = currentPosition.DEC_degrees_to(targetPosition);
                }

                /* Now don't go to tracking now, let ReadScope check again next second, and adjust again */
            }
            else
            {
                TrackState = SCOPE_TRACKING;
                LOG_INFO("Slew is complete. Tracking...");

                if (sendCmd(":RG#"))
                {
                    LOG_ERROR("Error setting guiding rate for tracking");
                    slewError(-1);
                    return false;
                }

                EqNP.s = IPS_OK;
                IDSetNumber(&EqNP, "Mount is tracking");
            }
        }
    }

    if (!isSimulation())
        NewRaDec(currentRA, currentDEC);

    return true;
}

bool EQ500X::Goto(double ra, double dec)
{
    targetPosition.RAm(targetRA = ra);
    targetPosition.DECm(targetDEC = dec);

    // Check whether a meridian flip is required
    ln_equ_posn lnradec { 0, 0 };
    lnradec.ra  = (currentRA * 360) / 24.0;
    lnradec.dec = currentDEC;

    ln_get_hrz_from_equ(&lnradec, &lnobserver, ln_get_julian_from_sys(), &lnaltaz);
    /* libnova measures azimuth from south towards west */
    double current_az = range360(lnaltaz.az + 180);
    double const MIN_AZ_FLIP = 180;
    double const MAX_AZ_FLIP = 200;

    if (current_az > MIN_AZ_FLIP && current_az < MAX_AZ_FLIP)
    {
        lnradec.ra  = (ra * 360) / 24.0;
        lnradec.dec = dec;

        ln_get_hrz_from_equ(&lnradec, &lnobserver, ln_get_julian_from_sys(), &lnaltaz);

        double target_az = range360(lnaltaz.az + 180);

        targetPosition.setFlipped(target_az >= current_az && target_az > MIN_AZ_FLIP);
    }

    char RAStr[16]={0}, DecStr[16]={0};
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // If moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
        if (!isSimulation() && abortSlew(PortFD) < 0)
        {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
        }

        AbortSP.s = IPS_OK;
        EqNP.s    = IPS_IDLE;
        IDSetSwitch(&AbortSP, "Slew aborted.");
        IDSetNumber(&EqNP, nullptr);

        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
            MovementNSSP.s = MovementWESP.s = IPS_IDLE;
            EqNP.s                          = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
        }

        // sleep for 100 mseconds
        const struct timespec timeout = {0, 100000000L};
        nanosleep(&timeout, nullptr);
    }

    if (setTargetPosition(targetPosition))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error setting RA/DEC.");
        return false;
    }

    if (!isSimulation())
    {
        /* The goto feature is quite imprecise because it will always use full speed.
         * By the time the mount stops, the position is off by nearly half a degree, depending on the speed attained during the move.
         * So, when slewing for more than a few degrees, use goto and adjust afterwards using centering speed or slower.
         * IF slewing for less than a few degrees, let ReadScope adjust the position.
         */

        double const distance = targetPosition - currentPosition;
        if (distance < RADIUS_ARCSECOND/2)
        {
            LOGF_INFO("Distance is %lf, not moving...", distance);
            return true;
        }
        else /*if (distance < 30.0*RADIUS_ARCMINUTE)*/
        {
            LOGF_INFO("Distance is %lf, adjusting to target...", distance);
            /* Let ReadScope do the centering */
        }
        /*
        else
        {
            LOGF_INFO("Distance is %lf, using full slew speed goto", distance);
            if (gotoTargetPosition())
            {
                LOGF_ERROR("Error Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
                slewError(-1);
                return false;
            }
        }
        */
    }

    TrackState = SCOPE_SLEWING;
    EqNP.s     = IPS_BUSY;

    LOGF_INFO("Slewing to JNow RA: %s - DEC: %s", RAStr, DecStr);

    return true;
}

bool EQ500X::slewEQ500X()
{
    if (!sendCmd(":MS#"))
        goto slew_failed;

    return true;

slew_failed:
    EqNP.s = IPS_ALERT;
    IDSetNumber(&EqNP, "Error slewing.");
    return false;
}

/**************************************************************************************
**
***************************************************************************************/
/*
function MountPosition () {
#Function that asks for mount coordinates. These coordinates are being returned in the MCC format, and with a bool variable USD to show if the  mount is currently upside-down.

    $ValueString = Send2Mount ":GR#" 1
    $ValueString = Send2Mount ":GD#" 1

    $MCC = mmcString2mcc $ValueString $ValueString
    $USD = isMccUSD $MCC[0] $MCC[1]

    #Returns variables, MCC is already an array of RA and DEC
    $MCC
    $USD
}

function mmcString2mcc ([String]$ValueString, [String]$ValueString) {

#This function takes two Strings from mount Input in the following form:
#RA: HH:MM:SS# from 00:00:00 to 23:59:59
#DEC: sDD*MM'SS# from -255:59:59 to +255:59:59.
#"from Mount input" means, that this function is directly used to interpret the strings the mount reports, when being asked for a set of coordinates.
#The function gives back a decimal representation for the RA hours and the DEC angle. The function automatically converts the MMC into MCC

    if ($ValueString.Substring(0,1) -like "+")  {$factor = +1} else {$factor = -1} #The sign (plus minus) of the DEC value is being interpreted first

    #The Mount has a weird way to represent the values from 1 to 25 with only one digit (when reading DEC value). For Example 155° is being reported as ?5°. Probably to save one character in the string....
    $DekaValue = switch ($ValueString.Substring(1,1)) { "0" {0} "1" {1} "2" {2} "3" {3} "4" {4} "5" {5} "6" {6} "7" {7} "8" {8} "9" {9} ":" {10} ";" {11} "<" {12} "=" {13} ">" {14} "?" {15} "@" {16} "A" {17} "B" {18} "C" {19} "D" {20} "E" {21} "F" {22} "G" {23} "H" {24} "I" {25}}

    #the following is almost identical to what one can find in string2hic
    $Degrees = $DekaValue * 10 + $ValueString.Substring(2,1)/1
    $ArcMinutes = $ValueString.Substring(4,2)/1
    $ArcSeconds = $ValueString.Substring(7,2)/1
    #Properly formating the values. /1 turns strings into integers
    $DecDouble = $factor * ($Degrees + $ArcMinutes/60 + $SArcSeconds/3600)
    $DecDouble = $DecDouble + 90 # To translate between MMC and MCC, 90° is added to the DEC value


    $RaHours = $ValueString.Substring(0,2)/1
    $RaMinutes = $ValueString.Substring(3,2)/1
    $RaSeconds = $ValueString.Substring(6,2)/1
    $RaDouble = $RaHours + $RaMinutes/60 + $RaSeconds/3600

    #Returns:
    $RaDouble
    $DecDouble
}
*/
int EQ500X::sendCmd(char const *data)
{
    tcflush(PortFD, TCIFLUSH);
    LOGF_DEBUG("CMD <%s>", data);
    if (!isSimulation())
    {
        int nbytes_write = 0;
        return tty_write_string(PortFD, data, &nbytes_write);
    }
    return 0;
}

int EQ500X::getReply(char *data, size_t const len)
{
    if (!isSimulation())
    {
        int nbytes_read = 0;
        int error_type = tty_read(PortFD, data, len, EQ500X_TIMEOUT, &nbytes_read);

        LOGF_DEBUG("RES <%.*s> (%d)", nbytes_read, data, error_type);
        return error_type;
    }
    return 0;
}

bool EQ500X::gotoTargetPosition()
{
    tcflush(PortFD, TCIFLUSH);
    if (!sendCmd(":MS#"))
    {
        char buf[64/*RB_MAX_LEN*/] = {0};
        if (!getReply(buf, 1))
            return buf[0] != '0'; // 0 is valid for :MS
    }
    return true;
}

bool EQ500X::getCurrentPosition(MechanicalPoint &p)
{
    char b[64/*RB_MAX_LEN*/] = {0};
    MechanicalPoint result = p;

    tcflush(PortFD, TCIFLUSH);

    if (isSimulation())
        memcpy(b, simEQ500X.MechanicalRA, std::min(sizeof(b), sizeof(simEQ500X.MechanicalRA)));
    else if (getCommandString(PortFD, b, ":GR#") < 0)
        goto radec_error;
    if (result.parseStringRA(b, 64))
        goto radec_error;

    LOGF_INFO("RA reads '%s' as %lf.", b, result.RAm());

    if (isSimulation())
        memcpy(b, simEQ500X.MechanicalDEC, std::min(sizeof(b), sizeof(simEQ500X.MechanicalDEC)));
    else if (getCommandString(PortFD, b, ":GD#") < 0)
        goto radec_error;
    if (result.parseStringDEC(b, 64))
        goto radec_error;

    LOGF_INFO("DEC reads '%s' as %lf.", b, result.DECm());

    p = result;
    return false;

radec_error:
    return true;
}

bool EQ500X::setTargetPosition(MechanicalPoint const &p)
{
    char CmdString[] = ":Sr" MechanicalPoint_RA_Format "#:Sd" MechanicalPoint_DEC_Format "#";
    char bufRA[sizeof(MechanicalPoint_RA_Format)], bufDEC[sizeof(MechanicalPoint_DEC_Format)];
    snprintf(CmdString, sizeof(CmdString), ":Sr%s#:Sd%s#", p.toStringRA(bufRA, sizeof(bufRA)), p.toStringDEC(bufDEC, sizeof(bufDEC)));
    LOGF_INFO("Target RA '%f' DEC '%f' converted to '%s'", static_cast <float> (p.RAm()), static_cast <float> (p.DECm()), CmdString);

    if (!isSimulation())
    {
        char buf[64/*RB_MAX_LEN*/] = {0};
        tcflush(PortFD, TCIFLUSH);

        if (sendCmd(CmdString))
        {
            LOGF_WARN("Failed '%s'", CmdString);
            return true;
        }

        if (getReply(buf, 2))
        {
            LOGF_WARN("Failed getting 2-byte reply to '%s'", CmdString);
            return true;
        }

        if (buf[0] != '1' && buf[1] != '1')
        {
            LOGF_WARN("Failed '%s', mount replied %c%c", CmdString, buf[0], buf[1]);
            return true;
        }
    }

    return false;
}

bool EQ500X::MechanicalPoint::parseStringRA(char const *buf, size_t buf_length)
{
    if (buf_length < sizeof(MechanicalPoint_RA_Format-1))
        return true;

    // Mount replies to "#GR:" with "HH:MM:SS".
    // HH, MM and SS are respectively hours, minutes and seconds in [00:00:00,23:59:59].
    // FIXME: Sanitize.

    int hours = 0, minutes = 0, seconds = 0;
    if (3 == sscanf(buf, "%02d:%02d:%02d", &hours, &minutes, &seconds))
    {
        _RAm = range24(
                    static_cast <double> (
                        std::fmod(
                            static_cast <float> (hours % 24) +
                            static_cast <float> (minutes) / 60.0f +
                            static_cast <float> (seconds) / 3600.0f +
                            (_isFlipped ? +12.0f : +0.0f), 24 ) )
                    - _LST );
        return false;
    }

    return true;
}

void EQ500X::MechanicalPoint::setLST(double const LST)
{
   _LST = LST;
}

double EQ500X::MechanicalPoint::RAm(double const value)
{
    return _RAm = value;
}

char const * EQ500X::MechanicalPoint::toStringRA(char *buf, size_t buf_length) const
{
    // NotFlipped                 Flipped
    // S -12.0h <-> -12:00:00 <-> -24.0h
    // E -06.0h <-> -06:00:00 <-> -18.0h
    // N +00.0h <-> +00:00:00 <-> -12.0h
    // W +06.0h <-> +06:00:00 <-> -06.0h
    // S +12.0h <-> +12:00:00 <-> +00.0h
    // E +18.0h <-> +18:00:00 <-> +06.0h
    // N +24.0h <-> +24:00:00 <-> +12.0h

    double value = range24(_RAm + _LST);
    int const sgn = _isFlipped ? (value <= -12.0 ? -1 : 1) : (value <= 0.0 ? -1 : 1);
    double const mechanical_hours = std::abs(value);
    int const hours = ((_isFlipped ? 12 : 0 ) + 24 + sgn * (static_cast <int> (std::floor(mechanical_hours)) % 24)) % 24;
    int const minutes = static_cast <int> (std::floor(mechanical_hours * 60.0)) % 60;
    int const seconds = static_cast <int> (std::lround(mechanical_hours * 3600.0)) % 60;

    int const written = snprintf(buf, buf_length, "%02d:%02d:%02d", hours, minutes, seconds);

    return (0 < written && written < (int) buf_length) ? buf : (char const*)0;
}

bool EQ500X::MechanicalPoint::parseStringDEC(char const *buf, size_t buf_length)
{
    if (buf_length < sizeof(MechanicalPoint_DEC_Format)-1)
        return true;

    char b[sizeof(MechanicalPoint_DEC_Format)] = {0};
    strncpy(b, buf, sizeof(b));

    // Mount replies to "#GD:" with "sDD:MM:SS".
    // s is in {+,-} and provides a sign.
    // DD are degrees, unit D spans '0' to '9' in [0,9] but high D spans '0' to 'I' in [0,25].
    // MM are minutes, SS are seconds in [00:00,59:59].
    // The whole reply is in [-255:59:59,+255:59:59].

    struct _DecValues {
        char degrees[4];
        char minutes[3];
        char seconds[3];
    } * const DecValues = (struct _DecValues*) b;

    if (DecValues->degrees[1] < '0' || 'I' < DecValues->degrees[1])
        return true;

    int const sgn = DecValues->degrees[0] == '-' ? -1 : +1;

    // Replace sign with hundredth, or space if lesser than 100
    switch (DecValues->degrees[1])
    {
    case ':': DecValues->degrees[0] = '1'; DecValues->degrees[1] = '0'; break;
    case ';': DecValues->degrees[0] = '1'; DecValues->degrees[1] = '1'; break;
    case '<': DecValues->degrees[0] = '1'; DecValues->degrees[1] = '2'; break;
    case '=': DecValues->degrees[0] = '1'; DecValues->degrees[1] = '3'; break;
    case '>': DecValues->degrees[0] = '1'; DecValues->degrees[1] = '4'; break;
    case '?': DecValues->degrees[0] = '1'; DecValues->degrees[1] = '5'; break;
    case '@': DecValues->degrees[0] = '1'; DecValues->degrees[1] = '6'; break;
    case 'A': DecValues->degrees[0] = '1'; DecValues->degrees[1] = '7'; break;
    case 'B': DecValues->degrees[0] = '1'; DecValues->degrees[1] = '8'; break;
    case 'C': DecValues->degrees[0] = '1'; DecValues->degrees[1] = '9'; break;
    case 'D': DecValues->degrees[0] = '2'; DecValues->degrees[1] = '0'; break;
    case 'E': DecValues->degrees[0] = '2'; DecValues->degrees[1] = '1'; break;
    case 'F': DecValues->degrees[0] = '2'; DecValues->degrees[1] = '2'; break;
    case 'G': DecValues->degrees[0] = '2'; DecValues->degrees[1] = '3'; break;
    case 'H': DecValues->degrees[0] = '2'; DecValues->degrees[1] = '4'; break;
    case 'I': DecValues->degrees[0] = '2'; DecValues->degrees[1] = '5'; break;
    default: DecValues->degrees[0] = ' '; break;
    }
    DecValues->degrees[3] = DecValues->minutes[2] = DecValues->seconds[2] = '\0';

    // FIXME: could be sscanf("%d%d%d", ...) but needs intermediate variables, however that would count matches
    int const orientation = _isFlipped ? -1 : +1;
    _DECm = static_cast <double> (
                90.0f + orientation * sgn * (
                    atof(DecValues->degrees) +
                    atof(DecValues->minutes)/60.0f +
                    atof(DecValues->seconds)/3600.0f ));

    //LOGF_INFO("DEC converts as %f.", value);

    return false;
}

double EQ500X::MechanicalPoint::DECm(double const value)
{
    // Should be inside [-180,+180], even [-90,+90], but mount supports a larger (not useful) interval
    return _DECm = value;
}

char const * EQ500X::MechanicalPoint::toStringDEC(char *buf, size_t buf_length) const
{
    // NotFlipped                         Flipped
    // -180.0° <-> -255.0 = -I5:00:00 <-> +345.0°
    // -135.0° <-> -225.0 = -F5:00:00 <-> +315.0°
    //  -90.0° <-> -180.0 = -B0:00:00 <-> +270.0°
    //  -45.0° <-> -135.0 = -=5:00:00 <-> +225.0°
    //  +00.0° <->  -90.0 = -90:00:00 <-> +180.0°
    //  +45.0° <->  -45.0 = -45:00:00 <-> +135.0°
    //  +90.0° <->    0.0 = +00:00:00 <->  +90.0°
    // +135.0° <->   45.0 = +45:00:00 <->  +45.0°
    // +180.0° <->   90.0 = +90:00:00 <->  +00.0°
    // +225.0° <->  135.0 = +=5:00:00 <->  -45.0°
    // +270.0° <->  180.0 = +B0:00:00 <->  -90.0°
    // +315.0° <->  225.0 = +F5:00:00 <-> -135.0°
    // +345.0° <->  255.0 = +I5:00:00 <-> -180.0°

    double value = _DECm;
    int const degrees = static_cast <int> (_isFlipped ? (90.0 - value) : (value - 90.0));

    if (degrees < -255 || +255 < degrees)
        return (char const*)0;

    char high_digit = '0';
    if (-100 < degrees && degrees < 100)
    {
        high_digit = '0' + (std::abs(degrees)/10);
    }
    else switch (std::abs(degrees)/10)
    {
    case 10: high_digit = ':'; break;
    case 11: high_digit = ';'; break;
    case 12: high_digit = '<'; break;
    case 13: high_digit = '='; break;
    case 14: high_digit = '>'; break;
    case 15: high_digit = '?'; break;
    case 16: high_digit = '@'; break;
    case 17: high_digit = 'A'; break;
    case 18: high_digit = 'B'; break;
    case 19: high_digit = 'C'; break;
    case 20: high_digit = 'D'; break;
    case 21: high_digit = 'E'; break;
    case 22: high_digit = 'F'; break;
    case 23: high_digit = 'G'; break;
    case 24: high_digit = 'H'; break;
    case 25: high_digit = 'I'; break;
    default: return (char const*)0; break;
    }

    char const low_digit = '0' + (std::abs(degrees)%10);

    int const minutes = static_cast <int> (std::floor(std::abs(degrees)* 60.0)) % 60;
    int const seconds = static_cast <int> (std::lround(std::abs(degrees) * 3600.0)) % 60;

    int const written = snprintf(buf, buf_length, "%c%c%c:%02d:%02d", (0<=degrees)?'+':'-', high_digit, low_digit, minutes, seconds);

    return (0 < written && written < (int) buf_length) ? buf : (char const*)0;
}

double EQ500X::MechanicalPoint::RA_hours_to(EQ500X::MechanicalPoint &b) const
{
    // RA is circular, DEC is not
    // We keep hours and not degrees because that's what the mount is handling in terms of precision
    // If we were to use real degrees, the RA movement would need to be 15 times more precise
    double ra = (b._RAm - _RAm);
    if (ra > +12.0) ra -= 24.0;
    if (ra < -12.0) ra += 24.0;
    return ra;
}

double EQ500X::MechanicalPoint::DEC_degrees_to(EQ500X::MechanicalPoint &b) const
{
    // RA is circular, DEC is not
    return b._DECm - _DECm;
}

double EQ500X::MechanicalPoint::operator -(EQ500X::MechanicalPoint &b) const
{
    double const ra_distance = RA_hours_to(b);
    double const dec_distance = DEC_degrees_to(b);
    // FIXME: Incorrect distance calculation, but enough for our purpose
    return std::sqrt(ra_distance*ra_distance + dec_distance*dec_distance);
}

bool EQ500X::MechanicalPoint::setFlipped(bool const flipped)
{
    return _isFlipped = flipped;
}

void EQ500X::runTestSuite()
{
    EQ500X::MechanicalPoint p;
    char b[64]= {0};
    // NotFlipped                 Flipped
    // S -12.0h <-> 12:00:00 <-> -24.0h
    // E -06.0h <-> 18:00:00 <-> -18.0h
    // N +00.0h <-> 00:00:00 <-> -12.0h
    // W +06.0h <-> 06:00:00 <-> -06.0h
    // S +12.0h <-> 12:00:00 <-> +00.0h
    // E +18.0h <-> 18:00:00 <-> +06.0h
    // N +24.0h <-> 00:00:00 <-> +12.0h

    assert(false == p.setFlipped(false));
    assert(!p.parseStringRA("00:00:00",8));
    assert( +0.0 == p.RAm());
    assert(!strncmp("00:00:00",p.toStringRA(b,64),64));
    assert(!p.parseStringRA("06:00:00",8));
    assert( +6.0 == p.RAm());
    assert(!strncmp("06:00:00",p.toStringRA(b,64),64));
    assert(!p.parseStringRA("12:00:00",8));
    assert(+12.0 == p.RAm());
    assert(!strncmp("12:00:00",p.toStringRA(b,64),64));
    assert(!p.parseStringRA("18:00:00",8));
    assert(+18.0 == p.RAm());
    assert(!strncmp("18:00:00",p.toStringRA(b,64),64));
    assert(!p.parseStringRA("24:00:00",8));
    assert( +0.0 == p.RAm());
    assert(!strncmp("00:00:00",p.toStringRA(b,64),64));

    assert(true == p.setFlipped(true));
    assert(!p.parseStringRA("00:00:00",8));
    assert(+12.0 == p.RAm());
    assert(!strncmp("00:00:00",p.toStringRA(b,64),64));
    assert(!p.parseStringRA("06:00:00",8));
    assert(+18.0 == p.RAm());
    assert(!strncmp("06:00:00",p.toStringRA(b,64),64));
    assert(!p.parseStringRA("12:00:00",8));
    assert( +0.0 == p.RAm());
    assert(!strncmp("12:00:00",p.toStringRA(b,64),64));
    assert(!p.parseStringRA("18:00:00",8));
    assert( +6.0 == p.RAm());
    assert(!strncmp("18:00:00",p.toStringRA(b,64),64));
    assert(!p.parseStringRA("24:00:00",8));
    assert(+12.0 == p.RAm());
    assert(!strncmp("00:00:00",p.toStringRA(b,64),64));

    // NotFlipped                         Flipped
    // -165.0° <-> -255.0 = -I5:00:00 <-> +345.0°
    // -135.0° <-> -225.0 = -F5:00:00 <-> +315.0°
    //  -90.0° <-> -180.0 = -B0:00:00 <-> +270.0°
    //  -45.0° <-> -135.0 = -=5:00:00 <-> +225.0°
    //  +00.0° <->  -90.0 = -90:00:00 <-> +180.0°
    //  +45.0° <->  -45.0 = -45:00:00 <-> +135.0°
    //  +90.0° <->    0.0 = +00:00:00 <->  +90.0°
    // +135.0° <->   45.0 = +45:00:00 <->  +45.0°
    // +180.0° <->   90.0 = +90:00:00 <->  +00.0°
    // +225.0° <->  135.0 = +=5:00:00 <->  -45.0°
    // +270.0° <->  180.0 = +B0:00:00 <->  -90.0°
    // +315.0° <->  225.0 = +F5:00:00 <-> -135.0°
    // +345.0° <->  255.0 = +I5:00:00 <-> -165.0°

    assert(false == p.setFlipped(false));
    assert(!p.parseStringDEC("-I5:00:00",9));
    assert(-165.0 == p.DECm());
    assert(!strncmp("-I5:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("-F5:00:00",9));
    assert(-135.0 == p.DECm());
    assert(!strncmp("-F5:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("-B0:00:00",9));
    assert( -90.0 == p.DECm());
    assert(!strncmp("-B0:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("-=5:00:00",9));
    assert( -45.0 == p.DECm());
    assert(!strncmp("-=5:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("-90:00:00",9));
    assert(  +0.0 == p.DECm());
    assert(!strncmp("-90:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("-45:00:00",9));
    assert( +45.0 == p.DECm());
    assert(!strncmp("-45:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+00:00:00",9));
    assert( +90.0 == p.DECm());
    assert(!strncmp("+00:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+45:00:00",9));
    assert(+135.0 == p.DECm());
    assert(!strncmp("+45:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+90:00:00",9));
    assert(+180.0 == p.DECm());
    assert(!strncmp("+90:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+=5:00:00",9));
    assert(+225.0 == p.DECm());
    assert(!strncmp("+=5:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+B0:00:00",9));
    assert(+270.0 == p.DECm());
    assert(!strncmp("+B0:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+F5:00:00",9));
    assert(+315.0 == p.DECm());
    assert(!strncmp("+F5:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+I5:00:00",9));
    assert(+345.0 == p.DECm());
    assert(!strncmp("+I5:00:00",p.toStringDEC(b,64),64));

    assert(p.parseStringDEC("+J0:00:00",9));
    assert(p.parseStringDEC("-J0:00:00",9));

    assert(true == p.setFlipped(true));
    assert(!p.parseStringDEC("-I5:00:00",9));
    assert(+345.0 == p.DECm());
    assert(!strncmp("-I5:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("-F5:00:00",9));
    assert(+315.0 == p.DECm());
    assert(!strncmp("-F5:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("-B0:00:00",9));
    assert(+270.0 == p.DECm());
    assert(!strncmp("-B0:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("-=5:00:00",9));
    assert(+225.0 == p.DECm());
    assert(!strncmp("-=5:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("-90:00:00",9));
    assert(+180.0 == p.DECm());
    assert(!strncmp("-90:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("-45:00:00",9));
    assert(+135.0 == p.DECm());
    assert(!strncmp("-45:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+00:00:00",9));
    assert( +90.0 == p.DECm());
    assert(!strncmp("+00:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+45:00:00",9));
    assert( +45.0 == p.DECm());
    assert(!strncmp("+45:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+90:00:00",9));
    assert( +0.0 == p.DECm());
    assert(!strncmp("+90:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+=5:00:00",9));
    assert( -45.0 == p.DECm());
    assert(!strncmp("+=5:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+B0:00:00",9));
    assert( -90.0 == p.DECm());
    assert(!strncmp("+B0:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+F5:00:00",9));
    assert(-135.0 == p.DECm());
    assert(!strncmp("+F5:00:00",p.toStringDEC(b,64),64));
    assert(!p.parseStringDEC("+I5:00:00",9));
    assert(-165.0 == p.DECm());
    assert(!strncmp("+I5:00:00",p.toStringDEC(b,64),64));

    assert(p.parseStringDEC("+J0:00:00",9));
    assert(p.parseStringDEC("-J0:00:00",9));
}
