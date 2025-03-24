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

#include <cstring>

namespace INDI
{

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
GuiderInterface::GuiderInterface(DefaultDevice *device) : m_defaultDevice(device)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
GuiderInterface::~GuiderInterface()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
void GuiderInterface::initProperties(const char *groupName)
{
    // @INDI_STANDARD_PROPERTY@
    GuideNSNP[DIRECTION_NORTH].fill("TIMED_GUIDE_N", "North (ms)", "%.f", 0, 60000, 100, 0);
    GuideNSNP[DIRECTION_SOUTH].fill("TIMED_GUIDE_S", "South (ms)", "%.f", 0, 60000, 100, 0);
    GuideNSNP.fill(m_defaultDevice->getDeviceName(), "TELESCOPE_TIMED_GUIDE_NS", "Guide N/S", groupName, IP_RW, 60, IPS_IDLE);

    // @INDI_STANDARD_PROPERTY@
    GuideWENP[DIRECTION_WEST].fill("TIMED_GUIDE_W", "West (ms)", "%.f", 0, 60000, 100, 0);
    GuideWENP[DIRECTION_EAST].fill("TIMED_GUIDE_E", "East (ms)", "%.f", 0, 60000, 100, 0);
    GuideWENP.fill(m_defaultDevice->getDeviceName(), "TELESCOPE_TIMED_GUIDE_WE", "Guide E/W", groupName, IP_RW, 60, IPS_IDLE);
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool GuiderInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        m_defaultDevice->defineProperty(GuideNSNP);
        m_defaultDevice->defineProperty(GuideWENP);
    }
    else
    {
        m_defaultDevice->deleteProperty(GuideNSNP);
        m_defaultDevice->deleteProperty(GuideWENP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool GuiderInterface::processNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, m_defaultDevice->getDeviceName()))
    {
        if (GuideNSNP.isNameMatch(name))
        {
            //  We are being asked to send a guide pulse north/south on the st4 port
            GuideNSNP.update(values, names, n);

            if (GuideNSNP[DIRECTION_NORTH].getValue() != 0)
            {
                GuideNSNP[DIRECTION_SOUTH].setValue(0);
                GuideNSNP.setState(GuideNorth(GuideNSNP[DIRECTION_NORTH].getValue()));
            }
            else if (GuideNSNP[DIRECTION_SOUTH].getValue() != 0)
                GuideNSNP.setState(GuideSouth(GuideNSNP[DIRECTION_SOUTH].getValue()));

            GuideNSNP.apply();
            return true;
        }

        if (GuideWENP.isNameMatch(name))
        {
            //  We are being asked to send a guide pulse north/south on the st4 port
            GuideWENP.update(values, names, n);

            if (GuideWENP[DIRECTION_WEST].getValue() != 0)
            {
                GuideWENP[DIRECTION_EAST].setValue(0);
                GuideWENP.setState(GuideWest(GuideWENP[DIRECTION_WEST].getValue()));
            }
            else if (GuideWENP[DIRECTION_EAST].getValue() != 0)
                GuideWENP.setState(GuideEast(GuideWENP[DIRECTION_EAST].getValue()));

            GuideWENP.apply();
            return true;
        }
    }

    return false;
}

void GuiderInterface::GuideComplete(INDI_EQ_AXIS axis)
{
    switch (axis)
    {
        case AXIS_DE:
            GuideNSNP.setState(IPS_IDLE);
            GuideNSNP.apply();
            break;

        case AXIS_RA:
            GuideWENP.setState(IPS_IDLE);
            GuideWENP.apply();
            break;
    }
}
}
