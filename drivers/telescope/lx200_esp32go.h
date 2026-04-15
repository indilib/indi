/*
    LX200 esp32go
    based on LX200 generic with some bits from LX200_onStep driver 

    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "lx200driver.h"
#include "indicom.h"
#include "connectionplugins/connectiontcp.h"
#include "indifocuserinterface.h"


#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

#define RBB_MAX_LEN 256
#define CMDB_MAX_LEN 32

class LX200_esp32go : public LX200Generic
{
    public:
        LX200_esp32go();
        ~LX200_esp32go() {}

        const char *getDefaultName();
        bool initProperties();
        bool updateProperties();
        void ISGetProperties(const char *dev);
        //virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n); //override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

    protected:
        virtual bool UnPark();

        
        virtual bool ReadScopeStatus() override;
        virtual void getBasicData() override;
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool SetTrackMode(uint8_t mode) override;

        INDI::PropertyText VersionTP {5};
        INDI::PropertyNumber GuideRateNP {2};
        bool guideUpdate = false;
        bool trackmodeUpdate = false;
        bool picgotoMode = false;
        bool activeFocus = true;
        enum TelescopeTrackMode
        {
            TRACK_SIDEREAL,
            TRACK_SOLAR,
            TRACK_LUNAR,
            TRACK_KING
        };

        long int OSTimeoutSeconds = 0;
        long int OSTimeoutMicroSeconds = 100000;
        
        int flushIO(int fd); // copied from LX200_OnStep driver
        int getCommandSingleCharErrorOrLongResponse(int fd, char *data, const char *cmd); // copied from LX200_OnStep driver
        bool sendCommandBlind(const char *cmd); // copied from LX200_OnStep driver
        int getCommandSingleCharResponse(int fd, char *data, const char *cmd);

        void splitString(const char *str, char array[][100], int *count);

        // ------------ FocuserInterface
        IPState MoveAbsFocuser (uint32_t targetTicks);// override;
        IPState MoveRelFocuser (FocusDirection dir, uint32_t ticks);// override;
        bool AbortFocuser ();// override;
        int UpdateFocuser(); //Return = 0 good, -1 = Communication error

};
