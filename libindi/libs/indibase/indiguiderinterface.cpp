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

    IUFillNumber(&GuideNSN[0],"TIMED_GUIDE_N","North (msec)","%g",0,60000,10,0);
    IUFillNumber(&GuideNSN[1],"TIMED_GUIDE_S","South (msec)","%g",0,60000,10,0);
    IUFillNumberVector(&GuideNSNP,GuideNSN,2,deviceName,"TELESCOPE_TIMED_GUIDE_NS","Guide North/South",groupName,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideWEN[0],"TIMED_GUIDE_E","East (msec)","%g",0,60000,10,0);
    IUFillNumber(&GuideWEN[1],"TIMED_GUIDE_W","West (msec)","%g",0,60000,10,0);
    IUFillNumberVector(&GuideWENP,GuideWEN,2,deviceName,"TELESCOPE_TIMED_GUIDE_WE","Guide East/West",groupName,IP_RW,60,IPS_IDLE);
}

void INDI::GuiderInterface::processGuiderProperties(const char *name, double values[], char *names[], int n)
{
    if(strcmp(name,GuideNSNP.name)==0)
    {
        //  We are being asked to send a guide pulse north/south on the st4 port
        GuideNSNP.s=IPS_BUSY;
        IUUpdateNumber(&GuideNSNP,values,names,n);
        //  Update client display
        IDSetNumber(&GuideNSNP,NULL);


        if(GuideNSN[0].value != 0)
        {
            GuideNorth(GuideNSN[0].value);
        }
        if(GuideNSN[1].value != 0) {
            GuideSouth(GuideNSN[1].value);
        }
        GuideNSN[0].value=0;
        GuideNSN[1].value=0;
        GuideNSNP.s=IPS_OK;
        IDSetNumber(&GuideNSNP,NULL);

        return;
    }

    if(strcmp(name,GuideWENP.name)==0)
    {
        //  We are being asked to send a guide pulse north/south on the st4 port
        GuideWENP.s=IPS_BUSY;
        IUUpdateNumber(&GuideWENP,values,names,n);
        //  Update client display
        IDSetNumber(&GuideWENP,NULL);


        if(GuideWEN[0].value != 0)
        {
            GuideEast(GuideWEN[0].value);
        } else
        {
            GuideWest(GuideWEN[1].value);
        }

        GuideWEN[0].value=0;
        GuideWEN[1].value=0;
        GuideWENP.s=IPS_OK;
        IDSetNumber(&GuideWENP,NULL);

        return;
    }
}
