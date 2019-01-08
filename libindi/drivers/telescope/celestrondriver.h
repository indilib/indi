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

#pragma once

#include <string>
#include "indicom.h"

/* Starsense specific constants */
#define ISNEXSTAR       0x11
#define ISSTARSENSE     0x13
#define MINSTSENSVER    float(1.18)
#define MAX_RESP_SIZE   20

// device IDs
#define CELESTRON_DEV_RA  0x10
#define CELESTRON_DEV_DEC 0x11
#define CELESTRON_DEV_GPS 0xb0

typedef enum { GPS_OFF, GPS_ON } CELESTRON_GPS_STATUS;
typedef enum { SR_1, SR_2, SR_3, SR_4, SR_5, SR_6, SR_7, SR_8, SR_9 } CELESTRON_SLEW_RATE;
typedef enum { TRACKING_OFF, TRACK_ALTAZ, TRACK_EQN, TRACK_EQS } CELESTRON_TRACK_MODE;
typedef enum { RA_AXIS, DEC_AXIS } CELESTRON_AXIS;
typedef enum { CELESTRON_N, CELESTRON_S, CELESTRON_W, CELESTRON_E } CELESTRON_DIRECTION;
typedef enum { FW_MODEL, FW_VERSION, FW_GPS, FW_RA, FW_DEC } CELESTRON_FIRMWARE;

typedef struct
{
    std::string Model;
    std::string Version;
    std::string GPSFirmware;
    std::string RAFirmware;
    std::string DEFirmware;
    float controllerVersion;
    char controllerVariant;
    bool isGem;
} FirmwareInfo;


typedef struct
{
    double ra;
    double dec;
    double az;
    double alt;
    CELESTRON_SLEW_RATE slewRate;
    CELESTRON_TRACK_MODE trackMode;
    CELESTRON_GPS_STATUS gpsStatus;
    bool isSlewing;
} SimData;


/**************************************************************************
 Utility functions
**************************************************************************/
namespace Celestron {
    double trimDecAngle(double angle);
    uint16_t dd2nex(double angle);
    uint32_t dd2pnex(double angle);
    double nex2dd(uint16_t value);
    double pnex2dd(uint32_t value);
}


class CelestronDriver
{
    public:
        CelestronDriver() {}

        // Misc.
        const char *getDeviceName();
        void set_port_fd(int port_fd) { fd = port_fd; }
        void set_simulation(bool enable) { simulation = enable; }
        void set_device(const char *name);

        // Simulation
        void set_sim_slew_rate(CELESTRON_SLEW_RATE val) { sim_data.slewRate = val; }
        void set_sim_track_mode(CELESTRON_TRACK_MODE val) { sim_data.trackMode = val; }
        void set_sim_gps_status(CELESTRON_GPS_STATUS val) { sim_data.gpsStatus = val; }
        void set_sim_slewing(bool isSlewing) { sim_data.isSlewing = isSlewing; }
        void set_sim_ra(double ra) { sim_data.ra = ra; }
        void set_sim_dec(double dec) { sim_data.dec = dec; }
        void set_sim_az(double az) { sim_data.az = az; }
        void set_sim_alt(double alt) { sim_data.alt = alt; }
        double get_sim_ra() { return sim_data.ra; }
        double get_sim_dec() { return sim_data.dec; }

        bool echo();
        bool check_connection();

        // Get info
        bool get_firmware(FirmwareInfo *info);
        bool get_version(char *version, int size);
        bool get_variant(char *variant);
        bool get_model(char *model, int size, bool *isGem);
        bool get_dev_firmware(int dev, char *version, int size);
        bool get_radec(double *ra, double *dec, bool precise);
        bool get_azalt(double *az, double *alt, bool precise);
        bool get_utc_date_time(double *utc_hours, int *yy, int *mm, int *dd, int *hh, int *minute, int *ss);

        // Motion
        bool start_motion(CELESTRON_DIRECTION dir, CELESTRON_SLEW_RATE rate);
        bool stop_motion(CELESTRON_DIRECTION dir);
        bool abort();
        bool slew_radec(double ra, double dec, bool precise);
        bool slew_azalt(double az, double alt, bool precise);
        bool sync(double ra, double dec, bool precise);

        // Time & Location
        bool set_location(double longitude, double latitude);
        bool set_datetime(struct ln_date *utc, double utc_offset);

        // Track Mode
        bool get_track_mode(CELESTRON_TRACK_MODE *mode);
        bool set_track_mode(CELESTRON_TRACK_MODE mode);

        bool is_slewing();

        // Hibernate/Wakup
        bool hibernate();
        bool wakeup();

        // Pulse Guide (experimental)
        int send_pulse(CELESTRON_DIRECTION direction, signed char rate, unsigned char duration_msec);
        int get_pulse_status(CELESTRON_DIRECTION direction, bool &pulse_state);

        // Pointing state, pier side, returns 'E' or 'W'
        bool get_pier_side(char * sop);

        // check if the mount is aligned using the mount J command
        bool check_aligned();

    protected:
        void set_sim_response(const char *fmt, ...);
        virtual int serial_write(const char *cmd, int nbytes, int *nbytes_written);
        virtual int serial_read(int nbytes, int *nbytes_read);
        virtual int serial_read_section(char stop_char, int *nbytes_read);

        int send_command(const char *cmd, int cmd_len, char *resp, int resp_len,
                bool ascii_cmd, bool ascii_resp);
        int send_passthrough(int dest, int cmd_id, const char *payload,
                int payload_len, char *response, int response_len);

        char response[MAX_RESP_SIZE];
        bool simulation = false;
        SimData sim_data;
        int fd = 0;
};
