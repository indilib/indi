/*
    Celestron driver

    Copyright (C) 2015 Jasem Mutlaq
    Copyright (C) 2017 Juan Menendez

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

#include "indicom.h"
#include "indilogger.h"
#include "celestrondriver.h"

#include <libnova/julian_day.h>

#include <map>
#include <cstring>
#include <cmath>
#include <termios.h>
#include <unistd.h>

#define CELESTRON_TIMEOUT 5 /* FD timeout in seconds */

using namespace Celestron;

static char device_str[MAXINDIDEVICE] = "Celestron GPS";

// Account for the quadrant in declination
double Celestron::trimDecAngle(double angle)
{
    angle = angle - 360 * floor(angle / 360);
    if (angle < 0)
        angle += 360.0;

    if ((angle > 90.) && (angle <= 270.))
        angle = 180. - angle;
    else if ((angle > 270.) && (angle <= 360.))
        angle = angle - 360.;

    return angle;
}

// Convert decimal degrees to NexStar angle
uint16_t Celestron::dd2nex(double angle)
{
    angle = angle - 360 * floor(angle / 360);
    if (angle < 0)
        angle += 360.0;

    return static_cast<uint16_t>(angle * 0x10000 / 360.0);
}

// Convert decimal degrees to NexStar angle (precise)
uint32_t Celestron::dd2pnex(double angle)
{
    angle = angle - 360 * floor(angle / 360);
    if (angle < 0)
        angle += 360.0;

    return static_cast<uint32_t>(angle * 0x100000000 / 360.0);
}

// Convert NexStar angle to decimal degrees
double Celestron::nex2dd(uint32_t value)
{
    return 360.0 * (static_cast<double>(value) / 0x10000);
}

// Convert NexStar angle to decimal degrees (precise)
double Celestron::pnex2dd(uint32_t value)
{
    return 360.0 * (static_cast<double>(value) / 0x100000000);
}

