/*
    INDI Explore Scientific PMC8 driver

    Copyright (C) 2017 Michael Fulbright

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

#include <libnova/julian_day.h>

#include <math.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define PMC8_TIMEOUT 5 /* FD timeout in seconds */

#define PMC8_SIMUL_VERSION_RESP "ESGvES06B9T9"

// FIXME - (MSF) these should be read from the controller? Depends on mount type.
#define PMC8_AXIS0_SCALE 4608000.0
#define PMC8_AXIS1_SCALE 4608000.0
#define ARCSEC_IN_CIRCLE 1296000.0

// FIXME - (MSF) just placeholders need better way to represent
//         This value is from PMC8 SDK document
#define PMC8_MAX_MOTOR_RATE 2641

bool pmc8_debug                 = false;
bool pmc8_simulation            = false;
char pmc8_device[MAXINDIDEVICE] = "iEQ";
IEQInfo simInfo;

struct
{
    double ra;
    double dec;
    double guide_rate;
} simData;

void set_pmc8_debug(bool enable)
{
    pmc8_debug = enable;
}

void set_pmc8_simulation(bool enable)
{
    pmc8_simulation = enable;
    if (enable)
        simData.guide_rate = 0.5;
}

void set_pmc8_device(const char *name)
{
    strncpy(pmc8_device, name, MAXINDIDEVICE);
}

#if 0
void set_sim_gps_status(IEQ_GPS_STATUS value)
{
    simInfo.gpsStatus = value;
}

void set_sim_system_status(IEQ_SYSTEM_STATUS value)
{
    simInfo.systemStatus = value;
}

void set_sim_track_rate(IEQ_TRACK_RATE value)
{
    simInfo.trackRate = value;
}

void set_sim_slew_rate(IEQ_SLEW_RATE value)
{
    simInfo.slewRate = value;
}

void set_sim_time_source(IEQ_TIME_SOURCE value)
{
    simInfo.timeSource = value;
}

void set_sim_hemisphere(IEQ_HEMISPHERE value)
{
    simInfo.hemisphere = value;
}
#endif // prob not needed pmc8

void set_sim_ra(double ra)
{
    simData.ra = ra;
}

void set_sim_dec(double dec)
{
    simData.dec = dec;
}

void set_sim_guide_rate(double rate)
{
    simData.guide_rate = rate;
}

bool check_pmc8_connection(int fd)
{
    char initCMD[] = "ESGv!";
    int errcode    = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "Initializing PMC8 using ESGv! CMD...");

    for (int i = 0; i < 2; i++)
    {
        if (pmv8_simulation)
        {
            strcpy(response, PMC8_SIMUL_VERSION_RESP);
            nbytes_read = strlen(response);
        }
        else
        {
            tcflush(fd, TCIFLUSH);

            if ((errcode = tty_write(fd, initCMD, 3, &nbytes_written)) != TTY_OK)
            {
                tty_error_msg(errcode, errmsg, MAXRBUF);
                DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
                usleep(50000);
                continue;
            }

            if ((errcode = tty_read_section(fd, response, '#', IEQPRO_TIMEOUT, &nbytes_read)))
            {
                tty_error_msg(errcode, errmsg, MAXRBUF);
                DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
                usleep(50000);
                continue;
            }
        }

        if (nbytes_read > 0)
        {
            response[nbytes_read] = '\0';
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

            // FIXME - (MSF) need to put in better check for a valid firmware version response
            if (!strncmp(response, "ESGvES", 6))
                return true;
        }

        usleep(50000);
    }

    return false;
}

