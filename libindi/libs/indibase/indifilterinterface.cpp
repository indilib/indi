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
#include "indilogger.h"

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

void INDI::FilterInterface::processFilterSlot(const char *deviceName, double values[], char *names[])
{
        TargetFilter = values[0];

        INumber *np = IUFindNumber(&FilterSlotNP, names[0]);

        if (!np)
        {
            FilterSlotNP.s = IPS_ALERT;
            DEBUGFDEVICE(deviceName, Logger::DBG_ERROR, "Unknown error. %s is not a member of %s property.", names[0], FilterSlotNP.name);
            IDSetNumber(&FilterSlotNP, NULL);
            return;
        }

        if (TargetFilter < FilterSlotN[0].min || TargetFilter > FilterSlotN[0].max)
        {
            FilterSlotNP.s = IPS_ALERT;
            DEBUGFDEVICE(deviceName, Logger::DBG_ERROR, "Error: valid range of filter is from %g to %g", FilterSlotN[0].min, FilterSlotN[0].max);
            IDSetNumber(&FilterSlotNP, NULL);
            return;
        }

        FilterSlotNP.s = IPS_BUSY;
        DEBUGFDEVICE(deviceName, Logger::DBG_SESSION, "Setting current filter to slot %d", TargetFilter);
        IDSetNumber(&FilterSlotNP, NULL);

        SelectFilter(TargetFilter);

        return;

}

void INDI::FilterInterface::processFilterName(const char *deviceName, char *texts[], char *names[], int n)
{
    int rc;
    FilterNameTP->s=IPS_OK;
    rc=IUUpdateText(FilterNameTP,texts,names,n);

    if (SetFilterNames() == true)
        IDSetText(FilterNameTP,NULL);
    else
    {
        FilterNameTP->s = IPS_ALERT;
        DEBUGDEVICE(deviceName, Logger::DBG_ERROR, "Error updating names of filters.");
        IDSetText(FilterNameTP, NULL);
    }

}




