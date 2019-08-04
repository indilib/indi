/*
    Avalon StarGo driver

    Copyright (C) 2019 Christopher Contaxis, Wolfgang Reissenberger,
    Ken Self and Tonino Tasselli

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

#ifndef AVALON_STARGO_H
#define AVALON_STARGO_H

#pragma once

#include <mounts/lx200telescope.h>
#include <indicom.h>
#include <indilogger.h>
#include <termios.h>

#include <cstring>
#include <string>
#include <unistd.h>

#define LX200_TIMEOUT 5 /* FD timeout in seconds */
#define RB_MAX_LEN    64
#define AVALON_TIMEOUT                                  2
#define AVALON_COMMAND_BUFFER_LENGTH                    32
#define AVALON_RESPONSE_BUFFER_LENGTH                   32

enum TDirection
{
    LX200_NORTH,
    LX200_WEST,
    LX200_EAST,
    LX200_SOUTH,
    LX200_ALL
};
enum TSlew
{
    LX200_SLEW_MAX,
    LX200_SLEW_FIND,
    LX200_SLEW_CENTER,
    LX200_SLEW_GUIDE
};
enum TFormat
{
    LX200_SHORT_FORMAT,
    LX200_LONG_FORMAT,
    LX200_LONGER_FORMAT
};

// StarGo specific tabs
extern const char *RA_DEC_TAB;

class LX200StarGo : public LX200Telescope
{
    public:
        enum TrackMode
        {
            TRACK_SIDEREAL = 0, //=Telescope::TelescopeTrackMode::TRACK_SIDEREAL,
            TRACK_SOLAR = 1, //=Telescope::TelescopeTrackMode::TRACK_SOLAR,
            TRACK_LUNAR = 2, //=Telescope::TelescopeTrackMode::TRACK_LUNAR,
            TRACK_NONE = 3
        };
        enum MotorsState
        {
            MOTORS_OFF = 0,
            MOTORS_DEC_ONLY = 1,
            MOTORS_RA_ONLY = 2,
            MOTORS_ON = 3
        };
        TrackMode CurrentTrackMode {TRACK_SIDEREAL};
        MotorsState CurrentMotorsState {MOTORS_OFF};
        TelescopeSlewRate CurrentSlewRate {SLEW_MAX};

        LX200StarGo();

        virtual const char *getDefaultName() override;
        virtual bool Handshake() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool updateProperties() override;
        virtual bool initProperties() override;
        virtual void ISGetProperties(const char *dev)override;

        // helper functions
        virtual bool receive(char* buffer, int* bytes, int wait = AVALON_TIMEOUT);
        virtual bool receive(char* buffer, int* bytes, char end, int wait = AVALON_TIMEOUT);
        virtual void flush();
        virtual bool transmit(const char* buffer);
        virtual bool SetTrackMode(uint8_t mode) override;

    protected:

        // Sync Home Position
        ISwitchVectorProperty SyncHomeSP;
        ISwitch SyncHomeS[1];

        // firmware info
        ITextVectorProperty MountFirmwareInfoTP;
        IText MountFirmwareInfoT[1] = {};

        // AUX focusers control
        ISwitchVectorProperty Aux1FocuserSP;
        ISwitch Aux1FocuserS[2];

        // goto home
        ISwitchVectorProperty MountGotoHomeSP;
        ISwitch MountGotoHomeS[1];

        // parking position
        ISwitchVectorProperty MountSetParkSP;
        ISwitch MountSetParkS[1];

        // guiding
        INumberVectorProperty GuidingSpeedNP;
        INumber GuidingSpeedP[2];

        ISwitchVectorProperty ST4StatusSP;
        ISwitch ST4StatusS[2];

        // Keypad
        ISwitchVectorProperty KeypadStatusSP;
        ISwitch KeypadStatusS[2];

        // Speed definitions
        ISwitch SystemSpeedSlewS[4];
        ISwitchVectorProperty SystemSpeedSlewSP;


        // meridian flip
        ISwitchVectorProperty MeridianFlipModeSP;
        ISwitch MeridianFlipModeS[3];

