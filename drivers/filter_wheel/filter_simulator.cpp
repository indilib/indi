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

// We declare an auto pointer to FilterSim.
std::unique_ptr<FilterSim> filter_sim(new FilterSim());

const char *FilterSim::getDefaultName()
{
    return (const char *)"Filter Simulator";
}

bool FilterSim::Connect()
{
    CurrentFilter      = 1;
    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = 8;
    return true;
}

bool FilterSim::Disconnect()
{
    return true;
}

bool FilterSim::SelectFilter(int f)
{
    CurrentFilter = f;
    SetTimer(500);
    return true;
}

void FilterSim::TimerHit()
{
    SelectFilterDone(CurrentFilter);
}