void hex_dump(char *buf, const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", data[i]);

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

// This method is required by the logging macros
const char *CelestronDriver::getDeviceName()
{
    return device_str;
}

void CelestronDriver::set_device(const char *name)
{
    strncpy(device_str, name, MAXINDIDEVICE);
}

// Virtual method for testing
int CelestronDriver::serial_write(const char *cmd, int nbytes, int *nbytes_written)
{
    tcflush(fd, TCIOFLUSH);
    return tty_write(fd, cmd, nbytes, nbytes_written);
}

// Virtual method for testing
int CelestronDriver::serial_read(int nbytes, int *nbytes_read)
{
    return tty_read(fd, response, nbytes, CELESTRON_TIMEOUT, nbytes_read);
}

// Virtual method for testing
int CelestronDriver::serial_read_section(char stop_char, int *nbytes_read)
{
    return tty_nread_section(fd, response, MAX_RESP_SIZE, stop_char, CELESTRON_TIMEOUT, nbytes_read);
}

// Set the expected response for a command in simulation mode
__attribute__((__format__ (__printf__, 2, 0)))
void CelestronDriver::set_sim_response(const char *fmt, ...)
{
    if (simulation)
    {
        va_list args;
        va_start(args, fmt);
        vsprintf(response, fmt, args);
        va_end(args);
    }
}

//static std::mutex tty_lock;

// Send a command to the mount. Return the number of bytes received or 0 if
// case of error
size_t CelestronDriver::send_command(const char *cmd, size_t cmd_len, char *resp,
                                     size_t resp_len, bool ascii_cmd, bool ascii_resp)
{
    int err;
    size_t nbytes = resp_len;
    char errmsg[MAXRBUF];
    char hexbuf[3 * MAX_RESP_SIZE];

    if (ascii_cmd)
        LOGF_DEBUG("CMD <%s>", cmd);
    else
    {
        // Non-ASCII commands should be represented as hex strings
        hex_dump(hexbuf, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hexbuf);
    }

    if (!simulation && fd)
    {
        // lock the serial command, unlocks when this goes out of scope
        //std::lock_guard<std::mutex> lockGuard(tty_lock);
        if ((err = serial_write(cmd, static_cast<int>(cmd_len), reinterpret_cast<int *>(&nbytes))) != TTY_OK)
        {
            tty_error_msg(err, errmsg, MAXRBUF);
            LOGF_ERROR("Serial write error: %s", errmsg);
            return 0;
        }

        if (resp_len > 0)
        {
            if (ascii_resp)
                err = serial_read_section('#', reinterpret_cast<int *>(&nbytes));
            else
            {
                err = serial_read(static_cast<int>(resp_len), reinterpret_cast<int *>(&nbytes));
                // passthrough commands that fail will return an extra 0 then the terminator
                while (err == TTY_OK && resp[nbytes - 1] != '#')
                {
                    char m[1];
                    int n;
                    err = tty_read(fd, m, 1, CELESTRON_TIMEOUT, &n);
                    if (n == 1)
                    {
                        resp[nbytes] = m[0];
                        nbytes++;
                    }
                }
            }
            if (err)
            {
                tty_error_msg(err, errmsg, MAXRBUF);
                LOGF_ERROR("Serial read error: %s", errmsg);
                return 0;
            }
        }
    }

    if (nbytes != resp_len)
    {
        size_t max = nbytes > resp_len ? nbytes : resp_len;
        hex_dump(hexbuf, resp, max);
        LOGF_DEBUG("Received %d bytes, expected %d <%s>", nbytes, resp_len, hexbuf);
        return max;
    }

    if (resp_len == 0)
    {
        LOG_DEBUG("resp_len 0, no response expected");
        return true;
    }

    resp[nbytes] = '\0';

    if (ascii_resp)
        LOGF_DEBUG("RES <%s>", resp);
    else
    {
        // Non-ASCII commands should be represented as hex strings
        hex_dump(hexbuf, resp, resp_len);
        LOGF_DEBUG("RES <%s>", hexbuf);
    }

    return nbytes;
}

// Send a 'passthrough command' to the mount. Return the number of bytes
// received or 0 in case of error
size_t CelestronDriver::send_passthrough(int dest, int cmd_id, const char *payload,
        size_t payload_len, char *resp, size_t response_len)
{
    char cmd[8] = {0};

    cmd[0] = 0x50;
    cmd[1] = static_cast<char>(payload_len + 1);
    cmd[2] = static_cast<char>(dest);
    cmd[3] = static_cast<char>(cmd_id);
    cmd[7] = static_cast<char>(response_len);

    // payload_len must be <= 3 !
    memcpy(cmd + 4, payload, payload_len);

    return send_command(cmd, 8, resp, response_len + 1, false, false);
}

bool CelestronDriver::check_connection()
{
    LOG_DEBUG("Initializing Celestron using Kx CMD...");

    for (int i = 0; i < 2; i++)
    {
        if (echo())
            return true;

        usleep(50000);
    }

    return false;
}

bool CelestronDriver::get_firmware(FirmwareInfo *info)
{
    char version[8], model[16], RAVersion[8], DEVersion[8];
    bool isGem;
    bool canPec;

    LOG_DEBUG("Getting controller version...");
    if (!get_version(version, 8))
        return false;
    info->Version = version;
    info->controllerVersion = atof(version);

    LOG_DEBUG("Getting controller variant...");
    info->controllerVariant = ISNEXSTAR;
    // variant is only available for NexStar + versions 5.28 or more and Starsense.
    // StarSense versions are currently 1.9 so overlap the early NexStar versions.
    // NS HCs before 2.0 will test and timeout
    if (info->controllerVersion < 2.2 || info->controllerVersion >= 5.28)
    {
        get_variant(&(info->controllerVariant));
    }

    if (((info->controllerVariant == ISSTARSENSE) &&
            info->controllerVersion >= MINSTSENSVER) ||
            (info->controllerVersion >= 2.2))
    {
        LOG_DEBUG("Getting controller model...");
        if (!get_model(model, 16, &isGem, &canPec))
            return false;
        info->Model = model;
        info->isGem = isGem;
        info->canPec = canPec;
    }
    else
    {
        info->Model = "Unknown";
        info->isGem = false;
        info->canPec = false;
    }

    //LOG_DEBUG("Getting GPS firmware version...");
    // char GPSVersion[8];
    //if (!get_dev_firmware(CELESTRON_DEV_GPS, GPSVersion, 8))
    //return false;
    //info->GPSFirmware = GPSVersion;
    //info->GPSFirmware = "0.0";

    LOG_DEBUG("Getting RA firmware version...");
    if (!get_dev_firmware(CELESTRON_DEV_RA, RAVersion, 8))
        return false;
    info->RAFirmware = RAVersion;

    LOG_DEBUG("Getting DEC firmware version...");
    if (!get_dev_firmware(CELESTRON_DEV_DEC, DEVersion, 8))
        return false;
    info->DEFirmware = DEVersion;


    LOG_DEBUG("Getting focuser version...");
    info->hasFocuser = foc_exists();

    LOGF_DEBUG("Firmware Info HC Ver %s model %s %s %s mount, HW Ver %s",
               info->Version.c_str(),
               info->Model.c_str(),
               info->controllerVariant == ISSTARSENSE ? "StarSense" : "NexStar",
               info->isGem ? "GEM" : "Fork",
               info->RAFirmware.c_str());

    return true;
}

bool CelestronDriver::echo()
{
    set_sim_response("x#");

    if (!send_command("Kx", 2, response, 2, true, true))
        return false;

    return !strcmp(response, "x#");
}

bool CelestronDriver::get_version(char *version, size_t size)
{
    set_sim_response("\x04\x29#");

    if (!send_command("V", 1, response, 3, true, false))
        return false;

    snprintf(version, size, "%d.%02d", static_cast<uint8_t>(response[0]), static_cast<uint8_t>(response[1]));

    LOGF_INFO("Controller version: %s", version);
    return true;
}

//TODO: no critical errors for this command
bool CelestronDriver::get_variant(char *variant)
{
    set_sim_response("\x11#");

    if (!send_command("v", 1, response, 2, true, false))
        return false;

    *variant = static_cast<uint8_t>(response[0]);
    return true;
}

int CelestronDriver::model()
{
    set_sim_response("%c#", 20);    // AVX
    if (!send_command("m", 1, response, 2, true, false))
        return -1;

    return static_cast<uint8_t>(response[0]);
}

bool CelestronDriver::get_model(char *model, size_t size, bool *isGem, bool *canPec)
{
    // extended list of mounts
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
        {15, "LCM"},
        {16, "Sky Prodigy"},
        {17, "CPC Deluxe"},
        {18, "GT 16"},
        {19, "StarSeeker"},
        {20, "AVX"},
        {21, "Cosmos"},
        {22, "Evolution"},
        {23, "CGX"},
        {24, "CGXL"},
        {25, "Astrofi"},
        {26, "SkyWatcher"},
    };

    set_sim_response("\x14#");  // Simulated response, AVX


    int m = CelestronDriver::model();
    if (m < 0)
        return false;

    if (models.find(m) != models.end())
    {
        strncpy(model, models[m].c_str(), size);
        LOGF_INFO("Mount model: %s", model);
    }
    else
    {
        strncpy(model, "Unknown", size);
        LOGF_WARN("Unrecognized model (%d).", model);
    }

    // use model# to detect the GEMs amd if PEC can be done
    // Only Gem mounts can report the pier side pointing state
    switch(m)
    {
    // fork mounts with PEC index
    case 1:     // GPS
    case 9:     // CPC
    case 17:    // CPC Deluxe
    case 22:    // Evolution
        *isGem = false;
        *canPec = true;
        break;

    // GEM with no PEC index
    case 6:     // AS-GT
        *isGem = true;
        *canPec = false;
        break;

    // GEM with PEC
    case 5:     // CGE
    case 13:    // CGE 2
    case 14:    // EQ6
    case 20:    // AVX
    case 23:    // CGX
    case 24:    // CGXL
        *isGem = true;
        *canPec = true;
        break;

    // the rest are fork mounte with no PEC
    default:
        *isGem = false;
        *canPec = false;
        break;
    }

    LOGF_DEBUG("get_model %s, %s mount, %s", model, *isGem ? "GEM" : "Fork", *canPec ? "has PEC" : "no PEC");

    return true;
}

