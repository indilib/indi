/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "IndiFilterWheel.h"

#include <string.h>

IndiFilterWheel::IndiFilterWheel()
{
    //ctor
}

IndiFilterWheel::~IndiFilterWheel()
{
    //dtor
}

int IndiFilterWheel::init_properties()
{
    IndiDevice::init_properties();   //  let the base class flesh in what it wants

    IUFillNumber(&FilterSlotN[0],"FILTER_SLOT_VALUE","Filter","%3.0f",1.0,10.0,1.0,1.0);
    IUFillNumberVector(&FilterSlotNV,FilterSlotN,1,deviceName(),"FILTER_SLOT","Filter","Main Control",IP_RW,60,IPS_IDLE);
    return 0;
}

void IndiFilterWheel::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    IDLog("IndiFilterWheel::ISGetProperties %s\n",dev);
    IndiDevice::ISGetProperties(dev);
    if(Connected) {
        IDDefNumber(&FilterSlotNV, NULL);
    }
    return;
}

bool IndiFilterWheel::UpdateProperties()
{
    //  Define more properties after we are connected
    //  first we want to update the values to reflect our actual wheel

    if(Connected) {
        IUFillNumber(&FilterSlotN[0],"FILTER_SLOT_VALUE","Filter","%3.0f",MinFilter,MaxFilter,1.0,CurrentFilter);
        IDDefNumber(&FilterSlotNV, NULL);
    } else {
        DeleteProperty(FilterSlotNV.name);
    }

    return true;
}

bool IndiFilterWheel::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    IDLog("IndiFilterWheel::ISNewNumber %s\n",name);
    if(strcmp(dev,deviceName())==0) {
        //  This is for our device
        //  Now lets see if it's something we process here

        if(strcmp(name,"FILTER_SLOT")==0) {

            int f;

            f=-1;
            for(int x=0; x<n; x++) {
                if(strcmp(names[x],"FILTER_SLOT_VALUE")==0) {
                    //  This is the new filter number we are being asked
                    //  to set as active
                    f=values[x];
                }
            }

            if(f != -1) {
                IDLog("Filter wheel got a filter slot change\n");
                //  tell the client we are busy changing the filter
                FilterSlotNV.s=IPS_BUSY;
                IDSetNumber(&FilterSlotNV,NULL);
                //  Tell the hardware to change
                SelectFilter(f);
                //  tell the caller we processed this
                return true;
            }
        }
    }
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return IndiDevice::ISNewNumber(dev,name,values,names,n);
}

int IndiFilterWheel::SelectFilterDone(int f)
{
    //  The hardware has finished changing
    //  filters
    FilterSlotN[0].value=f;
    FilterSlotNV.s=IPS_OK;
    // Tell the clients we are done, and
    //  filter is now useable
    IDSetNumber(&FilterSlotNV,NULL);
    return 0;
}

int IndiFilterWheel::SelectFilter(int)
{
    return -1;
}

int IndiFilterWheel::QueryFilter()
{
    return -1;
}
