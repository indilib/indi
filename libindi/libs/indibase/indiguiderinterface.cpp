/*
    Guider Interface
    Copyright (C) 2011 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "indiguiderinterface.h"
#include <indiapi.h>

#include <string.h>

INDI::GuiderInterface::GuiderInterface()
{

}

INDI::GuiderInterface::~GuiderInterface()
{

}

void INDI::GuiderInterface::initGuiderProperties(const char *deviceName, const char* groupName)
{

    IUFillNumber(&GuideNS[0],"TIMED_GUIDE_N","North (msec)","%g",0,60000,10,0);
    IUFillNumber(&GuideNS[1],"TIMED_GUIDE_S","South (msec)","%g",0,60000,10,0);
    IUFillNumberVector(&GuideNSP,GuideNS,2,deviceName,"TELESCOPE_TIMED_GUIDE_NS","Guide North/South",groupName,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideEW[0],"TIMED_GUIDE_E","East (msec)","%g",0,60000,10,0);
    IUFillNumber(&GuideEW[1],"TIMED_GUIDE_W","West (msec)","%g",0,60000,10,0);
    IUFillNumberVector(&GuideEWP,GuideEW,2,deviceName,"TELESCOPE_TIMED_GUIDE_WE","Guide East/West",groupName,IP_RW,60,IPS_IDLE);
}

void INDI::GuiderInterface::processGuiderProperties(const char *name, double values[], char *names[], int n)
{
    if(strcmp(name,GuideNSP.name)==0)
    {
        //  We are being asked to send a guide pulse north/south on the st4 port
        GuideNSP.s=IPS_BUSY;
        IUUpdateNumber(&GuideNSP,values,names,n);
        //  Update client display
        IDSetNumber(&GuideNSP,NULL);


        if(GuideNS[0].value != 0)
        {
            GuideNorth(GuideNS[0].value);
        }
        if(GuideNS[1].value != 0) {
            GuideSouth(GuideNS[1].value);
        }
        GuideNS[0].value=0;
        GuideNS[1].value=0;
        GuideNSP.s=IPS_OK;
        IDSetNumber(&GuideNSP,NULL);

        return;
    }

    if(strcmp(name,GuideEWP.name)==0)
    {
        //  We are being asked to send a guide pulse north/south on the st4 port
        GuideEWP.s=IPS_BUSY;
        IUUpdateNumber(&GuideEWP,values,names,n);
        //  Update client display
        IDSetNumber(&GuideEWP,NULL);


        if(GuideEW[0].value != 0)
        {
            GuideEast(GuideEW[0].value);
        } else
        {
            GuideWest(GuideEW[1].value);
        }

        GuideEW[0].value=0;
        GuideEW[1].value=0;
        GuideEWP.s=IPS_OK;
        IDSetNumber(&GuideEWP,NULL);

        return;
    }
}
