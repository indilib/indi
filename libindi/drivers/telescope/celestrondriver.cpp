/*
    Celestron driver

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

/*
    Version with experimental pulse guide support. GC 04.12.2015
*/

#include "celestrondriver.h"

#include "indicom.h"
#include "indilogger.h"

#include <libnova/julian_day.h>

#include <map>
#include <cstring>
#include <cmath>
#include <termios.h>
#include <unistd.h>

#define CELESTRON_TIMEOUT 5 /* FD timeout in seconds */

// device IDs
#define CELESTRON_DEV_RA  0x10
#define CELESTRON_DEV_DEC 0x11
#define CELESTRON_DEV_GPS 0xb0

char device_str[MAXINDIDEVICE] = "Celestron GPS";


void hex_dump(char *buf, const char *data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", (unsigned char)data[i]);

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

//TODO: check this function
// Limit an angle between -90 and 90 degrees
inline double trimAngle(double angle)
{
    if (angle < -90.0001) angle += 360;
    if (angle > 90.0001) angle -= 360;
    return angle;
}

// Convert decimal degrees to NexStar angle
inline uint16_t dd2nex(double angle)
{
    angle = angle - 360*floor(angle/360);
    if (angle < 0)
        angle += 360.0;

    return (uint16_t)(angle * 0x10000 / 360.0);
}

// Convert decimal degrees to NexStar angle (precise)
inline uint32_t dd2pnex(double angle)
{
    angle = angle - 360*floor(angle/360);
    if (angle < 0)
        angle += 360.0;

    return (uint32_t)(angle * 0x100000000 / 360.0);
}

// Convert NexStar angle to decimal degrees
inline double nex2dd(uint16_t value)
{
    return 360.0 * ((double)value / 0x10000);
}

// Convert NexStar angle to decimal degrees (precise)
inline double pnex2dd(uint32_t value)
{
    return 360.0 * ((double)value / 0x100000000);
}

/*
uint16_t get_az_fraction(double az)
{
    return (uint16_t)(az * 65536 / 360.0) + 0x4000;
}

uint16_t get_alt_fraction(double lat, double alt, double az)
{
    uint16_t alt_int = 0;

    if (alt >= 0)
    {
        // North
        if (az >= 270 || az <= 90)
            alt_int = (uint16_t)((alt - lat) * 65536 / 360.0) + 0x4000;
        else
            alt_int = (uint16_t)((180 - (lat + alt)) * 65536 / 360.0) + 0x4000;
    }
    else
    {
        if (az >= 270 || az <= 90)
            alt_int = (uint16_t)((360 - lat + alt) * 65536 / 360.0) + 0x4000;
        else
            alt_int = (uint16_t)((180 - (lat + alt)) * 65536 / 360.0) + 0x4000;
    }

    return alt_int;
}
*/

void CelestronDriver::set_port_fd(int port_fd)
{
    fd = port_fd;
}
void CelestronDriver::set_debug(bool enable)
{
    debug = enable;
}

void CelestronDriver::set_simulation(bool enable)
{
    simulation = enable;
}

void CelestronDriver::set_device(const char *name)
{
    strncpy(device_str, name, MAXINDIDEVICE);
}

void CelestronDriver::set_sim_gps_status(CELESTRON_GPS_STATUS value)
{
    sim_data.gpsStatus = value;
}

void CelestronDriver::set_sim_slew_rate(CELESTRON_SLEW_RATE value)
{
    sim_data.slewRate = value;
}

void CelestronDriver::set_sim_track_mode(CELESTRON_TRACK_MODE value)
{
    sim_data.trackMode = value;
}

void CelestronDriver::set_sim_slewing(bool isSlewing)
{
    sim_data.isSlewing = isSlewing;
}

void CelestronDriver::set_sim_ra(double ra)
{
    sim_data.ra = ra;
}

double CelestronDriver::get_sim_ra()
{
    return sim_data.ra;
}

void CelestronDriver::set_sim_dec(double dec)
{
    sim_data.dec = dec;
}

double CelestronDriver::get_sim_dec()
{
    return sim_data.dec;
}

void CelestronDriver::set_sim_az(double az)
{
    sim_data.az = az;
}

void CelestronDriver::set_sim_alt(double alt)
{
    sim_data.alt = alt;
}

// Send a command to the mount. Return the number of bytes received or 0 if
// case of error
int CelestronDriver::send_command(const char *cmd, int cmd_len, char *resp, int resp_len)
{
    int err;
    int nbytes = resp_len;
    char errmsg[MAXRBUF];
    char hexbuf[3 * MAX_RESP_SIZE];

    // Some commands must be represented as hex strings when debugging
    if (cmd[0] == 0x50 || cmd[0] == 'W' || cmd[0] == 'H')
    {
        hex_dump(hexbuf, cmd, cmd_len);
        DEBUGFDEVICE(device_str, INDI::Logger::DBG_DEBUG, "CMD <%s>", hexbuf);
    }
    else
        DEBUGFDEVICE(device_str, INDI::Logger::DBG_DEBUG, "CMD <%s>", cmd);

    if (!simulation && fd)
    {
        tcflush(fd, TCIOFLUSH);

        if ((err = tty_write(fd, cmd, cmd_len, &nbytes)) != TTY_OK)
        {
            tty_error_msg(err, errmsg, MAXRBUF);
            DEBUGFDEVICE(device_str, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return 0;
        }

        if (resp_len > 0)
        {
            if ((err = tty_read_section(fd, resp, '#', CELESTRON_TIMEOUT, &nbytes)))
            {
                tty_error_msg(err, errmsg, MAXRBUF);
                DEBUGFDEVICE(device_str, INDI::Logger::DBG_ERROR, "%s", errmsg);
                return 0;
            }

            tcflush(fd, TCIOFLUSH);
        }
    }

    //if (resp_len >= 0 && nbytes != resp_len)
    if (resp_len > 0 && nbytes != resp_len)
    {
        DEBUGFDEVICE(device_str, INDI::Logger::DBG_ERROR,
                     "Received %d bytes, expected %d.", nbytes, resp_len);
        return 0;
    }
    resp[nbytes] = '\0';

    hex_dump(hexbuf, resp, resp_len);
    DEBUGFDEVICE(device_str, INDI::Logger::DBG_DEBUG, "RES <%s>", hexbuf);
    return nbytes;
}

// Send a 'passthrough command' to the mount. Return the number of bytes
// received or 0 in case of error
int CelestronDriver::send_passthrough(int dest, int cmd_id,
                                      const char *payload, int payload_len, char *response, int response_len)
{
    char cmd[8] = {0};

    cmd[0] = 0x50;
    cmd[1] = (char)(payload_len + 1);
    cmd[2] = (char)dest;
    cmd[3] = (char)cmd_id;
    cmd[7] = (char)response_len;

    // payload_len must be <= 3 !
    memcpy(cmd + 4, payload, payload_len);

    return send_command(cmd, 8, response, response_len + 1);
}

bool CelestronDriver::check_connection()
{
    DEBUGDEVICE(device_str, INDI::Logger::DBG_DEBUG, "Initializing Celestron using Kx CMD...");

    for (int i = 0; i < 2; i++) {
        if (echo())
            return true;

        usleep(50000);
    }

    return false;
}

bool CelestronDriver::get_firmware(FirmwareInfo *info)
{
    char version[8], model[16], RAVersion[8], DEVersion[8];

    DEBUGDEVICE(device_str, INDI::Logger::DBG_DEBUG, "Getting controller version...");
    if (!get_version(version, 8))
        return false;
    info->Version = version;
    info->controllerVersion = atof(version);

    DEBUGDEVICE(device_str, INDI::Logger::DBG_DEBUG, "Getting controller variant...");
    info->controllerVariant = ISNEXSTAR;
    get_variant(&(info->controllerVariant));

    if (((info->controllerVariant == ISSTARSENSE) &&
            info->controllerVersion >= MINSTSENSVER) ||
            (info->controllerVersion >= 2.2))
    {
        DEBUGDEVICE(device_str, INDI::Logger::DBG_DEBUG, "Getting controller model...");
        if (!get_model(model, 16))
            return false;
        info->Model = model;
    }
    else
        info->Model = "Unknown";

    //DEBUGDEVICE(device_str, INDI::Logger::DBG_DEBUG, "Getting GPS firmware version...");
    // char GPSVersion[8];
    //if (!get_dev_firmware(CELESTRON_DEV_GPS, GPSVersion, 8))
    //return false;
    //info->GPSFirmware = GPSVersion;
    info->GPSFirmware = "0.0";

    DEBUGDEVICE(device_str, INDI::Logger::DBG_DEBUG, "Getting RA firmware version...");
    if (!get_dev_firmware(CELESTRON_DEV_RA, RAVersion, 8))
        return false;
    info->RAFirmware = RAVersion;

    DEBUGDEVICE(device_str, INDI::Logger::DBG_DEBUG, "Getting DEC firmware version...");
    if (!get_dev_firmware(CELESTRON_DEV_DEC, DEVersion, 8))
        return false;
    info->DEFirmware = DEVersion;

    return true;
}

bool CelestronDriver::echo()
{
    strcpy(response, "x#");  // Simulated response

    if (!send_command("Kx", 2, response, 2))
        return false;

    return !strcmp(response, "x#");
}

bool CelestronDriver::get_version(char *version, int size)
{
    strcpy(response, "\x04\x04#");  // Simulated response

    if (!send_command("V", 1, response, 3))
        return false;

    snprintf(version, size, "%d.%02d", response[0], response[1]);

    DEBUGFDEVICE(device_str, INDI::Logger::DBG_SESSION, "Controller version: %s", version);
    return true;
}

//TODO: no critical errors for this command
bool CelestronDriver::get_variant(char *variant)
{
    strcpy(response, "\x11#");  // Simulated response

    if (!send_command("v", 1, response, 2))
        return false;

    *variant = response[0];
    return true;
}

bool CelestronDriver::get_model(char *model, int size)
{
    std::map<int, std::string> models =
    {
        {1, "GPS Series"},
        {3, "i-Series"},
        {4, "i-Series SE"},
        {5, "CGE"},
        {6, "Advanced GT"},
        {7, "SLT"},
        {9, "CPC"},
        {10, "GT"},
        {11, "4/5 SE"},
        {12, "6/8 SE"},
        {13, "CGE Pro"},
        {14, "CGEM DX"},
        {20, "AVX"},
    };

    strcpy(response, "\x06#");  // Simulated response

    if (!send_command("m", 1, response, 2))
        return false;

    int m = response[0];

    if (models.find(m) != models.end())
    {
        strncpy(model, models[m].c_str(), size);
        DEBUGFDEVICE(device_str, INDI::Logger::DBG_SESSION, "Mount model: %s", model);
    }
    else
    {
        strncpy(model, "Unknown", size);
        DEBUGFDEVICE(device_str, INDI::Logger::DBG_WARNING, "Unrecognized model (%d).", model);
    }
    return true;
}

bool CelestronDriver::get_dev_firmware(int dev, char *version, int size)
{
    strcpy(response, "\x01\x09#");

    int rlen = send_passthrough(dev, 0xfe, NULL, 0, response, 2);

    if (rlen == 3)
        snprintf(version, size, "%d.%02d", response[0], response[1]);
    else if (rlen == 2) // some GPS models return only 2 bytes
        snprintf(version, size, "%01d.0", response[0]);
    else
        return false;

    return true;
}

/*****************************************************************
    PulseGuide commands, experimental
******************************************************************/

/*****************************************************************
    Send a guiding pulse to the  mount in direction "dir".
    "rate" should be a signed 8-bit integer in the range (-100,100) that
    represents the pulse velocity in % of sidereal.
    "duration_csec" is an unsigned  8-bit integer (0,255) with  the pulse
    duration in centiseconds (i.e. 1/100 s  =  10ms).
    The max pulse duration is 2550 ms.
******************************************************************/
int CelestronDriver::send_pulse(CELESTRON_DIRECTION dir, signed char rate, unsigned char duration_csec)
{
    int dev = (dir == CELESTRON_N || dir == CELESTRON_S) ? CELESTRON_DEV_DEC : CELESTRON_DEV_RA;
    char payload[2];
    payload[0] = (dir == CELESTRON_N || dir == CELESTRON_W) ? rate : -rate;
    payload[1] = duration_csec;

    strcpy(response, "#");  // Simulated response
    return send_passthrough(dev, 0x26, payload, 2, response, 1);
}

/*****************************************************************
    Send the guiding pulse status check command to the mount for the motor
    responsible for "dir". If  a pulse is being executed, "pulse_state" is set
    to 1, whereas if the pulse motion has been  completed it is set to 0.
    Return "false" if the status command fails, otherwise return "true".
******************************************************************/
int CelestronDriver::get_pulse_status(CELESTRON_DIRECTION dir, bool &pulse_state)
{
    int dev = (dir == CELESTRON_N || dir == CELESTRON_S) ? CELESTRON_DEV_DEC : CELESTRON_DEV_RA;
    char payload[2] = {0, 0};

    strcpy(response, "#");  // Simulated response
    if (!send_passthrough(dev, 0x27, payload, 2, response, 1))
        return false;

    pulse_state = (bool)response[0];
    return true;
}

bool CelestronDriver::start_motion(CELESTRON_DIRECTION dir, CELESTRON_SLEW_RATE rate)
{
    int dev = (dir == CELESTRON_N || dir == CELESTRON_S) ? CELESTRON_DEV_DEC : CELESTRON_DEV_RA;
    int cmd_id = (dir == CELESTRON_N || dir == CELESTRON_W) ? 0x24 : 0x25;
    char payload[1];
    payload[0] = rate + 1;

    strcpy(response, "#");  // Simulated response
    return send_passthrough(dev, cmd_id, payload, 1, response, 1);
}

bool CelestronDriver::stop_motion(CELESTRON_DIRECTION dir)
{
    int dev = (dir == CELESTRON_N || dir == CELESTRON_S) ? CELESTRON_DEV_DEC : CELESTRON_DEV_RA;
    char payload[] = { 0 };

    strcpy(response, "#");  // Simulated response
    return send_passthrough(dev, 0x24, payload, 1, response, 1);
}

bool CelestronDriver::abort()
{
    strcpy(response, "#");  // Simulated response
    return send_command("M", 1, response, 1);
}

//TODO: check response length ("Requested object is below horizon")
bool CelestronDriver::slew_radec(double ra, double dec, bool precise)
{
    char RAStr[16], DecStr[16];
    fs_sexa(RAStr, ra, 2, 3600);
    fs_sexa(DecStr, dec, 2, 3600);

    DEBUGFDEVICE(device_str, INDI::Logger::DBG_DEBUG, "Goto (%s,%s)", RAStr, DecStr);

    set_sim_slewing(true);

    char cmd[20];
    if (precise)
        sprintf(cmd, "r%08X,%08X", dd2pnex(ra*15), dd2pnex(dec));
    else
        sprintf(cmd, "R%04X,%04X", dd2nex(ra*15), dd2nex(dec));

    strcpy(response, "#");  // Simulated response
    return send_command(cmd, strlen(cmd), response, 1);
}

//TODO: check response length ("Requested object is below horizon")
bool CelestronDriver::slew_azalt(double az, double alt, bool precise)
{
    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, az, 3, 3600);
    fs_sexa(AltStr, alt, 2, 3600);

    DEBUGFDEVICE(device_str, INDI::Logger::DBG_DEBUG, "Goto AZM-ALT (%s,%s)", AzStr, AltStr);

    set_sim_slewing(true);

    char cmd[20];
    if (precise)
        sprintf(cmd, "b%08X,%08X", dd2pnex(az), dd2pnex(alt));
    else
        sprintf(cmd, "B%04X,%04X", dd2nex(az), dd2nex(alt));

    strcpy(response, "#");  // Simulated response
    return send_command(cmd, strlen(cmd), response, 1);
}

//TODO: check response length ("Requested object is below horizon")
bool CelestronDriver::sync(double ra, double dec, bool precise)
{
    char RAStr[16], DecStr[16];
    fs_sexa(RAStr, ra, 2, 3600);
    fs_sexa(DecStr, dec, 2, 3600);

    DEBUGFDEVICE(device_str, INDI::Logger::DBG_DEBUG, "Sync (%s,%s)", RAStr, DecStr);

    sim_data.ra  = ra;
    sim_data.dec = dec;

    char cmd[20];
    if (precise)
        sprintf(cmd, "s%08X,%08X", dd2pnex(ra*15), dd2pnex(dec));
    else
        sprintf(cmd, "S%04X,%04X", dd2nex(ra*15), dd2nex(dec));

    strcpy(response, "#");  // Simulated response
    return send_command(cmd, 10, response, 1);
}

void parseCoordsResponse(char *response, double *d1, double *d2, bool precise)
{
    uint32_t d1_int = 0, d2_int = 0;

    sscanf(response, "%x,%x#", &d1_int, &d2_int);

    if (precise)
    {
        *d1 = pnex2dd(d1_int);
        *d2 = pnex2dd(d2_int);
    }
    else
    {
        *d1 = nex2dd(d1_int);
        *d2 = nex2dd(d2_int);
    }
}

bool CelestronDriver::get_radec(double *ra, double *dec, bool precise)
{
    if (precise)
    {
        if (simulation)
            sprintf(response, "%08X,%08X#", dd2pnex(sim_data.ra*15), dd2pnex(sim_data.dec));

        if (!send_command("e", 1, response, 18))
            return false;
    }
    else
    {
        if (simulation)
            sprintf(response, "%04X,%04X#", dd2nex(sim_data.ra*15), dd2nex(sim_data.dec));

        if (!send_command("E", 1, response, 10))
            return false;
    }

    parseCoordsResponse(response, ra, dec, precise);
    *ra /= 15.0;
    *dec = trimAngle(*dec);

    char RAStr[16], DecStr[16];
    fs_sexa(RAStr, *ra, 2, 3600);
    fs_sexa(DecStr, *dec, 2, 3600);

    DEBUGFDEVICE(device_str, INDI::Logger::DBG_EXTRA_1, "RA-DEC (%s,%s)", RAStr, DecStr);
    return true;
}

//TODO: simulation not implemented
bool CelestronDriver::get_azalt(double *az, double *alt, bool precise)
{
    if (precise)
    {
        //if (simulation)
        //sprintf(response, "%08X,%08X#", dd2pnex(sim_data.az), dd2pnex(sim_data.alt));

        if (!send_command("z", 1, response, 18))
            return false;
    }
    else
    {
        //if (simulation)
        //sprintf(response, "%04X,%04X#", dd2nex(sim_data.az), dd2nex(sim_data.alt));

        if (!send_command("Z", 1, response, 10))
            return false;
    }

    parseCoordsResponse(response, az, alt, precise);

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, *az, 3, 3600);
    fs_sexa(AltStr, *alt, 2, 3600);
    DEBUGFDEVICE(device_str, INDI::Logger::DBG_EXTRA_1,
                 "RES <%s> ==> AZM-ALT (%s,%s)", response, AzStr, AltStr);
    return true;
}

