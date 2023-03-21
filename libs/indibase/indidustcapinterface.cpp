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

#include "indidustcapinterface.h"

#include <cstring>

namespace INDI
{

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
DustCapInterface::DustCapInterface(DefaultDevice *device) : AbstractInterface(device)
{
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void DustCapInterface::initProperties(const char *group)
{
    // Open/Close cover
    ParkCapSP[CAP_PARK].fill("PARK", "Park", ISS_OFF);
    ParkCapSP[CAP_UNPARK].fill("UNPARK", "Unpark", ISS_OFF);
    ParkCapSP.fill(m_DefaultDevice->getDeviceName(), "CAP_PARK", "Dust Cover", group, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
bool DustCapInterface::updateProperties()
{
    if (m_DefaultDevice->isConnected())
        m_DefaultDevice->defineProperty(ParkCapSP);
    else
        m_DefaultDevice->deleteProperty(ParkCapSP);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
bool DustCapInterface::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[],  int n)
{
    INDI_UNUSED(dev);
    // Park/UnPark Dust Cover
    if (ParkCapSP.isNameMatch(name))
    {
        auto prevSwitch = ParkCapSP.findOnSwitchIndex();
        ParkCapSP.update(states, names, n);
        ParkCapSP.setState(ParkCapSP[CAP_PARK].getState() == ISS_ON ? ParkCap() : UnParkCap());
        if (ParkCapSP.getState() == IPS_ALERT)
        {
            ParkCapSP.reset();
            ParkCapSP[prevSwitch].setState(ISS_ON);
        }

        ParkCapSP.apply();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
IPState DustCapInterface::ParkCap()
{
    // Must be implemented by child class
    return IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
IPState DustCapInterface::UnParkCap()
{
    // Must be implemented by child class
    return IPS_ALERT;
}
}
