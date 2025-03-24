/*
    INDI Explore Scientific PMC8 driver

    Copyright (C) 2017 Michael Fulbright
    Additional contributors:
        Thomas Olson, Copyright (C) 2019
        Karl Rees, Copyright (C) 2019-2023
        Martin Ruiz, Copyright (C) 2023

    Based on IEQPro driver.

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

#include "pmc8driver.h"

#include "indicom.h"
#include "indilogger.h"
#include "inditelescope.h"

#include <libnova/julian_day.h>
#include <libnova/sidereal_time.h>

#include <math.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

// only used for test timing of pulse guiding
#include <sys/time.h>

#define PMC8_TIMEOUT 5 /* FD timeout in seconds */

#define PMC8_SIMUL_VERSION_RESP "ESGvES06B9T9"

// MOUNT_G11
#define PMC8_G11_AXIS0_SCALE 4608000.0
#define PMC8_G11_AXIS1_SCALE 4608000.0
// MOUNT_EXOS2
#define PMC8_EXOS2_AXIS0_SCALE 4147200.0
#define PMC8_EXOS2_AXIS1_SCALE 4147200.0
// MOUNT_iEXOS100
#define PMC8_iEXOS100_AXIS0_SCALE 4147200.0
#define PMC8_iEXOS100_AXIS1_SCALE 4147200.0

// Need to initialize to some value, or certain clients (e.g., KStars Lite) freak out
double PMC8_AXIS0_SCALE = PMC8_EXOS2_AXIS0_SCALE;
double PMC8_AXIS1_SCALE = PMC8_EXOS2_AXIS1_SCALE;

#define ARCSEC_IN_CIRCLE 1296000.0

// Reference says 2621.44 counts, which then needs to be multiplied by 25 (so actually 16^4-1)
// However, on Exos2 62500 (F424) is reported when slewing
#define PMC8_MAX_PRECISE_MOTOR_RATE 62500

// any guide pulses less than this are ignored as it will not result in any actual motor motion
#define PMC8_PULSE_GUIDE_MIN_MS 20

// guide pulses longer than this require using a timer
#define PMC8_PULSE_GUIDE_MAX_NOTIMER 250

#define PMC8_MAX_RETRIES 3 /*number of times to retry reading a response */
#define PMC8_RETRY_DELAY 30000 /* how long to wait before retrying i/o */
#define PMC8_MAX_IO_ERROR_THRESHOLD 2 /* how many consecutive read timeouts before trying to reset the connection */

// Thanks to John Wells who contributed to this issue: https://github.com/indilib/indi/issues/2132
// and PMC8 documentation: https://02d3287.netsolhost.com/pmc-eight/PMC_Eight_ProgrammersReferenceManual_Release2_2019_February_01.pdf
// and confirmation: https://espmc-eight.groups.io/g/MAIN/topic/96696552?p=Created,,,20,2,0,0
// The rate is communicated as "arcsecs per sidereal second" NOT per second so it should be 15.000
// not 15.041067
// The INDI Driver uses arcsecs per SOLAR second, so it needs to be converted.
#define PMC8_RATE_SIDEREAL 15.000
#define PMC8_RATE_LUNAR 14.451
#define PMC8_RATE_SOLAR 14.959
#define PMC8_RATE_KING 14.996

PMC8_CONNECTION_TYPE pmc8_connection         = PMC8_SERIAL_AUTO;
bool pmc8_debug                 = false;
bool pmc8_simulation            = false;
bool pmc8_isRev2Compliant       = false;
bool pmc8_reconnect_flag        = false;
bool pmc8_goto_resume           = true;
int pmc8_io_error_ctr           = 0;
char pmc8_device[MAXINDIDEVICE] = "PMC8";
double pmc8_latitude            = 0;  // must be kept updated by pmc8.cpp when it is changed!
double pmc8_longitude           = 0;  // must be kept updated by pmc8.cpp when it is changed!
double pmc8_sidereal_rate_fraction_ra = 0.4;
double pmc8_sidereal_rate_fraction_de = 0.4;
int pmc8_east_dir               = 1; // 1 is for northern hemisphere, switch to 0 for southern
PMC8Info simPMC8Info;

// state variable for driver based pulse guiding
typedef struct PulseGuideState
{
    bool pulseguideactive = false;
    bool fakepulse = false;
    int ms;
    long long pulse_start_us;
    double cur_rate;
    int cur_dir;
    double new_rate;
    int new_dir;
} PulseGuideState;

// need one for NS and EW pulses which may be simultaneous
PulseGuideState NS_PulseGuideState, EW_PulseGuideState;

struct
{
    double ra;
    double dec;
    int raDirection;
    int decDirection;
    double trackRate;
    double moveRate;
    double guide_rate;
} simPMC8Data;

// convert mount count to 6 character two complement hex string
void convert_motor_counts_to_hex(int val, char *hex)
{
    unsigned tmp;
    char h[16];

    if (val < 0)
    {
        tmp = abs(val);
        tmp = ~tmp;
        tmp++;
    }
    else
    {
        tmp = val;
    }

    sprintf(h, "%08X", tmp);

    strcpy(hex, h + 2);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "convert_motor_counts_to_hex val=%d, h=%s, hex=%s", val, h, hex);
}

// convert rate in arcsec/sidereal_second to internal PMC8 precise motor rate for RA axis tracking ONLY
bool convert_precise_rate_to_motor(double rate, int *mrate)
{

    *mrate = round(25 * rate * (PMC8_AXIS0_SCALE / ARCSEC_IN_CIRCLE));

    if (*mrate > PMC8_MAX_PRECISE_MOTOR_RATE)
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_WARNING, "requested tracking motor rate %d exceeds maximum, using %d", *mrate,
                     PMC8_MAX_PRECISE_MOTOR_RATE);
        *mrate = PMC8_MAX_PRECISE_MOTOR_RATE;
    }
    else if (*mrate < -PMC8_MAX_PRECISE_MOTOR_RATE)
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_WARNING, "requested tracking motor rate %d exceeds maximum, using %d", *mrate,
                     -PMC8_MAX_PRECISE_MOTOR_RATE);
        *mrate = -PMC8_MAX_PRECISE_MOTOR_RATE;
    }

    return true;
}

// convert rate in arcsec/sidereal_second to internal PMC8 precise motor rate for RA axis tracking ONLY
bool convert_precise_motor_to_rate(int mrate, double *rate)
{
    *rate = ((double)mrate) * (ARCSEC_IN_CIRCLE / PMC8_AXIS0_SCALE) / 25;

    return true;
}

// convert rate in arcsec/sidereal_second to internal PMC8 motor rate for move action (not slewing)
bool convert_move_rate_to_motor(float rate, int *mrate)
{

    float capped_move_rate = rate;
    if (rate > PMC8_MAX_MOVE_RATE)
        capped_move_rate = PMC8_MAX_MOVE_RATE;
    else if (rate < -PMC8_MAX_MOVE_RATE)
        capped_move_rate = -PMC8_MAX_MOVE_RATE;

    *mrate = (int)(capped_move_rate * (PMC8_AXIS0_SCALE / ARCSEC_IN_CIRCLE));

    return true;
}

// convert rate internal PMC8 motor rate to arcsec/sec for move action (not slewing)
bool convert_motor_rate_to_move_rate(int mrate, double *rate)
{
    *rate = ((double)mrate) * ARCSEC_IN_CIRCLE / PMC8_AXIS0_SCALE;

    return true;
}

