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

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

static struct _simEQ500X
{
    char MechanicalRA[16];
    char MechanicalDEC[16];
} simEQ500X = {
    "12:00:00",
    "+00:00:00",
};

/**************************************************************************************
** EQ500X Constructor
***************************************************************************************/
EQ500X::EQ500X(): LX200Generic()
{
    setVersion(1, 0);
    setLX200Capability(LX200_HAS_TRACKING_FREQ | LX200_HAS_PULSE_GUIDING);
    SetTelescopeCapability(TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_LOCATION );
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
        return true;

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

    LOGF_INFO("Location updated: Longitude (%g) Latitude (%g)", lnobserver.lng, lnobserver.lat);
    return true;
}

bool EQ500X::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    if (isSimulation())
    {
        mountSim();
        getEQ500XRA_snprint(simEQ500X.MechanicalRA, sizeof(simEQ500X.MechanicalRA), currentRA);
        getEQ500XDEC_snprint(simEQ500X.MechanicalDEC, sizeof(simEQ500X.MechanicalDEC), currentDEC);
        LOGF_DEBUG("RA/DEC simulated as %f/%f, stored as %s/%s", currentRA, currentDEC, simEQ500X.MechanicalRA, simEQ500X.MechanicalDEC);
    }

    if (!getEQ500XRA(&currentRA) || !getEQ500XDEC(&currentDEC))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
        return false;
    }

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if mount is done slewing - NewRaDec will update EqN
        if (EqN[AXIS_RA].value == currentRA && EqN[AXIS_DE].value == currentDEC)
        {
            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }
        // If mount is close to target, issue a slower goto
        else if (std::fmod(std::abs(currentRA-targetRA), 24) < 5.f && std::fmod(std::abs(currentDEC-targetDEC), 360) < 5.f)
        {
            LOG_INFO("Slew is close to target...");
        }
    }

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool EQ500X::Goto(double ra, double dec)
{
    targetRA  = ra;
    targetDEC = dec;

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

        //forceMeridianFlip = (target_az >= current_az && target_az > MIN_AZ_FLIP);
    }

    char RAStr[16]={0}, DecStr[16]={0};
    int const fracbase = 3600;
    fs_sexa(RAStr, targetRA, 2, fracbase);
    fs_sexa(DecStr, targetDEC, 2, fracbase);

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

    if (!setEQ500XRA(targetRA) || !setEQ500XDEC(targetDEC))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error setting RA/DEC.");
        return false;
    }

    if (sendCmd(":RM#"))
    {
        LOGF_ERROR("Error Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
        slewError(-1);
        return false;
    }

    if (sendCmd(":MS#"))
    {
        LOGF_ERROR("Error Slewing to JNow RA %s - DEC %s", RAStr, DecStr);
        slewError(-1);
        return false;
    }

    TrackState = SCOPE_SLEWING;
    EqNP.s     = IPS_BUSY;

    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);

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
int EQ500X::sendCmd(const char *data)
{
    int error_type;
    int nbytes_write = 0;

    LOGF_DEBUG("CMD <%s>", data);
    if (!isSimulation())
    {
        if ((error_type = tty_write_string(PortFD, data, &nbytes_write)) != TTY_OK)
            return error_type;
        tcflush(PortFD, TCIFLUSH);
    }
    return 0;
}

bool EQ500X::getEQ500XRA(double *result)
{
    char b[64/*RB_MAX_LEN*/] = {0};

    if (isSimulation())
    {
        memcpy(b, simEQ500X.MechanicalRA, std::min(sizeof(b), sizeof(simEQ500X.MechanicalRA)));
    }
    else if (getCommandString(PortFD, b, ":GR#") < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA.");
        return false;
    }

    LOGF_INFO("RA reads '%s'.", b);

    // Mount replies to "#GR:" with "HH:MM:SS".
    // HH, MM and SS are respectively hours, minutes and seconds in [00:00:00,23:59:59].
    // FIXME: Sanitize.

    struct _RaValues {
        char hours[3];
        char minutes[3];
        char seconds[3];
    } * const RaValues = (struct _RaValues*) b;

    RaValues->hours[2] = RaValues->minutes[2] = RaValues->seconds[2] = '\0';

    float const value =
            atof(RaValues->hours) +
            atof(RaValues->minutes)/60.0f +
            atof(RaValues->seconds)/3600.0f+
            (forceMeridianFlip? -12 : 0 );

    LOGF_INFO("RA converts as %f.", value);

    if (result != nullptr)
        *result = static_cast <double> (value);

    return true;
}