bool get_pmc8_status(int fd, IEQInfo *info)
{
    char cmd[]  = ":GAS#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_EXTRA_1, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        snprintf(response, 8, "%d%d%d%d%d%d#", simInfo.gpsStatus, simInfo.systemStatus, simInfo.trackRate,
                 simInfo.slewRate + 1, simInfo.timeSource + 1, simInfo.hemisphere);
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read_section(fd, response, '#', IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_EXTRA_1, "RES (%s)", response);

        if (nbytes_read == 7)
        {
            info->gpsStatus    = (IEQ_GPS_STATUS)(response[0] - '0');
            info->systemStatus = (IEQ_SYSTEM_STATUS)(response[1] - '0');
            info->trackRate    = (IEQ_TRACK_RATE)(response[2] - '0');
            info->slewRate     = (IEQ_SLEW_RATE)(response[3] - '0' - 1);
            info->timeSource   = (IEQ_TIME_SOURCE)(response[4] - '0' - 1);
            info->hemisphere   = (IEQ_HEMISPHERE)(response[5] - '0');

            tcflush(fd, TCIFLUSH);

            return true;
        }
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 7.", nbytes_read);
    return false;
}

bool get_pmc8_firmware(int fd, FirmwareInfo *info)
{
    bool rc = false;

    rc = get_pmc8_model(fd, info);

    if (rc == false)
        return rc;

    rc = get_pmc8_main_firmware(fd, info);

    return rc;

}

bool get_pmc8_model(int fd, FirmwareInfo *info)
{
    // FIXME - only one model for now
    info->Model.assign("PMC-Eight");
    return true;
}

bool get_pmc8_main_firmware(int fd, FirmwareInfo *info)
{
    char cmd[]  = "ESGv!";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[24];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (pmc8_simulation)
    {
        strcpy(response, PMC8_SIMUL_VERSION_RESP);
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read_section(fd, response, '#', IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        if (nbytes_read == 13)
        {
            char firm[8] = {0};

            response[12] = '\0';
            strncpy(board, response+6, 6);

            info->MainBoardFirmware.assign(board, 6);

            tcflush(fd, TCIFLUSH);

            return true;
        }
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 13.", nbytes_read);
    return false;
}

#if 0
// these commands dont exist for PMC8
bool start_ieqpro_motion(int fd, IEQ_DIRECTION dir)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    int nbytes_written = 0;

    switch (dir)
    {
        case IEQ_N:
            strcpy(cmd, ":mn#");
            break;
        case IEQ_S:
            strcpy(cmd, ":ms#");
            break;
        case IEQ_W:
            strcpy(cmd, ":mw#");
            break;
        case IEQ_E:
            strcpy(cmd, ":me#");
            break;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
        return true;

    tcflush(fd, TCIFLUSH);

    if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    tcflush(fd, TCIFLUSH);
    return true;
}

bool stop_ieqpro_motion(int fd, IEQ_DIRECTION dir)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    switch (dir)
    {
        case IEQ_N:
        case IEQ_S:
            strcpy(cmd, ":qD#");
            break;

        case IEQ_W:
        case IEQ_E:
            strcpy(cmd, ":qR#");
            break;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}
#endif


#if 0
// PMC8 slew rate is 25x the tracking rate - no need to set
bool set_pmc8_slew_rate(int fd, IEQ_SLEW_RATE rate)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, 16, ":SR%d#", ((int)rate) + 1);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        simInfo.slewRate = rate;
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}
#endif

// convert rate in arcsec/sidereal_second to internal PMC8 motor rate
bool convert_rate_to_motor(float rate, int *mrate)
{

    *mrate = (int)(25*rate/(PMC8_AXIS0_SCALE/ARCSEC_IN_CIRCLE));

    if (*mrate > PMC8_MAX_MOTOR_RATE)
        *mrate = PMC8_MAX_MOTOR_RATE;
    else if (*mrate < -PMC8_MAX_MOTOR_RATE)
        *mrate = -PMC8_MAX_MOTOR_RATE;

    return true;
}