bool CelestronDriver::get_dev_firmware(int dev, char *version, size_t size)
{
    set_sim_response("\x06\x10#");

    size_t rlen = send_passthrough(dev, GET_VER, nullptr, 0, response, 2);

    switch (rlen)
    {
        case 2:
            snprintf(version, size, "%01d.0", static_cast<uint8_t>(response[0]));
            break;
        case 3:
            snprintf(version, size, "%d.%02d", static_cast<uint8_t>(response[0]), static_cast<uint8_t>(response[1]));
            break;
        default:
            return false;
    }

    return true;
}

/*****************************************************************
    PulseGuide commands
******************************************************************/

/*****************************************************************
    Send a guiding pulse to the  mount in direction "dir".
    "rate" should be an unsigned 8-bit integer in the range (0,100) that
    represents the pulse velocity in % of sidereal.
    "duration_csec" is an unsigned  8-bit integer (0,255) with  the pulse
    duration in centiseconds (i.e. 1/100 s  =  10ms).
    The max pulse duration is 2550 ms.
*******************************************)()***********************/
size_t CelestronDriver::send_pulse(CELESTRON_DIRECTION dir, unsigned char rate, unsigned char duration_csec)
{
    char payload[2];
    int dev = CELESTRON_DEV_RA;
    switch (dir)
    {
    case CELESTRON_N:
        dev = CELESTRON_DEV_DEC;
        payload[0] = rate;
        break;
    case CELESTRON_S:
        dev = CELESTRON_DEV_DEC;
        payload[0] = -rate;
        break;
    case CELESTRON_W:
        dev = CELESTRON_DEV_RA;
        payload[0] = rate;
        break;
    case CELESTRON_E:
        dev = CELESTRON_DEV_RA;
        payload[0] = -rate;
        break;
    }
    payload[1] = duration_csec;

    set_sim_response("#");
    return send_passthrough(dev, MTR_AUX_GUIDE, payload, 2, response, 0);
}

/*****************************************************************
    Send the guiding pulse status check command to the mount for the motor
    responsible for "dir". If  a pulse is being executed, returns true,
    otherwise false.
    If the getting the status fails returns false.
******************************************************************/
bool CelestronDriver::get_pulse_status(CELESTRON_DIRECTION dir)
{
    int dev = CELESTRON_DEV_RA;
    //char payload[2] = {0, 0};
    switch (dir)
    {
    case CELESTRON_N:
    case CELESTRON_S:
        dev = CELESTRON_DEV_DEC;
        break;
    case CELESTRON_W:
    case CELESTRON_E:
        dev = CELESTRON_DEV_RA;
        break;
    }
    set_sim_response("%c#", 0);

    if (!send_passthrough(dev, MTR_IS_AUX_GUIDE_ACTIVE, nullptr, 0, response, 1))
        return false;

    return static_cast<bool>(response[0]);
}

/*****************************************************************
    Get the guide rate from the mount for the axis.
    rate is 0 to 255 representing 0 to 100% sidereal
    If getting the rate fails returns false.
******************************************************************/
bool CelestronDriver::get_guide_rate(CELESTRON_AXIS axis, uint8_t * rate)
{
    int dev = (axis == CELESTRON_AXIS::DEC_AXIS) ? CELESTRON_DEV_DEC : CELESTRON_DEV_RA;
    //char payload[2] = {0, 0};
    set_sim_response("%c#", (axis == CELESTRON_AXIS::DEC_AXIS) ? sim_dec_guide_rate : sim_ra_guide_rate);

    if (!send_passthrough(dev, MC_GET_AUTOGUIDE_RATE, nullptr, 0, response, 1))
        return false;

    *rate = response[0];
    return true;
}

/*****************************************************************
    Set the guide rate for the axis.
    rate is 0 to 255 representing 0 to 100% sidereal
    If setting the rate fails returns false.
******************************************************************/
bool CelestronDriver::set_guide_rate(CELESTRON_AXIS axis, uint8_t rate)
{
    int dev = CELESTRON_DEV_DEC;
    switch (axis)
    {
    case CELESTRON_AXIS::RA_AXIS:
        dev = CELESTRON_DEV_RA;
        sim_ra_guide_rate = rate;
        break;
    case CELESTRON_AXIS::DEC_AXIS:
        dev = CELESTRON_DEV_DEC;
        sim_dec_guide_rate = rate;
        break;
    }
    char payload[1];
    payload[0] = rate;
    set_sim_response("#");
    return send_passthrough(dev, MC_SET_AUTOGUIDE_RATE, payload, 1, response, 0);
}

bool CelestronDriver::start_motion(CELESTRON_DIRECTION dir, CELESTRON_SLEW_RATE rate)
{
    int dev = (dir == CELESTRON_N || dir == CELESTRON_S) ? CELESTRON_DEV_DEC : CELESTRON_DEV_RA;
    int cmd_id = (dir == CELESTRON_N || dir == CELESTRON_W) ? MC_MOVE_POS : MC_MOVE_NEG;
    char payload[1];
    payload[0] = rate + 1;

    set_sim_response("#");
    return send_passthrough(dev, cmd_id, payload, 1, response, 0);
}

bool CelestronDriver::stop_motion(CELESTRON_DIRECTION dir)
{
    int dev = (dir == CELESTRON_N || dir == CELESTRON_S) ? CELESTRON_DEV_DEC : CELESTRON_DEV_RA;
    char payload[] = { 0 };

    set_sim_response("#");
    return send_passthrough(dev, MC_MOVE_POS, payload, 1, response, 0);
}

bool CelestronDriver::abort()
{
    set_sim_response("#");
    return send_command("M", 1, response, 1, true, true);
}

bool CelestronDriver::slew_radec(double ra, double dec, bool precise)
{
    char RAStr[16], DecStr[16];
    fs_sexa(RAStr, ra, 2, 3600);
    fs_sexa(DecStr, dec, 2, 3600);

    LOGF_DEBUG("Goto RA-DEC(%s,%s)", RAStr, DecStr);

    set_sim_slewing(true);

    char cmd[20];
    if (precise)
        sprintf(cmd, "r%08X,%08X", dd2pnex(ra * 15), dd2pnex(dec));
    else
        sprintf(cmd, "R%04X,%04X", dd2nex(ra * 15), dd2nex(dec));

    set_sim_response("#");
    return send_command(cmd, strlen(cmd), response, 1, true, true);
}