bool CelestronDriver::set_location(double longitude, double latitude)
{
    // Convert from INDI standard to regular east/west -180 to 180
    if (longitude > 180)
        longitude -= 360;

    int lat_d, lat_m, lat_s;
    int long_d, long_m, long_s;
    getSexComponents(latitude, &lat_d, &lat_m, &lat_s);
    getSexComponents(longitude, &long_d, &long_m, &long_s);

    char cmd[9];
    cmd[0] = 'W';
    cmd[1] = abs(lat_d);
    cmd[2] = lat_m;
    cmd[3] = lat_s;
    cmd[4] = lat_d > 0 ? 0 : 1;
    cmd[5] = abs(long_d);
    cmd[6] = long_m;
    cmd[7] = long_s;
    cmd[8] = long_d > 0 ? 0 : 1;

    strcpy(response, "#");  // Simulated response
    return send_command(cmd, 9, response, 1);
}

bool CelestronDriver::set_datetime(struct ln_date *utc, double utc_offset)
{
    struct ln_zonedate local_date;

    // Celestron takes local time
    ln_date_to_zonedate(utc, &local_date, utc_offset * 3600);

    char cmd[9];
    cmd[0] = 'H';
    cmd[1] = local_date.hours;
    cmd[2] = local_date.minutes;
    cmd[3] = local_date.seconds;
    cmd[4] = local_date.months;
    cmd[5] = local_date.days;
    cmd[6] = local_date.years - 2000;

    if (utc_offset < 0)
        cmd[7] = 256 - ((uint16_t)fabs(utc_offset));
    else
        cmd[7] = ((uint16_t)fabs(utc_offset));

    // Always assume standard time
    cmd[8] = 0;

    strcpy(response, "#");  // Simulated response
    return send_command(cmd, 9, response, 1);
}

