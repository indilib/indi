/*
    10micron INDI driver

    Copyright (C) 2017 Hans Lambermont

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

#include "lx200generic.h"

#include <chrono>

class LX200_10MICRON : public LX200Generic
{
    public:

        enum LX200_10MICRON_UNATTENDED_FLIP_SETTINGS
        {
            UNATTENDED_FLIP_DISABLED,
            UNATTENDED_FLIP_ENABLED,
            UNATTENDED_FLIP_COUNT
        };

        enum LX200_10MICRON_PRODUCT_INFO
        {
            PRODUCT_NAME,
            PRODUCT_CONTROL_BOX,
            PRODUCT_FIRMWARE_VERSION,
            PRODUCT_FIRMWARE_DATE,
            PRODUCT_COUNT
        };

        enum LX200_10MICRON_10MICRON_GSTAT
        {
            GSTAT_UNSET                       = -999,
            GSTAT_TRACKING                    = 0,
            GSTAT_STOPPED                     = 1,
            GSTAT_PARKING                     = 2,
            GSTAT_UNPARKING                   = 3,
            GSTAT_SLEWING_TO_HOME             = 4,
            GSTAT_PARKED                      = 5,
            GSTAT_SLEWING_OR_STOPPING         = 6,
            GSTAT_NOT_TRACKING_AND_NOT_MOVING = 7,
            GSTAT_MOTORS_TOO_COLD             = 8,
            GSTAT_TRACKING_OUTSIDE_LIMITS     = 9,
            GSTAT_FOLLOWING_SATELLITE         = 10,
            GSTAT_NEED_USEROK                 = 11,
            GSTAT_UNKNOWN_STATUS              = 98,
            GSTAT_ERROR                       = 99
        };

        enum LX200_10MICRON_ALIGNMENT_POINT
        {
            ALP_MRA,     // Mount Right Ascension
            ALP_MDEC,    // Mount Declination
            ALP_MSIDE,     // Mount Pier Side
            ALP_SIDTIME, // Sidereal Time
            ALP_PRA,     // Plate solved Right Ascension
            ALP_PDEC,    // Plate solved Declination
            ALP_COUNT
        };

        enum LX200_10MICRON_MINI_ALIGNMENT_POINT_RO
        {
            MALPRO_MRA,     // Mount Right Ascension
            MALPRO_MDEC,    // Mount Declination
            MALPRO_MSIDE,    // Mount Pier Side
            MALPRO_SIDTIME, // Sidereal Time
            MALPRO_COUNT
        };

        enum LX200_10MICRON_MINI_ALIGNMENT_POINT
        {
            MALP_PRA,  // Plate solved Right Ascension
            MALP_PDEC, // Plate solved Declination
            MALP_COUNT
        };

        enum LX200_10MICRON_ALIGNMENT_STATE
        {
            ALIGN_IDLE,
            ALIGN_START,
            ALIGN_END,
            ALIGN_DELETE_CURRENT,
            ALIGN_COUNT
        };

        enum LX200_10MICRON_MERIDIAN_LIMITS
        {
            MERIDIAN_LIMIT_SLEW,
            MERIDIAN_LIMIT_TRACK,
            MERIDIAN_LIMIT_COUNT
        };

        // Same order and values as :GMF# and :SMFn# minus 1
        enum LX200_10MICRON_ALLOWED_MERIDIAN_SIDE
        {
            MERIDIAN_SIDE_BOTH,
            MERIDIAN_SIDE_WEST_ONLY,
            MERIDIAN_SIDE_EAST_ONLY,
            MERIDIAN_SIDE_COUNT
        };

        // Why a mount setting is read, which decides what is logged and sent to clients:
        // at connect, by the periodic refresh (changes made elsewhere), or right after writing it.
        enum LX200_10MICRON_READ_CONTEXT
        {
            READ_AT_CONNECT,
            READ_PERIODIC,
            READ_AFTER_WRITE
        };

        // Same element order as TELESCOPE_PIER_SIDE, plus NONE
        enum LX200_10MICRON_TARGET_PIER_SIDE
        {
            TARGET_PIER_WEST,
            TARGET_PIER_EAST,
            TARGET_PIER_NONE,
            TARGET_PIER_COUNT
        };

        LX200_10MICRON();
        ~LX200_10MICRON() {}

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

        const char *getDefaultName() override;
        bool Handshake() override;
        bool initProperties() override;
        void ISGetProperties(const char *dev) override;
        bool updateProperties() override;
        bool saveConfigItems(FILE *fp) override;
        bool ReadScopeStatus() override;
        bool Park() override;
        bool UnPark() override;
        bool SetTrackEnabled(bool enabled) override;
        bool Flip(double ra, double dec) override;
        bool getUnattendedFlipSetting();
        bool setUnattendedFlipSetting(bool setting);
        void updateUnattendedFlipSetting(LX200_10MICRON_READ_CONTEXT context);
        bool confirmUnattendedFlipSetting(int wanted);
        bool getMeridianLimits(LX200_10MICRON_READ_CONTEXT context);
        bool setMeridianLimits(int slewLimit, int trackLimit);
        bool getTimeToTrackingEnd();
        bool getAllowedMeridianSide(LX200_10MICRON_READ_CONTEXT context);
        bool setAllowedMeridianSide(int side);
        bool getTargetPierSide();
        bool SyncConfigBehaviour(bool cmcfg);
        bool setLocalDate(uint8_t days, uint8_t months, uint16_t years) override;
        bool SetTLEtoFollow(const char *tle);
        bool SetTLEfromDatabase(int tleN);
        bool TrackSat();
        bool CalculateSatTrajectory(std::string start_pass_isodatetime, std::string end_pass_isodatetime);

        int AddSyncPoint(double MRa, double MDec, double MSide, double PRa, double PDec, double SidTime);
        int AddSyncPointHere(double PRa, double PDec);

        // TODO move these things elsewhere
        int monthToNumber(const char *monthName);
        int setStandardProcedureWithoutRead(int fd, const char *data);
        int setStandardProcedureAndExpectChar(int fd, const char *data, const char *expect);
        int setStandardProcedureAndReturnResponse(int fd, const char *data, char *response, int max_response_length);

    protected:
        void getBasicData() override;

        int UnattendedFlip = -1;
        ISwitch UnattendedFlipS[UNATTENDED_FLIP_COUNT];
        ISwitchVectorProperty UnattendedFlipSP;

        IText ProductT[4] {};
        ITextVectorProperty ProductTP;

        virtual int SetRefractionModelTemperature(double temperature);
        INumber RefractionModelTemperatureN[1];
        INumberVectorProperty RefractionModelTemperatureNP;

        virtual int SetRefractionModelPressure(double pressure);
        INumber RefractionModelPressureN[1];
        INumberVectorProperty RefractionModelPressureNP;

        INumber ModelCountN[1];
        INumberVectorProperty ModelCountNP;

        INumber AlignmentPointsN[1];
        INumberVectorProperty AlignmentPointsNP;

        ISwitch AlignmentStateS[ALIGN_COUNT];
        ISwitchVectorProperty AlignmentStateSP;

        INumber MiniNewAlpRON[MALPRO_COUNT];
        INumberVectorProperty MiniNewAlpRONP;
        INumber MiniNewAlpN[MALP_COUNT];
        INumberVectorProperty MiniNewAlpNP;

        INumber NewAlpN[ALP_COUNT];
        INumberVectorProperty NewAlpNP;

        INumber NewAlignmentPointsN[1];
        INumberVectorProperty NewAlignmentPointsNP;

        IText NewModelNameT[1] {};
        ITextVectorProperty NewModelNameTP;

        INumber TLEfromDatabaseN[1];
        INumberVectorProperty TLEfromDatabaseNP;

        // Wake on LAN
        IText WoLMacT[1] {};
        ITextVectorProperty WoLMacTP;
        ISwitch WoLSendS[1];
        ISwitchVectorProperty WoLSendSP;

        // Shutdown
        INDI::PropertySwitch MountShutdownSP {1};

        // Meridian
        INDI::PropertyNumber MeridianLimitsNP {MERIDIAN_LIMIT_COUNT};
        INDI::PropertyNumber TrackingEndNP {1};
        INDI::PropertySwitch AllowedMeridianSideSP {MERIDIAN_SIDE_COUNT};
        INDI::PropertySwitch TargetPierSideSP {TARGET_PIER_COUNT};

    private:
        int fd = -1; // short notation for PortFD/sockfd
        bool getMountInfo();
        bool flip();
        bool sendWakeOnLanPacket();
        bool m_ShutdownPending { false };
        void TimerHit() override;

        // Meridian values are polled less often than Ginfo, see ReadScopeStatus
        void updateMeridianValues(bool all);
        std::chrono::steady_clock::time_point lastMeridianFastPoll;
        std::chrono::steady_clock::time_point lastMeridianSlowPoll;

        int OldGstat = GSTAT_UNSET;
        struct _Ginfo
        {
            float RA_JNOW   = 0.0;
            float DEC_JNOW  = 0.0;
            char SideOfPier = 'x';
            float AZ        = 0.0;
            float ALT       = 0.0;
            float Jdate     = 0.0;
            int Gstat       = -1;
            int SlewStatus  = -1;
            // added :
            double SiderealTime = -1;
        } Ginfo;
        int AlignmentState = ALIGN_IDLE;

};
