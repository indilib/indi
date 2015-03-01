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
    IUFillNumberVector(&GuideNSNP,GuideNSN,2,deviceName,"TELESCOPE_TIMED_GUIDE_NS","Guide N/S",groupName,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideWEN[0],"TIMED_GUIDE_E","East (msec)","%g",0,60000,10,0);
    IUFillNumber(&GuideWEN[1],"TIMED_GUIDE_W","West (msec)","%g",0,60000,10,0);
    IUFillNumberVector(&GuideWENP,GuideWEN,2,deviceName,"TELESCOPE_TIMED_GUIDE_WE","Guide E/W",groupName,IP_RW,60,IPS_IDLE);
}

void INDI::GuiderInterface::processGuiderProperties(const char *name, double values[], char *names[], int n)
{
    if(strcmp(name,GuideNSNP.name)==0)
    {
        //  We are being asked to send a guide pulse north/south on the st4 port
        IUUpdateNumber(&GuideNSNP,values,names,n);
        bool rc= false;

        if(GuideNSN[0].value != 0)
        {
            GuideNSN[1].value = 0;
            rc = GuideNorth(GuideNSN[0].value);
        }
        else if(GuideNSN[1].value != 0)
        {
            rc = GuideSouth(GuideNSN[1].value);
        }

        GuideNSNP.s= rc ? IPS_OK : IPS_ALERT;
        IDSetNumber(&GuideNSNP,NULL);
        return;
    }

    if(strcmp(name,GuideWENP.name)==0)
    {
        //  We are being asked to send a guide pulse north/south on the st4 port
        IUUpdateNumber(&GuideWENP,values,names,n);
        bool rc=false;

        if(GuideWEN[0].value != 0)
        {
            GuideWEN[1].value = 0;
            rc = GuideEast(GuideWEN[0].value);
        }
        else if(GuideWEN[1].value != 0)
            rc = GuideWest(GuideWEN[1].value);

        GuideWENP.s= rc ? IPS_OK : IPS_ALERT;
        IDSetNumber(&GuideWENP,NULL);
        return;
    }
}