bool CelestronDriver::slew_azalt(double az, double alt, bool precise)
{
    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, az, 3, 3600);
    fs_sexa(AltStr, alt, 2, 3600);

    LOGF_DEBUG("Goto AZM-ALT (%s,%s)", AzStr, AltStr);

    set_sim_slewing(true);

    char cmd[20];
    if (precise)
        sprintf(cmd, "b%08X,%08X", dd2pnex(az), dd2pnex(alt));
    else
        sprintf(cmd, "B%04X,%04X", dd2nex(az), dd2nex(alt));

    set_sim_response("#");
    return send_command(cmd, strlen(cmd), response, 1, true, true);
}

bool CelestronDriver::sync(double ra, double dec, bool precise)
{
    char RAStr[16], DecStr[16];
    fs_sexa(RAStr, ra, 2, 3600);
    fs_sexa(DecStr, dec, 2, 3600);

    LOGF_DEBUG("Sync (%s,%s)", RAStr, DecStr);

    sim_data.ra  = ra;
    sim_data.dec = dec;

    char cmd[20];
    if (precise)
        sprintf(cmd, "s%08X,%08X", dd2pnex(ra * 15), dd2pnex(dec));
    else
        sprintf(cmd, "S%04X,%04X", dd2nex(ra * 15), dd2nex(dec));

    set_sim_response("#");
    return send_command(cmd, strlen(cmd), response, 1, true, true);
}

// NS+ 5.28 and more only, not StarSense
bool CelestronDriver::unsync()
{
    LOG_DEBUG("Unsync");
    set_sim_response("#");
    return send_command("u", 1, response, 1, true, true);
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
        set_sim_response("%08X,%08X#", dd2pnex(sim_data.ra * 15), dd2pnex(sim_data.dec));

        if (!send_command("e", 1, response, 18, true, true))
            return false;
    }
    else
    {
        set_sim_response("%04X,%04X#", dd2nex(sim_data.ra * 15), dd2nex(sim_data.dec));

        if (!send_command("E", 1, response, 10, true, true))
            return false;
    }

    parseCoordsResponse(response, ra, dec, precise);
    *ra /= 15.0;
    *dec = trimDecAngle(*dec);

    char RAStr[16], DecStr[16];
    fs_sexa(RAStr, *ra, 2, 3600);
    fs_sexa(DecStr, *dec, 2, 3600);

    LOGF_EXTRA1("RA-DEC (%s,%s)", RAStr, DecStr);
    return true;
}

bool CelestronDriver::get_azalt(double *az, double *alt, bool precise)
{
    if (precise)
    {
        set_sim_response("%08X,%08X#", dd2pnex(sim_data.az), dd2pnex(sim_data.alt));

        if (!send_command("z", 1, response, 18, true, true))
            return false;
    }
    else
    {
        set_sim_response("%04X,%04X#", dd2nex(sim_data.az), dd2nex(sim_data.alt));

        if (!send_command("Z", 1, response, 10, true, true))
            return false;
    }

    parseCoordsResponse(response, az, alt, precise);

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, *az, 3, 3600);
    fs_sexa(AltStr, *alt, 2, 3600);
    LOGF_EXTRA1("RES <%s> ==> AZM-ALT (%s,%s)", response, AzStr, AltStr);
    return true;
}

bool CelestronDriver::set_location(double longitude, double latitude)
{
    LOGF_DEBUG("Setting location (%.3f,%.3f)", longitude, latitude);

    // Convert from INDI standard to regular east/west -180 to 180
    if (longitude > 180)
        longitude -= 360;

    int lat_d, lat_m, lat_s;
    int long_d, long_m, long_s;
    getSexComponents(latitude, &lat_d, &lat_m, &lat_s);
    getSexComponents(longitude, &long_d, &long_m, &long_s);

    char cmd[9];
    cmd[0] = 'W';
    cmd[1] = static_cast<char>(abs(lat_d));
    cmd[2] = static_cast<char>(lat_m);
    cmd[3] = static_cast<char>(lat_s);
    cmd[4] = lat_d > 0 ? 0 : 1;
    cmd[5] = static_cast<char>(abs(long_d));       // not sure how the conversion from int to char will work for longtitudes > 127
    cmd[6] = static_cast<char>(long_m);
    cmd[7] = static_cast<char>(long_s);
    cmd[8] = long_d > 0 ? 0 : 1;

    set_sim_response("#");
    return send_command(cmd, 9, response, 1, false, true);
}

bool CelestronDriver::get_location(double *longitude, double *latitude)
{
    // Simulated response (lat_d lat_m lat_s N|S long_d long_m long_s E|W)
    set_sim_response("%c%c%c%c%c%c%c%c#", 51, 36, 17, 0, 0, 43, 3, 1);

    if (!send_command("w", 1, response, 9, true, false))
        return false;

    *latitude = response[0];
    *latitude += response[1] / 60.0;
    *latitude += response[2] / 3600.0;
    if (response[3] != 0)
        *latitude = -*latitude;

    *longitude = response[4];
    *longitude += response[5] / 60.0;
    *longitude += response[6] / 3600.0;
    if(response[7] != 0)
        *longitude = -*longitude;

    // convert longitude to INDI range 0 to 359.999
    if (*longitude < 0)
        *longitude += 360.0;

    return true;
}

// there are newer time commands that have the utc offset in 15 minute increments
bool CelestronDriver::set_datetime(struct ln_date *utc, double utc_offset, bool dst, bool precise)
{
    struct ln_zonedate local_date;

    // Celestron takes local time and DST but ln_zonedate doesn't have DST
    ln_date_to_zonedate(utc, &local_date, static_cast<int>(utc_offset * 3600));

    char cmd[9];
    cmd[0] = 'H';
    cmd[1] = static_cast<char>(local_date.hours);
    cmd[2] = static_cast<char>(local_date.minutes);
    cmd[3] = static_cast<char>(local_date.seconds);
    cmd[4] = static_cast<char>(local_date.months);
    cmd[5] = static_cast<char>(local_date.days);
    cmd[6] = static_cast<char>(local_date.years - 2000);

    int utc_int = static_cast<int>(utc_offset);

    // changes for HC versions that support the high precision time zone
    if(precise)
    {
        cmd[0] = 'I';
        utc_int *= 4;
    }

    cmd[7] = static_cast<char>(utc_int & 0xFF);

    // set dst
    cmd[8] = dst ? 1 : 0;

    set_sim_response("#");
    return send_command(cmd, 9, response, 1, false, true);
}

