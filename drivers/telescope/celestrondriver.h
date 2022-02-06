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

#include <stdint.h>

//#include <thread>
//#include <condition_variable>
//#include <atomic>

/* Starsense specific constants */
#define ISNEXSTAR       0x11
#define ISSTARSENSE     0x13
#define MINSTSENSVER    1.18
#define MAX_RESP_SIZE   20

// device IDs
#define CELESTRON_DEV_RA  0x10
#define CELESTRON_DEV_DEC 0x11
#define CELESTRON_DEV_GPS 0xb0
// focuser device
#define CELESTRON_DEV_FOC 0x12

// motor commands
#define MC_GET_POSITION         0x01    // return 24 bit position
#define MC_GOTO_FAST            0x02    // send 24 bit target
#define MC_SET_POS_GUIDERATE    0x06    // use the 2 byte CelestronTrackRates to set the rate
#define MC_SET_NEG_GUIDERATE    0x07    // for Southern hemisphere, track mode EQ_S
#define MC_LEVEL_START          0x0b    // move to switch position
#define MC_PEC_RECORD_START     0x0C    // n/a          Ack     Start recording PEC position
#define MC_PEC_PLAYBACK         0x0D    // 8 bits       Ack     Start(01)/stop(00) PEC playback

#define MTR_PECBIN              0x0E    // current PEC index - 1 byte 0 - 255(88)

#define MC_LEVEL_DONE           0x12    // return 0xFF when move finished
#define MC_SLEW_DONE            0x13    // return 0xFF when move finished
#define MC_PEC_RECORD_DONE      0x15    // n/a          8 bits  != 0 is PEC record completed
#define MC_PEC_RECORD_STOP      0x16    // n/a          n/a     Stop PEC recording

#define MC_GOTO_SLOW            0x17    //  16/24 bits  Ack     Goto position with slow, variable rate. Either 16 or 24 bit accuracy.
//                      Position is a signed fraction of a full rotation
#define MC_AT_INDEX             0x18    // n/a          8 bits  FFH at index, 00H not
#define MC_SEEK_INDEX           0x19    // n/a          n/a     Seek PEC Index
#define MC_MOVE_POS             0x24    // start move positive direction, rate 0-9, 0 is stop
#define MC_MOVE_NEG             0x25    // start move negative direction, rate 0-9, 0 is stop

#define MTR_AUX_GUIDE           0x26    // aux guide command, rate -100 to 100, duration centiseconds
#define MTR_IS_AUX_GUIDE_ACTIVE 0x27    // return 0x00 when aux guide is not in progress

// command 0x30 and 0x31 are read/ write memory commands
#define MC_PEC_READ_DATA        0x30    // 8            PEC data value  return 1 byte of data:
// 0x3f         number of PEC bins (88)
// 0x40+i       PEC data for bin i
#define MC_PEC_WRITE_DATA       0x31    // 16  PEC data address, PEC data value
// 0x40+i, value bin i

#define MC_SET_AUTOGUIDE_RATE   0x46    // 0 to 99 as % sidereal
#define MC_GET_AUTOGUIDE_RATE   0x47    // 0 to 99 as % sidereal

// focuser passthrough commands
#define FOC_CALIB_ENABLE        42      // send 0 to start or 1 to stop
#define FOC_CALIB_DONE          43      // returns 2 bytes [0] done, [1] state 0-12
#define FOC_GET_HS_POSITIONS    44      // returns 2 ints, low and high limits

// generic device commands
#define GET_VER                 0xfe    // return 2 or 4 bytes major.minor.build

typedef enum { GPS_OFF, GPS_ON } CELESTRON_GPS_STATUS;
typedef enum { SR_1, SR_2, SR_3, SR_4, SR_5, SR_6, SR_7, SR_8, SR_9 } CELESTRON_SLEW_RATE;
typedef enum { CTM_OFF, CTM_ALTAZ, CTM_EQN, CTM_EQS, CTM_RADEC } CELESTRON_TRACK_MODE;
typedef enum { RA_AXIS, DEC_AXIS } CELESTRON_AXIS;
typedef enum { CELESTRON_N, CELESTRON_S, CELESTRON_W, CELESTRON_E } CELESTRON_DIRECTION;
typedef enum { FW_MODEL, FW_VERSION, FW_RA, FW_DEC, FW_ISGEM, FW_CAN_AUX, FW_HAS_FOC } CELESTRON_FIRMWARE;


