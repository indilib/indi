/*
    INDI IOptron v3 Driver for firmware version 20171001 or later.

    Copyright (C) 2018 Jasem Mutlaq

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
#include <map>
#include "indilogger.h"

/**
 * @namespace IOPv3
 * @brief Encapsulates classes and structures required for iOptron Command Set v3 implementation
 *
 */
namespace IOPv3
{

typedef enum { GPS_OFF, GPS_ON, GPS_DATA_OK } IOP_GPS_STATUS;
typedef enum
{
    ST_STOPPED,
    ST_TRACKING_PEC_OFF,
    ST_SLEWING,
    ST_GUIDING,
    ST_MERIDIAN_FLIPPING,
    ST_TRACKING_PEC_ON,
    ST_PARKED,
    ST_HOME
} IOP_SYSTEM_STATUS;
typedef enum { TR_SIDEREAL, TR_LUNAR, TR_SOLAR, TR_KING, TR_CUSTOM } IOP_TRACK_RATE;
typedef enum { SR_1 = 1, SR_2, SR_3, SR_4, SR_5, SR_6, SR_7, SR_8, SR_MAX } IOP_SLEW_RATE;
typedef enum { TS_RS232, TS_CONTROLLER, TS_GPS } IOP_TIME_SOURCE;
typedef enum { HEMI_SOUTH, HEMI_NORTH } IOP_HEMISPHERE;
typedef enum { FW_MODEL, FW_BOARD, FW_CONTROLLER, FW_RA, FW_DEC } IOP_FIRMWARE;
typedef enum { RA_AXIS, DEC_AXIS } IOP_AXIS;
typedef enum { IOP_N, IOP_S, IOP_W, IOP_E } IOP_DIRECTION;
typedef enum { IOP_FIND_HOME, IOP_SET_HOME, IOP_GOTO_HOME } IOP_HOME_OPERATION;
typedef enum { IOP_PIER_EAST, IOP_PIER_WEST, IOP_PIER_UNKNOWN } IOP_PIER_STATE;
typedef enum { IOP_CW_UP, IOP_CW_NORMAL} IOP_CW_STATE;
typedef enum { IOP_MB_STOP, IOP_MB_FLIP} IOP_MB_STATE;

typedef struct
{
    IOP_GPS_STATUS gpsStatus;
    IOP_SYSTEM_STATUS systemStatus;
    IOP_SYSTEM_STATUS rememberSystemStatus;
    IOP_TRACK_RATE trackRate;
    IOP_SLEW_RATE slewRate;
    IOP_TIME_SOURCE timeSource;
    IOP_HEMISPHERE hemisphere;
    double longitude;
    double latitude;
} IOPInfo;

typedef struct
{
    std::string Model;
    std::string MainBoardFirmware;
    std::string ControllerFirmware;
    std::string RAFirmware;
    std::string DEFirmware;
} FirmwareInfo;

class Driver
{

    public:

        explicit Driver(const char *deviceName);
        ~Driver() = default;

        static const std::map<std::string, std::string> models;
        // Slew speeds. N.B. 1024 is arbitrary as the real max value different from
        // one mount to another. It is used for simulation purposes only.
        static const uint16_t IOP_SLEW_RATES[];

        /**************************************************************************
         Communication
        **************************************************************************/
        bool sendCommand(const char *command, int count = 1, char *response = nullptr, uint8_t timeout = IOP_TIMEOUT,
                         uint8_t debugLog = INDI::Logger::DBG_DEBUG);
        bool sendCommandOk(const char *command);
        bool checkConnection(int fd);

        /**************************************************************************
         Get Info
        **************************************************************************/
        /** Get iEQ current status info */
        bool getStatus(IOPInfo *info);
        /** Get All firmware informatin in addition to mount model */
        bool getFirmwareInfo(FirmwareInfo *info);
        /** Get RA/DEC */
        bool getCoords(double *ra, double *de, IOP_PIER_STATE *pierState, IOP_CW_STATE *cwState);
        /** Get UTC JD plus utc offset and whether daylight savings is active or not */
        bool getUTCDateTime(double *JD, int *utcOffsetMinutes, bool *dayLightSaving);

