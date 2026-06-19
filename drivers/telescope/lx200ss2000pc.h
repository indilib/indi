/*
    SkySensor2000 PC
    Copyright (C) 2015 Camiel Severijns
    Copyright (C) 2025 Jasem Mutlaq

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

class LX200SS2000PC : public LX200Generic
{
    public:
        LX200SS2000PC(void);

        virtual const char *getDefaultName(void) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual void getBasicData(void) override;
        virtual bool isSlewComplete(void) override;

        virtual bool Goto(double ra, double dec) override;
        virtual bool Sync(double ra, double dec) override;
        virtual bool ReadScopeStatus() override;

        virtual bool saveConfigItems(FILE *fp) override;

        virtual bool updateTime(ln_date *utc, double utc_offset) override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool setUTCOffset(double offset) override;

        // Parking
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;
        virtual bool Park() override;
        virtual bool UnPark() override;

    private:
        bool getCalendarDate(int &year, int &month, int &day);
        bool setCalenderDate(int year, int month, int day);

        int setSiteLongitude(int fd, double Long);
        int setSiteLatitude(int fd, double Long);

        INumber SlewAccuracyN[2];
        INumberVectorProperty SlewAccuracyNP;

        static const int ShortTimeOut;
        static const int LongTimeOut;
};
