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
        bool AbortGuideFrame;

    public:
        CcdSim();
        virtual ~CcdSim();

        char *getDefaultName();

        int init_properties();
        void ISGetProperties (const char *dev);
        bool UpdateProperties();

        bool Connect();
        bool Disconnect();

        int StartExposure(float);
        int StartGuideExposure(float);
        bool AbortGuideExposure();


        void TimerHit();

        int DrawCcdFrame();
        int DrawGuiderFrame();

};

#endif // CCDSIM_H
