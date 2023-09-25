/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI GPS Device Class

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

#include "indigps.h"

#include <cstring>

namespace INDI
{

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
GPS::GPS() : GI(this)
{

}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool GPS::initProperties()
{
    DefaultDevice::initProperties();

    GI::initProperties("Main Control");

    setDefaultPollingPeriod(2000);

    // Default interface, can be overridden by child
    setDriverInterface(GPS_INTERFACE);

    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool GPS::updateProperties()
{
    DefaultDevice::updateProperties();
    GI::updateProperties();
    return true;
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool GPS::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (GI::processSwitch(dev, name, states, names, n))
        {
            if (SystemTimeUpdateSP.isNameMatch(name))
                saveConfig(true, SystemTimeUpdateSP.getName());
            return true;
        }
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool GPS::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (GI::processNumber(dev, name, values, names, n))
            return true;
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

//////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////
bool GPS::saveConfigItems(FILE *fp)
{
    DefaultDevice::saveConfigItems(fp);
    GI::saveConfigItems(fp);
    return true;
}
}
