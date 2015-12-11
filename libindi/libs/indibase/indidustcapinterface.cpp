/*
    Dust Cap Interface
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <string.h>

#include "indidustcapinterface.h"
#include "indilogger.h"

INDI::DustCapInterface::DustCapInterface()
{
}

INDI::DustCapInterface::~DustCapInterface()
{
}

void INDI::DustCapInterface::initDustCapProperties(const char *deviceName, const char* groupName)
{
    strncpy(dustCapName, deviceName, MAXINDIDEVICE);

    // Open/Close cover
    IUFillSwitch(&ParkCapS[0], "PARK", "Park", ISS_OFF);
    IUFillSwitch(&ParkCapS[1], "UNPARK", "Unpark", ISS_OFF);
    IUFillSwitchVector(&ParkCapSP, ParkCapS, 2, deviceName, "CAP_PARK", "Dust Cover", groupName, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
}

bool INDI::DustCapInterface::processDustCapSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Park/UnPark Dust Cover
    if (!strcmp(ParkCapSP.name, name))
    {
        int prevSwitch = IUFindOnSwitchIndex(&ParkCapSP);
        IUUpdateSwitch(&ParkCapSP, states, names, n);
        if (ParkCapS[0].s == ISS_ON)
            ParkCapSP.s = ParkCap();
        else
            ParkCapSP.s = UnParkCap();

        if (ParkCapSP.s == IPS_ALERT)
        {
            IUResetSwitch(&ParkCapSP);
            ParkCapS[prevSwitch].s = ISS_ON;
        }

        IDSetSwitch(&ParkCapSP, NULL);
        return true;
    }

    return false;
}

IPState INDI::DustCapInterface::ParkCap()
{
    // Must be implemented by child class
    return IPS_ALERT;
}

IPState INDI::DustCapInterface::UnParkCap()
{
    // Must be implemented by child class
    return IPS_ALERT;
}