bool CelestronDriver::get_utc_date_time(double *utc_hours, int *yy, int *mm,
                                        int *dd, int *hh, int *minute, int *ss, bool* dst, bool precise)
{
    // Simulated response (HH MM SS MONTH DAY YEAR OFFSET DAYLIGHT)
    // 2015-04-01T17:30:10  tz +3 dst 0
    //set_sim_response("%c%c%c%c%c%c%c%c#", 17, 30, 10, 4, 1, 15, 3, 0);
    // use current system time for simulator
    time_t now = time(nullptr);
    tm *ltm = localtime(&now);

    set_sim_response("%c%c%c%c%c%c%c%c#",
                     ltm->tm_hour, ltm->tm_min, ltm->tm_sec,
                     ltm->tm_mon, ltm->tm_mday, ltm->tm_year - 100,
                     precise ? ltm->tm_gmtoff / 900 : ltm->tm_gmtoff / 3600, ltm->tm_isdst);

    // the precise time reader reports the time zone in 15 minute steps

    // read the local time from the HC
    if (!send_command(precise ? "i" : "h", 1, response, 9, true, false))
        return false;

    // Celestron returns local time, offset and DST
    ln_zonedate localTime;
    ln_date utcTime;

    // HH MM SS MONTH DAY YEAR OFFSET DAYLIGHT
    localTime.hours   = response[0];
    localTime.minutes = response[1];
    localTime.seconds = response[2];
    localTime.months  = response[3];
    localTime.days    = response[4];
    localTime.years   = 2000 + response[5];
    int gmtoff = response[6];
    *dst = response[7] != 0;

    // make gmtoff signed
    if (gmtoff > 50)
        gmtoff -= 256;

    // precise returns offset in 15 minute steps
    if (precise)
    {
        *utc_hours = gmtoff / 4;
        localTime.gmtoff = gmtoff * 900;
    }
    else
    {
        *utc_hours = gmtoff;
        localTime.gmtoff = gmtoff * 3600;
    }

    if (*dst)
    {
        *utc_hours += 1;
        localTime.gmtoff += 3600;
    }

//    LOGF_DEBUG("LT %d-%d-%dT%d:%d:%f, gmtoff %d, dst %s",
//               localTime.years, localTime.months, localTime.days, localTime.hours, localTime.minutes, localTime.seconds,
//               localTime.gmtoff, *dst ? "On" : "Off");

    // convert to UTC
    ln_zonedate_to_date(&localTime, &utcTime);

    *yy     = utcTime.years;
    *mm     = utcTime.months;
    *dd     = utcTime.days;
    *hh     = utcTime.hours;
    *minute = utcTime.minutes;
    *ss     = static_cast<int>(utcTime.seconds);

//    LOGF_DEBUG("UTC %d-%d-%dT%d:%d:%d utc_hours %f", *yy, *mm, *dd, *hh, *minute, *ss, *utc_hours);

    return true;
}

bool CelestronDriver::is_slewing(bool *slewing)
{
    set_sim_response("%d#", sim_data.isSlewing);

    if (!send_command("L", 1, response, 2, true, true))
        return false;

    *slewing = response[0] != '0';
    return true;
}

bool CelestronDriver::get_track_mode(CELESTRON_TRACK_MODE *mode)
{
    set_sim_response("\02#");

    if (!send_command("t", 1, response, 2, true, false))
        return false;

    *mode = static_cast<CELESTRON_TRACK_MODE>(response[0]);
    return true;
}

bool CelestronDriver::set_track_mode(CELESTRON_TRACK_MODE mode)
{
    char cmd[3];
    sprintf(cmd, "T%c", mode);
    set_sim_response("#");

    return send_command(cmd, 2, response, 1, false, true);
}

bool CelestronDriver::hibernate()
{
    set_sim_response("#");
    return send_command("x", 1, response, 1, true, true);
}

// wakeup the mount
bool CelestronDriver::wakeup()
{
    set_sim_response("#");
    return send_command("y", 1, response, 1, true, true);
}

// do a last align, assumes the mount is at the index position
bool CelestronDriver::lastalign()
{
    set_sim_response("#");
    return send_command("Y", 1, response, 1, true, true);
}

bool CelestronDriver::startmovetoindex()
{
    if (!send_passthrough(CELESTRON_DEV_RA, MC_LEVEL_START, nullptr, 0, response, 0))
        return false;
    return send_passthrough(CELESTRON_DEV_DEC, MC_LEVEL_START, nullptr, 0, response, 0);
}

bool CelestronDriver::indexreached(bool *atIndex)
{
    if (!send_passthrough(CELESTRON_DEV_DEC, MC_LEVEL_DONE, nullptr, 0, response, 1))
        return false;
    bool atDecIndex = response[0] != '\00';
    if (!send_passthrough(CELESTRON_DEV_RA, MC_LEVEL_DONE, nullptr, 0, response, 1))
        return false;
    bool atRaIndex = response[0] != '\00';
    *atIndex = atDecIndex && atRaIndex;
    return true;
}

// Get pier side command, returns 'E' or 'W'
bool CelestronDriver:: get_pier_side(char *side_of_pier)
{
    set_sim_response("W#");

    if (!send_command("p", 1, response, 2, true, true))
        return false;
    *side_of_pier = response[0];

    return true;
}

// check if the mount is aligned using the mount J command
bool CelestronDriver::check_aligned(bool *isAligned)
{
    // returns 0x01 or 0x00
    set_sim_response("\x01#");
    if (!send_command("J", 1, response, 2, true, false))
        return false;

    *isAligned = response[0] == 0x01;
    return true;
}

