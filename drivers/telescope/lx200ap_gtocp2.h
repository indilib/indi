/*
    Astro-Physics INDI driver

    Tailored for GTOCP2

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

#include "lx200generic.h"

class LX200AstroPhysicsGTOCP2 : public LX200Generic
{
    public:
        LX200AstroPhysicsGTOCP2();
        ~LX200AstroPhysicsGTOCP2() {}

        typedef enum { MCV_E, MCV_F, MCV_G, MCV_H, MCV_I, MCV_J, MCV_L, MCV_P, MCV_UNKNOWN} ControllerVersion;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual void ISGetProperties(const char *dev) override;

    protected:
        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ReadScopeStatus() override;
        virtual bool Handshake() override;
        virtual bool Disconnect() override;

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

        virtual int  SendPulseCmd(int8_t direction, uint32_t duration_msec) override;

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

        INDI::PropertyNumber HourangleCoordsNP {2}; // HA, DEC

        INDI::PropertyNumber HorizontalCoordsNP {2}; // AZ, ALT

        INDI::PropertySwitch APSlewSpeedSP {3}; // 600x, 900x, 1200x

        INDI::PropertySwitch SwapSP {2}; // NS, EW

        INDI::PropertySwitch SyncCMRSP {2}; // :CM#, :CMR#
        enum { USE_REGULAR_SYNC, USE_CMR_SYNC };

        INDI::PropertySwitch APGuideSpeedSP {3}; // 0.25x, 0.50x, 1.0x

        INDI::PropertyText VersionTP {1}; // Firmware Version

    private:
        bool initMount();

        // Side of pier
        void syncSideOfPier();

        bool timeUpdated = false, locationUpdated = false;
        ControllerVersion firmwareVersion = MCV_UNKNOWN;

        double currentAlt = 0, currentAz = 0;
        double lastRA = 0, lastDE = 0;
        double lastAZ = 0, lastAL = 0;

        bool motionCommanded = true;
        bool mountInitialized = false;
};
