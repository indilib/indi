/*
    LX200 Classic
    Copyright (C) 2003-2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

class LX200Classic : public LX200Generic
{
    public:
        LX200Classic();
        ~LX200Classic() {}

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:

        const char *getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;
        virtual bool saveConfigItems(FILE *fp) override;

        // Parking
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool ReadScopeStatus() override;

    private:

        ITextVectorProperty ObjectInfoTP;
        IText ObjectInfoT[1] {};

        ISwitchVectorProperty StarCatalogSP;
        ISwitch StarCatalogS[3];

        ISwitchVectorProperty DeepSkyCatalogSP;
        ISwitch DeepSkyCatalogS[7];

        ISwitchVectorProperty SolarSP;
        ISwitch SolarS[10];

        INumberVectorProperty ObjectNoNP;
        INumber ObjectNoN[1];

        INumberVectorProperty MaxSlewRateNP;
        INumber MaxSlewRateN[1];

        INumberVectorProperty ElevationLimitNP;
        INumber ElevationLimitN[2];
        
        ISwitchVectorProperty UnparkAlignmentSP;
        ISwitch UnparkAlignmentS[3];

    private:
        int currentCatalog {0};
        int currentSubCatalog {0};
        
        void azAltToRaDecNow(double az, double alt, double &ra, double &dec);
        void raDecToAzAltNow(double ra, double dec, double &az, double &alt);
        
};
