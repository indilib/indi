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

        typedef enum { DOME_UNKNOWN, DOME_CALIBRATING, DOME_READY } DomeStatus;
        typedef enum { CALIBRATION_UNKNOWN, CALIBRATION_STAGE1, CALIBRATION_STAGE2, CALIBRATION_STAGE3, CALIBRATION_COMPLETE } CalibrationStage;
        typedef enum { FLAP_OPEN, FLAP_CLOSE } FlapOperation;
        typedef enum { FLAP_OPENED,  FLAP_CLOSED,  FLAP_MOVING, FLAP_UNKNOWN } FlapStatus;

        BaaderDome();
        virtual ~BaaderDome();

        const char *getDefaultName();
        virtual bool initProperties();
        virtual bool updateProperties();
        virtual bool saveConfigItems(FILE *fp);

        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

        virtual IPState MoveRel(double azDiff);
        virtual IPState MoveAbs(double az);        
        virtual IPState Home();
        virtual IPState ControlShutter(ShutterOperation operation);
        virtual bool Abort();

        // Parking
        virtual IPState Park();
        virtual IPState UnPark();
        virtual void SetCurrentPark();
        virtual void SetDefaultPark();

    protected:

        ISwitch CalibrateS[1];
        ISwitchVectorProperty CalibrateSP;

        ISwitch DomeFlapS[2];
        ISwitchVectorProperty DomeFlapSP;

        // Commands
        bool Ack();
        bool UpdatePosition();
        bool UpdateShutterStatus();
        int  ControlDomeFlap(FlapOperation operation);
        bool UpdateFlapStatus();
        bool SaveEncoderPosition();
        const char * GetFlapStatusString(FlapStatus status);

        //Misc
        unsigned short MountAzToDomeAz(double mountAz);
        double DomeAzToMountAz(unsigned short domeAz);
        bool SetupParms();


        DomeStatus status;
        FlapStatus flapStatus;
        CalibrationStage calibrationStage;
        double targetAz, calibrationStart, calibrationTarget1, calibrationTarget2;
        ShutterOperation targetShutter;
        FlapOperation targetFlap;
        double prev_az, prev_alt;
        int PortFD;

        bool sim;
        double simShutterTimer, simFlapTimer;
        ShutterStatus simShutterStatus;
        FlapStatus simFlapStatus;
};

#endif
