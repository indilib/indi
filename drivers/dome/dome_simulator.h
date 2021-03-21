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

#pragma once

#include "indidome.h"

/**
 * @brief The DomeSim class provides an absolute position dome that supports parking, unparking, and slaving.
 *
 * The driver can support custom parking positions and includes shutter control. It can be used to simulate dome slaving.
 *
 * The dome parameters must be set before slaving is enabled. Furthermore, the dome listens to changes in the TARGET_EOD_COORS of the mount driver
 * in order to make the decision to move to a new target location.
 *
 * All the mathematical models are taken care of in the base INDI::Dome class.
 */
class DomeSim : public INDI::Dome
{
    public:
        DomeSim();
        virtual ~DomeSim() = default;

        virtual bool initProperties() override;
        const char *getDefaultName();
        bool updateProperties();

    protected:
        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation);
        virtual IPState MoveRel(double azDiff);
        virtual IPState MoveAbs(double az);
        virtual IPState Park();
        virtual IPState UnPark();
        virtual IPState ControlShutter(ShutterOperation operation);
        virtual bool Abort();

        // Parking
        virtual bool SetCurrentPark();
        virtual bool SetDefaultPark();

    private:
        double targetAz;
        double shutterTimer;
        bool SetupParms();
};