void set_pmc8_mountParameters(int index)
{
    switch(index)
    {
        case 0: // LosMandy G11
            PMC8_AXIS0_SCALE = PMC8_G11_AXIS0_SCALE;
            PMC8_AXIS1_SCALE = PMC8_G11_AXIS1_SCALE;
            break;
        case 1: // EXOS2
            PMC8_AXIS0_SCALE = PMC8_EXOS2_AXIS0_SCALE;
            PMC8_AXIS1_SCALE = PMC8_EXOS2_AXIS1_SCALE;
            break;
        case 2: // iEXOS100
            PMC8_AXIS0_SCALE = PMC8_iEXOS100_AXIS0_SCALE;
            PMC8_AXIS1_SCALE = PMC8_iEXOS100_AXIS1_SCALE;
            break;
        default:
            DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Need To Select a  Mount");
            break;
    }
}

void set_pmc8_debug(bool enable)
{
    pmc8_debug = enable;
}

void set_pmc8_simulation(bool enable)
{
    pmc8_simulation = enable;
    if (enable)
        simPMC8Data.guide_rate = 0.5;
}

void set_pmc8_device(const char *name)
{
    strncpy(pmc8_device, name, MAXINDIDEVICE);
}

void set_pmc8_location(double latitude, double longitude)
{
    pmc8_latitude = latitude;
    pmc8_longitude = longitude;

    pmc8_east_dir = (latitude < 0) ? 0 : 1;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "Set PMC8 'lowlevel' lat:%f long:%f", pmc8_latitude, pmc8_longitude);
}

void set_pmc8_sim_system_status(PMC8_SYSTEM_STATUS value)
{
    simPMC8Info.systemStatus = value;

    if (value == ST_PARKED)
    {
        double lst;
        double ra;

        lst = get_local_sidereal_time(pmc8_longitude);

        ra = lst + 6;
        if (ra > 24)
            ra -= 24;

        set_pmc8_sim_ra(ra);
        if (pmc8_latitude < 0)
            set_pmc8_sim_dec(-90.0);
        else
            set_pmc8_sim_dec(90.0);

    }
}

void set_pmc8_sim_track_rate(PMC8_TRACK_RATE value)
{
    simPMC8Data.trackRate = value;
}

void set_pmc8_sim_move_rate(int value)
{
    simPMC8Data.moveRate = value;
}

void set_pmc8_sim_ra(double ra)
{
    simPMC8Data.ra = ra;
}

void set_pmc8_sim_dec(double dec)
{
    simPMC8Data.dec = dec;
}

bool check_pmc8_connection(int fd, PMC8_CONNECTION_TYPE connection)
{
    pmc8_connection = connection;

    if (connection == PMC8_ETHERNET)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_SESSION, "Connecting to PMC8 via Ethernet.");
    }
    else
    {
        if (connection == PMC8_SERIAL_STANDARD) DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_SESSION,
                    "Connecting to PMC8 via standard Serial cable.  Please wait 15 seconds for mount to reset.");
        else if (connection == PMC8_SERIAL_AUTO) DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_SESSION,
                    "Connecting to PMC8 via Serial.  Autodecting cable type.  This could take up to 30 seconds.");
        else DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_SESSION, "Connecting to PMC8 via inverted Serial.");
    }

    if (connection != PMC8_SERIAL_STANDARD)
    {
        for (int i = 0; i < 2; i++)
        {
            if (i) DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_SESSION, "Retrying...");

            if (detect_pmc8(fd)) return true;
            usleep(PMC8_RETRY_DELAY);
        }
    }

    if ((connection == PMC8_SERIAL_STANDARD) || (connection == PMC8_SERIAL_AUTO))
    {
        // If they're not using a custom-configured cable, we need to clear DTR for serial to start working
        // But this resets the PMC8, so only do it after we've already checked for connection
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "Attempting to clear DTR for standard cable.");
        int serial = TIOCM_DTR;
        ioctl(fd, TIOCMBIC, &serial);

        // when we clear DTR, the PMC8 will respond with initialization screen, so may need read several times
        for (int i = 0; i < 2; i++)
        {
            if (i) DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_SESSION, "Retrying...");

            if (detect_pmc8(fd))
            {
                DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_WARNING, "Connected to PMC8 using a standard-configured FTDI cable."
                            "Your mount will reset and lose its position anytime you disconnect and reconnect."
                            "See http://indilib.org/devices/telescopes/explore-scientific-g11-pmc-eight/ ");
                return true;
            }
            usleep(PMC8_RETRY_DELAY);
        }
    }

    DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR,
                "check_pmc8_connection(): Error connecting. Check power and connection settings.");

    return false;
}

bool detect_pmc8(int fd)
{
    char initCMD[] = "ESGv!";
    int errcode    = 0;
    char errmsg[MAXRBUF];
    char response[64];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (pmc8_simulation)
    {
        strcpy(response, PMC8_SIMUL_VERSION_RESP);
        nbytes_read = strlen(response);
    }
    else
    {
        if ((errcode = send_pmc8_command(fd, initCMD, strlen(initCMD), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error connecting on write: %s", errmsg);
            return false;
        }

        if ((errcode = get_pmc8_response(fd, response, &nbytes_read, "ESGv")))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "Error connecting on read: %s", errmsg);
            return false;
        }
    }

    // return true if valid firmware response
    return (!strncmp(response, "ESGvES", 6));
}

bool get_pmc8_model(int fd, FirmwareInfo *info)
{
    // Only one model for now
    info->Model.assign("PMC-Eight");

    // Set the mount type from firmware if we can (instead of relying on interface)
    // older firmware has type in firmware string
    if (!pmc8_isRev2Compliant)
    {
        INDI_UNUSED(fd);

        if (strstr(info->MainBoardFirmware.c_str(), "G11"))
        {
            info->MountType = MOUNT_G11;
        }
        else if (strstr(info->MainBoardFirmware.c_str(), "EXOS2"))
        {
            info->MountType = MOUNT_EXOS2;
        }
        else if (strstr(info->MainBoardFirmware.c_str(), "ES1A"))
        {
            info->MountType = MOUNT_iEXOS100;
        }
    }
    else
    {
        //for newer firmware, need to use ESGi to get mount type
        char cmd[]  = "ESGi!";
        int errcode = 0;
        char errmsg[MAXRBUF];
        char response[64];
        int nbytes_read    = 0;
        int nbytes_written = 0;

        if (pmc8_simulation)
        {
            strcpy(response, PMC8_SIMUL_VERSION_RESP);
            nbytes_read = strlen(response);
        }
        else
        {
            if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
            {
                tty_error_msg(errcode, errmsg, MAXRBUF);
                DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "3 %s", errmsg);
                return false;
            }

            if ((errcode = get_pmc8_response(fd, response, &nbytes_read, "ESGi")))
            {
                DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "get_pmc8_main_firmware(): Error reading response.");
                return false;
            }

            //ESGi response should be 31 characters
            if (nbytes_read >= 31)
            {
                //locate P9 code in response
                char num_str[3] = {0};
                strncat(num_str, response + 20, 2);
                int p9 = (int)strtol(num_str, nullptr, 10);

                // Set mount type based on P9 code
                if (p9 <= 1) info->MountType = MOUNT_iEXOS100;
                // these codes are reserved.  I'm assuming for something like iExos100, so let's go with that
                else if (p9 <= 3)
                {
                    info->MountType = MOUNT_iEXOS100;
                    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Unrecognized device code #%d. Treating as iEXOS100.", p9);
                }
                else if (p9 <= 7) info->MountType = MOUNT_G11;
                else if (p9 <= 11) info->MountType = MOUNT_EXOS2;
                // unrecognized code.  Just going to guess and treat as iExos100.
                else
                {
                    info->MountType = MOUNT_iEXOS100;
                    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Unrecognized device code #%d. Treating as iEXOS100.", p9);
                }

            }
            else
            {
                DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR,
                             "Could not detect device type. Only received #%d bytes, expected at least 31.", nbytes_read);
                return false;
            }

            tcflush(fd, TCIFLUSH);
        }
    }
    // update mount parameters
    set_pmc8_mountParameters(info->MountType);
    return true;
}

