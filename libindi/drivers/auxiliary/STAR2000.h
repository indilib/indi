/*******************************************************************************
  created 2014 G. Schmidt

  derived from gpusb code from Jasem Mutlaq

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#ifndef STAR2000_H
#define STAR2000_H

#include "libs/indibase/defaultdevice.h"
#include "libs/indibase/indiguiderinterface.h"


/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include <sys/time.h>
#include <time.h>

class STAR2000 : public INDI::GuiderInterface, public INDI::DefaultDevice
{
    public:
    STAR2000();

    virtual bool initProperties();
    virtual bool updateProperties();
    virtual void ISGetProperties (const char *dev);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISSnoopDevice (XMLEle *root);

    protected:

    virtual bool saveConfigItems(FILE *fp);

    //  Generic indi device entries
    bool Connect();
    bool Connect(char *);
    bool Disconnect();
    const char *getDefaultName();

    void TimerHit();

    virtual bool GuideNorth(float ms);
    virtual bool GuideSouth(float ms);
    virtual bool GuideEast(float ms);
    virtual bool GuideWest(float ms);

    /* STAR2000 box RS232 port */

    ITextVectorProperty PortTP;
    IText PortT[1];

    private:

    float CalcWEPulseTimeLeft();
    float CalcNSPulseTimeLeft();


    bool InWEPulse;
    float WEPulseRequest;
    struct timeval WEPulseStart;
    int WEtimerID;


    bool InNSPulse;
    float NSPulseRequest;
    struct timeval NSPulseStart;
    int NStimerID;

    int WEDir;
    int NSDir;

};

#endif // STAR2000_H