bool CelestronDriver::set_track_rate(CELESTRON_TRACK_RATE rate, CELESTRON_TRACK_MODE mode)
{
    set_sim_response("#");
    char cmd;
    switch (mode)
    {
        case CTM_EQN:
            cmd = MC_SET_POS_GUIDERATE;
            break;
        case CTM_EQS:
            cmd = MC_SET_NEG_GUIDERATE;
            break;
        default:
            return false;
    }
    char payload[] = {static_cast<char>(rate >> 8 & 0xff), static_cast<char>(rate & 0xff)};
    return send_passthrough(CELESTRON_DEV_RA, cmd, payload, 2, response, 0);
}

// focuser commands

bool CelestronDriver::foc_exists()
{
    char focVersion[16];
    int vernum = 0;     // version as a number: 0xMMmmbbbb
    LOG_DEBUG("Does focuser exist...");
    size_t rlen = send_passthrough(CELESTRON_DEV_FOC, GET_VER, nullptr, 0, response, 4);
    switch (rlen)
    {
        case 2:
        case 3:
            snprintf(focVersion, 15, "%d.%02d", static_cast<uint8_t>(response[0]), static_cast<uint8_t>(response[1]));
            vernum = (static_cast<uint8_t>(response[0]) << 24) + (static_cast<uint8_t>(response[1]) << 16);
            break;
        case 4:
        case 5:
            snprintf(focVersion, 15, "%d.%02d.%d",
                     static_cast<uint8_t>(response[0]), static_cast<uint8_t>(response[1]),
                     static_cast<int>((static_cast<uint8_t>(response[2]) << 8) + static_cast<uint8_t>(response[3])));
            vernum = (static_cast<uint8_t>(response[0]) << 24) + (static_cast<uint8_t>(response[1]) << 16) + (static_cast<uint8_t>(response[2]) << 8) + static_cast<uint8_t>(response[3]);
            break;
        default:
            LOGF_DEBUG("No focuser found, %i", echo());
            return false;
    }

    LOGF_DEBUG("Focuser Version %s, exists %s", focVersion, vernum != 0 ? "true" : "false");
    return vernum != 0;
}

int CelestronDriver::foc_position()
{
    if (simulation)
    {
        int offset = get_sim_foc_offset();
        if (offset > 250)
            move_sim_foc(250);
        else if (offset < -250)
            move_sim_foc(-250);
        else
            move_sim_foc(offset);
    }
    set_sim_response("%c%c%c#", sim_data.foc_position >> 16 & 0xff, sim_data.foc_position >> 8 & 0XFF, sim_data.foc_position & 0XFF);

    size_t rlen = send_passthrough(CELESTRON_DEV_FOC, MC_GET_POSITION, nullptr, 0, response, 3);
    if (rlen >= 3)
    {
        int pos = (static_cast<uint8_t>(response[0]) << 16) + (static_cast<uint8_t>(response[1]) << 8) + static_cast<uint8_t>(response[2]);
        LOGF_DEBUG("get focus position %d", pos);
        return pos;
    }
    LOG_DEBUG("get Focus position fail");
    return -1;
}

bool CelestronDriver::foc_move(uint32_t steps)
{
    sim_data.foc_target = steps;
    LOGF_DEBUG("Focus move %d", steps);
    char payload[] = {static_cast<char>(steps >> 16 & 0xff), static_cast<char>(steps >> 8 & 0xff), static_cast<char>(steps & 0xff)};
    set_sim_response("#");
    size_t rlen = send_passthrough(CELESTRON_DEV_FOC, MC_GOTO_FAST, payload, 3, response, 0);
    return rlen > 0;
}

bool CelestronDriver::foc_moving()
{
    set_sim_response("%c#", sim_data.foc_target == sim_data.foc_position ? 0xff : 0x00);
    size_t rlen = send_passthrough(CELESTRON_DEV_FOC, MC_SLEW_DONE, nullptr, 0, response, 1);
    if (rlen < 1 )
        return false;
    return response[0] != '\xff';   // use char comparison because some compilers object
}

bool CelestronDriver::foc_limits(int * low, int * high)
{
    set_sim_response("%c%c%c%c%c%c%c%c#", 0, 0, 0x07, 0xd0, 0, 0, 0x9C, 0x40); // 2000, 40000

    size_t rlen = send_passthrough(CELESTRON_DEV_FOC, FOC_GET_HS_POSITIONS, nullptr, 0, response, 8);
    if (rlen < 8)
        return false;

    *low = (static_cast<uint8_t>(response[0]) << 24) + (static_cast<uint8_t>(response[1]) << 16) + (static_cast<uint8_t>(response[2]) << 8) + static_cast<uint8_t>(response[3]);
    *high = (static_cast<uint8_t>(response[4]) << 24) + (static_cast<uint8_t>(response[5]) << 16) + (static_cast<uint8_t>(response[6]) << 8) + static_cast<uint8_t>(response[7]);

    // check on integrity of values, they must be sensible and the range must be more than 2 turns
    if (*high - *low < 2000 || *high < 0 || *high > 60000 || *low < 0 || *low > 50000)
    {
        LOGF_INFO("Focus range %i to %i invalid, range not updated", *high, *low);
        return false;
    }

    LOGF_DEBUG("Focus Limits: Maximum (%i) Minimum (%i)", *high, *low);
    return true;
}

bool CelestronDriver::foc_abort()
{
    if(simulation)
    {
        sim_data.foc_target = sim_data.foc_position;
    }
    set_sim_response("#");

    char payload[] = {0};
    size_t rlen = send_passthrough(CELESTRON_DEV_FOC, MC_MOVE_POS, payload, 1, response, 0);
    return rlen > 0;
}

////////////////////////////////////////////////////////////////////////////
//      PEC Handling
////////////////////////////////////////////////////////////////////////////

bool CelestronDriver::PecSeekIndex()
{
    if (pecState >= PEC_STATE::PEC_INDEXED)
    {
        LOG_DEBUG("PecSeekIndex - already found");
        return true;
    }

    set_sim_response("#");

    size_t rlen = send_passthrough(CELESTRON_DEV_RA, MC_SEEK_INDEX, nullptr, 0,response, 0);
    if (rlen < 1)
    {
        LOG_WARN("Start PEC seek index failed");
        return false;
    }

    pecState = PEC_STATE::PEC_SEEKING;

    simSeekIndex = true;

    LOGF_DEBUG("PecSeekIndex %s", PecStateStr());

    return true;
}