bool get_pmc8_main_firmware(int fd, FirmwareInfo *info)
{
    char cmd[]  = "ESGv!";
    char board[64];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[64];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (pmc8_simulation)
    {
        strcpy(response, PMC8_SIMUL_VERSION_RESP);
        nbytes_read = strlen(response);
    }
    else
    {
        if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "3 %s", errmsg);
            return false;
        }

        if ((errcode = get_pmc8_response(fd, response, &nbytes_read, "ESGv")))
        {
            DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "get_pmc8_main_firmware(): Error reading response.");
            return false;
        }
    }

    // prior to v2, minimum size firmware string is 12 (for iExos100), 14 for others, but can be up to 20
    // post v2, can be 50+
    if (nbytes_read >= 12)
    {

        // strip ESGvES from string when getting firmware version
        strncpy(board, response + 6, nbytes_read - 7);
        info->MainBoardFirmware.assign(board, nbytes_read - 7);

        // Assuming version strings longer than 24 must be version 2.0 and up
        if (nbytes_read > 24) info->IsRev2Compliant = pmc8_isRev2Compliant = true;

        tcflush(fd, TCIFLUSH);

        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR,
                 "Could not read firmware. Only received #%d bytes, expected at least 12.", nbytes_read);
    return false;
}

bool get_pmc8_firmware(int fd, FirmwareInfo *info)
{
    bool rc = false;

    rc = get_pmc8_main_firmware(fd, info);

    if (rc == false)
        return rc;

    rc = get_pmc8_model(fd, info);

    return rc;
}

// return move rate in arcsec / sec
bool get_pmc8_move_rate_axis(int fd, PMC8_AXIS axis, double &rate)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, sizeof(cmd), "ESGr%d!", axis);

    if (pmc8_simulation)
    {
        if (axis == PMC8_AXIS_RA)
            rate = simPMC8Data.trackRate;
        else if (axis == PMC8_AXIS_DEC)
            rate = 0; // DEC tracking not supported yet
        else
            return false;

        return true;
    }

    if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    cmd[5] = '\0';

    if ((errcode = get_pmc8_response(fd, response, &nbytes_read, cmd)))
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error getting Move Rate");
        return false;
    }

    if (nbytes_read != 10)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Axis get move rate cmd response incorrect");
        return false;
    }

    char num_str[16] = {0};

    strcpy(num_str, "0X");
    strncat(num_str, response + 5, 6);

    int mrate = (int)strtol(num_str, nullptr, 0);

    convert_motor_rate_to_move_rate(mrate, &rate);

    return true;
}

bool get_pmc8_direction_axis(int fd, PMC8_AXIS axis, int &dir)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, sizeof(cmd), "ESGd%d!", axis);

    if (pmc8_simulation)
    {
        if (axis == PMC8_AXIS_RA)
            dir = simPMC8Data.raDirection;
        else if (axis == PMC8_AXIS_DEC)
            dir = simPMC8Data.decDirection;
        else
            return false;

        return true;
    }

    if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    cmd[5] = '\0';

    if ((errcode = get_pmc8_response(fd, response, &nbytes_read, cmd)))
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error getting direction axis");
        return false;
    }

    if (nbytes_read != 7)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Axis get dir cmd response incorrect");
        return false;
    }

    char num_str[16] = {0};

    strncat(num_str, response + 5, 2);

    dir = (int)strtol(num_str, nullptr, 0);

    return true;
}

// if fast is true dont wait on response!  Used for psuedo-pulse guide
// NOTE that this will possibly mean the response will be read by a following command if it is called before
//      response comes from controller, since next command will flush before data is in buffer!
bool set_pmc8_direction_axis(int fd, PMC8_AXIS axis, int dir, bool fast)
{

    char cmd[32], expresp[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, sizeof(cmd), "ESSd%d%d!", axis, dir);

    if (pmc8_simulation)
    {
        if (axis == PMC8_AXIS_RA)
            simPMC8Data.raDirection = (PMC8_DIRECTION) dir;
        else if (axis == PMC8_AXIS_DEC)
            simPMC8Data.decDirection = (PMC8_DIRECTION) dir;
        else
            return false;

        return true;
    }

    if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if (fast)
    {
        return true;
    }

    snprintf(expresp, sizeof(expresp), "ESGd%d%d!", axis, dir);

    if ((errcode = get_pmc8_response(fd, response, &nbytes_read, expresp)))
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Axis get dir cmd response incorrect: expected=%s", expresp);
        return false;
    }

    return true;
}

bool get_pmc8_is_scope_slewing(int fd, bool &isslew)
{
    double rarate;
    double decrate;
    bool rc;

    rc = get_pmc8_move_rate_axis(fd, PMC8_AXIS_RA, rarate);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "get_pmc8_is_scope_slewing(): Error reading RA move rate");
        return false;
    }

    rc = get_pmc8_move_rate_axis(fd, PMC8_AXIS_DEC, decrate);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "get_pmc8_is_scope_slewing(): Error reading DEC move rate");
        return false;
    }

    if (pmc8_simulation)
    {
        isslew = (simPMC8Info.systemStatus == ST_SLEWING);
    }
    else
    {
        isslew = ((rarate > PMC8_MAX_TRACK_RATE) || (decrate >= PMC8_MAX_TRACK_RATE));
    }

    return true;
}

// set move speed in terms of how many times sidereal
bool set_pmc8_move_rate_axis(int fd, PMC8_DIRECTION dir, int reqrate)
{
    int rate = reqrate;

    if (rate > PMC8_MAX_MOVE_RATE)
        rate = PMC8_MAX_MOVE_RATE;
    else if (rate < -PMC8_MAX_MOVE_RATE)
        rate = -PMC8_MAX_MOVE_RATE;

    switch (dir)
    {
        case PMC8_S:
            rate = -rate;
            [[fallthrough]];
        case PMC8_N:
            return set_pmc8_custom_dec_move_rate(fd, rate);
        case PMC8_E:
            rate = -rate;
            [[fallthrough]];
        case PMC8_W:
            return set_pmc8_custom_ra_move_rate(fd, rate);
    }

    return false;
}

bool stop_pmc8_tracking_motion(int fd)
{
    bool rc;

    // stop tracking
    rc = set_pmc8_custom_ra_track_rate(fd, 0);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error stopping RA axis!");
        return false;
    }

    return true;
}

// get current (precise) tracking rate in arcsec/sec
bool get_pmc8_track_rate(int fd, double &rate)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, sizeof(cmd), "ESGx!");

    if (pmc8_simulation)
    {
        rate = simPMC8Data.trackRate;
        return true;
    }

    if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if ((errcode = get_pmc8_response(fd, response, &nbytes_read, "ESGx")))
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error getting Tracking Rate");
        return false;
    }

    if (nbytes_read != 9)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Get track rate cmd response incorrect");
        return false;
    }

    char num_str[16] = {0};

    strcpy(num_str, "0X");
    strncat(num_str, response + 4, 4);

    int mrate = (int)strtol(num_str, nullptr, 0);
    convert_precise_motor_to_rate(mrate, &rate);

    return true;
}

