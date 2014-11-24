/*******************************************************************************
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

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

#ifndef DomeSIM_H
#define DomeSIM_H

#include "indibase/indidome.h"

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>


class DomeSim : public INDI::Dome
{

    public:
        DomeSim();
        virtual ~DomeSim();

        const char *getDefaultName();
        bool updateProperties();

        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual int MoveRelDome(DomeDirection dir, double azDiff);
        virtual int MoveAbsDome(double az);
        virtual int ParkDome();
        virtual int HomeDome();
        virtual int ControlDomeShutter(ShutterOperation operation);
        virtual bool AbortDome();

    protected:
    private:

        double targetAz;
        double shutterTimer;
        double prev_az, prev_alt;
        bool SetupParms();

};

#endif