bool set_pmc8_axis_rate(int fd, PMC8_AXIS axis, float rate)
{
    char cmd[24];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[24];
    int nbytes_read    = 0;
    int nbytes_written = 0;
    int rateval;

    if (!convert_rate_to_motor(rate, &rateval))
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error converting rate %f", rate);
        return false;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "PMC8 internal rate %d for requested rate %f", rateval, rate);

    snprintf(cmd, sizeof(cmd), "ESSr%d%05X!", axis, rateval);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (pmc8_simulation)
    {
        simInfo.trackRate = rate;
        return true;
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, strlen(cmd), IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read == 13)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 13.", nbytes_read);
    return false;
}

bool set_pmc8_track_mode(int fd, PMC8_TRACK_RATE rate)
{
    float ratereal;

    switch (rate)
    {
        case TR_SIDEREAL:
            ratereal = 15.0;
            break;
        case TR_LUNAR:
            ratereal = 14.453;
            break;
        case TR_SOLAR:
            ratereal = 15.041;
            break;
    }

    rc = set_pmc8_axis_rate(fd, RA_AXIS, ratereal);

    return rc;
}

#if 0
// not yet implemented for PMC8
bool set_ieqpro_custom_ra_track_rate(int fd, double rate)
{
    char cmd[16];
    char sign;
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (rate < 0)
        sign = '-';
    else
        sign = '+';

    snprintf(cmd, 16, ":RR%c%07.4f#", sign, fabs(rate));

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}
#endif
#if 0
// not yet implemented for PMC8
bool set_ieqpro_custom_de_track_rate(int fd, double rate)
{
    char cmd[16];
    char sign;
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (rate < 0)
        sign = '-';
    else
        sign = '+';

    snprintf(cmd, 16, ":RD%c%07.4f#", sign, fabs(rate));

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}
#endif
#if 0
// not yet implemented for PMC8
bool set_ieqpro_guide_rate(int fd, double rate)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    int num = rate * 100;
    snprintf(cmd, 16, ":RG%03d#", num);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        simData.guide_rate = rate;
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}
#endif
#if 0
// not yet implemented for PMC8
bool get_ieqpro_guide_rate(int fd, double *rate)
{
    char cmd[]  = ":AG#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        snprintf(response, 8, "%3d#", (int)(simData.guide_rate * 100));
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 4, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        int rate_num;

        if (sscanf(response, "%d#", &rate_num) > 0)
        {
            *rate = rate_num / 100.0;
            tcflush(fd, TCIFLUSH);
            return true;
        }
        else
        {
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error: Malformed result (%s).", response);
            return false;
        }
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}
#endif
#if 0
// not yet implemented for PMC8
bool start_ieqpro_guide(int fd, IEQ_DIRECTION dir, int ms)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    int nbytes_written = 0;

    char dir_c = 0;

    switch (dir)
    {
        case IEQ_N:
            dir_c = 'n';
            break;

        case IEQ_S:
            dir_c = 's';
            break;

        case IEQ_W:
            dir_c = 'w';
            break;

        case IEQ_E:
            dir_c = 'e';
            break;
    }

    snprintf(cmd, 16, ":M%c%05d#", dir_c, ms);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
        return true;
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    tcflush(fd, TCIFLUSH);
    return true;
}
#endif


