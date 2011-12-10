/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#ifndef CCDSIM_H
#define CCDSIM_H

#include "indibase/indiccd.h"

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>


class CCDSim : public INDI::CCD
{
    protected:
    private:

        bool InExposure;
        float ExposureRequest;
        struct timeval ExpStart;

        bool InGuideExposure;
        float GuideExposureRequest;
        struct timeval GuideExpStart;

        float CalcTimeLeft(timeval,float);

        int testvalue;
        int ShowStarField;
        int bias;
        int maxnoise;
        int maxval;
        float skyglow;
        float limitingmag;
        float saturationmag;
        float seeing;
        float ImageScalex;
        float ImageScaley;
        float focallength;
        float OAGoffset;
        float TimeFactor;
        //  our zero point calcs used for drawing stars
        float k;
        float z;

        bool AbortGuideFrame;


        float GuideRate;

        float PEPeriod;
        float PEMax;
        time_t RunStart;

        //  And this lives in our simulator settings page

        INumberVectorProperty *SimulatorSettingsNV;
        INumber SimulatorSettingsN[13];

        ISwitch TimeFactorS[3];
        ISwitchVectorProperty *TimeFactorSV;

        bool SetupParms();

    public:
        CCDSim();
        virtual ~CCDSim();

        const char *getDefaultName();

        bool initProperties();
        bool updateProperties();

        void ISGetProperties (const char *dev);


        bool Connect();
        bool Disconnect();

        int StartExposure(float duration);
        int StartGuideExposure(float);
        bool AbortGuideExposure();


        void TimerHit();

        int DrawCcdFrame();
        int DrawGuiderFrame();

        int DrawImageStar(float,float,float);
        int AddToPixel(int,int,int);

        int GuideNorth(float);
        int GuideSouth(float);
        int GuideEast(float);
        int GuideWest(float);

        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

};

#endif // CCDSim_H
