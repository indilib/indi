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

#pragma once

#include <string>

#include "inditelescope.h"

// if tracking speed is above this (arcsec / sec) then assume mount is slewing
// this is just less than 3x sidereal
// which is what we would normally see if we are tracking
// and start moving east at the min move rate (4x sidereal)
#define PMC8_MAX_TRACK_RATE 44

// set max settable slew rate as 833x sidereal
#define PMC8_MAX_MOVE_RATE (833*15)

// JM 2024.12.03: Since INDI tracking rate is defined as arcsecs per second (SOLAR second), we need to convert from solar to sidereal
#define SOLAR_SECOND 1.00278551532

typedef enum
{
    ST_STOPPED,
    ST_TRACKING,
    ST_SLEWING,
    ST_GUIDING,
    ST_MERIDIAN_FLIPPING,
    ST_PARKED,
    ST_HOME
} PMC8_SYSTEM_STATUS;

//#endif

typedef enum { PMC8_TRACK_SIDEREAL, PMC8_TRACK_LUNAR, PMC8_TRACK_SOLAR, PMC8_TRACK_CUSTOM, PMC8_TRACK_KING, PMC8_TRACK_UNDEFINED } PMC8_TRACK_RATE;

//typedef enum { HEMI_SOUTH, HEMI_NORTH } PMC8_HEMISPHERE;

typedef enum { PMC8_AXIS_RA = 0, PMC8_AXIS_DEC = 1 } PMC8_AXIS;
typedef enum { PMC8_N, PMC8_S, PMC8_W, PMC8_E } PMC8_DIRECTION;

typedef enum { MOUNT_G11 = 0, MOUNT_EXOS2 = 1, MOUNT_iEXOS100 = 2 } PMC8_MOUNT_TYPES;

typedef enum { PMC8_SERIAL_AUTO, PMC8_SERIAL_INVERTED, PMC8_SERIAL_STANDARD, PMC8_ETHERNET } PMC8_CONNECTION_TYPE;

typedef struct PMC8Info
{
    PMC8_SYSTEM_STATUS systemStatus;
    PMC8_SYSTEM_STATUS rememberSystemStatus;
    //    PMC8_HEMISPHERE hemisphere;
} PMC8Info;

typedef struct FirmwareInfo
{
    std::string Model;
    std::string MainBoardFirmware;
    PMC8_MOUNT_TYPES MountType;
    bool IsRev2Compliant;
} FirmwareInfo;

/**************************************************************************
 Misc.
**************************************************************************/

void set_pmc8_debug(bool enable);
void set_pmc8_simulation(bool enable);
void set_pmc8_device(const char *name);
void set_pmc8_mountParameters(int index);
bool get_pmc8_response(int fd, char* buf, int* nbytes_read, const char* expected);
bool send_pmc8_command(int fd, const char *buf, int nbytes, int *nbytes_written);

/**************************************************************************
 Simulation
**************************************************************************/
void set_pmc8_sim_system_status(PMC8_SYSTEM_STATUS value);
void set_pmc8_sim_track_rate(PMC8_TRACK_RATE value);
void set_pmc8_sim_move_rate(int value);
//void set_sim_hemisphere(IEQ_HEMISPHERE value);
void set_pmc8_sim_ra(double ra);
void set_pmc8_sim_dec(double dec);
//void set_sim_guide_rate(double rate);

/**************************************************************************
 Diagnostics
**************************************************************************/
bool check_pmc8_connection(int fd, PMC8_CONNECTION_TYPE isSerial);
bool detect_pmc8(int fd);
void set_pmc8_reconnect_flag();
bool get_pmc8_reconnect_flag();

/**************************************************************************
 Get Info
**************************************************************************/
/** Get PMC8 current status info */
bool get_pmc8_status(int fd, PMC8Info *info);
/** Get All firmware information in addition to mount model */
bool get_pmc8_firmware(int fd, FirmwareInfo *info);
/** Get RA/DEC */
bool get_pmc8_coords(int fd, double &ra, double &dec);
bool get_pmc8_move_rate_axis(int fd, PMC8_AXIS axis, double &rate);
bool get_pmc8_track_rate(int fd, double &rate);
bool get_pmc8_tracking_data(int fd, double &rate, uint8_t &mode);
uint8_t get_pmc8_tracking_mode_from_rate(double rate);

/**************************************************************************
 Motion
**************************************************************************/
bool set_pmc8_move_rate_axis(int fd, PMC8_DIRECTION dir, int reqrate);
bool stop_pmc8_motion(int fd, PMC8_DIRECTION dir);
bool stop_pmc8_tracking_motion(int fd);
bool set_pmc8_ra_tracking(int fd, double rate);
bool set_pmc8_custom_ra_track_rate(int fd, double rate);
bool set_pmc8_custom_dec_track_rate(int fd, double rate);
bool set_pmc8_custom_ra_move_rate(int fd, double rate);
bool set_pmc8_custom_dec_move_rate(int fd, double rate);
bool set_pmc8_track_mode(int fd, uint8_t mode);
bool get_pmc8_is_scope_slewing(int fd, bool &isslew);
bool get_pmc8_direction_axis(int fd, PMC8_AXIS axis, int &dir);
bool set_pmc8_direction_axis(int fd, PMC8_AXIS axis, int dir, bool fast);
bool abort_pmc8(int fd);
bool abort_pmc8_goto(int fd);
bool slew_pmc8(int fd, double ra, double dec);
bool sync_pmc8(int fd, double ra, double dec);
bool set_pmc8_radec(int fd, double ra, double dec);
void set_pmc8_goto_resume(bool resume);
INDI::Telescope::TelescopePierSide destSideOfPier(double ra, double dec);


/**************************************************************************
 Park
**************************************************************************/
bool park_pmc8(int fd);
bool unpark_pmc8(int fd);

/**************************************************************************
 Guide
**************************************************************************/
bool set_pmc8_guide_rate(int fd, PMC8_AXIS axis, double rate);
bool get_pmc8_guide_rate(int fd, PMC8_AXIS axis, double &rate);
bool start_pmc8_guide(int fd, PMC8_DIRECTION gdir, int ms, long &timetaken_us, double ratehint);
bool stop_pmc8_guide(int fd, PMC8_DIRECTION gdir);

/**************************************************************************
 Time & Location
**************************************************************************/
void set_pmc8_location(double latitude, double longitude);
int get_pmc8_east_dir();



