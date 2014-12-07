/*******************************************************************************
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

#ifndef BaaderDome_H
#define BaaderDome_H

#include "indibase/indidome.h"

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>


class BaaderDome : public INDI::Dome
{
    public:

        BaaderDome();
        virtual ~BaaderDome();

        const char *getDefaultName();
        virtual bool initProperties();
        virtual bool updateProperties();

        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual int MoveRelDome(DomeDirection dir, double azDiff);
        virtual int MoveAbsDome(double az);
        virtual int ParkDome();
        virtual int HomeDome();
        virtual int ControlDomeShutter(ShutterOperation operation);

    protected:

        // Commands
        bool Ack();
        bool UpdatePosition();
        bool UpdateShutterStatus();

        //Misc
        unsigned short MountAzToDomeAz(double mountAz);
        double DomeAzToMountAz(unsigned short domeAz);
        bool SetupParms();


        double targetAz;
        ShutterStatus targetShutter;
        double prev_az, prev_alt;
        int PortFD;


        bool sim;
        double simShutterTimer;
        ShutterStatus simShutterStatus;
};

#endif
