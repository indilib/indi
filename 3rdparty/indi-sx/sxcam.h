/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.
               2012 Jasem Mutlaq

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

#ifndef SXCAM_H
#define SXCAM_H

#include <libindi/indiccd.h>
#include <libindi/indiusbdevice.h>

#include "sxccd.h"

/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include <sys/time.h>
#include <time.h>


class SxCam : public INDI::CCD, public SxCCD
{
    public:
    SxCam();
    virtual ~SxCam();

    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    protected:

    //  Generic indi device entries
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();

    virtual bool initProperties();
    virtual bool updateProperties();

    int StartExposure(float);
    int StartGuideExposure(float);
    bool AbortGuideExposure();
    void TimerHit();
    virtual bool updateCCDBin(int hor, int ver);
    virtual int SetParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight);
    virtual int SetGuideParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight);
    virtual int SetInterlaced(bool);

    virtual bool GuideNorth(float ms);
    virtual bool GuideSouth(float ms);
    virtual bool GuideEast(float ms);
    virtual bool GuideWest(float ms);

    private:

        ISwitch ModelS[13];
        ISwitchVectorProperty ModelSP;

        int SetCamTimer(int ms);
        int GetCamTimer();
        int ReadCameraFrame(int,char *);
        float CalcTimeLeft();
        float CalcGuideTimeLeft();
        float CalcWEPulseTimeLeft();
        float CalcNSPulseTimeLeft();

        int DidFlush;
        int DidLatch;
        int DidGuideLatch;
        int SubType;
        unsigned short int CameraModel;

        bool ColorSensor;
        bool InGuideExposure;
        float GuideExposureRequest;
        struct timeval GuideExpStart;
        float ExposureRequest;
        struct timeval ExpStart;

        bool InWEPulse;
        float WEPulseRequest;
        struct timeval WEPulseStart;
        int WEtimerID;


        bool InNSPulse;
        float NSPulseRequest;
        struct timeval NSPulseStart;
        int NStimerID;



        char *evenBuf, *oddBuf;


};

#endif // SXCAM_H