bool CelestronDriver::get_utc_date_time(double *utc_hours, int *yy, int *mm,
                                        int *dd, int *hh, int *minute, int *ss)
{
    // Simulated response (HH MM SS MONTH DAY YEAR OFFSET DAYLIGHT)
    char sim_resp[] = {17, 30, 10, 4, 1, 15, 3, 0, '#'};
    strcpy(response, sim_resp);

    if (!send_command("h", 1, response, 9))
        return false;

    // HH MM SS MONTH DAY YEAR OFFSET DAYLIGHT
    *hh        = response[0];
    *minute    = response[1];
    *ss        = response[2];
    *mm        = response[3];
    *dd        = response[4];
    *yy        = response[5] + 2000;
    *utc_hours = response[6];

    if (*utc_hours > 12)
        *utc_hours -= 256;

    ln_zonedate localTime;
    ln_date utcTime;

    localTime.years   = *yy;
    localTime.months  = *mm;
    localTime.days    = *dd;
    localTime.hours   = *hh;
    localTime.minutes = *minute;
    localTime.seconds = *ss;
    localTime.gmtoff  = *utc_hours * 3600;

    ln_zonedate_to_date(&localTime, &utcTime);

    *yy     = utcTime.years;
    *mm     = utcTime.months;
    *dd     = utcTime.days;
    *hh     = utcTime.hours;
    *minute = utcTime.minutes;
    *ss     = utcTime.seconds;

    return true;
}

bool CelestronDriver::is_slewing()
{
    sprintf(response, "%d#", sim_data.isSlewing);    // Simulated response

    if (!send_command("L", 1, response, 2))
        return false;

    return response[0] != '0';
}

bool CelestronDriver::get_track_mode(CELESTRON_TRACK_MODE *mode)
{
    strcpy(response, "\02#");   // Simulated response

    if (!send_command("t", 1, response, 2))
        return false;

    *mode = ((CELESTRON_TRACK_MODE)response[0]);
    return true;
}

bool CelestronDriver::set_track_mode(CELESTRON_TRACK_MODE mode)
{
    char cmd[3];
    sprintf(cmd, "T%c", mode);
    strcpy(response, "#");  // Simulated response

    return send_command(cmd, 2, response, 1);
}

bool CelestronDriver::hibernate()
{
    return send_command("x#", 2, response, 0);
}

bool CelestronDriver::wakeup()
{
    strcpy(response, "#");  // Simulated response
    return send_command("y#", 2, response, 1);
}