bool get_pmc8_tracking_data(int fd, double &rate, uint8_t &mode)
{
    if (!get_pmc8_track_rate(fd, rate)) return false;
    mode = get_pmc8_tracking_mode_from_rate(rate);
    return true;
}


uint8_t get_pmc8_tracking_mode_from_rate(double rate)
{
    int tmotor, refmotor;
    uint8_t mode;

    //get precise motor rate
    convert_precise_rate_to_motor(rate, &tmotor);

    //now check what sidereal would be
    convert_precise_rate_to_motor(PMC8_RATE_SIDEREAL, &refmotor);
    if (tmotor == refmotor) mode = PMC8_TRACK_SIDEREAL;
    else
    {

        //now check lunar
        convert_precise_rate_to_motor(PMC8_RATE_LUNAR, &refmotor);
        if (tmotor == refmotor) mode = PMC8_TRACK_LUNAR;
        else
        {

            //now check solar
            convert_precise_rate_to_motor(PMC8_RATE_SOLAR, &refmotor);
            if (tmotor == refmotor) mode = PMC8_TRACK_SOLAR;
            else
            {

                //now check king
                convert_precise_rate_to_motor(PMC8_RATE_KING, &refmotor);
                if (tmotor == refmotor) mode = PMC8_TRACK_KING;
                // must be custom
                else mode = PMC8_TRACK_CUSTOM;
            }
        }
    }
    return mode;
}


// set speed for move action (MoveNS/MoveWE) NOT slews!  This version DOESNT handle direction and expects a motor rate!
// if fast is true dont wait on response!  Used for psuedo-pulse guide
// NOTE that this will possibly mean the response will be read by a following command if it is called before
//      response comes from controller, since next command will flush before data is in buffer!
bool set_pmc8_axis_motor_rate(int fd, PMC8_AXIS axis, int mrate, bool fast)
{
    char cmd[24];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[24];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, sizeof(cmd), "ESSr%d%04X!", axis, mrate);

    if (pmc8_simulation)
    {
        return true;
    }

    if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if (fast)
    {
        return true;
    }

    snprintf(cmd, sizeof(cmd), "ESGr%d", axis);

    if ((errcode = get_pmc8_response(fd, response, &nbytes_read, cmd)))
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error setting axis motor rate");
        return false;
    }

    if (nbytes_read == 10)
    {
        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 10.", nbytes_read);
    return false;
}

// set speed for move action (MoveNS/MoveWE) NOT slews! This version accepts arcsec/sec as rate.
// also handles direction
bool set_pmc8_axis_move_rate(int fd, PMC8_AXIS axis, float rate)
{
    bool rc;
    int motor_rate;

    // set direction
    if (rate < 0)
        rc = set_pmc8_direction_axis(fd, axis, 0, false);
    else
        rc = set_pmc8_direction_axis(fd, axis, 1, false);

    if (!rc)
        return rc;

    if (!convert_move_rate_to_motor(fabs(rate), &motor_rate))
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error converting rate %f", rate);
        return false;
    }

    rc = set_pmc8_axis_motor_rate(fd, axis, motor_rate, false);

    if (pmc8_simulation)
    {
        simPMC8Data.moveRate = rate;
        return true;
    }

    return rc;
}

#if 0
bool set_pmc8_track_enabled(int fd, bool enabled)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, 32, ":ST%d#", enabled ? 1 : 0);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (pmc8_simulation)
    {
        // FIXME - need to implement pmc8 track enabled sim
        //        simPMC8Info.systemStatus = enabled ? ST_TRACKING_PEC_ON : ST_STOPPED;
        //        strcpy(response, "1");
        //        nbytes_read = strlen(response);

        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Need to implement pmc8 track enabled sim");
        return false;
    }
    else
    {
        // determine current tracking mode

        //        return SetTrackMode(enabled ? IUFindOnSwitchIndex(&TrackModeSP) : AP_TRACKING_OFF);
    }
}
#endif

bool set_pmc8_track_mode(int fd, uint8_t mode)
{
    float ratereal = 0;

    switch (mode)
    {
        case PMC8_TRACK_SIDEREAL:
            ratereal = PMC8_RATE_SIDEREAL;
            break;
        case PMC8_TRACK_LUNAR:
            ratereal = PMC8_RATE_LUNAR;
            break;
        case PMC8_TRACK_SOLAR:
            ratereal = PMC8_RATE_SOLAR;
            break;
        case PMC8_TRACK_KING:
            ratereal = PMC8_RATE_KING;
            break;
        default:
            return false;
            break;
    }

    if (!set_pmc8_direction_axis(fd, PMC8_AXIS_RA, pmc8_east_dir, false)) return false;
    return set_pmc8_custom_ra_track_rate(fd, ratereal);
}

// start tracking at a precision track rate
bool set_pmc8_ra_tracking(int fd, double rate)
{
    //set right direction
    int direction = pmc8_east_dir;
    if (rate < 0) direction = !direction;
    if (!set_pmc8_direction_axis(fd, PMC8_AXIS_RA, direction, false)) return false;

    //then set rate
    return (set_pmc8_custom_ra_track_rate(fd, fabs(rate)));
}

// just set the precision track rate - for when we've already set tracking direction
bool set_pmc8_custom_ra_track_rate(int fd, double rate)
{
    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "set_pmc8_custom_ra_track_rate() called rate=%f ", rate);

    char cmd[24];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[24];
    int nbytes_read    = 0;
    int nbytes_written = 0;
    int rateval;

    if (!convert_precise_rate_to_motor(rate, &rateval))
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error converting rate %f", rate);
        return false;
    }

    snprintf(cmd, sizeof(cmd), "ESTr%04X!", rateval);

    if (pmc8_simulation)
    {
        simPMC8Data.trackRate = rate;
        return true;
    }
    else
    {
        if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = get_pmc8_response(fd, response, &nbytes_read, "ESGx")))
        {
            DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error setting custom RA track rate");
            return false;
        }
    }

    if (nbytes_read != 9)
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 9.", nbytes_read);
        return false;
    }

    tcflush(fd, TCIFLUSH);

    // set direction to 1
    // return set_pmc8_direction_axis(fd, PMC8_AXIS_RA, 1, false);
    return true;
}

#if 0
bool set_pmc8_custom_dec_track_rate(int fd, double rate)
{
    bool rc;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "set_pmc8_custom_dec_track_rate() called rate=%f ", rate);

    if (pmc8_simulation)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "set_pmc8_custom_dec_track_rate simulation not implemented");

        rc = false;
    }
    else
    {
        rc = set_pmc8_axis_rate(fd, PMC8_AXIS_DEC, rate);
    }

    return rc;
}
#else
bool set_pmc8_custom_dec_track_rate(int fd, double rate)
{
    INDI_UNUSED(fd);
    INDI_UNUSED(rate);

    DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "set_pmc8_custom_dec_track_rate not implemented!");
    return false;
}
#endif

bool set_pmc8_custom_ra_move_rate(int fd, double rate)
{
    bool rc;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "set_pmc8_custom_ra move_rate() called rate=%f ", rate);

    // safe guard for now - only all use to STOP slewing or MOVE commands with this
    if (fabs(rate) > PMC8_MAX_MOVE_RATE)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "set_pmc8_custom_ra_move rate only supports low rates currently");

        return false;
    }

    rc = set_pmc8_axis_move_rate(fd, PMC8_AXIS_RA, rate);

    return rc;
}

