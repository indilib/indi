/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "filter_simulator.h"

#include <memory>
#include <chrono>
#include <random>
#include <thread>

// We declare an auto pointer to FilterSim.
std::unique_ptr<FilterSim> filter_sim(new FilterSim());

const char *FilterSim::getDefaultName()
{
    return "Filter Simulator";
}

bool FilterSim::initProperties()
{
    INDI::FilterWheel::initProperties();

    DelayNP[0].fill("VALUE", "Seconds", "%.f", -1, 30, 1, 1);
    DelayNP.fill(getDeviceName(), "DELAY", "Filter Delay", FILTER_TAB, IP_RW, 60, IPS_IDLE);
    return true;
}

bool FilterSim::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        defineProperty(DelayNP);
    }
    else
    {
        deleteProperty(DelayNP);
    }

    return true;
}

bool FilterSim::Connect()
{
    CurrentFilter      = 1;
    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(8);
    return true;
}

bool FilterSim::Disconnect()
{
    return true;
}

bool FilterSim::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (DelayNP.isNameMatch(name))
        {
            DelayNP.update(values, names, n);
            DelayNP.setState(IPS_OK);
            DelayNP.apply();
            return true;
        }
    }

    return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);

}

bool FilterSim::SelectFilter(int f)
{
    int delay = DelayNP[0].getValue();
    if (delay < 0)
        return false;

    std::this_thread::sleep_for(std::chrono::seconds(delay));

    CurrentFilter = f;
    SetTimer(10);
    return true;
}

void FilterSim::TimerHit()
{
    SelectFilterDone(CurrentFilter);
}