bool CelestronDriver::isPecAtIndex(bool force)
{
    if (pecState <= PEC_STATE::PEC_NOT_AVAILABLE)
        return false;

    if (!force && pecState >= PEC_STATE::PEC_INDEXED)
        return true;

    set_sim_response("%c#", simSeekIndex ? 0xFF : 0x00);

    size_t rlen = send_passthrough(CELESTRON_DEV_RA, MC_AT_INDEX, nullptr, 0, response, 1);
    if (rlen < 1)
        return false;

    bool indexed = (response[0] == '\xFF');
    // update the local PEC state
    if (indexed && pecState <= PEC_STATE::PEC_INDEXED)
    {
        pecState = PEC_STATE::PEC_INDEXED;
        LOG_INFO("PEC Index Found");
    }

    LOGF_DEBUG("isPecAtIndex? %s", indexed ? "yes" : "no");

    return indexed;
}

size_t CelestronDriver::getPecNumBins()
{
    if (pecState < PEC_STATE::PEC_AVAILABLE)
    {
        LOG_DEBUG("getPecNumBins - PEC not available");
        return 0;
    }
    set_sim_response("%c#", 88);
    char payload[] = { 0x3F };
    size_t rlen = send_passthrough(CELESTRON_DEV_RA, MC_PEC_READ_DATA, payload, 1, response, 1);
    if (rlen < 1)
        return 0;

    size_t numPecBins = response[0];
    LOGF_DEBUG("getPecNumBins %d", numPecBins);
    return numPecBins;
}

size_t CelestronDriver::pecIndex()
{
    if (simulation)
    {
        // increment the index each time we read it.  Timing will be too fast, a good thing!
        simIndex++;
        if (simIndex >= 88)
            simIndex = 0;
    }
    set_sim_response("%c#", simIndex);

    size_t rlen = send_passthrough(CELESTRON_DEV_RA, MTR_PECBIN, nullptr, 0, response, 1);
    if (rlen < 1)
        return 0;

    return response[0];
}

bool CelestronDriver::PecPlayback(bool start)
{
    if (!(pecState == PEC_STATE::PEC_INDEXED || pecState == PEC_STATE::PEC_PLAYBACK))
        return false;
    char data[1]; data[0] = start ? 0x01 : 0x00;

    set_sim_response("#");

    size_t rlen = send_passthrough(CELESTRON_DEV_RA, MC_PEC_PLAYBACK, data, 1, response, 0);
    if (rlen <= 0)
    {
        LOGF_WARN("PEC Playback %s failed", start ? "start" : "stop");
        return false;
    }

    // we can't read the PEC state so use the start state to set it
    pecState = start ? PEC_STATE::PEC_PLAYBACK : PEC_STATE::PEC_INDEXED;

    LOGF_DEBUG("PecPayback %s, pecState %s", start ? "start" : "stop", PecStateStr());

    return true;
}

bool CelestronDriver::PecRecord(bool start)
{
    if (!(pecState == PEC_STATE::PEC_INDEXED || pecState == PEC_STATE::PEC_RECORDING))
        return false;

    int command = start ? MC_PEC_RECORD_START : MC_PEC_RECORD_STOP;

    set_sim_response("#");
    simRecordStart = simIndex;

    size_t rlen = send_passthrough(CELESTRON_DEV_RA, command, nullptr, 0, response, 0);
    if (rlen <= 0)
    {
        LOGF_WARN("PEC Record %s failed", start ? "start" : "stop");
        return false;
    }

    pecState = start ? PEC_STATE::PEC_RECORDING : PEC_STATE::PEC_INDEXED;

    LOGF_DEBUG("PecRecord %s, pecState %s", start ? "start" : "stop", PecStateStr());
    return true;
}

bool CelestronDriver::isPecRecordDone()
{
    if (pecState != PEC_STATE::PEC_RECORDING)
        return true;

    set_sim_response("%c#", simIndex == simRecordStart ? 1 : 0);

    size_t rlen = send_passthrough(CELESTRON_DEV_RA, MC_PEC_RECORD_DONE, nullptr, 0, response, 1);
    if (rlen < 1)
        return false;

    bool done = response[0] != 0x00;
    if (done)
        pecState = PEC_STATE::PEC_INDEXED;

    LOGF_DEBUG("isPecRecordDone %s", done ? "yes" : "no");

    return done;
}

int CelestronDriver::getPecValue(size_t index)
{
    if (simulation)
    {
        // generate PEC value from index, range -100 to +100, 1 cycle
        int val = static_cast<int>(std::round(std::cos(index * 2.0 * 3.14192 / 87.0) * 100.0));
        if (val < 0) val = 256 + val;
        set_sim_response("%c#", val);
    }
    char data[] = { static_cast<char>(0x40 + index) };
    size_t rlen = send_passthrough(CELESTRON_DEV_RA, MC_PEC_READ_DATA, data, 1, response, 1);
    if (rlen < 1)
        return 0;

    // make result signed
    return response[0] <= '\127' ? response[0] : -256 + response[0];
}

bool CelestronDriver::setPecValue(size_t index, int data)
{
    char payload[2];
    payload[0] = static_cast<char>(0x40 + index);
    payload[1] = static_cast<char>((data < 127) ? data : 256 - data);
    set_sim_response("#");
    return send_passthrough(CELESTRON_DEV_RA, MC_PEC_WRITE_DATA, payload, 2, response, 1) == 0;
}

PEC_STATE CelestronDriver::updatePecState()
{
    switch (pecState)
    {
    case PEC_STATE::PEC_SEEKING:
        isPecAtIndex();
        break;
    case PEC_STATE::PEC_RECORDING:
        isPecRecordDone();
        break;
    default:
        break;
    }
    return pecState;
}

const char * CelestronDriver::PecStateStr()
{
    return PecStateStr(pecState);
}

