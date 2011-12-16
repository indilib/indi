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
    protected:
    private:
    public:

        //char *RawFrame;
        //int RawFrameSize;
        //int XRes;
        //int YRes;
        //int GXRes;
        //int GYRes;
        //char *RawGuiderFrame;
        //int RawGuideSize;
        //int Interlaced;
        //bool HasGuideHead;

        int SetCamTimer(int ms);
        int GetCamTimer();

        int DidFlush;
        int DidLatch;
        int SubType;
        unsigned short int CameraModel;

        bool ColorSensor;
		int CamBits;
		int GuiderBits;

        float CalcTimeLeft();
        bool InExposure;
        float ExposureRequest;
        struct timeval ExpStart;

        int DidGuideLatch;
        bool InGuideExposure;
        float GuideExposureRequest;
        struct timeval GuideExpStart;
        float CalcGuideTimeLeft();

        int ReadCameraFrame(int,char *);

        ISwitch StreamS[2];
        ISwitchVectorProperty *StreamSP;


         SxCam();
        virtual ~SxCam();

        //  Generic indi device entries
        bool Connect();
        bool Disconnect();

        const char *getDefaultName();

        int StartExposure(float);
        int StartGuideExposure(float);
        bool AbortGuideExposure();

        void TimerHit();

        virtual bool initProperties();
        virtual bool updateProperties();
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual int SetParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight);
        virtual int SetGuideParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight);
        virtual int SetInterlaced(bool);

};

#endif // SXCAM_H
