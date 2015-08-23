/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  Simple GPS Simulator

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

#include <memory>
#include <libnova.h>
#include <time.h>

#include "gps_simulator.h"

// We declare an auto pointer to GPSSimulator.
std::auto_ptr<GPSSimulator> gpsSimulator(0);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(gpsSimulator.get() == 0) gpsSimulator.reset(new GPSSimulator());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        gpsSimulator->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        gpsSimulator->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        gpsSimulator->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        gpsSimulator->ISNewNumber(dev, name, values, names, num);
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


GPSSimulator::GPSSimulator()
{
   setVersion(1,0);
}

GPSSimulator::~GPSSimulator()
{

}

const char * GPSSimulator::getDefaultName()
{
    return (char *)"GPS Simulator";
}

bool GPSSimulator::Connect()
{
   return true;
}

bool GPSSimulator::Disconnect()
{
    return true;
}

bool GPSSimulator::updateGPS()
{
    static char ts[32];
    struct tm *utc, *local;

    time_t raw_time;
    time(&raw_time);

    utc  = gmtime(&raw_time);    
    strftime (ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", utc);
    IUSaveText(&TimeT[0], ts);

    local= localtime(&raw_time);
    snprintf(ts, sizeof(ts), "%4.2f", (local->tm_gmtoff/3600.0));
    IUSaveText(&TimeT[1], ts);

    TimeTP.s = IPS_OK;

    LocationN[LOCATION_LATITUDE].value = 29.1;
    LocationN[LOCATION_LONGITUDE].value = 48.5;
    LocationN[LOCATION_ELEVATION].value = 12;

    LocationNP.s = IPS_OK;

    return true;
}

