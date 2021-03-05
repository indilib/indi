/*******************************************************************************
 DDW Dome INDI Driver

 Copyright(c) 2020-2021 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Dome INDI Driver

 Copyright(c) 2017-2021 Jarno Paananen. All rights reserved.

 and

 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 Baader Planetarium Dome INDI Driver

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indidome.h"
#include <memory>

class DDW : public INDI::Dome
{
    public:
        DDW();
        virtual ~DDW() = default;

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool saveConfigItems(FILE *fp) override;

        virtual bool Handshake() override;

        virtual void TimerHit() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

        virtual IPState MoveAbs(double az) override;
        virtual IPState ControlShutter(ShutterOperation operation) override;
        virtual bool Abort() override;

        // Parking
        virtual IPState Park() override;
        virtual IPState UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

    protected:
        bool SetupParms();

        INumber FirmwareVersionN[1];
        INumberVectorProperty FirmwareVersionNP;

    private:
        int writeCmd(const char *cmd);
        int readStatus(std::string &status);
        void parseGINF(const char *response);

        int ticksPerRev { 100 };
        double homeAz { 0 };
        int watchdog { 0 };

        int fwVersion{ -1 };

        double gotoTarget { 0 };
        bool gotoPending { false };

        enum
        {
            IDLE,
            MOVING,
            SHUTTER_OPERATION,
            HOMING,
            PARKING,
            UNPARKING
        } cmdState{ IDLE };
};