        ISwitchVectorProperty MeridianFlipEnabledSP;
        ISwitch MeridianFlipEnabledS[2];
        ISwitchVectorProperty MeridianFlipForcedSP;
        ISwitch MeridianFlipForcedS[2];

        int controller_format { LX200_LONG_FORMAT };

        // override LX200Generic
        virtual void getBasicData() override;
        virtual bool ReadScopeStatus() override;
        virtual bool Park() override;
        virtual void SetParked(bool isparked);
        virtual bool UnPark() override;
        virtual bool saveConfigItems(FILE *fp) override;
        virtual bool Goto(double ra, double dec) override;
        virtual bool Connect() override;
        virtual bool Disconnect() override;

        // StarGo stuff
        virtual bool syncHomePosition();
        bool slewToHome(ISState *states, char *names[], int n);
        bool setParkPosition(ISState *states, char *names[], int n);
        bool getKeypadStatus (bool *isEnabled);
        bool setKeyPadEnabled(bool enabled);
        bool getSystemSlewSpeedMode (int *index);
        bool setSystemSlewSpeedMode(int index);

        // autoguiding
        virtual bool setGuidingSpeeds(int raSpeed, int decSpeed);

        // scope status
        virtual bool ParseMotionState(char* state);

        // location
        virtual bool sendScopeLocation();
        double LocalSiderealTime(double longitude);
        bool setLocalSiderealTime(double longitude);
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool getSiteLatitude(double *siteLat);
        virtual bool getSiteLongitude(double *siteLong);
        virtual bool getLST_String(char* input);
        bool getTrackFrequency(double *value);
        virtual bool getEqCoordinates(double *ra, double *dec);


        // queries to the scope interface. Wait for specified end character
        virtual bool sendQuery(const char* cmd, char* response, char end, int wait = AVALON_TIMEOUT);
        // Wait for default "#' character
        virtual bool sendQuery(const char* cmd, char* response, int wait = AVALON_TIMEOUT);
        virtual bool getFirmwareInfo(char *version);
        virtual bool setSiteLatitude(double Lat);
        virtual bool setSiteLongitude(double Long);
        virtual bool getScopeAlignmentStatus(char *mountType, bool *isTracking, int *alignmentPoints);
        virtual bool getMotorStatus(int *xSpeed, int *ySpeed);
        virtual bool getParkHomeStatus (char* status);
        virtual bool setMountGotoHome();
        virtual bool setMountParkPosition();

        // guiding
        virtual bool getST4Status(bool *isEnabled);
        virtual bool getGuidingSpeeds(int *raSpeed, int *decSpeed);
        virtual bool setST4Enabled(bool enabled);

        // meridian flip

        virtual bool syncSideOfPier();
        bool checkLX200Format();
        // Guide Commands
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;
        virtual bool SetSlewRate(int index) override;
        virtual bool SetMeridianFlipMode(int index);
        virtual bool GetMeridianFlipMode(int *index);
        virtual int SendPulseCmd(int8_t direction, uint32_t duration_msec) override;
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool SetTrackRate(double raRate, double deRate) override;
        // NSWE Motion Commands
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        virtual bool Sync(double ra, double dec) override;
        bool setObjectCoords(double ra, double dec);
        virtual bool setLocalDate(uint8_t days, uint8_t months, uint16_t years) override;
        virtual bool setLocalTime24(uint8_t hour, uint8_t minute, uint8_t second) override;
        virtual bool setUTCOffset(double offset) override;
        virtual bool getLocalTime(char *timeString) override;
        virtual bool getLocalDate(char *dateString) override;
        virtual bool getUTFOffset(double *offset) override;

        // Abort ALL motion
        virtual bool Abort() override;
        int MoveTo(int direction);

        bool setSlewMode(int slewMode);

};
inline bool LX200StarGo::sendQuery(const char* cmd, char* response, int wait)
{
    return sendQuery(cmd, response, '#', wait);
}
inline bool LX200StarGo::receive(char* buffer, int* bytes, int wait)
{
    return receive(buffer, bytes, '#', wait);
}

#endif // AVALON_STARGO_H