// PEC state machine
// the order matters because it's used to check what states are available.
// they do not match the the base TelescopePECState
typedef enum
{
    NotKnown,       // PEC has not been checked.

    /// <summary>
    /// PEC is not available, hardware has been checked, no other state is possible
    /// </summary>
    PEC_NOT_AVAILABLE,

    /// <summary>
    /// PEC is available but inactive, can seek index
    /// Seek index is only available command
    /// </summary>
    PEC_AVAILABLE,

    /// <summary>
    /// The PEC index is being searched for, goes to PEC_INDEXED when found
    /// </summary>
    PEC_SEEKING,

    /// <summary>
    /// the PEC index has been found, can go to Playback or Recording
    /// this is equivalent to TelescopePECState PEC_OFF
    /// </summary>
    PEC_INDEXED,

    /// <summary>
    /// PEC is being played back, stays in this state until stopped
    /// equivalent to TelescopePECState PEC_ON
    /// </summary>
    PEC_PLAYBACK,

    /// <summary>
    /// PEC is being recorded, goes to PEC_INDEXED when completed
    /// </summary>
    PEC_RECORDING

} PEC_STATE;

// These values are sent to the hour angle axis motor using the MC_SET_POS|NEG_GUIDERATE
// commands to set the tracking rate.
typedef enum
{
    CTR_SIDEREAL = 0xFFFF,
    CTR_SOLAR    = 0xFFFE,
    CTR_LUNAR    = 0xFFFD
} CELESTRON_TRACK_RATE;

typedef struct
{
    std::string Model;
    std::string Version;
    //std::string GPSFirmware;
    std::string RAFirmware;
    std::string DEFirmware;
    double controllerVersion;
    char controllerVariant;
    bool isGem;
    bool canPec;
    bool hasHomeIndex;
    bool hasFocuser;
    CELESTRON_TRACK_MODE celestronTrackMode;
} FirmwareInfo;

typedef struct SimData
{
    double ra;
    double dec;
    double az;
    double alt;
    CELESTRON_SLEW_RATE slewRate;
    CELESTRON_TRACK_MODE trackMode;
    CELESTRON_GPS_STATUS gpsStatus;
    bool isSlewing;
    uint32_t foc_position = 20000;
    uint32_t foc_target = 20000;
} SimData;


/**************************************************************************
 Utility functions
**************************************************************************/
namespace Celestron
{
double trimDecAngle(double angle);
uint16_t dd2nex(double angle);
uint32_t dd2pnex(double angle);
double nex2dd(uint32_t value);
double pnex2dd(uint32_t value);
}

class CelestronDriver
{
    public:
        CelestronDriver() {}
        virtual ~CelestronDriver() = default;

        // Misc.
        const char *getDeviceName();
        void set_port_fd(int port_fd)
        {
            fd = port_fd;
        }
        void set_simulation(bool enable)
        {
            simulation = enable;
        }
        void set_device(const char *name);

        // Simulation
        void set_sim_slew_rate(CELESTRON_SLEW_RATE val)
        {
            sim_data.slewRate = val;
        }
        void set_sim_track_mode(CELESTRON_TRACK_MODE val)
        {
            sim_data.trackMode = val;
        }
        void set_sim_gps_status(CELESTRON_GPS_STATUS val)
        {
            sim_data.gpsStatus = val;
        }
        void set_sim_slewing(bool isSlewing)
        {
            sim_data.isSlewing = isSlewing;
        }
        void set_sim_ra(double ra)
        {
            sim_data.ra = ra;
        }
        void set_sim_dec(double dec)
        {
            sim_data.dec = dec;
        }
        void set_sim_az(double az)
        {
            sim_data.az = az;
        }
        void set_sim_alt(double alt)
        {
            sim_data.alt = alt;
        }
        double get_sim_ra()
        {
            return sim_data.ra;
        }
        double get_sim_dec()
        {
            return sim_data.dec;
        }
        int get_sim_foc_offset()
        {
            return sim_data.foc_target - sim_data.foc_position;
        }
        void move_sim_foc(int offset)
        {
            sim_data.foc_position += offset;
        }

        bool echo();
        bool check_connection();

        // Get info
        bool get_firmware(FirmwareInfo *info);
        bool get_version(char *version, size_t size);
        bool get_variant(char *variant);
        int model();        // returns model number, -1 if failed
        bool get_model(char *model, size_t size, bool *isGem, bool *canPec, bool *hasHomeIndex);
        bool get_dev_firmware(int dev, char *version, size_t size);
        bool get_radec(double *ra, double *dec, bool precise);
        bool get_azalt(double *az, double *alt, bool precise);
        bool get_utc_date_time(double *utc_hours, int *yy, int *mm, int *dd, int *hh, int *minute, int *ss, bool *dst,
                               bool precise);