bool set_pmc8_point_position_axis(int fd, PMC8_AXIS axis, int point)
{

    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[16];
    int nbytes_read    = 0;
    int nbytes_written = 0;


    if (pmc8_simulation)
    {
        // FIXME - (MSF) - need to implement simulation code for setting point position
        return true;
    }


    snprintf(cmd, sizeof(cmd), "ESPt%d%d!", axis, point);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);


    tcflush(fd, TCIFLUSH);

    if ((errcode = tty_write(fd, cmdra, strlen(cmdra), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if ((errcode = tty_read(fd, response, strlen(cmdra), PMC8_TIMEOUT, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGFDEVICE(pmc8, INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    response[nbytes_read] = '\0';

    if (nbytes_read > 0)
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);


    if (strncmp(cmd, response, strlen(cmd)))
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Axis Set Point cmd response incorrect: %s - expected %s", response, cmd);
        return false;
    }

    return true;
}


bool set_pmc8_point_position(int fd, int rapoint, decpoint)
{
    bool rc;

    rc = set_point_position_axis(fd, RA_AXIS, rapoint);

    if (!rc)
        return rc;

    rc = set_point_position_axis(fd, DEC_AXIS, decpoint);
}

bool park_pmc8(int fd)
{

    bool rc;

    rc = set_pmc8_point_position(fd, 0, 0);

    // FIXME - (MSF) Need to add code to handle simulation and also setting any scope state values

    return rc;
}


bool unpark_pmc8(int fd)
{
    //char cmd[]  = ":MP0#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    // nothing really to do for PMC8 there is no unpark command

    if (pmc8_simulation)
    {
        // FIXME - (MSF) need to do something in simulation to show it is unparked?
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "PMC8 unparked in simulation - need to add more code?");
        return true;
    }


    // FIXME - (MSF) probably need to set a state variable to show we're unparked
    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "PMC8 unparked");

    return true;

}

bool abort_pmc8(int fd)
{
    bool rc;


    if (pmc8_simulation)
    {
        // FIXME - (MSF) need to do something to represent mount has stopped slewing
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "PMC8 slew stopped in simulation - need to add more code?");
        return true;
    }

    rc = set_pmc8_axis_rate(fd, RA_AXIS, 0);
    if (!rc)
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error stopping RA axis!");
        return false;
    }

    rc = set_pmc8_axis_rate(fd, DEC_AXIS, 0);
    if (!rc)
    {
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error stopping DEC axis!");
        return false;
    }

    return true;
}

bool convert_ra_to_motor(double ra, TelescopePierSide sop, int *mcounts)
{
    double motor_angle, hour_angle;

    hour_angle = SiderealTime - ra;

    // limit values to +/- 12 hours
    if (hour_angle > 12)
        hour_angle = hour_angle - 24;
    else if (hour_angle <= -12)
        hour_angle = hour_angle + 24;


    if (sop == INDI::Telescope::PIER_EAST)
        motor_angle = hour_angle - 6;
    else if (sop == INDI::Telescope::PIER_WEST)
        motor_angle = hour_angle + 6;
    else
        return false;

    *mcounts = motor_angle * PMC8_AXIS0_SCALE / 24;

    return true;
}

