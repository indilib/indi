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
std::auto_ptr<FilterSim> filter_sim(0);

void ISPoll(void *p);


void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(filter_sim.get() == 0) filter_sim.reset(new FilterSim());
    //IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISGetProperties(const char *dev)
{
        ISInit();
        filter_sim->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        filter_sim->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        filter_sim->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        filter_sim->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}
void ISSnoopDevice (XMLEle *root)
{
    INDI_UNUSED(root);
}


FilterSim::FilterSim()
{
    //ctor
}

FilterSim::~FilterSim()
{
    //dtor
}

const char *FilterSim::getDefaultName()
{
    return (char *)"Filter Simulator";
}

bool FilterSim::Connect()
{
    CurrentFilter=1;
    MinFilter=1;
    MaxFilter=5;
    FilterSlotN[0].max = MaxFilter;
    return true;
}

bool FilterSim::Disconnect()
{
    return true;
}

bool FilterSim::SelectFilter(int f)
{
    CurrentFilter=f;
    SetTimer(500);
    return true;
}

void FilterSim::TimerHit()
{
    SelectFilterDone(CurrentFilter);
}

bool FilterSim::GetFilterNames(const char* groupName)
{
    char filterName[MAXINDINAME];
    char filterLabel[MAXINDILABEL];

    const char *filterDesignation[5] = { "Red", "Green", "Blue", "H Alpha", "Luminosity" };

    if (FilterNameT != NULL)
        delete FilterNameT;

    FilterNameT = new IText[MaxFilter];

    for (int i=0; i < MaxFilter; i++)
    {
        snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i+1);
        snprintf(filterLabel, MAXINDILABEL, "Filter #%d", i+1);
        IUFillText(&FilterNameT[i], filterName, filterLabel, filterDesignation[i]);
    }

    IUFillTextVector(&FilterNameTP, FilterNameT, MaxFilter, getDeviceName(), "FILTER_NAME", "Filter", groupName, IP_RW, 0, IPS_IDLE);

    return true;
}
