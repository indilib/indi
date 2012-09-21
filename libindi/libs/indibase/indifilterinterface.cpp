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

#include "indifilterinterface.h"

INDI::FilterInterface::FilterInterface()
{
    FilterNameTP  = new ITextVectorProperty;
    FilterNameT  = NULL;
    MinFilter = MaxFilter = 0;
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