bool EQ500X::getEQ500XRA_snprint(char *buf, size_t buf_length, double value)
{
    // NotFlipped                 Flipped
    // S -12.0h <-> -12:00:00 <-> -24.0h
    // E -06.0h <-> -06:00:00 <-> -18.0
    // N +00.0h <-> +00:00:00 <-> -12.0h
    // W +06.0h <-> +06:00:00 <-> -06.0°
    // S +12.0h <-> +12:00:00 <-> +00.0°
    // E +18.0h <-> +18:00:00 <-> +06.0°
    // N +24.0h <-> +24:00:00 <-> +12.0°

    int const sgn = forceMeridianFlip ? value <= -12.0 ? -1 : 1 : value <= 0.0 ? -1 : 1;
    double const mechanical_hours = std::abs(value);
    int const hours = ((forceMeridianFlip ? 12 : 0 ) + 24 + sgn * (static_cast <int> (std::floor(mechanical_hours)) % 24)) % 24;
    int const minutes = static_cast <int> (std::floor(mechanical_hours * 60.0)) % 60;
    int const seconds = static_cast <int> (std::lround(mechanical_hours * 3600.0)) % 60;

    return 8 == snprintf(buf, buf_length, "%02d:%02d:%02d", hours, minutes, seconds);
}

bool EQ500X::setEQ500XRA(double value)
{
    char CmdString[16] = ":Sr";
    getEQ500XRA_snprint(CmdString+std::strlen(CmdString), sizeof(CmdString)-std::strlen(CmdString), value);
    std::strcat(CmdString, "#");
    LOGF_INFO("RA '%f' converted to '%s'", static_cast <float> (value), CmdString);

    // FIXME: Is there a result expected to the command?
    if (sendCmd(CmdString))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error writing RA.");
        return false;
    }

    return true;
}

bool EQ500X::getEQ500XDEC(double *result)
{
    char b[64/*RB_MAX_LEN*/] = {0};

    if (isSimulation())
    {
        memcpy(b, simEQ500X.MechanicalDEC, std::min(sizeof(b), sizeof(simEQ500X.MechanicalDEC)));
    }
    else if (getCommandString(PortFD, b, ":GD#") < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading DEC.");
        return false;
    }

    LOGF_INFO("DEC reads '%s'", b);

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

    int sgn = DecValues->degrees[0] == '-' ? -1 : +1;

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

    float const value = 90.0f + sgn * (
                atof(DecValues->degrees) +
                atof(DecValues->minutes)/60.0f +
                atof(DecValues->seconds)/3600.0f);

    LOGF_INFO("DEC converts as %f.", value);

    if (result != nullptr)
    {
        *result = static_cast <double> (value);
    }

    return true;
}

bool EQ500X::getEQ500XDEC_snprint(char *buf, size_t buf_length, double value)
{
    // NotFlipped                 Flipped
    //             -270:00:00
    //  -90.0° <-> -180:00:00
    //  +00.0° <->  -90:00:00
    //  +90.0° <->  +00:00:00 <->  -90.0°
    //              +90:00:00 <->  +00.0°
    //             +180:00:00 <->  +90.0°
    //             +270:00:00

    int const sgn = forceMeridianFlip ? -1 : +1;
    double const mechanical_degrees = std::abs( forceMeridianFlip ? value + 90.0 : value - 90.0);
    int const degrees = static_cast <int> (mechanical_degrees) * sgn;
    int const minutes = static_cast <int> (std::floor(mechanical_degrees * 60.0)) % 60;
    int const seconds = static_cast <int> (std::lround(mechanical_degrees * 3600.0)) % 60;

    snprintf(buf, buf_length, "%+03d:%02d:%02d", degrees, minutes, seconds);
    return true;
}

bool EQ500X::setEQ500XDEC(double value)
{
    char CmdString[16] = ":Sd";
    getEQ500XDEC_snprint(CmdString+std::strlen(CmdString), sizeof(CmdString)-std::strlen(CmdString), value);
    std::strcat(CmdString, "#");
    LOGF_INFO("DEC '%f' converted to '%s'", static_cast <float> (value), CmdString);

    // FIXME: Is there a result expected to the command?
    // See  int LX200_10MICRON::setStandardProcedureWithoutRead(int fd, const char *data)

    if (sendCmd(CmdString))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error writing DEC.");
        return false;
    }

    return true;
}

