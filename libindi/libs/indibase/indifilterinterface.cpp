/*
    Filter Interface
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

#include <string.h>

#include "indifilterinterface.h"

INDI::FilterInterface::FilterInterface()
{
    FilterNameTP  = new ITextVectorProperty;
    FilterNameT  = NULL;
}

void INDI::FilterInterface::initFilterProperties(const char *deviceName, const char* groupName)
{
    IUFillNumber(&FilterSlotN[0],"FILTER_SLOT_VALUE","Filter","%3.0f",1.0,12.0,1.0,1.0);
    IUFillNumberVector(&FilterSlotNP,FilterSlotN,1,deviceName,"FILTER_SLOT","Filter",groupName,IP_RW,60,IPS_IDLE);
}

INDI::FilterInterface::~FilterInterface()
{

    delete FilterNameTP;
}

void INDI::FilterInterface::SelectFilterDone(int f)
{
    //  The hardware has finished changing
    //  filters
    FilterSlotN[0].value=f;
    FilterSlotNP.s=IPS_OK;
    // Tell the clients we are done, and
    //  filter is now useable
    IDSetNumber(&FilterSlotNP,NULL);
}

void INDI::FilterInterface::processFilterProperties(const char *name, double values[], char *names[], int n)
{

    if (!strcmp(FilterSlotNP.name, name))
    {

        TargetFilter = values[0];

        INumber *np = IUFindNumber(&FilterSlotNP, names[0]);

        if (!np)
        {
            FilterSlotNP.s = IPS_ALERT;
            IDSetNumber(&FilterSlotNP, "Unknown error. %s is not a member of %s property.", names[0], name);
            return;
        }

        if (TargetFilter < FilterSlotN[0].min || TargetFilter > FilterSlotN[0].max)
        {
            FilterSlotNP.s = IPS_ALERT;
            IDSetNumber(&FilterSlotNP, "Error: valid range of filter is from %g to %g", FilterSlotN[0].min, FilterSlotN[0].max);
            return;
        }

        FilterSlotNP.s = IPS_BUSY;
        IDSetNumber(&FilterSlotNP, "Setting current filter to slot %d", TargetFilter);

        SelectFilter(TargetFilter);

        return;

    }
}




