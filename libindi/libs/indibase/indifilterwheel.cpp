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
    MaxFilter=-1;
}

INDI::FilterWheel::~FilterWheel()
{
    //dtor
}


bool INDI::FilterWheel::initProperties()
{
    DefaultDriver::initProperties();

    initFilterProperties(deviceName(), FILTER_TAB);
}

void INDI::FilterWheel::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    //IDLog("INDI::FilterWheel::ISGetProperties %s\n",dev);
    DefaultDriver::ISGetProperties(dev);
    if(isConnected())
    {
        defineNumber(FilterSlotNP);

        if (initFilterNames(deviceName(), FILTER_TAB))
            defineText(FilterNameTP);
    }
    return;
}

bool INDI::FilterWheel::updateProperties()
{
    //  Define more properties after we are connected
    //  first we want to update the values to reflect our actual wheel

    if(isConnected())
    {
        initFilterProperties(deviceName(), FILTER_TAB);
        defineNumber(FilterSlotNP);
        if (initFilterNames(deviceName(), FILTER_TAB))
            defineText(FilterNameTP);
    } else
    {
        deleteProperty(FilterSlotNP->name);
        deleteProperty(FilterNameTP->name);
    }

    return true;
}

bool INDI::FilterWheel::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    return DefaultDriver::ISNewSwitch(dev, name, states, names,n);
}

bool INDI::FilterWheel::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("INDI::FilterWheel::ISNewNumber %s\n",name);
    if(strcmp(dev,deviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here

        if(strcmp(name,"FILTER_SLOT")==0)
        {
            int f;

            f=-1;
            for(int x=0; x<n; x++)
            {
                if(strcmp(names[x],"FILTER_SLOT_VALUE")==0)
                {
                    //  This is the new filter number we are being asked
                    //  to set as active
                    f=values[x];
                }
            }

            if(f != -1)
            {
                //IDLog("Filter wheel got a filter slot change\n");
                //  tell the client we are busy changing the filter
                FilterSlotNP->s=IPS_BUSY;
                IDSetNumber(FilterSlotNP,NULL);
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
    if(strcmp(dev,deviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,FilterNameTP->name)==0)
        {
            int rc;
            //IDLog("calling update text\n");
            FilterNameTP->s=IPS_OK;
            rc=IUUpdateText(FilterNameTP,texts,names,n);

            if (SetFilterNames() == true)
                IDSetText(FilterNameTP,NULL);
            else
            {
                FilterNameTP->s = IPS_ALERT;
                IDSetText(FilterNameTP, "Error updating names of filters.");
            }
            //  We processed this one, so, tell the world we did it
            return true;
        }

    }

    return DefaultDriver::ISNewText(dev,name,texts,names,n);
}

int INDI::FilterWheel::QueryFilter()
{
    return -1;
}


bool INDI::FilterWheel::SelectFilter(int)
{
    return false;
}

bool INDI::FilterWheel::SetFilterNames()
{
    return true;
}

bool INDI::FilterWheel::initFilterNames(const char *deviceName, const char* groupName)
{
    return false;
}


