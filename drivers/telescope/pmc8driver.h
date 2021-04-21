/*
    INDI Explore Scientific PMC8 driver

    Copyright (C) 2017 Michael Fulbright
    Additional contributors: 
        Thomas Olson, Copyright (C) 2019
        Karl Rees, Copyright (C) 2019-2021
        
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

typedef enum {
    ST_STOPPED,
    ST_TRACKING,
    ST_SLEWING,
    ST_GUIDING,
    ST_MERIDIAN_FLIPPING,
    ST_PARKED,
    ST_HOME
} PMC8_SYSTEM_STATUS;


//#endif

typedef enum { PMC8_TRACK_SIDEREAL, PMC8_TRACK_LUNAR, PMC8_TRACK_SOLAR, PMC8_TRACK_CUSTOM } PMC8_TRACK_RATE;

typedef enum { PMC8_MOVE_4X, PMC8_MOVE_16X, PMC8_MOVE_64X, PMC8_MOVE_256X } PMC8_MOVE_RATE;

//typedef enum { HEMI_SOUTH, HEMI_NORTH } PMC8_HEMISPHERE;

//typedef enum { FW_MODEL, FW_BOARD, FW_CONTROLLER, FW_RA, FW_DEC } IEQ_FIRMWARE;

typedef enum { PMC8_AXIS_RA=0, PMC8_AXIS_DEC=1 } PMC8_AXIS;
typedef enum { PMC8_N, PMC8_S, PMC8_W, PMC8_E } PMC8_DIRECTION;

typedef enum { MOUNT_G11 = 0, MOUNT_EXOS2 = 1, MOUNT_iEXOS100 = 2 } PMC8_MOUNT_TYPES;

typedef struct
{
    PMC8_SYSTEM_STATUS systemStatus;
    PMC8_SYSTEM_STATUS rememberSystemStatus;
//    PMC8_HEMISPHERE hemisphere;
} PMC8Info;

typedef struct
{
    std::string Model;
    std::string MainBoardFirmware;
    PMC8_MOUNT_TYPES MountType;
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
void set_pmc8_sim_move_rate(PMC8_MOVE_RATE value);
//void set_sim_hemisphere(IEQ_HEMISPHERE value);
void set_pmc8_sim_ra(double ra);
void set_pmc8_sim_dec(double dec);
//void set_sim_guide_rate(double rate);

/**************************************************************************
 Diagnostics
**************************************************************************/
bool check_pmc8_connection(int fd, bool isSerial);
bool detect_pmc8(int fd);
void set_pmc8_reconnect_flag();
bool get_pmc8_reconnect_flag();

/**************************************************************************
 Get Info
**************************************************************************/
/** Get PMC8 current status info */
bool get_pmc8_status(int fd, PMC8Info *info);
/** Get All firmware informatin in addition to mount model */
bool get_pmc8_firmware(int fd, FirmwareInfo *info);
/** Get RA/DEC */
bool get_pmc8_coords(int fd, double &ra, double &dec);
bool get_pmc8_tracking_rate_axis(int fd, PMC8_AXIS axis, int &rate);

/**************************************************************************
 Motion
**************************************************************************/
bool start_pmc8_motion(int fd, PMC8_DIRECTION dir, int speedindex);
bool stop_pmc8_motion(int fd, PMC8_DIRECTION dir);
bool stop_pmc8_tracking_motion(int fd);
bool set_pmc8_custom_ra_track_rate(int fd, double rate);
bool set_pmc8_custom_dec_track_rate(int fd, double rate);
bool set_pmc8_custom_ra_move_rate(int fd, double rate);
bool set_pmc8_custom_dec_move_rate(int fd, double rate);
bool set_pmc8_track_mode(int fd, uint32_t rate);
//bool set_pmc8_track_enabled(int fd, bool enabled);
bool get_pmc8_is_scope_slewing(int fd, bool &isslew);
bool get_pmc8_direction_axis(int fd, PMC8_AXIS axis, int &dir);
bool set_pmc8_direction_axis(int fd, PMC8_AXIS axis, int dir, bool fast);
bool abort_pmc8(int fd);
bool slew_pmc8(int fd, double ra, double dec);
bool sync_pmc8(int fd, double ra, double dec);
bool set_pmc8_radec(int fd, double ra, double dec);
INDI::Telescope::TelescopePierSide destSideOfPier(double ra, double dec);
void set_pmc8_istracking(bool enabled);
bool get_pmc8_istracking();


/**************************************************************************
 Home
**************************************************************************/
//bool find_ieqpro_home(int fd);
//bool goto_pmc8_home(int fd);
//bool set_ieqpro_current_home(int fd);

/**************************************************************************
 Park
**************************************************************************/
bool park_pmc8(int fd);
bool unpark_pmc8(int fd);

/**************************************************************************
 Guide
**************************************************************************/
bool set_pmc8_guide_rate(int fd, double rate);
//bool get_pmc8_guide_rate(int fd, double *rate);
bool start_pmc8_guide(int fd, PMC8_DIRECTION gdir, int ms, long &timetaken_us);
bool stop_pmc8_guide(int fd, PMC8_DIRECTION gdir);

/**************************************************************************
 Time & Location
**************************************************************************/
void set_pmc8_location(double latitude, double longitude);

//bool set_ieqpro_longitude(int fd, double longitude);
//bool set_ieqpro_latitude(int fd, double latitude);
//bool get_ieqpro_longitude(int fd, double *longitude);
//bool get_ieqpro_latitude(int fd, double *latitude);
//bool set_ieqpro_local_date(int fd, int yy, int mm, int dd);
//bool set_ieqpro_local_time(int fd, int hh, int mm, int ss);
//bool set_ieqpro_utc_offset(int fd, double offset_hours);
//bool set_ieqpro_daylight_saving(int fd, bool enabled);



