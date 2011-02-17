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

#include "indifilterwheel.h"

#include <string.h>

INDI::FilterWheel::FilterWheel()
{
    //ctor
    MaxFilter=12;
}

INDI::FilterWheel::~FilterWheel()
{
    //dtor
}

bool INDI::FilterWheel::initProperties()
{
    DefaultDriver::initProperties();   //  let the base class flesh in what it wants

    IUFillNumber(&FilterSlotN[0],"FILTER_SLOT_VALUE","Filter","%3.0f",1.0,12.0,1.0,1.0);
    IUFillNumberVector(&FilterSlotNV,FilterSlotN,1,deviceName(),"FILTER_SLOT","Filter","Main Control",IP_RW,60,IPS_IDLE);

    IUFillText(&FilterNameT[0],"FILTER1","1","");
    IUFillText(&FilterNameT[1],"FILTER2","2","");
    IUFillText(&FilterNameT[2],"FILTER3","3","");
    IUFillText(&FilterNameT[3],"FILTER4","4","");
    IUFillText(&FilterNameT[4],"FILTER5","5","");
    IUFillText(&FilterNameT[5],"FILTER6","6","");
    IUFillText(&FilterNameT[6],"FILTER7","7","");
    IUFillText(&FilterNameT[7],"FILTER8","8","");
    IUFillText(&FilterNameT[8],"FILTER9","9","");
    IUFillText(&FilterNameT[9],"FILTER10","10","");
    IUFillText(&FilterNameT[10],"FILTER11","11","");
    IUFillText(&FilterNameT[11],"FILTER12","12","");
    IUFillTextVector(&FilterNameTV,FilterNameT,12,deviceName(),"FILTER_NAME","Filter","Filters",IP_RW,60,IPS_IDLE);

    return 0;
}

void INDI::FilterWheel::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    //IDLog("INDI::FilterWheel::ISGetProperties %s\n",dev);
    DefaultDriver::ISGetProperties(dev);
    if(isConnected())
    {
        IDDefNumber(&FilterSlotNV, NULL);
        IDDefText(&FilterNameTV, NULL);
    }
    return;
}

bool INDI::FilterWheel::updateProperties()
{
    //  Define more properties after we are connected
    //  first we want to update the values to reflect our actual wheel

    if(isConnected())
    {
        IUFillNumber(&FilterSlotN[0],"FILTER_SLOT_VALUE","Filter","%3.0f",MinFilter,MaxFilter,1.0,CurrentFilter);
        IDDefNumber(&FilterSlotNV, NULL);
        IUFillTextVector(&FilterNameTV,FilterNameT,MaxFilter,deviceName(),"FILTER_NAME","Filter","Filters",IP_RW,60,IPS_IDLE);
        IDDefText(&FilterNameTV, NULL);
        //LoadFilterNames();
    } else
    {
        deleteProperty(FilterSlotNV.name);
        deleteProperty(FilterNameTV.name);
    }

    return true;
}

bool INDI::FilterWheel::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("INDI::FilterWheel::ISNewNumber %s\n",name);
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
                //IDLog("Filter wheel got a filter slot change\n");
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
    return DefaultDriver::ISNewNumber(dev,name,values,names,n);
}

bool INDI::FilterWheel::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  Ok, lets see if this is a property wer process
    //IDLog("INDI::FilterWheel got %d new text items name %s\n",n,name);
    //  first check if it's for our device
    if(strcmp(dev,deviceName())==0) {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,FilterNameTV.name)==0) {
            //  This is our port, so, lets process it

            //  Some clients insist on sending a port
            //  and they may not be configured for the
            //  correct port
            //  If we are already connected
            //  and running, it makes absolutely no sense
            //  to accept a new port value
            //  so lets just lie to them and say
            //  we did this, but, dont actually change anything
            //if(Connected) return true;

            int rc;
            //IDLog("calling update text\n");
            FilterNameTV.s=IPS_OK;
            rc=IUUpdateText(&FilterNameTV,texts,names,n);
            //IDLog("update text returns %d\n",rc);
            //  Update client display
            IDSetText(&FilterNameTV,NULL);
            //SaveConfig();

            //  We processed this one, so, tell the world we did it
            return true;
        }

    }

    return DefaultDriver::ISNewText(dev,name,texts,names,n);
}

int INDI::FilterWheel::SelectFilterDone(int f)
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

int INDI::FilterWheel::SelectFilter(int)
{
    return -1;
}

int INDI::FilterWheel::QueryFilter()
{
    return -1;
}