bool set_pmc8_custom_dec_move_rate(int fd, double rate)
{
    bool rc;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "set_pmc8_custom_dec_move_rate() called rate=%f ", rate);

    // safe guard for now - only all use to STOP slewing with this
    if (fabs(rate) > PMC8_MAX_MOVE_RATE)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "set_pmc8_custom_dec_move_rate only supports low rates currently");
        return false;
    }

    rc = set_pmc8_axis_move_rate(fd, PMC8_AXIS_DEC, rate);

    return rc;
}

// rate is fraction of sidereal
bool set_pmc8_guide_rate(int fd, PMC8_AXIS axis, double rate)
{
    if (pmc8_simulation)
    {
        simPMC8Data.guide_rate = rate;
        return true;
    }

    // set driver values
    if (axis == PMC8_AXIS_RA)
    {
        pmc8_sidereal_rate_fraction_ra = rate;
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "set_pmc8_guide_rate: ra guide rate set to %f", rate);
    }
    if ((axis == PMC8_AXIS_DEC) || !pmc8_isRev2Compliant)
    {
        pmc8_sidereal_rate_fraction_de = rate;
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "set_pmc8_guide_rate: dec guide rate set to %f", rate);
    }

    if (pmc8_isRev2Compliant)
    {
        // now write to mount to sync ST4 rates
        char cmd[32], expresp[32];
        int errcode = 0;
        char errmsg[MAXRBUF];
        char response[16];
        int nbytes_read    = 0;
        int nbytes_written = 0;

        snprintf(cmd, sizeof(cmd), "ESSf%d%02X!", axis, int(rate * 100));

        if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        snprintf(expresp, sizeof(expresp), "ESGf%d%02X!", axis, int(rate * 100));

        if ((errcode = get_pmc8_response(fd, response, &nbytes_read, expresp)))
        {
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "SRF set cmd response incorrect: expected=%s", expresp);
            return false;
        }
    }

    return true;
}

// get SRF value for axis
bool get_pmc8_guide_rate(int fd, PMC8_AXIS axis, double &rate)
{
    if (pmc8_simulation)
    {
        rate =  simPMC8Data.guide_rate;
        return true;
    }

    // read from mount
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, sizeof(cmd), "ESGf%d!", axis);

    if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    cmd[5] = '\0';

    if ((errcode = get_pmc8_response(fd, response, &nbytes_read, cmd)))
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error getting SRF rate");
        return false;
    }

    if (nbytes_read != 8)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "SRF Get rate cmd response incorrect");
        return false;
    }

    char num_str[16] = {0};

    strcpy(num_str, "0X");
    strncat(num_str, response + 5, 2);
    int tint = strtol(num_str, nullptr, 0);

    rate = ((double)tint) / 100;

    // set driver values
    if (axis == PMC8_AXIS_RA)
    {
        pmc8_sidereal_rate_fraction_ra = rate;
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "get_pmc8_guide_rate: ra guide rate set to %f", rate);
    }
    else
    {
        pmc8_sidereal_rate_fraction_de = rate;
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "get_pmc8_guide_rate: dec guide rate set to %f", rate);
    }

    return true;
}

bool get_pmc8_guide_state(PMC8_DIRECTION gdir, PulseGuideState **pstate)
{

    switch (gdir)
    {
        case PMC8_N:
        case PMC8_S:
            *pstate = &NS_PulseGuideState;
            break;

        case PMC8_W:
        case PMC8_E:
            *pstate = &EW_PulseGuideState;
            break;

        default:
            return false;
            break;
    }
    return true;
}

// if return value is true then timetaken will return how much pulse time has already occurred
bool start_pmc8_guide(int fd, PMC8_DIRECTION gdir, int ms, long &timetaken_us, double ratehint)
{
    bool rc;
    double cur_rate = 0;
    int cur_dir = -1;

    // used to test timing
    struct timeval tp;
    long long pulse_start_us;
    long long pulse_sofar_us;

    PulseGuideState *pstate;

    if (!get_pmc8_guide_state(gdir, &pstate))
    {
        return false;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "pmc8_start_guide(): pulse dir=%d dur=%d ms", gdir, ms);

    if (pstate->pulseguideactive)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "pmc8_start_guide(): already executing a pulse guide!");
        return false;
    }

    // ignore short pulses - they do nothing
    if (ms < PMC8_PULSE_GUIDE_MIN_MS)
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "pmc8_start_guide(): ignore short pulse ms=%d ms", ms);
        timetaken_us = ms * 1000;
        pstate->pulseguideactive = true;
        pstate->fakepulse = true;
        return true;
    }

    // get precise tracking rate if in RA
    if ((gdir == PMC8_E) || (gdir == PMC8_W))
    {

        // use rate provided by interface if valid rather than querying for it
        if (ratehint <= 0)
        {
            rc = get_pmc8_track_rate(fd, cur_rate);
            if (!rc)
            {
                DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "pmc8_start_guide(): error reading current RA rate!");
                return rc;
            }
        }
        else
            cur_rate = ratehint;
    }
    // we could get slew rate if in DEC, but driver doesn't currently support DEC tracking
    // and we shouldn't get here if we're slewing, so for now we assume interface is always correct and avoid delay from unnecessary calls to mount
    else
    {
        cur_rate = ratehint;
    }

    // if slewing abort
    // shouldn't get here if slewing, but doesn't hurt to check
    if (cur_rate > PMC8_MAX_TRACK_RATE)
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR,
                     "pmc8_start_guide(): Cannot send guide correction while slewing! rate=%d dir=%d",
                     cur_rate, gdir);
        return rc;
    }

    double new_rate = cur_rate;
    int new_dir = 0;

    // RA guiding routine just changes the precision tracking call
    if ((gdir == PMC8_E) || (gdir == PMC8_W))
    {
        double guide_rate = pmc8_sidereal_rate_fraction_ra * PMC8_RATE_SIDEREAL;

        if (gdir == PMC8_E) new_rate -= guide_rate;
        else new_rate += guide_rate;

        if (new_rate < 0)
        {
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG,
                         "pmc8_start_guide(): with current tracking rate of %f, requested guide rate of %f would flip RA motor in opposite direction, so pausing motor instead.",
                         cur_rate, new_rate);
            new_rate = 0;
        }

        // measure time when we start pulse
        gettimeofday(&tp, nullptr);
        pulse_start_us = tp.tv_sec * 1000000 + tp.tv_usec;

        if (!set_pmc8_custom_ra_track_rate(fd, new_rate))
        {
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "pmc8_start_guide(): error settings new_rate to %f", new_rate);
            return false;
        }

    }
    // DEC guiding routine needs to set a DEC move rate and possibly a new direction
    else if ((gdir == PMC8_N) || (gdir == PMC8_S))
    {
        double guide_rate = pmc8_sidereal_rate_fraction_de * PMC8_RATE_SIDEREAL;

        if (gdir == PMC8_S) new_rate -= guide_rate;
        else new_rate += guide_rate;

        if (new_rate < 0) new_dir = 1;

        int mrate;
        if (!convert_move_rate_to_motor(fabs(new_rate), &mrate))
        {
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error converting rate %f", new_rate);
            return false;
        }

        // we should flip direction first so that we decrease the distance we could be going in the wrong direction
        // this is of course obvious with dec assumed to be 0, but just a reminder in case we ever support dec tracking

        // ideally, we would set direction only if needed
        // but based on our current assumptions, that could cost us an extra call to find out the current direction
        // so for now we'll always end up setting the direction
        if (cur_dir != new_dir)
            if (!set_pmc8_direction_axis(fd, PMC8_AXIS_DEC, new_dir, false))
                DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "pmc8_start_guide(): error setting new_dec_dir");

        // measure time when we start pulse
        gettimeofday(&tp, nullptr);
        pulse_start_us = tp.tv_sec * 1000000 + tp.tv_usec;

        if (!set_pmc8_axis_motor_rate(fd, PMC8_AXIS_DEC, mrate, false))
            DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "pmc8_start_guide(): error setting new_dec_rate");

    }
    // if for some reason the gdir is non-sensical ...
    else
    {
        return false;
    }

    // store state
    pstate->pulseguideactive = true;
    pstate->fakepulse = false;
    pstate->ms = ms;
    pstate->pulse_start_us = pulse_start_us;
    pstate->cur_rate  = cur_rate;
    pstate->cur_dir   = cur_dir;
    pstate->new_rate  = new_rate;
    pstate->new_dir   = new_dir;

    // see how long we've waited
    gettimeofday(&tp, nullptr);
    pulse_sofar_us = (tp.tv_sec * 1000000 + tp.tv_usec) - pulse_start_us;

    timetaken_us = pulse_sofar_us;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "pmc8_start_guide(): timetaken_us=%d us", timetaken_us);

    return true;
}

