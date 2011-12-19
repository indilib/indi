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

INDI::GuiderInterface::GuiderInterface()
{
    GuideNSP = new INumberVectorProperty;
    GuideEWP = new INumberVectorProperty;
}

INDI::GuiderInterface::~GuiderInterface()
{
    delete GuideNSP;
    delete GuideEWP;
}

void INDI::GuiderInterface::initGuiderProperties(const char *deviceName, const char* groupName)
{

    IUFillNumber(&GuideNS[0],"TIMED_GUIDE_N","North (sec)","%g",0,10,0.001,0);
    IUFillNumber(&GuideNS[1],"TIMED_GUIDE_S","South (sec)","%g",0,10,0.001,0);
    IUFillNumberVector(GuideNSP,GuideNS,2,deviceName,"TELESCOPE_TIMED_GUIDE_NS","Guide North/South",groupName,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideEW[0],"TIMED_GUIDE_E","East (sec)","%g",0,10,0.001,0);
    IUFillNumber(&GuideEW[1],"TIMED_GUIDE_W","West (sec)","%g",0,10,0.001,0);
    IUFillNumberVector(GuideEWP,GuideEW,2,deviceName,"TELESCOPE_TIMED_GUIDE_WE","Guide East/West",groupName,IP_RW,60,IPS_IDLE);
}