const char * CelestronDriver::PecStateStr(PEC_STATE state)
{    
    switch (state)
    {
    default:
        return "None";
    case PEC_STATE::PEC_NOT_AVAILABLE:
        return "Not Available";
    case PEC_STATE::PEC_AVAILABLE:
        return "Available";
    case PEC_STATE::PEC_PLAYBACK:
        return "PEC Playback";
    case PEC_STATE::PEC_SEEKING:
        return "seeking index";
    case PEC_STATE::PEC_INDEXED:
        return "Index Found";
    case PEC_STATE::PEC_RECORDING:
        return "PEC Recording";
    }
}

//////////////////////////////////////////////////////
/// PecData class
//////////////////////////////////////////////////////

// constructor, generates test data
PecData::PecData()
{
    numBins = 88;
    for (size_t i = 0; i <= numBins; ++i)
    {
        double p = i * 2.0 * 3.14192 / numBins;
        data[i] = std::sin(p) * 5;
    }
    wormArcSeconds = 7200;
}

// Load PEC data from the mount
bool PecData::Load(CelestronDriver *driver)
{
    // get model # and use it to set wormArcSeconds and rateScale
    int mountType = driver->model();
    rateScale = (mountType <= 2) ? 512 : 1024;
    wormArcSeconds = mountType == 8 ? 3600 : 7200;
    //LOGF_DEBUG("rateScale %f, wormArcSeconds %f, SiderealRate %f", rateScale, wormArcSeconds, SIDEREAL_ARCSEC_PER_SEC);

    numBins = driver->getPecNumBins();
    if (numBins < 88 || numBins > 254)
        return false;

    double posError = 0;
    data[0] = 0.0;
    for (size_t i = 0; i < numBins; i++)
    {
        // this is ported from the Celestron PECTool VB6 code
        // ' We traveled at SIDEREAL + binRate arcsec/sec over a distance of wormArcseconds/numPecBins arcseconds.
        // ' We need to figure out how long that took to get the error in arcseconds...
        // ' ie., error = binRate * binTime
        //        binRate = rawPecData(i)
        //        binTime = wormArcseconds / numPecBins / (SIDEREAL_ARCSEC_PER_SEC + binRate)
        //        posError = posError + binRate * binTime
        //        currentPecData(i + 1, 0) = posError

        int rawPec = driver->getPecValue(i);

        double binRate = rawPec * SIDEREAL_ARCSEC_PER_SEC / rateScale;
        double binTime = (wormArcSeconds / numBins) / (SIDEREAL_ARCSEC_PER_SEC + binRate);
        posError += binRate * binTime;
        data[i + 1] = posError;

        LOGF_DEBUG("i %d, rawPec %d, binRate %f, binTime %f, data[%d] %f", i, rawPec, binRate, binTime, i+1, data[i+1]);
    }
    return true;
}

// PEC file format, this matches the format used by Celestron in their PECTool application
// file format, one line for each entry:
//  line 0:         numBins, currently 88
//  lines 1 to 90:  double data[0] to data[numBins], numBins + 1 values, currently 89
//  line 91:        wormArcSecs, currently 7200
//

// Load PEC data from file
bool PecData::Load(const char *fileName)
{
    std::ifstream pecFile(fileName);

    if (pecFile.is_open())
    {
        pecFile >> numBins;
        for (size_t i = 0; i <= numBins; i++)
        {
            pecFile >> data[i];
        }
        pecFile >> wormArcSeconds;
        LOGF_DEBUG("PEC Load File %s, numBins %d, wormarcsecs %d", fileName, numBins, wormArcSeconds);
        return true;
    }
    else
    {
        // report file open failure
        LOGF_WARN("Load PEC file %s, error %s", fileName, strerror(errno));
    }
    return false;
}

// Save the current PEC data to file
// returns false if it fails
bool PecData::Save(const char *filename)
{
    std::ofstream pecFile(filename);
    if (!pecFile.is_open())
        return false;

    pecFile << numBins << "\n";
    for (size_t i = 0; i <= numBins; i++)
    {
        pecFile << data[i] << "\n";
        LOGF_DEBUG("data[%d] = %f", i, data[i]);
    }
    pecFile << wormArcSeconds << "\n";
    return true;
}

// Save the current PEC data to the mount
bool PecData::Save(CelestronDriver *driver)
{
    if (driver->getPecNumBins() != numBins)
    {
        return false;
    }

    double deltaDist;
    for (size_t i = 0; i < numBins; i++)
    {
        // this is ported from the Celestron PECTool VB6 code
        // deltaDist = currentPecData(i + 1, 0) - currentPecData(i, 0)
        // rawPecData(i) = deltaDist * SIDEREAL_ARCSEC_PER_SEC / (wormArcseconds / numPecBins - deltaDist)

        // get the offset in arcsecs per bin
        deltaDist = data[i + 1] - data[i];
        // convert to offset in arcsecs per second
        double rawPecData = deltaDist * SIDEREAL_ARCSEC_PER_SEC / (wormArcSeconds / numBins - deltaDist);

        int rawdata = static_cast<int>(std::round(rawPecData * rateScale / SIDEREAL_ARCSEC_PER_SEC));
        LOGF_DEBUG("i %d, deltaDist %f, rawPecdata %f, rawData %d", i, deltaDist, rawPecData, rawdata);
        if (rawdata < 0)
            rawdata += 256;
        driver->setPecValue(i, rawdata);
    }
    return true;
}

// Removes any drift over the PEC cycle
void PecData::RemoveDrift()
{
    // this works by taking the offset in arcseconds over one PEC cycle and correcting the PEC values
    // linearly so the drift is eliminated.
    // It gives slightly different values to what the original drift removal does but the difference is
    // small
    double delta = (data[numBins] - data[0]) / numBins;
    double offset = data[0];
    for (size_t i = 0; i <= numBins; i++)
    {
        data[i] = data[i] - offset - delta * i;
    }
}

void PecData::Kalman(PecData newData, int num)
{
    if (numBins != newData.numBins)
    {
        //throw new ApplicationException("Kalman not possible numBins do not match");
        return;
    }
    auto fraction = 1.0 / num;
    auto kf = 1 - fraction;
    //auto nd = newData.Data;
    for (size_t i = 0; i <= numBins; i++)
    {
        data[i] = data[i] * kf + newData.data[i] * fraction;
    }
}

// This method is required by the logging macros
const char *PecData::getDeviceName()
{
    return device_str;
}