        /**************************************************************************
         Motion
        **************************************************************************/
        bool startMotion(IOP_DIRECTION dir);
        bool stopMotion(IOP_DIRECTION dir);
        bool setSlewRate(IOP_SLEW_RATE rate);
        bool setCustomRATrackRate(double rate);
        bool setTrackMode(IOP_TRACK_RATE rate);
        bool setTrackEnabled(bool enabled);
        /* v3.0 Add in PEC Control */
        bool setPECEnabled(bool enabled); // start / stop PEC
        bool setPETEnabled(bool enabled); // record / cancel PEC
        bool getPETEnabled(bool enabled); // check data / recording status
        // End Mod */
        bool abort();
        bool slewNormal();
        bool slewCWUp();
        bool sync();
        bool setRA(double ra);
        bool setDE(double de);

        /**************************************************************************
         Home
        **************************************************************************/
        bool findHome();
        bool gotoHome();
        bool setCurrentHome();

        /**************************************************************************
         Park
        **************************************************************************/
        bool park();
        bool unpark();
        bool setParkAz(double az);
        bool setParkAlt(double alt);

        /**************************************************************************
         Guide
        **************************************************************************/
        bool setGuideRate(double RARate, double DERate);
        bool getGuideRate(double *RARate, double *DERate);
        bool startGuide(IOP_DIRECTION dir, uint32_t ms);

        /**************************************************************************
         Meridian Behavior
        **************************************************************************/
        bool setMeridianBehavior(IOP_MB_STATE action, uint8_t degrees);
        bool getMeridianBehavior(IOP_MB_STATE &action, uint8_t &degrees);

        /**************************************************************************
         Time & Location
        **************************************************************************/
        bool setLongitude(double longitude);
        bool setLatitude(double latitude);
        bool setUTCDateTime(double JD);
        bool setUTCOffset(int offsetMinutes);
        bool setDaylightSaving(bool enabled);

        /**************************************************************************
         Misc.
        **************************************************************************/
        void setDebug(bool enable);
        void setSimulation(bool enable);

        /**************************************************************************
         Simulation
        **************************************************************************/
        void setSimGPSstatus(IOP_GPS_STATUS value);
        void setSimSytemStatus(IOP_SYSTEM_STATUS value);
        void setSimTrackRate(IOP_TRACK_RATE value);
        void setSimSlewRate(IOP_SLEW_RATE value);
        void setSimTimeSource(IOP_TIME_SOURCE value);
        void setSimHemisphere(IOP_HEMISPHERE value);
        void setSimRA(double ra);
        void setSimDE(double de);
        void setSimLongLat(double longitude, double latitude);
        void setSimGuideRate(double raRate, double deRate);

    protected:

        /**************************************************************************
         Firmware Info
        **************************************************************************/
        /** Get mainboard and controller firmware only */
        bool getMainFirmware(std::string &mainFirmware, std::string &controllerFirmware);
        /** Get RA and DEC firmware info */
        bool getRADEFirmware(std::string &RAFirmware, std::string &DEFirmware);
        /** Get Mount model */
        bool getModel(std::string &model);

        struct
        {
            double ra;
            double de;
            double ra_guide_rate;
            double de_guide_rate;
            double JD;
            int utc_offset_minutes;
            bool day_light_saving;
            uint8_t mb_limit;
            IOP_PIER_STATE pier_state;
            IOP_CW_STATE cw_state;
            IOP_MB_STATE mb_state;

            IOPInfo simInfo;
        } simData;

    private:
        int PortFD = { -1 };
        bool m_Debug = {false};
        bool m_Simulation = {false};
        const char *m_DeviceName;

        // FD timeout in seconds
        static const uint8_t IOP_TIMEOUT = 5;
        // Buffer to store mount response
        static const uint8_t IOP_BUFFER = 64;
};

}
