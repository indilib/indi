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

        virtual bool initProperties();
        const char *getDefaultName();
        bool updateProperties();

        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual IPState MoveRel(DomeDirection dir, double azDiff);
        virtual IPState MoveAbs(double az);
        virtual IPState Park();
        virtual IPState UnPark();
        virtual IPState Home();
        virtual IPState ControlShutter(ShutterStatus operation);
        virtual bool Abort();

        // Parking
        virtual void SetCurrentPark();
        virtual void SetDefaultPark();

    protected:
    private:

        double targetAz;
        double shutterTimer;
        bool SetupParms();

};

#endif
