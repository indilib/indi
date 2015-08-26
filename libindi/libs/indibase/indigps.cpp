/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  INDI GPS Device Class

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "indigps.h"

#define POLLMS  1000

INDI::GPS::GPS()
{
    //ctor
}

INDI::GPS::~GPS()
{
}

bool INDI::GPS::initProperties()
{
    INDI::DefaultDevice::initProperties();

    IUFillSwitch(&RefreshS[0], "REFRESH", "GPS", ISS_OFF);
    IUFillSwitchVector(&RefreshSP, RefreshS, 1, getDeviceName(), "GPS_REFRESH", "Refresh", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    IUFillNumber(&LocationN[LOCATION_LATITUDE],"LAT","Lat (dd:mm:ss)","%010.6m",-90,90,0,0.0);
    IUFillNumber(&LocationN[LOCATION_LONGITUDE],"LONG","Lon (dd:mm:ss)","%010.6m",0,360,0,0.0 );
    IUFillNumber(&LocationN[LOCATION_ELEVATION],"ELEV","Elevation (m)","%g",-200,10000,0,0 );
    IUFillNumberVector(&LocationNP,LocationN,3,getDeviceName(),"GEOGRAPHIC_COORD","Location",MAIN_CONTROL_TAB,IP_RO,60,IPS_OK);

    IUFillText(&TimeT[0],"UTC","UTC Time",NULL);
    IUFillText(&TimeT[1],"OFFSET","UTC Offset",NULL);
    IUFillTextVector(&TimeTP,TimeT,2,getDeviceName(),"TIME_UTC","UTC",MAIN_CONTROL_TAB,IP_RO,60,IPS_IDLE);

    return true;
}

bool INDI::GPS::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Update GPS and send values to client
        IPState state = updateGPS();

        defineNumber(&LocationNP);
        defineText(&TimeTP);
        defineSwitch(&RefreshSP);

        if (state != IPS_OK)
        {
            if (state == IPS_BUSY)
                DEBUG(INDI::Logger::DBG_SESSION, "GPS fix is in progress...");

            SetTimer(POLLMS);
        }
    }
    else
    {
        deleteProperty(LocationNP.name);
        deleteProperty(TimeTP.name);
        deleteProperty(RefreshSP.name);
    }

    return true;
}


void INDI::GPS::TimerHit()
{
    if (updateGPS() == IPS_OK)
    {
        IDSetNumber(&LocationNP, NULL);
        IDSetText(&TimeTP, NULL);
        return;
    }

    SetTimer(POLLMS);
}

IPState INDI::GPS::updateGPS()
{
    DEBUG(INDI::Logger::DBG_ERROR, "updateGPS() must be implemented in GPS device child class to update TIME_UTC and GEOGRAPHIC_COORD properties.");
    return IPS_ALERT;
}

bool INDI::GPS::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, RefreshSP.name))
        {
            RefreshS[0].s = ISS_OFF;
            RefreshSP.s = updateGPS();

            IDSetNumber(&LocationNP, NULL);
            IDSetText(&TimeTP, NULL);
            IDSetSwitch(&RefreshSP, NULL);
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}