bool stop_pmc8_guide(int fd, PMC8_DIRECTION gdir)
{
    struct timeval tp;
    long long pulse_end_us;

    PulseGuideState *pstate;

    if (!get_pmc8_guide_state(gdir, &pstate))
    {
        return false;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "pmc8_stop_guide(): pulse dir=%d dur=%d ms", gdir, pstate->ms);

    if (!pstate->pulseguideactive)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "pmc8_stop_guide(): pulse guide not active!!");
        return false;
    }

    // flush any responses to commands we ignored above!
    tcflush(fd, TCIFLUSH);

    // "fake pulse" - it was so short we would have overshot its length AND the motors wouldn't have moved anyways
    if (pstate->fakepulse)
    {

        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "pmc8_stop_guide(): fake pulse done");
        pstate->pulseguideactive = false;
        return true;
    }

    gettimeofday(&tp, nullptr);
    pulse_end_us = tp.tv_sec * 1000000 + tp.tv_usec;

    if ((gdir == PMC8_E) || (gdir == PMC8_W))
    {
        if (!set_pmc8_custom_ra_track_rate(fd, pstate->cur_rate))
        {
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "pmc8_stop_guide(): error restoring tracking_rate to %f",
                         pstate->cur_rate);
            return false;
        }
    }
    else if ((gdir == PMC8_N) || (gdir == PMC8_S))
    {
        int mrate;

        if (!convert_move_rate_to_motor(fabs(pstate->cur_rate), &mrate))
        {
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error converting rate %f", pstate->cur_rate);
            return false;
        }

        // under assumption of no dec tracking, all we need to do is stop motion
        // but if dec tracking is ever supported, need to fix direction, and it may be better to do that first if cur_rate > new_rate
        if (!set_pmc8_axis_motor_rate(fd, PMC8_AXIS_DEC, mrate, false))
            DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "pmc8_stop_guide(): error returning to old move rate");
        // only change direction if needed
        if ((pstate->cur_rate != 0) && (pstate->cur_dir != pstate->new_dir))
            if (!set_pmc8_direction_axis(fd, PMC8_AXIS_DEC, pstate->cur_dir, false))
                DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "pmc8_stop_guide(): error returning to old direction");
    }
    else
    {
        return false;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "pmc8_stop_guide(): requested = %d ms, actual = %f ms",
                 pstate->ms, (pulse_end_us - pstate->pulse_start_us) / 1000.0);

    // flush any responses to commands we ignored above!
    tcflush(fd, TCIFLUSH);

    // mark pulse done
    pstate->pulseguideactive = false;

    return true;
}

// convert from axis position returned by controller to motor counts used in conversion to RA/DEC
int convert_axispos_to_motor(int axispos)
{
    int r;

    if (axispos > 8388608)
        r = 0 - (16777216 - axispos);
    else
        r = axispos;

    return r;
}

bool convert_ra_to_motor(double ra, INDI::Telescope::TelescopePierSide sop, int *mcounts)
{
    double motor_angle;
    double hour_angle;
    double lst;

    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "convert_ra_to_motor - ra=%f sop=%d", ra, sop);

    lst = get_local_sidereal_time(pmc8_longitude);

    hour_angle = lst - ra;

    // limit values to +/- 12 hours
    if (hour_angle > 12)
        hour_angle = hour_angle - 24;
    else if (hour_angle <= -12)
        hour_angle = hour_angle + 24;

    // Northern Hemisphere
    if (pmc8_east_dir)
    {
        if (sop == INDI::Telescope::PIER_EAST)
            motor_angle = hour_angle - 6;
        else if (sop == INDI::Telescope::PIER_WEST)
            motor_angle = hour_angle + 6;
        else
            return false;
    }
    // Southern Hemisphere
    else
    {
        if (sop == INDI::Telescope::PIER_EAST)
            motor_angle = -(hour_angle + 6);
        else if (sop == INDI::Telescope::PIER_WEST)
            motor_angle = -(hour_angle - 6);
        else
            return false;
    }


    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "convert_ra_to_motor - lst = %f hour_angle=%f", lst, hour_angle);

    *mcounts = motor_angle * PMC8_AXIS0_SCALE / 24;

    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "convert_ra_to_motor - motor_angle=%f *mcounts=%d", motor_angle, *mcounts);


    return true;
}

bool convert_motor_to_radec(int racounts, int deccounts, double &ra_value, double &dec_value)
{
    double motor_angle;
    double hour_angle;

    double lst;

    lst = get_local_sidereal_time(pmc8_longitude);

    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "lst = %f", lst);

    motor_angle = (24.0 * racounts) / PMC8_AXIS0_SCALE;

    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "racounts = %d  motor_angle = %f", racounts, motor_angle);

    // Northern Hemisphere
    if (pmc8_east_dir)
    {
        if (deccounts < 0)
            hour_angle = motor_angle + 6;
        else
            hour_angle = motor_angle - 6;
    }
    // Southern Hemisphere
    else
    {
        if (deccounts < 0)
            hour_angle = -(motor_angle + 6);
        else
            hour_angle = -(motor_angle - 6);
    }

    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "hour_angle = %f", hour_angle);

    ra_value = lst - hour_angle;

    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "ra_value  = %f", ra_value);

    if (ra_value >= 24.0)
        ra_value = ra_value - 24.0;
    else if (ra_value < 0.0)
        ra_value = ra_value + 24.0;

    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "ra_value (final) = %f", ra_value);

    motor_angle = (360.0 * deccounts) / PMC8_AXIS1_SCALE;

    // Northern Hemisphere
    if (pmc8_east_dir)
    {
        if (motor_angle >= 0)
            dec_value = 90 - motor_angle;
        else
            dec_value = 90 + motor_angle;
    }
    // Southern Hemisphere
    else
    {
        if (motor_angle >= 0)
            dec_value = -90 + motor_angle;
        else
            dec_value = -90 - motor_angle;
    }

    return true;
}

