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

#include "IndiCcd.h"

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>


class CcdSim : public IndiCcd
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
        float CenterOffsetDec;
        float TimeFactor;
        //  our zero point calcs used for drawing stars
        float k;
        float z;

        bool AbortGuideFrame;

        float RA;
        float Dec;
        float GuideRate;

        float PEPeriod;
        float PEMax;
        time_t RunStart;

        //  We are going to snoop these from a telescope
        INumberVectorProperty EqNV;
        INumber EqN[2];

        //  And this lives in our simulator settings page

        INumberVectorProperty SimulatorSettingsNV;
        INumber SimulatorSettingsN[13];

        ITextVectorProperty ConfigFileTV; //  A text vector that stores our configuration name
        IText ConfigFileT[1];

        ISwitch ConfigSaveRestoreS[2];
        ISwitchVectorProperty ConfigSaveRestoreSV;

        ISwitch TimeFactorS[3];
        ISwitchVectorProperty TimeFactorSV;

        bool SetupParms();
        int MakeConfigName(char *);


    public:
        CcdSim();
        virtual ~CcdSim();

        char *getDefaultName();

        int init_properties();
        void ISGetProperties (const char *dev);
        void ISSnoopDevice (XMLEle *root);

        bool UpdateProperties();

        bool Connect();
        bool Disconnect();

        int StartExposure(float);
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
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

};

#endif // CCDSIM_H
