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

        LX200_10MICRON();
        ~LX200_10MICRON() {}

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

        const char *getDefaultName() override;
        bool Handshake() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool saveConfigItems(FILE *fp) override;
        bool ReadScopeStatus() override;
        bool Park() override;
        bool UnPark() override;
        bool SetTrackEnabled(bool enabled) override;
        bool Flip(double ra, double dec) override;
        bool getUnattendedFlipSetting();
        bool setUnattendedFlipSetting(bool setting);
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


    private:
        int fd = -1; // short notation for PortFD/sockfd
        bool getMountInfo();
        bool flip();

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