bool convert_dec_to_motor(double dec, INDI::Telescope::TelescopePierSide sop, int *mcounts)
{
    double motor_angle;

    // Northern Hemisphere
    if (pmc8_east_dir)
    {
        if (sop == INDI::Telescope::PIER_EAST)
            motor_angle = (dec - 90.0);
        else if (sop == INDI::Telescope::PIER_WEST)
            motor_angle = -(dec - 90.0);
        else
            return false;
    }
    // Southern Hemisphere
    else
    {
        if (sop == INDI::Telescope::PIER_EAST)
            motor_angle = -(dec + 90.0);
        else if (sop == INDI::Telescope::PIER_WEST)
            motor_angle = (dec + 90.0);
        else
            return false;
    }

    *mcounts = (motor_angle / 360.0) * PMC8_AXIS1_SCALE;

    //     DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "convert_dec_to_motor dec = %f, sop = %d", dec, sop);
    //     DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "convert_dec_to_motor motor_angle = %f, motor_counts= %d", motor_angle, *mcounts);

    return true;
}

bool set_pmc8_target_position_axis(int fd, PMC8_AXIS axis, int point)
{

    char cmd[32];
    char expresp[32];
    char hexpt[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    convert_motor_counts_to_hex(point, hexpt);

    // for v2+ firmware, use axis 2 if we don't want to track after the slew
    int naxis = axis;
    if (pmc8_isRev2Compliant && !axis && !pmc8_goto_resume) naxis = 2;
    snprintf(cmd, sizeof(cmd), "ESPt%d%s!", naxis, hexpt);

    if (!pmc8_simulation)
    {

        if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        snprintf(expresp, sizeof(expresp), "ESGt%d%s!", naxis, hexpt);

        if ((errcode = get_pmc8_response(fd, response, &nbytes_read, expresp)))
        {
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Axis Set Point cmd response incorrect: %s - expected %s", response,
                         expresp);
            return false;
        }
    }

    return true;
}

bool set_pmc8_target_position(int fd, int rapoint, int decpoint)
{
    bool rc;

    rc = set_pmc8_target_position_axis(fd, PMC8_AXIS_RA, rapoint);

    if (!rc)
        return rc;

    rc = set_pmc8_target_position_axis(fd, PMC8_AXIS_DEC, decpoint);

    return rc;
}


bool set_pmc8_position_axis(int fd, PMC8_AXIS axis, int point)
{

    char cmd[32];
    char expresp[32];
    char hexpt[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (pmc8_simulation)
    {
        // FIXME - need to implement simulation code for setting point position
        return true;
    }

    convert_motor_counts_to_hex(point, hexpt);
    snprintf(cmd, sizeof(cmd), "ESSp%d%s!", axis, hexpt);

    if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    snprintf(expresp, sizeof(expresp), "ESGp%d%s!", axis, hexpt);

    if ((errcode = get_pmc8_response(fd, response, &nbytes_read, expresp)))
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Axis Set Point cmd response incorrect: %s - expected %s", response,
                     expresp);
        return false;
    }

    return true;
}


bool set_pmc8_position(int fd, int rapoint, int decpoint)
{
    bool rc;

    rc = set_pmc8_position_axis(fd, PMC8_AXIS_RA, rapoint);

    if (!rc)
        return rc;

    rc = set_pmc8_position_axis(fd, PMC8_AXIS_DEC, decpoint);

    return rc;
}


bool get_pmc8_position_axis(int fd, PMC8_AXIS axis, int &point)
{

    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (pmc8_simulation)
    {
        // FIXME - need to implement simulation code for setting point position
        return true;
    }

    snprintf(cmd, sizeof(cmd), "ESGp%d!", axis);

    if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    cmd[5] = '\0';

    if ((errcode = get_pmc8_response(fd, response, &nbytes_read, cmd)))
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error getting position axis");
        return false;
    }

    if (nbytes_read != 12)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Axis Get Point cmd response incorrect");
        return false;
    }

    char num_str[16] = {0};

    strcpy(num_str, "0X");
    strncat(num_str, response + 5, 6);

    point = (int)strtol(num_str, nullptr, 0);

    return true;
}


bool get_pmc8_position(int fd, int &rapoint, int &decpoint)
{
    bool rc;
    int axis_ra_pos, axis_dec_pos;

    rc = get_pmc8_position_axis(fd, PMC8_AXIS_RA, axis_ra_pos);

    if (!rc)
        return rc;

    rc = get_pmc8_position_axis(fd, PMC8_AXIS_DEC, axis_dec_pos);

    if (!rc)
        return rc;

    // convert from axis position to motor counts
    rapoint = convert_axispos_to_motor(axis_ra_pos);
    decpoint = convert_axispos_to_motor(axis_dec_pos);

    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "ra  axis pos = 0x%x  motor_counts=%d",  axis_ra_pos,  rapoint);
    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "dec axis pos = 0x%x  motor_counts=%d", axis_dec_pos, decpoint);

    return rc;
}


bool park_pmc8(int fd)
{

    bool rc;

    rc = set_pmc8_target_position(fd, 0, 0);

    // FIXME - Need to add code to handle simulation and also setting any scope state values

    return rc;
}


bool unpark_pmc8(int fd)
{
    INDI_UNUSED(fd);

    // nothing really to do for PMC8 there is no unpark command

    if (pmc8_simulation)
    {
        set_pmc8_sim_system_status(ST_STOPPED);
        return true;
    }


    // FIXME - probably need to set a state variable to show we're unparked
    DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "PMC8 unparked");

    return true;
}

bool abort_pmc8(int fd)
{
    bool rc;

    if (pmc8_simulation)
    {
        // FIXME - need to do something to represent mount has stopped slewing
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "PMC8 slew stopped in simulation - need to add more code?");
        return true;
    }

    // stop move/slew rates
    rc = set_pmc8_custom_ra_move_rate(fd, 0);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error stopping RA axis!");
        return false;
    }

    rc = set_pmc8_custom_dec_move_rate(fd, 0);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error stopping DEC axis!");
        return false;
    }

    return true;
}

bool abort_pmc8_goto(int fd)
{
    char cmd[32];
    char expresp[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, sizeof(cmd), "ESPt300000!");

    if (!pmc8_simulation)
    {

        if ((errcode = send_pmc8_command(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        snprintf(expresp, sizeof(expresp), "ESGt3!");

        if ((errcode = get_pmc8_response(fd, response, &nbytes_read, expresp)))
        {
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Abort Goto cmd response incorrect: %s - expected %s", response,
                         expresp);
            return false;
        }
    }

    return true;
}

// "slew" on PMC8 is instantaneous once you set the target ra/dec
// no concept of setting target and then starting a slew operation as two steps
bool slew_pmc8(int fd, double ra, double dec)
{
    bool rc;
    int racounts, deccounts;
    INDI::Telescope::TelescopePierSide sop;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "slew_pmc8: ra=%f  dec=%f", ra, dec);

    sop = destSideOfPier(ra, dec);

    rc = convert_ra_to_motor(ra, sop, &racounts);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "slew_pmc8: error converting RA to motor counts");
        return false;
    }

    rc = convert_dec_to_motor(dec, sop, &deccounts);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "slew_pmc8: error converting DEC to motor counts");
        return false;
    }

    rc = set_pmc8_target_position(fd, racounts, deccounts);

    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error slewing PMC8");
        return false;
    }

    if (pmc8_simulation)
    {
        set_pmc8_sim_system_status(ST_SLEWING);
    }

    return true;
}