        // Motion
        bool start_motion(CELESTRON_DIRECTION dir, CELESTRON_SLEW_RATE rate);
        bool stop_motion(CELESTRON_DIRECTION dir);
        bool abort();
        bool slew_radec(double ra, double dec, bool precise);
        bool slew_azalt(double az, double alt, bool precise);
        bool sync(double ra, double dec, bool precise);
        bool unsync();

        // Time & Location
        bool set_location(double longitude, double latitude);
        bool get_location(double* longitude, double *latitude);
        bool set_datetime(struct ln_date *utc, double utc_offset, bool dst = false, bool precise = false);

        // Track Mode, this is not the Indi track mode
        bool get_track_mode(CELESTRON_TRACK_MODE *mode);
        bool set_track_mode(CELESTRON_TRACK_MODE mode);

        bool is_slewing(bool *slewing);

        // Hibernate/Wakeup/ align
        bool hibernate();
        bool wakeup();
        bool lastalign();
        bool startmovetoindex();
        bool indexreached(bool *atIndex);

        // Pulse Guide
        size_t send_pulse(CELESTRON_DIRECTION direction, unsigned char rate, unsigned char duration_msec);
        bool get_pulse_status(CELESTRON_DIRECTION direction);

        // get and set guide rate
        // 0 to 255 corresponding to 0 to 100% sidereal
        bool get_guide_rate(CELESTRON_AXIS axis, uint8_t  * rate);
        bool set_guide_rate(CELESTRON_AXIS axis, uint8_t  rate);

        // Pointing state, pier side, returns 'E' or 'W'
        bool get_pier_side(char * sop);

        // check if the mount is aligned using the mount J command
        bool check_aligned(bool *isAligned);

        // set the tracking rate, sidereal, solar or lunar
        bool set_track_rate(CELESTRON_TRACK_RATE rate, CELESTRON_TRACK_MODE mode);

        // focuser commands
        bool foc_exists();      // read version
        int foc_position();     // read position, return -1 if failed
        bool foc_move(uint32_t steps);   // start move
        bool foc_moving();      // return true if moving
        bool foc_limits(int * low, int * high);     // read limits
        bool foc_abort();       // stop move

        // PEC management

        PEC_STATE pecState { PEC_STATE::NotKnown };

        PEC_STATE updatePecState();

        bool PecSeekIndex();
        bool isPecAtIndex(bool force = false);            // returns true if the PEC index has been found

        size_t pecIndex();              // reads the current PEC index
        int getPecValue(size_t index);     // reads the current PEC value
        bool setPecValue(size_t index, int data);

        bool PecPlayback(bool start);

        bool PecRecord(bool start);
        bool isPecRecordDone();

        size_t getPecNumBins();

        const char *PecStateStr(PEC_STATE);
        const char *PecStateStr();

        // PEC simulation properties
        int simIndex;
        int simRecordStart;
        bool simSeekIndex = false;

    protected:
        void set_sim_response(const char *fmt, ...);
        virtual int serial_write(const char *cmd, int nbytes, int *nbytes_written);
        virtual int serial_read(int nbytes, int *nbytes_read);
        virtual int serial_read_section(char stop_char, int *nbytes_read);

        size_t send_command(const char *cmd, size_t cmd_len, char *resp, size_t resp_len,
                            bool ascii_cmd, bool ascii_resp);
        size_t send_passthrough(int dest, int cmd_id, const char *payload,
                                size_t payload_len, char *resp, size_t response_len);

        char response[MAX_RESP_SIZE];
        bool simulation = false;
        SimData sim_data;
        int fd = 0;

        char sim_ra_guide_rate = 50;
        char sim_dec_guide_rate = 50;
};

class PecData
{
    public:
        PecData();

        // save PEC data to a file
        bool Save(const char * filename);

        // saves PEC data to mount
        bool Save(CelestronDriver * driver);

        // Loads PEC data from mount
        bool Load(CelestronDriver * driver);

        // Loads PEC data from file
        bool Load(const char * fileName);

        size_t NumBins()
        {
            return numBins;
        }

        void RemoveDrift();

        const char *getDeviceName();
        //    void set_device(const char *name);

    private:
        double wormArcSeconds = 7200;
        double rateScale = 1024;
        size_t numBins = 88;
        const double SIDEREAL_ARCSEC_PER_SEC = 360.0 * 60.0 * 60.0 / (23.0 * 3600.0 + 56 * 60 + 4.09);

        double data[255];   // numbins + 1 values, accumulated PEC offset in arc secs. First one zero

        void Kalman(PecData newData, int num);
};


