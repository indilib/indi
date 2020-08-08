/*
    Astro-Physics INDI driver

    Copyright (C) 2014 Jasem Mutlaq

    Based on INDI Astrophysics Driver by Markus Wildi

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
#define MOUNTNOTINITIALIZED 0
#define MOUNTINITIALIZED    1

class LX200AstroPhysicsExperimental : public LX200Generic
{
    public:
        LX200AstroPhysicsExperimental();

        typedef enum { MCV_D, MCV_E, MCV_F, MCV_G, MCV_H, MCV_I, MCV_J, MCV_K_UNUSED,
                       MCV_L, MCV_M, MCV_N, MCV_O, MCV_P, MCV_Q, MCV_R, MCV_S,
                       MCV_T, MCV_U, MCV_V, MCV_UNKNOWN
                     } ControllerVersion;
        typedef enum { GTOCP1 = 1, GTOCP2, GTOCP3, GTOCP4, GTOCP_UNKNOWN} ServoVersion;
        typedef enum { PARK_LAST = 0, PARK_CUSTOM = 0, PARK_PARK1 = 1, PARK_PARK2 = 2, PARK_PARK3 = 3, PARK_PARK4 = 4} ParkPosition;
        enum APTelescopeSlewRate {AP_SLEW_GUIDE, AP_SLEW_12X, AP_SLEW_64X, AP_SLEW_600X, AP_SLEW_1200X};
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual void ISGetProperties(const char *dev) override;

    protected:

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ReadScopeStatus() override;
        virtual bool Handshake() override;
        virtual bool Disconnect() override;
        virtual bool Connect() override;

        // Parking
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;
        virtual bool Park() override;
        virtual bool UnPark() override;

        virtual bool Sync(double ra, double dec) override;
        virtual bool Goto(double, double) override;
        virtual bool updateTime(ln_date *utc, double utc_offset) override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool SetSlewRate(int index) override;
        bool updateAPSlewRate(int index);

        // Guide Commands
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;
        virtual int  SendPulseCmd(int8_t direction, uint32_t duration_msec) override;
        virtual bool GuideNS(INDI_DIR_NS dir, TelescopeMotionCommand command);
        virtual bool GuideWE(INDI_DIR_WE dir, TelescopeMotionCommand command);

        // Pulse Guide specific to AstroPhysics
        static void pulseGuideTimeoutHelperWE(void * p);
        static void pulseGuideTimeoutHelperNS(void * p);
        static void simulGuideTimeoutHelperWE(void * p);
        static void simulGuideTimeoutHelperNS(void * p);
        void AstroPhysicsGuideTimeoutWE(bool simul);
        void AstroPhysicsGuideTimeoutNS(bool simul);

        virtual bool getUTFOffset(double *offset) override;
        // Tracking
        virtual bool SetTrackMode(uint8_t mode) override;
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool SetTrackRate(double raRate, double deRate) override;

        // NSWE Motion Commands
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

        virtual bool saveConfigItems(FILE *fp) override;

        virtual void debugTriggered(bool enable) override;

        void handleGTOCP2MotionBug();

        ISwitch StartUpS[2];
        ISwitchVectorProperty StartUpSP;

        INumber HourangleCoordsN[2];
        INumberVectorProperty HourangleCoordsNP;
        INumber APSiderealTimeN[1];
        INumberVectorProperty APSiderealTimeNP;

        INumber HorizontalCoordsN[2];
        INumberVectorProperty HorizontalCoordsNP;

        ISwitch APSlewSpeedS[3];
        ISwitchVectorProperty APSlewSpeedSP;

        ISwitch SwapS[2];
        ISwitchVectorProperty SwapSP;

        ISwitch SyncCMRS[2];
        ISwitchVectorProperty SyncCMRSP;
        enum { USE_REGULAR_SYNC, USE_CMR_SYNC };

        ISwitch APGuideSpeedS[3];
        ISwitchVectorProperty APGuideSpeedSP;

        ISwitch UnparkFromS[5];
        ISwitchVectorProperty UnparkFromSP;

        ISwitch ParkToS[5];
        ISwitchVectorProperty ParkToSP;

        INumberVectorProperty APUTCOffsetNP;
        INumber APUTCOffsetN[1];

        IText VersionT[1] {};
        ITextVectorProperty VersionInfo;

    private:
#ifdef no
        bool initMount();
#endif

        // Side of pier
        void syncSideOfPier();
#ifdef no
        bool IsMountInitialized(bool *initialized);
#endif
        bool IsMountParked(bool *isParked);
        bool getMountStatus(bool *isParked);
        bool getFirmwareVersion(void);
        bool calcParkPosition(ParkPosition pos, double *parkAlt, double *parkAz);
        void disclaimerMessage(void);

        //bool timeUpdated=false, locationUpdated=false;
        ControllerVersion firmwareVersion = MCV_UNKNOWN;
        ServoVersion servoType = GTOCP_UNKNOWN;

        double currentAlt = 0, currentAz = 0;
        double lastRA = 0, lastDE = 0;
        double lastAZ = 0, lastAL = 0;

        //int GuideNSTID;
        //int GuideWETID;

        //bool motionCommanded=false; // 2020-05-24, wildi, never reset
        //bool mountInitialized=false;
        int rememberSlewRate = { -1 };
        uint8_t initStatus = MOUNTNOTINITIALIZED;
};
