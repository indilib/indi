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

#ifndef RollOff_H
#define RollOff_H

#include "indibase/indidome.h"

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>


class RollOff : public INDI::Dome
{

    public:
        RollOff();
        virtual ~RollOff();

        virtual bool initProperties();
        const char *getDefaultName();
        bool updateProperties();

      protected:

        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation);
        virtual IPState Park();
        virtual IPState UnPark();                
        virtual bool Abort();

        virtual bool getFullOpenedLimitSwitch();
        virtual bool getFullClosedLimitSwitch();

    private:

        ISState fullOpenLimitSwitch;
        ISState fullClosedLimitSwitch;

        double MotionRequest;
        struct timeval MotionStart;
        bool SetupParms();

        float CalcTimeLeft(timeval);

};

#endif