bool convert_motor_to_ra
{
    Public Function MotorCounts_to_RA(MC_value As Int32) As Double
        Dim MotorAngle As Double
        Dim RA_value As Double
        Dim HourAngle As Double
        Dim DECCounts As Int32

        DECCounts = GetDECMotorPosition()

        MotorAngle = (24.0# * MC_value) / Telescope.MountRACCounts

        If DECCounts < 0 Then
            HourAngle = MotorAngle + 6
        ElseIf DECCounts >= 0 Then
            HourAngle = MotorAngle - 6
        End If

        RA_value = SiderealTime - HourAngle

        If RA_value >= 24.0# Then
            RA_value = RA_value - 24.0#
        ElseIf RA_value < 0.0# Then
            RA_value = RA_value + 24.0#
        End If

        Return RA_value
    End Function
}

bool convert_dec_to_motor(double dec, TelescopePierSide sop, int *mcounts)
{
    double motor_angle;

    if (sop == INDI::Telescope::PIER_EAST)
        motor_angle = (dec - 90.0);
    else if (sop == INDI::Telescope::PIER_WEST)
        motor_angle = -(dec - 90.0);
    else
        return false;

     *mcounts = (motor_angle / 360.0) * PMC8_AXIS1_SCALE;

     return true;
}

bool convert_motor_to_dec()
{
    Public Function MotorCounts_to_DEC(MC_value As Int32) As Double
        Dim MotorAngle As Double
        Dim DEC_value As Double

        MotorAngle = (360.0# * MC_value) / Telescope.MountDECCounts

        If MotorAngle >= 0 Then
            DEC_value = 90 - MotorAngle
        ElseIf MotorAngle < 0 Then
            DEC_value = 90 + MotorAngle
        End If

        Return DEC_value
    End Function
}

bool slew_pmc8(int fd)
{
    char cmd[]  = ":MS#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        simInfo.rememberSystemStatus = simInfo.systemStatus;
        simInfo.systemStatus = ST_SLEWING;
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        if (!strcmp(response, "1"))
        {
            tcflush(fd, TCIFLUSH);
            return true;
        }
        else
        {
            DEBUGDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Requested object is below horizon.");
            tcflush(fd, TCIFLUSH);
            return false;
        }
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool sync_ieqpro(int fd)
{
    char cmd[]  = ":CM#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool set_ieqpro_track_enabled(int fd, bool enabled)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, 32, ":ST%d#", enabled ? 1 : 0);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        simInfo.systemStatus = enabled ? ST_TRACKING_PEC_ON : ST_STOPPED;
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool set_ieqpro_ra(int fd, double ra)
{
    char cmd[32];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    // Send as milliseconds resolution
    int ieqValue = ra * 60 * 60 * 1000;

    snprintf(cmd, 32, ":Sr%08d#", ieqValue);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        simData.ra = ra;
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool set_ieqpro_dec(int fd, double dec)
{
    char cmd[32];
    char sign;
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (dec >= 0)
        sign = '+';
    else
        sign = '-';

    // Send as 0.01 arcseconds resolution
    int ieqValue = fabs(dec) * 60 * 60 * 100;

    snprintf(cmd, 32, ":Sd%c%08d#", sign, ieqValue);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        simData.dec = dec;
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool set_ieqpro_longitude(int fd, double longitude)
{
    char cmd[16];
    char sign;
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (longitude >= 0)
        sign = '+';
    else
        sign = '-';

    int longitude_arcsecs = fabs(longitude) * 60 * 60;
    snprintf(cmd, 16, ":Sg%c%06d#", sign, longitude_arcsecs);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool set_ieqpro_latitude(int fd, double latitude)
{
    char cmd[16];
    char sign;
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (latitude >= 0)
        sign = '+';
    else
        sign = '-';

    int latitude_arcsecs = fabs(latitude) * 60 * 60;
    snprintf(cmd, 16, ":St%c%06d#", sign, latitude_arcsecs);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool get_ieqpro_longitude(int fd, double *longitude)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    strcpy(cmd, ":Gg#");

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "+172800");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
        if ((errcode = tty_read_section(fd, response, '#', IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);

        int longitude_arcsecs = 0;

        if (sscanf(response, "%d#", &longitude_arcsecs) > 0)
        {
            *longitude = longitude_arcsecs / 3600.0;
            return true;
        }

        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error: Malformed result (%s).", response);
        return false;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 8.", nbytes_read);
    return false;
}

bool get_ieqpro_latitude(int fd, double *latitude)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    strcpy(cmd, ":Gt#");

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "+106200");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
        if ((errcode = tty_read_section(fd, response, '#', IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);

        int latitude_arcsecs = 0;

        if (sscanf(response, "%d#", &latitude_arcsecs) > 0)
        {
            *latitude = latitude_arcsecs / 3600.0;
            return true;
        }

        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Error: Malformed result (%s).", response);
        return false;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 8.", nbytes_read);
    return false;
}

bool set_ieqpro_local_date(int fd, int yy, int mm, int dd)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, 16, ":SC%02d%02d%02d#", yy, mm, dd);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool set_ieqpro_local_time(int fd, int hh, int mm, int ss)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    snprintf(cmd, 16, ":SL%02d%02d%02d#", hh, mm, ss);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool set_ieqpro_daylight_saving(int fd, bool enabled)
{
    char cmd[16];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (enabled)
        strcpy(cmd, ":SDS1#");
    else
        strcpy(cmd, ":SDS0#");

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool set_ieqpro_utc_offset(int fd, double offset)
{
    char cmd[16];
    char sign;
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[8];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    if (offset >= 0)
        sign = '+';
    else
        sign = '-';

    int offset_minutes = fabs(offset) * 60.0;

    snprintf(cmd, 16, ":SG%c%03d#", sign, offset_minutes);

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        strcpy(response, "1");
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read(fd, response, 1, IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(fd, TCIFLUSH);
        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool get_ieqpro_coords(int fd, double *ra, double *dec)
{
    char cmd[]  = ":GEC#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[32];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_EXTRA_1, "CMD (%s)", cmd);

    if (ieqpro_simulation)
    {
        char ra_str[16], dec_str[16];

        char sign;
        if (simData.dec >= 0)
            sign = '+';
        else
            sign = '-';

        int ieqDEC = fabs(simData.dec) * 60 * 60 * 100;

        snprintf(dec_str, 16, "%c%08d", sign, ieqDEC);

        int ieqRA = simData.ra * 60 * 60 * 1000;
        snprintf(ra_str, 16, "%08d", ieqRA);

        snprintf(response, 32, "%s%s#", dec_str, ra_str);
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read_section(fd, response, '#', IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        tcflush(fd, TCIFLUSH);
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_EXTRA_1, "RES (%s)", response);

        char ra_str[16]= {0}, dec_str[16] = {0};

        strncpy(dec_str, response, 9);
        strncpy(ra_str, response + 9, 8);

        int ieqDEC = atoi(dec_str);
        int ieqRA  = atoi(ra_str);

        *ra  = ieqRA / (60.0 * 60.0 * 1000.0);
        *dec = ieqDEC / (60.0 * 60.0 * 100.0);

        return true;
    }

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool get_ieqpro_utc_date_time(int fd, double *utc_hours, int *yy, int *mm, int *dd, int *hh, int *minute, int *ss)
{
    char cmd[]  = ":GLT#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[32];
    int nbytes_read    = 0;
    int nbytes_written = 0;

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    // Format according to Manual is sMMMYYMMDDHHMMSS#
    // However as pointed out by user Shepherd on INDI forums, actual format is
    // sMMMxYYMMDDHHMMSS#
    // Where x is either 0 or 1 denoting daying savings
    if (ieqpro_simulation)
    {
        strncpy(response, "+1800150331173000#", 32);
        nbytes_read = strlen(response);
    }
    else
    {
        tcflush(fd, TCIFLUSH);

        if ((errcode = tty_write(fd, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }

        if ((errcode = tty_read_section(fd, response, '#', IEQPRO_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        tcflush(fd, TCIFLUSH);
        response[nbytes_read] = '\0';
        DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        char utc_str[8]={0}, yy_str[8]={0}, mm_str[8]={0}, dd_str[8]={0}, hh_str[8]={0}, minute_str[8]={0}, ss_str[8]={0}, dst_str[8]={0};

        // UTC Offset
        strncpy(utc_str, response, 4);
        // Daylight savings
        strncpy(dst_str, response + 4, 1);
        // Year
        strncpy(yy_str, response + 5, 2);
        // Month
        strncpy(mm_str, response + 7, 2);
        // Day
        strncpy(dd_str, response + 9, 2);
        // Hour
        strncpy(hh_str, response + 11, 2);
        // Minute
        strncpy(minute_str, response + 13, 2);
        // Second
        strncpy(ss_str, response + 15, 2);

        *utc_hours = atoi(utc_str) / 60.0;
        *yy        = atoi(yy_str) + 2000;
        *mm        = atoi(mm_str) + 1;
        *dd        = atoi(dd_str);
        *hh        = atoi(hh_str);
        *minute    = atoi(minute_str);
        *ss        = atoi(ss_str);

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

    DEBUGFDEVICE(pmc8_device, INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

