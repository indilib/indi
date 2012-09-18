/*******************************************************************************
  Copyright(c) 2012 Jasem Mutlaq. All rights reserved.

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

#ifndef QHYCCD_H
#define QHYCCD_H

#include <libindi/indiccd.h>
#include <libindi/indiusbdevice.h>

/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include <sys/time.h>
#include <time.h>

class QHY5Driver;

class QHYCCD : public INDI::CCD
{
    public:
    QHYCCD();
    virtual ~QHYCCD();

    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);

    protected:

    //  Generic indi device entries
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();
    virtual bool initProperties();

    virtual bool updateProperties();
    int StartExposure(float);

    void TimerHit();
    //virtual int SetParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight);

    virtual bool updateCCDFrame(int x, int y, int w, int h);
    virtual bool updateCCDBin(int hor, int ver);

    virtual bool GuideNorth(float ms);
    virtual bool GuideSouth(float ms);
    virtual bool GuideEast(float ms);
    virtual bool GuideWest(float ms);

    private:

       void ReadCameraFrame();
       float CalcTimeLeft();


        float CalcPulseTimeLeft();

        struct timeval ExpStart;
        struct timeval PulseStart;

        float ExposureRequest;
        float PulseRequest;



        bool InExposure;
        bool InPulse;

        int guideDirection;

        QHY5Driver *driver;

        INumberVectorProperty GainNP;
        INumber GainN[1];



};

#endif // QHYCCD_H
