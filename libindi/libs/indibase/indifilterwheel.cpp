/*******************************************************************************
  Copyright(c) 2010, 2011 Gerry Rozema. All rights reserved.

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

#include "indifilterwheel.h"

#include <string.h>

INDI::FilterWheel::FilterWheel()
{
    //ctor
}

INDI::FilterWheel::~FilterWheel()
{
    //dtor
}


bool INDI::FilterWheel::initProperties()
{
    DefaultDevice::initProperties();

    initFilterProperties(getDeviceName(), FILTER_TAB);

    return true;
}

void INDI::FilterWheel::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    //IDLog("INDI::FilterWheel::ISGetProperties %s\n",dev);
    DefaultDevice::ISGetProperties(dev);
    if(isConnected())
    {
        defineNumber(&FilterSlotNP);

        if (GetFilterNames(FILTER_TAB))
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
        //initFilterProperties(getDeviceName(), FILTER_TAB);
        defineNumber(&FilterSlotNP);
        if (GetFilterNames(FILTER_TAB))
            defineText(FilterNameTP);
    } else
    {
        deleteProperty(FilterSlotNP.name);
        deleteProperty(FilterNameTP->name);
    }

    return true;
}

bool INDI::FilterWheel::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    return DefaultDevice::ISNewSwitch(dev, name, states, names,n);
}

bool INDI::FilterWheel::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("INDI::FilterWheel::ISNewNumber %s\n",name);
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here

        if(strcmp(name,"FILTER_SLOT")==0)
        {
            processFilterSlot(dev, values, names);
            return true;
        }
    }
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool INDI::FilterWheel::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  Ok, lets see if this is a property wer process
    //IDLog("INDI::FilterWheel got %d new text items name %s\n",n,name);
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,FilterNameTP->name)==0)
        {
            processFilterName(dev, texts, names, n);
            return true;
        }

    }

    return DefaultDevice::ISNewText(dev,name,texts,names,n);
}

bool INDI::FilterWheel::saveConfigItems(FILE *fp)
{
    IUSaveConfigNumber(fp, &FilterSlotNP);
    IUSaveConfigText(fp, FilterNameTP);
    return true;
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


bool INDI::FilterWheel::GetFilterNames(const char* groupName)
{
    INDI_UNUSED(groupName);
    return false;
}

bool INDI::FilterWheel::ISSnoopDevice (XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}