INDI::Telescope::TelescopePierSide destSideOfPier(double ra, double dec)
{
    double hour_angle;
    double lst;

    INDI_UNUSED(dec);

    lst = get_local_sidereal_time(pmc8_longitude);

    hour_angle = lst - ra;

    // limit values to +/- 12 hours
    if (hour_angle > 12)
        hour_angle = hour_angle - 24;
    else if (hour_angle <= -12)
        hour_angle = hour_angle + 24;

    // Northern Hemisphere
    if (pmc8_east_dir)
    {
        if (hour_angle < 0.0)
            return INDI::Telescope::PIER_WEST;
        else
            return INDI::Telescope::PIER_EAST;
    }
    //Southern Hemisphere
    else
    {
        if (hour_angle < 0.0)
            return INDI::Telescope::PIER_EAST;
        else
            return INDI::Telescope::PIER_WEST;
    }
}

bool sync_pmc8(int fd, double ra, double dec)
{
    bool rc;
    int racounts, deccounts;
    INDI::Telescope::TelescopePierSide sop;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "sync_pmc8: ra=%f  dec=%f", ra, dec);

    sop = destSideOfPier(ra, dec);

    rc = convert_ra_to_motor(ra, sop, &racounts);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "sync_pmc8: error converting RA to motor counts");
        return false;
    }

    rc = convert_dec_to_motor(dec, sop, &deccounts);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "sync_pmc8: error converting DEC to motor counts");
        return false;
    }

    if (pmc8_simulation)
    {
        // FIXME - need to implement pmc8 sync sim
        //        strcpy(response, "1");
        //        nbytes_read = strlen(response);
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Need to implement PMC8 sync simulation");
        return false;
    }
    else
    {
        rc = set_pmc8_position(fd, racounts, deccounts);
    }

    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error setting pmc8 position");
        return false;
    }

    return true;
}

bool set_pmc8_radec(int fd, double ra, double dec)
{
    bool rc;
    int racounts, deccounts;
    INDI::Telescope::TelescopePierSide sop;


    sop = destSideOfPier(ra, dec);

    rc = convert_ra_to_motor(ra, sop, &racounts);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "set_pmc8_radec: error converting RA to motor counts");
        return false;
    }

    rc = convert_dec_to_motor(ra, sop, &deccounts);
    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "set_pmc8_radec: error converting DEC to motor counts");
        return false;
    }

    if (pmc8_simulation)
    {
        // FIXME - need to implement pmc8 sync sim
        //        strcpy(response, "1");
        //        nbytes_read = strlen(response);
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Need to implement PMC8 sync simulation");
        return false;
    }
    else
    {

        rc = set_pmc8_target_position(fd, racounts, deccounts);
    }

    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error setting target position");
        return false;
    }

    return true;
}

bool get_pmc8_coords(int fd, double &ra, double &dec)
{
    int racounts, deccounts;
    bool rc;

    if (pmc8_simulation)
    {
        // sortof silly but convert simulated RA/DEC to counts so we can then convert
        // back to RA/DEC to test that conversion code
        INDI::Telescope::TelescopePierSide sop;

        sop = destSideOfPier(simPMC8Data.ra, simPMC8Data.dec);

        rc = convert_ra_to_motor(simPMC8Data.ra, sop, &racounts);

        if (!rc)
            return rc;

        rc = convert_dec_to_motor(simPMC8Data.dec, sop, &deccounts);

        if (!rc)
            return rc;
    }
    else
    {
        rc = get_pmc8_position(fd, racounts, deccounts);
    }

    if (!rc)
    {
        DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "Error getting PMC8 motor position");
        return false;
    }

    // convert motor counts to ra/dec
    convert_motor_to_radec(racounts, deccounts, ra, dec);

    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "ra  motor_counts=%d  RA  = %f", racounts, ra);
    //    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "dec motor_counts=%d  DEC = %f", deccounts, dec);

    return rc;
}

// wrap read commands to PMC8
bool get_pmc8_response(int fd, char* buf, int *nbytes_read, const char* expected = NULL )
{
    int err_code = 1;
    int cnt = 0;

    //repeat a few times, after that, let's assume we're not getting a response
    while ((err_code) && (cnt++ < PMC8_MAX_RETRIES))
    {
        //Read until exclamation point to get response
        if ((err_code = tty_read_section(fd, buf, '!', PMC8_TIMEOUT, nbytes_read)))
        {

            char errmsg[MAXRBUF];
            tty_error_msg(err_code, errmsg, MAXRBUF);

            // if we see connection timed out, exit out of here and try to reconnect
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "Read error: %s", errmsg);
            if (strstr(errmsg, "Connection timed out") || strstr(errmsg, "Bad"))
            {
                set_pmc8_reconnect_flag();
                return err_code;
            }
        }
        if (*nbytes_read > 0)
        {
            buf[*nbytes_read] = '\0';
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES %d bytes (%s)", *nbytes_read, buf);

            //PMC8 connection is not entirely reliable when using Ethernet instead of Serial connection.
            //So, try to compensate for common problems
            if (pmc8_connection == PMC8_ETHERNET)
            {
                //One problem is we get the string *HELLO* when we connect or disconnect, so discard that
                if (buf[0] == '*')
                {
                    strcpy(buf, buf + 7);
                    *nbytes_read = *nbytes_read - 7;
                }
                //Another problem is we sometimes get the string AT when we reconnect, so discard that
                if (strncmp(buf, "AT", 2) == 0)
                {
                    strcpy(buf, buf + 2);
                    *nbytes_read = *nbytes_read - 2;
                }
                //Another problem is random extraneous ESGp! reponses during slew, so when we see those, drop them and try again
                if (strncmp(buf, "ESGp!", 5) == 0)
                {
                    err_code = 1;
                    DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "Invalid response ESGp!");
                }
            }
            //If a particular response was expected, make sure we got it
            if (expected)
            {
                if (strncmp(buf, expected, strlen(expected)) != 0)
                {
                    err_code = 1;
                    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_EXTRA_1, "No Match for %s", expected);
                }
                else
                {
                    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_EXTRA_1, "Matches %s", expected);
                    // On rare occasions, there may have been a read error even though it's the response we want, so set err_code explicitly
                    err_code = 0;
                }
            }
        }
        else
        {
            DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "No Response");
            err_code = 1;
        }
    }
    // if this is our nth consecutive read error, try to reconnect
    if (err_code)
    {
        if (++pmc8_io_error_ctr > PMC8_MAX_IO_ERROR_THRESHOLD) set_pmc8_reconnect_flag();
    }
    else
    {
        pmc8_io_error_ctr = 0;
    }
    return err_code;
}

//wrap write commands to pmc8
bool send_pmc8_command(int fd, const char *buf, int nbytes, int *nbytes_written)
{

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", buf);

    tcflush(fd, TCIFLUSH);

    int err_code = 1;
    //try to reconnect if we see broken pipe error
    if ((err_code = tty_write(fd, buf, nbytes, nbytes_written)))
    {
        char errmsg[MAXRBUF];
        tty_error_msg(err_code, errmsg, MAXRBUF);
        if (strstr(errmsg, "Broken pipe") || strstr(errmsg, "Bad"))
        {
            set_pmc8_reconnect_flag();
            return err_code;
        }
    }
    return err_code;
}

void set_pmc8_reconnect_flag()
{
    DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Bad connection. Trying to reconnect.");
    pmc8_reconnect_flag = true;
}

bool get_pmc8_reconnect_flag()
{
    if (pmc8_reconnect_flag)
    {
        pmc8_reconnect_flag = false;
        return true;
    }
    return false;
}

void set_pmc8_goto_resume(bool resume)
{
    pmc8_goto_resume = resume;
}
