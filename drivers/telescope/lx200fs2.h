/*
    Astro-Electronic FS-2
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include "alignment/AlignmentSubsystemForDrivers.h"

class LX200FS2 : public LX200Generic
{
    public:
        LX200FS2();

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual const char *getDefaultName() override;
        virtual bool isSlewComplete() override;
        virtual bool checkConnection() override;

        virtual bool saveConfigItems(FILE *fp) override;

        // Parking
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

        // StopAfterPark
        virtual bool ReadScopeStatus() override;
        void TrackingStart();
        void TrackingStart_RestoreSlewRate();
        void TrackingStop();
        void TrackingStop_Abort();
        void TrackingStop_AllStop();

        // Fake Location
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;

        INumber SlewAccuracyN[2];
        INumberVectorProperty SlewAccuracyNP;

        ISwitchVectorProperty StopAfterParkSP;
        ISwitch StopAfterParkS[2];
        bool MotorsParked;
        enum TelescopeSlewRate savedSlewRateIndex {SLEW_MAX};
        enum TelescopeParkedStatus
        {
            PARKED_NOTPARKED = 0,
            PARKED_NEEDABORT,
            PARKED_NEEDSTOP,
            PARKED_STOPPED,
            UNPARKED_NEEDSLEW,
        } ParkedStatus;
};
